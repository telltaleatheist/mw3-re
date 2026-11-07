// PM's jump state (zai_jump_pm), for test_pm_ai_jump.py: a plan of steps, the state's fields and the calls after each.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mech3/zai_jump_pm.h"

namespace {
using namespace mech3::ai;
struct World : JumpPmHooks {
  float place[3] = {}, way[3] = {}, fw[2] = {}, from[3] = {};
  float goal_turn = 0, most = 1, clock = 0, k_speed = 1, up = 0, along = 0, gap = 0;
  bool steers = true, air = true, jets = true, pilot = true;
  int x588 = 0;
  std::string* log = nullptr;
  std::vector<const Contact*> none;
  void put(const char* n, float v) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    char b[32];
    std::snprintf(b, sizeof b, "%s=%x;", n, u);
    *log += b;
  }
  void position(zgame::Handle, float out[3]) override { out[0] = place[0], out[1] = place[1], out[2] = place[2]; }
  void forward(zgame::Handle, float* x, float* z) override { *x = fw[0], *z = fw[1]; }
  bool halted(zgame::Handle) override { return false; }
  bool order_steers(Brain*) override { return steers; }
  const std::vector<const Contact*>& contacts(zgame::Handle) override { return none; }
  zgame::Handle contact_entity(const Contact*) override { return 0; }
  bool targetable(zgame::Handle) override { return false; }
  float contact_dist2(const Contact*) override { return 0; }
  float turn_goal() override { return goal_turn; }
  float throttle_goal() override { return 0; }
  float max_throttle() override { return most; }
  zgame::Handle target_entity(zgame::Handle) override { return 0; }
  bool has_target_contact(zgame::Handle) override { return false; }
  bool pull(zgame::Handle, const float*, const float*, float*) override { return false; }
  float* way_from() override { return from; }
  bool has_pilot(zgame::Handle) override { return pilot; }
  float pilot_x544(zgame::Handle) override { return 0; }
  float now() override { return clock; }
  double speed_at(zgame::Handle, float t) override { return double{t} * k_speed; }
  double stop_distance(zgame::Handle, float v) override { return v; }
  float range_far() override { return 0; }
  float range_near() override { return 0; }
  zgame::Handle order_target() override { return 0; }
  const float* target_place(zgame::Handle) override { return nullptr; }
  bool target_live(zgame::Handle) override { return false; }
  bool must_leave(zgame::Handle) override { return false; }
  const InOutVia* at_via(zgame::Handle) override { return nullptr; }
  void queue_via(const InOutVia*) override {}
  zgame::Handle mech(zgame::Handle) override { return pilot ? 3 : 0; }
  bool has_jets(zgame::Handle) override { return jets; }
  void set_x588(zgame::Handle) override { ++x588; }
  bool in_air(zgame::Handle) override { return air; }
  float climb_speed(zgame::Handle) override { return up; }
  float along_speed(zgame::Handle) override { return along; }
  const float* way_point() override { return way; }
  float margin() override { return gap; }
  float jet_climb() override { return 38.0f; }
  float gravity() override { return 30.0f; }
  void set_throttle(float v) override { put("throttle", v); }
  void set_turn(float v) override { put("turn", v); }
  void set_jet_forward(float v) override { put("forward", v); }
  void set_jump(float v) override { put("jump", v); }
  void go_next() override { *log += "next;"; }
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

extern "C" int probe_pm_ai_jump(const char* plan, char* out, int cap) {
  World w;
  SteerPm s;
  std::string calls;
  w.log = &calls;
  std::unique_ptr<JumpPm> c;
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
    std::string extra;
    calls.clear();
    if (op == "W") {
      for (int i = 0; i < 3; ++i) w.place[i] = f(a[i]), w.way[i] = f(a[3 + i]);
      w.fw[0] = f(a[6]), w.fw[1] = f(a[7]);
      w.goal_turn = f(a[8]), w.most = f(a[9]), w.k_speed = f(a[10]), w.up = f(a[11]), w.along = f(a[12]);
      w.gap = f(a[13]);
      w.air = a[14] == "1", w.jets = a[15] == "1", w.pilot = a[16] == "1";
    } else if (op == "N") {
      w.clock = f(a[0]);
    } else if (op == "C") {
      c.reset(new JumpPm(w, s, 1));
    } else if (op == "R") {
      c->run(f(a[0]));
    } else if (op == "J") {
      const bool jets = w.jets, pilot = w.pilot;
      w.jets = a[1] == "1", w.pilot = a[2] == "1";
      extra = JumpPm::can(w, a[0] == "1" ? 1 : 0) ? "can 1" : "can 0";
      w.jets = jets, w.pilot = pilot;
    } else if (op == "L") {  // the plan alone
      const float from[3] = {f(a[0]), f(a[1]), f(a[2])}, to[3] = {f(a[3]), f(a[4]), f(a[5])};
      const JumpPlanPm p = jump_plan_pm(from, to, f(a[6]), f(a[7]), f(a[8]), f(a[9]));
      for (float v : p.t) put(extra, v);
      for (float v : {p.speed, p.climb, p.fall, p.peak, p.need, p.dist, p.rise, p.ratio, p.k, p.total}) put(extra, v);
    } else if (op == "T") {  // the turn alone
      const float fwd[3] = {f(a[0]), 0, f(a[1])}, to[3] = {f(a[2]), 0, f(a[3])};
      const TurnPmParams p{f(a[5]), f(a[6]), f(a[7]), f(a[8]), f(a[9])};
      float dot = 0, cross = 7.0f;  // what it held
      const float r = turn_pm(fwd, to, &dot, &cross, f(a[4]), p);
      put(extra, r), put(extra, dot), put(extra, cross);
    }
    if (c) {
      put(o, c->speed), put(o, c->x24), put(o, c->far), put(o, c->peak);
      for (float v : c->t) put(o, v);
      put(o, w.from[0]), put(o, w.from[1]), put(o, w.from[2]);
      put(o, s.dir[0]), put(o, s.dir[1]), put(o, s.along_min), put(o, s.length), put(o, s.along_at), put(o, s.rate);
    }
    o += extra + " | ";
    if (c) o += std::to_string(c->phase) + " " + std::to_string(c->x2c) + " ";
    o += "x588:" + std::to_string(w.x588 > 0) + " " + calls + "\n";
  }
  if (static_cast<int>(o.size()) >= cap) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
