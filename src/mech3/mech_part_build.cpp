// The parts' constructors and their armor and structure from a configuration
// (docs/mech-build.md section 4, docs/damage.md). Original file:
// mech3\mech_parts.cpp (the constructors sit with each class's methods).
#include <cstdio>
#include <cstring>

#include "mech3/mech.h"
#include "mech3/mech_build.h"
#include "mech3/mech_config.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {

const float kIdentity[4] = {1, 0, 0, 0};  // 0x005eda50

void identity(float q[4]) { std::memcpy(q, kIdentity, sizeof kIdentity); }

template <class T>
T* made(uint32_t size, T* p) {
  mech_build_hooks().on_new(size, p);
  return p;
}

float* at(Handle data, uint32_t off) { return reinterpret_cast<float*>(static_cast<uint8_t*>(data) + off); }

// 100 / points as the exe has it: 1 / (points x 0.01), or `none` for 0 points.
float point_scale(int32_t points, float none) {
  const float x = static_cast<float>(points) * 0.01f;  // 0x005c2758
  return x == 0.0f ? none : 1.0f / x;
}

}  // namespace

// Mech3.exe 1.2 @ 0x0045fbe0: the part's node by its slot's name (the part's
// own name when the slot has none) under the mech and under its low-detail
// model, both trees marked as the part's, and its node reference.
void part_construct(MechPart& p, Mech& m, const PartSlot& slot) {
  MechBuildHooks& h = mech_build_hooks();
  p.mech = &m;
  p.x2c = 0, p.x30 = 0, p.recoil = 0;
  p.internal = 0, p.internal_scale = 1.0f, p.armor = 0;
  p.destroyed = false, p.pending = false;
  p.pending_pos = Vec3{};
  identity(p.pending_q);
  p.x68 = nullptr, p.debris_effect = nullptr, p.trail_effect = nullptr;
  p.found = h.node_find(m.node, slot.name ? slot.name : part_name(slot.id));
  p.lod = h.node_find(m.lod_node, slot.name ? slot.name : part_name(slot.id));
  part_set_node(p, p.found);
  part_mark_nodes(p, slot.id, nullptr);
  part_mark_nodes(p, slot.id, p.lod);
  p.submerged = false;
  const Handle mem = h.alloc(0xc);
  p.x68 = mem ? h.node_ref_init(mem) : nullptr;
}

// Mech3.exe 1.2 @ 0x00460190: an object3d's matrix and translation become the
// part's; any other node (or none) leaves it without.
bool part_set_node(MechPart& p, Handle node) {
  MechBuildHooks& h = mech_build_hooks();
  if (p.x68) h.node_ref_set(p.x68, p.node);
  if (node && h.node_class(node) == 5) {
    p.node = node;
    const Handle data = h.node_data(node);
    p.rotation = p.x1c = at(data, 0x30);
    p.x20 = at(data, 0x3c);
    p.x24 = at(data, 0x48);
    p.translation = p.x28 = reinterpret_cast<Vec3*>(at(data, 0x54));
    if (p.x68) h.node_ref_set(p.x68, node);
    return true;
  }
  p.node = nullptr;
  p.rotation = p.x1c = p.x20 = p.x24 = nullptr;
  p.translation = p.x28 = nullptr;
  p.id = -1;
  return false;
}

// Mech3.exe 1.2 @ 0x00460050: a node (the part's own when none is given) and
// every node under it take the part's id (+0x28) and the part (+0x40), and
// lose their action.
void part_mark_nodes(MechPart& p, int32_t id, Handle node) {
  MechBuildHooks& h = mech_build_hooks();
  if (!node && !(node = p.node)) return;
  h.node_set_owner(node, id, &p);
  if (node == p.node) p.id = id;
  h.node_set_action(node, 0);
  for (int32_t i = 0; i < h.node_child_count(node); ++i) part_mark_nodes(p, id, h.node_child(node, i));
}

// Mech3.exe 1.2 @ 0x004620a0
Hip* make_hip(Mech& m, const PartSlot& s) {
  Hip* p = made(0xb0, new Hip);
  part_construct(*p, m, s);
  identity(p->rot);
  p->track = 0;
  p->offset = Vec3{};
  p->target_x = p->crouch_target = p->target_z = 0;
  return p;
}

// Mech3.exe 1.2 @ 0x00464000: the pose and aim at rest, facing -z, the
// look limits' defaults (-12, 20 and 100 degrees) until the setup reads the
// definition's.
Torso* make_torso(Mech& m, const PartSlot& s) {
  Torso* p = made(0x1b4, new Torso);
  part_construct(*p, m, s);
  p->death_effect = p->death_effect2 = nullptr;
  p->half_width = p->half_depth = 0;
  p->mode = 0, p->aim_local = 0;
  p->facing = 0;
  p->aim = 0;
  p->absolute = p->pitch_lock = p->view_aim = false;
  identity(p->view_q);
  p->look_yaw_angle = p->look_pitch_angle = 0;
  p->twist_in = p->pitch_in = 0;
  p->kick = p->kick_twist = 0;
  identity(p->turn_q);
  identity(p->rot);
  p->front = Vec3{0, 0, -1.0f};
  p->flat_front = Vec3{0, 0, p->front.z};
  p->pitch = p->yaw = 0;
  p->centring = 0;
  p->jolt_spin = p->jolt_turn = Vec3{};
  identity(p->jolt_q);
  p->jolt = p->jolt_pos = Vec3{};
  p->side_internal[1] = p->side_internal[0] = 0;
  p->max_yaw = 1.74532926f;   // 0x3fdf66f3, 100 degrees
  p->min_pitch = -0.209439516f;  // 0xbe567750, -12 degrees
  p->max_pitch = 0.34906584f;    // 0x3eb2b8c2, 20 degrees
  return p;
}

// Mech3.exe 1.2 @ 0x004671a0
Head* make_head(Mech& m, const PartSlot& s) {
  Head* p = made(0x74, new Head);
  part_construct(*p, m, s);
  return p;
}

// Mech3.exe 1.2 @ 0x00466280, with the hand 0x00466da0.
Arm* make_arm(Mech& m, const PartSlot& arm, const PartSlot& hand) {
  Arm* p = made(0xd0, new Arm);
  part_construct(*p, m, arm);
  p->hand = nullptr;
  p->length = p->recoil_hold = 0;
  p->aim_pitch = p->aim_yaw = p->xa0 = 0;
  identity(p->aim_q);
  Hand* hd = made(0x90, new Hand);
  part_construct(*hd, m, hand);
  identity(hd->rot);
  p->hand = hd;
  return p;
}

// Mech3.exe 1.2 @ 0x004635e0, with the calf 0x00466ee0 and the foot.
Leg* make_leg(Mech& m, const PartSlot& thigh, const PartSlot& calf, const PartSlot& foot) {
  Leg* p = made(0xd4, new Leg);
  part_construct(*p, m, thigh);
  Calf* c = made(0x74, new Calf);
  part_construct(*c, m, calf);
  p->calf = c;
  p->foot = make_foot(m, foot);
  identity(p->thigh_q);
  identity(p->calf_q);
  // A third quaternion's pattern over the lengths the setup computes.
  p->rest_x = 1.0f, p->thigh_len = 0, p->ik_b0 = 0, p->hip_x = 0;
  return p;
}

// Mech3.exe 1.2 @ 0x00462590: the sole's depth (the node's box below its
// origin), the shadow marker <c>shadow hidden and dropped, the toes <c>toe01..
// counted from the children that start "<c>toe" (c the node name's first
// letter: l or r).
Foot* make_foot(Mech& m, const PartSlot& s) {
  MechBuildHooks& h = mech_build_hooks();
  Foot* p = made(0xec, new Foot);
  part_construct(*p, m, s);
  p->in_water = false;
  p->sole = 0;
  p->shadow = nullptr;
  p->xb0 = 0;
  identity(p->rot);
  p->water_depth = p->water_surface = 0;
  p->effect = nullptr;
  const Handle node = p->node;
  h.node_tree_update(node);
  // A foot without its node reads through null in the exe (a crash).
  const float min_y = node ? h.node_min_y(node) : 0.0f;
  p->sole = -min_y;
  if (!(p->sole >= 0.0f)) p->sole = 0.0f;
  const char c = node ? h.node_name(node)[0] : '\0';
  char name[16] = "_shadow";
  name[0] = c;
  if (Handle shadow = h.node_find(node, name)) {
    p->shadow = shadow;
    h.node_set_translation(shadow, 0.0f, -p->sole, 0.0f);
    if (Handle model = h.node_model(shadow)) h.model_set_priority(model, 10);
    h.node_set_active(shadow, 0);
    p->shadow = nullptr;
  }
  p->toe_count = 0;
  p->toe_nodes = nullptr;
  p->toe_q = nullptr;
  p->x98 = p->x9c = 0;
  const char prefix[5] = {c, 't', 'o', 'e', '\0'};
  for (int32_t i = 0; node && i < h.node_child_count(node); ++i)
    if (std::strncmp(prefix, h.node_name(h.node_child(node, i)), 4) == 0) ++p->toe_count;
  if (p->toe_count) {
    p->x98 = p->toe_count;
    p->toe_nodes = new Handle[p->toe_count]();
    h.on_calloc(4, p->toe_count, p->toe_nodes);
    p->toe_q = new float[p->toe_count][4]();
    h.on_calloc(0x10, p->toe_count, p->toe_q);
    for (uint32_t i = 0; i < p->toe_count; ++i) {
      std::snprintf(name, sizeof name, "%ctoe%02d", c, static_cast<int>(i + 1));
      p->toe_nodes[i] = h.node_find(node, name);
      identity(p->toe_q[i]);
    }
  }
  p->last_clearance = p->clearance = 0;
  p->xb4 = p->xb8 = 0;
  p->pivot_rate = p->spin = 0;
  p->ground_state = 3;
  p->xbc = -1.0f;
  p->pivot = false;
  return p;
}

// Mech3.exe 1.2 @ 0x0045fe00: whole armor and structure, their scales from
// the configuration's armor slot for the part and the structure table
// (docs/damage.md, Armor). The setup's call (with no configuration) changes
// nothing.
bool MechPart::init(const Config* cfg) {
  if (!cfg) return false;
  const int32_t roster = mech->pilot->roster;
  int32_t slot;
  switch (id) {
    case kTorso: slot = 0; break;
    case kLeftArm: slot = 7; break;
    case kRightArm: slot = 8; break;
    case kLeftThigh: slot = 9; break;
    case kRightThigh: slot = 10; break;
    case kHead: slot = 6; break;
    case kLeftTorso: slot = 1; break;
    case kRightTorso: slot = 2; break;
    default: slot = -1; break;
  }
  int32_t points = 0;
  switch (slot) {
    case 0: case 3: points = internal_points(roster, 1); break;
    case 1: case 2: case 4: case 5: points = internal_points(roster, 2); break;
    case 6: points = 3; break;
    case 7: case 8: points = internal_points(roster, 3); break;
    case 9: case 10: points = internal_points(roster, 4); break;
    default: break;
  }
  internal_scale = point_scale(points, 1.0f);
  internal = 1.0f;
  armor = 1.0f;
  armor_scale = point_scale(slot >= 0 && slot < 11 ? cfg->armor[slot] : 0, 100.0f);  // 0x005c276c
  return true;
}

// Mech3.exe 1.2 @ 0x004644b0: the side and rear sections' armor too; the
// side torsos' structure is set whatever the configuration (the setup's
// call gives it).
bool Torso::init(const Config* cfg) {
  const bool ok = MechPart::init(cfg);
  if (ok && cfg) {
    for (int s = 1; s <= 5; ++s) {
      sec_armor[s] = 1.0f;
      sec_armor_scale[s] = point_scale(cfg->armor[s], 100.0f);
    }
  }
  const int32_t points = internal_points(mech->pilot->roster, 2);
  side_internal[1] = side_internal[0] = 1.0f;
  side_internal_scale[1] = side_internal_scale[0] = 1.0f / (static_cast<float>(points) * 0.01f);
  return ok;
}

}  // namespace mech3
