#pragma once
#include <cstdint>
#include <initializer_list>

#include "gamez/zgame/unported.h"

namespace zrdr {
struct Node;
}

// zEffect's effect table (zeffect\zeff_init.c, 0x0050d2c0..0x0050da50): named sprite effects from effects.zrd (a
// node, its gfx data and texture cycles), spawned by index (0x0050d650) as copies that grow, shrink and go back to
// a free list (0x0050d910). Both MW3 1.2 and Pirate's Moon ship an empty EFFECTS list, so nothing reaches the
// entries in play: ported for preservation (docs/weapon-effects.md, The effect table).
namespace zeff {

using EffectHandle = void*;

// What the table calls outside zEffect, keyed by the exe's address; the defaults report unported (the app wires
// them). The '@' keys are the exe's own reads of a node's fields.
class EffectHooks : public zgame::Unported {
 public:
  static uint32_t h(EffectHandle p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  static EffectHandle p(uint32_t v) { return reinterpret_cast<EffectHandle>(static_cast<uintptr_t>(v)); }
  void (*error)(const char* text) = nullptr;                          // the "Failed to read" report
  void (*spawn_asked)(int32_t index, const float* at) = nullptr;      // every spawn asked, loaded or not (the tests)
  virtual zrdr::Node* load(const char* file);  // 0x00556740: the reader file (zrdr::load)
  virtual EffectHandle node_named(const char* name) { return p(unported("0x004f7fe0", {6, name})); }
  virtual EffectHandle node_model(EffectHandle n) { return p(unported("0x004eb420", {h(n)})); }
  virtual int32_t child_count(EffectHandle n) { return static_cast<int32_t>(unported("@node+0x5c", {h(n)})); }
  virtual EffectHandle child(EffectHandle n, int32_t i) { return p(unported("@node+0x60", {h(n), i})); }
  virtual void report(const char* fmt, const char* a, const char* b) { unported("fprintf", {fmt, a, b}); }
  virtual void report_gfx(const char* name) { unported("0x0049bfd0", {0x400, 0x11d, name}); }
  virtual void set_flag8(EffectHandle n, int32_t on) { unported("0x004eb620", {h(n), on}); }
  virtual void set_intersect(EffectHandle n, int32_t on) { unported("0x004eb6d0", {h(n), on}); }
  virtual void set_active(EffectHandle n, int32_t on) { unported("0x004eb170", {h(n), on}); }
  virtual void model_facade(EffectHandle m, int32_t mode) { unported("0x00539cc0", {h(m), mode}); }
  virtual void cycle_on(EffectHandle m, int32_t count) { unported("0x0053bc30", {h(m), count}); }
  virtual void cycle_speed(EffectHandle m, float speed) { unported("0x0053bcf0", {h(m), zgame::bits(speed)}); }
  virtual void cycle_looping(EffectHandle m, int32_t on) { unported("0x0053bcd0", {h(m), on}); }
  virtual EffectHandle add_texture(const char* name) { return p(unported("0x00518c60", {name})); }
  virtual void cycle_map(EffectHandle m, EffectHandle tex) { unported("0x0053bd10", {h(m), h(tex)}); }
  virtual void textures_load() { unported("0x005190d0", {}); }
  virtual void cycle_reset(EffectHandle m) { unported("0x0053bc10", {h(m)}); }
  virtual void node_destroy(EffectHandle n) { unported("0x004f9050", {h(n)}); }
  virtual EffectHandle node_copy(EffectHandle n) { return p(unported("0x004f9af0", {h(n), 1, 1})); }
  virtual void set_scale(EffectHandle n, float s) {
    unported("0x004fbc10", {h(n), zgame::bits(s), zgame::bits(s), zgame::bits(s)});
  }
  virtual void set_translation(EffectHandle n, const float* at) {
    unported("0x004fc0b0", {h(n), zgame::bits(at[0]), zgame::bits(at[1]), zgame::bits(at[2])});
  }
  virtual void camera_position(EffectHandle cam, float out[3]);  // 0x004ed530 on [0x0073f3dc]
  virtual void add_child(EffectHandle parent, EffectHandle n) { unported("0x004eb9b0", {h(parent), h(n)}); }
  virtual void remove_child(EffectHandle parent, EffectHandle n) { unported("0x004ebb40", {h(parent), h(n)}); }
  // The node's action (0x004eb450): the tick (0x0050d910) on, or none.
  virtual void set_action(EffectHandle n, bool tick) { unported("0x004eb450", {h(n), tick ? 0x0050d910u : 0u}); }
  virtual uint32_t node_flags(EffectHandle n) { return unported("@node+0x24", {h(n)}); }
  virtual EffectHandle node_parent(EffectHandle n) { return p(unported("@node+0x54", {h(n)})); }
  virtual float dt() { return zgame::from_bits(unported("@dt", {})); }  // [0x0072c52c]
};
void set_effect_hooks(EffectHooks* h);  // null: the defaults
EffectHooks& effect_hooks();

// An entry of the table, or a spawned copy of one (0x4c bytes; the exe's calloc / malloc).
struct Effect {
  int32_t index = -1;               // +0x00  its place in the table; -1 an entry without its node or gfx
  const char* node_name = nullptr;  // +0x04  the entry's first item
  const char* name = nullptr;       // +0x08  "NAME"
  uint8_t flags = 0;                // +0x0c  1 a copy, 2 its node made
  EffectHandle node = nullptr;      // +0x10
  EffectHandle gfx = nullptr;       // +0x14  the model found under the node (0x0050d610)
  float grow_time = 0;              // +0x18  0.3: growing until then
  float grow = 0;                   // +0x1c  6 a second
  float x20 = 0;                    // +0x20  6
  float x24 = 0;                    // +0x24  30
  float shrink = 0;                 // +0x28  3.75 a second
  float x2c = 0;                    // +0x2c  3
  float x30 = 0;                    // +0x30  10.5
  float life = 0;                   // +0x34  1: freed after
  float t = 0;                      // +0x38  its age
  float scale = 0;                  // +0x3c  5 at first
  float x40 = 0;                    // +0x40  1.4
  float near2 = 0;                  // +0x44  56.25: nearer the camera than its root, not shown
  float far2 = 0;                   // +0x48  1600: nearer than its root, smaller
};

int32_t effects_init(const char* file, EffectHandle world = nullptr, EffectHandle camera = nullptr);  // 0x0050d2c0
int32_t effect_count();                   // [0x0073f3c8]
const Effect* effect_entry(int32_t i);    // [0x0073f3d4] + 0x4c i
int32_t effect_find(const char* name);    // 0x0050d9e0: the index, -1 none
void effect_spawn(int32_t index, const float* at = nullptr);  // 0x0050d650 (ECX the index, -1 none; EDX the place)
void effect_tick(EffectHandle node);      // 0x0050d910: the node's action each frame
const Effect* effect_of(EffectHandle node);  // the node's +0x40
int32_t effects_spawned();                // [0x0073f3e4]
void effects_init();                      // 0x0050d270: with 0x00508fc0, zEffect's start
void effects_free();                      // 0x0050d570: at the mission unload
void effects_shutdown();                  // 0x0050d2b0: at the game's end

}  // namespace zeff
