#pragma once

#include <cstddef>
#include <cstdint>

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

// RectTransform layout: anchors and pivot in 0..1 of the parent, position and size in canvas units.
void place(void* component, Vec2 anchor, Vec2 pivot, Vec2 position, Vec2 size);
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
