// PM's walking context (zai_walk_pm), for test_pm_ai_walk.py: a plan of steps, the context's fields after each.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mech3/zai_attack_pm.h"
#include "mech3/zai_drive_pm.h"
#include "mech3/zainet_inout.h"

namespace {
using namespace mech3::ai;
int g_deleted = 0;
struct Owned : StateOwnedPm {
  ~Owned() override { ++g_deleted; }
};
struct World : WalkPmHooks {
  float place[3] = {}, target[3] = {}, fw[2] = {};
  float goal_turn = 0, goal_throttle = 0, most = 1, x544 = 0;
  float brain_from[3] = {};
  bool on_from = false, live = true, leave = false;
  float clock = 0, k_speed = 1, k_stop = 1, far = 0, near = 0;
  bool halt = false, steers = true, blocked = false, contact = false, pilot = false;
  std::vector<const Contact*> none;
  void position(zgame::Handle e, float out[3]) override {
    const float* p = e == 1 ? place : target;
    out[0] = p[0], out[1] = p[1], out[2] = p[2];
  }
  void forward(zgame::Handle, float* x, float* z) override { *x = fw[0], *z = fw[1]; }
  bool halted(zgame::Handle) override { return halt; }
  bool order_steers(Brain*) override { return steers; }
  const std::vector<const Contact*>& contacts(zgame::Handle) override { return none; }
  zgame::Handle contact_entity(const Contact*) override { return 0; }
  bool targetable(zgame::Handle) override { return false; }
  float contact_dist2(const Contact*) override { return 0; }
  float turn_goal() override { return goal_turn; }
  float throttle_goal() override { return goal_throttle; }
  float max_throttle() override { return most; }
  zgame::Handle target_entity(zgame::Handle) override { return 2; }
  bool has_target_contact(zgame::Handle) override { return contact; }
  float asked[3] = {};
  bool pull(zgame::Handle, const float* a, const float* b, float* o) override {
    asked[0] = b[0], asked[1] = b[1], asked[2] = b[2];
    if (!blocked) return false;
    const float y = b[1];
    const float x = on_from ? a[0] : (a[0] + b[0]) * 0.5f + 3.0f;
    const float z = on_from ? a[2] : (a[2] + b[2]) * 0.5f - 2.0f;
    o[0] = x, o[1] = y, o[2] = z;
    return true;
  }
  float* way_from() override { return brain_from; }
  zgame::Handle next_along(const float d[3]) override {
    along[0] = d[0], along[1] = d[1], along[2] = d[2];
    return next == 2 ? 0x7700 : 0;
  }
  void set_behaviour(zgame::Handle b) override { ++taken, last_taken = static_cast<int>(b); }
  void set_again() override { again = 1; }
  void order_ready() override { ready = 1; }
  void return_back() override { ++backs; }
  int again = 0, ready = 0, backs = 0;
  int32_t rand_int() override { return rolls[roll++ & 1]; }
  double jump_fuel(zgame::Handle) override { return m_fuel; }
  bool airborne(zgame::Handle) override { return m_air; }
  float forward_speed(zgame::Handle) override { return m_z; }
  double move_speed(zgame::Handle) override { return m_speed; }
  void order_pop() override { ++m_pops; }
  zgame::Handle contact_pick(zgame::Handle, int32_t k) override { return k == 1 ? m_pick1 : m_pick3; }
  bool hit_lately(zgame::Handle) override { return m_hit; }
  zgame::Handle attacker(zgame::Handle) override { return 0x6601; }
  bool lead_point(zgame::Handle, zgame::Handle t, float o[3]) override {
    if (!pilot || !contact_same(t) || !m_lead_on) return false;
    o[0] = m_lead[0], o[1] = m_lead[1], o[2] = m_lead[2];
    return true;
  }
  bool contact_same(zgame::Handle t) const { return m_same && t == m_target0; }
  const float* entity_place(zgame::Handle) override { return m_place; }
  void set_fought(bool on) override { m_fought = on; }
  bool fought() override { return m_fought; }
  uint8_t params_ec() override { return m_ec; }
  float ceiling() override { return m_ceiling; }
  void set_jump(float v) override { m_sent[0] = v; }
  void set_jet_forward(float v) override { m_sent[1] = v; }
  void stop_controls() override { ++m_stops; }
  void manoeuvre_over() override { --m_over; }
  int m_stops = 0, m_over = 0;
  float m_fuel = 0, m_speed = 0, m_z = 0, m_ceiling = 0, m_place[3] = {}, m_lead[3] = {}, m_sent[2] = {};
  bool m_air = false, m_hit = false, m_fought = false, m_same = false, m_lead_on = false;
  zgame::Handle m_pick1 = 0, m_pick3 = 0, m_target0 = 0;
  int m_pops = 0;
  uint8_t m_ec = 0;
  float reach() override { return reach_at; }
  int next = 0, taken = 0, last_taken = 0, roll = 0;
  int32_t rolls[2] = {};
  float along[3] = {}, reach_at = 0;
  bool has_pilot(zgame::Handle) override { return pilot; }
  float pilot_x544(zgame::Handle) override { return x544; }
  float now() override { return clock; }
  double speed_at(zgame::Handle, float t) override { return double{t} * k_speed; }
  double stop_distance(zgame::Handle, float v) override { return double{v} * k_stop; }
  float range_far() override { return far; }
  float range_near() override { return near; }
  zgame::Handle order_target() override { return 2; }
  const float* target_place(zgame::Handle t) override { return t == 2 ? target : place; }
  bool target_live(zgame::Handle t) override { return t == 2 && live; }
  const float* target_facing(zgame::Handle) override { return face_on ? face : nullptr; }
  double stopping_at(zgame::Handle, float t) override { return double{t} * k_ahead; }
  float x94() override { return m_x94; }
  void clear_mode() override { ++m_cleared; }
  float k_ahead = 1, m_x94 = 0;
  int m_cleared = 0;
  bool face_on = false;
  float face[3] = {};
  bool must_leave(zgame::Handle) override { return leave; }
  void set_throttle(float v) override { sent[1] = v, ++sends; }
  void set_turn(float v) override { sent[0] = v, ++sends; }
  const InOutVia* at_via(zgame::Handle) override { return via_on ? &via : nullptr; }
  void queue_via(const InOutVia* v) override {
    queued_net = reinterpret_cast<uintptr_t>(v->net), queued_node = reinterpret_cast<uintptr_t>(v->node), ++queued;
  }
  float sent[2] = {};
  int sends = 0, queued = 0;
  uintptr_t queued_net = 0, queued_node = 0;
  bool via_on = false;
  InOutVia via;
};
float f(const std::string& s) {
  uint32_t u = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
  float v;
  std::memcpy(&v, &u, 4);
  return v;
}
void put(std::string& o, float v) {
  uint32_t u;
  std::memcpy(&u, &v, 4);
  char b[16];
  std::snprintf(b, sizeof b, "%x ", u);
  o += b;
}
}  // namespace

extern "C" int probe_pm_ai_walk(const char* plan, char* out, int cap) {
  World w;
  SteerPm s;
  std::unique_ptr<WalkPm> c;
  FightStatePm st;
  MoveStatePm ms;
  ManoeuvrePm man;
  GuardPickPm pick;
  WalkFightPm* b = nullptr;
  WalkGuardPm* g = nullptr;
  float drove[3] = {};
  BehindPm bh;
  bool behind = false, circling = false, guarding = false, attacking = false;
  AttackStatePm as;
  GuardStatePm gs;
  CirclePm cp;
  g_deleted = 0;
  std::string o;
  std::istringstream lines(plan);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream in(line);
    std::string op;
    in >> op;
    std::vector<std::string> a;
    std::string t;
    while (in >> t) a.push_back(t);
    if (op == "W") {  // the world
      for (int i = 0; i < 3; ++i) w.place[i] = f(a[i]), w.target[i] = f(a[3 + i]);
      w.fw[0] = f(a[6]), w.fw[1] = f(a[7]);
      w.goal_turn = f(a[8]), w.goal_throttle = f(a[9]), w.most = f(a[10]);
      w.blocked = a[11] == "1", w.contact = a[12] == "1", w.steers = a[13] == "1";
      w.pilot = a[14] == "1", w.x544 = f(a[15]);
    } else if (op == "T") {  // the steering's fields
      s.start[0] = f(a[0]), s.start[1] = f(a[1]), s.start[2] = f(a[2]);
      s.dir[0] = f(a[3]), s.dir[1] = f(a[4]);
      s.along_min = f(a[5]), s.length = f(a[6]), s.along_at = f(a[7]), s.across = f(a[8]);
      s.along = f(a[9]), s.side = f(a[10]);
    } else if (op == "F") {  // what the fight reads
      w.k_speed = f(a[0]), w.k_stop = f(a[1]), w.far = f(a[2]), w.near = f(a[3]), w.clock = f(a[4]);
    } else if (op == "B") {
      b = new WalkFightPm(w, s, 1, st);
      c.reset(b);
    } else if (op == "@") {
      static const float held[3] = {100.0f, 0.0f, 50.0f};
      b = new WalkAttackPm(w, s, 1, st, f(a[0]) != 0.0f ? held : nullptr, static_cast<int32_t>(f(a[1])));
      c.reset(b);
    } else if (op == "Y") {
      const float away[2] = {f(a[0]), f(a[1])};
      drive_pm(w, s, 1, away, f(a[2]), f(a[3]), &drove[0], &drove[1], f(a[4]), f(a[5]), f(a[6]));
    } else if (op == "X") {
      float d[2];
      away_pm(s, d, &drove[2]);
      drove[0] = d[0], drove[1] = d[1];
    } else if (op == "U") {
      w.via_on = a[0] == "1";
      w.via.net = reinterpret_cast<const void*>(uintptr_t(0x1111)), w.via.node = reinterpret_cast<const void*>(uintptr_t(0x2222));
    } else if (op == "a") {
      w.reach_at = f(a[0]);
    } else if (op == "j") {  // the target's facing
      w.face_on = a[0] == "1", w.face[0] = f(a[1]), w.face[2] = f(a[2]);
    } else if (op == "w") {
      bh = BehindPm{}, behind = true;
      bh.start(w, 1);
    } else if (op == "x") {
      bh.run(w, s, 1);
    } else if (op == "Q") {  // the guarding state, its rolls and the brain's +0x44
      w.rolls[0] = std::stoi(a[0]), w.rolls[1] = std::stoi(a[1]), w.roll = 0;
      w.reach_at = f(a[2]);
      gs = GuardStatePm{}, guarding = true;
      gs.start(w, 1, 0x1234);
    } else if (op == "e") {
      w.rolls[0] = std::stoi(a[0]), w.rolls[1] = std::stoi(a[1]), w.roll = 0;
    } else if (op == "g") {
      gs.run(w, s, 1);
    } else if (op == "I") {
      as = AttackStatePm{}, attacking = true;
      as.start(w, 1, 0x1234);
    } else if (op == "y") {  // the circling state, with the brain's +0x94 and vt+0x134's constant
      w.m_x94 = f(a[0]), w.k_ahead = f(a[1]), w.reach_at = f(a[2]);
      cp = CirclePm{}, circling = true, w.m_cleared = 0;
      cp.start(w, 0x1234);
    } else if (op == "z") {
      cp.run(w, 1);
    } else if (op == "!") {
      cp.change_side(w.clock);
    } else if (op == "b") {  // the follow's state made
      st = FightStatePm();
      follow_start(w, s, 1, st);
    } else if (op == "r") {  // and run
      follow_run(w, s, 1, st);
    } else if (op == "S") {
      st.step(w, *b);
    } else if (op == "m") {
      ms.step = std::stoi(a[0]), w.next = std::stoi(a[1]);
      ms.dist = f(a[2]), ms.slow = f(a[3]), ms.wake = f(a[4]), ms.turn = f(a[5]), ms.throttle = f(a[6]);
      ms.run(w, s, 1, w.place);
    } else if (op == "k") {  // the backup node's run, likewise
      ms.step = std::stoi(a[0]), w.next = std::stoi(a[1]);
      ms.dist = f(a[2]), ms.slow = f(a[3]), ms.wake = f(a[4]), ms.turn = f(a[5]), ms.throttle = f(a[6]);
      ms.run_backup(w, s, 1, w.place);
    } else if (op == "t") {  // the terminus node's run, likewise
      ms.step = std::stoi(a[0]);
      ms.dist = f(a[2]), ms.slow = f(a[3]), ms.wake = f(a[4]), ms.turn = f(a[5]), ms.throttle = f(a[6]);
      ms.run_terminus(w, s, 1, w.place);
    } else if (op == "u") {
      man.no_jets = std::stoi(a[0]), man.phase = std::stoi(a[1]);
      w.m_air = a[2] == "1", w.m_hit = a[5] == "1";
      w.m_pick1 = a[3] == "1" ? 0x6603 : 0, w.m_pick3 = a[4] == "1" ? 0x6604 : 0;
      man.target = static_cast<zgame::Handle>(std::stoi(a[6]));
      w.m_ec = static_cast<uint8_t>(std::stoi(a[7]));
      w.m_fought = a[8] == "1", w.pilot = a[9] == "1", w.m_same = a[10] == "1", w.m_target0 = man.target;
      w.m_lead_on = a[11] == "1";
      man.jump = f(a[12]), man.turn = f(a[13]), man.until = f(a[14]);
      w.m_fuel = f(a[15]), w.m_speed = f(a[16]), w.m_z = f(a[17]), w.m_ceiling = f(a[18]);
      for (int i = 0; i < 3; ++i) w.m_place[i] = f(a[19 + i]), w.m_lead[i] = f(a[22 + i]);
      man.run(w, 1);
    } else if (op == "l") {
      man.leave(w);
    } else if (op == "n") {
      ms.start(w, s, 1, w.place);
    } else if (op == "q") {
      w.rolls[0] = std::stoi(a[0]), w.rolls[1] = std::stoi(a[1]), w.roll = 0;
      pick.rate = f(a[2]), pick.reach = f(a[3]), w.reach_at = f(a[4]);
      if (g) g->pick(pick);
    } else if (op == "V") {
      w.live = a[0] == "1", w.leave = a[1] == "1";
    } else if (op == "A") {  // the whole of 0x004daa70: the context made again
      b = new WalkFightPm(w, s, 1, st);
      c.reset(b);
      b->take_order_target();
    } else if (op == "H") {
      b = nullptr;
      g = new WalkGuardPm(w, s, 1);
      c.reset(g);
    } else if (op == "Z") {
      w.on_from = a[0] == "1";
    } else if (op == "P") {
      st.plan(w, *b);
    } else if (op == "E") {
      st.enter(w, 1);
    } else if (op == "R") {
      st.reset();
    } else if (op == "f") {  // the state's own fields, as it would have them
      st.flags = static_cast<uint32_t>(std::stoul(a[0], nullptr, 16)), st.wake = f(a[1]), st.until = f(a[2]);
      st.pulled = st.x2d = 1;
    } else if (op == "L") {
      st.owned = new Owned;
    } else if (op == "K") {
      st.release();
    } else if (op == "N") {
      b->target_pos = a[0] == "1" ? w.target : nullptr;
    } else if (op == "D") {
      b->measure_target();
    } else if (op == "M") {
      c->measure();
    } else if (op == "G") {
      b->decide(st);
    } else if (op == "O") {
      c->dot = f(a[0]), c->off_along = f(a[1]), c->off_side = f(a[2]);
    } else if (op == "C") {
      c.reset(new WalkPm(w, s, 1));
    } else if (op == "0") {
      c->walk();
    } else if (op == "1") {
      c->walk_on();
    } else if (op == "2") {
      c->walk_straight();
    } else if (op == "3") {
      c->hold();
    } else if (op == "4") {
      c->measure();
    } else if (op == "5") {
      c->aim_turnabout();
    } else if (op == "6") {
      c->turn();
    } else if (op == "7") {
      c->throttle();
    }
    if (c) {
      for (float v : {c->off_along, c->off_side, c->off_sum, c->to[0], c->to[1], c->to[2], c->dir[0], c->dir[2], c->dot,
                      c->cross, c->turn_out, c->throttle_out, s.brake, drove[0], drove[1], drove[2], s.rate, s.ease,
                      s.along, s.side, s.near_dist, s.x40, s.keep_near, s.x4c, s.keep_slope, s.keep_scale, s.push_reach,
                      s.push_band, s.far_along, s.far_side, s.cos_on, s.cos_turnabout, s.turnabout_room, s.gain_along,
                      s.gain_side, s.least, s.brake, s.cos_dead, s.sin_dead, s.gain, s.least_turn, s.cos_behind})
        put(o, v);
      for (float v : {ms.turn, ms.throttle, ms.dist, ms.slow, ms.wake, w.along[0], w.along[1], w.along[2], man.jump, man.turn, man.until, w.m_sent[0], w.m_sent[1], pick.at[0], pick.at[1], pick.at[2], w.brain_from[0], w.brain_from[1], w.brain_from[2]}) put(o, v);
      if (b) {
        for (float v : {b->to_target[0], b->to_target[1], b->to_target[2], b->target_dist, b->way_own[0], b->way_own[1],
                        b->way_own[2], b->way_dir[0], b->way_dir[1], b->way_dir[2], b->way_dist, st.stop_dist, st.range_far,
                        st.range_near, st.range_mid, st.x1c, st.speed, st.wake, st.until, s.start[0], s.start[1], s.start[2], s.dir[0],
                        s.dir[1], s.along_min, s.length, s.along_at, s.rate, s.along, s.side, w.brain_from[0], w.brain_from[1],
                        w.brain_from[2]})
          put(o, v);
      }
      if (g) {
        for (float v : {g->to_target[0], g->to_target[1], g->to_target[2], g->target_dist, g->from_way}) put(o, v);
      }
      if (behind) {
        for (float v : {bh.point[0], bh.point[1], bh.point[2], bh.turn, bh.throttle, bh.speed, bh.stop, bh.inv_stop, bh.x34, bh.x38,
                        s.start[0], s.start[1], s.start[2], s.dir[0], s.dir[1], s.along_min, s.length, s.along_at, s.rate, s.along,
                        s.side, w.brain_from[0], w.brain_from[1], w.brain_from[2], w.sent[0], w.sent[1]})
          put(o, v);
      }
      if (guarding) {
        for (float v : {gs.at[0], gs.at[1], gs.at[2], gs.rate, gs.far_way, gs.most, gs.reach, s.start[0], s.start[1], s.start[2], s.dir[0],
                        s.dir[1], s.along_min, s.length, s.along_at, s.rate, s.along, s.side, w.brain_from[0], w.brain_from[1],
                        w.brain_from[2], w.sent[0], w.sent[1]})
          put(o, v);
      }
      if (attacking) {
        for (float v : {as.stop_dist, as.range_far, as.range_near, as.range_mid, as.x1c, as.speed, as.wake, as.until}) put(o, v);
      }
      if (circling) {
        for (float v : {cp.turn, cp.throttle, cp.side, w.asked[0], w.asked[1], w.asked[2], w.sent[0], w.sent[1]}) put(o, v);
      }
      if (b) {
        put(o, w.sent[0]), put(o, w.sent[1]);
      }
      o += "| " + std::to_string(c->backing) + " " + std::to_string(int(s.keep)) + " " + std::to_string(int(s.push));
      if (g) o += " " + std::to_string(int(g->gone)) + " " + std::to_string(int(g->x70)) + " " + std::to_string(int(g->x71)) + " " + std::to_string(int(g->x72));
      if (b) {
        const int way = !b->way_point ? 0 : b->way_point == b->way_own ? 1 : 2;
        for (int v : {int(b->in_range), int(b->at_stop), int(b->no_target), way, int(b->may_go), int(b->off_way),
                      int(st.flags & 0xff), int(st.pulled), int(st.x2d), int(st.owned != nullptr), int(st.x34), g_deleted,
                      g_deleted ? 1 : 0, w.sends, w.queued, int(w.queued_net), int(w.queued_node)})
          o += " " + std::to_string(v);
      }
      if (behind) o += " " + std::to_string(bh.step) + " " + std::to_string(w.sends);
      if (guarding) o += " " + std::to_string(gs.x08) + " " + std::to_string(gs.step) + " 0 " + std::to_string(w.sends);
      if (attacking)
        o += " " + std::to_string(as.flags) + " " + std::to_string(as.pulled | as.x2d << 8) + " 0 " + std::to_string(as.x38) + " 0 0";
      if (circling) o += " " + std::to_string(cp.step) + " " + std::to_string(cp.x18) + " " + std::to_string(int(w.m_cleared > 0)) + " " + std::to_string(w.sends);
      o += " " + std::to_string(w.m_stops) + " " + std::to_string(static_cast<uint32_t>(w.m_over));
      o += " " + std::to_string(man.phase) + " " + std::to_string(man.target) + " " + std::to_string(w.m_pops) + " " + std::to_string(int(w.m_fought));
      o += " " + std::to_string(w.again) + " " + std::to_string(w.ready) + " " + std::to_string(w.backs);
      o += " " + std::to_string(ms.step) + " " + std::to_string(w.taken) + " " + std::to_string(w.last_taken);
      o += "\n";
    }
  }
  delete st.owned;
  if (static_cast<int>(o.size()) >= cap) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
