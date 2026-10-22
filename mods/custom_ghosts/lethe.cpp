#include "lethe.h"

#include <windows.h>

#include <cmath>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include "behaviour.h"
#include "flame_net.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "lethe_rule.h"
#include "log.h"
#include "net.h"
#include "players.h"

namespace pg::custom::lethe {
namespace {

using il2cpp::Quaternion;
using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. REA decompiles of
// GhostAI.Appear/UnAppear, Journal_PhotoPage's removal and JournalMedia.AddMedia/RemoveMedia
// (2026-10-20).
constexpr uintptr_t kAppearRva = 0x2020FB0;    // GhostAI.Appear(int form): RPC to every client
constexpr uintptr_t kUnAppearRva = 0x202E140;  // GhostAI.UnAppear(): RPC to every client
constexpr size_t kGhostAppeared = 0xA8;        // GhostAI: showing itself
// Hunt state: the player it last went for. Its chase (0x214FC40) clears its target (+0x48) the first frame the
// sight test fails for them and keeps them here, wandering near where it lost them (+0x78) while it is set.
constexpr size_t kHuntLastPlayer = 0x58;
constexpr size_t kPlayerJournal = 0xC8;        // Player: JournalController
constexpr size_t kJournalOpen = 0xB8;          // JournalController: open
constexpr size_t kJournalPhotoPage = 0x70;     // JournalController: Journal_PhotoPage
constexpr size_t kPageLists[3] = {0x30, 0x38, 0x40};  // Journal_PhotoPage: photos, videos, sounds (JournalMedia[])
constexpr size_t kMediaItem = 0xA0;            // JournalMedia: the Media it shows
constexpr size_t kMediaFilled = 0xA9;          // JournalMedia: it holds media
constexpr size_t kMediaValues = 0x10;          // Media: MediaValues (what it shows)
constexpr size_t kValuesSubject = 0x1C;        // MediaValues: evidence type (the subject; class dump)
constexpr ULONGLONG kMediaCheckMs = 1000;
constexpr ULONGLONG kViewingMs = 2500;         // a viewing report counts this long

struct Api {
  const il2cpp::Method* removeMedia = nullptr;  // JournalMedia.RemoveMedia(bool), through its override
  bool (*warp)(void*, Vector3, const void*) = nullptr;
  bool ok = false;
} api;

using Appear_t = void (*)(void*, int, const void*);
using UnAppear_t = void (*)(void*, const void*);
Appear_t g_appear = nullptr;
UnAppear_t g_unAppear = nullptr;

struct Appearance {
  Vector3 at{};
  Quaternion facing{};
  int form = -1;
  ULONGLONG began = 0, lasted = 0, replayAt = 0;
  bool replayed = false;
};

struct Slot {
  il2cpp::Held media;  // the Media it held when first seen (compared only)
  ULONGLONG since = 0;
  bool viewed = false;
};

struct State {
  il2cpp::Held ghost;
  int type = 0;
  std::vector<Appearance> appearances;
  bool replaying = false;
  int replayState = -1;  // the AI's state when the replay began
  ULONGLONG replayEnds = 0;
  LetheMemory<il2cpp::Held> memory;  // the player its hunt still looks for, and how long they are out of sight
  std::map<std::pair<int, int>, Slot> slots;
  ULONGLONG nextMediaCheck = 0, lastViewing = 0, nextReport = 0, lastTick = 0;
} g;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

bool haunting_lethe(void* ai) { return ai && ai == g.ghost && g.type == kLethe; }

void detour_appear(void* ai, int form, const void* method) {
  hooks::Guard guard;
  g_appear(ai, form, method);
  if (!haunting_lethe(ai) || g.replaying || !game::is_master()) return;
  void* t = game::transform_of(ai);
  Appearance a;
  a.at = game::position(t);
  a.facing = game::rotation(t);
  a.form = form;
  a.began = GetTickCount64();
  g.appearances.push_back(a);
}

void detour_unappear(void* ai, const void* method) {
  hooks::Guard guard;
  g_unAppear(ai, method);
  if (!haunting_lethe(ai) || g.replaying || !game::is_master() || g.appearances.empty()) return;
  Appearance& a = g.appearances.back();
  if (a.lasted) return;
  const ULONGLONG now = GetTickCount64();
  a.lasted = now > a.began ? now - a.began : 1;
  const float r = std::uniform_real_distribution<float>(0, 1)(g_random);
  a.replayAt = a.began + static_cast<ULONGLONG>((lethe_replay_at(0, r)) * 1000);
}

/** The host: each appearance once more, the same, when it is due and the ghost is free. */
void host_deja_vu(void* ghost, ULONGLONG now) {
  if (g.replaying) {
    // The AI moved on. A hunt or an event of its own owns the model now, so the replay just stops;
    // anything else (wandering off) does not hide it, so the replay ends as it would have.
    const int state = game::ghost_state(ghost);
    if (state != g.replayState && lethe_state_shows_itself(state)) {
      g.replaying = false;
    } else if (state != g.replayState || now >= g.replayEnds) {
      g_unAppear(ghost, nullptr);
      g.replaying = false;
    }
    return;
  }
  if (game::ghost_state(ghost) == game::kStateHunting || il2cpp::field<bool>(ghost, kGhostAppeared)) return;
  for (Appearance& a : g.appearances) {
    if (a.replayed || !a.lasted || now < a.replayAt) continue;
    a.replayed = true;
    g.replaying = true;
    g.replayState = game::ghost_state(ghost);
    api.warp(game::ghost_agent(ghost), a.at, nullptr);
    game::set_rotation(game::transform_of(ghost), a.facing);
    g_appear(ghost, a.form, nullptr);
    g.replayEnds = now + a.lasted;
    PG_LOG("lethe: deja vu, an appearance plays again");
    return;
  }
}

/**
 * The host: in a hunt, the player it still looks for (kHuntLastPlayer) is forgotten once the hunt's own sight
 * test (its vision sensor, GhostAI +0xA0, not a ray of ours) has failed for them for a moment: the hunt stops
 * wandering where it lost them, as the game's own kill clears them.
 */
void host_forget_prey(void* ghost, float dt) {
  void* state = game::ghost_state(ghost) == game::kStateHunting ? game::ghost_state_object(ghost) : nullptr;
  void* lost = state ? il2cpp::field<void*>(state, kHuntLastPlayer) : nullptr;
  if (lost && !game::alive(lost)) lost = nullptr;
  if (g.memory.forgets(lost, lost && behaviour::hunt_sees(ghost, lost), dt)) {
    behaviour::forget(ghost, lost);
    PG_LOG("lethe: it forgets its prey");
  }
}

/** Every journal's photo page on this client (each player's journal is on every client). */
template <class Visit>
void each_page(Visit visit) {
  game::each_player([&](void* player) {
    void* journal = il2cpp::field<void*>(player, kPlayerJournal);
    void* page = journal && game::alive(journal) ? il2cpp::field<void*>(journal, kJournalPhotoPage) : nullptr;
    if (page && game::alive(page)) visit(journal, page);
  });
}

void* slot_of(void* page, int list, int index) {
  auto* slots = il2cpp::field<il2cpp::Array*>(page, kPageLists[list]);
  return slots && index >= 0 && static_cast<uintptr_t>(index) < slots->max_length ? slots->data<void*>()[index] : nullptr;
}

/** Whether this client's player has the journal open on its media pages now. */
bool viewing_media() {
  void* me = game::local_player();
  void* journal = me && game::alive(me) ? il2cpp::field<void*>(me, kPlayerJournal) : nullptr;
  void* page = journal && game::alive(journal) ? il2cpp::field<void*>(journal, kJournalPhotoPage) : nullptr;
  return page && game::alive(page) && il2cpp::field<bool>(journal, kJournalOpen) &&
         game::active_in_hierarchy(game::game_object_of(page));
}

/** Fade one slot on this client: every journal's copy of it, through the game's own removal. */
void forget(int list, int index) {
  each_page([&](void*, void* page) {
    void* slot = slot_of(page, list, index);
    if (!slot || !game::alive(slot) || !il2cpp::field<bool>(slot, kMediaFilled)) return;
    if (const il2cpp::Method* m = il2cpp::virtual_method(slot, api.removeMedia))
      reinterpret_cast<void (*)(void*, bool, const void*)>(*reinterpret_cast<void* const*>(m))(slot, false, m);
  });
}

/** The host: time each filled slot of its own journal; fade those nobody looked at in time. */
void host_media(ULONGLONG now) {
  if (now < g.nextMediaCheck) return;
  g.nextMediaCheck = now + kMediaCheckMs;
  void* me = game::local_player();
  void* journal = me && game::alive(me) ? il2cpp::field<void*>(me, kPlayerJournal) : nullptr;
  void* page = journal && game::alive(journal) ? il2cpp::field<void*>(journal, kJournalPhotoPage) : nullptr;
  if (!page || !game::alive(page)) return;
  const bool looked = viewing_media() || now - g.lastViewing < kViewingMs;
  for (int list = 0; list < 3; ++list) {
    auto* slots = il2cpp::field<il2cpp::Array*>(page, kPageLists[list]);
    for (uintptr_t i = 0; slots && i < slots->max_length && i < 256; ++i) {  // a slot travels as a byte
      const std::pair<int, int> key{list, static_cast<int>(i)};
      void* slot = slots->data<void*>()[i];
      if (!slot || !game::alive(slot) || !il2cpp::field<bool>(slot, kMediaFilled)) {
        g.slots.erase(key);
        continue;
      }
      void* media = il2cpp::field<void*>(slot, kMediaItem);
      void* values = media ? il2cpp::field<void*>(media, kMediaValues) : nullptr;
      if (!values || !lethe_activity(il2cpp::field<int>(values, kValuesSubject))) {  // not of its doing: kept
        g.slots.erase(key);
        continue;
      }
      Slot& s = g.slots[key];
      if (s.media != media) s = Slot{media, now, false};  // newly taken
      if (looked) s.viewed = true;
      if (!lethe_fades((now - s.since) / 1000.0f, s.viewed)) continue;
      const std::vector<uint8_t> message = net_msg::forget(list, static_cast<int>(i));
      net::send(message.data(), message.size(), net::To::Others, true);
      forget(list, static_cast<int>(i));
      g.slots.erase(key);
      PG_LOG("lethe: a piece of media fades from the journal");
    }
  }
}

}  // namespace

bool resolve() {
  il2cpp::Class* media = il2cpp::cls("Assembly-CSharp.dll", "", "JournalMedia");
  api.removeMedia = media ? il2cpp::method(media, "RemoveMedia", 1) : nullptr;
  bool ok = api.removeMedia != nullptr;
  ok &= il2cpp::bind(api.warp, il2cpp::cls("UnityEngine.AIModule.dll", "UnityEngine.AI", "NavMeshAgent"), "Warp", 1);
  api.ok = ok;
  PG_LOG("lethe: resolve %s", ok ? "ok" : "unavailable (it acts as any ghost)");
  return true;  // optional
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kAppearRva), reinterpret_cast<void*>(&detour_appear),
                       reinterpret_cast<void**>(&g_appear), "GhostAI.Appear") &&
         hooks::create(game::game_assembly_address(kUnAppearRva), reinterpret_cast<void*>(&detour_unappear),
                       reinterpret_cast<void**>(&g_unAppear), "GhostAI.UnAppear");
}

void tick(void* ghost, int type) {
  if (!api.ok) return;
  if (ghost != g.ghost || type != g.type) {
    reset();
    g.ghost = ghost;
    g.type = type;
  }
  if (!ghost || type != kLethe || !game::alive(ghost)) return;
  const ULONGLONG now = GetTickCount64();
  const float dt = g.lastTick ? (now - g.lastTick) / 1000.0f : 0;
  g.lastTick = now;
  if (game::is_master()) {
    host_deja_vu(ghost, now);
    host_forget_prey(ghost, dt);
    host_media(now);
  } else if (now >= g.nextReport && viewing_media()) {
    g.nextReport = now + 1000;
    const std::vector<uint8_t> message = net_msg::header(net_msg::kViewing);
    net::send(message.data(), message.size(), net::To::Master, false);
  }
}

void receive(int sender, const uint8_t* data, size_t size) {
  const uint8_t kind = net_msg::kind_of(data, size);
  if (kind == net_msg::kViewing && game::is_master()) {
    g.lastViewing = GetTickCount64();
  } else if (int list = 0, slot = 0; sender == game::master_actor() && net_msg::read_forget(data, size, list, slot)) {
    forget(list, slot);
  }
}

void reset() { g = State{}; }

}  // namespace pg::custom::lethe
