#include "mech3/mech_place.h"

#include <cmath>

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_move.h"
#include "mech3/mission_world.h"
#include "mech3/veh_basic.h"

#pragma STDC FP_CONTRACT OFF

// Original file: mech3\mech.cpp (0x004161c0 sits before the skis' step
// 0x004162c0, 0x00418e90 and 0x00418f30 by the turn 0x00418fc0).
namespace mech3 {

namespace {
PlaceCalls g_calls;

constexpr float kAbove = 100.0f;  // 0x005bcd80 (-100): the query starts this far above the given y
constexpr float kStep = 10.0f;    // 0x005bcd84 (-10)
constexpr double kSink = -0.01;   // 0x005bcd88: subtracted, so the mech stands 0.01 above

void moved(Mech& m) {
  if (update_calls().moved) update_calls().moved(m);  // 0x005a85c0
  else m.entity_moved();
}
}  // namespace

PlaceCalls& place_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x004161c0. The ground is looked for from 100 above pos
// with every zone in, before the mech's own node joins the world. The pick
// is given y + 10 as its step, so ground up to 2y + 10 counts (the other
// callers pass a constant; docs/mission-start.md section 11); none in reach
// takes the lowest polygon. FIX (bug): a step of 10, as the other callers
// (Owen 2026-10-20; zfix bugs). Always 1.
int Mech::place(const float pos[3], float yaw) {
  float p[3] = {pos[0], pos[1], pos[2]};
  zmod::Zones& zones = zmod::zones();
  const int32_t zone = zones.mode;  // [0x005ea1d0]
  zones.mode = 0;
  zcls::GroundHits hits;
  int none = 1;
  if (g_calls.ground) none = g_calls.ground(p[0], p[1] + kAbove, p[2], &hits);
  else if (zcls::Node* w = world_hooks().world) none = zcls::ground_point(w, p[0], p[1] + kAbove, p[2], &hits);
  else hits.count = 0;  // the exe needs [0x00608c84]
  if (none == 0 && hits.count > 0) {
    int32_t best, in_water;
    float surface;
    const float y = p[1];
    const float step = zfix::options().bugs ? kStep : y + kStep;
    const float h = ground_pick(hits, y, step, &best, 1, &in_water, &surface);
    p[1] = static_cast<float>(static_cast<double>(h) - kSink);
  }
  zones.mode = zone;
  if (g_calls.attach) g_calls.attach(node);
  else if (zcls::Node* w = world_hooks().world) zcls::node_add_child(w, static_cast<zcls::Node*>(node));
  set_place(p, yaw);
  return 1;
}

// Mech3.exe 1.2 @ 0x00418f30
void Mech::set_place(const float pos[3], float yaw) {
  controls->zero();
  zmth::Quat q;
  zmth::quat_yaw(yaw, q);
  orientation[0] = q.w, orientation[1] = q.x, orientation[2] = q.y, orientation[3] = q.z;
  zmth::quat_matrix(orientation, transform);
  reset_at(pos);
}

// Mech3.exe 1.2 @ 0x00418e90. vtable +0x18 is zEntity3d's 0x005a8550: the
// node's translation, then 0x005a85c0, which runs again after it.
void Mech::reset_at(const float pos[3]) {
  controls->zero();
  vec_3c = {0, 0, 0};  // 0x0063d870
  velocity = {0, 0, 0};
  vec_58 = {0, 0, 0};
  if (collide_calls().set_position) {
    const Vec3 p{pos[0], pos[1], pos[2]};
    collide_calls().set_position(*this, &p);
  } else {
    zcls::object3d_set_translation(static_cast<zcls::Node*>(node), pos[0], pos[1], pos[2]);
    moved(*this);
  }
  moved(*this);
  collider.place(transform);
  skis->reset();
}

// Mech3.exe 1.2 @ 0x005a85c0 (zEntity), for a node in the world: vtable
// +0x1c, zEntity3d's 0x005a85a0, which is 0x005a8580 (the node's matrix set
// from the mech's, 0x005a8500 its two marker nodes' world matrices) and
// 0x005a84e0 (+0x18 and +0x24 from the matrix's z row).
void Mech::entity_moved() {
  auto* n = static_cast<zcls::Node*>(node);
  if (!n || n->parent_count() == 0) return;
  zcls::object3d_set_matrix(n, transform);
  refresh_markers();
  x18 = {-transform[6], -transform[7], -transform[8]};  // 0x005a86b0
  zmth::Vec3 f{-transform[6], 0.0f, -transform[8]};     // 0x005a86e0
  zmth::normalize(f);
  forward = {f.x, f.y, f.z};
}

// Mech3.exe 1.2 @ 0x005a8000 (zEntity3d): turned by +0x58 over dt (a half
// angle's quaternion), the velocity +0x30 the orientation's turn of +0x3c,
// the node moved by it, the matrix from the orientation, then 0x005a85c0.
void Mech::entity_step(float dt) {
  const float h = dt * 0.5f;  // 0x005d0ee4
  const zmth::Vec3 v{h * vec_58.x, vec_58.y * h, vec_58.z * h};
  zmth::Quat e, p, n;
  zmth::quat_exp(v, e);
  const zmth::Quat o{orientation[0], orientation[1], orientation[2], orientation[3]};
  zmth::quat_mul(e, o, p);
  zmth::quat_normalize(p, n);
  orientation[0] = n.w, orientation[1] = n.x, orientation[2] = n.y, orientation[3] = n.z;
  zmth::Vec3 wv;
  zmth::quat_rotate(n, {vec_3c.x, vec_3c.y, vec_3c.z}, wv);
  velocity = {wv.x, wv.y, wv.z};
  const float mx = velocity.x * dt, my = velocity.y * dt, mz = velocity.z * dt;
  if (update_calls().translate) update_calls().translate(node, mx, my, mz);  // 0x004fc1b0
  else zcls::object3d_translate(static_cast<zcls::Node*>(node), mx, my, mz);
  zmth::quat_matrix(orientation, transform);  // 0x00525740
  moved(*this);
}

// Mech3.exe 1.2 @ 0x005a8500
void Mech::refresh_markers() {
  zmth::Mat34 buf;
  zmth::push(&buf);
  zmth::load_identity();
  zcls::node_to_world(static_cast<zcls::Node*>(target_node), 3);
  zmth::load_identity();
  zcls::node_to_world(static_cast<zcls::Node*>(centroid), 3);
  zmth::pop();
}

// Mech3.exe 1.2 @ 0x005a8680, then 0x005a85c0 as its callers do.
void Mech::set_target_node(Handle n) {
  target_node = n;
  refresh_markers();
  moved(*this);
}

}  // namespace mech3
