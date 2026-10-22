#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"

// The mechs' sun shading, the options' "shadow parts" (docs/shade.md).
// Original file unknown: 0x004e61e0..0x004e6990 sit between zrdrvalve.cpp
// and zClass's class.c. Each mech (+0x68c) and mech part (+0x68) keeps a
// diffuse level; one mech a frame casts a 150 m ray from its centroid
// towards the sun, the level fades (1 a second) to 0 in shadow and 1 in
// the sun, and while it differs from the sun's own diffuse the mech's node
// is flagged so that the scene walk swaps the sun's diffuse for the mech's
// level while it draws (0x004e6720 before, 0x004e6770 after).
namespace mech3 {

// The level a mech or part keeps (0x0c bytes: 0x004e6250).
struct ShadeLevel {
  float saved = 0;    // +0x00  the sun's diffuse while the node draws
  float target = 0;   // +0x04  0 in shadow, 1 in the sun
  float current = 0;  // +0x08
};

// A mech's (0x24 bytes, vtable 0x005cd5e4; 0x004e61e0).
struct SunShade {
  zgame::Handle mech = 0;   // +0x04  the vehicle (its +0x04 node, +0x14 centroid)
  zgame::Handle parts = 0;  // +0x08  &mech+0x380 (the parts' list +0x04)
  uint8_t differs = 0;      // +0x0c  the level is not the sun's: the node is flagged
  ShadeLevel level;         // +0x10
  int32_t turn = 0;         // +0x20  its turn of the round
};

// The globals 0x0060b7dc..0x0060b7f4.
struct ShadeGlobals {
  zgame::Handle sun = 0;    // 0x0060b7dc  the world's sun light (0 off)
  uint8_t on = 0;           // 0x0060b7e0
  int32_t count = 0;        // 0x0060b7e4  shades made: the round's length
  int32_t turn = 0;         // 0x0060b7e8  whose turn this frame is
  int32_t frame = 0;        // 0x0060b7ec  the frame the turn moved on (0x7fffffff off)
  uint8_t option = 0;       // 0x0060b7f0  the option as last set
  uint8_t result = 0;       // 0x0060b7f4  what 0x004e6820 last returned
};
ShadeGlobals& shade_globals();

class ShadeHooks : public zgame::Unported {
 public:
  // The world's sun: [0x006098d4] +0x04, its light +0x38 (0 without).
  virtual zgame::Handle world_sun() { return unported("[0x006098d4]+4+0x38", {}); }
  virtual float sun_diffuse(zgame::Handle sun) { return zgame::from_bits(unported("light+0x9c", {sun})); }
  virtual void set_sun_diffuse(zgame::Handle sun, float v) { unported("light+0x9c=", {sun, zgame::bits(v)}); }
  virtual void sun_direction(zgame::Handle sun, float out[3]) {  // light +0x84
    unported("light+0x84", {sun});
    out[0] = out[1] = out[2] = 0;
  }
  virtual zgame::Handle entity_node(zgame::Handle e) { return unported("entity+0x04", {e}); }
  virtual zgame::Handle entity_centroid(zgame::Handle e) { return unported("entity+0x14", {e}); }
  // The node's world position (its data +0x84), its matrix first redone when
  // stale (data flag 0x20: 0x0051e9d0, 0x0051fde0, 0x004ed280(1), 0x0051ea00).
  virtual void node_world_cached(zgame::Handle node, float out[3]) {
    unported("node.data+0x84", {node});
    out[0] = out[1] = out[2] = 0;
  }
  virtual void node_world_position(zgame::Handle node, float out[3]) {  // 0x004ed530
    unported("0x004ed530", {node});
    out[0] = out[1] = out[2] = 0;
  }
  virtual void node_intersect(zgame::Handle node, int32_t on) { unported("0x004eb6d0", {node, on}); }
  virtual void query_mode(int32_t on) { unported("0x004ed9c0", {on}); }
  virtual void query_exclude(uint32_t mask) { unported("0x004eda00", {mask}); }
  // 0x004eed90 on the world ([0x00608c84] +4) from a to b: its result
  // (0 found) and the hit list's first byte.
  virtual int32_t segment(const float a[3], const float b[3], uint8_t* first) {
    unported("0x004eed90", {zgame::bits(a[0]), zgame::bits(a[1]), zgame::bits(a[2]), zgame::bits(b[0]),
                            zgame::bits(b[1]), zgame::bits(b[2])});
    *first = 0;
    return 1;
  }
  // 0x004eb8a0 and 0x004eb8e0: the node's +0x2c bits 5 and 6 (the scene
  // walk's before and after calls).
  virtual void node_flags(zgame::Handle node, uint8_t on) { unported("0x004eb8a0", {node, on}); }
  virtual int32_t frame() { return static_cast<int32_t>(unported("[0x0072f338]", {})); }
  virtual void walk_callbacks(bool on) { unported("0x004f7630", {on ? 1u : 0u}); }  // and 0x004f7650
  virtual void static_lights(int32_t on) { unported("[0x005ef740]", {on}); }
  virtual void stamps_frozen(int32_t on) { unported("0x004ed950", {on}); }  // [0x0060b94c]
  // The before and after calls' owners: a part's level (0x0040ef30, +0x68),
  // else a mech's (0x0040ef10, +0x68c +0x10); null for neither.
  virtual ShadeLevel* part_level(zgame::Handle node) { unported("0x0040ef30", {node}); return nullptr; }
  virtual ShadeLevel* mech_level(zgame::Handle node) { unported("0x0040ef10", {node}); return nullptr; }
  // The parts' levels (the shade's +0x08 list), for 0x004e67c0, with their nodes.
  virtual int32_t part_count(SunShade& s) { (void)s; return 0; }
  virtual ShadeLevel* part(SunShade& s, int32_t i, zgame::Handle* node) {
    (void)s, (void)i;
    *node = 0;
    return nullptr;
  }
  // zEntity's list: each mech's shade (vt+0x2c, vt+0x50, +0x68c), null else.
  virtual int32_t entity_count() { return 0; }
  virtual SunShade* entity_shade(int32_t i) { (void)i; return nullptr; }
  virtual float game_time() { return zgame::from_bits(unported("[0x0060991c]", {})); }
};
void set_shade_hooks(ShadeHooks* h);
ShadeHooks& shade_hooks();

void shade_level_reset(ShadeLevel& l, zgame::Handle node);   // 0x004e66a0
void shade_level_fade(ShadeLevel& l, float dt);              // 0x004e6640
void shade_init(SunShade& s, zgame::Handle mech, zgame::Handle parts);  // 0x004e61e0
void shade_update(SunShade& s, float dt);                    // 0x004e6480
void shade_reset(SunShade& s);                               // 0x004e67c0
uint8_t shade_enable(uint8_t on);                            // 0x004e6820
void shade_apply();                                          // 0x004e6900: the option as set
void shade_off();                                            // 0x004e6910
void shade_set_option(uint8_t on);                           // 0x004e6920
void shade_before(zgame::Handle node);                       // 0x004e6720
void shade_after(zgame::Handle node);                        // 0x004e6770

}  // namespace mech3
