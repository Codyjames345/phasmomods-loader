#include "picker.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "body.h"
#include "game.h"
#include "journal.h"
#include "log.h"
#include "mapview.h"
#include "panorama.h"
#include "round.h"
#include "ui.h"

namespace pg::picker {
namespace {

using il2cpp::Quaternion;
using il2cpp::Vector3;
using ui::Color;

constexpr int kSortingOrder = 70;
constexpr Color kBackdrop{0.02f, 0.02f, 0.03f, 0.94f};
constexpr Color kText{1, 1, 1, 0.95f};
constexpr ULONGLONG kSendRemainingMs = 2000;  // unconfirmed choices go to the host this close to the end

std::string two_digits(unsigned v) { return (v < 10 ? "0" : "") + std::to_string(v); }

std::string clock(uint64_t deadline, ULONGLONG now) {
  const unsigned left = static_cast<unsigned>((deadline > now ? deadline - now + 999 : 0) / 1000);
  return std::to_string(left / 60) + ":" + two_digits(left % 60);
}

void set_if_changed(void* text, std::string& shown, const std::string& value) {
  if (!text || value == shown) return;
  shown = value;
  ui::set_text(text, value.c_str());
}

// ---- The journal: the evidence spread ---------------------------------------------------------

// The mod's page over the evidence page, in journal page units (916 x 1460, origin at the centre).
constexpr ui::Vec2 kHeadingPosition = {0, 630};  // where the journal's page titles sit
constexpr ui::Vec2 kHeadingSize = {825, 121};
constexpr ui::Vec2 kMapPosition = {0, 70};
constexpr ui::Vec2 kMapSize = {760, 760};
constexpr float kFloorRowY = -390;
constexpr float kRoomRowY = -475;
constexpr float kHintRowY = -560;
constexpr float kConfirmRowY = -645;
constexpr float kArrowX = 230;
constexpr ui::Vec2 kArrowSize = {120, 80};
constexpr ui::Vec2 kLineSize = {820, 80};
constexpr int kHighlights = 12;  // room collider rectangles drawn at once (selected and hovered)
constexpr Color kSelectedRoom = {0.55f, 0.08f, 0.05f, 0.45f};  // journal ink red
constexpr Color kHoveredRoom = {0.2f, 0.2f, 0.2f, 0.3f};

// Start: the contract's first choice of ghost and room. Claim: a new favourite room after a native change.
enum class Mode { None, Start, Claim };

// The page's text and markers, rebuilt each time the spread is taken over (it ends when the pause
// menu is opened from the picker).
struct Page {
  il2cpp::Held floorLabel;
  il2cpp::Held roomLabel;
  il2cpp::Held hint;
  il2cpp::Held confirm;
  il2cpp::Held arrowLeft;
  il2cpp::Held arrowRight;
  il2cpp::Held highlights[kHighlights];
  std::string floorShown, roomShown, hintShown, confirmShown, ghostTitleShown, ghostLineShown;
  bool built = false;
  bool armed = false;  // the button was up since the page was built: a click that opened it is not a choice
} p;

struct BookPicker {
  Mode mode = Mode::None;
  uint64_t deadline = 0;  // the offer this belongs to
  int type = -1;          // chosen GhostType
  int room = -1;          // chosen favourite-room index
  bool sent = false;
  bool lists = false;  // the plain lists instead (no journal or map)
} b;

void* line(const journal::Spread* s, float y, ui::Vec2 size) {
  void* t = journal::copy_text(s->description, s->page);
  if (!t) return nullptr;
  ui::place(t, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, y}, size);
  ui::set_alignment(t, ui::Align::Center);
  return t;
}

// Selected and hovered rooms as rectangles over the map, in the order given.
void show_highlights(const std::vector<std::pair<ui::Rect, Color>>& boxes) {
  for (int i = 0; i < kHighlights; ++i) {
    void* h = p.highlights[i];
    if (!h) continue;
    const bool on = i < static_cast<int>(boxes.size());
    ui::set_active(h, on);
    if (!on) continue;
    const auto& [r, colour] = boxes[i];
    ui::place(h, {r.x + r.width / 2, r.y + r.height / 2}, {0.5f, 0.5f}, {0, 0}, {r.width * kMapSize.x, r.height * kMapSize.y});
    ui::set_color(h, colour);
  }
}

void drop_page() {
  if (p.built) mapview::hide();  // the truck's map camera and floor as they were
  p = Page{};
}

bool build_page(const char* heading) {
  drop_page();
  const journal::Spread* s = journal::begin_spread();
  void* camera = body::camera();
  if (!s || !s->page || !s->title || !s->description || !game::alive(camera) ||
      !mapview::show(s->page, kMapPosition, kMapSize, camera)) {
    journal::end_spread();
    return false;
  }
  p.built = true;
  if (void* h = journal::copy_text(s->title, s->page)) {
    ui::place(h, {0.5f, 0.5f}, {0.5f, 0.5f}, kHeadingPosition, kHeadingSize);
    ui::set_alignment(h, ui::Align::Center);
    ui::set_text(h, heading);
  }
  p.floorLabel = line(s, kFloorRowY, {360, kLineSize.y});
  p.roomLabel = line(s, kRoomRowY, kLineSize);
  p.hint = line(s, kHintRowY, kLineSize);
  p.confirm = line(s, kConfirmRowY, kLineSize);
  p.arrowLeft = line(s, kFloorRowY, kArrowSize);
  p.arrowRight = line(s, kFloorRowY, kArrowSize);
  ui::place(p.arrowLeft, {0.5f, 0.5f}, {0.5f, 0.5f}, {-kArrowX, kFloorRowY}, kArrowSize);
  ui::place(p.arrowRight, {0.5f, 0.5f}, {0.5f, 0.5f}, {kArrowX, kFloorRowY}, kArrowSize);
  ui::set_text(p.arrowLeft, "<b><</b>");
  ui::set_text(p.arrowRight, "<b>></b>");
  for (il2cpp::Held& h : p.highlights) h = ui::create_image(mapview::image(), "Room", kHoveredRoom);
  PG_LOG("picker: the journal's evidence spread shows the %s page", b.mode == Mode::Claim ? "favourite-room" : "ghost");
  return true;
}

// Leaves the spread; the journal stays open on its pause menu if that is where the player is.
void end_book() {
  if (b.mode == Mode::None) return;
  drop_page();
  journal::end_spread();
  if (!journal::on_pause()) journal::close();
  b = BookPicker{};
}

bool offered(const session::PickOffer& offer, int type) {
  if (offer.fixedType != session::kNoType) return type == offer.fixedType;
  return std::find(offer.types.begin(), offer.types.end(), static_cast<uint8_t>(type)) != offer.types.end();
}

void send(session::Session& s, const session::PickOffer& offer) {
  session::Choice c;
  if (offer.fixedType == session::kNoType) c.type = b.type;
  c.room = b.room;
  s.send_pick(c);
  b.sent = true;
  PG_LOG("picker: sent type %d room %d", c.type, c.room);
}

// Unconfirmed choices go to the host when the time is nearly up.
void send_if_late(session::Session& s, const session::PickOffer& offer, ULONGLONG now) {
  if (b.mode == Mode::Start && !b.sent && now + kSendRemainingMs >= offer.deadline && (b.type >= 0 || b.room >= 0))
    send(s, offer);
}

// Clicks on the map and its floor arrows; returns true if one was used.
bool click_map(ui::Vec2 at, void* camera) {
  if (ui::contains(p.arrowLeft, at, camera)) mapview::change_floor(false);
  else if (ui::contains(p.arrowRight, at, camera)) mapview::change_floor(true);
  else if (const int room = mapview::room_at(at); room >= 0) b.room = room;
  else return false;
  return true;
}

void show_rooms(bool pointer, ui::Vec2 at) {
  std::vector<std::pair<ui::Rect, Color>> boxes;
  for (const ui::Rect& r : mapview::room_outline(b.room)) boxes.push_back({r, kSelectedRoom});
  const int hovered = pointer && !b.sent ? mapview::room_at(at) : -1;
  if (hovered >= 0 && hovered != b.room)
    for (const ui::Rect& r : mapview::room_outline(hovered)) boxes.push_back({r, kHoveredRoom});
  show_highlights(boxes);
  set_if_changed(p.floorLabel, p.floorShown, mapview::floor_name());
  set_if_changed(p.roomLabel, p.roomShown, b.room >= 0 ? "<b>" + round::room_name(b.room) + "</b>" : "Click a room on the map");
}

std::string link(const char* label, bool hovered) {
  return std::string(hovered ? "<b><u>" : "<u>") + label + (hovered ? "</u></b>" : "</u>");
}

void tick_start_page(session::Session& s, const session::PickOffer& offer, bool focused, ULONGLONG now) {
  const journal::Spread* spread = journal::begin_spread();  // already taken over: returns it
  if (!spread) return;
  const bool fixed = offer.fixedType != session::kNoType;
  const bool ready = (fixed || b.type >= 0) && b.room >= 0;
  float mx = 0, my = 0;
  const bool pointer = focused && game::mouse_position(mx, my);
  const ui::Vec2 at{mx, my};
  void* cam = body::camera();
  if (!game::mouse_left_held()) p.armed = true;
  if (pointer && p.armed && !b.sent && game::mouse_left_pressed()) {
    if (ready && ui::contains(p.confirm, at, cam)) send(s, offer);
    else if (!click_map(at, cam) && !fixed) {
      for (const journal::GhostButton& g : spread->ghosts)
        if (offered(offer, g.type) && ui::contains(g.rect, at, cam)) b.type = g.type;
    }
  }
  send_if_late(s, offer, now);
  const journal::GhostButton* hovered = nullptr;
  for (const journal::GhostButton& g : spread->ghosts) {
    const bool chosen = fixed ? g.type == offer.fixedType : g.type == b.type;
    journal::set_state(g, !offered(offer, g.type) ? 2 : chosen ? 1 : 0);
    if (pointer && !b.sent && !fixed && offered(offer, g.type) && ui::contains(g.rect, at, cam)) hovered = &g;
  }
  journal::hover(hovered);
  set_if_changed(spread->ghostTitle, p.ghostTitleShown, "Ghost type");
  set_if_changed(spread->ghostLine, p.ghostLineShown,
                 fixed ? "This contract's difficulty has chosen your ghost" : "Which ghost will you be?");
  show_rooms(pointer, at);
  const int type = fixed ? offer.fixedType : b.type;
  std::string hint;
  if (type >= 0) hint = "You will be a <b>" + round::ghost_type_name(type) + "</b>";
  else hint = "Choose your ghost on the right-hand page";
  set_if_changed(p.hint, p.hintShown, hint);
  std::string confirm;
  if (b.sent) confirm = "Your ghost is ready  " + clock(offer.deadline, now);
  else if (ready) confirm = link("Begin the haunting", pointer && ui::contains(p.confirm, at, cam)) + "  " + clock(offer.deadline, now);
  else confirm = clock(offer.deadline, now) + "  anything left unchosen is random";
  set_if_changed(p.confirm, p.confirmShown, confirm);
}

void tick_claim_page(session::Session& s, const session::RoomOffer& offer, bool focused, ULONGLONG now) {
  const int favourite = round::favourite_room();
  const bool ready = b.room >= 0 && b.room != favourite;
  float mx = 0, my = 0;
  const bool pointer = focused && game::mouse_position(mx, my);
  const ui::Vec2 at{mx, my};
  void* cam = body::camera();
  if (!game::mouse_left_held()) p.armed = true;
  if (pointer && p.armed && !b.sent && game::mouse_left_pressed()) {
    if (ready && ui::contains(p.confirm, at, cam)) {
      s.send_room_claim(b.room);
      b.sent = true;
      PG_LOG("picker: claimed room %d as the favourite room", b.room);
    } else {
      click_map(at, cam);
    }
  }
  show_rooms(pointer, at);
  if (const journal::Spread* spread = journal::begin_spread()) {
    const uint8_t type = session::named_type(s.status());
    set_if_changed(spread->ghostTitle, p.ghostTitleShown, "Your ghost");
    set_if_changed(spread->ghostLine, p.ghostLineShown,
                   type != session::kNoType ? "You are a <b>" + round::ghost_type_name(type) + "</b>" : std::string());
  }
  set_if_changed(p.hint, p.hintShown,
                 favourite >= 0 ? "Your favourite room moved to <b>" + round::room_name(favourite) + "</b>"
                                : std::string("Your favourite room moved"));
  const std::string confirm = ready ? link("Make it my favourite room", pointer && ui::contains(p.confirm, at, cam))
                                    : std::string("Choose another room");
  set_if_changed(p.confirm, p.confirmShown, confirm + "  " + clock(offer.deadline, now));
}

// ---- Fallback: two plain lists --------------------------------------------------------------

constexpr int kRows = 11;  // visible rows per list, centred on the selection
constexpr float kRowHeight = 42;
constexpr float kColumnX = 320;
constexpr ULONGLONG kRepeatDelayMs = 400;
constexpr ULONGLONG kRepeatMs = 110;

struct Item {
  int id;  // GhostType or favourite-room index
  std::string name;
};

struct Column {
  std::vector<Item> items;
  int selected = 0;
  int confirmed = -1;  // index into items
  il2cpp::Held header;
  il2cpp::Held rows[kRows];
  std::string shown[kRows];
};

struct ListPicker {
  il2cpp::Held canvas;
  il2cpp::Held title;
  il2cpp::Held hint;
  Column column[2];  // 0 ghost type, 1 favourite room
  int active = 0;
  bool typeFixed = false;
  std::string titleShown, hintShown;
  bool held[5] = {};
  ULONGLONG repeatAt = 0;
} l;

std::vector<Item> sorted(std::vector<Item> items) {
  std::sort(items.begin(), items.end(), [](const Item& a, const Item& c) { return a.name < c.name; });
  return items;
}

void build_lists(const session::PickOffer& offer) {
  std::vector<Item> types;
  if (offer.fixedType != session::kNoType) {
    types.push_back({offer.fixedType, round::ghost_type_name(offer.fixedType)});
  } else {
    for (uint8_t t : offer.types) types.push_back({t, round::ghost_type_name(t)});
  }
  std::vector<Item> rooms;
  for (int i = 0; i < offer.rooms; ++i) rooms.push_back({i, round::room_name(i)});
  l.column[0].items = sorted(std::move(types));
  l.column[1].items = sorted(std::move(rooms));
  l.typeFixed = offer.fixedType != session::kNoType;
  if (l.typeFixed) l.column[0].confirmed = 0;
  l.active = l.typeFixed ? 1 : 0;
}

void build_list_ui() {
  l.canvas = ui::create_canvas("PlayableGhostPicker", kSortingOrder);
  if (!l.canvas) return;
  void* backdrop = ui::create_image(l.canvas, "Backdrop", kBackdrop);
  ui::place(backdrop, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {4000, 3000});
  l.title = ui::create_text(l.canvas, "Title", 40);
  ui::place(l.title, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 400}, {1400, 56});
  l.hint = ui::create_text(l.canvas, "Hint", 24);
  ui::place(l.hint, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -420}, {1600, 36});
  const char* headers[2] = {"Ghost type", "Favourite room"};
  for (int c = 0; c < 2; ++c) {
    const float x = c == 0 ? -kColumnX : kColumnX;
    l.column[c].header = ui::create_text(l.canvas, "Header", 30);
    ui::place(l.column[c].header, {0.5f, 0.5f}, {0.5f, 0.5f}, {x, 300}, {560, 44});
    ui::set_text(l.column[c].header, headers[c]);
    for (int r = 0; r < kRows; ++r) {
      l.column[c].rows[r] = ui::create_text(l.canvas, "Row", 28);
      ui::place(l.column[c].rows[r], {0.5f, 0.5f}, {0.5f, 0.5f}, {x, 230 - r * kRowHeight}, {560, kRowHeight});
    }
  }
  for (void* t : std::initializer_list<void*>{l.title, l.hint, l.column[0].header, l.column[1].header})
    ui::set_color(t, kText);
}

void render_column(int c) {
  Column& col = l.column[c];
  const int n = static_cast<int>(col.items.size());
  const int first = std::clamp(col.selected - kRows / 2, 0, std::max(0, n - kRows));
  for (int r = 0; r < kRows; ++r) {
    const int i = first + r;
    std::string text;
    if (i < n) {
      const bool selected = c == l.active && i == col.selected;
      const bool confirmed = i == col.confirmed;
      const char* alpha = selected ? "<alpha=#FF>" : confirmed ? "<alpha=#DD>" : c == l.active ? "<alpha=#77>" : "<alpha=#44>";
      text = std::string(alpha) + (confirmed ? "<b>" : "") + (selected ? "> " : "") + col.items[i].name +
             (selected ? " <" : "") + (confirmed ? "</b>" : "");
    }
    set_if_changed(col.rows[r], col.shown[r], text);
  }
}

// Edge-triggered presses, with auto-repeat for the list keys.
bool pressed(int index, bool down, bool repeats, ULONGLONG now) {
  const bool was = l.held[index];
  l.held[index] = down;
  if (down && !was) {
    if (repeats) l.repeatAt = now + kRepeatDelayMs;
    return true;
  }
  if (down && repeats && now >= l.repeatAt) {
    l.repeatAt = now + kRepeatMs;
    return true;
  }
  return false;
}

void read_list_input(session::Session& s, const Keys& k, bool on, ULONGLONG now) {
  Column& col = l.column[l.active];
  const int n = static_cast<int>(col.items.size());
  if (pressed(0, on && bindings::down(k.up), true, now) && n) col.selected = (col.selected + n - 1) % n;
  if (pressed(1, on && bindings::down(k.down), true, now) && n) col.selected = (col.selected + 1) % n;
  const bool left = pressed(2, on && bindings::down(k.left), false, now);
  const bool right = pressed(3, on && bindings::down(k.right), false, now);
  if ((left || right) && !l.typeFixed) l.active = 1 - l.active;
  if (pressed(4, on && bindings::down(k.confirm), false, now) && n) {
    col.confirmed = col.selected;
    if (l.active == 0) l.active = 1;
    session::Choice c;
    if (!l.typeFixed && l.column[0].confirmed >= 0) c.type = l.column[0].items[l.column[0].confirmed].id;
    if (l.column[1].confirmed >= 0) c.room = l.column[1].items[l.column[1].confirmed].id;
    s.send_pick(c);
  }
}

void tick_list(session::Session& s, const session::PickOffer& offer, const Keys& keys, bool focused, ULONGLONG now) {
  if (focused) game::lock_cursor();  // the lists are driven by keys, as the ghost is
  read_list_input(s, keys, focused && game::cursor_locked(), now);
  const bool done = l.column[0].confirmed >= 0 && l.column[1].confirmed >= 0;
  set_if_changed(l.title, l.titleShown,
                 std::string(done ? "Your ghost is ready" : "You are the ghost") + "   <alpha=#AA>" + clock(offer.deadline, now));
  set_if_changed(l.hint, l.hintShown,
                 "<alpha=#BB>[" + keys.up.label + "] [" + keys.down.label + "] choose     [" + keys.left.label + "] [" +
                     keys.right.label + "] switch list     [" + keys.confirm.label +
                     "] confirm     <alpha=#77>Anything unconfirmed is chosen at random");
  render_column(0);
  render_column(1);
}

void hide_list() {
  if (l.canvas) ui::destroy(l.canvas);
  l = ListPicker{};
}


void use_lists(const session::PickOffer& offer) {
  b.lists = true;
  build_lists(offer);
  build_list_ui();
}

// The contract's first choice. Escape switches between the journal's pause menu and the choice;
// the pause menu's Resume (which closes the journal) also comes back to it.
bool tick_start(session::Session& s, const session::PickOffer& offer, const Keys& keys, const Input& in, ULONGLONG now) {
  if (b.mode != Mode::Start || b.deadline != offer.deadline) {
    hide();
    b.mode = Mode::Start;
    b.deadline = offer.deadline;
    if (!journal::kept()) {
      PG_LOG("picker: the ghost player has no journal; showing the plain lists");
      use_lists(offer);
    }
  }
  if (b.lists) {
    if (!l.canvas) return false;
    tick_list(s, offer, keys, in.focused, now);
    return true;
  }
  void* camera = body::camera();
  const bool leavingPause = in.pausePressed && journal::on_pause();
  if (in.pausePressed && !leavingPause) {
    drop_page();
    journal::open(true);  // ends the spread
  }
  if (in.focused) game::free_cursor();
  panorama::show(camera, b.room);
  if (journal::on_pause() && !leavingPause) {
    send_if_late(s, offer, now);  // a choice already made still reaches the host in time
    return true;
  }
  if (!journal::spread_active() && !build_page("Favourite room")) {
    PG_LOG("picker: the journal's spread or the map is unavailable; showing the plain lists");
    if (!journal::on_pause()) journal::close();
    use_lists(offer);
    return l.canvas != nullptr;
  }
  tick_start_page(s, offer, in.focused, now);
  return true;
}

// A new favourite room, while the host allows one after a native change: opened with the Journal key.
bool tick_claim(session::Session& s, const Input& in, ULONGLONG now) {
  const session::RoomOffer& offer = s.room_offer();
  const bool open = offer.valid && now < offer.deadline;
  if (b.mode == Mode::Claim) {
    if (open && !b.sent && !in.pausePressed && !in.journalPressed && journal::spread_active()) {
      if (in.focused) game::free_cursor();
      tick_claim_page(s, offer, in.focused, now);
      return true;
    }
    end_book();
    if (in.pausePressed) journal::open(true);  // Escape from the page goes to the pause menu, as elsewhere
    return true;  // this frame's keys were the page's
  }
  if (!open || !in.journalPressed || !journal::kept()) return false;
  b.mode = Mode::Claim;
  b.deadline = offer.deadline;
  if (!build_page("New favourite room")) {
    end_book();
    return false;
  }
  return true;
}

}  // namespace

bool tick(session::Session& s, const Keys& keys, const Input& in) {
  const session::PickOffer& offer = s.pick_offer();
  const ULONGLONG now = GetTickCount64();
  if (offer.valid && now < offer.deadline) return tick_start(s, offer, keys, in, now);
  if (b.mode == Mode::Start || l.canvas) hide();  // the choice is over
  return tick_claim(s, in, now);
}

void hide() {
  end_book();
  hide_list();
}

}  // namespace pg::picker
