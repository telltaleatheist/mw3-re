#pragma once
#include <cstdint>

// zModel\gmod_init.c: the render state the scene walk sets around each node
// (opacity, tint, shadows, overlay), the camera's viewport and projection
// (0x00526e10) and its clip planes (0x005271a0, 0x005272b0); the last three
// sit between gmod_init.c and gmod_matl.c, so most likely the same file
// (docs/drafts/scenewalk.md).
namespace zcls { struct Node; struct Camera; }

namespace zmod {

// The walk's plane-mask stack and the model draw it calls (0x00526510 sets
// the pointers). Bits: 1, 2, 4, 8 sides, 0x10 near, 0x20 far; a set bit is a
// plane the node's sphere still crosses.
struct WalkState {
  // 0x0074a2e4..0x0074a320: 16 entries in the exe, unchecked (the next dword
  // is the top pointer itself). Deeper trees overrun there; 64 here.
  uint32_t mask[64] = {};
  int32_t top = 0;                 // 0x0074a324 -> mask[top]
  // [0x0074a2e0]: 0x0053f2e0 with Direct3D, 0x0053e050 in software.
  int32_t (*draw)(zcls::Node* node, uint32_t mask) = nullptr;
  zcls::Camera* camera = nullptr;  // 0x007402f4 (also zmod::draw_state().camera_data)
  int32_t screen_w = 640;          // 0x008027c8  zvid's display size, used when the
  int32_t screen_h = 480;          // 0x008027cc  window has none
};
WalkState& walk_state();
uint32_t& mask_top();              // *[0x0074a324]

void set_tint(const float* rgb, float weight);  // 0x00526a10  null rgb keeps the colour
void set_opacity(float opacity);   // 0x00526a60
void set_shadows(int32_t on);      // 0x00526a70
int32_t shadows_on();              // 0x00526a80
void set_overlay(int32_t on);      // 0x00526a90
int32_t fog_on();                  // 0x00526b90
float fog_near();                  // 0x00526bf0

// ECX camera data: near/far, the window's rect into the clip struct, the
// projection (0x00521c60) and the field of view (0x00521c10).
void set_camera(zcls::Camera* camera);          // 0x00526e10
// ECX camera data: the six inward plane normals in camera space (+0x13c).
void camera_local_planes(zcls::Camera* camera);  // 0x005271a0
// ECX camera data: those normals through its world matrix (+0x44) into +0x188.
void camera_world_planes(zcls::Camera* camera);  // 0x005272b0

}  // namespace zmod
