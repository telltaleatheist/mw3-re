#include "mech3/mech_reader.h"

#include <cmath>

#include "gamez/zreader/zreader.h"
#include "mech3/mech_names.h"

namespace mech3 {

// Mech3.exe 1.2 @ 0x0046b890. The defaults every mech starts from.
MechDfn::MechDfn(int32_t i, zrdr::Node* tree) : index(i), reader(tree) {
  put<int32_t>(0x10, 10), put<int32_t>(0x14, 400);
  static const struct { int off; uint32_t bits; } kDefaults[] = {
      {0x148, 0x3f800000}, {0x14c, 0x42be0000}, {0x150, 0x42aa0000}, {0x154, 0x3edf66f3},
      {0x158, 0x3f800000}, {0x15c, 0x3f800000}, {0x160, 0x3efa35dd}, {0x164, 0x3edf66f3},
      {0x168, 0x3e61307a}, {0x16c, 0x41600000}, {0x170, 0x4039999a}, {0x180, 0x3f000000},
      {0x188, 0x3eb2b8c2}, {0x18c, 0x40400000}, {0x190, 0xbfb2b8c2}, {0x194, 0x3f9c61aa},
      {0x198, 0xbfb2b8c2}, {0x19c, 0x3fb2b8c2}, {0x1a0, 0x3e99999a}};
  for (const auto& d : kDefaults) put(d.off, d.bits);
}

// Mech3.exe 1.2 @ 0x0046ba20. The node is never read.
bool PartSlot::set(int32_t part, zrdr::Node*) {
  name = part_name(part);
  id = part;
  return true;
}

namespace {
constexpr double kRad = 0.01745329251994;  // 0x005c3398

// A float key into the image; the getters write only on success.
bool float_key(MechDfn& d, zrdr::Node* node, const char* key, int off) {
  float v = d.get<float>(off);
  const bool ok = zrdr::get_float(node, key, &v);
  d.put(off, v);
  return ok;
}

void angle_key(MechDfn& d, zrdr::Node* node, const char* key, int off) {
  if (float_key(d, node, key, off)) d.put(off, static_cast<float>(d.get<float>(off) * kRad));
}
}  // namespace

// The movement, torso, lean, recoil and aim settings (docs/mechdef-head.md).
// Angles are degrees in the file, radians here; the file's engine maximum is
// replaced by 400.
bool read_head(MechDfn& d, HeadValues* out) {
  zrdr::Node* tree = d.reader;
  out->tech = mech_tech(d.index);
  int32_t version = d.get<int32_t>(0xc);
  const bool has_version = zrdr::get_int(tree, "version", &version);
  d.put(0xc, version);
  if (!has_version || version != 2) return false;
  bool chicken = d.raw[0x18] != 0;
  zrdr::get_bool(tree, "chicken", &chicken);
  d.raw[0x18] = chicken;
  out->half_turn_rate = 1.25663686f;  // 72 degrees, 0x3fa0d97a
  out->accel_rate = 0.5f, out->decel_rate = 1.3f, out->turn_accel_factor = 1.5f;
  zrdr::get_float(tree, "max_turn_rate", &out->half_turn_rate);
  zrdr::get_float(tree, "accel_rate", &out->accel_rate);
  zrdr::get_float(tree, "decel_rate", &out->decel_rate);
  zrdr::get_float(tree, "turn_accel_factor", &out->turn_accel_factor);
  out->half_turn_rate *= 0.5f;
  float_key(d, tree, "crouch_height", 0x148);
  int32_t engine[2] = {d.get<int32_t>(0x10), d.get<int32_t>(0x14)};
  zrdr::get_ivec2(tree, "engine_minmax", engine);
  d.put(0x10, engine[0]), d.put(0x14, 400);
  // Uninitialised stack in the exe; only a wrongly typed component would show it.
  float torso[3] = {};
  if (zrdr::get_vec3(tree, "torso_limits", torso)) {
    d.put(0x174, static_cast<float>(torso[0] * kRad));
    d.put(0x17c, static_cast<float>(torso[1] * kRad));
    d.put(0x178, static_cast<float>(torso[2] * kRad));
  }
  if (zrdr::Node* shake = zrdr::find(tree, "torso_shake")) {
    angle_key(d, shake, "pitch_amp", 0x160);
    angle_key(d, shake, "yaw_amp", 0x164);
    angle_key(d, shake, "roll_amp", 0x168);
    float_key(d, shake, "freq", 0x16c);
    float_key(d, shake, "damp_freq", 0x170);
  }
  float lod[2] = {};
  if (zrdr::get_vec2(tree, "lod_range", lod)) d.put(0x14c, lod[0]), d.put(0x150, lod[1]);
  angle_key(d, tree, "lean_angle", 0x154);
  float_key(d, tree, "lean_factor", 0x158);
  float_key(d, tree, "lean_speed", 0x15c);
  if (float_key(d, tree, "recoil_bend", 0x180))
    d.put(0x180, static_cast<float>(std::sin(d.get<float>(0x180) * kRad)));
  float_key(d, tree, "recoil_slide", 0x184);
  angle_key(d, tree, "recoil_twist", 0x188);
  float_key(d, tree, "aim_slerp_freq", 0x18c);
  angle_key(d, tree, "aim_min_pitch", 0x190);
  angle_key(d, tree, "aim_max_pitch", 0x194);
  angle_key(d, tree, "aim_min_yaw", 0x198);
  angle_key(d, tree, "aim_max_yaw", 0x19c);
  float_key(d, tree, "aim_arm_pitch_factor", 0x1a0);
  bool ignore = d.raw[0x1a4] != 0;
  zrdr::get_bool(tree, "aim_ignore_arms", &ignore);
  d.raw[0x1a4] = ignore;
  return true;
}

// The list is always made, empty when the file has no firepoints. Missing
// radius or share read as 0.
void read_firepoints(MechDfn& dfn) {
  zrdr::Node* found = zrdr::find(dfn.reader, "firepoints");
  dfn.firepoints = new std::vector<Firepoint*>();
  if (!found) return;
  zrdr::Node* list = found->list;
  const int32_t pairs = (list[0].i - 1) / 2;
  if (pairs > 0) dfn.firepoints->reserve(pairs);
  for (int32_t i = 0; i < pairs; ++i) {
    zrdr::Node* body = &list[2 + 2 * i];
    float radius = 0, share = 0;
    zrdr::get_float(body, "radius", &radius);
    zrdr::get_float(body, "share", &share);
    const int32_t part = part_id(zrdr::get_string(body, "part"));
    dfn.firepoints->push_back(new Firepoint{list[1 + 2 * i].s, radius, share, part});
  }
}

// The file's "parts" (no shipped file has one) is looked up and ignored: every
// mech gets the same 13 slots, and the side torsos (7, 8) none.
void read_part_slots(MechDfn& dfn) {
  zrdr::Node* node = zrdr::find(dfn.reader, "parts");
  for (int32_t id : {15, 1, 2, 3, 9, 10, 4, 5, 11, 12, 13, 14, 6}) dfn.parts[id].set(id, node);
}

}  // namespace mech3
