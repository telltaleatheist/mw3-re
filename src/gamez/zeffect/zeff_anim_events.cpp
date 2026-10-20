#include "gamez/zeffect/zeff_anim_events.h"

#include <cmath>
#include <cstring>

#pragma STDC FP_CONTRACT OFF

#include "gamez/zclass/camera.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/light.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/sound.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zrender/zrndr_fog.h"

namespace zeff {

using zcls::Node;
using ev::get;
using ev::put;

namespace {
AnimState& A() { return anim(); }
Node* world() { return static_cast<Node*>(A().world); }
float& left() { return A().time_left(); }
float f32(const uint8_t* e, int off) { return get<float>(e, off); }
float& fref(uint8_t* e, int off) { return *ev::fp(e, off); }
float& sf(SeqDef& s, int off) { return *s.floats(off); }
float& df(AnimDef& d, int off) { return *d.floats(off); }
const AnimRunHooks& H() { return anim_run_hooks(); }

// A node ref by index (the def's +0x118 list): entry 0 is empty.
Node* ref(AnimDef& d, int32_t i) { return d.nodes[i].node; }
// The handlers' common node pick: refs 1.., -200 the node the anim was
// started at (with its offset in `off` when given).
Node* ref_or_at(AnimDef& d, int32_t i, float* off = nullptr) {
  if (i > 0) return ref(d, i);
  if (i == -200) {
    if (off) std::memcpy(off, d.raw + 0x78, 12);
    return d.at_node;
  }
  return nullptr;
}

// The time this event may use this frame: what is left, less what the event
// ran past its end (+end).
float step(SeqDef& s, float end) {
  return sf(s, 0x28) <= end ? left() : left() - (sf(s, 0x28) - end);
}

bool not_above(float a, float b) { return !(a > b); }

// The opacity a model is drawn with (+0x24, flag 8 at +0x08 while it is
// under 1e-5..1).
void model_opacity(zmod::Model* m, float v, bool add) {
  uint32_t f;
  std::memcpy(&f, m->header + 8, 4);
  f |= 8;
  std::memcpy(m->header + 8, &f, 4);
  float o;
  std::memcpy(&o, m->header + 0x24, 4);
  o = add ? v + o : v;
  std::memcpy(m->header + 0x24, &o, 4);
  if (!not_above(o, 1.0f)) {
    o = 1.0f;
    std::memcpy(m->header + 0x24, &o, 4);
  } else if (o < 1e-05f) {
    f &= ~8u;
    std::memcpy(m->header + 8, &f, 4);
  }
}

bool streq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

// The 3x3 transposed, translation 0 (0x005047fa / 0x0050497e copy it so).
zmth::Mat34 transposed(const zmth::Mat34& m) {
  zmth::Mat34 r{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) r.m[3 * i + j] = m.m[3 * j + i];
  return r;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00507340. The squared distance from the node to the
// player position (block +0x2c); 0 when the node has no position.
float distance2_to_player(Node* node) {
  float p[3];
  if (zcls::node_world_position(node, p) != 0) return 0.0f;
  const float* pl = A().block.floats(0x2c);
  const float dx = p[0] - pl[0], dy = p[1] - pl[1], dz = p[2] - pl[2];
  return (dx * dx + dy * dy) + dz * dz;
}

// Mech3.exe 1.2 @ 0x00503100. A static sound, at a node (+0x0e) plus an
// offset (+0x10), or plain (volume 1).
int32_t ev_sound(AnimDef& d, uint8_t* e) {
  if (!A().sounds_on) return 2;
  void* sound = d.static_sounds[get<int16_t>(e, 0xc)].sound;
  const int16_t ni = get<int16_t>(e, 0xe);
  if (ni > 0 && ref(d, ni)) {
    float p[3];
    zcls::node_world_position(ref(d, ni), p);
    p[0] = f32(e, 0x10) + p[0], p[1] = f32(e, 0x14) + p[1], p[2] = f32(e, 0x18) + p[2];
    if (H().play_sound_at) H().play_sound_at(H().ctx, sound, p, 1.0f);
    return 2;
  }
  if (H().play_sound) H().play_sound(H().ctx, sound, 1.0f);
  return 2;
}

// Mech3.exe 1.2 @ 0x00503270. A dynamic sound node: on (and into the world)
// or off; with sounds off it is always switched off. Position absolute (1)
// or from a node (2).
int32_t ev_sound_node(AnimDef& d, uint8_t* e) {
  if (get<int32_t>(e, 0x2c) < 1) {
    const int32_t i = anim_sound_ref(d, reinterpret_cast<const char*>(e + 0xc));
    put(e, 0x2c, i);
    if (i < 1) return 2;
  }
  CreatedRef& r = d.sounds[get<int32_t>(e, 0x2c)];
  Node* n = static_cast<Node*>(r.made);
  if (!A().sounds_on) put<int32_t>(e, 0x34, 0);
  zcls::node_set_active(n, get<int32_t>(e, 0x34));
  if (get<int32_t>(e, 0x34) == 1) {
    if (!r.get<int32_t>(0x28)) {
      zcls::world_add_sound(world(), n);
      r.put<int32_t>(0x28, 1);
    }
  } else if (r.get<int32_t>(0x28)) {
    zcls::world_remove_sound(world(), n);
    r.put<int32_t>(0x28, 0);
  }
  const uint32_t f = get<uint32_t>(e, 0x30);
  if (f & 1) zcls::sound_set_translation(n, f32(e, 0x3c), f32(e, 0x40), f32(e, 0x44));
  const int32_t at = get<int32_t>(e, 0x38);
  if ((f & 2) && at > 0 && ref(d, at)) {
    float p[3];
    zcls::node_world_position(ref(d, at), p);
    zcls::sound_set_translation(n, f32(e, 0x3c) + p[0], f32(e, 0x40) + p[1], f32(e, 0x44) + p[2]);
  }
  return 2;
}

// Mech3.exe 1.2 @ 0x00503390. A light (+0x0c name, its ref cached at +0x2c):
// active (+0x34, joining or leaving the world), type (+0x38: 0 bit 3, else
// bit 4), then by the flags at +0x30 (mech3ax LightFlags) position (1 +0x50,
// 2 at node +0x4c with it as the offset), orientation (4), range (8), colour
// (0x10), ambient (0x20), diffuse (0x40), directional (0x80), saturated
// (0x100), subdivide (0x200), static (0x400).
int32_t ev_light_state(AnimDef& d, uint8_t* e) {
  if (get<int32_t>(e, 0x2c) < 1) {
    const int32_t i = anim_light_ref(d, reinterpret_cast<const char*>(e + 0xc));
    put(e, 0x2c, i);
    if (i < 1) return 2;
  }
  CreatedRef& r = d.lights[get<int32_t>(e, 0x2c)];
  Node* n = static_cast<Node*>(r.made);
  zcls::node_set_active(n, get<int32_t>(e, 0x34));
  if (get<int32_t>(e, 0x34) == 1) {
    if (!r.get<int32_t>(0x28)) {
      zcls::world_add_light(world(), n);
      r.put<int32_t>(0x28, 1);
    }
  } else if (r.get<int32_t>(0x28)) {
    zcls::world_remove_light(world(), n);
    r.put<int32_t>(0x28, 0);
  }
  if (get<int32_t>(e, 0x38)) zcls::light_set_bit4(n);
  else zcls::light_set_bit3(n);
  const uint32_t f = get<uint32_t>(e, 0x30);
  if (f & 1) zcls::light_set_position(n, f32(e, 0x50), f32(e, 0x54), f32(e, 0x58));
  if (f & 2) {
    float p[3] = {0, 0, 0};
    const int32_t at = get<int32_t>(e, 0x4c);
    Node* a = ref_or_at(d, at, p);
    if (at > 0 && !a) a = nullptr;  // an empty ref is not -200
    if (a) {
      zcls::node_world_point(a, p);
      zcls::light_set_position(n, f32(e, 0x50) + p[0], f32(e, 0x54) + p[1], f32(e, 0x58) + p[2]);
    }
  }
  if (f & 4) zcls::light_set_orientation(n, f32(e, 0x5c), f32(e, 0x60), f32(e, 0x64));
  if (f & 8) zcls::light_set_range(n, f32(e, 0x68), f32(e, 0x6c));
  if (f & 0x10) zcls::light_set_color(n, f32(e, 0x70), f32(e, 0x74), f32(e, 0x78));
  if (f & 0x20) zcls::light_set_ambient(n, f32(e, 0x7c));
  if (f & 0x40) zcls::light_set_diffuse(n, f32(e, 0x80));
  if (f & 0x80) zcls::light_set_bit2(n, get<int32_t>(e, 0x3c));
  if (f & 0x100) zcls::light_set_ambient_only(n, get<int32_t>(e, 0x40));
  if (f & 0x200) zcls::light_set_diffuse_only(n, get<int32_t>(e, 0x44));
  if (f & 0x400) zcls::light_set_directional(n, get<int32_t>(e, 0x48));
  return 2;
}

// Mech3.exe 1.2 @ 0x00503910. ECX def, EDX seq, the event on the stack. The
// range and colour move by their rates (+0x40 range, +0x60 colour, their
// own rates +0x38 / +0x54) until +0x6c. A near range below 0 becomes 1; a
// far one below the near one becomes near + 1. Colour stays in 0..1.
int32_t ev_light_animation(AnimDef& d, SeqDef& s, uint8_t* e) {
  if (get<int32_t>(e, 0x2c) < 1) {
    const int32_t i = anim_light_ref(d, reinterpret_cast<const char*>(e + 0xc));
    put(e, 0x2c, i);
    if (i < 1) return 2;
  }
  Node* n = static_cast<Node*>(d.lights[get<int32_t>(e, 0x2c)].made);
  if (s.raw[0x20] == 0) {
    fref(e, 0x40) = f32(e, 0x30), fref(e, 0x44) = f32(e, 0x34);
    std::memcpy(e + 0x60, e + 0x48, 12);
  }
  const float t = not_above(sf(s, 0x28), f32(e, 0x6c)) ? left() : left() - (sf(s, 0x28) - f32(e, 0x6c));
  float lo, hi;
  zcls::light_get_range(n, &lo, &hi);
  lo = t * f32(e, 0x40) + lo;
  hi = t * f32(e, 0x44) + hi;
  if (lo < 0.0f) lo = 1.0f;
  if (hi < lo) hi = lo - -1.0f;
  fref(e, 0x40) = f32(e, 0x38) * t + f32(e, 0x40);
  fref(e, 0x44) = f32(e, 0x3c) * t + f32(e, 0x44);
  zcls::light_set_range(n, lo, hi);
  float c[3];
  zcls::light_get_color(n, &c[0], &c[1], &c[2]);
  for (int i = 0; i < 3; ++i) {
    c[i] = t * f32(e, 0x60 + 4 * i) + c[i];
    fref(e, 0x60 + 4 * i) = f32(e, 0x54 + 4 * i) * t + f32(e, 0x60 + 4 * i);
  }
  for (float& v : c) {
    if (!not_above(v, 1.0f)) v = 1.0f;
    else if (v < 0.0f) v = 0.0f;
  }
  zcls::light_set_color(n, c[0], c[1], c[2]);
  left() = left() - t;
  return sf(s, 0x28) < f32(e, 0x6c) ? 1 : 2;
}

// Mech3.exe 1.2 @ 0x005044d0. Node ref +0x10 (-100 the def's node) on/off.
int32_t ev_object_active(AnimDef& d, uint8_t* e) {
  const int16_t i = get<int16_t>(e, 0x10);
  if (i >= 0 && ref(d, i)) {
    zcls::node_set_active(ref(d, i), get<int32_t>(e, 0xc));
    return 2;
  }
  if (i == -100) zcls::node_set_active(d.node, get<int32_t>(e, 0xc));
  return 2;
}

// Mech3.exe 1.2 @ 0x00504380. Node +0x1c to +0x10, relative to node +0x1e's
// frame (-200 the start node with its offset); flag 1 adds.
int32_t ev_object_translate(AnimDef& d, uint8_t* e) {
  float p[3] = {0, 0, 0};
  Node* at = ref_or_at(d, get<int16_t>(e, 0x1e), p);
  if (at) zcls::node_world_point(at, p);
  p[0] = f32(e, 0x10) + p[0], p[1] = f32(e, 0x14) + p[1], p[2] = f32(e, 0x18) + p[2];
  Node* n = ref(d, get<int16_t>(e, 0x1c));
  if (!n) return 2;
  const bool add = get<uint32_t>(e, 0xc) & 1;
  if (n->cls() == 1) {
    add ? zcls::camera_add_position(n, p[0], p[1], p[2]) : zcls::camera_set_position(n, p[0], p[1], p[2]);
  } else if (n->cls() == 5) {
    add ? zcls::object3d_add_translation(n, p[0], p[1], p[2]) : zcls::object3d_set_translation(n, p[0], p[1], p[2]);
  }
  return 2;
}

// Mech3.exe 1.2 @ 0x00504350
int32_t ev_object_scale(AnimDef& d, uint8_t* e) {
  if (Node* n = ref(d, get<int16_t>(e, 0x18))) zcls::object3d_set_scale(n, f32(e, 0xc), f32(e, 0x10), f32(e, 0x14));
  return 2;
}

// Mech3.exe 1.2 @ 0x00504180. Node +0x1c turned to +0x10: added to its own
// (1), to node +0x1e's rotation (2) or its world rotation (4), else set. A
// camera takes 1 or a plain set.
int32_t ev_object_rotate(AnimDef& d, uint8_t* e) {
  Node* n = ref(d, get<int16_t>(e, 0x1c));
  if (!n) return 2;
  const uint32_t f = get<uint32_t>(e, 0xc);
  const float x = f32(e, 0x10), y = f32(e, 0x14), z = f32(e, 0x18);
  if (n->cls() == 1) {
    (f & 1) ? zcls::camera_add_rotation(n, x, y, z) : zcls::camera_set_rotation(n, x, y, z);
    return 2;
  }
  if (n->cls() != 5) return 2;
  if (f & 6) {
    float base[3] = {0, 0, 0};
    Node* from = ref_or_at(d, get<int16_t>(e, 0x1e));
    if (from) {
      if (f & 2) {
        zcls::object3d_get_rotation(from, &base[0], &base[1], &base[2]);
      } else {
        zmth::Mat34 m;
        zmth::push(&m);
        zmth::load_identity();
        zcls::node_to_world(from, 1);
        zmth::matrix_to_euler(m, base);
        zmth::pop();
      }
    }
    zcls::object3d_set_rotation(n, x + base[0], y + base[1], z + base[2]);
    return 2;
  }
  (f & 1) ? zcls::object3d_add_rotation(n, x, y, z) : zcls::object3d_set_rotation(n, x, y, z);
  return 2;
}

// Mech3.exe 1.2 @ 0x00505160. The ground hit nearest the point, among those
// less than 10 above it (the first one is taken whatever its height).
const GroundHit* ground_below(const float pos[3], float reach, GroundHits& hits) {
  hits.count = 0;
  if (H().ground) H().ground(H().ctx, world(), pos[0], reach + pos[1], pos[2], &hits);
  int32_t best = -1;
  float bd = reach;
  for (int32_t i = 0; i < hits.count; ++i) {
    const float dd = std::fabs(pos[1] - hits.hit[i].y);
    if (best < 0) {
      best = i, bd = dd;
    } else if (dd < bd && hits.hit[i].y - pos[1] < 10.0f) {
      best = i, bd = dd;
    }
  }
  return best < 0 ? nullptr : &hits.hit[best];
}

// Mech3.exe 1.2 @ 0x00504520. ECX def, EDX seq, the event on the stack.
// Object motion (mech3ax ObjectMotion): node +0x10; flags +0x0c: 1 gravity
// (+0x18, along world y, or the node's own frame with 0x2000), 2 add the
// anim's start vector in the parent's frame, 4 velocity +0x40 / accel +0x4c,
// 8 a random direction and speeds (+0x20..+0x3c ranges), 0x20 spin +0x88
// (its rate +0x94), 0x40 roll with the motion, 0x80 tumble, 0x100 scale
// (+0xac, rate +0xb8), 0x200 fade (+0x1c), 0x400 a set run time (+0x148),
// 0x800 a sequence per surface on landing, 0x1000 a sound per surface,
// 0x4000 no ground. Falling 15 s with no ground under it ends it.
int32_t ev_object_motion(AnimDef& d, SeqDef& s, uint8_t* e) {
  int32_t result = 1;
  const int16_t ni = get<int16_t>(e, 0x10);
  if (ni < 0) return 2;
  Node* n = ref(d, ni);
  if (!n) return 2;
  AnimState& a = A();
  float dx = 0, dy = 0, dz = 0;  // this step's move (the exe leaves them unset without 0xc)
  if (s.raw[0x20] == 0) {
    uint32_t f = get<uint32_t>(e, 0xc);
    if (!(f & 0x400)) put<uint32_t>(e, 0x148, 0);
    if (f & 0x100) std::memcpy(e + 0xc4, e + 0xac, 12);
    if (f & 0x20) std::memcpy(e + 0xa0, e + 0x88, 12);
    if (f & 4) std::memcpy(e + 0x58, e + 0x40, 12), std::memcpy(e + 0x64, e + 0x4c, 12);
    if (f & 8) {
      const auto rnd = [&a] {
        const float r = a.random[a.random_index];
        a.random_index = (a.random_index + 1) % 200;
        return r;
      };
      const float r0 = rnd();
      const float yaw = r0 * (f32(e, 0x24) - f32(e, 0x20)) + f32(e, 0x20);
      const float r1 = rnd();
      const float pitch = (f32(e, 0x2c) - f32(e, 0x28)) * r1 + f32(e, 0x28);
      const float r2 = rnd();
      const float speed = (f32(e, 0x34) - f32(e, 0x30)) * r2 + f32(e, 0x30);
      const float r3 = rnd();
      const float accel = (f32(e, 0x3c) - f32(e, 0x38)) * r3 + f32(e, 0x38);
      // 0x00522210: (-sin, 0, -cos) of the yaw in radians.
      const float rad = static_cast<float>(yaw * 0.01745329251994);
      const float dir_x = -static_cast<float>(std::sin(static_cast<double>(rad)));
      const float dir_z = -static_cast<float>(std::cos(static_cast<double>(rad)));
      const float up = pitch * 0.011111111f;
      fref(e, 0x74) = up;
      const float flat = up < 0.0 ? up - -1.0f : 1.0f - up;
      const float vx = flat * dir_x, vz = flat * dir_z;
      fref(e, 0x70) = vx, fref(e, 0x78) = vz;
      fref(e, 0x40) = speed * vx, fref(e, 0x44) = speed * up, fref(e, 0x48) = speed * vz;
      fref(e, 0x4c) = accel * vx, fref(e, 0x50) = accel * up, fref(e, 0x54) = accel * vz;
      std::memcpy(e + 0x58, e + 0x40, 12), std::memcpy(e + 0x64, e + 0x4c, 12);
    }
    if (f & 0xc0) {
      // An overlapping copy: +0x8c gets the +0x84 just written.
      fref(e, 0x84) = f32(e, 0x7c), fref(e, 0x88) = f32(e, 0x80), fref(e, 0x8c) = f32(e, 0x84);
    }
    f = get<uint32_t>(e, 0xc);
    bool frame = (f & 2) && (d.get<uint32_t>(0x94) & 0x80);
    if (f & 1) {
      if (f & 0x2000) frame = true;
      else fref(e, 0x68) = f32(e, 0x18) + f32(e, 0x68);
    }
    if (frame) {
      zmth::Mat34 m;
      zmth::push(&m);
      if ((get<uint32_t>(e, 0xc) & 2) && (d.get<uint32_t>(0x94) & 0x80) && n->parent_count() == 1) {
        zmth::load_identity();
        zcls::node_to_world(n->parents[0], 2);
        const zmth::Mat34 r = transposed(m);  // the inverse rotation: world to the frame
        zmth::load(r);
        zmth::Vec3 v{df(d, 0xb8), df(d, 0xbc), df(d, 0xc0)};
        zmth::transform_points(&v, 1);
        fref(e, 0x58) = v.x + f32(e, 0x58), fref(e, 0x5c) = v.y + f32(e, 0x5c), fref(e, 0x60) = v.z + f32(e, 0x60);
      }
      if (get<uint32_t>(e, 0xc) & 1) {
        zmth::Vec3 g{0.0f, f32(e, 0x18), 0.0f};
        zmth::load(m);
        zmth::load_identity();
        zcls::node_to_world(n, 2);
        const zmth::Mat34 r = transposed(m);  // the inverse rotation: world to the frame
        zmth::load(r);
        zmth::transform_points(&g, 1);
        fref(e, 0x64) = g.x + f32(e, 0x64), fref(e, 0x68) = g.y + f32(e, 0x68), fref(e, 0x6c) = g.z + f32(e, 0x6c);
      }
      zmth::pop();
    }
  }
  uint32_t f = get<uint32_t>(e, 0xc);
  const float t = ((f & 0x400) && !not_above(sf(s, 0x28), f32(e, 0x148)))
                      ? left() - (sf(s, 0x28) - f32(e, 0x148))
                      : left();
  if (f & 0xc) {
    dx = t * f32(e, 0x58);
    bool landed = false;
    const GroundHit* hit = nullptr;
    dz = t * f32(e, 0x60);
    dy = t * f32(e, 0x5c);
    float pos[3];
    if ((f & 1) && !(f & 0x4000)) {
      zcls::node_world_position(n, pos);
      int32_t saved;
      zcls::node_get_flag8(d.node, &saved);
      zcls::node_set_flag8(d.node, 0);
      GroundHits hits;
      hit = ground_below(pos, std::fabs(dy), hits);
      static GroundHit kept;  // the hit outlives `hits` below
      if (hit) kept = *hit, hit = &kept;
      zcls::node_set_flag8(d.node, saved);
      if (dy < 0.0) {
        if (hit) {
          if (pos[1] + dy < hit->y) {
            landed = true;
            if (std::fabs(f32(e, 0x58)) < 0.10000000149011612 && std::fabs(f32(e, 0x60)) < 0.10000000149011612)
              dy = hit->y - pos[1];
            else
              dy = (std::fabs(dy * 0.5f) + hit->y) - pos[1];
          }
        } else if (!(get<uint32_t>(e, 0xc) & 0x400)) {
          fref(e, 0x148) = t + f32(e, 0x148);
          if (!not_above(f32(e, 0x148), 15.0f)) result = 2, landed = true;
        }
      }
    }
    if (n->cls() == 5) zcls::object3d_add_translation(n, dx, dy, dz);
    else if (n->cls() == 1) zcls::camera_add_position(n, dx, dy, dz);
    if (landed) {
      const float v2 = (f32(e, 0x58) * f32(e, 0x58) + f32(e, 0x5c) * f32(e, 0x5c)) + f32(e, 0x60) * f32(e, 0x60);
      const float a2 = (f32(e, 0x64) * f32(e, 0x64) + f32(e, 0x68) * f32(e, 0x68)) + f32(e, 0x6c) * f32(e, 0x6c);
      if (v2 < a2) {
        result = 2;
      } else {
        fref(e, 0x58) = f32(e, 0x58) * 0.19999998807907104f;
        fref(e, 0x60) = f32(e, 0x60) * 0.19999998807907104f;
        fref(e, 0x5c) = f32(e, 0x5c) * 0.19999998807907104f;
      }
      const int32_t surf = (hit && hit->has_surface) ? hit->surface : 0;
      const int32_t k = surf == 1 ? 1 : surf == 4 ? 2 : 0;
      if (get<uint32_t>(e, 0xc) & 0x800) {
        int32_t j = k;
        while (get<int16_t>(e, 0xf0 + 0x28 * j) < 0) {
          const char* name = reinterpret_cast<const char*>(e + 0xd0 + 0x28 * j);
          if (name[0]) {
            for (int32_t q = 0; q < d.count(0x108); ++q)
              if (streq(d.seqs[q].name(), name)) {
                put<int16_t>(e, 0xf0 + 0x28 * j, static_cast<int16_t>(q));
                break;
              }
          }
          if (get<int16_t>(e, 0xf0 + 0x28 * j) < 0) {
            if (j == 0) break;
            j = 0;
          }
        }
        const int16_t q = get<int16_t>(e, 0xf0 + 0x28 * j);
        if (q >= 0 && d.seqs[q].raw[0x20] == 3) d.seqs[q].raw[0x20] = 0;
      }
      if ((get<uint32_t>(e, 0xc) & 0x1000) && a.sounds_on) {
        int32_t j = -1;
        if (get<int16_t>(e, 0xf2 + 0x28 * k) > 0) j = k;
        else if (k != 0 && get<int16_t>(e, 0xf2) > 0) j = 0;
        if (j >= 0) {
          const float speed = static_cast<float>(
              std::sqrt(static_cast<double>((f32(e, 0x60) * f32(e, 0x60) + f32(e, 0x5c) * f32(e, 0x5c)) +
                                            f32(e, 0x58) * f32(e, 0x58))));
          const float scale = f32(e, 0xf4 + 0x28 * j) < 0.0f ? f32(e, 0x18) * 10.0f : f32(e, 0xf4 + 0x28 * j);
          const float vol = not_above(speed, scale) ? speed / scale : 1.0f;
          void* sound = d.static_sounds[get<int16_t>(e, 0xf2 + 0x28 * j)].sound;
          if (H().play_sound_at) H().play_sound_at(H().ctx, sound, pos, vol);
        }
      }
    }
    fref(e, 0x58) = f32(e, 0x64) * t + f32(e, 0x58);
    fref(e, 0x5c) = f32(e, 0x68) * t + f32(e, 0x5c);
    fref(e, 0x60) = f32(e, 0x6c) * t + f32(e, 0x60);
  }
  f = get<uint32_t>(e, 0xc);
  if (f & 0x20) {
    const float x = f32(e, 0xa0) * t, y = f32(e, 0xa4) * t, z = f32(e, 0xa8) * t;
    if (n->cls() == 5) zcls::object3d_add_rotation(n, x, y, z);
    else if (n->cls() == 1) zcls::camera_add_rotation(n, x, y, z);
    for (int i = 0; i < 3; ++i) fref(e, 0xa0 + 4 * i) = f32(e, 0x94 + 4 * i) * t + f32(e, 0xa0 + 4 * i);
  }
  if (get<uint32_t>(e, 0xc) & 0x40)  // no class check: a camera's data would be written too
    zcls::object3d_add_rotation(n, dz * f32(e, 0x84), 0.0f, -(dx * f32(e, 0x84)));
  if (get<uint32_t>(e, 0xc) & 0x80) {
    const float x = f32(e, 0x78) * t * f32(e, 0x84);
    const float z = -(f32(e, 0x70) * t * f32(e, 0x84));
    if (n->cls() == 1) zcls::camera_add_rotation(n, x, 0.0f, z);
    else if (n->cls() == 5) zcls::object3d_add_rotation(n, x, 0.0f, z);
    fref(e, 0x84) = f32(e, 0x80) * t + f32(e, 0x84);
  }
  if (get<uint32_t>(e, 0xc) & 0x100) {
    float sc[3];
    zcls::object3d_get_scale(n, &sc[0], &sc[1], &sc[2]);
    for (int i = 0; i < 3; ++i) {
      sc[i] = t * f32(e, 0xc4 + 4 * i) + sc[i];
      if (sc[i] < 0.0010000000474974513f) sc[i] = 0.001f;
      fref(e, 0xc4 + 4 * i) = f32(e, 0xb8 + 4 * i) * t + f32(e, 0xc4 + 4 * i);
    }
    zcls::object3d_set_scale(n, sc[0], sc[1], sc[2]);
  }
  if ((get<uint32_t>(e, 0xc) & 0x200) && n->model) model_opacity(n->model, f32(e, 0x1c) * t, true);
  left() = left() - t;
  if ((get<uint32_t>(e, 0xc) & 0x400) && !(sf(s, 0x28) < f32(e, 0x148))) return 2;
  return result;
}

// Mech3.exe 1.2 @ 0x00505200. From +0x20 / +0x44 / +0x68 / +0x14 (position,
// rotation, scale, opacity, by flags 1 / 2 / 4 / 8) at their rates to the
// ends at +0x8c. The end rotation is +0x50 for an object3d but the start
// one (+0x44) again for a camera.
int32_t ev_object_motion_from_to(AnimDef& d, SeqDef& s, uint8_t* e) {
  const int32_t ni = get<int32_t>(e, 0x10);
  if (ni < 0) return 2;
  Node* n = ref(d, ni);
  const uint32_t f = get<uint32_t>(e, 0xc);
  if (s.raw[0x20] == 0 && n) {
    if ((f & 4) && n->cls() == 5) zcls::object3d_set_scale(n, f32(e, 0x68), f32(e, 0x6c), f32(e, 0x70));
    if (f & 2) {
      if (n->cls() == 1) zcls::camera_set_rotation(n, f32(e, 0x44), f32(e, 0x48), f32(e, 0x4c));
      else if (n->cls() == 5) zcls::object3d_set_rotation(n, f32(e, 0x44), f32(e, 0x48), f32(e, 0x4c));
    }
    if (f & 1) {
      if (n->cls() == 1) zcls::camera_set_position(n, f32(e, 0x20), f32(e, 0x24), f32(e, 0x28));
      else if (n->cls() == 5) zcls::object3d_set_translation(n, f32(e, 0x20), f32(e, 0x24), f32(e, 0x28));
    }
    if ((f & 8) && n->model) model_opacity(n->model, f32(e, 0x14), false);
  }
  const float t = step(s, f32(e, 0x8c));
  if (n) {
    if (f & 1) {
      const float x = f32(e, 0x38) * t, y = f32(e, 0x3c) * t, z = f32(e, 0x40) * t;
      if (n->cls() == 1) zcls::camera_add_position(n, x, y, z);
      else if (n->cls() == 5) zcls::object3d_add_translation(n, x, y, z);
    }
    if (f & 2) {
      const float x = f32(e, 0x5c) * t, y = f32(e, 0x60) * t, z = f32(e, 0x64) * t;
      if (n->cls() == 1) zcls::camera_add_rotation(n, x, y, z);
      else if (n->cls() == 5) zcls::object3d_add_rotation(n, x, y, z);
    }
    if ((f & 4) && n->cls() == 5) {
      float sc[3];
      zcls::object3d_get_scale(n, &sc[0], &sc[1], &sc[2]);
      for (int i = 0; i < 3; ++i) sc[i] = f32(e, 0x80 + 4 * i) * t + sc[i];
      for (float& v : sc)
        if (v < 0.0010000000474974513f) v = 0.001f;
      zcls::object3d_set_scale(n, sc[0], sc[1], sc[2]);
    }
    if ((f & 8) && n->model) model_opacity(n->model, f32(e, 0x1c) * t, true);
  }
  left() = left() - t;
  if (!(f32(e, 0x8c) <= sf(s, 0x28))) return 1;
  if (n) {
    if ((f & 4) && n->cls() == 5) zcls::object3d_set_scale(n, f32(e, 0x74), f32(e, 0x78), f32(e, 0x7c));
    if (f & 2) {
      if (n->cls() == 1) zcls::camera_set_rotation(n, f32(e, 0x44), f32(e, 0x48), f32(e, 0x4c));
      else if (n->cls() == 5) zcls::object3d_set_rotation(n, f32(e, 0x50), f32(e, 0x54), f32(e, 0x58));
    }
    if (f & 1) {
      if (n->cls() == 1) zcls::camera_set_position(n, f32(e, 0x2c), f32(e, 0x30), f32(e, 0x34));
      else if (n->cls() == 5) zcls::object3d_set_translation(n, f32(e, 0x2c), f32(e, 0x30), f32(e, 0x34));
    }
    if ((f & 8) && n->model) model_opacity(n->model, f32(e, 0x18), false);
  }
  return 2;
}

namespace {
// 0x005056b0 (ECX seq, EDX event, stack frame): on to the next key frame;
// 1 while it is inside the event.
int32_t next_frame(SeqDef& s, uint8_t* e, const uint8_t* frame) {
  put<int32_t>(e, 0x20, get<int32_t>(e, 0x20) + 1);
  int32_t at = get<int32_t>(e, 0x1c) + 0xc;
  if (frame[0] & 1) at += 0x4c;
  if (frame[0] & 2) at += 0x4c;
  if (frame[0] & 4) at += 0x4c;
  put(e, 0x1c, at);
  put<float>(e, 0x18, 0.0f);
  return at < get<int32_t>(s.cur, 4) ? 1 : 0;
}

// A cubic (+0x14 set) or linear track at t: c0 + t c1 (+ t^2 c2 + t^3 c3).
void track(const uint8_t* p, bool cubic, float t, float out[3]) {
  if (cubic) {
    for (int i = 0; i < 3; ++i) {
      const int b = 0x10 * i;
      out[i] = ((f32(p, b + 0x1c) * t + f32(p, b + 0x18)) * t + f32(p, b + 0x14)) * t + f32(p, b + 0x10);
    }
  } else {
    for (int i = 0; i < 3; ++i) out[i] = f32(p, 0x10 + 4 * i) * t + f32(p, 4 * i);
  }
}

// 0x00505710 (ECX seq, EDX event, stack node, frame, &time): plays one key
// frame up to the sequence's time; returns the time it used. A frame before
// its start returns the cursor's old value (sic) and moves it to now.
float play_frame(SeqDef& s, uint8_t* e, Node* n, const uint8_t* frame, float* time) {
  float extra = 0.0f;
  if (sf(s, 0x28) < f32(frame, 4)) {
    const float old = *time;
    *time = sf(s, 0x28);
    return old;
  }
  if (*time < f32(frame, 4)) {
    extra = f32(frame, 4) - *time;
    *time = f32(frame, 4);
  }
  float until;
  if (!not_above(sf(s, 0x28), f32(frame, 8))) {
    const uint32_t saved18 = get<uint32_t>(e, 0x18);
    const int32_t saved1c = get<int32_t>(e, 0x1c);
    if (next_frame(s, e, frame) && get<uint32_t>(e, get<int32_t>(e, 0x1c)) == get<uint32_t>(frame, 0)) {
      const float r = (f32(frame, 8) - *time) + extra;
      *time = f32(frame, 8);
      put<float>(e, 0x18, f32(frame, 8));
      put(e, 0x1c, saved1c);
      return r;
    }
    put(e, 0x1c, saved1c);
    put(e, 0x18, saved18);
    put<int32_t>(e, 0x20, get<int32_t>(e, 0x20) - 1);
    until = f32(frame, 8);
  } else {
    until = sf(s, 0x28);
  }
  const float dt = until - *time;
  const float t = dt + f32(e, 0x18);
  put(e, 0x18, t);
  const bool cubic = get<int32_t>(e, 0x14) != 0;
  const uint8_t* p = frame + 0xc;
  if (frame[0] & 1) {
    float v[3];
    track(p, cubic, t, v);
    if (n->cls() == 1) zcls::camera_set_position(n, v[0], v[1], v[2]);
    else if (n->cls() == 5) zcls::object3d_set_translation(n, v[0], v[1], v[2]);
    p += 0x4c;
  }
  if (frame[0] & 2) {
    // The rotation track gives an axis-angle vector: its exponential times
    // the frame's base quaternion (+0x00), as euler angles.
    float v[3];
    if (cubic) {
      track(p, true, t, v);
    } else {
      v[0] = t * f32(p, 0x10), v[1] = f32(p, 0x14) * t, v[2] = f32(p, 0x18) * t;
    }
    float q[4], r[4], m[12];
    zmth::quat_exp(v, q);
    zmth::quat_mul(q, reinterpret_cast<const float*>(p), r);
    zmth::quat_matrix(r, m);
    m[9] = m[10] = m[11] = 0;
    float rot[3];
    zmth::matrix_to_euler(*reinterpret_cast<zmth::Mat34*>(m), rot);
    if (n->cls() == 1) zcls::camera_set_rotation(n, rot[0], rot[1], rot[2]);
    else if (n->cls() == 5) zcls::object3d_set_rotation(n, rot[0], rot[1], rot[2]);
    p += 0x4c;
  }
  if (frame[0] & 4) {
    float v[3];
    track(p, cubic, t, v);
    if (n->cls() == 5) zcls::object3d_set_scale(n, v[0], v[1], v[2]);
  }
  const float r = extra + dt;
  *time = dt + *time;
  return r;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00505b00. A key-framed motion (mech3ax ObjectMotionSiScript)
// for node +0x0c: frames from +0x24, the frame at +0x1c, time in it +0x18.
int32_t ev_object_motion_script(AnimDef& d, SeqDef& s, uint8_t* e) {
  int32_t result = 1;
  const int32_t ni = get<int32_t>(e, 0xc);
  if (ni < 0) return 2;
  Node* n = ref(d, ni);
  if (!n) return 2;
  if (s.raw[0x20] == 0) put<int32_t>(e, 0x1c, 0x24), put<uint32_t>(e, 0x18, 0), put<int32_t>(e, 0x20, 0);
  float time = sf(s, 0x28) - left();
  if (get<int32_t>(e, 0x1c) >= get<int32_t>(s.cur, 4)) return 2;
  do {
    const uint8_t* frame = e + get<int32_t>(e, 0x1c);
    left() = left() - play_frame(s, e, n, frame, &time);
    if (time < sf(s, 0x28) && !next_frame(s, e, frame)) result = 2;
  } while (time < sf(s, 0x28) && result != 2);
  return result;
}

// Mech3.exe 1.2 @ 0x00505bf0. Node +0x14 (-100 the def's node): opacity on
// (+0x0c 1) or off (0), the value +0x10 when +0x0e is 1.
int32_t ev_object_opacity(AnimDef& d, uint8_t* e) {
  const int16_t i = get<int16_t>(e, 0x14);
  Node* n = i >= 0 ? ref(d, i) : i == -100 ? d.node : nullptr;
  if (!n) return 2;
  const int16_t use = get<int16_t>(e, 0xc);
  if (use == 1 || use == 0) zcls::object3d_use_opacity(n, use == 1);
  if (get<int16_t>(e, 0xe) == 1) zcls::object3d_set_opacity(n, f32(e, 0x10));
  return 2;
}

// Mech3.exe 1.2 @ 0x00505c60. Node +0x0c from +0x14 at rate +0x1c to +0x18
// by +0x20; the opacity flag from +0x10 at the start, +0x12 at the end.
int32_t ev_object_opacity_from_to(AnimDef& d, SeqDef& s, uint8_t* e) {
  const int32_t ni = get<int32_t>(e, 0xc);
  if (ni < 0) return 2;
  Node* n = ref(d, ni);
  if (!n) return 2;
  if (s.raw[0x20] == 0) {
    const int16_t use = get<int16_t>(e, 0x10);
    if (use == 1 || use == 0) zcls::object3d_use_opacity(n, use == 1);
    zcls::object3d_set_opacity(n, f32(e, 0x14));
  }
  const float t = step(s, f32(e, 0x20));
  float o;
  if (zcls::object3d_get_opacity(n, &o) != 0) return 2;
  zcls::object3d_set_opacity(n, f32(e, 0x1c) * t + o);
  left() = left() - t;
  if (sf(s, 0x28) < f32(e, 0x20)) return 1;
  const int16_t use = get<int16_t>(e, 0x12);
  if (use == 1 || use == 0) zcls::object3d_use_opacity(n, use == 1);
  zcls::object3d_set_opacity(n, f32(e, 0x18));
  return 2;
}

// Mech3.exe 1.2 @ 0x00505d90. Ref +0x0e under ref +0x0c unless already there.
int32_t ev_object_add_child(AnimDef& d, uint8_t* e) {
  const int16_t pi = get<int16_t>(e, 0xc), ci = get<int16_t>(e, 0xe);
  if (pi <= 0 || ci <= 0) return 2;
  Node* parent = ref(d, pi);
  Node* child = ref(d, ci);
  for (int32_t i = 0; i < parent->child_count(); ++i)
    if (parent->children[i] == child) return 2;
  zcls::node_add_child(parent, child);
  return 2;
}

// Mech3.exe 1.2 @ 0x00505e20. The model's texture cycle back to its start,
// then to frame +0x12 (flag 1 at +0x0c).
int32_t ev_object_cycle_texture(AnimDef& d, SeqDef&, uint8_t* e) {
  const int16_t ni = get<int16_t>(e, 0x10);
  if (ni < 0) return 2;
  Node* n = ref(d, ni);
  if (!(e[0xc] & 1) || !n || !n->model) return 2;
  zmod::cycle_reset(n->model);
  zmod::cycle_set_frame(n->model, get<int16_t>(e, 0x12));
  return 2;
}

// Mech3.exe 1.2 @ 0x00502f00. The connector node from `from` toward `to`:
// placed, turned and stretched (z scale) to the distance (a fast square root).
float connector_set(Node* node, const float from[3], const float to[3]) {
  if (!node) return 0.0f;
  zcls::object3d_set_translation(node, from[0], from[1], from[2]);
  float ang[3];
  zmth::direction_angles(from, to, ang);
  zcls::object3d_set_rotation(node, ang[0], ang[1], 0.0f);
  float sc[3];
  zcls::object3d_get_scale(node, &sc[0], &sc[1], &sc[2]);
  const float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
  const float len = fast_sqrt((dx * dx + dy * dy) + dz * dz);
  zcls::object3d_set_scale(node, sc[0], sc[1], len);
  return len;
}

// Mech3.exe 1.2 @ 0x00502fd0. Only the part between t0 and t1 of the way;
// the direction is still from `from` to `to`.
float connector_part(Node* node, const float from[3], float t0, const float to[3], float t1) {
  if (!node) return 0.0f;
  const float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
  const float a[3] = {dx * t0 + from[0], dy * t0 + from[1], dz * t0 + from[2]};
  const float b[3] = {dx * t1 + from[0], dy * t1 + from[1], dz * t1 + from[2]};
  zcls::object3d_set_translation(node, a[0], a[1], a[2]);
  float ang[3];
  zmth::direction_angles(from, to, ang);
  zcls::object3d_set_rotation(node, ang[0], ang[1], 0.0f);
  float sc[3];
  zcls::object3d_get_scale(node, &sc[0], &sc[1], &sc[2]);
  const float ex = b[0] - a[0], ey = b[1] - a[1], ez = b[2] - a[2];
  const float len = fast_sqrt((ex * ex + ey * ey) + ez * ez);
  zcls::object3d_set_scale(node, sc[0], sc[1], len);
  return len;
}

// Mech3.exe 1.2 @ 0x00505e80. ECX def, EDX seq, the event on the stack.
// Connector node +0x10 between two points, each from a ref (+0x12 / +0x14),
// the def's ends (+0x74 / +0x84) or absolute, with offsets (+0x18 / +0x24 or
// the def's +0x78 / +0x88; flags 8/0x10 and 0x100/0x200; flags 4 and 0x80
// turn a first frame's point into a fixed one). It grows from +0x30 to
// +0x34 (rate +0x38) and its start from +0x40 to +0x44 (rate +0x48) by
// +0x50; 0x8000 ends early at length +0x54.
int32_t ev_object_connector(AnimDef& d, SeqDef& s, uint8_t* e) {
  const int16_t ni = get<int16_t>(e, 0x10);
  if (ni < 0 || !ref(d, ni)) return 2;
  Node* n = ref(d, ni);
  if (s.raw[0x20] == 0) {
    uint32_t f = get<uint32_t>(e, 0xc);
    if (f & 0x800) put(e, 0x3c, get<uint32_t>(e, 0x30));
    if (f & 0x2000) put(e, 0x4c, get<uint32_t>(e, 0x40));
    if (f & 4) {
      put(e, 0xc, f & ~4u);
      Node* at = d.at_node;
      float p[3] = {0, 0, 0};
      if (f & 8) {
        put(e, 0xc, f & ~0xcu);
        std::memcpy(p, e + 0x18, 12);
      } else if (f & 0x10) {
        put(e, 0xc, f & ~0x14u);
        std::memcpy(p, d.raw + 0x78, 12);
      }
      if (at) zcls::node_world_point(at, p);
      std::memcpy(e + 0x18, p, 12);
      put(e, 0xc, get<uint32_t>(e, 0xc) | 8);
    }
    f = get<uint32_t>(e, 0xc);
    if (f & 0x80) {
      put(e, 0xc, f & ~0x80u);
      Node* at = d.at_node2;
      float p[3] = {0, 0, 0};
      if (f & 0x100) {
        put(e, 0xc, f & ~0x180u);
        std::memcpy(p, e + 0x24, 12);
      } else if (f & 0x200) {
        put(e, 0xc, f & ~0x280u);
        std::memcpy(p, d.raw + 0x88, 12);
      }
      if (at) zcls::node_world_point(at, p);
      std::memcpy(e + 0x24, p, 12);
      put(e, 0xc, get<uint32_t>(e, 0xc) | 0x100);
    }
  }
  const float t = step(s, f32(e, 0x50));
  uint32_t f = get<uint32_t>(e, 0xc);
  Node* a = nullptr;
  if (f & 1) a = get<int16_t>(e, 0x12) < 0 ? nullptr : ref(d, get<int16_t>(e, 0x12));
  else if (f & 2) a = d.at_node;
  float from[3] = {0, 0, 0};
  if (f & 8) std::memcpy(from, e + 0x18, 12);
  else if (f & 0x10) std::memcpy(from, d.raw + 0x78, 12);
  if (a) zcls::node_world_point(a, from);
  f = get<uint32_t>(e, 0xc);
  Node* b = nullptr;
  if (f & 0x20) b = get<int16_t>(e, 0x14) < 0 ? nullptr : ref(d, get<int16_t>(e, 0x14));
  else if (f & 0x40) b = d.at_node2;
  float to[3] = {0, 0, 0};
  if (f & 0x100) std::memcpy(to, e + 0x24, 12);
  else if (f & 0x200) std::memcpy(to, d.raw + 0x88, 12);
  if (b) zcls::node_world_point(b, to);
  f = get<uint32_t>(e, 0xc);
  bool part = false;
  if (f & 0x400) part = true, put(e, 0x3c, get<uint32_t>(e, 0x30));
  else if (f & 0x800) part = true, fref(e, 0x3c) = f32(e, 0x38) * t + f32(e, 0x3c);
  else put<uint32_t>(e, 0x3c, 0);
  if (f & 0x1000) part = true, put(e, 0x4c, get<uint32_t>(e, 0x40));
  else if (f & 0x2000) part = true, fref(e, 0x4c) = f32(e, 0x48) * t + f32(e, 0x4c);
  else fref(e, 0x4c) = 1.0f;
  float len;
  if (part) {
    if (f32(e, 0x50) <= sf(s, 0x28)) put(e, 0x3c, get<uint32_t>(e, 0x34)), put(e, 0x4c, get<uint32_t>(e, 0x44));
    len = connector_part(n, from, f32(e, 0x3c), to, f32(e, 0x4c));
  } else {
    len = connector_set(n, from, to);
  }
  int32_t r = 2;
  if ((get<uint32_t>(e, 0xc) & 0x8000) && len <= f32(e, 0x54)) r = 1;
  if (sf(s, 0x28) < f32(e, 0x50)) r = 1;
  left() = left() - t;
  return r;
}

// Mech3.exe 1.2 @ 0x00506290. Starts the def named +0x10 (cached +0x30) as a
// connector between two points, each a ref (+0x34 / +0x36), the def's own
// node, a node's world position (flags 2 / 8, 0x80 / 0x200) or the def's
// ends, with offsets (+0x38 / +0x44 or the def's); the anim ref +0x32 keeps it.
int32_t ev_call_connector(AnimDef& d, uint8_t* e) {
  if (get<int16_t>(e, 0x30) < 1) {
    for (int32_t j = 0; j < A().def_count(); ++j)
      if (streq(A().defs[j].name(), reinterpret_cast<const char*>(e + 0x10))) {
        put<int16_t>(e, 0x30, static_cast<int16_t>(j));
        break;
      }
    if (get<int16_t>(e, 0x30) < 1) return 2;
  }
  const uint32_t f = get<uint32_t>(e, 0xc);
  float p1[3] = {0, 0, 0}, p2[3] = {0, 0, 0};
  if (f & 0x10) std::memcpy(p1, e + 0x38, 12);
  else if (f & 0x20) std::memcpy(p1, d.raw + 0x78, 12);
  if (f & 0x400) std::memcpy(p2, e + 0x44, 12);
  else if (f & 0x800) std::memcpy(p2, d.raw + 0x88, 12);
  // With none of an end's flags the exe passes the def itself as that end's
  // node (a bug); the shipped events always set one. Null here.
  Node* n1 = nullptr;
  if (f & 1) n1 = ref(d, get<int16_t>(e, 0x34));
  else if (f & 2) zcls::node_world_point(ref(d, get<int16_t>(e, 0x34)), p1);
  else if (f & 4) n1 = d.at_node;
  else if (f & 8) zcls::node_world_point(d.at_node, p1);
  Node* n2 = nullptr;
  const uint32_t g = get<uint32_t>(e, 0xc);
  if (g & 0x40) n2 = ref(d, get<int16_t>(e, 0x36));
  else if (g & 0x80) zcls::node_world_point(ref(d, get<int16_t>(e, 0x36)), p2);
  else if (g & 0x100) n2 = d.at_node2;
  else if (g & 0x200) zcls::node_world_point(d.at_node2, p2);
  AnimDef* started = anim_start_connector_at(&A().defs[get<int16_t>(e, 0x30)], nullptr, n1, p1, n2, p2);
  if (get<int16_t>(e, 0x32) >= 0) d.anim_refs[get<int16_t>(e, 0x32)].ref = started;
  return 2;
}

namespace {
// The name lookup of 0x005064e0 / 0x00506590: the def's sequence named
// +0x0c, cached at +0x2c.
int32_t seq_index(AnimDef& d, uint8_t* e) {
  if (get<int32_t>(e, 0x2c) < 0) {
    for (int32_t q = 0; q < d.count(0x108); ++q)
      if (streq(d.seqs[q].name(), reinterpret_cast<const char*>(e + 0xc))) {
        put(e, 0x2c, q);
        break;
      }
  }
  return get<int32_t>(e, 0x2c);
}

// The name lookup of the stop/reset/invalidate events (0x00506d60..): over
// all defs, cached at +0x2c; index 0 counts as not found.
AnimDef* def_by_name(uint8_t* e) {
  if (get<int32_t>(e, 0x2c) < 1) {
    for (int32_t j = 0; j < A().def_count(); ++j)
      if (streq(A().defs[j].name(), reinterpret_cast<const char*>(e + 0xc))) {
        put(e, 0x2c, j);
        break;
      }
  }
  const int32_t j = get<int32_t>(e, 0x2c);
  return j > 0 ? &A().defs[j] : nullptr;
}
}  // namespace

// Mech3.exe 1.2 @ 0x005064e0. A waiting sequence (3) starts.
int32_t ev_call_sequence(AnimDef& d, uint8_t* e) {
  const int32_t q = seq_index(d, e);
  if (q >= 0 && d.seqs[q].raw[0x20] == 3) d.seqs[q].raw[0x20] = 0;
  return 2;
}

// Mech3.exe 1.2 @ 0x00506590
int32_t ev_stop_sequence(AnimDef& d, uint8_t* e) {
  const int32_t q = seq_index(d, e);
  if (q >= 0) d.seqs[q].raw[0x20] = 2;
  return 2;
}

// Mech3.exe 1.2 @ 0x00506640. Starts the def named +0x0c (cached +0x30) at
// node ref +0x2c, with the caller's start vector: at a node's position and
// rotation (flag 1, node +0x34 or -200, offset +0x38, rotation +0x44 with
// 4), as a connector from it (8), or plainly. The anim ref +0x32 keeps it;
// with 0x10 the event waits while it runs.
int32_t ev_call_animation(AnimDef& d, SeqDef& s, uint8_t* e) {
  AnimDef* started = nullptr;
  if (s.raw[0x20] == 0) {
    const int16_t slot = get<int16_t>(e, 0x32);
    if (slot >= 0) d.anim_refs[slot].ref = nullptr;
    if (get<int16_t>(e, 0x30) <= 0) {
      for (int32_t j = 0; j < A().def_count(); ++j)
        if (streq(A().defs[j].name(), reinterpret_cast<const char*>(e + 0xc))) {
          put<int16_t>(e, 0x30, static_cast<int16_t>(j));
          break;
        }
    }
    const int16_t target = get<int16_t>(e, 0x30);
    if (target > 0) {
      AnimDef* t = &A().defs[target];
      Node* at = get<int16_t>(e, 0x2c) > 0 ? ref(d, get<int16_t>(e, 0x2c)) : nullptr;
      const uint16_t f = get<uint16_t>(e, 0x2e);
      const float* vec = d.floats(0xb8);
      if (f & 1) {
        const int16_t pi = get<int16_t>(e, 0x34);
        Node* pn = nullptr;
        float base[3] = {0, 0, 0};
        if (pi > 0) pn = ref(d, pi);
        else if (pi == -200) pn = d.at_node, std::memcpy(base, d.raw + 0x78, 12);
        if (pn) {
          float pos[3] = {f32(e, 0x38) + base[0], f32(e, 0x3c) + base[1], f32(e, 0x40) + base[2]};
          float rot[3] = {0, 0, 0};
          if (f & 4) {
            zcls::node_world_frame(pn, pos, rot);
            rot[0] = f32(e, 0x44) + rot[0], rot[1] = f32(e, 0x48) + rot[1], rot[2] = f32(e, 0x4c) + rot[2];
          } else {
            zcls::node_world_point(pn, pos);
          }
          const float v[3] = {vec[0], vec[1], vec[2]};
          started = anim_start_at(t, at, pos, rot, v);
        }
      } else if (f & 8) {
        const int16_t pi = get<int16_t>(e, 0x34);
        Node* pn = nullptr;
        float pos[3] = {0, 0, 0};
        if (pi > 0) {
          pn = ref(d, pi);
          std::memcpy(pos, e + 0x38, 12);
        } else if (pi == -200) {
          pn = d.at_node;
          pos[0] = f32(e, 0x38) + df(d, 0x78), pos[1] = f32(e, 0x3c) + df(d, 0x7c);
          pos[2] = f32(e, 0x40) + df(d, 0x80);
        }
        started = anim_start_connector(t, at, pn, pos, vec);
      } else {
        started = anim_start(t, at, vec[0], vec[1], vec[2]);
      }
      if (slot >= 0) d.anim_refs[slot].ref = started;
    }
    s.raw[0x20] = 1;
  }
  const int16_t slot = get<int16_t>(e, 0x32);
  if ((get<uint8_t>(e, 0x2e) & 0x10) && slot >= 0) {
    AnimDef*& r = d.anim_refs[slot].ref;
    if (r) {
      if (r->raw[0x98] == 2 || r->raw[0x98] == 6) return s.raw[0x20];
      r = nullptr;
    }
  }
  s.raw[0x20] = 2;
  return 2;
}

// Mech3.exe 1.2 @ 0x00506d60
int32_t ev_stop_animation(uint8_t* e) {
  if (AnimDef* t = def_by_name(e)) anim_stop(t);
  return 2;
}

// Mech3.exe 1.2 @ 0x00506e00
int32_t ev_reset_animation(uint8_t* e) {
  if (AnimDef* t = def_by_name(e)) anim_restart(t);
  return 2;
}

// Mech3.exe 1.2 @ 0x00506ea0
int32_t ev_invalidate_animation(uint8_t* e) {
  if (AnimDef* t = def_by_name(e)) anim_invalidate(t);
  return 2;
}

// Mech3.exe 1.2 @ 0x00503ba0. The world's fog by the flags at +0x2c: type
// +0x30 (1), colour +0x34 (2), altitude +0x40 (4), range +0x48 (8).
int32_t ev_fog_state(uint8_t* e) {
  const uint8_t f = e[0x2c];
  if (f & 1) zcls::world_set_fog_type(world(), get<uint32_t>(e, 0x30));
  if (f & 2) zcls::world_set_fog_color(world(), f32(e, 0x34), f32(e, 0x38), f32(e, 0x3c));
  if (f & 4) zcls::world_set_fog_altitude(world(), f32(e, 0x40), f32(e, 0x44));
  if (f & 8) zcls::world_set_fog_range(world(), f32(e, 0x48), f32(e, 0x4c));
  return 2;
}

// Mech3.exe 1.2 @ 0x00506f60. Ends after +0x10 passes (flag 1, -1 for ever)
// or once the summed run time reaches +0x10 (flag 2); else the sequence
// starts over (state 0). The anim's clock is held under a day.
int32_t ev_loop(AnimDef& d, SeqDef& s, uint8_t* e) {
  const float total = sf(s, 0x24) + sf(s, 0x2c);
  s.put<int16_t>(0x30, static_cast<int16_t>(s.get<int16_t>(0x30) + 1));
  sf(s, 0x2c) = total;
  const uint32_t f = get<uint32_t>(e, 0xc);
  if (f & 1) {
    if (get<int16_t>(e, 0x10) != -1 && s.get<int16_t>(0x30) == get<int16_t>(e, 0x10)) {
      s.raw[0x20] = 2;
      return 2;
    }
  } else if ((f & 2) && 0.0f <= f32(e, 0x10) && f32(e, 0x10) <= total) {
    s.raw[0x20] = 2;
    return 2;
  }
  if (86400.0f < df(d, 0xa8)) d.put<uint32_t>(0xa8, 0x47a8c000);
  anim_seq_restart(&s);
  return 0;
}

// Mech3.exe 1.2 @ 0x00507010. A condition (+0x0c flags, +0x14 value): 1
// random under it, 0x20 hardware rendering, 0x40 block +0x3c, 2 player
// within the squared distance (cached while the flags repeat), 8 the
// ground under the player (0x00507220), 0x10 the ground under ref +0x10,
// 0x80 the player below a height, 0x100 ref +0x10 below it, 4 level at
// least it. False skips to the next else (32), elseif (33, tested the same
// way) or endif (34).
int32_t ev_if(AnimDef& d, SeqDef& s, uint8_t* e) {
  AnimState& a = A();
  uint32_t f = get<uint32_t>(e, 0xc);
  uint32_t v = get<uint32_t>(e, 0x14);
  uint32_t prev = 0;
  float cached = 0.0f;
  for (;;) {
    float value;
    std::memcpy(&value, &v, 4);
    bool yes = false;
    if (f & 1) {
      cached = a.random[a.random_index];
      a.random_index = (a.random_index + 1) % 200;
      yes = cached <= value;
    } else if (f & 0x20) {
      yes = zrndr::device_fog().active != 0;
    } else if (f & 0x40) {
      yes = a.block.get<int32_t>(0x3c) != 0;
    } else if (f & 2) {
      if (a.block.get<int32_t>(0x24)) {
        if (prev != f) cached = distance2_to_player(d.root);
        yes = cached <= value;
      }
    } else if (f & 8) {
      if (a.block.get<int32_t>(0x24)) {
        int32_t hit = 0;
        const int32_t r = H().ray_down ? H().ray_down(H().ctx, a.player_node,
                                                     a.block.floats(0x2c), value, &hit)
                                       : 1;
        yes = r == 0 && hit;
      }
    } else if (f & 0x10) {  // the node is always the if event's own (+0x10), elseif or not
      if (Node* n = ref(d, get<int32_t>(e, 0x10))) {
        int32_t hit = 0;
        const int32_t r = H().ray_down ? H().ray_down(H().ctx, n, nullptr, value, &hit) : 1;
        yes = r == 0 && hit;
      }
    } else if (f & 0x80) {
      yes = a.block.get<int32_t>(0x24) && a.block.get<float>(0x30) < value;
    } else if (f & 0x100) {
      if (Node* n = ref(d, get<int32_t>(e, 0x10))) {
        float p[3];
        yes = zcls::node_world_position(n, p) == 0 && p[1] < value;
      }
    } else if (f & 4) {
      yes = a.level >= static_cast<int32_t>(v);
    }
    prev = f;
    if (yes) return 2;
    uint8_t* end = s.events + s.get<int32_t>(0x3c);
    uint8_t* c = s.cur;
    do {
      c += get<int32_t>(c, 4);
      s.cur = c;
      if (c[0] == 0x20 || c[0] == 0x21 || c[0] == 0x22) break;
    } while (c < end);
    if (c[0] != 0x21) return 2;
    v = get<uint32_t>(c, 0x14);
    f = get<uint32_t>(c, 0xc);
  }
}

// Mech3.exe 1.2 @ 0x005073b0 (and 0x005073e0). Reached at the end of a
// branch: on to the endif.
int32_t ev_else(SeqDef& s) {
  uint8_t* end = s.events + s.get<int32_t>(0x3c);
  uint8_t* c = s.cur;
  do {
    c += get<int32_t>(c, 4);
    s.cur = c;
    if (c[0] == 0x22) return 2;
  } while (c < end);
  return 2;
}

// Mech3.exe 1.2 @ 0x00507410. The def's callback hears +0x0c.
int32_t ev_callback(AnimDef& d, uint8_t* e) {
  if (d.callback) d.callback(&d, d.callback_arg, get<int32_t>(e, 0xc));
  return 2;
}

// Mech3.exe 1.2 @ 0x00507430. The weapon named +0x0c goes off at +0x18 (in
// node +0x16's frame, -200 the start node).
int32_t ev_detonate_weapon(AnimDef& d, uint8_t* e) {
  float p[3];
  std::memcpy(p, e + 0x18, 12);
  if (Node* n = ref_or_at(d, get<int16_t>(e, 0x16))) zcls::node_world_point(n, p);
  if (H().detonate) H().detonate(H().ctx, e + 0xc, p);
  return 2;
}

}  // namespace zeff
