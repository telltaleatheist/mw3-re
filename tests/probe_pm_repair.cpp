// PM's repair state (zai_drive_pm: RepairStatePm, 0x004e03c0 / 0x004e0790 / 0x004e0570) for test_pm_repair_state.py:
// a plan of steps on one state and one steering, the fields after each as 32-bit words.
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "mech3/zai_drive_pm.h"
#include "mech3/zai_steer_pm.h"

namespace {
using namespace mech3::ai;

struct World : WalkPmHooks {
  float place[3] = {}, fw[2] = {0, 1}, goal_turn = 0, goal_throttle = 0, most = 1, brain_from[3] = {};
  float clock = 0, k_speed = 1, k_stop = 1;
  bool blocked = false, on_from = false, steers = true, via = false, drive = false, contact = false, down = false;
  int32_t hold_v = 0, runs = 0, pops = 0, queued = 0, powers = 0, power_last = -1, sends = 0;
  float sent[2] = {};
  std::vector<const Contact*> none;
  void position(zgame::Handle, float out[3]) override { out[0] = place[0], out[1] = place[1], out[2] = place[2]; }
  void forward(zgame::Handle, float* x, float* z) override { *x = fw[0], *z = fw[1]; }
  bool halted(zgame::Handle) override { return false; }
  bool order_steers(Brain*) override { return steers; }
  const std::vector<const Contact*>& contacts(zgame::Handle) override { return none; }
  zgame::Handle contact_entity(const Contact*) override { return 0; }
  bool targetable(zgame::Handle) override { return false; }
  float contact_dist2(const Contact*) override { return 0; }
  float turn_goal() override { return goal_turn; }
  float throttle_goal() override { return goal_throttle; }
  float max_throttle() override { return most; }
  zgame::Handle target_entity(zgame::Handle) override { return 0; }
  bool has_target_contact(zgame::Handle) override { return false; }
  bool pull(zgame::Handle, const float* a, const float* b, float* o) override {  // test_pm_ai_walk.py's stand-in
    if (!blocked) return false;
    const float y = b[1];
    const float x = on_from ? a[0] : (a[0] + b[0]) * 0.5f + 3.0f;
    const float z = on_from ? a[2] : (a[2] + b[2]) * 0.5f - 2.0f;
    o[0] = x, o[1] = y, o[2] = z;
    return true;
  }
  float* way_from() override { return brain_from; }
  bool has_pilot(zgame::Handle) override { return true; }
  float pilot_x544(zgame::Handle) override { return 0; }
  float now() override { return clock; }
  double speed_at(zgame::Handle, float t) override { return static_cast<double>(t) * k_speed; }  // the register, unrounded
  double stop_distance(zgame::Handle, float s) override { return static_cast<double>(s) * k_stop; }
  float range_far() override { return 0; }
  float range_near() override { return 0; }
  zgame::Handle order_target() override { return 0; }
  const float* target_place(zgame::Handle) override { return place; }
  bool target_live(zgame::Handle) override { return true; }
  bool must_leave(zgame::Handle) override { return false; }
  void set_throttle(float v) override { sent[1] = v, ++sends; }
  void set_turn(float v) override { sent[0] = v, ++sends; }
  const InOutVia* at_via(zgame::Handle) override { return via ? reinterpret_cast<const InOutVia*>(this) : nullptr; }
  void queue_via(const InOutVia*) override { ++queued; }
  void order_pop() override { ++pops; }
  bool dock_drive() override { return drive; }
  bool dock_contact() override { return contact; }
  bool powered_down(zgame::Handle) override { return down; }
  void mech_power(zgame::Handle, int32_t d) override { ++powers, power_last = d; }
  int32_t hold() override { return hold_v; }
  void set_hold(int32_t on) override { hold_v = on; }
  void set_runs_down(bool on) override { runs = on ? 1 : 0; }
};

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
float f_of(const std::string& s) {
  const uint32_t u = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
}  // namespace

// Lines: "S dx dy dz px py pz" starts the state at the dock point d from place p; "W ..." the world (see the test);
// "R" runs a frame and "E" ends it, each adding a line of words. Floats come as hex words.
extern "C" int probe_pm_repair(const char* plan, char* out, int cap) {
  World w;
  SteerPm s;
  RepairStatePm r;
  float dock[3] = {};
  std::string o;
  std::istringstream lines(plan);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream in(line);
    std::string op;
    in >> op;
    std::vector<std::string> a;
    for (std::string t; in >> t;) a.push_back(t);
    if (op == "S") {
      for (int i = 0; i < 3; ++i) dock[i] = f_of(a[i]);
      const float p[3] = {f_of(a[3]), f_of(a[4]), f_of(a[5])};
      r.start(dock, p);
    } else if (op == "P") {  // the state's fields set: phase, own_fights, slow, inv, until
      r.phase = std::stoi(a[0]), r.own_fights = static_cast<uint8_t>(std::stoi(a[1]));
      r.slow = f_of(a[2]), r.inv_slow = f_of(a[3]), r.until = f_of(a[4]);
    } else if (op == "W") {
      for (int i = 0; i < 3; ++i) w.place[i] = f_of(a[i]);
      w.fw[0] = f_of(a[3]), w.fw[1] = f_of(a[4]), w.goal_turn = f_of(a[5]), w.goal_throttle = f_of(a[6]);
      w.most = f_of(a[7]), w.clock = f_of(a[8]), w.k_speed = f_of(a[9]), w.k_stop = f_of(a[10]);
      w.blocked = a[11] == "1", w.on_from = a[12] == "1", w.via = a[13] == "1", w.steers = a[14] == "1";
      w.drive = a[15] == "1", w.contact = a[16] == "1", w.down = a[17] == "1", w.hold_v = std::stoi(a[18]);
    } else if (op == "R" || op == "E") {
      if (op == "R") s.update(w, 0.05f, 1), r.run(w, s, 1);  // the tick's steer_update first
      else r.end(w, 1);
      std::vector<uint32_t> v = {static_cast<uint32_t>(r.phase), r.own_fights, bits(r.slow), bits(r.inv_slow), bits(r.until)};
      for (float f : r.pulled) v.push_back(bits(f));
      for (float f : r.approach) v.push_back(bits(f));
      v.push_back(bits(w.sent[0])), v.push_back(bits(w.sent[1]));
      for (int32_t x : {w.sends, w.powers, w.power_last, w.hold_v, w.runs, w.pops, w.queued}) v.push_back(static_cast<uint32_t>(x));
      for (float f : w.brain_from) v.push_back(bits(f));
      for (float f : {s.start[0], s.start[1], s.start[2], s.dir[0], s.dir[1], s.along_min, s.length, s.along_at, s.across,
                      s.along, s.side, s.x40})
        v.push_back(bits(f));
      for (size_t i = 0; i < v.size(); ++i) o += (i ? " " : "") + std::to_string(v[i]);
      o += "\n";
    }
  }
  if (static_cast<int>(o.size()) >= cap) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
