// PM's firing's aim (zai_firing_pm), for test_pm_ai_aim.py: the world's answers come in, the calls
// out and the aim's words go back.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/zai_firing_pm.h"
#include "mech3/zai_target.h"

namespace {
using namespace mech3::ai;
struct W : AimPmHooks {
  const float* f;
  const int32_t* in;
  const int32_t* rands;
  int nrand = 0;
  Contact c;
  float pos[3];
  std::vector<uint32_t> log;
  Contact* plan_contact(zgame::Handle) override { return &c; }
  const float* plan_position(zgame::Handle) override { return pos; }
  zgame::Handle contact_entity(const Contact*) override { return in[0] ? 1 : 0; }
  bool entity_is_vehicle(zgame::Handle) override { return in[1] != 0; }
  uint32_t entity_pilot_kind(zgame::Handle) override { return static_cast<uint32_t>(in[2]); }
  float difficulty_error() override { return f[0]; }
  float pilot_error(Brain*) override { return f[1]; }
  float pilot_aid(Brain*) override { return f[2]; }
  bool targeting_aid(Brain*) override { return in[3] != 0; }
  void vehicle_position(Brain*, float out[3]) override { std::memcpy(out, f + 3, 12); }
  void weapon_line(zgame::Handle, const float at[3], float muzzle[3], float dir[3]) override {
    for (int i = 0; i < 3; ++i) log.push_back(bits(at[i]));
    std::memcpy(muzzle, f + 6, 12), std::memcpy(dir, f + 9, 12);
  }
  int32_t rand() override { return rands[nrand++]; }
  float now() override { return f[12]; }
  void wm_aim_at(zgame::Handle, const float* at) override {
    log.push_back(0x41);
    for (int i = 0; i < 3; ++i) log.push_back(bits(at[i]));
  }
  void wm_has_target(zgame::Handle, bool on) override { log.push_back(0x42), log.push_back(on); }
  static uint32_t bits(float x) {
    uint32_t u;
    std::memcpy(&u, &x, 4);
    return u;
  }
};
}  // namespace

// in: entity, vehicle, its pilot kind, the aid, a weapon, the op (0 track, 1 lead); rands; f: the
// difficulty, +0xb8, +0xc0, the vehicle's place, the weapon's muzzle and line, now, dt, the
// plan's place, the contact's delta and its dist, the aim's words (active by in[6]).
extern "C" int32_t probe_pm_ai_aim(const int32_t* in, const int32_t* rands, const float* f, uint32_t* out, int32_t cap) {
  W w;
  w.in = in, w.f = f, w.rands = rands;
  std::memcpy(w.pos, f + 14, 12);
  std::memcpy(w.c.delta, f + 17, 12);
  w.c.dist = f[20];
  AimPmHooks* saved = &aim_pm_hooks();
  set_aim_pm_hooks(&w);
  AimPm a;
  std::memcpy(a.offset, f + 21, 12), std::memcpy(a.drift, f + 24, 12);
  a.active = in[6] != 0, a.until = f[27], a.limit2 = f[28];
  const zgame::Handle weapon = in[4] ? 7 : 0;
  if (in[5] == 0) a.track(1, weapon, nullptr);
  else a.lead(f[13], 1, weapon, 2, nullptr);
  set_aim_pm_hooks(saved);
  std::vector<uint32_t>& l = w.log;
  l.push_back(0x2b);
  for (int i = 0; i < 3; ++i) l.push_back(W::bits(a.offset[i]));
  for (int i = 0; i < 3; ++i) l.push_back(W::bits(a.drift[i]));
  l.push_back(a.active), l.push_back(W::bits(a.until)), l.push_back(W::bits(a.limit2));
  l.push_back(static_cast<uint32_t>(w.nrand));
  const int32_t n = static_cast<int32_t>(l.size());
  if (n > cap) return -1;
  std::memcpy(out, l.data(), 4 * l.size());
  return n;
}

// The plan's pick, for test_pm_ai_pick.py. in: 16 ints, then 16 for each contact; f: 18 floats, then
// 18 for each contact. Each handle is its kind's base plus the contact's number.
namespace {
enum { kEnt = 0x100, kVeh = 0x200, kGrp = 0x300, kMech = 0x400, kPart = 0x1000, kMe = 0x50, kPlayer = 0x51, kAimed = 0x52 };
constexpr int kI = 16, kF = 18, kCI = 16, kCF = 18;
struct P : PickPmHooks {
  const int32_t* in;
  const float* f;
  const int32_t* rands;
  int nrand = 0, nselect = 0;
  std::vector<Contact*> list;
  std::vector<Contact> store;
  Contact* target = nullptr;
  std::vector<uint32_t> log;
  float mine[3], aim_at[3];
  const int32_t* ci(int i) const { return in + kI + kCI * i; }
  const float* cf(int i) const { return f + kF + kCF * i; }
  int of(zgame::Handle h) const { return static_cast<int>(h & 0xff); }
  std::vector<Contact*>& contacts(zgame::Handle) override { return list; }
  Contact** target_slot(zgame::Handle) override { return &target; }
  uint32_t allies(zgame::Handle) override { return static_cast<uint32_t>(in[10]); }
  const float* forward(zgame::Handle) override { return f + 9; }
  bool faces(zgame::Handle, const float dir[3]) override {
    log.push_back(0x73);
    for (int i = 0; i < 3; ++i) log.push_back(W::bits(dir[i]));
    return in[11] & 1;
  }
  bool in_sight(zgame::Handle, zgame::Handle e) override {
    log.push_back(0x74), log.push_back(static_cast<uint32_t>(of(e)));
    return ci(of(e))[1] != 0;
  }
  int index(const Contact* c) const {
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i] == c) return static_cast<int>(i);
    return -1;
  }
  bool select(zgame::Handle, Contact* c) override {
    log.push_back(0x71), log.push_back(static_cast<uint32_t>(index(c)));
    const bool ok = (in[12] >> (nselect++ & 15)) & 1;
    if (ok) target = c;
    return ok;
  }
  void select_none(zgame::Handle) override { log.push_back(0x72), target = nullptr; }
  zgame::Handle contact_entity(const Contact* c) override {
    const int i = index(c);
    return i >= 0 && ci(i)[0] ? kEnt + i : 0;
  }
  zgame::Handle contact_mech(const Contact* c) override {
    const int i = index(c);
    return i >= 0 && ci(i)[0] && ci(i)[13] ? kMech + i : 0;
  }
  bool active(zgame::Handle e) override { return ci(of(e))[2] != 0; }
  uint32_t team(zgame::Handle e) override { return static_cast<uint32_t>(ci(of(e))[3]); }
  uint8_t flags(zgame::Handle e) override { return static_cast<uint8_t>(ci(of(e))[9]); }
  zgame::Handle group(zgame::Handle e) override { return ci(of(e))[4] ? kGrp + of(e) : 0; }
  uint32_t group_marker(zgame::Handle g) override { return static_cast<uint32_t>(ci(of(g))[5]); }
  uint32_t group_xe8(zgame::Handle g) override { return static_cast<uint32_t>(ci(of(g))[6]); }
  zgame::Handle vehicle(zgame::Handle e) override { return ci(of(e))[7] ? kVeh + of(e) : 0; }
  zgame::Handle vehicle_x58(zgame::Handle m) override { const int v = ci(of(m))[8]; return v == 2 ? kPlayer : v ? 0x77 : 0; }
  uint32_t vehicle_x100(zgame::Handle m) override { return static_cast<uint32_t>(ci(of(m))[10]); }
  uint32_t entity_x50(zgame::Handle e) override { return static_cast<uint32_t>(ci(of(e))[11]); }
  float health(zgame::Handle e) override { return cf(of(e))[8]; }
  zgame::Handle mech(zgame::Handle e) override { return ci(of(e))[13] ? kMech + of(e) : 0; }
  float heat(zgame::Handle m) override { return cf(of(m))[9]; }
  zgame::Handle part(zgame::Handle m, int32_t offset) override { return kPart + of(m) * 0x100 + (offset - 0x39c); }
  uint32_t part_child(zgame::Handle m, int32_t leg) override { return static_cast<uint32_t>(kPart + 0x80 + of(m) * 0x100 + leg); }
  float part_health(zgame::Handle p) override { const int i = (static_cast<int>(p) - kPart) / 0x100; return cf(i)[10 + (static_cast<int>(p) - kPart) % 0x100 / 4]; }
  const float* position(zgame::Handle e) override { return e == kAimed ? f + 12 : f + 15; }
  zgame::Handle player() override { return kPlayer; }
  zgame::Handle threat(Brain*, int32_t list_) override { const int v = in[list_ ? 14 : 13]; return v ? kEnt + v - 1 : 0; }
  int32_t rand() override { return rands[nrand++]; }
  float now() override { return f[7]; }
};
}  // namespace

// in: 0 n, 1 pick order, 2 part mode, 3 +0xed, 4 +0xee, 5 marker, 6 plan state (0 none, 1 a live one, 2 one with no
// contact), 7 prefer+1, 8 next+1, 9 aimed (0 none, 1 down, 2 up), 10 allies, 11 faces, 12 select bits, 13, 14 threats+1,
// 15 target+1. each: 0 entity, 1 in sight, 2 active, 3 team, 4 group, 5 its marker, 6 its xe8, 7 vehicle, 8 x58, 9 flags,
// 10 x100, 11 x50, 13 mech.  f: 0..3 weights, 4 repick, 5 range, 6 reach, 7 now, 8 until, 9..11 forward, 12..14 the aimed
// place, 15..17 ours. each: 0 dist2, 1..3 delta, 4 dist, 8 health, 9 heat, 10.. parts by slot.
extern "C" int32_t probe_pm_ai_pick(const int32_t* in, const int32_t* rands, const float* f, uint32_t* out, int32_t cap) {
  P p;
  p.in = in, p.f = f, p.rands = rands;
  const int n = in[0];
  p.store.resize(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    Contact& c = p.store[static_cast<size_t>(i)];
    c.dist2 = p.cf(i)[0], std::memcpy(c.delta, p.cf(i) + 1, 12), c.dist = p.cf(i)[4];
    p.list.push_back(&c);
  }
  if (in[15]) p.target = p.list[static_cast<size_t>(in[15] - 1)];
  set_pick_pm_hooks(&p);
  PickFiring fi;
  fi.vehicle = kMe;
  fi.pilot.pick_order = in[1], fi.pilot.part_mode = in[2];
  fi.pilot.filter[0] = static_cast<uint8_t>(in[3]), fi.pilot.filter[1] = static_cast<uint8_t>(in[4]);
  fi.pilot.marker = static_cast<uint32_t>(in[5]);
  std::memcpy(fi.pilot.weights, f, 16);
  fi.pilot.repick = f[4], fi.pilot.range = f[5], fi.reach = f[6], fi.until = f[8];
  fi.prefer = in[7] ? kEnt + in[7] - 1 : 0, fi.prefer_next = in[8] ? kEnt + in[8] - 1 : 0;
  fi.aimed = in[9] ? kAimed : 0, fi.aimed_up = in[9] == 2;
  Contact held;
  Contact* held_p = in[6] == 1 ? &held : nullptr;
  if (in[6]) fi.plan = new PlanPm, fi.plan->slot = &held_p;
  PlanPm* const old = fi.plan;
  PlanPm* r = plan_pick(fi);
  auto& l = p.log;
  l.push_back(0x2b);
  if (!r) {
    l.push_back(0);
  } else if (r == old) {
    l.push_back(2);
  } else {
    l.push_back(1), l.push_back(r->slot == &p.target ? 99 : static_cast<uint32_t>(p.index(*r->slot)));
    l.push_back(W::bits(r->score)), l.push_back(r->faces), l.push_back(r->in_sight), l.push_back(r->part);
  }
  l.push_back(W::bits(fi.until)), l.push_back(fi.prefer_next ? 1 : 0), l.push_back(fi.aimed ? 1 : 0);
  l.push_back(static_cast<uint32_t>(p.index(p.target) + 1)), l.push_back(static_cast<uint32_t>(p.index(fi.seen) + 1));
  l.push_back(static_cast<uint32_t>(p.nrand));
  delete r;
  set_pick_pm_hooks(nullptr);
  const int32_t m = static_cast<int32_t>(l.size());
  if (m > cap) return -1;
  std::memcpy(out, l.data(), 4 * l.size());
  return m;
}
