#include "flames.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Candle.h"
#include "Equipment.h"
#include "FireSource.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Photon.h"
#include "PlayerSanity.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "contract.h"
#include "flame_rules.h"
#include "game_api.h"
#include "held.h"
#include "managed_exception.h"  // Il2CppExceptionWrapper: the C++ type a managed exception is thrown as
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "runtime.h"

// Every game value comes through the game map (sdk/map: FireSource.h, Candle.h, Equipment.h,
// PlayerSanity.h, LevelController.h, LevelRoom.h, UnityRendering.h and Photon.h), whose notes say what
// the game's code does with each.
namespace pm::flame::flames {
namespace {

using api::Held;
using phasmo::Color;

constexpr ULONGLONG kScanMs = 1000;        // looking for new fire sources and candelabras
constexpr ULONGLONG kSendMs = 250;         // the host's message to every client
constexpr ULONGLONG kStaleMs = 3000;       // a client with no message for this long shows none
constexpr ULONGLONG kRoomMs = 2000;        // a still flame's room is looked up again this often
constexpr float kRoomMove = 0.25f;         // metres a flame moves before its room is looked up again
constexpr float kShowStep = 0.004f;        // blueness change worth redrawing
constexpr int kMaxDepth = 8;               // how deep a flame's objects are searched for its renderers

// A shader property a flame is painted by (UnityRendering.h: the Candle Flame shader's tint and sprite,
// the Lantern Flame's two colours, the wax's glow), with its id. One that did not resolve leaves what
// it colours as the game's: no material is asked for it.
struct Property {
  int32_t id = 0;
  bool ok = false;
};
struct Properties {
  Property candleTint, candleSprite, lanternTint, lanternBase, emission;
} g_ids;
// Every optional entry a grey copy of a flame sprite needs resolved (UnityRendering.h). Without them
// a candle flame keeps the game's sprite under its tint.
bool g_textures = false;

struct Tinted {  // a flame material, recoloured
  Held material;
  int32_t property;
  bool lantern;    // a Lantern Flame colour, else the Candle Flame's tint
  Color original;  // the game's own, put back when the contract ends
  Color warm;      // its colour at none: the game's own, or what shows the grey sprite as the game's
  Held sprite;     // a candle flame's own sprite, which shows grey for the contract (flame_rules.h)
};
struct Glow {  // a candle's wax, lit from within while it burns
  Held material;
  Color original;  // the glow the game gives it when lit (Candle.candleEmissionColor)
};
// A grey copy of one flame sprite, made from the sprite the first time a flame shows it, with the tint
// that shows it as the sprite did, and kept for the contract.
struct Grey {
  Held source;
  Held texture;
  Rgb match{1, 1, 1};  // flame_rules.h grey_match
  bool failed = false;  // it could not be made: flames keep the game's sprite and their own tint
};
struct Lit {  // a light, recoloured
  Held light;
  Color original;
};
// The lights a candle casts besides its flames' own: every Light on the candle (a Tier 2 Firelight
// has one for the whole candle, "Point light.1", besides one per flame), blue with its bluest flame.
// Kept per candle, so a light its flames share has its own colour captured once.
struct Look {
  Held candle;
  std::vector<Lit> lights;
  float shown = 0;
  bool found = false;  // its lights have been looked for
};

struct Flame {
  Held fire;    // FireSource
  Held candle;  // Candle it belongs to
  Kind kind = Kind::Tier1;
  int view = 0;            // PhotonView id: what the host's message names it by
  float ghostTime = 0;     // host: the ghost's time in its room, built up and drained (flame_rules.h)
  float blueness = 0;      // host: worked out here; others: as received
  bool full = false;       // host: it has been fully blue since it was set down and lit
  float shown = 0;         // what its colours show
  Held room;
  phasmo::Vector3 roomAt{};
  ULONGLONG roomAgain = 0;
  bool found = false;      // its flame renderers have been looked for
  std::vector<Tinted> materials;
  std::vector<Glow> glows;  // the wax of its own candle
};

struct Candelabra {
  Held object;
  std::vector<Held> candles;
  Lit light{};  // its own Light, blue with its bluest candle
  float shown = 0;
};

Held g_level;  // the LevelController the flames belong to
std::vector<Flame> g_flames;
std::vector<Look> g_looks;
std::vector<Candelabra> g_candelabras;
std::vector<Grey> g_greys;
ULONGLONG g_nextScan = 0, g_nextSend = 0, g_lastHeard = 0, g_lastTick = 0;
std::unordered_map<int, float> g_heard;  // view id -> blueness, from the host's last message
std::unordered_map<void*, float> g_candleBlue;  // Candle -> its bluest flame, for the sanity detours
std::mt19937 g_random{std::random_device{}()};
bool g_enabled = false;
int32_t g_evidence = -1;     // Spectral Flame's game value in this contract
bool g_givesFlame = false;   // host: the ghost gives Spectral Flame, as of the last step
bool g_green = false;        // the ghost's flames turn green (the Hydra), as of the last scan
bool g_inSanityUpdate = false;  // PlayerSanity.Update is running (main thread)

void* transform_of(void* component) {
  return api::alive(component) ? phasmo::Component::get_transform(component) : nullptr;
}

/** The first component of `systemType` on the GameObject of `component`, or null. */
void* component_of(void* component, void* systemType) {
  return api::alive(component) && systemType ? phasmo::Component::GetComponent(component, systemType) : nullptr;
}

void destroy(void* object) {
  if (api::alive(object)) phasmo::Object::Destroy(object, 0.0f);
}

void log_exception(const char* where, const Il2CppExceptionWrapper& e) { mod::host().log_exception(where, e.ex); }

/** Recolour a light or material back to `original` or towards blue (green for the Hydra). */
void paint(const Lit& l, float blueness) {
  if (!api::alive(l.light)) return;
  const Rgb warm{l.original.r, l.original.g, l.original.b};
  const FlameColours stops = light_colours(brightness(warm), g_green);
  const Rgb c = tint(warm, stops.pale, stops.deep, blueness);
  Color out{c.r, c.g, c.b, l.original.a};
  phasmo::Light::set_color(l.light, &out);
}

/** A readable Texture2D of the game's own kind (RGBA32 with mipmaps), kept until the contract ends. */
void* new_texture(int width, int height) {
  void* texture = phasmo::Texture2D::type.make();
  if (!texture) return nullptr;
  phasmo::Texture2D::ctor(texture, width, height);
  phasmo::Object::set_hideFlags(texture, phasmo::HideFlags::DontUnloadUnusedAsset());
  return texture;
}

/**
 * The texels of a flame sprite, which the game keeps unreadable: drawn into a render texture and read
 * back from it. RenderTexture.active is put back however the read ends.
 */
bool read_back(void* source, int width, int height, std::vector<uint8_t>& out) {
  void* target = phasmo::RenderTexture::GetTemporary(width, height, 0, phasmo::RenderTextureFormat::ARGB32(),
                                                     phasmo::RenderTextureReadWrite::sRGB());
  if (!target) return false;
  void* previous = phasmo::RenderTexture::get_active();
  Held readable;
  bool ok = false;
  try {
    phasmo::Graphics::Blit(source, target);
    phasmo::RenderTexture::set_active(target);
    readable = new_texture(width, height);
    phasmo::Rect all{0, 0, static_cast<float>(width), static_cast<float>(height)};
    if (readable) phasmo::Texture2D::ReadPixels(readable, &all, 0, 0);
    const api::Array<phasmo::Color32> texels{readable ? phasmo::Texture2D::GetPixels32(readable, 0) : nullptr};
    if (texels.size() > 0 && texels.size() == static_cast<size_t>(width) * height) {
      const auto* rgba = reinterpret_cast<const uint8_t*>(&texels[0]);
      out.assign(rgba, rgba + texels.size() * sizeof(phasmo::Color32));
      ok = true;
    }
  } catch (const Il2CppExceptionWrapper& e) {
    log_exception("flames: reading a flame sprite back threw", e);
  }
  phasmo::RenderTexture::set_active(previous);
  phasmo::RenderTexture::ReleaseTemporary(target);
  destroy(readable);
  return ok;
}

/**
 * The grey copy of `g.source`, sampled as the game's own sprite, and the tint match for it. The copy
 * is held from the start, so it goes with the contract whatever happens, and is kept only when whole.
 */
bool make_grey(Grey& g) {
  const int width = phasmo::Texture::get_width(g.source), height = phasmo::Texture::get_height(g.source);
  std::vector<uint8_t> texels;
  if (width <= 0 || height <= 0 || !read_back(g.source, width, height, texels)) return false;
  g.match = grey_match(texels.data(), texels.size() / 4);
  g.texture = new_texture(width, height);
  void* copy = g.texture ? phasmo::Texture2D::GetPixels32(g.texture, 0) : nullptr;
  const api::Array<phasmo::Color32> grey{copy};
  if (grey.size() == 0 || grey.size() * sizeof(phasmo::Color32) != texels.size()) return false;
  auto* rgba = reinterpret_cast<uint8_t*>(&grey[0]);
  for (size_t i = 0; i < texels.size(); i += 4) {
    std::copy_n(texels.data() + i, 4, rgba + i);
    to_grey(rgba + i);
  }
  phasmo::Texture2D::SetPixels32(g.texture, copy, 0);
  phasmo::Texture::set_wrapMode(g.texture, phasmo::Texture::get_wrapMode(g.source));
  phasmo::Texture::set_filterMode(g.texture, phasmo::Texture::get_filterMode(g.source));
  phasmo::Texture2D::Apply(g.texture, true, true);  // its mipmaps, then gone from memory but on the GPU
  PM_LOG("flames: a flame sprite (%dx%d) shows grey, matched by (%.3f, %.3f, %.3f)", width, height, g.match.r,
         g.match.g, g.match.b);
  return true;
}

/** The grey copy of the flame sprite `source`, made the first time it is asked for; null if it cannot be. */
const Grey* grey_of(void* source) {
  if (!g_textures || !api::alive(source)) return nullptr;
  for (const Grey& known : g_greys)
    if (known.source == source) return known.failed ? nullptr : &known;
  Grey& g = g_greys.emplace_back();
  g.source = source;
  try {
    g.failed = !make_grey(g);
  } catch (const Il2CppExceptionWrapper& e) {
    log_exception("flames: making a grey flame sprite threw", e);
    g.failed = true;
  }
  if (g.failed) {
    destroy(g.texture);
    g.texture = nullptr;
    PM_LOG("flames: a flame sprite could not be made grey: its flames keep the game's sprite under the tint");
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
  if (!api::alive(t.material)) return;
  const Rgb warm{t.warm.r, t.warm.g, t.warm.b};
  const FlameColours stops = t.lantern ? lantern_colours(g_green) : flame_colours(g_green);
  const Rgb c = tint(warm, stops.pale, stops.deep, blueness);
  Color out{c.r, c.g, c.b, t.warm.a};
  phasmo::Material::SetColorImpl(t.material, t.property, &out);
}

/** The game's own colour, and its own sprite, back on a flame material. */
void restore(const Tinted& t) {
  if (!api::alive(t.material)) return;
  Color original = t.original;
  phasmo::Material::SetColorImpl(t.material, t.property, &original);
  if (api::alive(t.sprite)) phasmo::Material::SetTextureImpl(t.material, g_ids.candleSprite.id, t.sprite);
}

/**
 * The wax's glow follows its flame, at the flame's hue and the glow's own brightness, as a light does.
 * Only while the flame burns: out, the game has darkened the wax, and it stays so.
 */
void paint(const Glow& g, float blueness, bool lit) {
  if (!lit || !api::alive(g.material)) return;
  const Rgb warm{g.original.r, g.original.g, g.original.b};
  const FlameColours stops = light_colours(brightness(warm), g_green);
  const Rgb c = tint(warm, stops.pale, stops.deep, blueness);
  Color out{c.r, c.g, c.b, g.original.a};
  phasmo::Material::SetColorImpl(g.material, g_ids.emission.id, &out);
}

void paint(const Look& l, float blueness) {
  for (const Lit& light : l.lights) paint(light, blueness);
}

void paint(Flame& f) {
  for (const Tinted& t : f.materials) paint(t, f.blueness);
  const bool lit = phasmo::FireSource::isOn.get(f.fire);
  for (const Glow& g : f.glows) paint(g, f.blueness, lit);
  f.shown = f.blueness;
}

void release(Flame& f) {
  for (const Tinted& t : f.materials) restore(t);
  const bool lit = api::alive(f.fire) && phasmo::FireSource::isOn.get(f.fire);
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
  for (Grey& g : g_greys) destroy(g.texture);
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
  if (void* renderer = component_of(transform, phasmo::Renderer::type.type_object())) {
    void* shared = phasmo::Renderer::get_sharedMaterial(renderer);
    const auto has = [&](const Property& p) { return shared && p.ok && phasmo::Material::HasProperty(shared, p.id); };
    const bool candle = has(g_ids.candleTint), lantern = !candle && has(g_ids.lanternTint);
    const int32_t property = candle ? g_ids.candleTint.id : g_ids.lanternTint.id;
    // This renderer's own copy.
    void* material = candle || lantern ? phasmo::Renderer::get_material(renderer) : nullptr;
    bool known = false;
    for (const Tinted& t : f.materials) known |= t.material == material;
    if (material && !known) {
      Tinted t{material, property, lantern, {}, {}};
      phasmo::Material::GetColorImpl(material, property, &t.original);
      t.warm = t.original;
      // A candle flame shows its sprite's grey copy from now on, with the tint that shows it as the
      // game's own did; the tint alone then colours the whole flame.
      if (candle && g_textures && g_ids.candleSprite.ok &&
          phasmo::Material::HasProperty(material, g_ids.candleSprite.id)) {
        void* sprite = own_texture(phasmo::Material::GetTextureImpl(material, g_ids.candleSprite.id));
        if (const Grey* grey = grey_of(sprite)) {
          t.sprite = sprite;
          phasmo::Material::SetTextureImpl(material, g_ids.candleSprite.id, grey->texture);
          const Rgb warm = grey_tint({t.original.r, t.original.g, t.original.b}, grey->match);
          t.warm = {warm.r, warm.g, warm.b, t.original.a};
        }
      }
      f.materials.push_back(std::move(t));
      // A lantern's flame is noise between two colours; both turn, so none of the game's blue stays
      // in a green flame.
      if (lantern && g_ids.lanternBase.ok && phasmo::Material::HasProperty(material, g_ids.lanternBase.id)) {
        Tinted base{material, g_ids.lanternBase.id, true, {}, {}};
        phasmo::Material::GetColorImpl(material, g_ids.lanternBase.id, &base.original);
        base.warm = base.original;
        f.materials.push_back(std::move(base));
      }
    }
  }
  const int children = phasmo::Transform::get_childCount(transform);
  for (int i = 0; i < children; ++i) collect(f, phasmo::Transform::GetChild(transform, i), depth + 1);
}

/** The wax of the flame's own candle (Tier 1 and 2), which glows near the flame while it burns. */
void collect_glows(Flame& f) {
  if (f.kind == Kind::Tier3) return;
  const Color glow = phasmo::Candle::candleEmissionColor.get(f.candle);
  const api::RefArray objects{phasmo::Candle::candleObjects.get(f.candle)};
  for (size_t i = 0; i < objects.size(); ++i) {
    void* o = objects[i];
    if (!o || phasmo::CandleObject::fireSource.get(o).raw() != f.fire.get()) continue;
    const api::RefArray wax{phasmo::CandleObject::candleRend.get(o)};
    for (size_t k = 0; k < wax.size(); ++k) {
      void* renderer = wax[k];
      // As the game sets it.
      void* material = api::alive(renderer) ? phasmo::Renderer::get_material(renderer) : nullptr;
      if (material && g_ids.emission.ok && phasmo::Material::HasProperty(material, g_ids.emission.id))
        f.glows.push_back({material, glow});
    }
  }
}

void find_visuals(Flame& f, bool onlyFlameOfItsCandle) {
  f.found = true;
  collect(f, transform_of(f.fire), 0);
  if (void* flame = phasmo::FireSource::flame.get(f.fire); api::alive(flame))
    collect(f, phasmo::GameObject::get_transform(flame), 0);
  // A lantern's flame sits on each of its models (held, set down, in the truck), apart from its fire
  // source: a candle with one flame has all of them for that flame.
  if (onlyFlameOfItsCandle) collect(f, transform_of(f.candle), 0);
  collect_glows(f);
  int sprites = 0;
  for (const Tinted& t : f.materials) sprites += t.sprite ? 1 : 0;
  const Color glow = phasmo::Candle::candleEmissionColor.get(f.candle);
  PM_LOG("flames: flame %d shows through %zu materials (%d with a sprite) and %zu wax glows (%.2f, %.2f, %.2f)",
         f.view, f.materials.size(), sprites, f.glows.size(), glow.r, glow.g, glow.b);
}

void add_light(Look& l, void* light) {
  if (!api::alive(light)) return;
  for (const Lit& known : l.lights)
    if (known.light == light) return;
  // A candelabra's own light is painted with its bluest candle, never by one candle as well.
  for (const Candelabra& c : g_candelabras)
    if (c.light.light == light) return;
  l.lights.push_back({light, phasmo::Light::get_color(light)});
}

/** The lights in a transform's subtree, added to `l` once each. */
void collect(Look& l, void* transform, int depth) {
  if (!transform || depth > kMaxDepth) return;
  add_light(l, component_of(transform, phasmo::Light::type.type_object()));
  const int children = phasmo::Transform::get_childCount(transform);
  for (int i = 0; i < children; ++i) collect(l, phasmo::Transform::GetChild(transform, i), depth + 1);
}

void find_look(Look& l) {
  l.found = true;
  collect(l, transform_of(l.candle), 0);
  const api::RefArray objects{phasmo::Candle::candleObjects.get(l.candle)};
  for (size_t i = 0; i < objects.size(); ++i) {
    void* o = objects[i];
    void* fire = o ? phasmo::CandleObject::fireSource.get(o) : nullptr;
    if (!api::alive(fire)) continue;
    collect(l, transform_of(fire), 0);
    add_light(l, phasmo::FireSource::flameLight.get(fire));
  }
  PM_LOG("flames: a candle shows through %zu lights", l.lights.size());
}

int fires_of(void* candle) {
  const api::RefArray objects{phasmo::Candle::candleObjects.get(candle)};
  int n = 0;
  for (size_t i = 0; i < objects.size(); ++i)
    if (void* o = objects[i]; o && phasmo::CandleObject::fireSource.get(o)) ++n;
  return n;
}

const Candelabra* candelabra_of(void* candle) {
  for (const Candelabra& c : g_candelabras)
    if (std::find(c.candles.begin(), c.candles.end(), candle) != c.candles.end()) return &c;
  return nullptr;
}

/** A Firelight's kind by its store item's tier. */
Kind tier_kind(void* storeItem) {
  const int32_t tier = phasmo::StoreItem::tier.get(storeItem);
  if (tier == phasmo::StoreItemTier::Three()) return Kind::Tier3;
  if (tier == phasmo::StoreItemTier::Two()) return Kind::Tier2;
  return Kind::Tier1;
}

/** Fire sources and candelabras that joined the contract since the last look, and those that left. */
void scan(void* level) {
  // Candelabras first: their candles are counted although they belong to no Firelight.
  const api::RefArray found{phasmo::Object::FindObjectsOfType(phasmo::Candlebra::type.type_object())};
  for (size_t i = 0; i < found.size(); ++i) {
    void* object = found[i];
    if (!api::alive(object)) continue;
    bool known = false;
    for (const Candelabra& c : g_candelabras) known |= c.object == object;
    if (known) continue;
    Candelabra c;
    c.object = object;
    const api::RefArray candles{phasmo::Candlebra::allCandles.get(object)};
    for (size_t k = 0; k < candles.size(); ++k)
      if (void* candle = candles[k]) c.candles.emplace_back(candle);
    if (void* light = phasmo::Candlebra::lightSource.get(object); api::alive(light))
      c.light = {light, phasmo::Light::get_color(light)};
    g_candelabras.push_back(std::move(c));
  }
  // Flames whose objects are gone.
  for (Flame& f : g_flames)
    if (!api::alive(f.fire) || !api::alive(f.candle)) release(f);
  std::erase_if(g_flames, [](const Flame& f) { return !f.fire; });
  std::erase_if(g_looks, [](const Look& l) { return !api::alive(l.candle); });
  // New fire sources: those of a Firelight (a Candle sold in the store) or of a candelabra's candle.
  const api::List fires{phasmo::LevelController::fireSources.get(level)};
  for (size_t i = 0; i < fires.size(); ++i) {
    void* fire = fires[i];
    if (!fire) continue;
    bool known = false;
    for (const Flame& f : g_flames) known |= f.fire == fire;
    if (known) continue;
    void* owner = phasmo::FireSource::equipment.get(fire);
    if (!api::alive(owner) || !phasmo::Candle::type.is(owner)) continue;
    const Candelabra* holder = candelabra_of(owner);
    void* storeItem = phasmo::Equipment::storeItem.get(owner);
    if (!holder && !storeItem) continue;  // a summoning circle's or an altar's candle
    void* view = phasmo::MonoBehaviourPun::get_photonView(fire);
    if (!view) continue;
    Flame f;
    f.fire = fire;
    f.candle = owner;
    f.view = phasmo::PhotonView::get_ViewID(view);
    f.kind = holder ? Kind::Candelabra : tier_kind(storeItem);
    // Its visuals from the start, so a candle flame shows its grey sprite (as the game's own) before
    // it ever turns, in the truck, in a hand or set down alike.
    find_visuals(f, fires_of(owner) == 1);
    paint(f);
    g_flames.push_back(std::move(f));
  }
}

/** Whether a world point lies inside one of a LevelRoom's colliders. */
bool in_room(void* room, phasmo::Vector3 p) {
  const api::List colliders{phasmo::LevelRoom::roomColliders.get(room)};
  for (size_t i = 0; i < colliders.size(); ++i) {
    void* collider = colliders[i];
    if (!api::alive(collider)) continue;
    const phasmo::Bounds b = phasmo::Collider::get_bounds(collider);
    if (std::fabs(p.x - b.center.x) <= b.extents.x && std::fabs(p.y - b.center.y) <= b.extents.y &&
        std::fabs(p.z - b.center.z) <= b.extents.z)
      return true;
  }
  return false;
}

void* room_of(Flame& f, void* level, ULONGLONG now) {
  const phasmo::Vector3 at = phasmo::Transform::get_position(transform_of(f.fire));
  const float dx = at.x - f.roomAt.x, dy = at.y - f.roomAt.y, dz = at.z - f.roomAt.z;
  if (now < f.roomAgain && dx * dx + dy * dy + dz * dz < kRoomMove * kRoomMove) return f.room;
  f.roomAt = at;
  f.roomAgain = now + kRoomMs;
  f.room = nullptr;
  const api::RefArray rooms{phasmo::LevelController::rooms.get(level)};
  for (size_t i = 0; i < rooms.size() && !f.room; ++i)
    if (void* room = rooms[i]; api::alive(room) && in_room(room, at)) f.room = room;
  return f.room;
}

/** The host's view of the ghost: its room, whether it is calm, and whether it gives Spectral Flame. */
struct GhostNow {
  Held room;
  bool calm = false;
  bool hasEvidence = false;
};

GhostNow ghost_now(void* level) {
  GhostNow g;
  void* ghost = contract::ghost();
  if (!ghost) return g;
  const int32_t state = phasmo::GhostAI::state.get(ghost);
  g.calm = state != phasmo::GhostState::hunting() && state != phasmo::GhostState::appear() &&
           state != phasmo::GhostState::randomEvent();
  g.room = phasmo::LevelController::currentGhostRoom.get(level);
  g.hasEvidence = contract::ghost_gives(ghost, g_evidence);
  return g;
}

bool placed(const Flame& f) { return f.kind == Kind::Candelabra || phasmo::Equipment::placed.get(f.candle); }

void host_step(void* level, float dt, ULONGLONG now) {
  const GhostNow ghost = ghost_now(level);
  g_givesFlame = ghost.hasEvidence;
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  for (Flame& f : g_flames) {
    Inputs in;
    in.lit = phasmo::FireSource::isOn.get(f.fire);
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
      phasmo::FireSource::BlowOutFire(f.fire.get(), true);
      PM_LOG("flames: the ghost blew out a flame %.0f%% blue", f.blueness * 100);
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
    burning[f.candle] = burning[f.candle] || phasmo::FireSource::isOn.get(f.fire);
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
    const float left = phasmo::Candle::candleDuration.get(candle);
    phasmo::Candle::candleDuration.set(candle, left - dt * (burn_multiplier(blueness) - 1));
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

// PlayerSanity.Update: the first candle near the player that its test (CandleIsOn) finds lit eases the
// drain. While Update runs, a flame more than half blue gives no comfort: its candle's test answers
// that it is out, so Update takes the next lit candle, or none. The test is answered so only inside
// Update, the one caller of it the game's code has.
void detour_sanity_update(api::Original<phasmo::PlayerSanity::Update> original, void* sanity) {
  struct Inside {
    bool was = g_inSanityUpdate;
    Inside() { g_inSanityUpdate = true; }
    ~Inside() { g_inSanityUpdate = was; }  // however the call ends, a managed exception included
  } inside;
  original(sanity);
}

bool detour_candle_is_on(api::Original<phasmo::PlayerSanity::CandleIsOn> original, void* closure, void* candle) {
  const bool on = original(closure, candle);
  if (!on || !g_enabled || !g_inSanityUpdate) return on;
  const auto it = g_candleBlue.find(candle);
  return it == g_candleBlue.end() || gives_comfort(it->second);
}

// FireSource.BlowOutFire(byGhost), called on the host by the ghost (it sends the RPC to everyone): a
// ghost that gives Spectral Flame cannot put out a flame set down until it is fully blue.
void detour_blow_out(api::Original<phasmo::FireSource::BlowOutFire> original, void* fire, bool byGhost) {
  if (byGhost && g_enabled && g_givesFlame && phasmo::PhotonNetwork::get_IsMasterClient()) {
    for (const Flame& f : g_flames) {
      if (f.fire != fire || ghost_may_blow_out(placed(f), g_givesFlame, {f.ghostTime, f.blueness, f.full})) continue;
      PM_LOG("flames: flame %d stays lit: the ghost cannot blow it out before it has been fully blue", f.view);
      return;
    }
  }
  original(fire, byGhost);
}

/** The shader property P's id; when it did not resolve, logged once with what goes without it. */
template <const auto& P>
Property property(const char* without) {
  if (P.available()) return {P.id(), true};
  PM_LOG("flames: %s did not resolve: %s", P.desc.key, without);
  return {};
}

int master_actor() {
  void* master = phasmo::PhotonNetwork::get_MasterClient();
  return master ? phasmo::PhotonPlayer::get_ActorNumber(master) : 0;
}

}  // namespace

void resolve() {
  using namespace phasmo;
  g_ids = {property<CandleFlame::TintColour>("candle flames keep the game's colours"),
           property<CandleFlame::CandleSprite>("candle flames keep the game's sprite under their tint"),
           property<LanternFlame::Color2>("lantern flames keep the game's colours"),
           property<LanternFlame::Color1>("lantern flames turn only their warm colour"),
           property<CandleMelt::EmissionColor>("candle wax keeps the game's glow")};
  g_textures = api::all_resolved<Material::GetTextureImpl, Material::SetTextureImpl, Texture::get_width,
                                 Texture::get_height, Texture::get_wrapMode, Texture::set_wrapMode,
                                 Texture::get_filterMode, Texture::set_filterMode, Texture2D::type, Texture2D::ctor,
                                 Texture2D::ReadPixels, Texture2D::GetPixels32, Texture2D::SetPixels32,
                                 Texture2D::Apply, RenderTexture::GetTemporary, RenderTexture::ReleaseTemporary,
                                 RenderTexture::get_active, RenderTexture::set_active, RenderTextureFormat::ARGB32,
                                 RenderTextureReadWrite::sRGB, Graphics::Blit, Object::set_hideFlags,
                                 HideFlags::DontUnloadUnusedAsset>();
  PM_LOG("flames: resolve ok%s", g_textures ? "" : " (no grey flame textures: tint only)");
}

bool create_hooks() {
  return api::hook<phasmo::PlayerSanity::Update>(&detour_sanity_update, "PlayerSanity.Update",
                                                 api::Twins::CanonicalOnly) &&
         api::hook<phasmo::PlayerSanity::CandleIsOn>(&detour_candle_is_on, "PlayerSanity.CandleIsOn",
                                                     api::Twins::CanonicalOnly) &&
         api::hook<phasmo::FireSource::BlowOutFire>(&detour_blow_out, "FireSource.BlowOutFire",
                                                    api::Twins::CanonicalOnly);
}

void tick(const Frame& frame) {
  void* level = contract::level();
  if (!frame.enabled || !level || level != g_level) {
    // Outside a contract (or with the flames off) every flame shows its own colours again.
    if (g_level) clear_all();
    g_enabled = false;
    if (!frame.enabled || !level) return;
    g_level = level;
    g_nextScan = 0;
  }
  g_enabled = true;
  g_evidence = frame.evidence;
  const ULONGLONG now = frame.now;
  const float dt = g_lastTick ? std::min((now - g_lastTick) / 1000.0f, 0.25f) : 0;
  g_lastTick = now;
  if (now >= g_nextScan) {
    g_nextScan = now + kScanMs;
    scan(level);
    set_green(contract::green(contract::ghost()));
  }
  if (phasmo::PhotonNetwork::get_IsMasterClient()) host_step(level, dt, now);
  else client_step(now);
  show_and_burn(dt);
}

void receive(int sender, const uint8_t* data, size_t size) {
  if (sender != master_actor()) return;
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
  g_lastTick = 0;
}

}  // namespace pm::flame::flames
