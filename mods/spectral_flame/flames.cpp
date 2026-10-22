#include "flames.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flame_rules.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "net.h"

namespace pg::flame::flames {
namespace {

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. Read in REA
// decompiles of FireSource.Start/Update/BlowOutFire, Candle.Update/PutDown, Candlebra.Start,
// PlayerSanity.Update (2026-10-20), and Equipment.PlaceNetworked/GrabNetworked and
// GhostInfo.SyncValues/SyncEvidence (2026-10-21, after a contract showed the placed read wrong).
constexpr size_t kLevelGhostRoom = 0x38;      // LevelController: the room the ghost is in
constexpr size_t kLevelRooms = 0x50;          // LevelController: LevelRoom[]
constexpr size_t kLevelFireSources = 0xC0;    // LevelController: List<FireSource> (each adds itself in Start)
constexpr size_t kFireLight = 0x38;           // FireSource: its Light
constexpr size_t kFireFlame = 0x30;           // FireSource: its flame GameObject
constexpr size_t kFireOwner = 0x40;           // FireSource: the Equipment it belongs to
constexpr size_t kFireLit = 0xB0;             // FireSource: burning
constexpr size_t kCandleObjects = 0x3D0;      // Candle: CandleObject[] (each +0x18 its FireSource)
constexpr size_t kCandleObjectFire = 0x18;
constexpr size_t kCandleRemaining = 0x464;    // Candle: seconds left to burn
constexpr size_t kEquipmentStoreItem = 0x180; // Equipment: StoreItem (null for a house's own candles)
// Equipment: set down. PlaceNetworked sets it and GrabNetworked clears it, on every client. (Candle's
// own world model, +0x410, is switched by the Tier 3 lantern only.)
constexpr size_t kEquipmentPlaced = 0x19C;
constexpr size_t kStoreItemTier = 0x1C;       // StoreItem: tier, 0..2
constexpr size_t kCandelabraCandles = 0x20;   // Candlebra: Candle[]
constexpr size_t kCandelabraLight = 0x28;     // Candlebra: its Light
constexpr size_t kAiInfo = 0x38;              // GhostAI: GhostInfo
// GhostInfo: List<int>, the evidence the ghost gives as the difficulty left it (SyncEvidence, every
// client); empty when the difficulty gives none. The host's GhostController+0x30 is the same list.
constexpr size_t kInfoEvidence = 0x30;
constexpr size_t kSanityCandles = 0x60;       // PlayerSanity: List<Candle> near the player
constexpr uintptr_t kSanityUpdateRva = 0xDA9430;  // PlayerSanity.Update
constexpr uintptr_t kBlowOutRva = 0x652090;       // FireSource.BlowOutFire(bool byGhost)
constexpr int kStateHunting = 2, kStateAppear = 8, kStateRandomEvent = 14;

constexpr ULONGLONG kScanMs = 1000;        // looking for new fire sources and candelabras
constexpr ULONGLONG kSendMs = 250;         // the host's message to every client
constexpr ULONGLONG kStaleMs = 3000;       // a client with no message for this long shows none
constexpr ULONGLONG kRoomMs = 2000;        // a still flame's room is looked up again this often
constexpr float kRoomMove = 0.25f;         // metres a flame moves before its room is looked up again
constexpr float kShowStep = 0.004f;        // blueness change worth redrawing
constexpr int kMaxDepth = 8;               // how deep a flame's objects are searched for its renderers
constexpr ULONGLONG kReportMs = 5000;      // the host's flame report in mods.log (FINDINGS 20)
constexpr int kReportFlames = 12;          // lit flames listed per report

struct Color {
  float r, g, b, a;
};

struct Api {
  il2cpp::Class* candle = nullptr;
  il2cpp::Object* candelabraType = nullptr;
  il2cpp::Object* rendererType = nullptr;
  il2cpp::Object* lightType = nullptr;
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  void* (*sharedMaterial)(void*, const void*) = nullptr;
  void* (*material)(void*, const void*) = nullptr;
  Color (*lightColor)(void*, const void*) = nullptr;
  void (*setLightColor)(void*, const Color*, const void*) = nullptr;
  void* (*photonView)(void*, const void*) = nullptr;
  int (*viewId)(void*, const void*) = nullptr;
  int (*propertyId)(il2cpp::String*) = nullptr;            // Shader.PropertyToID (icall)
  bool (*hasProperty)(void*, int) = nullptr;               // Material.HasProperty (icall)
  void (*getColor)(void*, int, Color*) = nullptr;          // Material.GetColorImpl_Injected (icall)
  void (*setColor)(void*, int, const Color*) = nullptr;    // Material.SetColorImpl_Injected (icall)
  int candleTint = 0, lanternTint = 0;  // "_TintColour" (Candle Flame shader), "_Color_2" (Lantern Flame)
  bool ok = false;
} api;

struct Tinted {  // a flame material, recoloured
  il2cpp::Held material;
  int property;
  Color original;
};
struct Lit {  // a light, recoloured
  il2cpp::Held light;
  Color original;
};
// The lights a candle casts besides its flames' own: every Light on the candle (a Tier 2 Firelight
// has one for the whole candle, "Point light.1", besides one per flame), blue with its bluest flame.
// Kept per candle, so a light its flames share has its own colour captured once.
struct Look {
  il2cpp::Held candle;
  std::vector<Lit> lights;
  float shown = 0;
  bool found = false;  // its lights have been looked for
};

struct Flame {
  il2cpp::Held fire;    // FireSource
  il2cpp::Held candle;  // Candle it belongs to
  Kind kind = Kind::Tier1;
  int view = 0;            // PhotonView id: what the host's message names it by
  float blueness = 0;      // host: worked out here; others: as received
  bool full = false;       // host: it has been fully blue since it was set down and lit
  float shown = 0;         // what its colours show
  il2cpp::Held room;
  il2cpp::Vector3 roomAt{};
  ULONGLONG roomAgain = 0;
  bool found = false;      // its flame renderers have been looked for
  std::vector<Tinted> materials;
};

struct Candelabra {
  il2cpp::Held object;
  std::vector<il2cpp::Held> candles;
  Lit light{};  // its own Light, blue with its bluest candle
  float shown = 0;
};

il2cpp::Held g_level;  // the LevelController the flames belong to
std::vector<Flame> g_flames;
std::vector<Look> g_looks;
std::vector<Candelabra> g_candelabras;
ULONGLONG g_nextScan = 0, g_nextSend = 0, g_lastHeard = 0, g_nextReport = 0;
std::unordered_map<int, float> g_heard;  // view id -> blueness, from the host's last message
std::unordered_map<void*, float> g_candleBlue;  // Candle -> its bluest flame, for the sanity detour
std::mt19937 g_random{std::random_device{}()};
bool g_enabled = false;
bool g_givesFlame = false;  // host: the ghost gives Spectral Flame, as of the last step

using SanityUpdate_t = void (*)(void*, const void*);
SanityUpdate_t g_sanityUpdate = nullptr;
using BlowOut_t = void (*)(void*, bool, const void*);
BlowOut_t g_blowOut = nullptr;  // FireSource.BlowOutFire, past the detour

bool alive(void* o) { return o && game::alive(o); }

template <class F>
void each_item(il2cpp::List* list, F visit) {
  for (int i = 0; list && list->items && i < list->size; ++i)
    if (void* item = list->items->data<void*>()[i]) visit(item);
}

/** Recolour a light or material back to `original` or towards blue. */
void paint(const Lit& l, float blueness) {
  if (!alive(l.light)) return;
  const Rgb warm{l.original.r, l.original.g, l.original.b};
  const float b = brightness(warm);
  const Rgb c = tint(warm, light_pale(b), light_deep(b), blueness);
  const Color out{c.r, c.g, c.b, l.original.a};
  api.setLightColor(l.light, &out, nullptr);
}

void paint(const Tinted& t, float blueness) {
  if (!alive(t.material)) return;
  const Rgb warm{t.original.r, t.original.g, t.original.b};
  const Rgb c = tint(warm, flame_pale(kFlameDeep), kFlameDeep, blueness);
  const Color out{c.r, c.g, c.b, t.original.a};
  api.setColor(t.material, t.property, &out);
}

void paint(const Look& l, float blueness) {
  for (const Lit& light : l.lights) paint(light, blueness);
}

void release(Flame& f) {
  for (const Tinted& t : f.materials) paint(t, 0);
  f = Flame{};
}

void release(Candelabra& c) {
  if (c.light.light) paint(c.light, 0);
}

void clear_all() {
  for (Flame& f : g_flames) release(f);
  for (const Look& l : g_looks) paint(l, 0);
  for (Candelabra& c : g_candelabras) release(c);
  g_flames.clear();
  g_looks.clear();
  g_candelabras.clear();
  g_candleBlue.clear();
  g_heard.clear();
  g_level = nullptr;
  g_givesFlame = false;
}

/** The flame materials in a transform's subtree, added to `f` once each. */
void collect(Flame& f, void* transform, int depth) {
  if (!transform || depth > kMaxDepth) return;
  if (void* renderer = game::get_component(transform, api.rendererType)) {
    void* shared = api.sharedMaterial(renderer, nullptr);
    int property = 0;
    if (shared && api.hasProperty(shared, api.candleTint)) property = api.candleTint;
    else if (shared && api.hasProperty(shared, api.lanternTint)) property = api.lanternTint;
    if (property) {
      void* material = api.material(renderer, nullptr);  // this renderer's own copy
      bool known = false;
      for (const Tinted& t : f.materials) known |= t.material == material;
      if (material && !known) {
        Tinted t{material, property, {}};
        api.getColor(material, property, &t.original);
        f.materials.push_back(std::move(t));
      }
    }
  }
  for (int i = 0; i < game::child_count(transform); ++i) collect(f, game::child(transform, i), depth + 1);
}

void find_visuals(Flame& f, bool onlyFlameOfItsCandle) {
  f.found = true;
  collect(f, game::transform_of(f.fire), 0);
  if (void* flame = il2cpp::field<void*>(f.fire, kFireFlame); alive(flame)) collect(f, game::object_transform(flame), 0);
  // A lantern's flame sits on each of its models (held, set down, in the truck), apart from its fire
  // source: a candle with one flame has all of them for that flame.
  if (onlyFlameOfItsCandle) collect(f, game::transform_of(f.candle), 0);
  PG_LOG("flames: flame %d shows through %zu materials", f.view, f.materials.size());
}

void add_light(Look& l, void* light) {
  if (!alive(light)) return;
  for (const Lit& known : l.lights)
    if (known.light == light) return;
  // A candelabra's own light is painted with its bluest candle, never by one candle as well.
  for (const Candelabra& c : g_candelabras)
    if (c.light.light == light) return;
  l.lights.push_back({light, api.lightColor(light, nullptr)});
}

/** The lights in a transform's subtree, added to `l` once each. */
void collect(Look& l, void* transform, int depth) {
  if (!transform || depth > kMaxDepth) return;
  add_light(l, game::get_component(transform, api.lightType));
  for (int i = 0; i < game::child_count(transform); ++i) collect(l, game::child(transform, i), depth + 1);
}

void find_look(Look& l) {
  l.found = true;
  collect(l, game::transform_of(l.candle), 0);
  auto* objects = il2cpp::field<il2cpp::Array*>(l.candle, kCandleObjects);
  for (uintptr_t i = 0; objects && i < objects->max_length; ++i) {
    void* o = objects->data<void*>()[i];
    void* fire = o ? il2cpp::field<void*>(o, kCandleObjectFire) : nullptr;
    if (!alive(fire)) continue;
    collect(l, game::transform_of(fire), 0);
    add_light(l, il2cpp::field<void*>(fire, kFireLight));
  }
  PG_LOG("flames: a candle shows through %zu lights", l.lights.size());
}

int fires_of(void* candle) {
  auto* objects = il2cpp::field<il2cpp::Array*>(candle, kCandleObjects);
  int n = 0;
  for (uintptr_t i = 0; objects && i < objects->max_length; ++i)
    if (void* o = objects->data<void*>()[i]; o && il2cpp::field<void*>(o, kCandleObjectFire)) ++n;
  return n;
}

const Candelabra* candelabra_of(void* candle) {
  for (const Candelabra& c : g_candelabras)
    if (std::find(c.candles.begin(), c.candles.end(), candle) != c.candles.end()) return &c;
  return nullptr;
}

/** Fire sources and candelabras that joined the contract since the last look, and those that left. */
void scan(void* level) {
  // Candelabras first: their candles are counted although they belong to no Firelight.
  il2cpp::Array* found = api.findObjects(api.candelabraType, nullptr);
  for (uintptr_t i = 0; found && i < found->max_length; ++i) {
    void* object = found->data<void*>()[i];
    if (!alive(object)) continue;
    bool known = false;
    for (const Candelabra& c : g_candelabras) known |= c.object == object;
    if (known) continue;
    Candelabra c;
    c.object = object;
    auto* candles = il2cpp::field<il2cpp::Array*>(object, kCandelabraCandles);
    for (uintptr_t k = 0; candles && k < candles->max_length; ++k)
      if (void* candle = candles->data<void*>()[k]) c.candles.emplace_back(candle);
    if (void* light = il2cpp::field<void*>(object, kCandelabraLight); alive(light))
      c.light = {light, api.lightColor(light, nullptr)};
    g_candelabras.push_back(std::move(c));
  }
  // Flames whose objects are gone.
  for (Flame& f : g_flames)
    if (!alive(f.fire) || !alive(f.candle)) release(f);
  g_flames.erase(std::remove_if(g_flames.begin(), g_flames.end(), [](const Flame& f) { return !f.fire; }), g_flames.end());
  g_looks.erase(std::remove_if(g_looks.begin(), g_looks.end(), [](const Look& l) { return !alive(l.candle); }), g_looks.end());
  // New fire sources: those of a Firelight (a Candle sold in the store) or of a candelabra's candle.
  each_item(il2cpp::field<il2cpp::List*>(level, kLevelFireSources), [](void* fire) {
    for (const Flame& f : g_flames)
      if (f.fire == fire) return;
    void* owner = il2cpp::field<void*>(fire, kFireOwner);
    if (!alive(owner) || !il2cpp::is_class(owner, "Candle")) return;
    const Candelabra* holder = candelabra_of(owner);
    void* storeItem = il2cpp::field<void*>(owner, kEquipmentStoreItem);
    if (!holder && !storeItem) return;  // a summoning circle's or an altar's candle
    void* view = api.photonView(fire, nullptr);
    if (!view) return;
    Flame f;
    f.fire = fire;
    f.candle = owner;
    f.view = api.viewId(view, nullptr);
    const int tier = storeItem ? il2cpp::field<int>(storeItem, kStoreItemTier) : 0;
    f.kind = holder ? Kind::Candelabra : tier == 2 ? Kind::Tier3 : tier == 1 ? Kind::Tier2 : Kind::Tier1;
    g_flames.push_back(std::move(f));
  });
}

void* room_of(Flame& f, void* level, ULONGLONG now) {
  const il2cpp::Vector3 at = game::position(game::transform_of(f.fire));
  const float dx = at.x - f.roomAt.x, dy = at.y - f.roomAt.y, dz = at.z - f.roomAt.z;
  if (now < f.roomAgain && dx * dx + dy * dy + dz * dz < kRoomMove * kRoomMove) return f.room;
  f.roomAt = at;
  f.roomAgain = now + kRoomMs;
  f.room = nullptr;
  auto* rooms = il2cpp::field<il2cpp::Array*>(level, kLevelRooms);
  for (uintptr_t i = 0; rooms && i < rooms->max_length && !f.room; ++i)
    if (void* room = rooms->data<void*>()[i]; alive(room) && game::in_room(room, at)) f.room = room;
  return f.room;
}

/** The host's view of the ghost: its room, whether it is calm, and whether it gives Spectral Flame. */
struct GhostNow {
  il2cpp::Held room;
  int state = -1;
  bool calm = false;
  bool hasEvidence = false;
  int given[8]{};  // the evidence the ghost gives, as the difficulty left it
  int givenCount = 0;
};

GhostNow ghost_now(void* level) {
  GhostNow g;
  void* ai = game::ghost_ai();
  if (!alive(ai)) return g;
  g.state = game::ghost_state(ai);
  g.calm = g.state != kStateHunting && g.state != kStateAppear && g.state != kStateRandomEvent;
  g.room = il2cpp::field<void*>(level, kLevelGhostRoom);
  void* info = il2cpp::field<void*>(ai, kAiInfo);
  auto* given = alive(info) ? il2cpp::field<il2cpp::List*>(info, kInfoEvidence) : nullptr;
  for (int i = 0; given && given->items && i < given->size; ++i) {
    const int evidence = given->items->data<int>()[i];
    g.hasEvidence |= evidence == kEvidence;
    if (g.givenCount < static_cast<int>(std::size(g.given))) g.given[g.givenCount++] = evidence;
  }
  return g;
}

bool placed(const Flame& f) { return f.kind == Kind::Candelabra || il2cpp::field<bool>(f.candle, kEquipmentPlaced); }

int room_index(void* level, void* room) {
  auto* rooms = il2cpp::field<il2cpp::Array*>(level, kLevelRooms);
  for (uintptr_t i = 0; room && rooms && i < rooms->max_length; ++i)
    if (rooms->data<void*>()[i] == room) return static_cast<int>(i);
  return -1;
}

const char* kind_name(Kind kind) {
  switch (kind) {
    case Kind::Tier2: return "tier 2";
    case Kind::Tier3: return "tier 3";
    case Kind::Candelabra: return "candelabra";
    case Kind::Tier1:
    default: return "tier 1";
  }
}

// What the host sees every few seconds while a flame set down is burning, so mods.log shows which
// step stops a flame turning blue: the ghost's room and state, the evidence it gives, and each such
// flame's room and blueness. Until FINDINGS 20 is closed; quiet otherwise.
void report(void* level, const GhostNow& ghost, ULONGLONG now) {
  if (now < g_nextReport) return;
  int lit = 0;
  for (const Flame& f : g_flames) lit += il2cpp::field<bool>(f.fire, kFireLit) && placed(f) ? 1 : 0;
  if (!lit) return;
  g_nextReport = now + kReportMs;
  char given[48] = "";
  for (int i = 0, n = 0; i < ghost.givenCount && n < static_cast<int>(sizeof given); ++i)
    n += std::snprintf(given + n, sizeof given - n, i ? ",%d" : "%d", ghost.given[i]);
  auto* sources = il2cpp::field<il2cpp::List*>(level, kLevelFireSources);
  PG_LOG("flames: ghost in room %d, state %d (calm %d), gives [%s] (flame %d); %zu flames (%d set down and lit) of %d fire sources, %zu candelabras",
         room_index(level, ghost.room), ghost.state, ghost.calm, given, ghost.hasEvidence, g_flames.size(), lit,
         sources ? sources->size : -1, g_candelabras.size());
  int listed = 0;
  for (Flame& f : g_flames) {
    if (!il2cpp::field<bool>(f.fire, kFireLit) || !placed(f)) continue;
    if (listed++ == kReportFlames) break;
    PG_LOG("flames:   %s flame %d: placed %d, in room %d, %.0f%% blue, burns %.0fs more, %zu materials%s",
           kind_name(f.kind), f.view, placed(f), room_index(level, room_of(f, level, now)), f.blueness * 100,
           il2cpp::field<float>(f.candle, kCandleRemaining), f.materials.size(),
           f.found ? "" : " (not looked for yet)");
  }
}

void host_step(void* level, float dt, ULONGLONG now) {
  const GhostNow ghost = ghost_now(level);
  g_givesFlame = ghost.hasEvidence;
  report(level, ghost, now);
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  for (Flame& f : g_flames) {
    Inputs in;
    in.lit = il2cpp::field<bool>(f.fire, kFireLit);
    in.placed = placed(f);
    in.ghostInRoom = in.lit && in.placed && ghost.room && room_of(f, level, now) == ghost.room;
    in.hasEvidence = ghost.hasEvidence;
    in.kind = f.kind;
    const State next = step({f.blueness, f.full}, in, dt);
    f.blueness = next.blueness;
    f.full = next.full;
    // Now and then the calm ghost in its room puts out a fully blue flame (BlowOutFire, as the game's own).
    if (in.ghostInRoom && ghost.calm && roll(g_random) < blow_out_chance(f.blueness, dt)) {
      g_blowOut(f.fire, true, nullptr);
      PG_LOG("flames: the ghost blew out a flame %.0f%% blue", f.blueness * 100);
    }
  }
  if (now < g_nextSend) return;
  g_nextSend = now + kSendMs;
  std::vector<std::pair<int, float>> blue;
  for (const Flame& f : g_flames)
    if (f.blueness > 0) blue.emplace_back(f.view, f.blueness);
  size_t sent = 0;
  do {  // at least once, so an all-clear reaches every client
    const size_t n = std::min(kMaxPerMessage, blue.size() - sent);
    const std::vector<uint8_t> message = encode({blue.begin() + sent, blue.begin() + sent + n});
    net::send(message.data(), message.size(), net::To::Others, false);
    sent += n;
  } while (sent < blue.size());
}

void client_step(ULONGLONG now) {
  const bool fresh = now - g_lastHeard < kStaleMs;
  for (Flame& f : g_flames) {
    const auto it = g_heard.find(f.view);
    f.blueness = fresh && it != g_heard.end() ? it->second : 0;
  }
}

void show_and_burn(float dt) {
  g_candleBlue.clear();
  std::unordered_map<void*, bool> burning;  // Candle -> any of its flames lit
  for (Flame& f : g_flames) {
    float& candle = g_candleBlue[f.candle];
    candle = std::max(candle, f.blueness);
    burning[f.candle] = burning[f.candle] || il2cpp::field<bool>(f.fire, kFireLit);
    if (std::fabs(f.blueness - f.shown) >= kShowStep || (f.blueness == 0 && f.shown != 0)) {
      if (!f.found) find_visuals(f, fires_of(f.candle) == 1);
      for (const Tinted& t : f.materials) paint(t, f.blueness);
      f.shown = f.blueness;
    }
  }
  // Each candle's lights, with its bluest flame; looked for when it first turns blue.
  for (const auto& [candle, blueness] : g_candleBlue) {
    if (blueness <= 0) continue;
    bool known = false;
    for (const Look& l : g_looks) known |= l.candle == candle;
    if (!known) g_looks.push_back(Look{candle});
  }
  for (Look& l : g_looks) {
    const auto it = g_candleBlue.find(l.candle);
    const float blueness = it != g_candleBlue.end() ? it->second : 0;
    if (std::fabs(blueness - l.shown) < kShowStep && !(blueness == 0 && l.shown != 0)) continue;
    if (!l.found) find_look(l);
    paint(l, blueness);
    l.shown = blueness;
  }
  // A blue candle burns down faster, by its bluest flame (once per candle, however many flames it
  // has): the extra time comes off the candle's own burn time, which Candle.Update counts down while
  // it is lit (and puts it out at zero).
  for (const auto& [candle, blueness] : g_candleBlue) {
    if (blueness <= 0 || !burning[candle]) continue;
    const float left = il2cpp::field<float>(candle, kCandleRemaining);
    il2cpp::set_field<float>(candle, kCandleRemaining, left - dt * (burn_multiplier(blueness) - 1));
  }
  for (Candelabra& c : g_candelabras) {
    float bluest = 0;
    for (void* candle : c.candles)
      if (auto it = g_candleBlue.find(candle); it != g_candleBlue.end()) bluest = std::max(bluest, it->second);
    if (c.light.light && std::fabs(bluest - c.shown) >= kShowStep) {
      paint(c.light, bluest);
      c.shown = bluest;
    }
  }
}

// PlayerSanity.Update: the first lit candle in the player's list eases the drain. A flame more than
// half blue gives no comfort, so its candle is left out of the list for the call and put back after.
void detour_sanity_update(void* sanity, const void* method) {
  hooks::Guard guard;
  auto* list = g_enabled && sanity ? il2cpp::field<il2cpp::List*>(sanity, kSanityCandles) : nullptr;
  if (!list || !list->items || list->size <= 0 || g_candleBlue.empty()) {
    g_sanityUpdate(sanity, method);
    return;
  }
  std::vector<void*> all(list->items->data<void*>(), list->items->data<void*>() + list->size);
  std::vector<void*> kept;
  for (void* candle : all) {
    const auto it = g_candleBlue.find(candle);
    if (it == g_candleBlue.end() || gives_comfort(it->second)) kept.push_back(candle);
  }
  if (kept.size() == all.size()) {
    g_sanityUpdate(sanity, method);
    return;
  }
  // The list is put back however the call ends, a managed exception included.
  struct Restore {
    il2cpp::List* list;
    const std::vector<void*>& all;
    ~Restore() {
      for (size_t i = 0; i < all.size(); ++i) il2cpp::set_ref_field(list->items, 0x20 + i * sizeof(void*), all[i]);
      list->size = static_cast<int32_t>(all.size());
    }
  } restore{list, all};
  for (size_t i = 0; i < kept.size(); ++i)
    il2cpp::set_ref_field(list->items, 0x20 + i * sizeof(void*), kept[i]);
  list->size = static_cast<int32_t>(kept.size());
  g_sanityUpdate(sanity, method);  // reads the list only; nothing adds to it during the call
}

// FireSource.BlowOutFire(byGhost), called on the host by the ghost (it sends the RPC to everyone): a
// ghost that gives Spectral Flame cannot put out a flame set down until it is fully blue.
void detour_blow_out(void* fire, bool byGhost, const void* method) {
  hooks::Guard guard;
  if (byGhost && g_enabled && g_givesFlame && game::is_master()) {
    for (const Flame& f : g_flames) {
      if (f.fire != fire || ghost_may_blow_out(placed(f), g_givesFlame, {f.blueness, f.full})) continue;
      PG_LOG("flames: flame %d stays lit: the ghost cannot blow it out before it has been fully blue", f.view);
      return;
    }
  }
  g_blowOut(fire, byGhost, method);
}

}  // namespace

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  constexpr const char* kGame = "Assembly-CSharp.dll";
  constexpr const char* kPun = "PhotonUnityNetworking.dll";
  api.candle = il2cpp::cls(kGame, "", "Candle");
  il2cpp::Class* candelabra = il2cpp::cls(kGame, "", "Candlebra");
  il2cpp::Class* renderer = il2cpp::cls(kCore, "UnityEngine", "Renderer");
  il2cpp::Class* light = il2cpp::cls(kCore, "UnityEngine", "Light");
  api.candelabraType = candelabra ? il2cpp::type_object(candelabra) : nullptr;
  api.rendererType = renderer ? il2cpp::type_object(renderer) : nullptr;
  api.lightType = light ? il2cpp::type_object(light) : nullptr;
  bool ok = api.candle && api.candelabraType && api.rendererType && api.lightType;
  ok &= il2cpp::bind(api.findObjects, il2cpp::cls(kCore, "UnityEngine", "Object"), "FindObjectsOfType", 1);
  ok &= il2cpp::bind(api.sharedMaterial, renderer, "get_sharedMaterial", 0);
  ok &= il2cpp::bind(api.material, renderer, "get_material", 0);
  ok &= il2cpp::bind(api.lightColor, light, "get_color", 0);
  ok &= il2cpp::bind(api.setLightColor, light, "set_color", 1);
  ok &= il2cpp::bind(api.photonView, il2cpp::cls(kPun, "Photon.Pun", "MonoBehaviourPun"), "get_photonView", 0);
  ok &= il2cpp::bind(api.viewId, il2cpp::cls(kPun, "Photon.Pun", "PhotonView"), "get_ViewID", 0);
  api.propertyId = reinterpret_cast<decltype(api.propertyId)>(il2cpp::resolve_icall("UnityEngine.Shader::PropertyToID"));
  api.hasProperty = reinterpret_cast<decltype(api.hasProperty)>(il2cpp::resolve_icall("UnityEngine.Material::HasProperty"));
  api.getColor = reinterpret_cast<decltype(api.getColor)>(il2cpp::resolve_icall("UnityEngine.Material::GetColorImpl_Injected"));
  api.setColor = reinterpret_cast<decltype(api.setColor)>(il2cpp::resolve_icall("UnityEngine.Material::SetColorImpl_Injected"));
  ok &= api.propertyId && api.hasProperty && api.getColor && api.setColor;
  if (ok) {
    api.candleTint = api.propertyId(il2cpp::string_new("_TintColour"));
    api.lanternTint = api.propertyId(il2cpp::string_new("_Color_2"));
  }
  api.ok = ok;
  PG_LOG("flames: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kSanityUpdateRva), reinterpret_cast<void*>(&detour_sanity_update),
                       reinterpret_cast<void**>(&g_sanityUpdate), "PlayerSanity.Update") &&
         hooks::create(game::game_assembly_address(kBlowOutRva), reinterpret_cast<void*>(&detour_blow_out),
                       reinterpret_cast<void**>(&g_blowOut), "FireSource.BlowOutFire");
}

void tick(bool enabled) {
  void* level = game::level_controller();
  if (!api.ok) return;
  if (!enabled || !alive(level) || level != g_level) {
    // Outside a contract (or with the flames off) every flame shows its own colours again.
    if (g_level) clear_all();
    g_enabled = false;
    if (!enabled || !alive(level)) return;
    g_level = level;
    g_nextScan = 0;
  }
  g_enabled = true;
  static ULONGLONG last = 0;
  const ULONGLONG now = GetTickCount64();
  const float dt = last ? std::min((now - last) / 1000.0f, 0.25f) : 0;
  last = now;
  if (now >= g_nextScan) {
    g_nextScan = now + kScanMs;
    scan(level);
  }
  if (game::is_master()) host_step(level, dt, now);
  else client_step(now);
  show_and_burn(dt);
}

void receive(int sender, const uint8_t* data, size_t size) {
  if (sender != game::master_actor()) return;
  std::vector<std::pair<int, float>> flames;
  if (!decode(data, size, flames)) return;
  // A message names every blue flame up to kMaxPerMessage; a longer list arrives in parts, each
  // adding to what the last few hundred milliseconds said.
  const ULONGLONG now = GetTickCount64();
  if (now - g_lastHeard > kSendMs / 2) g_heard.clear();
  g_lastHeard = now;
  for (const auto& [view, blueness] : flames) g_heard[view] = blueness;
}

void teardown() {
  clear_all();
  g_enabled = false;
}

}  // namespace pg::flame::flames
