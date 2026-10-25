// Probes for test_mix_fog.py (docs/render-lighting.md, the fog mix; docs
// vehicles.md, TARGET's tint): the lighting's fog mix to the device (zrndr
// set_mix_fog_color 0x005680c0, zvid set_mix_fog_device 0x00579950 and
// send_mix_fog 0x00579980) and a mech's TARGET tint (mech_view_tint
// 0x0041b9d0 with the parts' 0x00462040, 0x00463aa0, 0x004643d0) on nodes
// the test lays out as it lays out the emulator's.
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "mech3/mech.h"
#include "mech3/mech_parts.h"
#include "probe_app.h"

namespace {
void send() { probe_app::call("send", {}); }
}  // namespace

extern "C" {

// The mix: `state` in and out, floats: the mix colour's rgb (3), device
// active (as 0/1), device mix (3), sent (3); uint32s: r, g, b, packed,
// doubled, ramp[32]; the format: r_mask, g_mask, r_shift, g_shift,
// b_shift_right. `rgb` in and out.
void probe_mf_set(float* rgb, float* fstate, uint32_t* ustate, const uint32_t* fmt) {
  zrndr::FogColor& c = zrndr::mix_color();
  zrndr::DeviceFog& d = zrndr::device_fog();
  std::memcpy(c.rgb, fstate, 12);
  d.active = fstate[3] != 0.0f;
  std::memcpy(d.mix, fstate + 4, 12);
  c.r = ustate[0], c.g = ustate[1], c.b = ustate[2], c.packed = static_cast<uint16_t>(ustate[3]);
  c.doubled = ustate[4];
  std::memcpy(c.ramp, ustate + 5, sizeof c.ramp);
  zrndr::SoftwareFormat& f = zrndr::software_format();
  f.r_mask = fmt[0], f.g_mask = fmt[1], f.r_shift = static_cast<int32_t>(fmt[2]), f.g_shift = static_cast<int32_t>(fmt[3]),
  f.b_shift_right = static_cast<int32_t>(fmt[4]);
  zrndr::set_mix_fog_color(rgb);
  std::memcpy(fstate, c.rgb, 12);
  std::memcpy(fstate + 4, d.mix, 12);
  ustate[0] = c.r, ustate[1] = c.g, ustate[2] = c.b, ustate[3] = c.packed, ustate[4] = c.doubled;
  std::memcpy(ustate + 5, c.ramp, sizeof c.ramp);
}

// The send: `sent` and `mix` (3 each) in, `sent` out; the device's call recorded.
void probe_mf_send(float* sent, const float* mix) {
  zrndr::DeviceFog& d = zrndr::device_fog();
  std::memcpy(d.sent, sent, 12);
  std::memcpy(d.mix, mix, 12);
  d.send = send;
  zvid::send_mix_fog();
  std::memcpy(sent, d.sent, 12);
  d.send = nullptr;
}

// The mech's tint: `n` nodes (class `cls[i]`, object3d data `data` 0x90 bytes
// each, in and out); the parts (legs 0, 1, arms 0, 1, head, torso, hip, the
// legs' feet 0, 1) by their node index (-1: no node; -2: no part) and
// internal. Returns nothing; the data says it.
void probe_mf_mech(int32_t active, int32_t on, int32_t n, const int32_t* cls, uint8_t* data, const int32_t* part_node,
                   const float* internal) {
  std::vector<zcls::Node> nodes(static_cast<size_t>(n));
  std::vector<zcls::Object3d> datas(static_cast<size_t>(n));
  for (int32_t i = 0; i < n; ++i) {
    std::memcpy(datas[static_cast<size_t>(i)].raw, data + 0x90 * i, 0x90);
    nodes[static_cast<size_t>(i)].put<int32_t>(0x34, cls[i]);
    nodes[static_cast<size_t>(i)].data = &datas[static_cast<size_t>(i)];
  }
  auto node_of = [&](int32_t k) -> mech3::Handle { return part_node[k] >= 0 ? &nodes[static_cast<size_t>(part_node[k])] : nullptr; };
  auto m = std::make_unique<mech3::Mech>();
  m->active = active != 0;
  std::unique_ptr<mech3::Leg> legs[2];
  std::unique_ptr<mech3::Foot> feet[2];
  std::unique_ptr<mech3::Arm> arms[2];
  std::unique_ptr<mech3::Head> head;
  std::unique_ptr<mech3::Torso> torso;
  std::unique_ptr<mech3::Hip> hip;
  for (int i = 0; i < 2; ++i) {
    if (part_node[i] != -2) {
      legs[i] = std::make_unique<mech3::Leg>();
      legs[i]->node = node_of(i), legs[i]->internal = internal[i];
      if (part_node[7 + i] != -2) {
        feet[i] = std::make_unique<mech3::Foot>();
        feet[i]->node = node_of(7 + i), feet[i]->internal = internal[7 + i];
        legs[i]->foot = feet[i].get();
      }
    }
    m->legs[i] = legs[i].get();
    if (part_node[2 + i] != -2) {
      arms[i] = std::make_unique<mech3::Arm>();
      arms[i]->node = node_of(2 + i), arms[i]->internal = internal[2 + i];
    }
    m->arms[i] = arms[i].get();
  }
  if (part_node[4] != -2) head = std::make_unique<mech3::Head>(), head->node = node_of(4), head->internal = internal[4];
  if (part_node[5] != -2) torso = std::make_unique<mech3::Torso>(), torso->node = node_of(5), torso->internal = internal[5];
  if (part_node[6] != -2) hip = std::make_unique<mech3::Hip>(), hip->node = node_of(6), hip->internal = internal[6];
  m->head = head.get(), m->torso = torso.get(), m->hip = hip.get();
  mech3::mech_view_tint(*m, on != 0);
  for (int32_t i = 0; i < n; ++i) std::memcpy(data + 0x90 * i, datas[static_cast<size_t>(i)].raw, 0x90);
  for (auto& l : legs)
    if (l) l->foot = nullptr;
  m->legs[0] = m->legs[1] = nullptr, m->arms[0] = m->arms[1] = nullptr;
  m->head = nullptr, m->torso = nullptr, m->hip = nullptr;
}

}  // extern "C"
