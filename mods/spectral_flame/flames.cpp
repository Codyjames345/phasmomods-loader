#include "flames.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
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
#include "mod_glue.h"
#include "net.h"
#include "services.h"

namespace pg::flame::flames {
namespace {

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. Read in REA
// decompiles of FireSource.Start/Update/BlowOutFire, Candle.Update/PutDown, Candlebra.Start,
// PlayerSanity.Update (2026-10-20), Equipment.PlaceNetworked/GrabNetworked and
// GhostInfo.SyncValues/SyncEvidence (2026-10-21, after a contract showed the placed read wrong), and
// Candle.UseNetworked with the flame and wax shaders' disassembly (2026-10-22, the colours on screen).
constexpr size_t kLevelGhostRoom = 0x38;      // LevelController: the room the ghost is in
constexpr size_t kLevelRooms = 0x50;          // LevelController: LevelRoom[]
constexpr size_t kLevelFireSources = 0xC0;    // LevelController: List<FireSource> (each adds itself in Start)
constexpr size_t kFireLight = 0x38;           // FireSource: its Light
constexpr size_t kFireFlame = 0x30;           // FireSource: its flame GameObject
constexpr size_t kFireOwner = 0x40;           // FireSource: the Equipment it belongs to
constexpr size_t kFireLit = 0xB0;             // FireSource: burning
constexpr size_t kCandleObjects = 0x3D0;      // Candle: CandleObject[] (each +0x18 its FireSource)
constexpr size_t kCandleObjectFire = 0x18;
// CandleObject: Renderer[], its wax. Candle.UseNetworked sets each one's _EmissionColor to the candle's
// glow (Candle+0x450) when it lights and to black when it goes out, on every client (Tier 1 and 2; the
// Tier 3 lantern has none). The wax shader ("Candle Melt") lights the top of the candle with it.
constexpr size_t kCandleObjectWax = 0x10;
constexpr size_t kCandleGlow = 0x450;         // Candle: Color, its wax's glow while lit
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
// GhostInfo: the ghost's GhostType, what it is (SyncValuesNetworked writes it on every client; Custom
// Ghosts' registry sends its own types there).
constexpr size_t kInfoType = 0x28;
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

struct Color {
  float r, g, b, a;
};
struct Rect {
  float x, y, width, height;
};

constexpr int kFormatArgb32 = 0;      // RenderTextureFormat.ARGB32
constexpr int kReadWriteSrgb = 2;     // RenderTextureReadWrite.sRGB: read back as the texture stores it
constexpr int kDontUnloadUnused = 32; // HideFlags.DontUnloadUnusedAsset: ours until the contract ends

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
  void* (*getTexture)(void*, int, const void*) = nullptr;        // Material.GetTexture(int)
  void (*setTexture)(void*, int, void*, const void*) = nullptr;  // Material.SetTexture(int, Texture)
  // A grey copy of a flame texture: read back through a render texture (the game's are not readable).
  il2cpp::Class* texture2D = nullptr;
  void (*texture2DCtor)(void*, int, int, const void*) = nullptr;  // Texture2D(width, height): RGBA32, mips
  int (*width)(void*, const void*) = nullptr;
  int (*height)(void*, const void*) = nullptr;
  int (*wrapMode)(void*, const void*) = nullptr;
  void (*setWrapMode)(void*, int, const void*) = nullptr;
  int (*filterMode)(void*, const void*) = nullptr;
  void (*setFilterMode)(void*, int, const void*) = nullptr;
  void* (*getTemporary)(int, int, int, int, int, const void*) = nullptr;
  void (*releaseTemporary)(void*, const void*) = nullptr;
  void* (*activeTarget)(const void*) = nullptr;            // RenderTexture.active
  void (*setActiveTarget)(void*, const void*) = nullptr;
  void (*blit)(void*, void*, const void*) = nullptr;       // Graphics.Blit(Texture, RenderTexture)
  void (*readPixels)(void*, const Rect*, int, int, const void*) = nullptr;
  il2cpp::Array* (*getPixels)(void*, const void*) = nullptr;   // Texture2D.GetPixels32()
  void (*setPixels)(void*, il2cpp::Array*, const void*) = nullptr;
  void (*apply)(void*, bool, bool, const void*) = nullptr;     // Texture2D.Apply(mipmaps, no longer readable)
  void (*setHideFlags)(void*, int, const void*) = nullptr;
  // "_TintColour" and "_CandleSprite" (Candle Flame shader: the sprite times the tint), "_Color_2" and
  // "_Color_1" (Lantern Flame: noise between the two), "_EmissionColor" (the wax's glow).
  int candleTint = 0, candleSprite = 0, lanternTint = 0, lanternBase = 0, emission = 0;
  bool textures = false;  // the texture calls above are bound
  bool ok = false;
} api;

struct Tinted {  // a flame material, recoloured
  il2cpp::Held material;
  int property;
  Color original;       // the game's own, put back when the contract ends
  Color warm;           // its colour at none: the game's own, or what shows the grey sprite as the game's
  il2cpp::Held sprite;  // a candle flame's own sprite, which shows grey for the contract (flame_rules.h)
};
struct Glow {  // a candle's wax, lit from within while it burns
  il2cpp::Held material;
  Color original;  // the glow the game gives it when lit (Candle+0x450)
};
// A grey copy of one flame sprite, made from the sprite the first time a flame shows it, with the tint
// that shows it as the sprite did, and kept for the contract.
struct Grey {
  il2cpp::Held source;
  il2cpp::Held texture;
  Rgb match{1, 1, 1};  // flame_rules.h grey_match
  bool failed = false;  // it could not be made: flames keep the game's sprite and their own tint
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
  float ghostTime = 0;     // host: the ghost's time in its room, built up and drained (flame_rules.h)
  float blueness = 0;      // host: worked out here; others: as received
  bool full = false;       // host: it has been fully blue since it was set down and lit
  float shown = 0;         // what its colours show
  il2cpp::Held room;
  il2cpp::Vector3 roomAt{};
  ULONGLONG roomAgain = 0;
  bool found = false;      // its flame renderers have been looked for
  std::vector<Tinted> materials;
  std::vector<Glow> glows;  // the wax of its own candle
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
std::vector<Grey> g_greys;
ULONGLONG g_nextScan = 0, g_nextSend = 0, g_lastHeard = 0;
std::unordered_map<int, float> g_heard;  // view id -> blueness, from the host's last message
std::unordered_map<void*, float> g_candleBlue;  // Candle -> its bluest flame, for the sanity detour
std::mt19937 g_random{std::random_device{}()};
bool g_enabled = false;
bool g_givesFlame = false;  // host: the ghost gives Spectral Flame, as of the last step
bool g_green = false;       // the ghost's flames turn green (the Hydra), as of the last scan

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

/** Recolour a light or material back to `original` or towards blue (green for the Hydra). */
void paint(const Lit& l, float blueness) {
  if (!alive(l.light)) return;
  const Rgb warm{l.original.r, l.original.g, l.original.b};
  const FlameColours stops = light_colours(brightness(warm), g_green);
  const Rgb c = tint(warm, stops.pale, stops.deep, blueness);
  const Color out{c.r, c.g, c.b, l.original.a};
  api.setLightColor(l.light, &out, nullptr);
}

/** A readable Texture2D of the game's own kind (RGBA32 with mipmaps), kept until the contract ends. */
void* new_texture(int width, int height) {
  il2cpp::Object* texture = il2cpp::object_new(api.texture2D);
  if (!texture) return nullptr;
  api.texture2DCtor(texture, width, height, nullptr);
  api.setHideFlags(texture, kDontUnloadUnused, nullptr);
  return texture;
}

/**
 * The texels of a flame sprite, which the game keeps unreadable: drawn into a render texture and read
 * back from it. RenderTexture.active is put back however the read ends.
 */
bool read_back(void* source, int width, int height, std::vector<uint8_t>& out) {
  void* target = api.getTemporary(width, height, 0, kFormatArgb32, kReadWriteSrgb, nullptr);
  if (!target) return false;
  void* previous = api.activeTarget(nullptr);
  il2cpp::Held readable;
  bool ok = false;
  try {
    api.blit(source, target, nullptr);
    api.setActiveTarget(target, nullptr);
    readable = new_texture(width, height);
    const Rect all{0, 0, static_cast<float>(width), static_cast<float>(height)};
    if (readable) api.readPixels(readable, &all, 0, 0, nullptr);
    il2cpp::Array* texels = readable ? api.getPixels(readable, nullptr) : nullptr;
    if (texels && texels->max_length == static_cast<uintptr_t>(width) * height) {
      const uint8_t* rgba = texels->data<uint8_t>();
      out.assign(rgba, rgba + texels->max_length * 4);
      ok = true;
    }
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("flames: reading a flame sprite back threw", e.ex);
  }
  api.setActiveTarget(previous, nullptr);
  api.releaseTemporary(target, nullptr);
  game::destroy(readable);
  return ok;
}

/**
 * The grey copy of `g.source`, sampled as the game's own sprite, and the tint match for it. The copy
 * is held from the start, so it goes with the contract whatever happens, and is kept only when whole.
 */
bool make_grey(Grey& g) {
  const int width = api.width(g.source, nullptr), height = api.height(g.source, nullptr);
  std::vector<uint8_t> texels;
  if (width <= 0 || height <= 0 || !read_back(g.source, width, height, texels)) return false;
  g.match = grey_match(texels.data(), texels.size() / 4);
  g.texture = new_texture(width, height);
  il2cpp::Array* copy = g.texture ? api.getPixels(g.texture, nullptr) : nullptr;
  if (!copy || copy->max_length * 4 != texels.size()) return false;
  uint8_t* rgba = copy->data<uint8_t>();
  for (size_t i = 0; i < texels.size(); i += 4) {
    std::copy_n(texels.data() + i, 4, rgba + i);
    to_grey(rgba + i);
  }
  api.setPixels(g.texture, copy, nullptr);
  api.setWrapMode(g.texture, api.wrapMode(g.source, nullptr), nullptr);
  api.setFilterMode(g.texture, api.filterMode(g.source, nullptr), nullptr);
  api.apply(g.texture, true, true, nullptr);  // its mipmaps, then gone from memory but on the GPU
  PG_LOG("flames: a flame sprite (%dx%d) shows grey, matched by (%.3f, %.3f, %.3f)", width, height, g.match.r,
         g.match.g, g.match.b);
  return true;
}

/** The grey copy of the flame sprite `source`, made the first time it is asked for; null if it cannot be. */
const Grey* grey_of(void* source) {
  if (!api.textures || !alive(source)) return nullptr;
  for (const Grey& known : g_greys)
    if (known.source == source) return known.failed ? nullptr : &known;
  Grey& g = g_greys.emplace_back();
  g.source = source;
  try {
    g.failed = !make_grey(g);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("flames: making a grey flame sprite threw", e.ex);
    g.failed = true;
  }
  if (g.failed) {
    game::destroy(g.texture);
    g.texture = nullptr;
    PG_LOG("flames: a flame sprite could not be made grey: its flames keep the game's sprite under the tint");
    return nullptr;
  }
  return &g;
}

/** The game's own sprite, for `texture` itself or a grey copy of it. */
void* own_texture(void* texture) {
  for (const Grey& g : g_greys)
    if (texture && g.texture == texture) return g.source;
  return texture;
}

void paint(const Tinted& t, float blueness) {
  if (!alive(t.material)) return;
  const Rgb warm{t.warm.r, t.warm.g, t.warm.b};
  const bool lantern = t.property == api.lanternTint || t.property == api.lanternBase;
  const FlameColours stops = lantern ? lantern_colours(g_green) : flame_colours(g_green);
  const Rgb c = tint(warm, stops.pale, stops.deep, blueness);
  const Color out{c.r, c.g, c.b, t.warm.a};
  api.setColor(t.material, t.property, &out);
}

/** The game's own colour, and its own sprite, back on a flame material. */
void restore(const Tinted& t) {
  if (!alive(t.material)) return;
  api.setColor(t.material, t.property, &t.original);
  if (t.sprite && alive(t.sprite)) api.setTexture(t.material, api.candleSprite, t.sprite, nullptr);
}

/**
 * The wax's glow follows its flame, at the flame's hue and the glow's own brightness, as a light does.
 * Only while the flame burns: out, the game has darkened the wax, and it stays so.
 */
void paint(const Glow& g, float blueness, bool lit) {
  if (!lit || !alive(g.material)) return;
  const Rgb warm{g.original.r, g.original.g, g.original.b};
  const FlameColours stops = light_colours(brightness(warm), g_green);
  const Rgb c = tint(warm, stops.pale, stops.deep, blueness);
  const Color out{c.r, c.g, c.b, g.original.a};
  api.setColor(g.material, api.emission, &out);
}

void paint(const Look& l, float blueness) {
  for (const Lit& light : l.lights) paint(light, blueness);
}

void paint(Flame& f) {
  for (const Tinted& t : f.materials) paint(t, f.blueness);
  const bool lit = il2cpp::field<bool>(f.fire, kFireLit);
  for (const Glow& g : f.glows) paint(g, f.blueness, lit);
  f.shown = f.blueness;
}

void release(Flame& f) {
  for (const Tinted& t : f.materials) restore(t);
  const bool lit = alive(f.fire) && il2cpp::field<bool>(f.fire, kFireLit);
  for (const Glow& g : f.glows) paint(g, 0, lit);
  f = Flame{};
}

void release(Candelabra& c) {
  if (c.light.light) paint(c.light, 0);
}

void clear_all() {
  for (Flame& f : g_flames) release(f);
  for (const Look& l : g_looks) paint(l, 0);
  for (Candelabra& c : g_candelabras) release(c);
  // Every flame shows its own sprite again, so the grey copies go with the contract.
  for (Grey& g : g_greys) game::destroy(g.texture);
  g_flames.clear();
  g_looks.clear();
  g_candelabras.clear();
  g_greys.clear();
  g_candleBlue.clear();
  g_heard.clear();
  g_level = nullptr;
  g_givesFlame = false;
  g_green = false;
}

/**
 * Whether the ghost's flames turn green: Custom Ghosts names the types whose do (the Hydra), by the
 * ghost's own type, which every client knows. Blue without Custom Ghosts (or before its version 5).
 */
bool green_ghost() {
  void* ai = game::ghost_ai();
  void* info = alive(ai) ? il2cpp::field<void*>(ai, kAiInfo) : nullptr;
  if (!alive(info)) return false;
  const auto* s = static_cast<const PgCustomGhostsService*>(mod::host().service(PG_SERVICE_CUSTOM_GHOSTS));
  return s && s->version >= 5 && s->greenFlame && s->greenFlame(il2cpp::field<int>(info, kInfoType));
}

/** Once the ghost's colour is known (or changes), every flame and light showing one is repainted in it. */
void set_green(bool green) {
  if (green == g_green) return;
  g_green = green;
  for (Flame& f : g_flames)
    if (f.shown != 0) f.shown = -1;
  for (Look& l : g_looks)
    if (l.shown != 0) l.shown = -1;
  for (Candelabra& c : g_candelabras)
    if (c.shown != 0) c.shown = -1;
}

/** The flame materials in a transform's subtree, added to `f` once each. */
void collect(Flame& f, void* transform, int depth) {
  if (!transform || depth > kMaxDepth) return;
  if (void* renderer = game::get_component(transform, api.rendererType)) {
    void* shared = api.sharedMaterial(renderer, nullptr);
    int property = 0;
    if (shared && api.hasProperty(shared, api.candleTint)) property = api.candleTint;
    else if (shared && api.hasProperty(shared, api.lanternTint)) property = api.lanternTint;
    void* material = property ? api.material(renderer, nullptr) : nullptr;  // this renderer's own copy
    bool known = false;
    for (const Tinted& t : f.materials) known |= t.material == material;
    if (material && !known) {
      Tinted t{material, property, {}, {}};
      api.getColor(material, property, &t.original);
      t.warm = t.original;
      // A candle flame shows its sprite's grey copy from now on, with the tint that shows it as the
      // game's own did; the tint alone then colours the whole flame.
      if (property == api.candleTint && api.textures && api.hasProperty(material, api.candleSprite)) {
        void* sprite = own_texture(api.getTexture(material, api.candleSprite, nullptr));
        if (const Grey* grey = grey_of(sprite)) {
          t.sprite = sprite;
          api.setTexture(material, api.candleSprite, grey->texture, nullptr);
          const Rgb warm = grey_tint({t.original.r, t.original.g, t.original.b}, grey->match);
          t.warm = {warm.r, warm.g, warm.b, t.original.a};
        }
      }
      f.materials.push_back(std::move(t));
      // A lantern's flame is noise between two colours; both turn, so none of the game's blue stays
      // in a green flame.
      if (property == api.lanternTint && api.hasProperty(material, api.lanternBase)) {
        Tinted base{material, api.lanternBase, {}, {}};
        api.getColor(material, api.lanternBase, &base.original);
        base.warm = base.original;
        f.materials.push_back(std::move(base));
      }
    }
  }
  for (int i = 0; i < game::child_count(transform); ++i) collect(f, game::child(transform, i), depth + 1);
}

/** The wax of the flame's own candle (Tier 1 and 2), which glows near the flame while it burns. */
void collect_glows(Flame& f) {
  if (f.kind == Kind::Tier3) return;
  const Color glow = il2cpp::field<Color>(f.candle, kCandleGlow);
  auto* objects = il2cpp::field<il2cpp::Array*>(f.candle, kCandleObjects);
  for (uintptr_t i = 0; objects && i < objects->max_length; ++i) {
    void* o = objects->data<void*>()[i];
    if (!o || il2cpp::field<void*>(o, kCandleObjectFire) != f.fire.get()) continue;
    auto* wax = il2cpp::field<il2cpp::Array*>(o, kCandleObjectWax);
    for (uintptr_t k = 0; wax && k < wax->max_length; ++k) {
      void* renderer = wax->data<void*>()[k];
      void* material = alive(renderer) ? api.material(renderer, nullptr) : nullptr;  // as the game sets it
      if (material && api.hasProperty(material, api.emission)) f.glows.push_back({material, glow});
    }
  }
}

void find_visuals(Flame& f, bool onlyFlameOfItsCandle) {
  f.found = true;
  collect(f, game::transform_of(f.fire), 0);
  if (void* flame = il2cpp::field<void*>(f.fire, kFireFlame); alive(flame)) collect(f, game::object_transform(flame), 0);
  // A lantern's flame sits on each of its models (held, set down, in the truck), apart from its fire
  // source: a candle with one flame has all of them for that flame.
  if (onlyFlameOfItsCandle) collect(f, game::transform_of(f.candle), 0);
  collect_glows(f);
  int sprites = 0;
  for (const Tinted& t : f.materials) sprites += t.sprite ? 1 : 0;
  const Color glow = il2cpp::field<Color>(f.candle, kCandleGlow);
  PG_LOG("flames: flame %d shows through %zu materials (%d with a sprite) and %zu wax glows (%.2f, %.2f, %.2f)",
         f.view, f.materials.size(), sprites, f.glows.size(), glow.r, glow.g, glow.b);
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
    // Its visuals from the start, so a candle flame shows its grey sprite (as the game's own) before
    // it ever turns, in the truck, in a hand or set down alike.
    find_visuals(f, fires_of(owner) == 1);
    paint(f);
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
  for (int i = 0; given && given->items && i < given->size; ++i) g.hasEvidence |= given->items->data<int>()[i] == kEvidence;
  return g;
}

bool placed(const Flame& f) { return f.kind == Kind::Candelabra || il2cpp::field<bool>(f.candle, kEquipmentPlaced); }

void host_step(void* level, float dt, ULONGLONG now) {
  const GhostNow ghost = ghost_now(level);
  g_givesFlame = ghost.hasEvidence;
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  for (Flame& f : g_flames) {
    Inputs in;
    in.lit = il2cpp::field<bool>(f.fire, kFireLit);
    in.placed = placed(f);
    in.ghostInRoom = in.lit && in.placed && ghost.room && room_of(f, level, now) == ghost.room;
    in.hasEvidence = ghost.hasEvidence;
    in.kind = f.kind;
    const State next = step({f.ghostTime, f.blueness, f.full}, in, dt);
    f.ghostTime = next.ghostTime;
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
      paint(f);
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
      if (f.fire != fire || ghost_may_blow_out(placed(f), g_givesFlame, {f.ghostTime, f.blueness, f.full})) continue;
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
    api.candleSprite = api.propertyId(il2cpp::string_new("_CandleSprite"));
    api.lanternTint = api.propertyId(il2cpp::string_new("_Color_2"));
    api.lanternBase = api.propertyId(il2cpp::string_new("_Color_1"));
    api.emission = api.propertyId(il2cpp::string_new("_EmissionColor"));
  }
  // The grey flame textures. Without them a flame is still tinted, as before, over the game's colours.
  il2cpp::Class* material = il2cpp::cls(kCore, "UnityEngine", "Material");
  il2cpp::Class* texture = il2cpp::cls(kCore, "UnityEngine", "Texture");
  il2cpp::Class* renderTexture = il2cpp::cls(kCore, "UnityEngine", "RenderTexture");
  api.texture2D = il2cpp::cls(kCore, "UnityEngine", "Texture2D");
  bool textures = api.texture2D != nullptr;
  textures &= il2cpp::bind(api.getTexture, material, "GetTextureImpl", 1);
  textures &= il2cpp::bind(api.setTexture, material, "SetTextureImpl", 2);
  textures &= il2cpp::bind(api.texture2DCtor, api.texture2D, ".ctor", 2);
  textures &= il2cpp::bind(api.width, texture, "get_width", 0);
  textures &= il2cpp::bind(api.height, texture, "get_height", 0);
  textures &= il2cpp::bind(api.wrapMode, texture, "get_wrapMode", 0);
  textures &= il2cpp::bind(api.setWrapMode, texture, "set_wrapMode", 1);
  textures &= il2cpp::bind(api.filterMode, texture, "get_filterMode", 0);
  textures &= il2cpp::bind(api.setFilterMode, texture, "set_filterMode", 1);
  textures &= il2cpp::bind(api.getTemporary, renderTexture, "GetTemporary", 5);
  textures &= il2cpp::bind(api.releaseTemporary, renderTexture, "ReleaseTemporary", 1);
  textures &= il2cpp::bind(api.activeTarget, renderTexture, "get_active", 0);
  textures &= il2cpp::bind(api.setActiveTarget, renderTexture, "set_active", 1);
  textures &= il2cpp::bind(api.blit, il2cpp::cls(kCore, "UnityEngine", "Graphics"), "Blit", 2);
  textures &= il2cpp::bind(api.readPixels, api.texture2D, "ReadPixels", 3);
  textures &= il2cpp::bind(api.getPixels, api.texture2D, "GetPixels32", 0);
  textures &= il2cpp::bind(api.setPixels, api.texture2D, "SetPixels32", 1);
  textures &= il2cpp::bind(api.apply, api.texture2D, "Apply", 2);
  textures &= il2cpp::bind(api.setHideFlags, il2cpp::cls(kCore, "UnityEngine", "Object"), "set_hideFlags", 1);
  api.textures = ok && textures;
  api.ok = ok;
  PG_LOG("flames: resolve %s%s", ok ? "ok" : "FAILED", ok && !textures ? " (no grey flame textures: tint only)" : "");
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
    set_green(green_ghost());
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
