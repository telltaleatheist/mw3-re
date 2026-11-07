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

// The shot, for test_pm_ai_shot.py. in: n, a place, the shooter's vehicle, pilot, group's size, then 6 for each
// weapon: ready, can hit, ammo, a bin, its full; f: dist2, k, rate, the shooter's 7, then 6 for each: least, most,
// cost, value, heat, unused.
namespace {
struct Sh : ShotPmHooks {
  const int32_t* in;
  const float* f;
  std::vector<zgame::Handle> ws;
  std::vector<uint32_t> log;
  float at[3] = {1, 2, 3};
  const int32_t* wi(zgame::Handle w) const { return in + 5 + 6 * (w - 1); }
  const float* wf(zgame::Handle w) const { return f + 10 + 6 * (w - 1); }
  ShotShooter shooter(const Contact*) override {
    ShotShooter s;
    s.heat = f[3], s.health = f[4], s.vehicle = in[2] != 0, s.pilot = in[3] != 0;
    s.heat_most = f[5], s.rate_most = f[6], s.cost_least = f[7], s.health_past = f[8], s.ammo_keep = f[9];
    return s;
  }
  const std::vector<zgame::Handle>& weapons(zgame::Handle) override { return ws; }
  bool ready(zgame::Handle w) override { return wi(w)[0] != 0; }
  bool can_hit(zgame::Handle w, const float*) override { log.push_back(0x81), log.push_back(static_cast<uint32_t>(w)); return wi(w)[1] != 0; }
  double reach_least(zgame::Handle w) override { return wf(w)[0]; }
  double reach_most(zgame::Handle w) override { return wf(w)[1]; }
  float cost(zgame::Handle w) override { return wf(w)[2]; }
  float value(zgame::Handle w) override { return wf(w)[3]; }
  double dps(zgame::Handle w) override { return wf(w)[4]; }
  int32_t ammo(zgame::Handle w) override { return wi(w)[2]; }
  bool bin(zgame::Handle w, int32_t* full) override { *full = wi(w)[4]; return wi(w)[3] != 0; }
  void group_clear(zgame::Handle) override {
    if (in[4]) log.push_back(0x82), log.push_back(static_cast<uint32_t>(in[4]));
  }
  void group_add(zgame::Handle, zgame::Handle w, float rate) override { log.push_back(0x83), log.push_back(static_cast<uint32_t>(w)), log.push_back(W::bits(rate)); }
  void set_mode(zgame::Handle, int32_t mode) override { log.push_back(0x84), log.push_back(static_cast<uint32_t>(mode)); }
};
}  // namespace

extern "C" int32_t probe_pm_ai_shot(const int32_t* in, const float* f, uint32_t* out, int32_t cap) {
  Sh s;
  s.in = in, s.f = f;
  for (int i = 0; i < in[0]; ++i) s.ws.push_back(static_cast<zgame::Handle>(i + 1));
  Contact c;
  c.dist2 = f[0];
  set_shot_pm_hooks(&s);
  const bool r = shot_pick(9, &c, in[1] ? s.at : nullptr, f[1], f[2]);
  set_shot_pm_hooks(nullptr);
  s.log.push_back(0x2b), s.log.push_back(r);
  const int32_t m = static_cast<int32_t>(s.log.size());
  if (m > cap) return -1;
  std::memcpy(out, s.log.data(), 4 * s.log.size());
  return m;
}

// The step, for test_pm_ai_step.py. in: 0 start phase, 1 a mech, 2 a manager, 3 bits: busy, shot, seen, a far time; then for each frame: target,
// group, contact, plan quick; bits: the answers in order. f: 0 hold, 1 rest, 2 range, 3 rate, 4 quick, 5 delay, 6 rest
// field, 7..9 reach, track, fire, 10 too hot, 11 cool for, 12 cool below, 13 shot every, 14 reach of the best, then for
// each frame: now, heat, coolant, soonest.
namespace {
constexpr int kStepF = 15;
struct St : StepPmHooks {
  const int32_t* in;
  const float* f;
  const uint8_t* bits;
  int n = 0, fr = 0;
  bool fired = false, seen = false;
  Contact con;
  float at[3] = {};
  std::vector<uint32_t> log;
  bool bit() { return bits[n++] != 0; }
  void put(uint32_t k, uint32_t v = 0) { log.push_back(k), log.push_back(v); }
  const int32_t* fi() const { return in + 4 + 4 * fr; }
  const float* ff() const { return f + kStepF + 4 * fr; }
  zgame::Handle wm(zgame::Handle) override { return in[2] ? 5 : 0; }
  zgame::Handle mech(zgame::Handle) override { return in[1] ? 6 : 0; }
  Contact* target(zgame::Handle) override { return fi()[0] ? &con : nullptr; }
  double heat(zgame::Handle) override { return ff()[1]; }
  float coolant(zgame::Handle) override { return ff()[2]; }
  void flush(zgame::Handle) override { put(1); }
  float now() override { return ff()[0]; }
  float hold(Brain*) override { return f[0]; }
  float rest(Brain*) override { return f[1]; }
  float range(Brain*) override { return f[2]; }
  float rate(Brain*) override { return f[3]; }
  uint32_t xd0(Brain*) override { return 0x77; }
  void seed(uint32_t v) override { put(2, v); }
  void start_x50(uint32_t v) override { put(3, v); }
  void set_range(zgame::Handle, float r) override { put(4, W::bits(r)); }
  void look_start() override { put(5); }
  zgame::Handle best_weapon(zgame::Handle) override { put(0x15); return bit() ? 8 : 0; }
  float weapon_reach(zgame::Handle) override { return f[14]; }
  zgame::Handle pick(zgame::Handle) override { put(6); return bit() ? 9 : 0; }
  void plan_free(zgame::Handle) override { put(7); }
  bool plan_turn(zgame::Handle, float c) override { put(8, W::bits(c)); return bit(); }
  bool plan_cone(zgame::Handle, float c) override { put(9, W::bits(c)); return bit(); }
  bool plan_sight(zgame::Handle) override { put(10); seen = bit(); return seen; }
  bool plan_quick(zgame::Handle) override { return fi()[3] != 0; }
  bool plan_seen(zgame::Handle) override { return seen; }
  Contact* plan_contact(zgame::Handle) override { return fi()[2] ? &con : nullptr; }
  const float* plan_place(zgame::Handle, float, float) override { put(0x16); return at; }
  bool plan_fired(zgame::Handle) override { return fired; }
  void plan_set_fired(zgame::Handle, bool on) override { fired = on; }
  bool shot(zgame::Handle, Contact*, const float*, float a, float r) override { put(11, W::bits(a)), put(11, W::bits(r)); return bit(); }
  bool any_can_hit(zgame::Handle) override { put(12); return bit(); }
  void too_far(Brain*) override { put(13); }
  void trigger(Brain*, bool on) override { put(14, on); }
  zgame::Handle group(zgame::Handle) override { return fi()[1] ? 11 : 0; }
  double group_soonest(zgame::Handle) override { return ff()[3]; }
  bool clear_of_friends(zgame::Handle) override { put(0x14); return !bit(); }
  void aim_track(zgame::Handle, Brain*) override { put(16); }
  void aim_lead(float dt, zgame::Handle, zgame::Handle, Brain*) override { put(17, W::bits(dt)); }
  void stand_down(zgame::Handle) override { put(18); }
  void brain_x20(Brain*) override { put(19); }
};
}  // namespace

extern "C" int32_t probe_pm_ai_step(const int32_t* in, int32_t frames, const uint8_t* bits, const float* f, uint32_t* out, int32_t cap) {
  St s;
  s.in = in, s.f = f, s.bits = bits;
  set_step_pm_hooks(&s);
  FiringPm p;
  p.vehicle = 4, p.brain = nullptr, p.phase = in[0], p.busy = (in[3] & 1) != 0, p.plan = in[0] == 8 || in[0] == 0 ? 0 : 9;
  p.quick = f[4], p.delay = f[5], p.rest = f[6], p.reach_arg = f[7], p.track_arg = f[8], p.fire_arg = f[9];
  p.too_hot = f[10], p.cool_for = f[11], p.cool_below = f[12], p.shot_every = f[13], p.x50 = 0x55;
  p.wait = 0.25f, p.time = in[3] & 8 ? 1e6f : 0;
  s.fired = (in[3] & 2) != 0, s.seen = (in[3] & 4) != 0;
  for (s.fr = 0; s.fr < frames; ++s.fr) {
    p.step(1.0f / 30);
    s.log.push_back(0x2b), s.log.push_back(static_cast<uint32_t>(p.phase)), s.log.push_back(W::bits(p.time));
    s.log.push_back(W::bits(p.wait)), s.log.push_back(W::bits(p.range)), s.log.push_back(p.busy);
    s.log.push_back(W::bits(p.cool_until)), s.log.push_back(W::bits(p.shot_next)), s.log.push_back(s.fired);
    s.log.push_back(W::bits(p.x40));
  }
  set_step_pm_hooks(nullptr);
  s.log.push_back(static_cast<uint32_t>(s.n));
  const int32_t m = static_cast<int32_t>(s.log.size());
  if (m > cap) return -1;
  std::memcpy(out, s.log.data(), 4 * s.log.size());
  return m;
}

// The plan's virtuals and the weapon checks, for test_pm_ai_plan.py. in: 0 op, 1 entity, 2 its mech, 3 grouped, 4 a
// group weapon, 5 aim ok, 6 there, 7 sight, 8 part, 9 n, 10 list by group, then 12 for each weapon: type, ammo, weapon_cos
// ok, mounted, spreads, buried, has mount, mount reaches, mech reaches, holder; f: 0 cone, 1..3 at, 4..6 weapon's place,
// 7..9 vehicle's, 10..18 matrix, 19..21 turn's out, 22..24 part, 25..27 entity's, 28..30 mount place... see the test.
namespace {
constexpr int kPI = 11, kPW = 10, kPF = 40, kWF = 12;
struct Pl : PlanPmHooks {
  const int32_t* in;
  const float* f;
  const int32_t* rands;
  int nr = 0;
  std::vector<zgame::Handle> ws;
  std::vector<uint32_t> log;
  const int32_t* wi(zgame::Handle w) const { return in + kPI + kPW * (w - 1); }
  const float* wf(zgame::Handle w) const { return f + kPF + kWF * (w - 1); }
  void vec(uint32_t k, const float* v) {
    log.push_back(k);
    for (int i = 0; i < 3; ++i) log.push_back(W::bits(v[i]));
  }
  zgame::Handle contact_entity(const Contact*) override { return in[1] ? 21 : 0; }
  zgame::Handle mech(zgame::Handle) override { return in[2] ? 22 : 0; }
  void refresh(zgame::Handle) override { log.push_back(0x91); }
  zgame::Handle wm(zgame::Handle) override { return 23; }
  zgame::Handle group_weapon(zgame::Handle) override { return in[4] ? 24 : 0; }
  bool aim_point(zgame::Handle, Contact*, zgame::Handle w, float out[3]) override {
    log.push_back(0x92), log.push_back(w ? 1 : 0);
    std::memcpy(out, f + 1, 12);
    return in[5] != 0;
  }
  void weapon_place(zgame::Handle, float out[3]) override { std::memcpy(out, f + 4, 12); }
  const float* position(zgame::Handle) override { return f + 7; }
  const float* matrix(zgame::Handle) override { return f + 10; }
  bool turn(zgame::Handle, const float dir[3], float out[3]) override {
    vec(0x93, dir);
    std::memcpy(out, f + 19, 12);
    return in[6] != 0;
  }
  void face(zgame::Handle, const float dir[3]) override { vec(0x94, dir); }
  void wm_aim_at(zgame::Handle, const float at[3]) override { vec(0x95, at); }
  const std::vector<zgame::Handle>& weapons(zgame::Handle) override { return ws; }
  const std::vector<zgame::Handle>& group(zgame::Handle) override { return ws; }
  bool weapon_cos(zgame::Handle w, zgame::Handle, float* cos) override {
    log.push_back(0x96), log.push_back(static_cast<uint32_t>(w));
    *cos = wf(w)[0];
    return wi(w)[2] != 0;
  }
  bool in_sight(zgame::Handle, zgame::Handle) override { log.push_back(0x97); return in[7] != 0; }
  void part_place(uint32_t, float out[3]) override { std::memcpy(out, f + 22, 12); }
  const float* entity_place(zgame::Handle) override { return f + 25; }
  bool has_type(zgame::Handle w) override { return wi(w)[0] != 0; }
  int32_t ammo(zgame::Handle w) override { return wi(w)[1]; }
  float blocked(zgame::Handle w) override { return wf(w)[1]; }
  double reach(zgame::Handle w) override { return wf(w)[2]; }
  zgame::Handle holder_mech(zgame::Handle w) override { return wi(w)[9] ? 22 : 0; }
  bool mounted(zgame::Handle w) override { return wi(w)[3] != 0; }
  zgame::Handle muzzle_node(zgame::Handle w) override { return wi(w)[3] ? 31 : 32; }
  const float* mount_place(zgame::Handle w) override { return wf(w) + 3; }
  bool spreads(zgame::Handle w) override { return wi(w)[4] != 0 && wi(w)[3] != 0; }
  float spread(zgame::Handle w) override { return wf(w)[6]; }
  const float* offset(zgame::Handle w) override { return wf(w) + 7; }
  bool buried(zgame::Handle node, float at[3]) override {
    log.push_back(0x98), log.push_back(static_cast<uint32_t>(node));
    vec(0x98, at);
    at[0] = at[0] + 7.0f, at[2] = at[2] - 3.0f;  // into the world, as the exe's
    for (zgame::Handle w : ws)
      if (w == g_cur) return wi(w)[5] != 0;
    return false;
  }
  bool has_mount(zgame::Handle w) override { return wi(w)[3] != 0; }
  bool mount_reaches(zgame::Handle w, const float dir[3]) override { vec(0x99, dir); return wi(w)[7] != 0; }
  bool mech_reaches(zgame::Handle, const float dir[3]) override { vec(0x9a, dir); return wi(g_cur)[8] != 0; }
  int32_t rand() override { return rands[nr++]; }
  zgame::Handle g_cur = 1;
};
}  // namespace

extern "C" int32_t probe_pm_ai_plan(const int32_t* in, const int32_t* rands, const float* f, uint32_t* out, int32_t cap) {
  Pl s;
  s.in = in, s.f = f, s.rands = rands;
  for (int i = 0; i < in[9]; ++i) s.ws.push_back(static_cast<zgame::Handle>(i + 1));
  set_plan_pm_hooks(&s);
  PlanPm p;
  p.vehicle = 20, p.part = in[8] ? 33 : 0;
  Contact a, b;
  std::memcpy(a.delta, f + 28, 12), a.dist = f[31];
  std::memcpy(b.delta, f + 32, 12), b.dist = f[35];
  int32_t r = 0;
  switch (in[0]) {
    case 0: r = plan_turn(p, &a, in[3] != 0, f[0]); break;
    case 1: r = plan_cone(p, &a, 23, in[3] != 0, 25, f[0]); break;
    case 2: s.vec(0x9b, plan_place(p, &a)); break;
    case 3: plan_sight(p, &a), r = p.seen; break;
    case 4: r = static_cast<int32_t>(best_weapon(23)); break;
    case 5: r = can_hit(1, f + 1); break;
    case 6: {
      bool any = false;
      for (zgame::Handle w : s.ws) {
        s.g_cur = w;
        if (can_hit(w, f + 1)) { any = true; break; }
      }
      r = any;
      break;
    }
    default: r = in_line(a, b); break;
  }
  set_plan_pm_hooks(nullptr);
  auto& l = s.log;
  l.push_back(0x2b), l.push_back(static_cast<uint32_t>(r));
  if (in[0] == 0) for (int i = 0; i < 3; ++i) l.push_back(W::bits(p.dir[i]));
  l.push_back(static_cast<uint32_t>(s.nr));
  const int32_t m = static_cast<int32_t>(l.size());
  if (m > cap) return -1;
  std::memcpy(out, l.data(), 4 * l.size());
  return m;
}

extern "C" void probe_pm_cone_table(float* out) { cone_table(out); }

// The firing's defaults, word by word from +0x00, for test_pm_ai_brain.py.
extern "C" void probe_pm_firing_seed(uint32_t* out) {
  FiringPm f;
  f.seed();
  const auto put = [&](int off, float v) { std::memcpy(out + off / 4, &v, 4); };
  for (int i = 0; i < 27; ++i) out[i] = 0;
  out[0] = f.phase;
  put(0x04, f.time), put(0x08, f.quick), put(0x0c, f.delay), put(0x10, f.rest), put(0x18, f.reach_arg), put(0x1c, f.track_arg);
  put(0x20, f.fire_arg), put(0x24, f.x24), put(0x38, f.too_hot), put(0x3c, f.x3c), put(0x40, f.x40), put(0x44, f.x44), put(0x48, f.x48);
  out[0x50 / 4] = f.x50;
  put(0x54, f.x54), put(0x58, f.cool_for), put(0x5c, f.cool_until), put(0x60, f.cool_below), put(0x64, f.shot_every);
}

extern "C" int32_t probe_pm_firepoint_reaches(const float* cone, const float* dir) { return firepoint_reaches(cone, dir); }
extern "C" int32_t probe_pm_torso_reaches(const float* lim, const float* dir) { return torso_reaches(lim[0], lim[1], lim[2], dir); }
