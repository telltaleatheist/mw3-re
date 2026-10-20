#pragma once
#include <cstdint>

#include "mech3/mech_parts.h"

// The mech's ground probes, "skis" (mech +0x41c, 0x300 bytes): seven points
// that find the ground under the mech each frame, move its body up and down,
// slide it off steep slopes and decide whether it is airborne
// (docs/movement-ground.md). Original file: mech3\mech_skis.cpp.
namespace mech3 {

struct Mech;

// One polygon under a probe, as the engine's ground query writes it (0x28
// bytes in the exe); the skis keep the chosen one with the probe's x and z.
struct GroundHit {
  Vec3 normal{};           // +0x00
  float x = 0;             // +0x0c  (the probe's, filled by the skis)
  float height = 0;        // +0x10
  float z = 0;             // +0x14
  uint8_t zone[8] = {};    // +0x18  the polygon's zone set
  Handle material = nullptr;  // +0x20  its +0x20 indexes the soil table 0x00608c00
  Handle node = nullptr;   // +0x24
};
struct GroundHits {        // 0x504 bytes per probe, at 0x006061d0
  uint8_t count = 0;
  GroundHit hit[32];
};

// Calls into the engine. The ground query is zclass/cls_di's (0x004ee2f0) on
// world_hooks().world; a test can stand in for it with `query`.
struct SkisCalls {
  void (*query)(Vec3* probes, int count, float top, GroundHits* out) = nullptr;  // test hook; unset: zcls::ground_query
  int (*soil)(Handle material) = nullptr;            // [0x00608c00 + 4 * material +0x20]
  float* (*node_matrix)(Handle node) = nullptr;      // 0x004fc3f0: the object3d's local 3x4
  void (*set_node_matrix)(Handle node, const float* m) = nullptr;  // 0x004fc300
  const Vec3* (*position)(Mech&) = nullptr;          // 0x005a85e0: the world position
  void (*zone)(const uint8_t* zone_set) = nullptr;   // 0x00476550: the centre probe's zone
};
SkisCalls& skis_calls();

class Skis {
 public:
  Mech* mech = nullptr;       // +0x000
  bool slid = false;          // +0x005  pushed this frame: no push decay, throttle stop
  float matrix[12] = {};      // +0x008  the mech's world matrix, moved ahead by the frame's motion
  Handle base = nullptr;      // +0x038  "ski base" under the mech's node (its wrapper's node)
  Handle ski[2] = {};         // +0x03c, +0x040  (their wrappers' node)
  float sin_pitch[2] = {}, cos_pitch[2] = {};  // +0x044, +0x04c
  float a[2] = {}, b[2] = {}, c[2] = {};       // +0x054, +0x05c, +0x064  each ski's height parabola
  bool down[2] = {true, true};  // +0x06c  each ski on the ground
  float slope[2] = {};        // +0x070  sine of each ski's pitch, smoothed
  float half_track = 0;       // +0x078
  Vec3 world[7] = {};         // +0x07c  probes 0..2 left ski (front, mid, back), 3..5 right, 6 centre
  Vec3 local[7] = {};         // +0x0d0  in ski space
  float ground[7] = {};       // +0x124
  float vy[7] = {};           // +0x140
  float surface[7] = {};      // +0x15c  water surface, -300 if none
  int32_t in_water[7] = {};   // +0x178
  GroundHit contact[7];       // +0x194
  float clearance[7] = {};    // +0x2ac
  float y[7] = {};            // +0x2c8
  bool landed[7] = {};        // +0x2e4  vy > 20 at the last snap
  float length = 12.0f;       // +0x2ec  4 for the Elemental
  float half = 6.0f;          // +0x2f0
  float mid_width = 0;        // +0x2f4  never written in the exe (docs/movement-ground.md)
  // FIX (bug): the mid probe's width as the half length (the nearest probe
  // wins); the exe's unset +0x2f4 with zfix bugs off.
  float mid() const;
  float inv_half = 0, inv_half2 = 0;  // +0x2f8, +0x2fc

  bool update(float dt);                                     // 0x004765b0, returns airborne
  void fit(int side);                                        // 0x00475c80
  float height_at(int side, float z, GroundHit** contact);   // 0x00475de0
  bool water_at(int side, float z, float* surface_y) const;  // 0x00475ea0
  void set_ski(int side, float angle, float lift);           // 0x004764a0
  void set_vy(float v);                                      // 0x00476590

  static float pick_ground(const GroundHits& hits, int* best, float probe_y, float step,
                           bool water_not_ground, int32_t* in_water, float* surface);  // 0x00477820
  static bool integrate(float* y, float* vy, float dt, float ground, float gravity);  // 0x00477940
  static bool slope_slide(const GroundHit& c, Vec3* push, float dt);                  // 0x00477620
};

}  // namespace mech3
