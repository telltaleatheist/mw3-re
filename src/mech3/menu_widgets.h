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
  // +0x14 (0x0040a510): a button of the screen was pressed; nothing here.
  virtual void on_button(int32_t id, int32_t a, int32_t b) {
    (void)id, (void)a, (void)b;
  }
  virtual uint8_t set_busy(uint8_t on);  // +0x18  0x0040a520: returns the old value
  virtual uint8_t busy() { return busy_; }  // +0x1c  0x0040a540
  Engine& engine() { return e_; }

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

}  // namespace mech3
