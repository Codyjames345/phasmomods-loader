// Offline tests for a ghost's one target (host/content_target.h): the hunt keeps to it as the vanilla
// Banshee's does, whoever steers the ghost, its AI or a player, through the game API over the fake backend,
// and a ghost player's HUD shows it as a Banshee's (mods/playable_ghost/target.h). The fake stands in for
// the game's own hunt code as decompiled (sdk/internals/ghost-ai.md, "The Banshee's one target"): its start
// pick, its test of the ghost's Banshee target, the step that moves the kept player to the nearest, its
// look in sight, Listen and its target pick; the host's hooks run in between, as in the game. Run:
// build\content_target_test.exe
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "GhostAI.h"
#include "GhostHuntState.h"
#include "LevelRoom.h"
#include "Player.h"
#include "check.h"
#include "text_fixture.h"
#include "content.h"
#include "content_target.h"
#include "fake_game.h"
#include "bogeyman_rule.h"
#include "draugr_rule.h"
#include "ghostbook.h"
#include "runtime.h"
#include "session.h"
#include "target.h"

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace content = pm::api::content;

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// A fake managed object: its class pointer first, as a real one's, then room for its fields.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x38]{};
};

template <class T>
void put(Obj& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}
template <class T>
T get(const void* o, int32_t offset) {
  T value{};
  std::memcpy(&value, static_cast<const char*>(o) + offset, sizeof value);
  return value;
}

constexpr int32_t kOutsideMap = 1;  // RoomKind.outsideMap in build 25811160
constexpr int32_t kNoGhost = 24;    // GhostType.None in build 25811160
constexpr int32_t kSpirit = 0;      // GhostType.Spirit in build 25811160: a game type, with no definition
// The fake's field offsets.
constexpr int32_t kHuntGhost = 0x10, kHuntKilled = 0x18;  // GhostHuntState.ghost, .killed
constexpr int32_t kBansheeTarget = 0x10;                   // GhostAI.bansheeTarget
constexpr int32_t kRoom = 0x10, kDead = 0x18;              // Player.currentRoom, .isDead
constexpr int32_t kRoomKind = 0x10;                        // LevelRoom.kind

// The game, as the fake runs it. Players are a, b and c, in the network's order; `g_near` lists them
// nearest the ghost first; `g_seen` those its sight test passes for; `g_heard` those Listen's voice or
// electronics tests pass for. `g_kept` is the hunt's kept player, which the host never reads.
std::vector<void*> g_players, g_near, g_seen, g_heard;
void* g_kept = nullptr;
int g_killCalls = 0, g_keepCalls = 0, g_sightCalls = 0;

bool in_list(const std::vector<void*>& list, void* p) { return std::find(list.begin(), list.end(), p) != list.end(); }
void* ghost_of(void* hunt) { return get<void*>(hunt, kHuntGhost); }
bool killed_in(void* hunt, void* p) { return in_list(fake::list_items_of(get<void*>(hunt, kHuntKilled)), p); }
bool outside(void* p) {
  void* room = get<void*>(p, kRoom);
  return room && get<int32_t>(room, kRoomKind) == kOutsideMap;
}
bool dead(void* p) { return get<bool>(p, kDead); }

// TargetAllows: anyone while the ghost's Banshee target is null, outside the map, dead or killed; else
// that player alone. It reads the field, not the type.
bool target_allows(void* hunt, void* p) {
  void* t = get<void*>(ghost_of(hunt), kBansheeTarget);
  if (!t || outside(t) || dead(t) || killed_in(hunt, t)) return true;
  return p == t;
}
// StartPick: the Banshee target when the test passes and they are alive and in the house; else the first
// player in the list.
bool fake_start_pick(void* hunt, const void*) {
  void* t = get<void*>(ghost_of(hunt), kBansheeTarget);
  g_kept = t && target_allows(hunt, t) && !dead(t) && !outside(t) ? t : g_players.front();
  return g_kept != nullptr;
}
// KeepNearest: the nearest player it may go for, when nearer than the one kept.
void fake_keep_nearest(void* hunt, const void*) {
  ++g_keepCalls;
  for (void* p : g_near) {
    if (p == g_kept) return;  // none nearer may be gone for
    if (!dead(p) && !killed_in(hunt, p) && !outside(p) && target_allows(hunt, p)) {
      g_kept = p;
      return;
    }
  }
}
// SightPick, for a type other than the Banshee's: the first player its sight test passes for.
bool fake_sight_pick(void*, void** found, const void*) {
  ++g_sightCalls;
  *found = nullptr;
  for (void* p : g_players)
    if (in_list(g_seen, p)) {
      *found = p;
      return true;
    }
  return false;
}
void fake_exit(void*, const void*) {}
// SetNewBansheeTarget's own body: for any type but the Banshee's it does nothing.
int g_gameRetargets = 0;
void fake_new_banshee_target(void*, void*, const void*) { ++g_gameRetargets; }

// What the ghost's definition answers for its target. By default g_target, or else g_next (a next thief),
// the first of them neither excluded nor gone, as a definition never names an excluded or dead player. With
// g_rule set it answers by Custom Ghosts' own rule over g_players instead.
enum class Rule { Preset, Bogeyman, Draugr };
Rule g_rule = Rule::Preset;
void* g_target = nullptr;
void* g_next = nullptr;
void* g_foxTarget = nullptr;            // what the other registered ghost's definition answers
int g_targetAsks = 0;                   // how often the host asked it
std::vector<const void*> g_excluded;    // the players it was last told to leave out
bool excluded(const void* p) { return std::find(g_excluded.begin(), g_excluded.end(), p) != g_excluded.end(); }
bool living(void* p) { return p && pm::api::alive(p) && !dead(p); }
// The Bogeyman's lobby: g_players in join order (actors 1, 2, 3), the first the youngest, then the next.
std::vector<int> g_prestige;
void* bogeyman_answer() {
  std::vector<pm::custom::Candidate> lobby;
  std::vector<int32_t> out;
  for (size_t i = 0; i < g_players.size(); ++i) {
    const int32_t actor = static_cast<int32_t>(i) + 1;
    lobby.push_back({g_prestige[i], 1, actor, living(g_players[i])});
    if (excluded(g_players[i])) out.push_back(actor);
  }
  const int chosen = pm::custom::bogeyman_target(lobby, out);
  return chosen < 0 ? nullptr : g_players[static_cast<size_t>(chosen)];
}
// The Draugr's things: g_thieves took one each, the last the most recent, and every thing is still out.
std::vector<void*> g_thieves;
void* draugr_answer() {
  namespace draugr = pm::custom::draugr;
  std::vector<draugr::Thing<>> things(g_thieves.size());
  for (size_t i = 0; i < g_thieves.size(); ++i) {
    things[i].out = true;
    things[i].thief = g_thieves[i];
    things[i].takenAt = 1000 * (i + 1);
  }
  std::vector<const void*> out = g_excluded;
  return draugr::target(things, true, [](void* p) { return living(p); }, out);
}

// The detours the host hooked, by entry key, through a capturing host (as tests/hooks_test.cpp does).
PmHostApi g_host{};
bool (*g_fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;
std::map<std::string, void*> g_detour;
bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!g_fakeHookBody(key, index, detour, original, name)) return false;
  g_detour[key] = detour;
  return true;
}

// The game's re-target (GhostAI.SetNewBansheeTarget), called through its hook, as the kill and the Hanged
// Man card call it with the player about to die.
void new_banshee_target(void* ghost, void* dying) {
  using F = void (*)(void*, void*, const void*);
  reinterpret_cast<F>(g_detour.at("GhostAI.SetNewBansheeTarget"))(ghost, dying, nullptr);
}
// The game's kill: it calls the re-target with the victim before the death begins, and the victim is
// marked dead only later (CompleteDeathSequenceNetworked), so neither isDead nor the killed list is set yet.
bool fake_kill(void* hunt, void* player, bool, float, float, const void*) {
  new_banshee_target(ghost_of(hunt), player);
  return ++g_killCalls > 0;
}

// A contract: a hunt of the ghost, three players in the house, and a registered ghost acting now whose
// target is g_target (the first player, a). Its AI steers it unless a test hands it to a player.
struct Hunt {
  Obj ghost, hunt, house, truck, a, b, c;
  pm::host::ContentTarget holder{fake::acting()};
  int32_t hound = -1;  // the registered ghost's type
  int32_t fox = -1;    // another registered ghost's, whose target callback answers nobody now
  bool hunting = false;  // the hunt runs: its start has been called and its end has not

  Hunt() {
    fake::reset();
    fake::field(map::GhostHuntState_ghost, kHuntGhost);
    fake::field(map::GhostHuntState_killed, kHuntKilled);
    fake::field(map::GhostAI_bansheeTarget, kBansheeTarget);
    fake::field(map::Player_currentRoom, kRoom);
    fake::field(map::Player_isDead, kDead);
    fake::field(map::LevelRoom_kind, kRoomKind);
    fake::value(map::RoomKind_outsideMap, kOutsideMap);
    fake::method(map::GhostHuntState_StartPick, reinterpret_cast<void*>(&fake_start_pick));
    fake::method(map::GhostHuntState_KeepNearest, reinterpret_cast<void*>(&fake_keep_nearest));
    fake::method(map::GhostHuntState_SightPick, reinterpret_cast<void*>(&fake_sight_pick));
    fake::method(map::GhostHuntState_Exit, reinterpret_cast<void*>(&fake_exit));
    fake::method(map::GhostAI_SetNewBansheeTarget, reinterpret_cast<void*>(&fake_new_banshee_target));
    g_host = *fake::host();
    g_fakeHookBody = g_host.hook_body;
    g_host.hook_body = &capture_hook_body;
    g_detour.clear();
    std::string missing;
    pm::api::bind(&g_host, missing);
    content::GhostDef def;
    def.id = "a.hound";
    def.name = content::Text("en", "Hound");
    def.description = def.strength = def.weakness = content::Text("en", "-");
    def.evidence = {"emf", "orbs", "dots"};
    def.target = [](const void* const* list, uint32_t count, void*) -> void* {
      ++g_targetAsks;
      g_excluded.assign(list, list + count);
      if (g_rule == Rule::Bogeyman) return bogeyman_answer();
      if (g_rule == Rule::Draugr) return draugr_answer();
      for (void* p : {g_target, g_next})
        if (p && !excluded(p) && living(p)) return p;
      return nullptr;
    };
    CHECK(content::register_ghost(def) > 0);
    // As Custom Ghosts registers it for every ghost, whether or not the ghost has a target.
    content::GhostDef other = def;
    other.id = "a.fox";
    other.name = content::Text("en", "Fox");
    other.evidence = {"emf", "orbs", "writing"};
    other.target = [](const void* const*, uint32_t, void*) -> void* { return g_foxTarget; };
    CHECK(content::register_ghost(other) > 0);
    fake::commit_content();
    fake::room(1, {1});
    hound = content::type_of("a.hound");
    fox = content::type_of("a.fox");
    fake::contract_ghost(hound, kNoGhost);
    CHECK(holder.missing().empty() && holder.create_hooks());
    CHECK(g_detour.size() == 5);
    put(hunt, kHuntGhost, static_cast<void*>(&ghost));
    put(hunt, kHuntKilled, fake::list({}));
    put(truck, kRoomKind, kOutsideMap);
    for (Obj* p : {&a, &b, &c}) put(*p, kRoom, static_cast<void*>(&house));
    g_players = {&a, &b, &c};
    g_near = {&b, &c, &a};  // b stands nearest the ghost, the target a furthest
    g_seen = {&a, &b, &c};
    g_heard = {};
    g_kept = nullptr;
    g_target = &a;
    g_next = nullptr;
    g_foxTarget = nullptr;
    g_excluded.clear();
    g_rule = Rule::Preset;
    g_prestige = {0, 5, 9};      // a is the youngest, then b, then c
    g_thieves = {&c, &b, &a};    // a took a thing last, b before, c first
    g_killCalls = g_keepCalls = g_sightCalls = g_targetAsks = g_gameRetargets = 0;
  }
  ~Hunt() {
    holder.withdraw();
    g_target = g_next = nullptr;
    g_rule = Rule::Preset;
  }

  void tick() { holder.tick(); }  // the host's next frame
  void* banshee_target() const { return get<void*>(&ghost, kBansheeTarget); }
  // What a ghost player's HUD shows as the target of this ghost, of a type other than the Banshee's.
  void* shown() const { return pm::target::shown(false, hunting, banshee_target(), content::target()); }

  // The hunt's enter runs the start pick through its hook.
  bool start() {
    hunting = true;
    using F = bool (*)(void*, const void*);
    return reinterpret_cast<F>(g_detour.at("GhostHuntState.StartPick"))(&hunt, nullptr);
  }
  // One frame of the chase with no target: KeepNearest (the hunt is not of the Banshee's type), then the
  // target pick: Listen (whom it hears and TargetAllows passes), the kept player when seen, else SightPick.
  void* chase() {
    using Keep = void (*)(void*, const void*);
    reinterpret_cast<Keep>(g_detour.at("GhostHuntState.KeepNearest"))(&hunt, nullptr);
    for (void* p : g_players)
      if (in_list(g_heard, p) && target_allows(&hunt, p)) return p;
    if (in_list(g_seen, g_kept)) return g_kept;
    void* found = &truck;  // anything but null: a look that finds nobody must clear it, as the game's does
    using Sight = bool (*)(void*, void**, const void*);
    const bool any = reinterpret_cast<Sight>(g_detour.at("GhostHuntState.SightPick"))(&hunt, &found, nullptr);
    CHECK(any == (found != nullptr));
    return found;
  }
  bool kill(void* player) { return fake_kill(&hunt, player, false, -1.0f, -1.0f, nullptr); }  // the game's own
  void end() {
    hunting = false;
    using F = void (*)(void*, const void*);
    reinterpret_cast<F>(g_detour.at("GhostHuntState.Exit"))(&hunt, nullptr);
  }
};

// The ghost player's HUD and journal show a ghost's target as they show a Banshee's: "Your target's
// sanity", with the team's sanity beside it while the hunt check measures the team.
void target_shown_like_banshee() {
  using namespace pm::session;
  Status banshee;
  set_sanity(banshee, 60.0f, true, -1.0f);  // the game's own focus: a Banshee's target, 40% sanity
  CHECK(banshee.sanity == 40 && (banshee.flags & kFocusSanity) && banshee.huntSanity == kUnknown);
  Status hound;
  set_sanity(hound, 38.0f, false, 60.0f);  // a target at 40%, the team at 62%
  CHECK(hound.sanity == 40 && (hound.flags & kFocusSanity) && hound.huntSanity == 62);
  CHECK(hunt_sanity(hound) == 62 && hunt_sanity(banshee) == 40);
  Status team;
  set_sanity(team, 38.0f, false, -1.0f);  // no target: the team's, as before
  CHECK(team.sanity == 62 && !(team.flags & kFocusSanity) && team.huntSanity == kUnknown);

  pm::ghostbook::Facts f;
  f.status = hound;
  f.status.huntBelow = 50;
  std::string page = pm::ghostbook::write(f).pages[pm::ghostbook::kOverviewRight].body;
  CHECK(has(page, "Your target's sanity: <b>40%</b>") && has(page, "The team's sanity: <b>62%</b>"));
  CHECK(!has(page, "low enough"));  // the target is low, but the hunt check measures the team
  f.status = banshee;
  f.status.huntBelow = 50;
  page = pm::ghostbook::write(f).pages[pm::ghostbook::kOverviewRight].body;
  CHECK(has(page, "Your target's sanity: <b>40%</b>") && has(page, "low enough") && !has(page, "team's"));

  // In a hunt the HUD shows the target fixed at its start, not the one the definition answers now; the
  // hunt check never measures a target the host holds a hunt to, only a Banshee's.
  Hunt h;
  fake::player_controlled(true);
  CHECK(h.shown() == &h.a);
  CHECK(h.start());
  g_target = &h.b;
  CHECK(h.shown() == &h.a && h.holder.holds(&h.ghost));
  CHECK(!pm::target::hunt_focus(false, true, h.banshee_target()) && pm::target::hunt_focus(true, true, &h.c) == &h.c);
  CHECK(pm::target::shown(true, true, &h.c, nullptr) == nullptr);  // a Banshee's own target is the hunt check's focus
  h.end();
  CHECK(h.shown() == &h.b && !h.holder.holds(&h.ghost));
}

// While the target is alive and in the house the hunt goes for, and kills, the target alone: the game's
// start pick keeps them, KeepNearest leaves them kept though another player stands nearer, its sight finds
// nobody else, and Listen hears nobody else. The kill is always the game's own.
void kills_only_target_while_target_in_house() {
  for (const bool controlled : {false, true}) {  // its AI, then a player controlling it
    Hunt h;
    fake::player_controlled(controlled);
    CHECK(h.start() && h.banshee_target() == &h.a && g_kept == &h.a && g_targetAsks == 1);
    CHECK(h.chase() == &h.a && g_keepCalls == 0);  // seen, though b stands nearer and is seen too
    g_seen = {&h.b, &h.c};
    CHECK(h.chase() == nullptr && g_sightCalls == 0);  // the target out of sight: nobody, as the Banshee's look
    g_heard = {&h.b};
    CHECK(h.chase() == nullptr);  // b is heard, but the game's own test holds Listen to the target
    g_heard = {&h.b, &h.a};
    CHECK(h.chase() == &h.a);
    CHECK(h.kill(&h.a) && g_killCalls == 1);
    CHECK(h.kill(&h.b) && g_killCalls == 2);  // no guard: the game's kill takes whoever the hunt gives it
    h.end();
    CHECK(h.banshee_target() == nullptr);  // the hunt's end clears the field the host set
  }

  // A ghost whose definition names nobody, and a game type, keep the game's rules.
  for (const bool gameType : {false, true}) {
    Hunt n;
    if (gameType) fake::contract_ghost(kSpirit, kSpirit);
    else g_target = nullptr;
    g_heard = {};
    CHECK(n.start() && n.banshee_target() == nullptr && g_kept == &n.a);
    CHECK(n.chase() == &n.b && g_keepCalls == 1);  // KeepNearest moved the kept player to b
    g_seen = {&n.c};
    CHECK(n.chase() == &n.c && g_sightCalls == 1);  // the game's look in sight
    CHECK(!n.holder.holds(&n.ghost));
  }
}

// Once the target is in the truck, dead or killed in this hunt, the game's own test lets Listen go for
// anyone. The kept player stays the target, as the Banshee's does, and sight still finds nobody else.
void kills_anyone_once_target_out() {
  {
    Hunt h;
    CHECK(h.start());
    put(h.a, kRoom, static_cast<void*>(&h.truck));  // in the truck
    g_seen = {&h.b, &h.c};
    CHECK(h.chase() == nullptr && g_kept == &h.a && g_keepCalls == 0 && g_sightCalls == 0);
    g_heard = {&h.c};
    CHECK(h.chase() == &h.c && h.kill(&h.c));
  }
  {
    Hunt h;
    CHECK(h.start());
    put(h.a, kDead, true);
    g_heard = {&h.b};
    CHECK(h.chase() == &h.b);
  }
  {
    Hunt h;
    CHECK(h.start());
    put(h.hunt, kHuntKilled, fake::list({&h.a}));  // killed in this hunt
    g_heard = {&h.c};
    CHECK(h.chase() == &h.c);
  }
  {
    // A target already out when the hunt begins: the game's start pick keeps another player, and Listen
    // goes for anyone, as for a Banshee whose target is in the truck.
    Hunt h;
    put(h.a, kRoom, static_cast<void*>(&h.truck));
    g_players = {&h.b, &h.c, &h.a};
    CHECK(h.start() && h.banshee_target() == &h.a && g_kept == &h.b);
    g_heard = {&h.c};
    CHECK(h.chase() == &h.c);
  }
}

// The target is fixed for the hunt at its start, as a Banshee's is: a thief who changes mid-hunt changes
// nothing in this hunt, and binds from the next one.
void hunt_target_stays_fixed_when_the_thief_changes() {
  Hunt h;
  CHECK(h.start() && h.banshee_target() == &h.a);
  g_target = &h.b;  // the Draugr's thief is now b
  g_heard = {&h.b};
  g_seen = {&h.b};
  CHECK(h.chase() == nullptr && h.banshee_target() == &h.a && g_kept == &h.a);
  g_seen = {&h.a};
  CHECK(h.chase() == &h.a);
  CHECK(g_targetAsks == 1);  // asked once, at the hunt's start
  h.end();
  CHECK(h.start() && h.banshee_target() == &h.b && g_kept == &h.b);  // the next hunt
  CHECK(h.chase() == &h.b);
  // A definition that answers nobody at the start leaves that hunt unrestricted, though it names a
  // player later in the hunt.
  h.end();
  g_target = nullptr;
  CHECK(h.start() && h.banshee_target() == nullptr);
  g_target = &h.c;
  g_heard = {};
  g_seen = {&h.b};
  CHECK(h.chase() == &h.b && g_keepCalls == 1 && h.banshee_target() == nullptr);
}

// The kill re-targets through the game's own SetNewBansheeTarget before the victim's death is registered:
// the definition, which still counts the victim alive, is told to leave them out and answers the next
// player, who binds at once; the kept player stays the victim, as a Banshee's does.
void retargets_at_kill_before_death_registers() {
  Hunt h;
  CHECK(h.start() && g_targetAsks == 1 && g_excluded.empty());
  g_next = &h.c;  // the next thief, once a is left out
  CHECK(h.kill(&h.a) && g_targetAsks == 2 && excluded(&h.a) && g_excluded.size() == 1);
  CHECK(h.banshee_target() == &h.c && g_kept == &h.a && g_gameRetargets == 0);
  put(h.hunt, kHuntKilled, fake::list({&h.a}));  // the kill adds the victim after the re-target
  g_heard = {&h.b, &h.c};
  CHECK(h.chase() == &h.c);  // the new target binds Listen at once
  CHECK(h.shown() == &h.c);  // and the HUD shows them, not the victim
}

// The Hanged Man card calls the same re-target with the player it kills: the definition leaves that
// player out too.
void retarget_by_hanged_man() {
  Hunt h;
  CHECK(h.start());
  g_next = &h.b;
  new_banshee_target(&h.ghost, &h.a);  // the card's victim is the target, alive and in no killed list
  CHECK(excluded(&h.a) && h.banshee_target() == &h.b);
  g_heard = {&h.c, &h.b};
  CHECK(h.chase() == &h.b);
  // A ghost the host holds no hunt for keeps the game's own re-target, which acts only for the Banshee.
  h.end();
  new_banshee_target(&h.ghost, &h.b);
  CHECK(g_gameRetargets == 1 && h.banshee_target() == nullptr);
}

// With nobody else to name, the target stays, as the game's re-target leaves the field when nobody
// qualifies.
void keeps_target_when_nobody_else_qualifies() {
  Hunt h;
  CHECK(h.start() && h.banshee_target() == &h.a);
  CHECK(h.kill(&h.a) && excluded(&h.a) && h.banshee_target() == &h.a);  // it answers nobody
  put(h.b, kDead, true);
  g_next = &h.b;  // the only other candidate is dead
  new_banshee_target(&h.ghost, &h.a);
  CHECK(h.banshee_target() == &h.a);
}

// The Hanged Man card on a player who is not the target: only that player is left out (a design choice;
// a Banshee would move off its target too), so the Bogeyman's own rule keeps the youngest.
void hanged_man_on_non_target_keeps_youngest() {
  Hunt h;
  g_rule = Rule::Bogeyman;
  CHECK(h.start() && h.banshee_target() == &h.a);  // the youngest
  new_banshee_target(&h.ghost, &h.c);              // the card kills c
  CHECK(excluded(&h.c) && g_excluded.size() == 1 && h.banshee_target() == &h.a);
  g_heard = {&h.b, &h.a};
  CHECK(h.chase() == &h.a);  // still the target alone
}

// The same for the Draugr: its most recent thief stays the target when the card kills another thief.
void hanged_man_on_non_target_keeps_thief() {
  Hunt h;
  g_rule = Rule::Draugr;
  CHECK(h.start() && h.banshee_target() == &h.a);  // the most recent thief
  new_banshee_target(&h.ghost, &h.b);              // the card kills b, an earlier thief
  CHECK(excluded(&h.b) && g_excluded.size() == 1 && h.banshee_target() == &h.a);
}

// When the card's or the kill's victim is the target, each rule's next player follows: the next youngest,
// the next most recent thief. Each is chosen while the victim still counts as alive.
void retarget_on_the_target_takes_the_rules_next() {
  {
    Hunt h;
    g_rule = Rule::Bogeyman;
    CHECK(h.start());
    new_banshee_target(&h.ghost, &h.a);  // the Hanged Man on the youngest
    CHECK(h.banshee_target() == &h.b);
  }
  {
    Hunt h;
    g_rule = Rule::Draugr;
    CHECK(h.start());
    CHECK(h.kill(&h.a) && h.banshee_target() == &h.b);  // the kill of the latest thief
    g_heard = {&h.c, &h.b};
    CHECK(h.chase() == &h.b);
  }
}

// The target leaves the game mid-hunt: at the next frame the definition names the next player, with
// nobody left out, as the game re-targets a Banshee whose target is gone; with nobody to name the field
// stays.
void target_leaves_mid_hunt_retargets() {
  Hunt h;
  CHECK(h.start() && h.banshee_target() == &h.a);
  g_next = &h.b;
  fake::destroy(&h.a);  // a leaves
  h.tick();             // the next frame
  CHECK(g_excluded.empty() && h.banshee_target() == &h.b);
  g_heard = {&h.c, &h.b};
  CHECK(h.chase() == &h.b);  // the new target binds Listen
  g_next = nullptr;
  fake::destroy(&h.b);
  h.tick();
  CHECK(h.banshee_target() == &h.b);  // nobody to name: the field stays
}

// Who steers the ghost changes nothing: a player who takes it over, or hands it back to its AI, mid-hunt
// keeps the hunt held to the same target, since its AI and a player hold the same one.
void control_changing_hands_keeps_target() {
  Hunt h;
  fake::player_controlled(true);
  CHECK(h.start() && h.holder.holds(&h.ghost));
  fake::player_controlled(false);  // the player lets go
  h.tick();
  g_seen = {&h.b};
  CHECK(h.banshee_target() == &h.a && h.holder.holds(&h.ghost));
  CHECK(h.chase() == nullptr && g_keepCalls == 0 && g_sightCalls == 0);
  fake::player_controlled(true);  // and takes it again
  h.tick();
  g_seen = {&h.a, &h.b};
  CHECK(h.chase() == &h.a && h.banshee_target() == &h.a);
}

// A ghost that stops acting as a type whose definition names a target mid-hunt (a Mimic's copy of a game
// type) leaves the hunt to the game's own rules from the next frame.
void acting_as_untargeted_type_releases() {
  Hunt h;
  CHECK(h.start() && h.holder.holds(&h.ghost));
  fake::contract_ghost(kSpirit, kSpirit);
  h.tick();
  CHECK(h.banshee_target() == nullptr && !h.holder.holds(&h.ghost));
  g_near = {&h.c, &h.b, &h.a};
  g_seen = {&h.b};
  CHECK(h.chase() == &h.b && g_keepCalls == 1 && g_kept == &h.c && g_sightCalls == 1);
  new_banshee_target(&h.ghost, &h.a);  // a kill now goes to the game's own re-target
  CHECK(g_gameRetargets == 1);
}

// A Mimic's copy of another custom ghost mid-hunt ends the hold as a copy of a game type does: every custom ghost
// registers the target callback, so having one says nothing about whose hold it was. Even a definition that
// names a player then takes over nothing until the next hunt.
void acting_as_another_custom_type_releases() {
  for (const bool namesAPlayer : {false, true}) {
    Hunt h;
    CHECK(h.start() && h.holder.holds(&h.ghost) && h.banshee_target() == &h.a);
    g_foxTarget = namesAPlayer ? &h.b : nullptr;
    fake::contract_ghost(h.fox, kNoGhost);
    h.tick();
    CHECK(h.banshee_target() == nullptr && !h.holder.holds(&h.ghost));
    g_near = {&h.c, &h.b, &h.a};
    g_seen = {&h.b};
    CHECK(h.chase() == &h.b && g_keepCalls == 1 && g_kept == &h.c && g_sightCalls == 1);
    new_banshee_target(&h.ghost, &h.a);  // a kill now goes to the game's own re-target
    CHECK(g_gameRetargets == 1);
    h.end();
    // The next hunt asks the definition it now acts as.
    CHECK(h.start() && h.banshee_target() == g_foxTarget && h.holder.holds(&h.ghost) == namesAPlayer);
  }
  // The same definition, frame after frame, keeps its hold.
  Hunt h;
  CHECK(h.start());
  for (int frame = 0; frame < 3; ++frame) h.tick();
  CHECK(h.holder.holds(&h.ghost) && h.banshee_target() == &h.a);
}

// The hunt check runs outside a hunt and reads the field whatever the type: a Mimic that copied the Banshee got the
// field through the game's own RPC, nothing clears it at its next copy, and the game goes on measuring that
// player. The hold exists only inside a hunt, where the field is no focus of the check's.
void a_mimic_that_was_a_banshee_measures_that_player() {
  using pm::target::hunt_focus;
  using pm::target::shown;
  Hunt h;
  put(h.ghost, kBansheeTarget, static_cast<void*>(&h.c));  // the game's own write, at its Banshee copy
  CHECK(hunt_focus(false, false, h.banshee_target()) == &h.c);
  CHECK(shown(false, false, h.banshee_target(), content::target()) == &h.a);  // what its definition now answers
  CHECK(h.start() && h.banshee_target() == &h.a);  // a hold on this hunt
  CHECK(!hunt_focus(false, true, h.banshee_target()));
  CHECK(shown(false, true, h.banshee_target(), nullptr) == &h.a);
  h.end();
  CHECK(!hunt_focus(false, false, h.banshee_target()));  // no field: the team's
  CHECK(hunt_focus(true, false, &h.c) == &h.c && hunt_focus(true, true, &h.c) == &h.c);  // a Banshee's own
}

// Before the mods stop the host clears a target it set, and only one it set.
void withdraw_clears_the_target_it_set() {
  {
    Hunt h;
    CHECK(h.start() && h.banshee_target() == &h.a);
    h.holder.withdraw();
    CHECK(h.banshee_target() == nullptr && !h.holder.holds(&h.ghost));
    h.tick();
    CHECK(h.banshee_target() == nullptr);
  }
  {
    Hunt h;  // the game's own field, with no hold of the host's
    put(h.ghost, kBansheeTarget, static_cast<void*>(&h.c));
    h.holder.withdraw();
    CHECK(h.banshee_target() == &h.c);
  }
}

// The regression the review found: a ghost standing on its target, with another player nearer and in
// sight since before, still takes its target as the hunt's target and kills them.
void kills_its_target_with_another_player_nearer() {
  Hunt h;
  fake::player_controlled(true);
  CHECK(h.start());
  g_near = {&h.b, &h.a, &h.c};
  g_seen = {&h.b, &h.a};
  for (int frame = 0; frame < 3; ++frame) CHECK(h.chase() == &h.a);
  CHECK(h.kill(&h.a) && g_killCalls == 1);
}

// The Draugr's AI, angered, hunts only its most recent thief while they are in the house and alive: not
// another player it sees nearer, nor one it hears; once the thief is in the truck, anyone it hears.
void ai_draugr_held_to_thief() {
  Hunt h;
  g_rule = Rule::Draugr;
  fake::player_controlled(false);
  CHECK(h.start() && h.banshee_target() == &h.a && g_kept == &h.a);
  g_seen = {&h.b, &h.c};
  g_heard = {&h.c};
  for (int frame = 0; frame < 3; ++frame) CHECK(h.chase() == nullptr);
  CHECK(g_keepCalls == 0 && g_sightCalls == 0);
  g_seen = {&h.a, &h.b};
  CHECK(h.chase() == &h.a);
  put(h.a, kRoom, static_cast<void*>(&h.truck));  // the thief is in the truck
  CHECK(h.chase() == &h.c);
}

// The Bogeyman's AI hunts only the youngest living player, whoever else it sees, and never one it hears
// while the youngest is killable; the youngest dying moves it to the next youngest at once.
void ai_bogeyman_held_to_youngest() {
  Hunt h;
  g_rule = Rule::Bogeyman;
  g_prestige = {7, 2, 9};  // b is the youngest
  fake::player_controlled(false);
  CHECK(h.start() && h.banshee_target() == &h.b && g_kept == &h.b);
  g_seen = {&h.a, &h.c};  // two others in sight, the youngest not
  g_heard = {&h.a};
  CHECK(h.chase() == nullptr && g_sightCalls == 0);
  g_seen = {&h.a, &h.b, &h.c};
  CHECK(h.chase() == &h.b);
  CHECK(h.kill(&h.b) && h.banshee_target() == &h.a);  // the next youngest
}

// A player controlling the Bogeyman and its AI hold the same target, from the same lobby, by the same rule.
void player_and_ai_bogeyman_hold_same_target() {
  std::vector<std::vector<size_t>> held;  // per run: the held player's place in the lobby, at each step
  for (const bool controlled : {false, true}) {
    Hunt h;
    g_rule = Rule::Bogeyman;
    g_prestige = {4, 4, 1};  // c is the youngest; a and b tie
    fake::player_controlled(controlled);
    const auto place = [&] {
      return static_cast<size_t>(std::find(g_players.begin(), g_players.end(), h.banshee_target()) - g_players.begin());
    };
    std::vector<size_t>& run = held.emplace_back();
    CHECK(h.start());
    run.push_back(place());
    g_seen = {&h.a, &h.b};
    CHECK(h.chase() == nullptr);
    CHECK(h.kill(&h.c));
    run.push_back(place());
  }
  CHECK(held[0] == held[1] && held[0] == std::vector<size_t>({2, 0}));  // c, then a, who joined before b
}

}  // namespace

int main() {
  use_text({"mods/playable_ghost/i18n/default.json", "mods/custom_ghosts/i18n/default.json"});
  target_shown_like_banshee();
  kills_only_target_while_target_in_house();
  kills_anyone_once_target_out();
  hunt_target_stays_fixed_when_the_thief_changes();
  retargets_at_kill_before_death_registers();
  retarget_by_hanged_man();
  keeps_target_when_nobody_else_qualifies();
  hanged_man_on_non_target_keeps_youngest();
  hanged_man_on_non_target_keeps_thief();
  retarget_on_the_target_takes_the_rules_next();
  target_leaves_mid_hunt_retargets();
  control_changing_hands_keeps_target();
  acting_as_untargeted_type_releases();
  acting_as_another_custom_type_releases();
  a_mimic_that_was_a_banshee_measures_that_player();
  withdraw_clears_the_target_it_set();
  kills_its_target_with_another_player_nearer();
  ai_draugr_held_to_thief();
  ai_bogeyman_held_to_youngest();
  player_and_ai_bogeyman_hold_same_target();
  std::printf(g_failures ? "%d check(s) failed\n" : "all content target tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
