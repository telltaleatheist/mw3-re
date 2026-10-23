#include "mech3/veh_controls.h"

#include <cmath>
#include <cstring>
#include <string>

#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zreader/zreader.h"
#include "mech3/mech.h"
#include "mech3/weapon.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

float player_gravity();  // mech3/player.h (its Handle clashes with mech.h's): [0x00609a14]

namespace {
VehControlsHooks g_default_hooks;
VehControlsHooks* g_hooks = &g_default_hooks;
int32_t g_serial = 0;           // [0x0060b148]
const Vec3c kUnit5eda80{0, 0, 1};  // [0x005eda80..88]

zcls::Node* zn(void* h) { return static_cast<zcls::Node*>(h); }
Vehicle* veh(void* owner) { return static_cast<Vehicle*>(owner); }
}  // namespace

void set_veh_controls_hooks(VehControlsHooks* h) { g_hooks = h ? h : &g_default_hooks; }
int32_t& vehicle_serial() { return g_serial; }

// --- the hooks' defaults: the ports -------------------------------------------
void VehControlsHooks::node_rotation(Handle node, float* x, float* y, float* z) {
  zcls::object3d_get_rotation(zn(node), x, y, z);
}
VehControlsHooks::Handle VehControlsHooks::node_find(Handle root, const char* name) {
  return zcls::node_find_in_tree(zn(root), name);
}
VehControlsHooks::Handle VehControlsHooks::node_child(Handle node, const char* name) {
  return zcls::node_find_in_tree(zn(node), name);
}
void VehControlsHooks::node_position(Handle node, Vec3c& out) {
  const float* m = zcls::object3d_matrix(zn(node));
  if (m) out = {m[9], m[10], m[11]};
}
void VehControlsHooks::node_translation(Handle node, float* x, float* y, float* z) {
  zcls::object3d_get_translation(zn(node), x, y, z);
}
VehControlsHooks::Handle VehControlsHooks::node_model(Handle node) { return zn(node)->model; }
void VehControlsHooks::node_set_model(Handle node, Handle model) {
  zcls::node_set_model(zn(node), static_cast<zmod::Model*>(model));
}
void VehControlsHooks::model_release(Handle model) { zmod::model_release(static_cast<zmod::Model*>(model)); }
void VehControlsHooks::node_remove_child(Handle parent, Handle child) { zcls::node_remove_child(zn(parent), zn(child)); }
void VehControlsHooks::node_free(Handle node) { zcls::tree_destroy(zn(node)); }
zrdr::Node* VehControlsHooks::reader_find(zrdr::Node* node, const char* key) { return zrdr::find(node, key); }
VehControlsHooks::Handle VehControlsHooks::anim_find(const char* name) { return zeff::anim_find(name); }
void VehControlsHooks::anim_start(Handle def, Handle node) {
  zeff::anim_start(static_cast<zeff::AnimDef*>(def), zn(node), 0, 0, 0);
}
void VehControlsHooks::owner_weapons(void* owner, zrdr::Node* def) {
  Vehicle* v = veh(owner);
  v->ammo = build_vehicle_weapons(owner, static_cast<WeaponManager*>(v->ammo), def);
}

// Mech3.exe 1.2 @ 0x004a3fc0 (through 0x004a4a70, 0x004a4000).
TurretLimits::TurretLimits(float lo, float hi)
    : lo_sin(static_cast<float>(std::sin(static_cast<double>(lo)))),
      lo_cos(static_cast<float>(std::cos(static_cast<double>(lo)))),
      hi_sin(static_cast<float>(std::sin(static_cast<double>(hi)))),
      hi_cos(static_cast<float>(std::cos(static_cast<double>(hi)))) {}

// Mech3.exe 1.2 @ 0x004ac750 (0x004ac740 on +0x764 -> 0x004a4af0: the
// turret limits of -pi/2 and pi/2).
VehicleControls::VehicleControls() : turret(-1.57079637f, 1.57079637f) {}

// Mech3.exe 1.2 @ 0x005869d0
void Block398::clear() {
  x10 = 0, x08 = 0, x0c = 0;
  flags &= ~3u;
  x14 = 0;
}

// Mech3.exe 1.2 @ 0x004ab990. The x87's fsin/fcos range check (|a| over
// 9.22e18 skips nothing here: both branches compute the same).
void euler_rotation(const float a[3], float out[9]) {
  const float sx = std::sin(a[0]), cx = std::cos(a[0]);
  const float sy = std::sin(a[1]), cy = std::cos(a[1]);
  const float sz = std::sin(a[2]), cz = std::cos(a[2]);
  out[0] = sy * sx * sz + cz * cy;
  out[1] = sz * cx;
  out[2] = sz * cy * sx - cz * sy;
  out[3] = sy * sx * cz - sz * cy;
  out[4] = cz * cx;
  out[5] = cz * cy * sx + sz * sy;
  out[6] = sy * cx;
  out[7] = -sx;
  out[8] = cy * cx;
}

// Mech3.exe 1.2 @ 0x004ac8a0: from the owner's node: a serial number, the
// node's rotation into the owner's matrix, the body, turret, shadow and gun
// nodes, the gun's firepoints; the health 100.
void VehicleControls::init() {
  VehControlsHooks& h = *g_hooks;
  Vehicle* v = veh(owner);
  if (!v || !v->node) return;
  float* m = v->transform;
  serial = g_serial++;
  h.node_rotation(v->node, &angles.x, &angles.y, &angles.z);
  angles0 = angles;
  x50 = Vec3c{};
  euler_rotation(&angles.x, m);
  up = {m[3], m[4], m[5]};
  h.owner_refresh(v);
  body = h.node_find(v->node, "body");
  turret_node = h.node_find(v->node, "turret");
  shadow = h.node_find(v->node, "shadow");
  gun = h.node_find(v->node, "gun");
  if (gun) h.node_position(gun, gun_pos);
  for (uint32_t& w : x94) w = 0;
  x5fc = kUnit5eda80;
  x760 = 0;
  x650 = 1;
  x5ec = 1;
  if (gun) {
    if (Handle c = h.node_child(gun, "fpnt_c")) {
      h.node_translation(c, &fpnt_c.x, &fpnt_c.y, &fpnt_c.z);
      Handle model = h.node_model(c);
      h.node_set_model(c, nullptr);
      h.model_release(model);
      fire_c_scale = 1.0f / std::sqrt(fpnt_c.x * fpnt_c.x + fpnt_c.y * fpnt_c.y + fpnt_c.z * fpnt_c.z);
    } else {
      fire_c_default = {0, 0, -1.0f};
      fire_c_scale = 1.0f;
    }
    if (Handle l = h.node_child(gun, "fpnt_l")) {
      h.node_translation(l, &fpnt_l.x, &fpnt_l.y, &fpnt_l.z);
      h.node_remove_child(gun, l);
      h.node_free(l);
    } else {
      fire_l_default = {1.51f, 0.396f, -2.46f};
    }
    if (Handle r = h.node_child(gun, "fpnt_r")) {
      h.node_translation(r, &fpnt_r.x, &fpnt_r.y, &fpnt_r.z);
      h.node_remove_child(gun, r);
      h.node_free(r);
    } else {
      fire_r_default = {-1.51f, 0.396f, -2.46f};
    }
  }
  x28 = 0, x2c = 0;
  health = health_max = 100.0f;
}

// Mech3.exe 1.2 @ 0x004acbc0 (vt+0x0c): the init (vt+0x10), the
// definition's start_anims started on the owner's node, its health, the
// owner's own 0x004af2a0 and weapons.
void VehicleControls::setup(zrdr::Node* def) {
  VehControlsHooks& h = *g_hooks;
  Vehicle* v = veh(owner);
  if (!v || !v->node) return;
  init();
  if (zrdr::Node* anims = h.reader_find(def, "start_anims")) {
    const zrdr::Node* list = anims->list;
    const int32_t n = list[0].i - 1;
    for (int32_t i = 0; i < n; ++i) {
      const std::string name = list[1 + i].s;  // copied to 80 bytes in the exe
      h.anim_start(h.anim_find(name.c_str()), v->node);
    }
  }
  if (zrdr::Node* hp = h.reader_find(def, "health")) health = health_max = hp->list[1].f;
  h.owner_4af2a0(v, def);
  h.owner_weapons(v, def);
  node = v->node;
  x398.clear();
}

// Mech3.exe 1.2 @ 0x004accd0
void VehicleControls::reset_1c() {
  x30 = 0;
  x350 = 0;
  x3c = 0;
  x1c = player_gravity();
  x5e8 = 0;
}

}  // namespace mech3
