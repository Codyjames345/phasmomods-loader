// Offline tests for Playable Ghost's glue that decides game behaviour through the game API, over the fake
// backend: the Yokai player's hearing (yokai_ears.cpp), the door, light and prop chances' own rolls
// (chances.cpp), the aimed interaction's filter (aim.h), what the Kormos hears (footsteps.cpp), the Mimic's
// pick (mimic.cpp), the guess each investigator sends (guess.cpp) and what the ghost player must not see
// (hide.cpp).
// Run: build\glue_test.exe
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "GhostAI.h"
#include "GhostController.h"
#include "GhostInfo.h"
#include "GhostTypes.h"
#include "JournalController.h"
#include "GhostSenses.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Player.h"
#include "PlayerVoice.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "aim.h"
#include "chances.h"
#include "check.h"
#include "fake_game.h"
#include "Equipment.h"
#include "Props.h"
#include "UnityRendering.h"
#include "footsteps.h"
#include "guess.h"
#include "hide.h"
#include "mimic.h"
#include "runtime.h"
#include "yokai_ears.h"

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace api = pm::api;

// A fake managed object: its class pointer first, as a real one's, then room for its fields.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x38]{};
};

template <class T>
void put(Obj& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}

void bind() {
  std::string missing;
  api::bind(fake::host(), missing);
}

// The fake's world: where each transform stands, the component behind each tap and the transform of
// each object, and every AudioSource's mute.
std::map<const void*, phasmo::Vector3> g_at;
std::map<const void*, void*> g_component, g_transform;
std::map<const void*, bool> g_muted;
void* g_voices = nullptr;
phasmo::Vector3 fake_position(void* t, const void*) { return g_at[t]; }
void* fake_transform(void* o, const void*) { return g_transform[o]; }
void* fake_component(void* tap, void*, const void*) { return g_component[tap]; }
void* fake_find(void*, const void*) { return g_voices; }
void fake_mute(void* source, bool mute, const void*) { g_muted[source] = mute; }
// Whether the source was muted last time its mute was set; -1 when it never was.
int mute_of(const void* source) {
  const auto it = g_muted.find(source);
  return it == g_muted.end() ? -1 : it->second;
}

// A hunting Yokai on floor 1 with its head at the origin, and three investigators' voices: near on its
// floor, near on another floor, and far on its floor. Each voice has a positional tap and a radio tap.
struct Yokai {
  Obj ghost, head, level, room, upstairs, truck;
  Obj voice[3], player[3], body[3], bodyAt[3], local[3], localSource[3], radio[3], radioSource[3];
  void* instance = nullptr;

  Yokai() {
    fake::reset();
    g_at.clear();
    g_component.clear();
    g_transform.clear();
    g_muted.clear();
    fake::field(map::GhostAI_head, 0x10);
    fake::static_field(map::LevelController_instance, &instance, sizeof instance, true);
    fake::field(map::LevelController_currentGhostRoom, 0x10);
    fake::field(map::PlayerVoice_player, 0x10);
    fake::field(map::PlayerVoice_localTap, 0x18);
    fake::field(map::PlayerVoice_globalTap, 0x20);
    fake::field(map::Player_body, 0x10);
    fake::field(map::Player_currentRoom, 0x18);
    fake::field(map::Player_wished, 0x20);
    fake::field(map::LevelRoom_floor, 0x10);
    fake::field(map::LevelRoom_kind, 0x14);
    fake::value(map::RoomKind_outsideMap, 1);
    fake::method(map::Transform_get_position, reinterpret_cast<void*>(&fake_position));
    fake::method(map::GameObject_get_transform, reinterpret_cast<void*>(&fake_transform));
    fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_transform));
    fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&fake_component));
    fake::method(map::UnityObject_FindObjectsOfType, reinterpret_cast<void*>(&fake_find));
    fake::method(map::AudioSource_set_mute, reinterpret_cast<void*>(&fake_mute));
    bind();
    instance = &level;
    put(level, 0x10, static_cast<void*>(&room));
    put(ghost, 0x10, static_cast<void*>(&head));
    g_at[&head] = {0, 0, 0};
    put(room, 0x10, int32_t{1});
    put(upstairs, 0x10, int32_t{2});
    put(truck, 0x10, int32_t{1});
    put(truck, 0x14, int32_t{1});
    const float distance[3] = {2.0f, 1.0f, 3.0f};
    Obj* where[3] = {&room, &upstairs, &room};
    g_voices = fake::array(sizeof(void*), 3);
    for (int i = 0; i < 3; ++i) {
      put(voice[i], 0x10, static_cast<void*>(&player[i]));
      put(voice[i], 0x18, static_cast<void*>(&local[i]));
      put(voice[i], 0x20, static_cast<void*>(&radio[i]));
      g_component[&local[i]] = &localSource[i];
      g_component[&radio[i]] = &radioSource[i];
      put(player[i], 0x10, static_cast<void*>(&body[i]));
      put(player[i], 0x18, static_cast<void*>(where[i]));
      g_transform[&body[i]] = &bodyAt[i];
      g_at[&bodyAt[i]] = {distance[i], 0, 0};
      api::RefArray{g_voices}.set(i, &voice[i]);
    }
  }
};

// The Yokai player hears, on the tap that plays where each investigator stands, only those the game's
// hunt would hear: within 2.5 m on its floor, or anyone a Monkey Paw wish marked; never the radio's tap.
void yokai_mutes_the_positional_voices_it_would_not_hear() {
  Yokai y;
  std::vector<api::Held> muted;
  pm::yokai_ears::hear(&y.ghost, muted);
  CHECK(mute_of(&y.localSource[0]) == 0 && mute_of(&y.localSource[1]) == 1 && mute_of(&y.localSource[2]) == 1);
  for (const Obj& r : y.radioSource) CHECK(!g_muted.count(&r));  // the radio is senses.h's
  CHECK(muted.size() == 2);
  pm::yokai_ears::hear(&y.ghost, muted);
  CHECK(muted.size() == 2);  // each source kept once
  // A wish marks the far one and the one upstairs: heard from anywhere in the house, not from the truck.
  put(y.player[1], 0x20, true);
  put(y.player[2], 0x20, true);
  g_at[&y.bodyAt[2]] = {900.0f, 0, 0};
  pm::yokai_ears::hear(&y.ghost, muted);
  CHECK(mute_of(&y.localSource[1]) == 0 && mute_of(&y.localSource[2]) == 0);
  put(y.player[2], 0x18, static_cast<void*>(&y.truck));
  pm::yokai_ears::hear(&y.ghost, muted);
  CHECK(mute_of(&y.localSource[2]) == 1);
  // Leaving the hunt unmutes every source it muted.
  pm::yokai_ears::unmute(muted);
  CHECK(muted.empty() && mute_of(&y.localSource[1]) == 0 && mute_of(&y.localSource[2]) == 0);
}

// The detours a test's code hooked, by entry key, through a capturing host (as tests/hooks_test.cpp does).
PmHostApi g_host{};
bool (*g_fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;
std::map<std::string, void*> g_detour;
bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!g_fakeHookBody(key, index, detour, original, name)) return false;
  g_detour[key] = detour;
  return true;
}
void bind_capturing() {
  g_host = *fake::host();
  g_fakeHookBody = g_host.hook_body;
  g_host.hook_body = &capture_hook_body;
  g_detour.clear();
  std::string missing;
  api::bind(&g_host, missing);
}

// The game's chances as the fake runs them: each draws what g_rolls lists, through the mod's draw (which
// stands for its hook on Random.RandomRangeInt), and records what the game got. The door chance runs the
// light chance first when g_nest is set, and throws after its draws when g_throw is set.
bool g_aimed = false, g_nest = false, g_throw = false;
std::vector<int> g_got;
bool light_detour(void* activity) {
  using F = bool (*)(void*, bool, const void*);
  return reinterpret_cast<F>(g_detour.at("GhostActivity.lightChance"))(activity, false, nullptr);
}
bool fake_light_chance(void*, bool, const void*) {
  g_got.push_back(pm::chances::draw(0, 100, 61, g_aimed));
  g_got.push_back(pm::chances::draw(0, 4, 3, g_aimed));  // the light or flicker draw is the game's
  return true;
}
bool fake_door_chance(void* activity, const void*) {
  if (g_nest) light_detour(activity);
  g_got.push_back(pm::chances::draw(0, 100, 57, g_aimed));
  g_got.push_back(pm::chances::draw(0, 100, 58, g_aimed));  // the door's state change draws its own
  if (g_throw) throw 1;
  return true;
}
bool fake_prop_chance(void*, bool, const void*) {
  g_got.push_back(pm::chances::draw(0, 100, 33, g_aimed));
  return true;
}
bool door(void* activity) {
  using F = bool (*)(void*, const void*);
  return reinterpret_cast<F>(g_detour.at("GhostActivity.doorChance"))(activity, nullptr);
}

// Inside an aimed interaction each chance's own roll, the first draw of 0 to 100 after it begins, is
// answered 0, and no other draw; a chance inside another, or one that throws, leaves the roll as it was.
void each_chance_answers_its_own_roll_only() {
  fake::reset();
  fake::method(map::GhostActivity_doorChance, reinterpret_cast<void*>(&fake_door_chance));
  fake::method(map::GhostActivity_lightChance, reinterpret_cast<void*>(&fake_light_chance));
  fake::method(map::GhostActivity_InteractWithARandomProp, reinterpret_cast<void*>(&fake_prop_chance));
  bind_capturing();
  CHECK(pm::chances::create_hooks() && g_detour.size() == 3);
  Obj activity;
  g_aimed = true;
  g_got.clear();
  CHECK(door(&activity) && g_got == std::vector<int>({0, 58}));
  CHECK(pm::chances::draw(0, 100, 59, true) == 59);  // no chance running: the game's own
  g_got.clear();
  using Prop = bool (*)(void*, bool, const void*);
  CHECK(reinterpret_cast<Prop>(g_detour.at("GhostActivity.InteractWithARandomProp"))(&activity, false, nullptr));
  CHECK(g_got == std::vector<int>({0}));
  g_aimed = false;
  g_got.clear();
  CHECK(door(&activity) && g_got == std::vector<int>({57, 58}));  // not aimed: every roll stands
  g_aimed = true;
  g_nest = true;  // the light chance runs inside the door chance, before its roll
  g_got.clear();
  CHECK(door(&activity) && g_got == std::vector<int>({0, 3, 0, 58}));
  g_nest = false;
  g_throw = true;
  g_got.clear();
  try {
    door(&activity);
  } catch (int) {
  }
  g_throw = false;
  CHECK(g_got == std::vector<int>({0, 58}) && pm::chances::draw(0, 100, 60, true) == 60);
  g_aimed = false;
}

// The aimed interaction keeps only what its test passes in the sensor's list, in their order, through the
// List's own RemoveAt.
void the_aimed_filter_keeps_only_what_passes() {
  fake::reset();
  bind();
  Obj a, b, c, d;
  void* list = fake::list({&a, &b, &c, &d});
  pm::aim::keep_only(api::List{list}, [&](void* item) { return item == &b || item == &d; });
  CHECK(fake::list_items_of(list) == std::vector<void*>({&b, &d}));
  pm::aim::keep_only(api::List{list}, [](void*) { return false; });
  CHECK(fake::list_items_of(list).empty());
  pm::aim::keep_only(api::List{nullptr}, [](void*) { return false; });  // no list: nothing to do
}

// The Kormos hears each noise its hearing holds, a List of structs read at the runtime's own stride, as a
// mark of the footstep's kind where its maker stands; a dead investigator makes none.
void the_kormos_hears_each_footstep_its_hearing_holds() {
  fake::reset();
  g_at.clear();
  g_transform.clear();
  fake::field(map::GhostAI_hearing, 0x10);
  fake::field(map::GhostHearing_heard, 0x10);
  fake::field(map::HeardNoise_noise, 0x0);
  fake::field(map::PlayerNoise_kind, 0x10);
  fake::field(map::PlayerNoise_player, 0x18);
  fake::field(map::Player_isDead, 0x18);
  fake::value(map::NoiseKind_SprintFootstep, 3);
  fake::value(map::NoiseKind_CrouchFootstep, 4);
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_transform));
  fake::method(map::Transform_get_position, reinterpret_cast<void*>(&fake_position));
  bind();
  Obj ghost, ears, noise[4], player[4], at[4];
  put(ghost, 0x10, static_cast<void*>(&ears));
  void* heard = fake::struct_list(16, 4);  // HeardNoise: the PlayerNoise, then a float, padded to 16
  put(ears, 0x10, heard);
  const int32_t kinds[4] = {3, 4, 0, 3};
  const api::ValueArray elements = api::List{heard}.values();
  for (int i = 0; i < 4; ++i) {
    void* n = &noise[i];
    std::memcpy(elements.at(i), &n, sizeof n);
    put(noise[i], 0x10, kinds[i]);
    put(noise[i], 0x18, static_cast<void*>(&player[i]));
    g_transform[&player[i]] = &at[i];
    g_at[&at[i]] = {static_cast<float>(i), 0, 1};
  }
  put(player[3], 0x18, true);  // dead
  std::vector<pm::perceive::Mark> out;
  pm::footsteps::heard(&ghost, out);
  CHECK(out.size() == 3 && out[0] == (pm::perceive::Mark{pm::perceive::kStepSprint, 0, 0, 1}) &&
        out[1] == (pm::perceive::Mark{pm::perceive::kStepCreep, 1, 0, 1}) &&
        out[2] == (pm::perceive::Mark{pm::perceive::kStepWalk, 2, 0, 1}));
}


// A Mimic's controller, with its type numbered unlike the game's own so the test proves the numbers are
// read: Mimic 7, None 9. Its copies are 1, 7, 300 and 4; 300 does not fit a type the session sends.
// Its ghost (the controller's levelController, the level's currentGhost and its info) is copying 7.
struct Mimic {
  Obj controller, level, ghost, info;
  void* instance = nullptr;
  Mimic() {
    fake::reset();
    fake::field(map::GhostController_ghostType, 0x10);
    fake::field(map::GhostController_typeChangeDue, 0x14);
    fake::field(map::GhostController_mimicNext, 0x18);
    fake::field(map::GhostController_mimicCopies, 0x20);
    fake::field(map::GhostController_levelController, 0x28);
    fake::field(map::LevelController_currentGhost, 0x10);
    fake::field(map::GhostAI_info, 0x10);
    fake::field(map::GhostInfo_behaviourType, 0x10);
    fake::static_field(map::GhostController_instance, &instance, sizeof instance, true);
    fake::value(map::GhostType_Mimic, 7);
    bind();
    put(controller, 0x10, int32_t{7});
    put(controller, 0x20, fake::value_list({1, 7, 300, 4}));
    put(controller, 0x28, static_cast<void*>(&level));
    put(level, 0x10, static_cast<void*>(&ghost));
    put(ghost, 0x10, static_cast<void*>(&info));
    put(info, 0x10, int32_t{7});
  }
  phasmo::NullableGhostType next() const { return phasmo::GhostController::mimicNext.get(const_cast<Obj*>(&controller)); }
};

// The Mimic's player is offered the types the game would copy, and its pick goes into the game's own
// override only at the game's moment to switch, only for a type the game would copy, and never over an
// override already waiting.
void mimic_pick_waits_for_the_games_own_switch() {
  Mimic m;
  CHECK(pm::mimic::copies(&m.controller) == std::vector<uint8_t>({1, 7, 4}));
  CHECK(!pm::mimic::place_pick(&m.controller, 4) && !m.next().hasValue);  // the timer has not asked
  put(m.controller, 0x14, true);
  CHECK(!pm::mimic::place_pick(&m.controller, 2) && !m.next().hasValue);  // not one it may copy
  CHECK(!pm::mimic::place_pick(&m.controller, pm::session::kNoType) && !m.next().hasValue);
  CHECK(pm::mimic::place_pick(&m.controller, 4) && m.next().hasValue && m.next().value == 4);
  CHECK(!pm::mimic::place_pick(&m.controller, 1) && m.next().value == 4);  // one already waits
  put(m.controller, 0x18, phasmo::NullableGhostType{});
  put(m.controller, 0x10, int32_t{20});  // the game's own Mimic number, which is not this build's
  CHECK(!pm::mimic::place_pick(&m.controller, 4) && !m.next().hasValue && pm::mimic::copies(&m.controller).empty());
  put(m.controller, 0x10, int32_t{7});
  put(m.controller, 0x20, static_cast<void*>(nullptr));  // before its first copy
  CHECK(pm::mimic::copies(&m.controller).empty() && !pm::mimic::place_pick(&m.controller, 4));
  put(m.controller, 0x20, fake::value_list({4}));
  fake::destroy(&m.controller);
  CHECK(pm::mimic::copies(&m.controller).empty() && !pm::mimic::place_pick(&m.controller, 4));
  CHECK(pm::mimic::copies(nullptr).empty() && !pm::mimic::place_pick(nullptr, 4));
}

// The Mimic's player is never offered or left with the type it is copying now, though the game's own
// random pick (GhostController.MimicCopy) may repeat it. The choice list leaves the current copy out in
// the game's order; with no current copy none is left out; a list holding only the current copy is
// empty. At the switch, place_pick refuses a pick equal to the current copy, however stale its offer.
void mimic_player_cannot_pick_current_copy() {
  CHECK(pm::mimic::offer({1, 7, 4}, 7) == std::vector<uint8_t>({1, 4}));
  CHECK(pm::mimic::offer({1, 7, 4}, pm::session::kNoType) == std::vector<uint8_t>({1, 7, 4}));
  CHECK(pm::mimic::offer({7}, 7).empty());
  Mimic m;  // copies 1, 7 and 4; its ghost is copying 7
  put(m.controller, 0x14, true);  // the timer has asked
  CHECK(!pm::mimic::place_pick(&m.controller, 7) && !m.next().hasValue);  // the type it copies now
  put(m.info, 0x10, int32_t{1});  // it now copies 1
  CHECK(!pm::mimic::place_pick(&m.controller, 1) && !m.next().hasValue);
  CHECK(pm::mimic::place_pick(&m.controller, 4) && m.next().hasValue && m.next().value == 4);
}

// A map entry that did not resolve reads as 0, and 0 is a real ghost type: when the chain to the copying
// ghost's behaviour type is unreadable, nothing may be taken for the current copy. Here the info's type
// entry is missing, so a pick of type 0 is still the ghost player's to make.
void mimic_an_unreadable_chain_hides_no_type() {
  Obj controller, level, ghost, info;
  fake::reset();
  fake::field(map::GhostController_ghostType, 0x10);
  fake::field(map::GhostController_typeChangeDue, 0x14);
  fake::field(map::GhostController_mimicNext, 0x18);
  fake::field(map::GhostController_mimicCopies, 0x20);
  fake::field(map::GhostController_levelController, 0x28);
  fake::field(map::LevelController_currentGhost, 0x10);
  fake::field(map::GhostAI_info, 0x10);
  // GhostInfo_behaviourType is not given: it stays unresolved, and its get would read 0.
  fake::value(map::GhostType_Mimic, 7);
  bind();
  put(controller, 0x10, int32_t{7});
  put(controller, 0x14, true);  // the timer has asked
  put(controller, 0x20, fake::value_list({0, 7}));  // type 0 is one it may copy
  put(controller, 0x28, static_cast<void*>(&level));
  put(level, 0x10, static_cast<void*>(&ghost));
  put(ghost, 0x10, static_cast<void*>(&info));
  CHECK(pm::mimic::copies(&controller) == std::vector<uint8_t>({0, 7}));
  CHECK(pm::mimic::place_pick(&controller, 0));
  const phasmo::NullableGhostType next = phasmo::GhostController::mimicNext.get(&controller);
  CHECK(next.hasValue && next.value == 0);
}

// Each investigator's guess is its death and the type chosen in its own journal, None meaning no guess.
// The numbers are read: None is 9 here, and the game's own None (24) counts as a type.
void the_guess_is_the_journals_choice() {
  fake::reset();
  static int32_t chosen = 0;
  struct Journal {
    static int32_t get(void*, const void*) { return chosen; }
  };
  Obj player, journal, other;
  static Obj journalClass;
  journal.klass = &journalClass;
  fake::field(map::Player_isDead, 0x10);
  fake::field(map::Player_journal, 0x18);
  fake::type(map::JournalController, &journalClass);
  fake::method(map::JournalController_GetGhostType, reinterpret_cast<void*>(&Journal::get));
  fake::value(map::GhostType_None, 9);
  bind();
  put(player, 0x18, static_cast<void*>(&journal));
  chosen = 3;
  CHECK((pm::guess::of(&player) == pm::score::Guess{3, false}));
  chosen = 9;
  CHECK((pm::guess::of(&player) == pm::score::Guess{pm::score::kNoGuess, false}));
  chosen = 24;
  put(player, 0x10, true);
  CHECK((pm::guess::of(&player) == pm::score::Guess{24, true}));
  chosen = -1;
  CHECK((pm::guess::of(&player) == pm::score::Guess{pm::score::kNoGuess, true}));
  chosen = 3;
  put(player, 0x18, static_cast<void*>(&other));  // not a journal
  CHECK((pm::guess::of(&player) == pm::score::Guess{pm::score::kNoGuess, true}));
  fake::destroy(&player);
  CHECK((pm::guess::of(&player) == pm::score::Guess{}));
  CHECK((pm::guess::of(nullptr) == pm::score::Guess{}));
}

// What the ghost player must not see: every renderer on each salt pile and crucifix, and under it, is
// switched off on the ghost player's client, and nothing at all otherwise.
struct Hidden {
  static inline std::map<const void*, void*> found, objectOf, components;
  static inline std::map<const void*, std::vector<void*>> children;
  static inline std::map<const void*, bool> enabled;
  static void* find(void* type, const void*) { return found[type]; }
  static void* game_object(void* c, const void*) { return objectOf[c]; }
  static void* get_components(void* go, void*, const void*) { return components[go]; }
  static int32_t child_count(void* t, const void*) { return static_cast<int32_t>(children[t].size()); }
  static void* child(void* t, int32_t i, const void*) { return children[t][i]; }
  static bool get_enabled(void* r, const void*) { return enabled[r]; }
  static void set_enabled(void* r, bool on, const void*) { enabled[r] = on; }
};

void the_ghost_player_sees_no_salt_or_crucifix() {
  fake::reset();
  Hidden::found.clear();
  Hidden::objectOf.clear();
  Hidden::components.clear();
  Hidden::children.clear();
  Hidden::enabled.clear();
  static Obj saltClass, crucifixClass, rendererClass;
  fake::type(map::SaltSpot, &saltClass);
  fake::type(map::Crucifix, &crucifixClass);
  fake::type(map::Renderer, &rendererClass);
  fake::method(map::UnityObject_FindObjectsOfType, reinterpret_cast<void*>(&Hidden::find));
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_transform));
  fake::method(map::Component_get_gameObject, reinterpret_cast<void*>(&Hidden::game_object));
  fake::method(map::GameObject_GetComponents, reinterpret_cast<void*>(&Hidden::get_components));
  fake::method(map::Transform_get_childCount, reinterpret_cast<void*>(&Hidden::child_count));
  fake::method(map::Transform_GetChild, reinterpret_cast<void*>(&Hidden::child));
  fake::method(map::Renderer_get_enabled, reinterpret_cast<void*>(&Hidden::get_enabled));
  fake::method(map::Renderer_set_enabled, reinterpret_cast<void*>(&Hidden::set_enabled));
  bind();
  // A salt pile with two renderers and a child with one; a destroyed pile; a crucifix with one renderer.
  Obj salt, gone, crucifix, saltAt, saltChildAt, crucifixAt, goneAt, saltObject, childObject, crucifixObject, goneObject;
  Obj r[5];
  void* salts = fake::array(sizeof(void*), 2);
  api::RefArray{salts}.set(0, &salt);
  api::RefArray{salts}.set(1, &gone);
  void* crucifixes = fake::array(sizeof(void*), 1);
  api::RefArray{crucifixes}.set(0, &crucifix);
  Hidden::found[phasmo::SaltSpot::type.type_object()] = salts;
  Hidden::found[phasmo::Crucifix::type.type_object()] = crucifixes;
  g_transform = {{&salt, &saltAt}, {&gone, &goneAt}, {&crucifix, &crucifixAt}};
  Hidden::objectOf = {{&saltAt, &saltObject}, {&saltChildAt, &childObject}, {&crucifixAt, &crucifixObject}, {&goneAt, &goneObject}};
  Hidden::children[&saltAt] = {&saltChildAt};
  auto renderers = [](std::vector<void*> items) {
    void* a = fake::array(sizeof(void*), items.size());
    for (size_t i = 0; i < items.size(); ++i) api::RefArray{a}.set(i, items[i]);
    return a;
  };
  Hidden::components = {{&saltObject, renderers({&r[0], &r[1]})}, {&childObject, renderers({&r[2]})},
                        {&crucifixObject, renderers({&r[3]})}, {&goneObject, renderers({&r[4]})}};
  for (Obj& one : r) Hidden::enabled[&one] = true;
  Hidden::enabled[&r[1]] = false;
  fake::destroy(&gone);
  pm::hide::tick(false);  // not the ghost player's contract
  CHECK(Hidden::enabled[&r[0]] && Hidden::enabled[&r[2]] && Hidden::enabled[&r[3]]);
  pm::hide::tick(true);
  CHECK(!Hidden::enabled[&r[0]] && !Hidden::enabled[&r[1]] && !Hidden::enabled[&r[2]] && !Hidden::enabled[&r[3]]);
  CHECK(Hidden::enabled[&r[4]]);  // the destroyed pile is left alone
}

}  // namespace

int main() {
  yokai_mutes_the_positional_voices_it_would_not_hear();
  each_chance_answers_its_own_roll_only();
  the_aimed_filter_keeps_only_what_passes();
  the_kormos_hears_each_footstep_its_hearing_holds();
  mimic_pick_waits_for_the_games_own_switch();
  mimic_player_cannot_pick_current_copy();
  mimic_an_unreadable_chain_hides_no_type();
  the_guess_is_the_journals_choice();
  the_ghost_player_sees_no_salt_or_crucifix();
  std::printf(g_failures ? "%d check(s) failed\n" : "all glue tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
