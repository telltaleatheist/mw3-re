// The pose on a mech's parts (mech3/mech_pose.cpp) for tests/differential/test_pose.py.
// The mech and its twelve parts go back and forth as byte images laid out
// like the original's (pose_world.py has the same offsets), each part's node
// as its 3x4 matrix; calls out (node sets, the feet's ground update) are
// logged as (count, code, args...) on both sides.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_anim.h"
#include "mech3/mech_move.h"

using namespace mech3;

namespace {
template <typename T> T rd(const uint8_t* b, int off) {
  T v;
  std::memcpy(&v, b + off, sizeof v);
  return v;
}
template <typename T> void wr(uint8_t* b, int off, T v) { std::memcpy(b + off, &v, sizeof v); }
int32_t bits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}

struct Io {
  uint8_t* b;
  bool store;
  void f(int off, float& v) { store ? wr(b, off, v) : void(v = rd<float>(b, off)); }
  void i(int off, int32_t& v) { store ? wr(b, off, v) : void(v = rd<int32_t>(b, off)); }
  void u32(int off, uint32_t& v) { store ? wr(b, off, v) : void(v = rd<uint32_t>(b, off)); }
  void bo(int off, bool& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off) != 0); }
  void v3(int off, Vec3& v) { f(off, v.x), f(off + 4, v.y), f(off + 8, v.z); }
  void q(int off, float* v) {
    for (int k = 0; k < 4; ++k) f(off + 4 * k, v[k]);
  }
};

std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}

// Parts in pose_world.py's order.
enum { kLeg0, kLeg1, kCalf0, kCalf1, kFoot0, kFoot1, kHip, kTorso, kArm0, kArm1, kHand0, kHand1, kParts };
constexpr int kImage = 0x1b4;

struct PoseWorld {
  Mech m;
  Leg legs[2];
  Calf calves[2];
  Foot feet[2];
  Hip hip;
  Torso torso;
  Arm arms[2];
  Hand hands[2];
  float nodes[kParts][12] = {};
  MechPart* part(int k) {
    switch (k) {
      case kLeg0: case kLeg1: return &legs[k - kLeg0];
      case kCalf0: case kCalf1: return &calves[k - kCalf0];
      case kFoot0: case kFoot1: return &feet[k - kFoot0];
      case kHip: return &hip;
      case kTorso: return &torso;
      case kArm0: case kArm1: return &arms[k - kArm0];
      default: return &hands[k - kHand0];
    }
  }
};
PoseWorld* g = nullptr;

int node_index(const void* n) {
  for (int k = 0; k < kParts; ++k)
    if (n == g->nodes[k]) return k;
  return -1;
}

void base_io(MechPart& p, Io& io) {
  io.bo(0x48, p.destroyed), io.bo(0x4a, p.pending), io.v3(0x4c, p.pending_pos), io.q(0x58, p.pending_q);
}

void part_io(int k, uint8_t* b, bool store) {
  Io io{b, store};
  MechPart& p = *g->part(k);
  base_io(p, io);
  if (k == kLeg0 || k == kLeg1) {
    Leg& l = g->legs[k - kLeg0];
    io.bo(0x7c, l.knee_back), io.f(0x80, l.thigh_angle), io.f(0x84, l.knee_angle);
    io.q(0x88, l.thigh_q), io.q(0x98, l.calf_q);
    float* ik[] = {&l.ik_a8, &l.thigh_len, &l.ik_b0, &l.hip_x, &l.calf_len, &l.ik_bc, &l.ik_c0, &l.ik_c4,
                   &l.ik_c8, &l.ik_k, &l.ik_m};
    for (int i = 0; i < 11; ++i) io.f(0xa8 + 4 * i, *ik[i]);
  } else if (k == kFoot0 || k == kFoot1) {
    Foot& f = g->feet[k - kFoot0];
    io.f(0x8c, f.clearance), io.i(0x94, f.ground_state), io.q(0xc4, f.last_q);
  } else if (k == kHip) {
    io.q(0x74, g->hip.rot), io.q(0x84, g->hip.inv), io.v3(0x98, g->hip.offset), io.f(0xa8, g->hip.crouch_target);
  }
}

void mech_io(uint8_t* b, bool store) {
  Io io{b, store};
  Mech& m = g->m;
  int32_t d = static_cast<int32_t>(m.detail);
  io.i(0x688, d);
  m.detail = static_cast<uint32_t>(d);
}
}  // namespace

extern "C" {

// present: bit k, part k has a node.
void probe_pose_load(const uint8_t* mech, const uint8_t* parts, const float* nodes, uint32_t present) {
  if (!g) g = new PoseWorld;
  PoseWorld& w = *g;
  w.m = Mech{};
  w.m.legs[0] = &w.legs[0], w.m.legs[1] = &w.legs[1];
  w.m.arms[0] = &w.arms[0], w.m.arms[1] = &w.arms[1];
  w.m.hip = &w.hip, w.m.torso = &w.torso;
  for (int i = 0; i < 2; ++i) {
    w.legs[i] = Leg{}, w.calves[i] = Calf{}, w.feet[i] = Foot{}, w.arms[i] = Arm{}, w.hands[i] = Hand{};
    w.legs[i].calf = &w.calves[i], w.legs[i].foot = &w.feet[i], w.arms[i].hand = &w.hands[i];
  }
  w.hip = Hip{}, w.torso = Torso{};
  std::memcpy(w.nodes, nodes, sizeof w.nodes);
  for (int k = 0; k < kParts; ++k) {
    MechPart& p = *w.part(k);
    p.mech = &w.m;
    p.node = present >> k & 1 ? w.nodes[k] : nullptr;
    p.rotation = w.nodes[k];
    p.translation = reinterpret_cast<Vec3*>(w.nodes[k] + 9);
    part_io(k, const_cast<uint8_t*>(parts + kImage * k), false);
  }
  mech_io(const_cast<uint8_t*>(mech), false);
  g_log.clear();
  pose_calls().node_set = [](Handle n, const float* m) {
    std::vector<int32_t> e{14, node_index(n)};
    for (int i = 0; i < 12; ++i) e.push_back(bits(m[i]));
    g_log.push_back(static_cast<int32_t>(e.size()));
    g_log.insert(g_log.end(), e.begin(), e.end());
  };
  move_calls().foot_ground = [](Foot& f) { log({15, &f == &g->feet[0] ? 0 : 1}); };
}

// op 0: legs[a0].reach(p = a1..a3, q = a4..a7); 1: part a0's set_pose with
// pos (a1 != 0: a2..a4) and q (a5 != 0: a6..a9); 2: legs[a0].solve(p).
int probe_pose_op(int op, const float* a) {
  PoseWorld& w = *g;
  switch (op) {
    case 0: {
      const Vec3 p{a[1], a[2], a[3]};
      return w.legs[static_cast<int>(a[0])].reach(p, a + 4) ? 1 : 0;
    }
    case 1: {
      const Vec3 p{a[2], a[3], a[4]};
      w.part(static_cast<int>(a[0]))->set_pose(a[1] != 0 ? &p : nullptr, a[5] != 0 ? a + 6 : nullptr);
      return 0;
    }
    case 2: return w.legs[static_cast<int>(a[0])].solve(Vec3{a[1], a[2], a[3]}) ? 1 : 0;
    default: return -1;
  }
}

int probe_pose_store(uint8_t* mech, uint8_t* parts, float* nodes, int32_t* out, int cap) {
  PoseWorld& w = *g;
  for (int k = 0; k < kParts; ++k) part_io(k, parts + kImage * k, true);
  mech_io(mech, true);
  std::memcpy(nodes, w.nodes, sizeof w.nodes);
  pose_calls() = PoseCalls{};
  move_calls().foot_ground = nullptr;
  const int n = static_cast<int>(g_log.size());
  if (n > cap) return -1;
  std::memcpy(out, g_log.data(), n * 4);
  return n;
}

}  // extern "C"
