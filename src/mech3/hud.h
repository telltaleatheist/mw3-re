#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"
#include "gamez/ztwod/twod.h"

// The HUD (docs/hud.md). File unknown: the code between mech3\mech_data.cpp
// and mech3\mech_map.cpp (0x0043a7c0..0x00448880); named after what it is.
// One HUD per mech type (Pilot +0x224), a zTwoD Layer holding its pieces;
// each piece is built here on the ported zTwoD classes. The comments give
// each member's offset in the original's 0x22e40-byte object.
namespace mech3 {

struct Mech;

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
  float scale = 206.90143f;          // +0x34  pixels a radian
  int32_t half = 0;                  // +0x38  half the frame's width
  int32_t centre = 0xb8;             // +0x3c  the tape's x at north
  ztwod::Rect src{};                 // +0x40  the tape's part shown
  ztwod::Image tape;                 // +0x50  hud_compass at (245, 19)
  ztwod::Image frame;                // +0x10c hud_compass_frame at (245, 11)
  ztwod::FontText text;              // +0x1c8
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
  void set_opacity(float f);         // 0x0043aa60
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
  ztwod::FontText value;             // +0x4cc (vtable 0x005bc598: FontText's but the destructor)
  ztwod::Image light;                // +0x97c
  int32_t state = 0;                 // +0xa38
  int32_t item = 0;                  // +0xa3c
};

// The equipment list (vtable 0x005c0b90, 0xa548 bytes): sixteen lines over a panel.
class HudEquipment : public ztwod::Object2D {
 public:
  HudEquipment();                    // 0x00448070
  void place(int32_t px, int32_t py, int32_t dy);  // 0x00448140
  void* hud = nullptr;               // +0x34 the HUD it shows (0x00448200)
  HudEquipLine lines[16];            // +0x38
  HudPanel panel;                    // +0xa438
  uint8_t below = 1;                 // +0xa540 under the point; else left of it by the column
  int32_t column = 0x6e;             // +0xa544 the values' right edge
};

// What the HUD calls outside it.
class HudHooks : public zgame::Unported {
 public:
  virtual uint32_t id(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  // The mission's current world entry (0x00491c80 on [0x006098d0]) and its
  // +0xd0 (above 0: the map is flipped, the compass turned half round).
  virtual uint32_t mission_entry() { return unported("0x00491c80", {}); }
  virtual float entry_flip(uint32_t entry) { return zgame::from_bits(unported("@entry+0xd0", {entry})); }
  virtual void setup(void* hud, Mech* m) { unported("0x0043bb80", {id(hud), id(m)}); }
  // A piece not ported yet: its constructor's address and arguments.
  virtual void piece(uint32_t ctor, std::initializer_list<zgame::Arg> args);
};
HudHooks& hud_hooks();
void set_hud_hooks(HudHooks* h);

// A piece not ported yet: an Object2D whose construction is the original
// constructor's recorded call (HudHooks::piece), in the HUD's member order.
class PendingPiece : public ztwod::Object2D {
 public:
  PendingPiece(uint32_t ctor, std::initializer_list<zgame::Arg> args);
};

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
  Mech* mech = nullptr;              // +0x10
  PendingPiece radar{0x0043d260, {zgame::bits(60.0f)}};       // +0x14   0x0043d260 (not ported yet)
  HudCompass compass;                // +0xdd4
  PendingPiece reticle{0x00443b60, {"ret_on", "ret_off"}};     // +0x144c 0x00443b60 (not ported yet)
  PendingPiece gauges{0x00444760, {}};      // +0x1da8 0x00444760 (not ported yet)
  PendingPiece mfd{0x00442060, {0x212, 0x172, 0x4b, 100, 1}};         // +0x3434 0x00442060 (not ported yet)
  PendingPiece target{0x0043fcb0, {10, 0x172, 0x4b, 100}};      // +0x410c 0x0043fcb0 (not ported yet)
  PendingPiece weapons{0x00445f80, {}};     // +0x6520 0x00445f80 (not ported yet)
  HudObjectives objectives;          // +0x17964
  HudTimer timer;                    // +0x18410
  HudEquipment equipment;            // +0x188c0
  uint32_t x22e08[12] = {};          // +0x22e08..+0x22e34
  void* reader = nullptr;            // +0x22e38 hud.zrd's root during the set-up
  // +0x22e3c the resolution's block during it. Not set by the original's
  // constructor; FIX (bug), behind zfix bugs: cleared here (docs/hud.md).
  void* block;
};

}  // namespace mech3
