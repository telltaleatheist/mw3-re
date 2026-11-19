#pragma once
// MW1's screen template (VISION.md, "Screens": one reusable screen for the
// hub, the market and the rest): a zTwoD dialog from one of MW1's own layouts
// (a .zrd in MW1's data folder) with its buttons bound by name, each with
// MW1's words, and blocks of text it is given. Built as MW3's briefing screen
// is. The art is Pirate's Moon's, named in the layout and read from the
// player's install: a BACKGROUND primitive (else the main menu's backdrop,
// borrowed as its sub-screens do), PANEL images behind the texts, the buttons'
// bitmaps (DISABLE's when greyed).
//
// The screen only shows what it is given (ScreenView) and says which button
// was pressed; the game loop (mw1_game.cpp) decides everything.
//
// MW1's own art (FIXES.md item 3; mw1image.zbd, its images found by name from
// any layout) adds our keys to a layout, which the game's own readers ignore:
// - ANCHOR ("LEFT" | "CENTRE" | "RIGHT") on the BACKGROUND, a PANEL, a
//   button or a text place: the canvas is 640 wide at 4:3 and wider at 16:9,
//   and a piece keeps its place from that side (the centre: half the extra
//   width). The template places every piece itself (the engine's widening,
//   GameEngine::widen_screen, leaves them be): without an ANCHOR, a 4:3
//   layout's pieces are centred on a wider canvas, its texts, marks and mouse
//   area with them, as the engine centres its own menus; a _WIDE layout's
//   stay where it puts them. MW1's scenes are 854 wide, drawn centred
//   (BACKGROUND at -107 on 4:3, ANCHOR "CENTRE").
// - LABEL_AT (x y), LABEL_FONT and LABEL_OFF_FONT on a button: its words' place
//   and fonts (a centred font: x is the words' middle).
// - TEXTS: the screen's text places by name, each a POSITION, and a FONT, a
//   GAP (from one line to the next) and an ANCHOR when it has them; a block
//   names its place (TextBlock::at), so the layout places the words too.
// - <dialog>_WIDE: the same screen laid out for the wide canvas (853, 16:9),
//   taken in place of <dialog> when the canvas is wider than 4:3's; its places
//   are the wide canvas's (anchored from 853 on a wider one still).
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gamez/ztwod/twod.h"
#include "mech3/app_states.h"
#include "mech3/menu_states.h"
#include "mech3/menu_widgets.h"

namespace mech3 {
class GameEngine;
}

namespace mw1::screens {

// A button: its name in the layout's BUTTONS (its id is its place in the list, from 1) and its words. A repeat button
// (FIXES.md item 24: the office's term arrows, the lists' arrows) acts on the press, and again while it is held down
// on it: after kRepeatDelay, every kRepeatEvery, every kRepeatFast once held kRepeatFastAfter.
struct ScreenButton {
  const char* name;
  const char* words;
  bool repeat = false;
};
inline constexpr float kRepeatDelay = 0.4f, kRepeatEvery = 0.08f, kRepeatFast = 0.04f, kRepeatFastAfter = 2.0f;

// The fonts (PM's fonts.zrd, by name).
inline constexpr const char* kPlaqueFont = "main_red";
inline constexpr const char* kPlaqueSmallFont = "main_red_small";
inline constexpr const char* kTitleFont = "main_green";
inline constexpr const char* kLineFont = "brief_green";
inline constexpr const char* kNoteFont = "brief_yellow";
inline constexpr int32_t kLineGap = 16;

// Where a piece keeps its place from on a wider canvas (a layout's ANCHOR, a block's anchor). kLayout: the
// layout's own way, a 4:3 layout centred (as the engine centres its own menus' pieces), a _WIDE one from the left.
enum class Anchor { kLayout, kLeft, kCentre, kRight };

// A line's highlighted spans (FIXES.md item 20, the key facts in a brighter colour): each kHighlight toggles one on or
// off; a span is drawn in the line's font's highlight (its name with kHighlightSuffix, when the fonts have it; else
// the line's own). Left-aligned lines only. shown() gives the line without them.
inline constexpr int32_t kStatusGap = 14;  // the status bar: from the world's name to its words
inline constexpr int32_t kStatusMargin = 8;  // the status bar's words end this far inside the layout's right
inline constexpr char kHighlight = '\x02';
inline constexpr const char* kHighlightSuffix = "_hi";
std::string highlighted(const std::string& s);  // s between two kHighlight
std::string plain_line(const std::string& s);   // s without its kHighlight marks

// Lines of text at a place, in a font, one under another.
struct TextBlock {
  int32_t x = 0, y = 0;
  const char* font = kLineFont;
  std::vector<std::string> lines;
  int32_t gap = kLineGap;  // from one line to the next
  const char* at = nullptr;  // a place in the layout's TEXTS: its POSITION, FONT, GAP and ANCHOR in place of these
  Anchor anchor = Anchor::kLayout;
  int32_t dy = 0;  // with `at`: this far below the place (a box sized to its text, its top lower for fewer lines)
};
// Lines at a layout's text place, `dy` below it.
inline TextBlock at_place_down(const char* place, std::vector<std::string> lines, int32_t dy) {
  TextBlock b;
  b.at = place, b.lines = std::move(lines), b.dy = dy;
  return b;
}
// Lines at a layout's text place (TEXTS).
inline TextBlock at_place(const char* place, std::vector<std::string> lines) {
  TextBlock b;
  b.at = place, b.lines = std::move(lines);
  return b;
}
struct ButtonState {
  bool there = true;  // shown
  bool on = true;     // enabled (else greyed: its DISABLE bitmap and grey words)
  std::string words;  // its words now (empty: its own, ScreenButton's)
  std::string look;   // its images now: <look>, _lit, _down, _off (a picture tile's, mw1_chrome.h); empty: the layout's
};
// A shape drawn over the panels and under the texts (the travel map's worlds and markers): a ring (a circle's
// outline, `size` its radius; radius 1 reads as a dot), a line to (x2, y2), or a square's outline `size` from its
// centre. Its colour as red, green, blue.
struct Mark {
  enum class Kind { kRing, kLine, kSquare };
  Kind kind = Kind::kRing;
  int32_t x = 0, y = 0, x2 = 0, y2 = 0, size = 1;
  uint8_t r = 255, g = 255, b = 255;
};
struct ScreenView {
  std::string backdrop;  // the BACKGROUND's image now, by name (the hub's scenery); empty: the layout's
  std::vector<Mark> marks;
  std::vector<TextBlock> blocks;
  std::vector<ButtonState> buttons;  // by id - 1; missing ones shown and enabled
  std::vector<bool> panels;          // the layout's PANEL, PANEL2, ... shown; missing ones shown
  // A PANEL's picture now, by its place among the panels (a portrait, a mech): its image's name; empty, or past
  // the end: the layout's own.
  std::vector<std::string> pictures;
};

class Mw1Screen : public mech3::MenuScreen {
 public:
  // The layout `dialog` read from `file`; `pressed` hears each button pressed (a shown, enabled one).
  Mw1Screen(mech3::Engine& e, zgame::Handle state, const char* file, const char* dialog, std::vector<ScreenButton> buttons,
            std::function<void(int32_t)> pressed);
  ~Mw1Screen() override;
  bool loaded() const { return loaded_; }  // the layout read and every button bound
  void show(const ScreenView& v);
  void on_button(int32_t id, int32_t a, int32_t b) override;
  // For the tests: every line of text shown now (the blocks in order, then the buttons' words), and a button's state.
  std::vector<std::string> shown() const;
  bool visible(int32_t id) const;
  bool enabled(int32_t id) const;
  bool panel_shown(size_t i) const;  // the layout's i-th PANEL
  bool panel_at(size_t i, int32_t* x, int32_t* y) const;  // its place on the canvas (false: no such panel)
  int32_t button_count() const { return static_cast<int32_t>(buttons_.size()); }
  bool button_middle(int32_t id, int32_t* x, int32_t* y) const;  // for the tests: where to click it (false: not there)
  // A part of the screen that hears the mouse (the travel map): `heard` with the mouse's place on each move over it
  // (click false) and on a left click there (true). point() is the same from a test.
  void set_pointer(int32_t left, int32_t top, int32_t right, int32_t bottom, std::function<void(int32_t, int32_t, bool)> heard);
  void point(int32_t x, int32_t y, bool click);
  // `hover` hears a button's id as the mouse comes onto it (enabled), 0 as it goes off; hover_on() is the same from a
  // test.
  void set_hover(std::function<void(int32_t)> hover) { hover_ = std::move(hover); }
  void hover_on(int32_t id) {
    if (hover_) hover_(id);
  }
  const std::vector<Mark>& marks() const { return marks_now_; }  // for the tests: the shapes shown now
  int32_t shift(Anchor a) const;  // how far a piece moves right from its layout's place on this canvas
  bool wide() const { return wide_; }  // laid out by <dialog>_WIDE
  std::string backdrop() const { return backdrop_; }  // for the tests: the BACKGROUND's image's name now
  std::string look(int32_t id) const;                 // and a button's look ("" the layout's)
  std::string picture(size_t i) const { return i < pictures_.size() ? pictures_[i] : ""; }  // a PANEL's, as asked

 private:
  ztwod::Text* text(std::vector<std::unique_ptr<ztwod::Text>>& pool, size_t i, int32_t x, int32_t y, const char* font);
  void set_look(size_t i, const std::string& look);
  std::vector<ScreenButton> names_;
  std::vector<std::string> label_fonts_, label_off_fonts_, looks_;  // by id - 1
  std::string backdrop_;
  std::vector<std::string> pictures_;  // the panels' pictures as asked (ScreenView::pictures)
  int32_t extra_ = 0;  // the canvas's width past the layout's (640, or 853 for a _WIDE one)
  bool wide_ = false;
  struct Place {
    int32_t x = 0, y = 0, gap = -1;
    std::string font;
  };
  std::vector<std::pair<std::string, Place>> places_;  // TEXTS, anchored
  std::function<void(int32_t)> pressed_;
  std::function<void(int32_t)> hover_;  // the buttons hold its address (set_hover)
  bool loaded_ = false;
  std::vector<std::unique_ptr<mech3::HelpButton>> buttons_;   // by id - 1
  std::vector<std::unique_ptr<ztwod::Text>> labels_;          // the buttons' words, by id - 1
  std::vector<std::vector<std::unique_ptr<ztwod::Text>>> blocks_;
  std::vector<std::vector<std::unique_ptr<ztwod::Text>>> spans_;  // by block: a line's spans after its first
  std::vector<std::vector<std::string>> shown_lines_;
  void fit_status(ztwod::Text* t, int32_t y, size_t world_block, int32_t world_x);              // by block: the lines as shown, plain
  std::vector<std::unique_ptr<ztwod::Image>> panels_;         // the layout's PANEL images, behind the texts
  std::vector<std::unique_ptr<ztwod::Frame>> rings_;           // the marks, by kind, reused show to show
  std::vector<std::unique_ptr<ztwod::Line2D>> lines_;
  std::vector<std::unique_ptr<ztwod::PolyLine>> squares_;
  std::vector<Mark> marks_now_;
  std::unique_ptr<ztwod::Object2D> pointer_;
  std::function<void(int32_t, int32_t, bool)> heard_;     // in the layout's places (point())
  std::function<void(int32_t, int32_t, bool)> on_canvas_;  // the pointer's: the canvas's places, made the layout's
};

// A screen's state: pushed by the loop, its screen made on entering
// (Engine::adopt_screen) and gone on leaving, as the engine's own menus are.
// Escape presses `escape_id`.
class Mw1State : public mech3::MenuState {
 public:
  Mw1State(zgame::Handle h, const char* name, const char* file, const char* dialog, std::vector<ScreenButton> buttons,
           int32_t escape_id, std::function<void(int32_t)> pressed);
  bool enter() override;
  void leave() override;
  void escape() override;
  void show(const ScreenView& v);  // now, and on each entering
  Mw1Screen* screen() { return screen_ptr_; }
  // The mouse over a part of the screen (Mw1Screen::set_pointer), set on each entering; and the keys: each frame's
  // first plain key (its DirectInput code) to `keys`, Escape aside (escape_id's button).
  void set_pointer(int32_t left, int32_t top, int32_t right, int32_t bottom, std::function<void(int32_t, int32_t, bool)> heard);
  void set_keys(std::function<void(uint32_t)> keys) { keys_ = std::move(keys); }
  // Ours (the live mech views, mech3/model_view.h): a frame drawn as the Mech Lab draws its own: `begin` before the 2D
  // (the target set and cleared), `holes` the canvas rects (x, y, w, h) left see-through in the 2D, `after` once
  // the 2D is down (the 3D drawn into the holes). Unset: the menu's own frame.
  struct View3D {
    std::function<void(mech3::GameEngine&)> begin;
    std::function<std::vector<std::array<int32_t, 4>>(Mw1Screen&)> holes;
    std::function<void(mech3::GameEngine&, Mw1Screen&)> after;
  };
  void set_view3d(View3D v) { view3d_ = std::move(v); }
  // The mouse wheel turned (each frame it moved: +1 up, -1 down; FIXES.md item 38, the results' list).
  void set_wheel(std::function<void(int32_t)> wheel) { wheel_ = std::move(wheel); }
  void set_hover(std::function<void(int32_t)> hover) {  // the buttons' hover (Mw1Screen::set_hover), set on each entering
    hover_ = hover;
    if (screen_ptr_) screen_ptr_->set_hover(std::move(hover));
  }
  void key(uint32_t code) {  // a key as pressed (tests)
    if (keys_) keys_(code);
  }
  bool update() override;
  void resume(uint32_t arg) override;

 private:
  bool fading_frame();  // a frame while the fade between screens runs (FIXES.md item 52)
  void draw_faded(bool step_dt);  // the screen drawn under the fade's black, and shown
  const char* file_;
  const char* dialog_;
  std::vector<ScreenButton> buttons_;
  int32_t escape_id_;
  std::function<void(int32_t)> pressed_;
  ScreenView view_;
  Mw1Screen* screen_ptr_ = nullptr;
  int32_t pointer_rect_[4] = {};
  std::function<void(int32_t, int32_t, bool)> pointer_;
  std::function<void(uint32_t)> keys_;
  std::function<void(int32_t)> wheel_;
  View3D view3d_;
  bool frame_3d();  // the frame with view3d_ (false: not drawn so, the menu's own)
  int32_t wheel_at_ = 0;  // the wheel's position last frame
  bool wheel_read_ = false;
  std::function<void(int32_t)> hover_;
};

// FIXES.md item 52: after each frame shown (Engine::on_present), the fade between screens on a screen that isn't
// MW1's (Pirate's Moon's lab, its options): stepped, and its black left for the next frame's scene.
void fade_present(mech3::Engine& en);
// The same inside a frame that isn't an MW1 state's, drawn in a lock (the loading screen in PM's Load state): the
// black over what was drawn so far.
void fade_over_layer(mech3::Engine& en);
// FIXES.md item 54: for a state that draws its own frames (the movie, mw1_movie.h). fade_frame steps the fade once
// this frame and gives its black's alpha (0: no fade); -1 when the change was just made on this frame (the next screen
// is up: draw nothing more). fade_black lays that black over this frame's scene (then video_draw_3d draws it).
float fade_frame(mech3::Engine& en);
void fade_black(mech3::Engine& en, float alpha);

// True when MW1's data has `file` with `dialog` in it (the loop falls back to message boxes when not).
bool layout_there(const char* file, const char* dialog);

}  // namespace mw1::screens

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace screens;
}  // namespace mw1
