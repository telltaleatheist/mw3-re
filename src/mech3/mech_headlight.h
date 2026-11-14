#pragma once
#include <cstdint>

// Pirate's Moon's headlight (0x00479330..0x00479c60, between mech_skin and mech_waynet): a mech's `spotlight`
// node under its torso, the `headlight.flt` beam (facade mode 2) and a light, faded in and out
// (docs/pm-map.md, Headlights).
namespace zcls { struct Node; }
namespace zrdr { struct Node; }

namespace mech3 {

// The dfn's +0x1a8, new(0x40) by the dfn's constructor. No shipped file has `lodrange` or `subdivide`.
struct HeadlightConfig {            // 0x00479330
  float lod_scale = 150.0f;         // +0x00  lodrange until the read: then level = d2 * scale + base
  float lod_base = 200.0f;          // +0x04
  float model_offset[3] = {0.0f, 2.0f, -5.0f};  // +0x08
  float light_offset[3] = {0.0f, 0.0f, -80.0f};  // +0x14
  float color[3] = {0.8f, 0.8f, 0.8f};  // +0x20
  float ambient = 0.5f;             // +0x2c
  float diffuse = 0.5f;             // +0x30
  float range[2] = {40.0f, 50.0f};  // +0x34
  uint8_t subdivide = 1;            // +0x3c
};
void headlight_config_read(HeadlightConfig& c, zrdr::Node* block);  // 0x00479440, the dfn's `headlight`

class Headlight {                   // 0x40 bytes, vtable 0x005e5bd8
 public:
  Headlight(zcls::Node* parent, zcls::Node* beam, const HeadlightConfig* cfg, zcls::Node* world);  // 0x004795c0
  ~Headlight();                     // 0x004796f0

  bool active() const;              // 0x00479940, vt+8: the node's active flag
  bool set_active(bool on);         // 0x00479950, vt+0xc: off also leaves the parent, on returns to it
  void turn(bool on, bool fade);    // 0x004799b0: without fade the level at once
  void set_level(float level);      // 0x00479a40
  void set_lod(float distance_sq);  // 0x004799f0
  void fade(float dt);              // 0x00479b50
  void detach();                    // 0x00479c30, the mech's vt+0x34: out of the tree for the mech's update
  void attach(float dt);            // 0x00479c60, the mech's vt+0x3c: back, and the fade
  void show_beam(bool on);          // 0x00479c20's +0x20, vt+0xc: the HUD's view of the mech hides it
  // 0x004798d0: out of its parent, onto `to`; while active, the new parent placed at `offset` (0,0,0 for none).
  void reattach(zcls::Node* to, const float* offset);

  zcls::Node* node = nullptr;       // +0x04  `spotlight`
  bool on = false;                  // +0x08
  float fade_rate = -5.0f;          // +0x0c
  float level = 0.0f;               // +0x10
  float target = 0.0f;              // +0x14
  zcls::Node* parent = nullptr;     // +0x1c
  zcls::Node* beam = nullptr;       // +0x24
  zcls::Node* light = nullptr;      // +0x2c
  float lod = 1.0f;                 // +0x34
  zcls::Node* held_parent = nullptr;  // +0x38  while detached
  const HeadlightConfig* cfg = nullptr;  // +0x3c
};

// The object's world and the beam's tree: [0x0062ca90]'s node, and 0x004868d0 (the mech library's load and
// tree copy, null with no library). A test stands in for them.
struct HeadlightHooks {
  zcls::Node* (*world)() = nullptr;
  zcls::Node* (*load_beam)() = nullptr;
};
HeadlightHooks& headlight_hooks();

// 0x00479510: hardware only ([0x008c4b70]), null without a card or a library.
Headlight* headlight_create(zcls::Node* parent, const HeadlightConfig* cfg);

}  // namespace mech3
