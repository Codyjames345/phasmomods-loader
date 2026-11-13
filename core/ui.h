#pragma once

#include <cstddef>
#include <cstdint>

#include "held.h"

// Screen-space UI built from the game's own Unity UI classes (Canvas, TextMeshPro, Image), using
// the game's own font so mod UI reads as part of the game, through the game API. Main thread only.
namespace pm::ui {

struct Color {
  float r, g, b, a;
};
struct Vec2 {
  float x, y;
};
struct Rect {
  float x, y, width, height;
};

bool resolve();

// A root overlay canvas, scaled like the game's 1920x1080 UI.
void* create_canvas(const char* name, int sortingOrder);
void* create_text(void* parent, const char* name, float fontSize);  // TextMeshProUGUI
void* create_image(void* parent, const char* name, Color color);    // a solid colour (a RawImage)
void* create_raw_image(void* parent, const char* name, void* texture);  // UnityEngine.UI.RawImage
void set_texture(void* rawImage, void* texture);
void* create_sprite_image(void* parent, const char* name, void* sprite);  // UnityEngine.UI.Image showing a Sprite
void set_sprite(void* image, void* sprite);  // an Image's Sprite, or null
void set_uv_rect(void* rawImage, Rect uv);  // the part of the texture shown, 0..1
void destroy(void* gameObjectOrComponent);
void set_active(void* component, bool on);

// RectTransform layout: anchors and pivot in 0..1 of the parent, position and size in canvas units. Pass the
// UI component, or the GameObject it is on: either way the object's transform has to be a RectTransform, which it
// is once the object has taken a component of a class that requires one (every UI class here does). An object
// without one is left alone and named in the log once: the game faults on laying out a plain Transform.
void place(void* gameObjectOrComponent, Vec2 anchor, Vec2 pivot, Vec2 position, Vec2 size);
void set_text(void* text, const char* utf8);  // TextMeshPro rich text
// TMPro.TextAlignmentOptions, each read from the game (phasmo::TextAlignmentOptions).
enum class Align { TopLeft, Left, Center, Top };
void set_alignment(void* text, Align align);
void set_word_wrap(void* text, bool on);  // off by default on the mod's texts
float font_size(void* text);
float font_size_max(void* text);  // configured native size, independent of the last auto-sized rendering
// Keep a readable size on world-space paper; content must wrap within its allotted rectangle.
void set_fixed_font_size(void* text, float size);
// TextMeshPro auto-sizing: the text shrinks from `max` down to `min` until it fits its rectangle.
void set_auto_size(void* text, float min, float max);

void set_color(void* graphic, Color color);
Color color(void* graphic);

// A scrollable list on the game's own ScrollRect and RectMask2D, so the list is longer than the space it
// takes: `viewport` clips and catches the pointer, `content` is what the rows go under, and `scroll` is the
// game's ScrollRect that moves it. Lay the rows out top-down inside `content` (anchor {0.5, 1}, pivot
// {0.5, 0.5}, y negative from the top edge), and the game's own pointer handling drags and wheel-scrolls the
// view. scroll_reveal() brings back on screen whatever the keyboard has reached.
struct ScrollView {
  // The three game objects are held, as every object a mod keeps between frames is (held.h).
  api::Held viewport;           // the clipped area, a child of the canvas
  api::Held content;            // the whole list, a child of viewport
  api::Held scroll;             // the ScrollRect component, on viewport
  float height = 0.0f;          // the viewport's height, in canvas units
  float contentHeight = 0.0f;   // the content's height, the same units
};

// A vertical scroll view under `parent` of `size` centred at `position`, its content `contentHeight` tall.
// Its parts are null when the game's classes did not resolve: a view with no `scroll` still shows its rows,
// they just cannot move. The view carries a background that catches the pointer, so a drag anywhere in it
// scrolls, and a clip, so rows outside the view are not drawn over the rest of the menu. The content shows
// nothing of its own, so it carries a LayoutElement: that is what gives it a RectTransform for the list's
// height to be set on (menus.md, "A list that scrolls"). Put the rows under `content` with the create_*
// helpers, which give every row a UI component the same way.
ScrollView create_scroll_view(void* parent, const char* name, Vec2 position, Vec2 size, float contentHeight);

// Make the content `contentHeight` tall, for a list whose rows come and go; the part showing stays showing.
void set_scroll_height(ScrollView& view, float contentHeight);

// Scroll no further than it must to show the band that starts `fromTop` canvas units below the content's top
// edge and is `band` tall. Does nothing when the whole list already fits the view.
void scroll_reveal(const ScrollView& view, float fromTop, float band);

// The arithmetic scroll_reveal runs, on its own so it can be tested without a game: how far the list has to
// be scrolled down (0 at the top of the list, contentHeight - viewHeight at the bottom) for the band that
// starts `bandTop` below the content's top edge and is `bandHeight` tall to be on screen. The content moves
// only as far as it must (a
// band already showing leaves it at `currentTop`) and never past either end of what it can travel; with the
// whole list inside the view the answer is 0.
inline float scroll_top_needed(float contentHeight, float viewHeight, float currentTop, float bandTop,
                               float bandHeight) {
  const float span = contentHeight - viewHeight;
  if (!(span > 0.0f)) return 0.0f;
  float top = currentTop;
  if (bandTop < top)
    top = bandTop;
  else if (bandTop + bandHeight > top + viewHeight)
    top = bandTop + bandHeight - viewHeight;
  if (top < 0.0f) return 0.0f;
  return top > span ? span : top;
}

// Pointer hit tests against a UI element: camera is null on a screen-space overlay canvas, and the
// camera that renders it on a world-space canvas.
bool contains(void* component, Vec2 screenPoint, void* camera = nullptr);
// Where screenPoint falls inside the element, 0..1 from its bottom-left corner; false if outside.
bool normalized_point(void* component, Vec2 screenPoint, Vec2& out, void* camera = nullptr);

// A canvas drawn in the world (RenderMode.WorldSpace), as a flat object under the Transform `follow`, at its origin and
// turned with it: one canvas unit is `metresPerUnit` metres. For a VR player, whose screen is the world. Lay out its
// children with place() as on an overlay canvas, centred on (0, 0) with a size in canvas units.
void* create_world_canvas(const char* name, void* follow, float metresPerUnit);

// A Texture2D of `width` by `height` RGBA texels (row by row from the bottom, 4 bytes each), kept until destroyed:
// for shapes a mod draws in code. Null when `rgba` is not width * height * 4 bytes or the entries did not resolve.
void* texture_from_rgba(int width, int height, const uint8_t* rgba, size_t bytes);

}  // namespace pm::ui
