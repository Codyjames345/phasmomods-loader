#include "lethe.h"

#include <windows.h>

#include <cmath>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include "GhostAI.h"
#include "GhostHuntState.h"
#include "JournalController.h"
#include "Player.h"
#include "Transform.h"
#include "UnityWorld.h"
#include "actors.h"
#include "behaviour.h"
#include "flame_net.h"
#include "game.h"
#include "game_api.h"
#include "ghosts.h"
#include "held.h"
#include "lethe_rule.h"
#include "log.h"
#include "net.h"
#include "players.h"
#include "runtime.h"

namespace pm::custom::lethe {
namespace {

using phasmo::Quaternion;
using phasmo::Vector3;

// The appearances (GhostAI.Appear / UnAppear, RPCs to every client), the hunt's last player
// (GhostHuntState.lastPlayer) and the journal's media page (sdk/map/JournalController.h): decompiles of
// build 25811160 (2026-10-20).
constexpr ULONGLONG kMediaCheckMs = 1000;
constexpr ULONGLONG kViewingMs = 2500;         // a viewing report counts this long

struct Appearance {
  Vector3 at{};
  Quaternion facing{};
  int form = -1;
  ULONGLONG began = 0, lasted = 0, replayAt = 0;
  bool replayed = false;
};

struct Slot {
  api::Held media;  // the Media it held when first seen (compared only)
  ULONGLONG since = 0;
  bool viewed = false;
};

struct State {
  api::Held ghost;
  int type = 0;
  std::vector<Appearance> appearances;
  bool replaying = false;
  int replayState = -1;  // the AI's state when the replay began
  ULONGLONG replayEnds = 0;
  LetheMemory<api::Held> memory;  // the player its hunt still looks for, and how long they are out of sight
  std::map<std::pair<int, int>, Slot> slots;
  ULONGLONG nextMediaCheck = 0, lastViewing = 0, nextReport = 0, lastTick = 0;
} g;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

bool haunting_lethe(void* ai) { return ai && ai == g.ghost && g.type == kLethe; }

void detour_appear(api::Original<phasmo::GhostAI::Appear> original, void* ai, int32_t form) {
  original(ai, form);
  if (!haunting_lethe(ai) || g.replaying || !game::is_master()) return;
  void* t = game::transform_of(ai);
  Appearance a;
  a.at = game::position(t);
  a.facing = game::rotation(t);
  a.form = form;
  a.began = GetTickCount64();
  g.appearances.push_back(a);
}

void detour_unappear(api::Original<phasmo::GhostAI::UnAppear> original, void* ai) {
  original(ai);
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
    const ShowingStates showing{phasmo::GhostState::hunting(), phasmo::GhostState::appear(),
                                phasmo::GhostState::randomEvent()};
    if (state != g.replayState && lethe_state_shows_itself(state, showing)) {
      g.replaying = false;
    } else if (state != g.replayState || now >= g.replayEnds) {
      phasmo::GhostAI::UnAppear(ghost);
      g.replaying = false;
    }
    return;
  }
  if (game::ghost_state(ghost) == phasmo::GhostState::hunting() || phasmo::GhostAI::isAppeared.get(ghost)) return;
  for (Appearance& a : g.appearances) {
    if (a.replayed || !a.lasted || now < a.replayAt) continue;
    a.replayed = true;
    g.replaying = true;
    g.replayState = game::ghost_state(ghost);
    const phasmo::Vector3 at{a.at.x, a.at.y, a.at.z};
    phasmo::NavMeshAgent::Warp(phasmo::GhostAI::agent.get(ghost), &at);
    game::set_rotation(game::transform_of(ghost), a.facing);
    phasmo::GhostAI::Appear(ghost, a.form);
    g.replayEnds = now + a.lasted;
    PM_LOG("lethe: deja vu, an appearance plays again");
    return;
  }
}

/**
 * The host: in a hunt, the player it still looks for (GhostHuntState.lastPlayer) is forgotten once the
 * hunt's own sight test (its vision sensor, GhostAI.vision, not a ray of ours) has failed for them for a
 * moment: the hunt stops wandering where it lost them, as the game's own kill clears them. The hunt's
 * chase (GhostHuntState.Chase) clears its target the first frame the sight test fails for them
 * and keeps them as its last player, wandering near where it lost them while it is set.
 */
void host_forget_prey(void* ghost, float dt) {
  void* state = game::ghost_state(ghost) == phasmo::GhostState::hunting() ? game::ghost_state_object(ghost) : nullptr;
  void* lost = state ? phasmo::GhostHuntState::lastPlayer.get(state) : nullptr;
  if (lost && !api::alive(lost)) lost = nullptr;
  if (g.memory.forgets(lost, lost && behaviour::hunt_sees(ghost, lost), dt)) {
    behaviour::forget(ghost, lost);
    PM_LOG("lethe: it forgets its prey");
  }
}

/** A journal's media page, or null. */
void* page_of(void* journal) {
  void* page = api::alive(journal) ? phasmo::JournalController::photoPage.get(journal) : nullptr;
  return api::alive(page) ? page : nullptr;
}

/** Every journal's photo page on this client (each player's journal is on every client). */
template <class Visit>
void each_page(Visit visit) {
  game::each_player([&](void* player) {
    void* journal = phasmo::Player::journal.get(player);
    if (void* page = page_of(journal)) visit(journal, page);
  });
}

/** One of the page's three lists of slots: 0 photos, 1 videos, 2 sounds. */
api::RefArray slots_of(void* page, int list) {
  switch (list) {
    case 0: return api::RefArray{phasmo::Journal_PhotoPage::photos.get(page)};
    case 1: return api::RefArray{phasmo::Journal_PhotoPage::videos.get(page)};
    case 2: return api::RefArray{phasmo::Journal_PhotoPage::sounds.get(page)};
    default: return api::RefArray{nullptr};
  }
}

void* slot_of(void* page, int list, int index) {
  const api::RefArray slots = slots_of(page, list);
  return index >= 0 && static_cast<size_t>(index) < slots.size() ? slots[index] : nullptr;
}

/** This client's own journal, or null. */
void* my_journal() {
  void* me = game::local_player();
  return api::alive(me) ? phasmo::Player::journal.get(me) : nullptr;
}

/** Whether this client's investigator has the journal open on its media pages now. */
bool viewing_media() {
  void* journal = my_journal();
  void* page = page_of(journal);
  const bool onMediaPages =
      page && phasmo::JournalController::open.get(journal) && game::active_in_hierarchy(game::game_object_of(page));
  return lethe_looking(onMediaPages, actor_of_player(game::local_player()) != 0);
}

/** Fade one slot on this client: every journal's copy of it, through the game's own removal. */
void forget(int list, int index) {
  each_page([&](void*, void* page) {
    void* slot = slot_of(page, list, index);
    if (!api::alive(slot) || !phasmo::JournalMedia::filled.get(slot)) return;
    phasmo::JournalMedia::RemoveMedia.virtual_call(slot, false);
  });
}

/** The media subjects that record the ghost's activity, as this game build numbers them. */
ActivitySubjects activity_subjects() {
  namespace m = phasmo::MediaSubject;
  return {m::emfSpot(),        m::fingerprint(),     m::footstep(),        m::ghost(),
          m::dirtyWater(),     m::ghostWriting(),    m::usedCrucifix(),    m::dotsGhost(),
          m::ghostOrb(),       m::salt(),            m::freezingTemperature(), m::ghostSmoke(),
          m::camoGhost(),      m::paranormalSound(), m::spiritBox(),       m::ghostHunt(),
          m::bansheeWail(),    m::spiritBoxBreath(), m::motionSensor(),    m::shadowGhost(),
          m::ghostGroan(),     m::ghostLaugh(),      m::ghostTalk(),       m::ghostWhisper(),
          m::obakeFingerprint(), m::burningChapelCrucifix(), m::obakeShapeshift(), m::emf5()};
}

/** The host: time each filled slot of its own journal; fade those nobody looked at in time. */
void host_media(ULONGLONG now) {
  if (now < g.nextMediaCheck) return;
  g.nextMediaCheck = now + kMediaCheckMs;
  void* page = page_of(my_journal());
  if (!page) return;
  const bool looked = viewing_media() || now - g.lastViewing < kViewingMs;
  const ActivitySubjects activities = activity_subjects();
  for (int list = 0; list < 3; ++list) {
    const api::RefArray slots = slots_of(page, list);
    for (size_t i = 0; i < slots.size() && i < 256; ++i) {  // a slot travels as a byte
      const std::pair<int, int> key{list, static_cast<int>(i)};
      void* slot = slots[i];
      if (!api::alive(slot) || !phasmo::JournalMedia::filled.get(slot)) {
        g.slots.erase(key);
        continue;
      }
      void* media = phasmo::JournalMedia::media.get(slot);
      void* values = media ? phasmo::Media::value.get(media) : nullptr;
      if (!values || !lethe_activity(phasmo::MediaValues::evidenceType.get(values), activities)) {  // not of its doing: kept
        g.slots.erase(key);
        continue;
      }
      Slot& s = g.slots[key];
      if (s.media != media) {  // newly taken
        s.media = media;
        s.since = now;
        s.viewed = false;
      }
      if (looked) s.viewed = true;
      if (!lethe_fades((now - s.since) / 1000.0f, s.viewed)) continue;
      const std::vector<uint8_t> message = net_msg::forget(list, static_cast<int>(i));
      net::send(message.data(), message.size(), net::To::Others, true);
      forget(list, static_cast<int>(i));
      g.slots.erase(key);
      PM_LOG("lethe: a piece of media fades from the journal");
    }
  }
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() {
  return api::hook<phasmo::GhostAI::Appear>(&detour_appear, "GhostAI.Appear", api::Twins::CanonicalOnly) &&
         api::hook<phasmo::GhostAI::UnAppear>(&detour_unappear, "GhostAI.UnAppear", api::Twins::CanonicalOnly);
}

void tick(void* ghost, int type) {
  if (ghost != g.ghost || type != g.type) {
    reset();
    g.ghost = ghost;
    g.type = type;
  }
  if (!ghost || type != kLethe || !api::alive(ghost)) return;
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

}  // namespace pm::custom::lethe
