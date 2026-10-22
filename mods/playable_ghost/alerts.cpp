#include "alerts.h"

#include <cmath>
#include <vector>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::alerts {
namespace {

using il2cpp::Vector3;
using perceive::Alert;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr uintptr_t kCrucifixRva = 0x5DC690;   // Crucifix.CrucifixUseNetworked(bool, PhotonMessageInfo)
constexpr uintptr_t kOuijaRva = 0x707540;      // OuijaBoard.PlayMessageSequence(string, PhotonMessageInfo)
constexpr uintptr_t kMirrorRva = 0x6996C0;     // HauntedMirror.UseNetworked(bool, PhotonMessageInfo)
constexpr uintptr_t kSageRva = 0x827360;       // WhiteSage.UseNetworked(bool, PhotonMessageInfo)
constexpr uintptr_t kMusicBoxRva = 0x730D40;   // MusicBox.UseNetworked(bool, PhotonMessageInfo)
constexpr uintptr_t kTarotRva = 0x7BDBA0;      // TarotCards.ChangeState(int card, bool, PhotonMessageInfo)
constexpr uintptr_t kVoodooRva = 0x80AE20;     // VoodooDoll.NetworkedUse(int, PhotonMessageInfo)
constexpr uintptr_t kMonkeyPawRva = 0x6D4240;  // MonkeyPaw.MakeWishNetworked(int, int, string, PhotonMessageInfo)
constexpr uintptr_t kNameRva = 0x2370BF0;      // PhraseListenerController.NetworkedReaction() (the ghost's host)
constexpr uintptr_t kBoxLocationRva = 0x639650;    // EVPRecorder.PlayLocationSound(int, int)
constexpr uintptr_t kBoxAboutRva = 0x6394B0;       // EVPRecorder.PlayAboutSound(int, int)
constexpr uintptr_t kBoxDifficultyRva = 0x6395F0;  // EVPRecorder.PlayDifficultySound(int, int)
constexpr uintptr_t kSaltRva = 0x778060;       // SaltSpot.SyncSalt(PhotonMessageInfo): the ghost stepped in it
constexpr size_t kSaltFlattened = 0x30;        // SaltSpot: the flattened pile, active once the ghost stepped in it
constexpr size_t kCrucifixUses = 0x410;      // Crucifix: times used (changes only when a use is accepted)
constexpr size_t kMusicBoxTween = 0x410;     // MusicBox: its DOTween Sequence, replaced by each accepted use
constexpr float kSmudgeReach = 6.0f;           // metres: lit sage this close to the ghost smudges it
constexpr float kSummonJump = 4.0f;            // metres the ghost moves in one frame when it is teleported
constexpr float kCircleReach = 3.0f;           // metres from a summoning circle where a summoned ghost appears

using BoolRpc_t = void (*)(void*, bool, void*, const void*);
using IntRpc_t = void (*)(void*, int, void*, const void*);
using Tarot_t = void (*)(void*, int, bool, void*, const void*);
using Ouija_t = void (*)(void*, il2cpp::String*, void*, const void*);
using Wish_t = void* (*)(void*, int, int, il2cpp::String*, void*, const void*);
using Plain_t = void (*)(void*, const void*);
using Box_t = void (*)(void*, int, int, const void*);
using Salt_t = void (*)(void*, void*, const void*);

BoolRpc_t g_crucifix = nullptr, g_mirror = nullptr, g_sage = nullptr, g_musicBox = nullptr;
IntRpc_t g_voodoo = nullptr;
Tarot_t g_tarot = nullptr;
Ouija_t g_ouija = nullptr;
Wish_t g_wish = nullptr;
Plain_t g_name = nullptr;
Box_t g_boxLocation = nullptr, g_boxAbout = nullptr, g_boxDifficulty = nullptr;
Salt_t g_salt = nullptr;

struct Api {
  il2cpp::Object* circleType = nullptr;
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
} api;

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

void detour_crucifix(void* self, bool flag, void* info, const void* method) {
  hooks::Guard guard;
  const int usesBefore = self ? il2cpp::field<int>(self, kCrucifixUses) : 0;
  g_crucifix(self, flag, info, method);
  if (self && il2cpp::field<int>(self, kCrucifixUses) != usesBefore) raise_at(perceive::kCrucifix, self);  // the game accepted it
}

void detour_mirror(void* self, bool on, void* info, const void* method) {
  hooks::Guard guard;
  g_mirror(self, on, info, method);
  if (on) raise_at(perceive::kMirror, nullptr);  // where the mirror is says nothing to the ghost
}

void detour_sage(void* self, bool flag, void* info, const void* method) {
  hooks::Guard guard;
  g_sage(self, flag, info, method);
  if (distance_to_ghost(self) <= kSmudgeReach) raise_at(perceive::kSmudged, nullptr);
}

void detour_music_box(void* self, bool flag, void* info, const void* method) {
  hooks::Guard guard;
  void* before = self ? il2cpp::field<void*>(self, kMusicBoxTween) : nullptr;
  g_musicBox(self, flag, info, method);
  // An accepted use starts a new tween (a call the game rejects returns before it).
  if (self && il2cpp::field<void*>(self, kMusicBoxTween) != before) raise_at(perceive::kMusicBox, self);
}

void detour_tarot(void* self, int card, bool flag, void* info, const void* method) {
  hooks::Guard guard;
  g_tarot(self, card, flag, info, method);
  if (perceive::tarot_affects_ghost(card)) raise_at(perceive::kTarot, nullptr, static_cast<uint8_t>(card));
}

void detour_voodoo(void* self, int pin, void* info, const void* method) {
  hooks::Guard guard;
  g_voodoo(self, pin, info, method);
  raise_at(perceive::kVoodoo, nullptr);  // held by an investigator anywhere: where says nothing to the ghost
}

void* detour_wish(void* self, int wish, int a, il2cpp::String* s, void* info, const void* method) {
  hooks::Guard guard;
  void* routine = g_wish(self, wish, a, s, info, method);
  raise_at(perceive::kMonkeyPaw, nullptr);  // held by an investigator anywhere
  return routine;
}

void detour_ouija(void* self, il2cpp::String* message, void* info, const void* method) {
  hooks::Guard guard;
  g_ouija(self, message, info, method);
  Alert a;
  a.kind = perceive::kOuija;
  char text[perceive::kMaxAlertText + 1] = {};
  if (message) il2cpp::to_utf8(message, text, sizeof text);
  a.text = text;  // the board answers from anywhere: no place
  raise(std::move(a));
}

void detour_name(void* self, const void* method) {
  hooks::Guard guard;
  g_name(self, method);
  raise_at(perceive::kName, nullptr);
}

void box(void* self) { raise_at(perceive::kSpiritBox, self); }
void detour_box_location(void* self, int a, int b, const void* method) {
  hooks::Guard guard;
  g_boxLocation(self, a, b, method);
  box(self);
}
void detour_box_about(void* self, int a, int b, const void* method) {
  hooks::Guard guard;
  g_boxAbout(self, a, b, method);
  box(self);
}
void detour_box_difficulty(void* self, int a, int b, const void* method) {
  hooks::Guard guard;
  g_boxDifficulty(self, a, b, method);
  box(self);
}

void detour_salt(void* self, void* info, const void* method) {
  hooks::Guard guard;
  g_salt(self, info, method);
  // The game's own checks (a Wraith never steps in salt) decide; the flattened pile shows it did.
  void* flat = self ? il2cpp::field<void*>(self, kSaltFlattened) : nullptr;
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
  if (!jumped || !api.circleType) return;
  il2cpp::Array* circles = api.findObjects(api.circleType, nullptr);
  for (uintptr_t i = 0; circles && i < circles->max_length; ++i) {
    void* circle = circles->data<void*>()[i];
    if (!circle) continue;
    const Vector3 c = where(circle);
    if (std::hypot(p.x - c.x, p.z - c.z) <= kCircleReach) {
      raise_at(perceive::kSummoned, circle);
      return;
    }
  }
}

template <class T>
bool hook(uintptr_t rva, T detour, T& original, const char* name) {
  return hooks::create(game::game_assembly_address(rva), reinterpret_cast<void*>(detour),
                       reinterpret_cast<void**>(&original), name);
}

}  // namespace

bool resolve() {
  api.circleType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "SummoningCircle"));
  api.findObjects = reinterpret_cast<decltype(api.findObjects)>(
      il2cpp::method_ptr(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectsOfType", 1));
  const bool ok = api.circleType && api.findObjects;
  PG_LOG("alerts: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  return hook(kCrucifixRva, &detour_crucifix, g_crucifix, "Crucifix.CrucifixUseNetworked") &&
         hook(kOuijaRva, &detour_ouija, g_ouija, "OuijaBoard.PlayMessageSequence") &&
         hook(kMirrorRva, &detour_mirror, g_mirror, "HauntedMirror.UseNetworked") &&
         hook(kSageRva, &detour_sage, g_sage, "WhiteSage.UseNetworked") &&
         hook(kMusicBoxRva, &detour_music_box, g_musicBox, "MusicBox.UseNetworked") &&
         hook(kTarotRva, &detour_tarot, g_tarot, "TarotCards.ChangeState") &&
         hook(kVoodooRva, &detour_voodoo, g_voodoo, "VoodooDoll.NetworkedUse") &&
         hook(kMonkeyPawRva, &detour_wish, g_wish, "MonkeyPaw.MakeWishNetworked") &&
         hook(kNameRva, &detour_name, g_name, "PhraseListenerController.NetworkedReaction") &&
         hook(kBoxLocationRva, &detour_box_location, g_boxLocation, "EVPRecorder.PlayLocationSound") &&
         hook(kBoxAboutRva, &detour_box_about, g_boxAbout, "EVPRecorder.PlayAboutSound") &&
         hook(kBoxDifficultyRva, &detour_box_difficulty, g_boxDifficulty, "EVPRecorder.PlayDifficultySound") &&
         hook(kSaltRva, &detour_salt, g_salt, "SaltSpot.SyncSalt");
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

}  // namespace pg::alerts
