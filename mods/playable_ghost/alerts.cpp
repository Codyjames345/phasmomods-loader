#include "alerts.h"

#include <cmath>
#include <vector>

#include "CursedItems.h"
#include "Equipment.h"
#include "Player.h"
#include "Props.h"
#include "UnityObject.h"
#include "game.h"
#include "Transform.h"
#include "log.h"
#include "runtime.h"

namespace pm::alerts {
namespace {

using phasmo::Vector3;
using perceive::Alert;

constexpr float kSmudgeReach = 6.0f;           // metres: lit sage this close to the ghost smudges it
constexpr float kSummonJump = 4.0f;            // metres the ghost moves in one frame when it is teleported
constexpr float kCircleReach = 3.0f;           // metres from a summoning circle where a summoned ghost appears

using phasmo::EVPRecorder;

session::Session* g_session = nullptr;
bool g_active = false;  // a player controls the ghost in this contract
std::vector<Alert> g_local;
Vector3 g_lastGhost{};
bool g_haveLastGhost = false;

Vector3 where(void* component) { return game::position(game::transform_of(component)); }

void raise(Alert a) {
  if (!g_active || !g_session) return;
  if (g_session->local_is_ghost()) {
    if (g_local.size() < 16) g_local.push_back(std::move(a));
  } else if (g_session->local_is_master()) {
    g_session->send_alert(a);  // the ghost player's client may not run this one
  }
}

void raise_at(uint8_t kind, void* component, uint8_t arg = 0) {
  Alert a;
  a.kind = kind;
  a.arg = arg;
  if (component) {
    const Vector3 p = where(component);
    a.located = true;
    a.x = p.x, a.y = p.y, a.z = p.z;
  }
  raise(std::move(a));
}

float distance_to_ghost(void* component) {
  void* ghost = game::ghost_ai();
  if (!ghost || !game::alive(ghost)) return 1e9f;
  const Vector3 a = where(component), b = game::position(game::transform_of(ghost));
  return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

void detour_crucifix(api::Original<phasmo::Crucifix::CrucifixUseNetworked> original, void* self, bool flag,
                     void* info) {
  const int usesBefore = self ? phasmo::Crucifix::useCount.get(self) : 0;
  original(self, flag, info);
  if (self && phasmo::Crucifix::useCount.get(self) != usesBefore) raise_at(perceive::kCrucifix, self);  // the game accepted it
}

void detour_mirror(api::Original<phasmo::HauntedMirror::UseNetworked> original, void* self, bool on, void* info) {
  original(self, on, info);
  if (on) raise_at(perceive::kMirror, nullptr);  // where the mirror is says nothing to the ghost
}

void detour_sage(api::Original<phasmo::WhiteSage::UseNetworked> original, void* self, bool flag, void* info) {
  original(self, flag, info);
  if (distance_to_ghost(self) <= kSmudgeReach) raise_at(perceive::kSmudged, nullptr);
}

void detour_music_box(api::Original<phasmo::MusicBox::UseNetworked> original, void* self, bool flag, void* info) {
  void* before = self ? phasmo::MusicBox::tween.get(self) : nullptr;
  original(self, flag, info);
  // An accepted use starts a new tween (a call the game rejects returns before it).
  if (self && phasmo::MusicBox::tween.get(self).raw() != before) raise_at(perceive::kMusicBox, self);
}

void detour_tarot(api::Original<phasmo::TarotCards::ChangeState> original, void* self, int32_t card, bool flag,
                  void* info) {
  original(self, card, flag, info);
  if (perceive::tarot_affects_ghost(card)) raise_at(perceive::kTarot, nullptr, static_cast<uint8_t>(card));
}

void detour_voodoo(api::Original<phasmo::VoodooDoll::NetworkedUse> original, void* self, int32_t pin, void* info) {
  original(self, pin, info);
  raise_at(perceive::kVoodoo, nullptr);  // held by an investigator anywhere: where says nothing to the ghost
}

void* detour_wish(api::Original<phasmo::MonkeyPaw::MakeWishNetworked> original, void* self, int32_t wish, int32_t a,
                  void* text, void* info) {
  void* routine = original(self, wish, a, text, info);
  raise_at(perceive::kMonkeyPaw, nullptr);  // held by an investigator anywhere
  return routine;
}

void detour_ouija(api::Original<phasmo::OuijaBoard::PlayMessageSequence> original, void* self, void* message,
                  void* info) {
  original(self, message, info);
  Alert a;
  a.kind = perceive::kOuija;
  a.text = api::to_utf8(message).substr(0, perceive::kMaxAlertText);  // the board answers from anywhere: no place
  raise(std::move(a));
}

void detour_name(api::Original<phasmo::PhraseListenerController::NetworkedReaction> original, void* self) {
  original(self);
  raise_at(perceive::kName, nullptr);
}

void box(void* self) { raise_at(perceive::kSpiritBox, self); }
void detour_box_location(api::Original<EVPRecorder::PlayLocationSound> original, void* self, int32_t a, int32_t b) {
  original(self, a, b);
  box(self);
}
void detour_box_about(api::Original<EVPRecorder::PlayAboutSound> original, void* self, int32_t a, int32_t b) {
  original(self, a, b);
  box(self);
}
void detour_box_difficulty(api::Original<EVPRecorder::PlayDifficultySound> original, void* self, int32_t a,
                           int32_t b) {
  original(self, a, b);
  box(self);
}

void detour_salt(api::Original<phasmo::SaltSpot::SyncSalt> original, void* self, void* info) {
  original(self, info);
  // The game's own checks (a Wraith never steps in salt) decide; the flattened pile shows it did.
  void* flat = self ? phasmo::SaltSpot::flatSalt.get(self) : nullptr;
  if (flat && game::alive(flat) && game::active_self(flat)) raise_at(perceive::kSalt, self);
}

/** The summoning circle teleports the ghost onto itself: a jump that lands beside a circle. */
void watch_for_summons(void* ghost) {
  if (!ghost || !game::alive(ghost)) {
    g_haveLastGhost = false;
    return;
  }
  const Vector3 p = game::position(game::transform_of(ghost));
  const bool jumped = g_haveLastGhost && std::hypot(p.x - g_lastGhost.x, p.z - g_lastGhost.z) > kSummonJump;
  g_lastGhost = p;
  g_haveLastGhost = true;
  if (!jumped) return;
  const api::RefArray circles{phasmo::Object::FindObjectsOfType(phasmo::SummoningCircle::type.type_object())};
  for (size_t i = 0, n = circles.size(); i < n; ++i) {
    void* circle = circles[i];
    if (!circle) continue;
    const Vector3 c = where(circle);
    if (std::hypot(p.x - c.x, p.z - c.z) <= kCircleReach) {
      raise_at(perceive::kSummoned, circle);
      return;
    }
  }
}

}  // namespace

bool create_hooks() {
  // The canonical bodies, as the raw hooks by address were; each hook is named by its entry's key.
  constexpr api::Twins kCanonical = api::Twins::CanonicalOnly;
  return api::hook<phasmo::Crucifix::CrucifixUseNetworked>(&detour_crucifix, nullptr, kCanonical) &&
         api::hook<phasmo::OuijaBoard::PlayMessageSequence>(&detour_ouija, nullptr, kCanonical) &&
         api::hook<phasmo::HauntedMirror::UseNetworked>(&detour_mirror, nullptr, kCanonical) &&
         api::hook<phasmo::WhiteSage::UseNetworked>(&detour_sage, nullptr, kCanonical) &&
         api::hook<phasmo::MusicBox::UseNetworked>(&detour_music_box, nullptr, kCanonical) &&
         api::hook<phasmo::TarotCards::ChangeState>(&detour_tarot, nullptr, kCanonical) &&
         api::hook<phasmo::VoodooDoll::NetworkedUse>(&detour_voodoo, nullptr, kCanonical) &&
         api::hook<phasmo::MonkeyPaw::MakeWishNetworked>(&detour_wish, nullptr, kCanonical) &&
         api::hook<phasmo::PhraseListenerController::NetworkedReaction>(&detour_name, nullptr, kCanonical) &&
         api::hook<EVPRecorder::PlayLocationSound>(&detour_box_location, nullptr, kCanonical) &&
         api::hook<EVPRecorder::PlayAboutSound>(&detour_box_about, nullptr, kCanonical) &&
         api::hook<EVPRecorder::PlayDifficultySound>(&detour_box_difficulty, nullptr, kCanonical) &&
         api::hook<phasmo::SaltSpot::SyncSalt>(&detour_salt, nullptr, kCanonical);
}

void tick(session::Session& s, void* ghost, bool ghostControlled) {
  g_session = &s;
  g_active = ghostControlled && s.ghost_actor() > 0 && game::level_controller();
  if (!g_active) {
    g_local.clear();
    g_haveLastGhost = false;
    return;
  }
  if (s.local_is_ghost() || s.local_is_master()) watch_for_summons(ghost);
}

bool take(perceive::Alert& out) {
  if (g_local.empty()) return false;
  out = std::move(g_local.front());
  g_local.erase(g_local.begin());
  return true;
}

void teardown() {
  g_active = false;
  g_session = nullptr;
  g_local.clear();
}

}  // namespace pm::alerts
