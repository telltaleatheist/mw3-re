// The mech's animation objects and the zSIScript samplers (mech3/mech_anim,
// gamez/zsiscript) for tests/differential/test_mechanim.py. Objects are
// made from the real motion archive and kept by handle; each op takes and
// returns floats, the state goes out laid out as test_mechanim.py reads it.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/mech_anim.h"

using namespace mech3;

namespace {
std::vector<MechAnim*> g_objs;

void put(std::vector<uint8_t>& b, const void* p, size_t n) {
  const auto* c = static_cast<const uint8_t*>(p);
  b.insert(b.end(), c, c + n);
}
template <typename T> void put(std::vector<uint8_t>& b, T v) { put(b, &v, sizeof v); }

void put_part(std::vector<uint8_t>& b, const zsis::Part* p, uint32_t n, bool keys) {
  put<uint32_t>(b, p ? 1 : 0);
  if (!p) return;
  put(b, p->name, std::strlen(p->name) + 1);
  put<uint32_t>(b, p->has_matrix);
  put(b, p->matrix, 36);
  put(b, p->offset, 12);
  put<uint32_t>(b, p->velocities ? 1 : 0);
  if (!keys) return;
  put(b, p->translations, (n + 1) * 12);
  put(b, p->rotations, (n + 1) * 16);
  if (p->velocities) put(b, p->velocities, (n + 1) * 12);
}
}  // namespace

extern "C" {

int probe_ma_open(const char* path) { return open_motions(path) ? 1 : 0; }

// kind 0 MechAnim (0x0041f000), 1 one-shot (0x00420d00); mirror: then
// mirror_init(speed, frames) as 0x00431350 makes the right limp.
int probe_ma_load(int kind, const char* name, float speed, uint32_t frames, int mirror) {
  MechAnim* a = kind == 0 ? load_anim(name, speed, frames) : load_action(name, frames);
  if (!a) return -1;
  if (mirror) a->mirror_init(speed, frames);
  g_objs.push_back(a);
  return static_cast<int>(g_objs.size()) - 1;
}

int probe_ma_blend(int a, int b) {
  g_objs.push_back(make_blend(g_objs[a], g_objs[b]));
  return static_cast<int>(g_objs.size()) - 1;
}

int probe_ma_copy(int h) {
  auto* c = new MechAnim;
  c->copy_from(*g_objs[h]);
  g_objs.push_back(c);
  return static_cast<int>(g_objs.size()) - 1;
}

// keys: the parts' key arrays too.
int probe_ma_state(int h, int keys, uint8_t* out, int cap) {
  MechAnim& a = *g_objs[h];
  std::vector<uint8_t> b;
  put(b, a.rate), put(b, a.mult), put(b, a.stride), put<uint32_t>(b, a.active);
  put(b, a.loop_time), put(b, a.inv_loop_time), put(b, a.frame_time), put(b, a.inv_frame_time);
  put(b, a.frame_count), put<uint32_t>(b, a.wraps);
  put(b, a.time), put(b, a.phase), put(b, a.frac), put(b, a.key0), put(b, a.key1);
  put(b, a.t), put(b, a.stride_speed), put(b, a.events, 12);
  uint32_t live = 0;
  for (uint32_t i = 0; i < a.part_count; ++i) live += a.parts && a.parts[i] ? 1 : 0;
  put(b, live);
  const uint32_t n = a.frame_count;
  zsis::Part* slots[] = {a.feet[0], a.feet[1], a.hip_part, a.torso_part, a.arms[0], a.arms[1], a.hands[0],
                         a.hands[1], a.toes[0][0], a.toes[0][1], a.toes[0][2], a.toes[0][3], a.toes[1][0],
                         a.toes[1][1], a.toes[1][2], a.toes[1][3]};
  for (zsis::Part* p : slots) put_part(b, p, n, keys != 0);
  if (auto* act = dynamic_cast<MechAction*>(&a)) {
    for (zsis::Part* p : {act->thighs[0], act->thighs[1], act->calves[0], act->calves[1]}) put_part(b, p, n, keys != 0);
    put<uint32_t>(b, act->done), put<uint32_t>(b, act->reversed);
  }
  if (auto* bl = dynamic_cast<BlendAnim*>(&a)) put(b, bl->weight);
  if (static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}

// One call; in: the op's arguments, out: what it returns (the op's own
// layout, as test_mechanim.py's `run`).
int probe_ma_op(int h, int op, const float* in, float* out) {
  MechAnim& a = *g_objs[h];
  auto i0 = [&] { return static_cast<int>(in[0]); };
  switch (op) {
    case 0: a.set_speed(in[0]); return 0;
    case 1: a.set_time(in[0]); return 0;
    case 2: a.advance(in[0]); return 0;
    case 3: a.set_blend(in[0]); return 0;
    case 4: a.hip_frame(in[0] != 0 ? in + 1 : nullptr); return 0;
    case 5: a.ski_frame(i0(), in[1] != 0 ? in + 2 : nullptr); return 0;
    case 6: {
      Vec3 v{in[0], in[1], in[2]};
      a.velocity(&v);
      out[0] = v.x, out[1] = v.y, out[2] = v.z;
      return 3;
    }
    case 7: a.foot(i0(), out, out + 3); return 7;
    case 8: case 9: {
      for (int k = 0; k < 7; ++k) out[1 + k] = in[3 + k];
      float* pos = in[1] != 0 ? out + 1 : nullptr;
      float* q = in[2] != 0 ? out + 4 : nullptr;
      const bool r = op == 8 ? a.arm(i0(), pos, q) : a.hand(i0(), pos, q);
      out[0] = r ? 1.0f : 0.0f;
      return 8;
    }
    case 10: a.hip(out, out + 3); return 7;
    case 11: a.torso(out, out + 3); return 7;
    case 12: a.torso_rotation(out); return 4;
    case 13: {
      for (int k = 0; k < 4; ++k) out[1 + k] = in[2 + k];
      out[0] = a.toe(i0(), static_cast<int>(in[1]), out + 1) ? 1.0f : 0.0f;
      return 5;
    }
    case 14: a.seek(i0()); return 0;
    case 15: a.copy_phase(*g_objs[i0()]); return 0;
    case 16: a.set_stride_speed(in[0]); return 0;
    case 17: out[0] = a.footfalls(); return 1;
    case 18: static_cast<MechAction&>(a).stop(); return 0;
    case 19: static_cast<MechAction&>(a).reset(); return 0;
    case 20: out[0] = static_cast<MechAction&>(a).started() ? 1.0f : 0.0f; return 1;
    default: return -1;
  }
}

// The rotation keys of part slot `slot` (probe_ma_state's order) replaced:
// the mirror's Euler round trip is a last bit apart from the emulator's
// x87 asin stand-in, so test_mechanim.py checks it within a bound and syncs.
int probe_ma_set_rotations(int h, int slot, const float* keys, float* old) {
  MechAnim& a = *g_objs[h];
  zsis::Part* slots[] = {a.feet[0], a.feet[1], a.hip_part, a.torso_part, a.arms[0], a.arms[1], a.hands[0],
                         a.hands[1], a.toes[0][0], a.toes[0][1], a.toes[0][2], a.toes[0][3], a.toes[1][0],
                         a.toes[1][1], a.toes[1][2], a.toes[1][3]};
  zsis::Part* p = slot < 16 ? slots[slot] : nullptr;
  if (auto* act = dynamic_cast<MechAction*>(&a); act && slot >= 16)
    p = slot < 18 ? act->thighs[slot - 16] : act->calves[slot - 18];
  if (!p) return 0;
  std::memcpy(old, p->rotations, (a.frame_count + 1) * 16);
  std::memcpy(p->rotations, keys, (a.frame_count + 1) * 16);
  return 1;
}

void probe_ma_free() {
  for (MechAnim* a : g_objs) delete a;
  g_objs.clear();
}

}  // extern "C"
