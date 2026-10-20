#pragma once
#include <cstdint>
#include <vector>

#include "gamez/ztwod/twod.h"
#include "mech3/app_hooks.h"

// What the game's menu screens share on top of zTwoD (docs/mainmenu.md): the
// screens' base, the scrolling background, the game's push button, its
// rippling subclass and the scrolling title. Original file unknown: the screen
// base and the button sit between mech3\briefing.cpp and mech3\mech.cpp
// (0x0040a110..0x0040a9f0), the rippling button and the title between
// mech3\mech_anim.cpp and mech3\mech_data.cpp (0x0042a140..0x0042fb30) and
// between mech3\mech_waynet.cpp and mech3\mission.cpp (0x00485510..0x00485650).
// Outside zTwoD they call the options and the app through mech3::Engine.
namespace mech3 {

// The background of the menus (vtable 0x005bc1b0), 0x13c bytes: unless
// FastMenu, the image scrolls by (step_x, step_y) a frame and wraps, drawn as
// up to four pieces cut at its edges.
class ScrollImage : public ztwod::Image {
 public:
  explicit ScrollImage(Engine& e);   // in the screens' constructor 0x0040a110
  void draw() override;              // +0x04  0x0040a490
  void update(float dt) override;    // +0x24  0x0040a210

  int32_t step_x = 1, step_y = 1;    // +0xbc
  uint8_t piece[4] = {};             // +0xc4  draw this piece next (never set by the constructor)
  int32_t piece_pos[4][2] = {};      // +0xc8  where each piece goes; the first at (0, 0)
  ztwod::Rect view;                  // +0xe8  the image's part at the screen's top left
  ztwod::Rect piece_src[4];          // +0xf8
  uint8_t moving = 0;                // +0x138  not FastMenu

 private:
  Engine& e_;
  float steps_ = 0.0f;               // FIX (B): frames of scrolling owed (not in the original)
};

// The main menu's title (MECH3TEXT, vtable 0x005bd988): unless FastMenu it
// slides left `speed` pixels a frame and comes back in at the screen's right.
class Marquee : public ztwod::Image {
 public:
  explicit Marquee(Engine& e);       // in the main menu's constructor 0x00486260
  void update(float dt) override;    // +0x24  0x0042fb30

  float speed = 2.0f;                // +0xbc

 private:
  Engine& e_;
  float steps_ = 0.0f;               // FIX (B): pixels owed (not in the original)
};

// A menu screen (vtable 0x005bc190): a dialog with the shell state that owns
// it, a still image for FastMenu and the BACKGROUND. The main menu's, the
// options' and the other menus' screens derive from it.
class MenuScreen : public ztwod::Dialog {
 public:
  MenuScreen(Engine& e, Handle state);  // 0x0040a110
  ~MenuScreen() override = default;  // 0x0040a570 (deleting 0x0040a550)
  // +0x10 (0x004a5bc0 here, empty): the option screens' refresh (0) from the
  // options and apply (1) to them (docs/menus.md).
  virtual void apply(int32_t to_options) { (void)to_options; }
  // +0x14 (0x0040a510): a button of the screen was pressed; nothing here.
  virtual void on_button(int32_t id, int32_t a, int32_t b) {
    (void)id, (void)a, (void)b;
  }
  virtual uint8_t set_busy(uint8_t on);  // +0x18  0x0040a520: returns the old value
  virtual uint8_t busy() { return busy_; }  // +0x1c  0x0040a540
  Engine& engine() { return e_; }
  // Mech3.exe 1.2 @ 0x00486ed0: everything drawn once without the cursor and
  // grabbed; the grab becomes the static background and the first object (FastMenu).
  void bake();
  // The shell state's +0x28 (its Escape: most pop themselves), as the BACK
  // buttons call it ([screen+0x10fec]->+0x28).
  void state_escape() { e_.vcall(state, 0x28, {}); }
  // The sub-screens' BACKGROUND (inline in 0x004379f0, 0x00438f60, 0x0042a590,
  // 0x004289a0): the main menu's image ([0x00609854]) and its scrolling, or the
  // dialog's own without a main menu (the credits do not check); still and at
  // the origin when the main menu's is still, and (not the credits) while a
  // mission is running: the menus of the pause screen.
  void borrow_background(bool option_screen);

 private:
  Engine& e_;

 public:
  Handle state = 0;                  // +0x10fec
  uint8_t busy_ = 0;                 // +0x10ff0  the buttons take no mouse while set
  ztwod::Image still{0};             // +0x10ff4  FastMenu: the screen drawn once, as one image
  ScrollImage background;            // +0x110b0  BACKGROUND
};

// The game's push button (vtable 0x005bc230), 0x158 bytes: a zTwoD Control
// with an id it reports to its screen when pressed.
class ShellButton : public ztwod::Control {
 public:
  ShellButton(Engine& e, int32_t id);  // 0x0040a5d0
  ~ShellButton() override;           // 0x0040a600
  ztwod::Rect* get_rect() override;  // +0x2c  0x0040a6b0
  void on_left_up() override;        // +0x30  0x0042a3d0: pressed, then tells the screen
  bool accepts(ztwod::Window* w, bool inside) override;  // +0x54  0x0040a720
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x0040a680
  virtual void notify(int32_t a, int32_t b);  // +0x8c  0x0040a6e0: the screen's +0x14
  // +0x90: 0x0040a9c0 here, a body folded with another class's destructor;
  // every button that is baked overrides it.
  virtual void set_baked(bool on) { (void)on; }
  virtual void init_ripple() {}      // +0x94  0x004bf620 (it pops one argument)
  virtual void restart_ripple() { dirty(); }  // +0x98  0x0040a9f0

  uint8_t reading = 0;               // +0x14c  inside read_control
  int32_t id;                        // +0x150
  uint8_t borrowed = 0;              // +0x154  the images are not its own (cleared before the Control frees them)

 protected:
  Engine& e_;                        // the outside calls (not a field of the original)
};

// A push button with a ripple around it (vtable 0x005bd888), 0x1ec bytes: on
// rollover a ring spreads from its centre while the ripple fades (twod.md,
// Ripple). Baked (FastMenu) it has no image of its own: the screen grab shows it.
class SlideButton : public ShellButton {
 public:
  SlideButton(Engine& e, int32_t id);  // 0x00438d70
  ~SlideButton() override;           // 0x0042a450
  void set_pos(int32_t px, int32_t py) override;  // +0x0c  0x0042a400: and the ripple with it
  void update(float dt) override;    // +0x24  0x00485650
  ztwod::Rect* get_rect() override;  // +0x2c  0x00485590
  void on_enter() override;          // +0x3c  0x00485570
  void on_leave() override;          // +0x40  0x00485640
  int32_t hit(int32_t px, int32_t py) override;  // +0x5c  0x004855f0
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x0042a1d0
  void after_bind() override;        // +0x88  0x0042a250
  void set_baked(bool on) override;  // +0x90  0x0042a160
  void init_ripple() override;       // +0x94  0x00485510
  void restart_ripple() override;    // +0x98  0x00485530

  zimg::Texture* stashed = nullptr;  // +0x158  the normal image while baked
  ztwod::Rect reach;                 // +0x15c  the square RADIUS around the centre (read by nothing here)
  ztwod::Ripple ripple;              // +0x16c
  uint8_t rippling = 0;              // +0x1b8
  float speed = 0.0f;                // +0x1bc  the ring's growth, pixels a second
  float amplitude = 0.0f;            // +0x1c0  7, less 0.3 a frame; it stops at 2
  float wavelength = 0.0f;           // +0x1c4
  float phase = 0.0f;                // +0x1c8
  int32_t radius = 0;                // +0x1cc
  int32_t max_radius = 0;            // +0x1d0  MAXRADIUS (200)
  float reach_radius = 0.0f;         // +0x1d4  RADIUS (20)
  float reach_sq = 0.0f;             // +0x1d8
  std::vector<ztwod::Object2D*> owned;  // +0x1dc  (HP STL) deleted with the button; empty in the main menu

 protected:
  void read_ripple(zrdr::Node* node);  // the shared tail of +0x84: RADIUS, MAXRADIUS
};

// A choice stepped through with two arrows (vtable 0x005c5ba0, "cycle button"
// by its destructor's text), 0x424 bytes: TOGGLE_LEFT and TOGGLE_RIGHT are
// controls of their own, the choice shows in the first LABEL. The option
// screens' DIFFICULTY, CAMERA, ON/OFF switches, RESOLUTION and the rest.
// Original file unknown: 0x0048b280..0x0048c050, between mech3\mech_waynet.cpp
// and mech3\mission.cpp; the helpers 0x00438e10.. and 0x0043a710 sit with the
// option screens.
class CycleButton : public ShellButton {
 public:
  struct Item {                      // 8 bytes, new'd
    char* text;                      // +0x00  _strdup'd
    int32_t value;                   // +0x04
  };
  CycleButton(Engine& e, int32_t id);  // 0x0048b280
  ~CycleButton() override;           // 0x0048b380 (deleting 0x0048b360, 0x004898a0)
  void set_pos(int32_t px, int32_t py) override;  // +0x0c  0x0048b9b0: the label and arrows with it
  void update(float dt) override;    // +0x24  0x0048b550: the arrows' rollover
  void on_left_up() override;        // +0x30  0x0048b6b0
  void on_drag(int32_t, int32_t) override {}  // +0x4c  0x0048b940
  int32_t hit(int32_t px, int32_t py) override;  // +0x5c  0x0048b950
  void show_for(float seconds) override;  // +0x60  0x0048b4e0
  void hide() override;              // +0x64  0x0048b520
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x0048be70
  void after_bind() override;        // +0x88  0x0048bf60; each kind adds its choices
  void notify(int32_t a, int32_t b) override;  // +0x8c  0x0042fbc0: the owner's +0x14 with the cycle id
  // +0x90 (0x0048b490): enables (not bakes) the button and both arrows.
  void set_baked(bool on) override;
  void set_enabled(int32_t on);      // the same slot with its int argument
  void set_arrows(uint8_t prev_on, uint8_t click_on, uint8_t next_on);  // +0x94  0x0048bd40
  void set_arrows(uint8_t prev_on, uint8_t next_on);  // +0x98  0x0048bcf0
  virtual void set_label_font(Handle font);  // +0x9c  0x0048ba80
  virtual void select(int32_t i);    // +0xa0  0x0048b850: shown in the first label
  virtual void step(int32_t by);     // +0xa4  0x0048b790: wraps or stops at the ends

  int32_t add_item(const char* text, int32_t value);  // 0x0048bac0: the old count
  void clear_items();                // 0x0048bda0
  Item* item(int32_t i);             // 0x00438e10: null out of range
  int32_t value_at(int32_t i);       // 0x00438e50: 0 out of range
  int32_t select_value(int32_t v);   // 0x0048b8f0: the first with v, or -1
  void select_each_value(int32_t v);  // 0x0043a710: every one with v (the last stays)
  int32_t count() const { return static_cast<int32_t>(items.size()); }  // 0x004fd1c0
  int32_t selected_value() { return item(index) ? item(index)->value : 0; }  // the screens' inline read

  std::vector<Item*> items;          // +0x15c  (+0x158 its allocator byte)
  uint8_t click_steps = 0;           // +0x168  a press off the arrows steps forward too
  int32_t index = -1;                // +0x16c
  uint8_t wraps = 1;                 // +0x170
  int32_t hover = 0;                 // +0x174  1 over the right arrow, 2 the left
  int32_t prev_dx = 0, prev_dy = 0;  // +0x178  TOGGLE_LEFT's place from the button
  int32_t next_dx = 0, next_dy = 0;  // +0x180  TOGGLE_RIGHT's
  ztwod::Control prev;               // +0x188  TOGGLE_LEFT
  ztwod::Control next;               // +0x2d4  TOGGLE_RIGHT
  int32_t cycle_id;                  // +0x420  reported with a press
};

// A cycle whose one choice (alert) shows in a second font (0x00438090, vtables
// 0x005bf6e8, 0x005bf3b0, 0x005bf308, 0x005bf260), 0x430 bytes: OFF / ON with
// fonts.zrd's ia_small_green and, for `alert`, ia_small_red.
class AlertCycle : public CycleButton {
 public:
  AlertCycle(Engine& e, int32_t alert_index) : CycleButton(e, -1), alert(alert_index) {}
  void after_bind() override;        // +0x88  0x00437fe0
  void select(int32_t i) override;   // +0xa0  0x00438040

  Handle font_normal = 0;            // +0x424
  Handle font_alert = 0;             // +0x428
  int32_t alert;                     // +0x42c
};

// The ON / OFF cycle (+0x88 0x00438ed0): OFF 0, ON 1.
class OnOffCycle : public CycleButton {
 public:
  explicit OnOffCycle(Engine& e) : CycleButton(e, -1) {}
  void after_bind() override;        // 0x00438ed0
};

// The LOW / MEDIUM / HIGH cycle (+0x88 0x00438f10): values 0, 1, 2.
class LevelCycle : public CycleButton {
 public:
  explicit LevelCycle(Engine& e) : CycleButton(e, -1) {}
  void after_bind() override;        // 0x00438f10
};

}  // namespace mech3
