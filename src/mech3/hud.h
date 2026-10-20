#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zweapon/zwep_init.h"
#include "gamez/ztwod/twod.h"

// The HUD (docs/hud.md). File unknown: the code between mech3\mech_data.cpp
// and mech3\mech_map.cpp (0x0043a7c0..0x00448880); named after what it is.
// One HUD per mech type (Pilot +0x224), a zTwoD Layer holding its pieces;
// each piece is built here on the ported zTwoD classes. The comments give
// each member's offset in the original's 0x22e40-byte object.
namespace mech3 {

struct Mech;
class Hud;

// One part's damage silhouettes: blue, green, yellow, red, black, and where
// the part sits in the silhouette (a 0x20-byte entry of [0x00605888]).
struct DamageImages {
  zimg::Texture* image[5] = {};
  int32_t x = 0, y = 0;
  void set(const char* const names[5], int32_t px, int32_t py);  // 0x0043ae50
};
// The silhouettes every HUD shares ([0x00605888..0x006059a8], loaded once by
// 0x0043afc0 and the constructor): by the table's order, rtorso, lleg, rarm,
// ctorso, ltorso, head, larm, rleg; three backgrounds and their size.
struct DamageSet {
  bool loaded = false;               // 0x00605990
  DamageImages parts[8];             // 0x00605888
  int32_t bg_w = 0, bg_h = 0;        // 0x00605998, 0x0060599c
  zimg::Texture* bg[3] = {};         // 0x006059a0 blue, red, green
};
DamageSet& damage_set();
void load_damage_set();              // 0x0043afc0

// The compass (0x0043f3e0, vtable 0x005c02e0, 0x678 bytes): a tape scrolled
// under a frame by the heading.
class HudCompass : public ztwod::Object2D {
 public:
  HudCompass();
  void set_heading(float radians);   // 0x0043f590
  void update(float dt) override;    // +0x24  0x0043f620
  float scale = 206.90143f;          // +0x34  pixels a radian
  int32_t half = 0;                  // +0x38  half the frame's width
  int32_t centre = 0xb8;             // +0x3c  the tape's x at north
  ztwod::Rect src{};                 // +0x40  the tape's part shown
  ztwod::Image tape;                 // +0x50  hud_compass at (245, 19)
  ztwod::Image frame;                // +0x10c hud_compass_frame at (245, 11)
  void read_layout(const Hud* hud);  // 0x0043f500 (COMPASS)
  ztwod::FontText text;              // +0x1c8
};

// A text in the font mech_string (vtable 0x005bc598, inlined constructor:
// FontText's with its own destructor 0x0048c050).
class HudText : public ztwod::FontText {
 public:
  HudText();
};

// The panels' green (0, 64, 0) and opacity: 0.6 in Direct3D, opaque in software.
uint16_t hud_green();                // 0x0043b170
float hud_opacity();                 // 0x0043a7c0

// A see-through panel behind a piece (vtable 0x005c0158, 0x108 bytes): an
// image of one colour and alpha made to its size (0x0043ab10), with the
// corners Direct3D draws it by.
class HudPanel : public ztwod::Image {
 public:
  HudPanel();                        // 0x0043a7e0
  void set_pos(int32_t px, int32_t py) override;  // +0x0c  0x0043a8f0
  void set_color(uint16_t c);        // 0x0043aaa0
  void set_alpha(uint8_t a);         // 0x0043a9d0: 0xfe and up opaque
  void update(float dt) override;    // +0x24  0x0043ad00: here only when opaque or in software
  void draw3d(float dt) override;    // +0x28  0x0043ad20: its quad in a scene of its own
  void draw_quad();                  // 0x0043ad70: Direct3D's see-through quad, else the image
  void set_opacity(float f);         // 0x0043aa60
  void set_size(int32_t w, int32_t h);  // 0x0043ab10: its image made or resized
  ztwod::Rect rect{};                // +0xbc
  float corners[4][3] = {};          // +0xcc  (x, y, 0.9): top left, bottom left, bottom right, top right
  uint16_t color = 0;                // +0xfc
  uint8_t alpha = 0;                 // +0xfe
  uint16_t* colors = nullptr;        // +0x100 the image's pixels
  uint8_t* alphas = nullptr;         // +0x104 its alpha
};

// The objectives (vtable 0x005c0918, 0xaac bytes): a text box of the
// objectives over a panel, and one of the multiplayer scores.
class HudObjectives : public ztwod::TextBox {
 public:
  HudObjectives();                   // 0x00446700
  void set_pos(int32_t px, int32_t py) override;  // +0x0c  0x00446870: the scores 260 to the right
  void update(float dt) override;    // +0x24  0x00447410
  void show_for(float seconds) override;  // +0x60  0x00446820 (part: 0x00446ad0 not ported)
  void hide() override;              // +0x64  0x00446850
  HudPanel panel;                    // +0x4c8
  ztwod::TextBox scores;             // +0x5d4
  ztwod::Handle font_title = 0;             // +0xa9c rtobjective_title
  ztwod::Handle font_complete = 0;          // +0xaa0 rtobjective_complete
  ztwod::Handle font_incomplete = 0;        // +0xaa4 rtobjective_incomplete
  ztwod::Handle font_scores = 0;            // +0xaa8 rt_mp_scores
};

// The equipment list's fonts and lights, shared ([0x006059d0..0x006059ec],
// loaded by the first entry made).
struct EquipmentSet {
  zimg::Texture* light[3] = {};      // 0x006059d0 red, yellow, green
  ztwod::Handle font_red = 0;               // 0x006059e0
  ztwod::Handle font_yellow = 0;            // 0x006059e4
  ztwod::Handle font_green = 0;             // 0x006059e8
  ztwod::Handle font_grey = 0;              // 0x006059ec
};
EquipmentSet& equipment_set();

// One line of the equipment list (vtable 0x005c0ae0, 0xa40 bytes): its name,
// a value right-aligned at the column and a light.
class HudEquipLine : public ztwod::Text {
 public:
  HudEquipLine();                    // 0x00447c40
  void place(int32_t px, int32_t py, int32_t column);  // 0x00447fe0
  void reset();                      // 0x00447e10
  void update(float dt) override;    // +0x24  0x00448030
  HudText value;                     // +0x4cc
  ztwod::Image light;                // +0x97c
  int32_t state = 0;                 // +0xa38
  int32_t item = 0;                  // +0xa3c
};

// The equipment list (vtable 0x005c0b90, 0xa548 bytes): sixteen lines over a panel.
class HudEquipment : public ztwod::Object2D {
 public:
  HudEquipment();                    // 0x00448070
  void place(int32_t px, int32_t py, int32_t dy);  // 0x00448140
  void update(float dt) override;    // +0x24  0x00448450: the lines up to the first hidden
  void read_layout(Hud* hud);        // 0x00448200 (EQUIPMENT)
  void* hud = nullptr;               // +0x34 the HUD it shows
  HudEquipLine lines[16];            // +0x38
  HudPanel panel;                    // +0xa438
  uint8_t below = 1;                 // +0xa540 under the point; else left of it by the column
  int32_t column = 0x6e;             // +0xa544 the values' right edge
};

// The weapon list's lights and fonts, shared ([0x006059b0..0x006059cc],
// loaded by the first line made).
struct WeaponSet {
  zimg::Texture* light[3] = {};      // 0x006059b0 green, yellow, red
  ztwod::Handle font_red = 0;        // 0x006059c0
  ztwod::Handle font_green = 0;      // 0x006059c4
  ztwod::Handle font_yellow = 0;     // 0x006059c8
  ztwod::Handle font_grey = 0;       // 0x006059cc
};
WeaponSet& weapon_set();

// One line of the weapon list (vtable 0x005c0808, 0x1120 bytes): a slider
// (the recycle bar) with the weapon's name, two texts, its light and a mark.
class HudWeaponLine : public ztwod::Slider {
 public:
  HudWeaponLine();                   // 0x00445760
  void place(int32_t px, int32_t py, int32_t column);  // 0x00445ba0
  void reset();                      // 0x00445a30
  bool set_look(bool red, bool grey);  // 0x00445a60: the texts' fonts; false when unchanged
  void set_weapon(void* w);          // 0x00445d10 (part: with a weapon, not ported yet)
  void update(float dt) override;    // +0x24  0x00445c10: the bar's images from the texts when changed
  HudText name;                      // +0x188
  HudText text2;                     // +0x638
  HudText count_text;                // +0xae8 "%3d", right-aligned at the column
  ztwod::Image light;                // +0xf98
  ztwod::Image mark;                 // +0x1054 left of the line
  int32_t count = 0;                 // +0x1110 the number shown
  uint8_t red = 1;                   // +0x1118
  uint8_t grey = 0;                  // +0x1119 (not set by the constructor)
  uint8_t changed = 0;               // +0x111a
  uint8_t x111b = 0;                 // +0x111b
  void* weapon = nullptr;            // +0x111c
};

// The weapon list (vtable 0x005c0898, 0x11444 bytes): sixteen lines over a panel.
class HudWeapons : public ztwod::Object2D {
 public:
  HudWeapons();                      // 0x00445f80
  void set_pos(int32_t px, int32_t py) override;  // +0x0c  0x004460c0: the lines 10 apart
  void update(float dt) override;    // +0x24  0x00446680
  void show_for(float seconds) override;  // +0x60  0x00446200 (part: 0x00446520 not ported)
  void hide() override;              // +0x64  0x00446220
  void read_layout(Hud* hud);        // 0x00446120 (WEAPONS)
  void* hud = nullptr;               // +0x34 the HUD it shows
  uint32_t weapons = 0;              // +0x38 the mech's weapon manager
  ztwod::PolyLine poly;              // +0x3c
  zimg::Texture* select = nullptr;   // +0x128 hud_weapon_select
  zimg::Texture* chain = nullptr;    // +0x12c hud_weapon_chain
  HudWeaponLine lines[16];           // +0x130
  HudPanel panel;                    // +0x11330
  uint8_t shown = 1;                 // +0x11438
  int32_t x1143c = 0;                // +0x1143c
  int32_t column = 0x6e;             // +0x11440
};

// A gauge's bar (vtable 0x005c0788, 0x140 bytes; 0x00448530 builds one too):
// a four-point polygon.
class HudBar : public ztwod::Polygon {
 public:
  void set_box(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x00448550: w + 1 wide
  int32_t height = 0;                // +0x138
  int32_t width = 0;                 // +0x13c
};

// The gauges (vtable 0x005c0708, 0x168c bytes): speed, coolant and fuel
// frames with their bars, the heat bar and meter, and three texts.
class HudGauges : public ztwod::Object2D {
 public:
  HudGauges();                       // 0x00444760
  void update(float dt) override;    // +0x24  0x004452c0
  ztwod::FontText coolant_text;      // +0x38   message 0x1d5
  ztwod::FontText heat_text;         // +0x4e8  message 0x1d6 (hud.zrd heat_text)
  HudText speed_text;                // +0x998
  HudBar speed_bar;                  // +0xe48
  HudBar fuel_bar;                   // +0xf88
  HudBar coolant_bar;                // +0x10c8
  ztwod::Image speed_frame;          // +0x1208 hud_speed_frame at (575, 256)
  ztwod::Image coolant_frame;        // +0x12c4 hud_coolant_frame at (8, 290)
  ztwod::Image fuel_frame;           // +0x1380 hud_fuel_frame at (527, 296)
  int32_t x143c = 0;                 // +0x143c
  int32_t x1440 = 0, x1444 = 0;      // +0x1440, +0x1444
  int32_t heat_w = 0, heat_h = 0;    // +0x1448, +0x144c the heat bar's size
  ztwod::Image heat_bar;             // +0x1450 hud_heat_bar at (18, 163)
  int32_t heat_bottom = 0;           // +0x150c the heat bar's bottom
  ztwod::Image heat_meter;           // +0x1510 hud_heat_meter at (16, 163)
  ztwod::Image toggler;              // +0x15cc lab_toggler_green at (10, 150)
  int32_t toggler_y = 0;             // +0x1688 the meter's bottom less half the toggler
  void read_layout(const Hud* hud);  // 0x00444dc0 (GAUGES)
};

// A light of n images, one shown (vtable 0x005c0260, 0xd0 bytes; its
// constructor inlined).
class HudLight : public ztwod::Image {
 public:
  explicit HudLight(int32_t n);
  ~HudLight() override;
  void select(int32_t i);            // inlined: the image of light i
  int32_t count;                     // +0xc0
  zimg::Texture** images;            // +0xbc calloc'd
  int32_t current = -1;              // +0xc4
  float interval = 0.0f;             // +0xc8 seconds an image, 0: the current one stays
  float time = 0.0f;                 // +0xcc
  void update(float dt) override;    // +0x24  0x004486e0
};

// The radar (vtable 0x005c01e0, 0xdc0 bytes): a filled box, the view cone's
// lines, the range texts, the light and two polylines. Two sizes: small
// (55, 55, radius 50) and zoomed (320, 240, radius 180), moved to in steps.
class HudRadar : public ztwod::FilledBox {
 public:
  explicit HudRadar(float fov_degrees);  // 0x0043d260
  void show_for(float seconds) override;  // +0x60  0x0043db70: grows back
  void hide() override;              // +0x64  0x0043dc00: shrinks away, stays shown
  void attach(void* hud);            // 0x0043d6d0: the zoomed size from the display
  void set_amber(bool on);           // 0x0043d630: amber instead of green
  void set_fade(float f);            // 0x0043dc70
  void set_zoom(bool zoomed);        // 0x0043da50
  void select_sound();               // 0x0043da00 (part: 0x00458590 not ported)
  void set_fov(float radians);       // 0x0043f0c0
  void set_dir(const float* v);      // 0x0043f130: the cone's edges from the view direction
  void update(float dt) override;    // +0x24  0x0043dd00: hooked (radar_update)
  uint16_t x58 = 0;                  // +0x58
  uint16_t zoomed = 0;               // +0x5a
  uint8_t active = 0;                // +0x5c
  uint8_t changed = 0;               // +0x5d
  int32_t cx[2] = {}, cy[2] = {}, r[2] = {};  // +0x60, +0x68, +0x70 by zoom
  int32_t to_r = 0, to_x = 0, to_y = 0;      // +0x78 the size moved to
  int32_t r_steps = 0, x_steps = 0, y_steps = 0;  // +0x84
  float dir[3] = {};                 // +0x90
  float half_fov = 0.0f;             // +0xa8
  float cos_half = 0.0f, sin_half = 0.0f;  // +0xac, +0xb0
  ztwod::ClipLine2D lines[3];        // +0xb4 the cone's edges (green) and a red one
  HudText text1;                     // +0x1a4
  HudText text2;                     // +0x654
  HudLight light{3};                 // +0xb04 green, yellow, red
  uint32_t xbd4 = 0, xbd8 = 0;       // +0xbd4
  ztwod::PolyLine polys[2];          // +0xbdc
  uint32_t xdb4 = 0, xdb8 = 0;       // +0xdb4
  void* source = nullptr;            // +0xdbc the HUD it shows
};

// zView's window (0x004b5440, vtable 0x005c91a4), 0x3c bytes: a window node
// and its area on screen.
struct HudViewWindow {
  explicit HudViewWindow(const char* name);  // 0x004b5440
  void set_area(const ztwod::Rect& r);  // inlined in 0x0043f660: the area and its target
  uint32_t node = 0;                 // +0x04
  ztwod::Rect area{};                // +0x08
  ztwod::Rect target{};              // +0x18
  uint8_t scaled = 0;                // +0x28 area and target differ
  float sx = 0.0f, sy = 0.0f, isx = 0.0f, isy = 0.0f;  // +0x2c their ratio (unset when equal)
};

// A zView (0x004b5150, vtable 0x005c9118), 0x18 bytes: a camera node in a
// window, in the world.
struct HudZView {
  HudZView(const char* name, HudViewWindow* w, uint32_t world);  // 0x004b5150
  uint32_t camera = 0;               // +0x04
  HudViewWindow* window = nullptr;   // +0x08 (0x004b51b0)
  uint32_t world = 0;                // +0x0c (0x004b51d0)
  uint32_t x10 = 0;                  // +0x10
  int32_t hardware = 0;              // +0x14 [0x0072f348]
};

// A 3D view in a HUD box (vtable 0x005c0368, 0x16c bytes): the MFD's mech,
// TARGET's and the reticle's zoom. Partial: built; its drawing and camera
// work not ported (the render work, hud.md).
class HudZoomView : public ztwod::Object2D {
 public:
  HudZoomView(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x0043f660
  void show_for(float seconds) override;  // +0x60  0x0043fc10: the camera on too
  void hide() override;              // +0x64  0x0043fc60: the camera off too
  HudZView view;                     // +0x34 (its vtable 0x005c0360)
  uint32_t x4c = 0xffff;             // +0x4c
  ztwod::Rect rect{};                // +0x50
  HudPanel panel;                    // +0x60
  void* source = nullptr;            // +0x168 the HUD it shows
  void set_source(void* s) { if (s) source = s; }  // 0x0043fa20
  void place(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x0043f890
};

// The MFD's 3D view (0x00442620, vtable 0x005c0500, 0x258 bytes): the zoom
// view on a panel of its size, boxed.
class HudMfdView : public HudZoomView {
 public:
  HudMfdView(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x00442620
  void place(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x004427b0
  void update(float dt) override;    // +0x24  0x00442880: the box (the 3D view draws in the scene)
  void draw3d(float dt) override;    // +0x28  0x00442870: its zView's render (hooked)
  ztwod::PolyLine box;               // +0x16c (vtable 0x005c0580)
};

// One part's damage image in a damage view (vtable 0x005c09e0, 0xdc bytes).
class HudDamagePart : public ztwod::FlashImage {
 public:
  HudDamagePart(DamageImages* images, int32_t px, int32_t py);  // 0x00447460
  DamageImages* images = nullptr;    // +0xd8
};

// A mech's damage silhouette (vtable 0x005c0a60, 0x7e4 bytes): the background
// and the eight parts, in one of two orders (the player's own, or a target's
// seen from the front).
class HudDamageView : public ztwod::Object2D {
 public:
  // 0x00447680 (own; a third argument unused) and 0x00447820 (target).
  HudDamageView(int32_t px, int32_t py, bool target);
  void update(float dt) override;    // +0x24  0x00447b50
  ztwod::Image bg;                   // +0x34
  HudDamagePart parts[8];            // +0xf0
  void* mech = nullptr;              // +0x7d0 (not set by the constructor)
  ztwod::Rect rect{};                // +0x7d4 the background's
  bool set_mech(void* m);            // 0x00447c00: shown with one
  void move(int32_t px, int32_t py);  // 0x004479a0
};

// An MFD (vtable 0x005c0470, 0xcd8 bytes): the damage view, the 3D view, the
// nav list and a title. The MFD itself (0x00442060) and TARGET's base
// (0x0043fcb0 through 0x00441da0) differ in the damage view and the nav
// list's sizes.
class HudMfd : public ztwod::Object2D {
 public:
  HudMfd(int32_t px, int32_t py, int32_t w, int32_t h, bool target);  // 0x00442060 / 0x00441da0
  void place(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x00442230 (MFD in hud.zrd)
  void attach(void* hud);            // 0x004423c0
  void update(float dt) override;    // +0x24  0x00442540: by the mode, then the title
  virtual int32_t shown_mode() { return mode; }  // +0x80  0x004487d0: 1 the 3D view, 2 the damage, 3 the nav list
  void update_mode(float dt);        // the shared middle of 0x00442540 and 0x00441490
  // 0x004425d0: the shown piece's rect by the mode (asked again): 1 the 3D
  // view's, 2 the damage view's (0x00447bf0), 3 the nav list's (+0x1c84 of
  // it, hooked); else none.
  const ztwod::Rect* shown_rect();
  void draw3d(float dt) override;    // +0x28  0x00442500: mode 1 the view's, 2 the damage view's (empty)
  void* hud = nullptr;               // +0x34
  int32_t mode = 0;                  // +0x38
  HudDamageView damage;              // +0x3c
  uint32_t nav = 0;                  // +0x820 the nav list (hooked)
  HudMfdView* view = nullptr;        // +0x824
  HudText title;                     // +0x828
};

// TARGET (vtable 0x005c03e8, 0x2414 bytes): an MFD for the target, with its
// arrows, markers and three texts above it.
class HudTarget : public HudMfd {
 public:
  HudTarget(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x0043fcb0
  void hide() override;              // +0x64  0x004415f0
  // +0x60 show_for 0x00441560: not ported yet.
  virtual void set_mode(int32_t m);  // +0x7c  0x004400f0: kept for the next TARGET
  void set_target(void* t);          // 0x00441800 (part: with a target, not ported yet)
  void set_shown(bool on);           // 0x00441780 (part, as set_target)
  void update_texts(bool clear);     // 0x00441660 (part, as set_target)
  void place(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x00440110 (TARGET in hud.zrd)
  void attach(void* hud);            // 0x00441760
  void update(float dt) override;    // +0x24  0x00441490
  int32_t shown_mode() override;     // +0x80  0x004413a0: none without a live target
  void draw3d(float dt) override;    // +0x28  0x00441410: mode 1 the target through the view
  ztwod::Shape shape1;               // +0xcd8
  ztwod::Line2D lines[3];            // +0xdfc (0x004400e0: all zero)
  ztwod::Shape shape2;               // +0xee0
  ztwod::Frame frame;                // +0x1004
  zimg::Texture* arrows[12] = {};    // +0x1058 enemy, ally, other: left, right, up, down
  ztwod::Image marker{0};            // +0x1088
  HudText texts[4];                  // +0x1144
  void* target = nullptr;            // +0x2404
  uint8_t shown = 0;                 // +0x2408
  float x240c = 0.0f;                // +0x240c
  int32_t line_h = 0;                // +0x2410
};

// The reticle's zoom window (0x00442ac0, vtable 0x005c0608): the zoom
// camera's view in a box or circle, its light and text.
class HudZoomWindow : public HudZoomView {
 public:
  HudZoomWindow(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x00442ac0
  // +0x60 show_for 0x004438d0: not ported yet.
  void hide() override;              // +0x64  0x00443880
  void update(float dt) override;    // +0x24  0x00442e30: the scale grows (shown) or shrinks
  void set_rect(int32_t px, int32_t py, int32_t w, int32_t h);  // 0x004438f0: scaled by the zoom, kept on screen
  void set_view_rect(int32_t* px, int32_t* py, int32_t* w, int32_t* h);  // 0x0043fa40: 2 pixels inside the display
  void follow(int32_t px, int32_t py);  // 0x00443660: centred on the reticle, when shown or scaled
  uint8_t round = 0;                 // +0x16c a circle, not a box
  float zoom = 0.0f;                 // +0x170 the window's scale
  uint8_t x174 = 0;                  // +0x174
  int32_t base_w = 0, base_h = 0;    // +0x178
  HudText text;                      // +0x180
  HudLight light{4};                 // +0x630 hud_zoomoff, on, over, cant
  ztwod::Frame frame;                // +0x700 the circle
  ztwod::PolyLine box;               // +0x754 (vtable 0x005c0580)
};

// The reticle (vtable 0x005c0688, 0x95c bytes): its images by state and the
// zoom window.
class HudReticle : public ztwod::Image {
 public:
  HudReticle(const char* on, const char* off);  // 0x00443b60
  // +0x60 show_for 0x00443cf0: Object2D's.
  void hide() override;              // +0x64  0x00443d00: the zoom window too
  void attach(void* hud);            // 0x00444250
  void reset();                      // 0x00443cc0
  void update(float dt) override;    // +0x24  0x004441a0: the zoom, then itself while the zoom is away
  // 0x004445f0: at the display's centre plus (fx, fy) of its half size, the
  // aim worked out, the zoom window following.
  void place(float fx, float fy);
  void aim();                        // 0x00444350: the ray through the reticle into the world
  void set_hit(const zwep::HitRecord* h, float d);  // 0x00444710
  void set_miss(const float* far_point, float far_z);  // 0x004446a0
  HudZoomWindow zoom{10, 0x172, 0x5a, 0x5a};  // +0xbc
  float aim_point[3] = {};           // +0x8fc the world point the weapons aim at (0x0041cd40)
  int32_t state = 0;                 // +0x908
  uint8_t locked = 0;                // +0x90c the image kept
  zimg::Texture* img_off = nullptr;  // +0x910
  zimg::Texture* img_on = nullptr;   // +0x914
  zimg::Texture* img_over = nullptr; // +0x918 ret_over
  zimg::Texture* img_cant = nullptr; // +0x91c ret_cant
  zimg::Texture* img_x920 = nullptr; // +0x920 ret_on again
  float aim_distance = 0.0f;         // +0x924 to the aim point from the near plane (0x00444690)
  zwep::HitRecord hit{};             // +0x928 what the aim hit (its node 0: nothing)
  int32_t lock = 0;                  // +0x950 the missile lock: 0 none, 1 acquiring, 2 waiting, 3 locked, 4 lost
  float lock_timer = 0.0f;           // +0x954 0.6 after a reset ([0x005e3a0c]); counts down in update
  void aim_state(bool over);         // 0x00443d20: the lock and the reticle's image
  void* source = nullptr;            // +0x958 (not set by the constructor)
};

// What the HUD calls outside it.
class HudHooks : public zgame::Unported {
 public:
  // zRender's nodes for the zoom views (not ported: the render work).
  // The nav list in an MFD (0x0047e840 on a new 0x2fb8-byte object; not
  // ported yet, hud.md): its handle.
  virtual uint32_t nav_list_new(int32_t x, int32_t y, int32_t w, int32_t a, int32_t b) {
    return unported("0x0047e840", {x, y, w, a, b});
  }
  // The pilot's game when it is multiplayer (0x00479480 on [0x0060861c]).
  virtual uint32_t multiplayer_game() { return unported("0x00479480", {}); }
  virtual const char* message(int32_t id);  // 0x00576040, Mech3Msg.dll's text
  // The MFD's and TARGET's last modes ([0x00605988], [0x0060598c]; 0 at first).
  virtual int32_t saved_mfd_mode() { return static_cast<int32_t>(unported("peek32", {0x605988})); }
  virtual void save_target_mode(int32_t m) { unported("poke32", {0x60598c, m}); }
  virtual int32_t saved_target_mode() { return static_cast<int32_t>(unported("peek32", {0x60598c})); }
  virtual uint32_t window_node_new() { return unported("0x004f9e90", {}); }
  virtual uint32_t camera_node_new() { return unported("0x004f32e0", {}); }
  virtual void node_set_name(uint32_t node, const char* name) { unported("0x004eb300", {node, name}); }
  virtual void camera_set_flag8(uint32_t cam, bool on) { unported("0x004f34c0", {cam, on ? 1u : 0u}); }
  virtual void node_set_active(uint32_t node, bool on) { unported("0x004eb170", {node, on ? 1u : 0u}); }
  virtual void window_set_size(uint32_t node, int32_t w, int32_t h) { unported("0x004f9fa0", {node, w, h}); }
  virtual void window_set_pos(uint32_t node, int32_t x, int32_t y) { unported("0x004fa0b0", {node, x, y}); }
  virtual void camera_set_window(uint32_t cam, uint32_t window) { unported("0x004f3620", {cam, window}); }
  virtual void camera_set_world(uint32_t cam, uint32_t world) { unported("0x004f3560", {cam, world}); }
  virtual void camera_set_zone(uint32_t cam, uint32_t zone) { unported("0x004f6c80", {cam, zone}); }
  virtual void camera_set_zoom(uint32_t cam, float z) { unported("0x004f3ff0", {cam, zgame::bits(z)}); }
  virtual void camera_set_fov(uint32_t cam, float x, float y) {
    unported("0x004f3d90", {cam, zgame::bits(x), zgame::bits(y)});
  }
  virtual void camera_set_clip(uint32_t cam, float near_z, float far_z) {
    unported("0x004f3a70", {cam, zgame::bits(near_z), zgame::bits(far_z)});
  }
  // The player view's camera clip (0x004f3b00 on [0x005f4c8c] +4).
  virtual void player_camera_clip(float* near_z, float* far_z) {
    *near_z = zgame::from_bits(unported("0x004f3b00.near", {}));
    *far_z = zgame::from_bits(unported("0x004f3b00.far", {}));
  }
  // The options' display and window rects (0x0040d750, 0x0040d7b0): left, top, right, bottom.
  virtual ztwod::Rect display_rect();
  virtual ztwod::Rect window_rect();
  // The aim ray (0x00444350): the player view prepared for a projection
  // (0x004b51f0 on [0x005f4c8c]), a screen point (x, y, 1/z) into the world
  // (0x00523930), a node left out of queries (0x004eb6d0), the world's
  // segment query (0x004eed90 on [0x00608c84] +4; nonzero: failed), a
  // material's game soil ([0x00608c00]), the mech's node (+4).
  virtual void view_prepare() { unported("0x004b51f0", {}); }
  virtual void unproject_world(const float screen[3], float out[3]);
  virtual void node_set_intersect(uint32_t node, bool on) { unported("0x004eb6d0", {node, on ? 1u : 0u}); }
  virtual int32_t segment_list(uint32_t world, const float a[3], const float b[3], zwep::HitList* out);
  virtual uint32_t material_soil(zwep::Handle material);
  virtual uint32_t mech_node(Mech* m) { return unported("peek32", {id(m) + 4}); }
  // The world ([0x00608c84]) and its node (+4).
  virtual uint32_t world() { return unported("peek32", {0x608c84}); }
  virtual uint32_t world_node(uint32_t w) { return unported("peek32", {w + 4}); }
  virtual uint32_t id(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  // The mission's current world entry (0x00491c80 on [0x006098d0]) and its
  // +0xd0 (above 0: the map is flipped, the compass turned half round).
  virtual uint32_t mission_entry() { return unported("0x00491c80", {}); }
  virtual float entry_flip(uint32_t entry) { return zgame::from_bits(unported("@entry+0xd0", {entry})); }
  // 0x0043da50's sound for the zoom on the source HUD's mech (0x00432990).
  virtual void radar_zoom_sound(void* source, bool zoomed) {
    unported("0x0043da50.sound", {id(source), zoomed ? 1u : 0u});
  }
  // The set-up's outside: the video mode (**[0x005f4d0c], 0x0040d9d0), the
  // display's size (0x0040d760 / 0x0040d770), the player camera's field of
  // view (its data +0xe8), hud.zrd (0x00556740).
  virtual int32_t vmode() { return static_cast<int32_t>(unported("peek32", {unported("peek32", {0x5f4d0c})})); }
  virtual int32_t display_w();
  virtual int32_t display_h();
  virtual float player_camera_fov() { return zgame::from_bits(unported("0x004f3b00.fov", {})); }
  virtual zrdr::Node* load_hud_zrd() { return zrdr::load("hud.zrd"); }
  // The mech's (not ported here): vt+0xa0 (dead), 0x0041df10 (its
  // definition's kind), +0xbc bit 2, the weapon manager +0xd4 and its count
  // +0x24, a part's equipment count +0x3ac, the target slot +0x6c.
  virtual bool mech_dead(Mech* m) { return unported("mech.vt+0xa0", {id(m)}) != 0; }
  virtual int32_t mech_kind(Mech* m) { return static_cast<int32_t>(unported("0x0041df10", {id(m)})); }
  virtual bool mech_flag_bc4(Mech* m) { return (unported("peek8", {id(m) + 0xbc}) >> 2 & 1) != 0; }
  virtual uint32_t mech_weapons(Mech* m) { return unported("peek32", {id(m) + 0xd4}); }
  virtual int32_t weapons_count(uint32_t w) { return static_cast<int32_t>(unported("peek32", {w + 0x24})); }
  virtual int32_t mech_part_count(Mech* m, int32_t i) {
    return static_cast<int32_t>(unported("peek32", {id(m) + 0x3ac + 4 * i}));
  }
  virtual void* mech_target_slot(Mech*) { return nullptr; }  // wired with the mech (item 10)
  virtual bool target_valid(void* t) { return t && unported("peek32", {id(t)}) != 0; }
  // A target slot's object (0x004e6fe0 on what the slot holds: its +4's
  // vt+0x2c), that object's vt+0xa8 (true: TARGET shows nothing) and its
  // mech (vt+0x50, ai.md).
  virtual void* target_object(void* t) {
    return reinterpret_cast<void*>(uintptr_t(unported("0x004e6fe0", {unported("peek32", {id(t)})})));
  }
  virtual bool object_a8(void* o) { return (unported("obj.vt+0xa8", {id(o)}) & 0xff) != 0; }
  virtual bool object_mech(void* o) { return unported("obj.vt+0x50", {id(o)}) != 0; }
  // The HUD's sound channels (0x0041e7a0: a channel's sound stopped; with
  // `fade`, faded instead).
  virtual void hud_sound_stop(int32_t channel, int32_t fade) { unported("0x0041e7a0", {channel, fade}); }
  // The nav list: given the mech (0x0047eba0), placed (0x0047eb60).
  virtual void nav_list_attach(uint32_t nav, Mech* m) { unported("0x0047eba0", {nav, id(m)}); }
  virtual void nav_list_place(uint32_t nav, int32_t x, int32_t y, int32_t h, int32_t a, int32_t b) {
    unported("0x0047eb60", {nav, x, y, h, a, b});
  }
  // The HUD frame's pieces not ported yet: the debug overlays (0x0043c820's
  // first part: the mech's numbers into five debug texts, while the debug
  // text 0x005fc378 shows, toggled by 0x0040dc34 and [0x005f4d6c]; its flags
  // are [0x005fc384]), the radar's update
  // (0x0043dd00), whether the reticle's point is over TARGET's shape or
  // circle while a live target is in range (0x00440a30), the reticle's aim
  // state from it (0x00443d20: ranges, lock sounds, its image), the weapon
  // lines' looks (0x00446540), the nav list's update (its vt+0x24), the
  // network's (0x00479ed0 while [0x00609980]).
  virtual void hud_debug(void* hud) {
    if (!(unported("peek32", {0x5fc384}) >> 4 & 1)) unported("0x0043c820.debug", {id(hud)});
  }
  virtual void radar_update(void* radar, float dt) { unported("0x0043dd00", {id(radar), zgame::bits(dt)}); }
  virtual bool reticle_over_target(void* target, int32_t x, int32_t y) {
    return (unported("0x00440a30", {id(target), uint32_t(x), uint32_t(y)}) & 0xff) != 0;
  }
  virtual void reticle_aim_state(void* reticle, bool over);  // 0x00443d20 (HudReticle::aim_state)
  // The aim state's outside (0x00443d20). The weapon manager's selected
  // group (mac 4, the planner): its longest range (0x004b1bd0), whether any
  // of its weapons locks (0x004b1c70) and 0x004b1cd0; its +0x88, the lock
  // shown. TARGET's slot's contact ([slot]), its mech (0x004e6ff0) and
  // distance (0x004e7030, zai_target); a mech's kind (+0x684 +0xc) and
  // +0x3cc. The aimed node's object (0x0040eee0 → 0x0040e920), the object's
  // +8 byte and the node's +0xbc. The HUD's sounds (0x0041e800 playing,
  // 0x0041e770 played at 1.0, -1.0).
  virtual float group_range(uint32_t wm) { return zgame::from_bits(unported("0x004b1bd0", {wm})); }
  virtual bool group_locks(uint32_t wm) { return (unported("0x004b1c70", {wm}) & 0xff) != 0; }
  virtual bool group_4b1cd0(uint32_t wm) { return (unported("0x004b1cd0", {wm}) & 0xff) != 0; }
  virtual void set_lock_shown(uint32_t wm, bool on) { unported("poke8", {wm + 0x88, on ? 1u : 0u}); }
  virtual uint32_t slot_contact(void* slot) { return unported("peek32", {id(slot)}); }
  virtual uint32_t contact_mech(uint32_t c) { return unported("0x004e6ff0", {c}); }
  virtual float contact_distance(uint32_t c) { return zgame::from_bits(unported("0x004e7030", {c})); }
  virtual int32_t mech_kind_of(uint32_t m) {
    return static_cast<int32_t>(unported("peek32", {unported("peek32", {m + 0x684}) + 0xc}));
  }
  virtual int32_t mech_3cc(uint32_t m) { return static_cast<int32_t>(unported("peek32", {m + 0x3cc})); }
  virtual uint32_t node_object(uint32_t node) { return unported("0x0040e920", {node}); }
  virtual bool object_byte8(uint32_t o) { return (unported("peek8", {o + 8}) & 0xff) != 0; }
  virtual uint32_t node_bc(uint32_t node) { return unported("peek32", {node + 0xbc}); }
  virtual bool hud_sound_playing(int32_t channel) { return unported("0x0041e800", {channel}) != 0; }
  virtual void hud_sound_play(int32_t channel) { unported("0x0041e770", {channel}); }
  virtual void weapons_looks(void* weapons) { unported("0x00446540", {id(weapons)}); }
  virtual void nav_list_update(uint32_t nav, float dt) { unported("nav.vt+0x24", {nav, zgame::bits(dt)}); }
  virtual void network_hud() {
    if (unported("peek32", {0x609980})) unported("0x00479ed0", {});
  }
  // The Direct3D pass's render calls: a dirty rect (0x00576ce0, through
  // [0x00809348]), the scene (0x00579af0 / 0x00579b10), a flat see-through
  // quad (four vertices into the TL buffer 0x0072f41c from the last, then
  // [0x0072f3b0] = 0x0057bf80 with the colour, alpha, 4, 0, 0), the
  // transparent flush ([0x0072f3e4]), the message panel (0x0040fb30: the
  // panel [0x00603d7c] while shown), a zView's render (0x00442870, its +4),
  // TARGET's view of its target (0x004428b0 on the view and the target's
  // node, between the target's vt+0x68(1) and (0)).
  virtual void dirty_rect(const ztwod::Rect* r) {
    unported("0x00576ce0", {r ? r->left : 0, r ? r->top : 0, r ? r->right : 0, r ? r->bottom : 0});
  }
  virtual void scene_begin() { unported("0x00579af0", {}); }
  virtual void scene_end() { unported("0x00579b10", {}); }
  virtual void flat_quad(const float v[4][3], int16_t colour, uint8_t alpha) {
    unported("0x0057bf80", {colour, alpha, zgame::bits(v[0][0]), zgame::bits(v[0][1]), zgame::bits(v[0][2]),
                            zgame::bits(v[1][0]), zgame::bits(v[1][1]), zgame::bits(v[1][2]), zgame::bits(v[2][0]),
                            zgame::bits(v[2][1]), zgame::bits(v[2][2]), zgame::bits(v[3][0]), zgame::bits(v[3][1]),
                            zgame::bits(v[3][2])});
  }
  virtual void flush_transparent() { unported("[0x0072f3e4]", {}); }
  virtual const ztwod::Rect* nav_rect(uint32_t nav) {
    unported("nav+0x1c84", {nav});
    return nullptr;
  }
  virtual void message_panel_draw() { unported("0x0040fb30", {}); }
  virtual void zview_render(void* view, float dt) { unported("0x00442870", {id(view), zgame::bits(dt)}); }
  virtual void object_view_flag(void* o, int32_t on) { unported("obj.vt+0x68", {id(o), on}); }
  virtual uint32_t object_node(void* o) { return unported("peek32", {id(o) + 4}); }
  virtual void view_draw_node(void* view, uint32_t node) { unported("0x004428b0", {id(view), node}); }
  // Whether the MFD's title shows: [0x00609994] alone, [0x00605f88] clear in a
  // multiplayer game.
  virtual bool title_flag() { return (unported("peek8", {0x609994}) & 0xff) != 0; }
  virtual bool title_mp_off() { return unported("peek32", {0x605f88}) != 0; }
  // The weapon list's lines from the mech's weapons (0x00446520).
  virtual void weapons_refresh(void* weapons) { unported("0x00446520", {id(weapons)}); }
};
HudHooks& hud_hooks();
void set_hud_hooks(HudHooks* h);

// The timer (+0x18410): a FontText "00:00:00.00" at (0x244, 0x0f) in
// mech_string, set as it is made (vtable 0x005bc598, FontText's slots).
class HudTimer : public ztwod::FontText {
 public:
  HudTimer();
};

// The HUD object (0x0043b190, vtable 0x005c01d8).
class Hud : public ztwod::Layer {
 public:
  explicit Hud(Mech* mech);          // 0x0043b190
  void setup(Mech* m);               // 0x0043bb80: hud.zrd's layout for the video mode, the mech given out
  void set_active(int32_t on) override;  // +0x04  0x0043baf0: the radar shown or hidden
  void update(float dt) override;    // +0x00  0x0043c820: the HUD's frame
  void draw3d(float dt);             // 0x0043c6b0: the Direct3D pass (from the mech's 0x0041cd90)
  void reset();                      // 0x0043be10
  Mech* mech = nullptr;              // +0x10
  HudRadar radar{60.0f};             // +0x14
  HudCompass compass;                // +0xdd4
  HudReticle reticle{"ret_on", "ret_off"};  // +0x144c
  HudGauges gauges;                  // +0x1da8
  HudMfd mfd{0x212, 0x172, 0x4b, 100, false};  // +0x3434 (a fifth argument 1, unused)
  HudTarget target{10, 0x172, 0x4b, 100};  // +0x410c
  HudWeapons weapons;                // +0x6520
  HudObjectives objectives;          // +0x17964
  HudTimer timer;                    // +0x18410
  HudEquipment equipment;            // +0x188c0
  uint32_t x22e08[12] = {};          // +0x22e08..+0x22e34
  zrdr::Node* reader = nullptr;      // +0x22e38 hud.zrd's root during the set-up
  // +0x22e3c the resolution's block during it. Not set by the original's
  // constructor; FIX (bug), behind zfix bugs: cleared here (docs/hud.md).
  zrdr::Node* block;
};

}  // namespace mech3
