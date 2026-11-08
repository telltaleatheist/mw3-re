#include "pm/gamez/zeffect/zeff_anim_events.h"

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zeffect/zeff_anim_events.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "pm/gamez/zclass/light.h"

#include <cstring>

#pragma STDC FP_CONTRACT OFF

namespace pm::zeff {

using ::zcls::Node;
using ::zeff::ev::get;
using ::zeff::ev::put;

namespace {
float f32(const uint8_t* e, int off) { return get<float>(e, off); }
}  // namespace

// Pirate's Moon 1.0 @ 0x0051b910
int32_t ev_light_state(::zeff::AnimDef& d, uint8_t* e) {
  if (get<int32_t>(e, 0x2c) < 1) {
    const int32_t i = ::zeff::anim_light_ref(d, reinterpret_cast<const char*>(e + 0xc));  // 0x00522280
    put(e, 0x2c, i);
    if (i < 1) return 2;
  }
  ::zeff::CreatedRef& r = d.lights[get<int32_t>(e, 0x2c)];
  Node* n = static_cast<Node*>(r.made);
  Node* world = static_cast<Node*>(::zeff::anim().world);  // [0x008d3f20]
  ::zcls::node_set_active(n, get<int32_t>(e, 0x34));      // 0x004fca50
  if (get<int32_t>(e, 0x34) == 1) {
    if (!r.get<int32_t>(0x28)) {
      ::zcls::world_add_light(world, n);  // 0x00506160
      r.put<int32_t>(0x28, 1);
    }
  } else if (r.get<int32_t>(0x28)) {
    ::zcls::world_remove_light(world, n);  // 0x00506200
    r.put<int32_t>(0x28, 0);
  }
  if (get<int32_t>(e, 0x38)) zcls::light_set_bit4(n);
  else zcls::light_set_bit3(n);
  const uint32_t f = get<uint32_t>(e, 0x30);
  if (f & 1) zcls::light_set_position(n, f32(e, 0x58), f32(e, 0x5c), f32(e, 0x60));
  if (f & 2) {
    float p[3] = {0, 0, 0};
    Node* a = ::zeff::anim_event_node(d, get<int32_t>(e, 0x54), p);
    if (a) {
      ::zcls::node_world_point(a, p);  // 0x004ff3c0
      zcls::light_set_position(n, p[0] + f32(e, 0x58), p[1] + f32(e, 0x5c), p[2] + f32(e, 0x60));
    }
  }
  if (f & 4) zcls::light_set_orientation(n, f32(e, 0x64), f32(e, 0x68), f32(e, 0x6c));
  if (f & 8) zcls::light_set_range(n, f32(e, 0x70), f32(e, 0x74));
  if (f & 0x10) zcls::light_set_color(n, f32(e, 0x78), f32(e, 0x7c), f32(e, 0x80));
  if (f & 0x800) zcls::light_set_ambient_color(n, f32(e, 0x84), f32(e, 0x88), f32(e, 0x8c));
  if (f & 0x20) zcls::light_set_ambient(n, f32(e, 0x90));
  if (f & 0x40) zcls::light_set_diffuse(n, f32(e, 0x94));
  if (f & 0x1000) zcls::light_set_own_colors(n, get<int32_t>(e, 0x50));
  if (f & 0x80) zcls::light_set_bit2(n, get<int32_t>(e, 0x3c));
  if (f & 0x100) zcls::light_set_ambient_only(n, get<int32_t>(e, 0x40));
  if (f & 0x200) zcls::light_set_diffuse_only(n, get<int32_t>(e, 0x44));
  if (f & 0x2000) zcls::light_set_light_map(n, get<int32_t>(e, 0x48));
  if (f & 0x400) zcls::light_set_directional(n, get<int32_t>(e, 0x4c));
  return 2;
}

// Pirate's Moon 1.0 @ 0x0051f900
int32_t ev_stop_animation(::zeff::AnimDef& d, uint8_t* e) {
  const char* name = reinterpret_cast<const char*>(e + 0xc);
  if (get<int16_t>(e, 0x2c) < 0 && get<int16_t>(e, 0x2e) < 0) {
    for (int i = 0; i < d.count(0x112); ++i) {
      const auto* r = reinterpret_cast<const char*>(d.anim_refs[i].raw);
      if (std::strcmp(r, name) == 0 || std::strcmp(r + 0x20, name) == 0) put<int16_t>(e, 0x2c, static_cast<int16_t>(i));
    }
    if (get<int16_t>(e, 0x2c) < 0) {
      ::zeff::AnimState& a = ::zeff::anim();
      for (int j = 0; j < a.def_count(); ++j)
        if (std::strcmp(a.defs[j].name(), name) == 0) {
          put<int16_t>(e, 0x2e, static_cast<int16_t>(j));
          break;
        }
    }
  }
  const int16_t r = get<int16_t>(e, 0x2c);
  if (r >= 0) {
    ::zeff::anim_stop(d.anim_refs[r].ref);  // 0x0051f830
  } else if (get<int16_t>(e, 0x2e) > 0) {
    ::zeff::anim_stop(&::zeff::anim().defs[get<int16_t>(e, 0x2e)]);
  }
  return 2;
}

// Pirate's Moon 1.0 @ 0x0051e2c0
int32_t ev_object_motion_script(::zeff::AnimDef& d, ::zeff::SeqDef& s, uint8_t* e) {
  const int32_t ni = get<int32_t>(e, 0xc);
  if (ni < 0) return 2;
  Node* n = d.nodes[ni].node;
  if (!n) return 2;
  const int32_t si = get<int32_t>(e, 0x10);
  ::zeff::AnimState& a = ::zeff::anim();
  if (si < 0 || si >= a.pm_script_count) return 2;  // ours: the exe takes any index past the list
  const ::zeff::SiScript& sc = a.pm_scripts[si];
  if (s.raw[0x20] == 0) put<int32_t>(e, 0x1c, 0), put<uint32_t>(e, 0x18, 0), put<int32_t>(e, 0x20, 0);
  float time = s.get<float>(0x28) - a.time_left();
  const ::zeff::FrameSource src{sc.data, sc.get<int32_t>(0x14), sc.get<int32_t>(0xc)};
  if (get<int32_t>(e, 0x1c) >= src.limit) return 2;
  int32_t result = 1;
  do {
    const uint8_t* frame = sc.data + get<int32_t>(e, 0x1c);
    a.time_left() = a.time_left() - ::zeff::anim_play_frame(s, e, n, frame, &time, src);
    if (time < s.get<float>(0x28) && !::zeff::anim_next_frame(s, e, frame, src.limit)) result = 2;
  } while (time < s.get<float>(0x28) && result != 2);
  return result;
}

}  // namespace pm::zeff
