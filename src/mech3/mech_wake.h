#pragma once
#include <cstdint>

namespace zcls { struct Node; }
namespace zeff { struct AnimDef; }

// The wake a mech's foot leaves in water (0x1c bytes, vtable 0x005c2838;
// the source file is unknown: named for its node). Each foot has one
// (Foot::setup, 0x00462860); its animations are mission animations by name.
namespace mech3 {

struct FootWake {
  zcls::Node* node = nullptr;          // +0x04  "wake_node", under the world
  zeff::AnimDef* anims[3] = {};        // +0x08  by strength
  zeff::AnimDef* playing = nullptr;    // +0x14
  int32_t level = -1;                  // +0x18
};

// What the wake calls outside; unset: zEffect's and zClass's ports (the
// tests stand in for them).
struct WakeHooks {
  zeff::AnimDef* (*start)(zeff::AnimDef* def, zcls::Node* node) = nullptr;  // 0x00508da0(def, 0; node, 0, 0)
  void (*stop)(zeff::AnimDef* playing) = nullptr;                          // 0x00506ca0
  void (*set_active)(zcls::Node* node, int on) = nullptr;                  // 0x004eb170
};
WakeHooks& wake_hooks();

// The three animations by name; null when no world wrapper ([0x00608c84]).
FootWake* foot_wake_new(zcls::Node* world, const char* weak, const char* mid, const char* strong);  // 0x0045f980 (0x0045fa00)
void foot_wake_free(FootWake* w, zcls::Node* world);      // 0x0045fab0
// 0 below 0.2, 1 below 0.5, else 2 (of |throttle|; kept in 0x00605cac).
int32_t wake_level(float throttle);                        // 0x0045fa70
// The animation for the throttle's strength, started on the node when it
// changes.
void foot_wake_start(FootWake* w, float throttle);         // 0x0045faf0
void foot_wake_stop(FootWake* w);                          // 0x0045fb50
bool foot_wake_playing(const FootWake* w);                 // 0x0045fb80

}  // namespace mech3
