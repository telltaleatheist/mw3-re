// PM's walking context (zai_walk_pm), for test_pm_ai_walk.py: a plan of steps, the context's fields after each.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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
  bool pull(zgame::Handle, const float* a, const float* b, float* o) override {
    if (!blocked) return false;
    const float y = b[1];
    const float x = on_from ? a[0] : (a[0] + b[0]) * 0.5f + 3.0f;
    const float z = on_from ? a[2] : (a[2] + b[2]) * 0.5f - 2.0f;
    o[0] = x, o[1] = y, o[2] = z;
    return true;
  }
  float* way_from() override { return brain_from; }
  bool has_pilot(zgame::Handle) override { return pilot; }
  float pilot_x544(zgame::Handle) override { return x544; }
  float now() override { return clock; }
  float speed_at(zgame::Handle, float t) override { return t * k_speed; }
  float stop_distance(zgame::Handle, float v) override { return v * k_stop; }
  float range_far() override { return far; }
  float range_near() override { return near; }
  zgame::Handle order_target() override { return 2; }
  const float* target_place(zgame::Handle t) override { return t == 2 ? target : place; }
  bool target_live(zgame::Handle t) override { return t == 2 && live; }
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
  WalkFightPm* b = nullptr;
  WalkGuardPm* g = nullptr;
  float drove[3] = {};
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
    } else if (op == "S") {
      st.step(w, *b);
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
      o += "\n";
    }
  }
  delete st.owned;
  if (static_cast<int>(o.size()) >= cap) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
