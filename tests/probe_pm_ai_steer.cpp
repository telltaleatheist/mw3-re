// PM's steering (zai_steer_pm), for test_pm_ai_steer.py: a plan of steps on one SteerPm, each step's
// fields as words.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "mech3/zai_steer_pm.h"

namespace {
using namespace mech3::ai;
struct World : SteerPmHooks {
  std::map<uint32_t, std::vector<float>> places;
  bool halt = false, steers = true;
  std::vector<const Contact*> list;
  std::map<const Contact*, uint32_t> ent;
  std::map<const Contact*, float> dist;
  std::map<uint32_t, bool> target;
  float fwd[2] = {};
  void position(zgame::Handle e, float out[3]) override {
    const auto& p = places[e];
    out[0] = p[0], out[1] = p[1], out[2] = p[2];
  }
  void forward(zgame::Handle, float* x, float* z) override { *x = fwd[0], *z = fwd[1]; }
  bool halted(zgame::Handle) override { return halt; }
  bool order_steers(Brain*) override { return steers; }
  const std::vector<const Contact*>& contacts(zgame::Handle) override { return list; }
  zgame::Handle contact_entity(const Contact* c) override { return ent[c]; }
  bool targetable(zgame::Handle e) override { return target[e]; }
  float contact_dist2(const Contact* c) override { return dist[c]; }
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
void dump(std::string& o, const SteerPm& s) {
  for (float v : {s.start[0], s.start[1], s.start[2], s.dir[0], s.dir[1], s.along_min, s.length, s.along_at, s.across,
                  s.across_to, s.rate, s.along, s.side})
    put(o, v);
  o += std::to_string(s.sets) + "\n";
}
}  // namespace

extern "C" int probe_pm_ai_steer(const char* plan, char* out, int cap) {
  World w;
  SteerPm s;
  const zgame::Handle v = 1;
  std::string o;
  std::istringstream lines(plan);
  std::string line;
  std::vector<Contact*> keep;
  while (std::getline(lines, line)) {
    std::istringstream in(line);
    std::string op;
    in >> op;
    std::vector<std::string> a;
    std::string t;
    while (in >> t) a.push_back(t);
    if (op == "P") {  // a place
      w.places[static_cast<uint32_t>(std::stoul(a[0]))] = {f(a[1]), f(a[2]), f(a[3])};
    } else if (op == "T") {
      s.across_to = f(a[0]);
    } else if (op == "F") {
      w.fwd[0] = f(a[0]), w.fwd[1] = f(a[1]);
    } else if (op == "H") {
      w.halt = a[0] == "1", w.steers = a[1] == "1";
    } else if (op == "K") {  // the keep and push switches and fields
      s.keep = static_cast<uint8_t>(std::stoi(a[0]));
      s.push = static_cast<uint8_t>(std::stoi(a[1]));
    } else if (op == "C") {  // a contact: entity, targetable, distance
      auto* c = reinterpret_cast<Contact*>(static_cast<uintptr_t>(w.list.size() + 1));
      const uint32_t e = static_cast<uint32_t>(std::stoul(a[0]));
      w.list.push_back(c);
      w.ent[c] = e;
      w.target[e] = a[1] == "1";
      w.dist[c] = f(a[2]);
    } else if (op == "X") {
      w.list.clear();
    } else if (op == "S") {
      const float to[3] = {f(a[0]), f(a[1]), f(a[2])}, from[3] = {f(a[3]), f(a[4]), f(a[5])};
      s.set(w, to, from, f(a[6]), v);
    } else if (op == "W") {
      s.start[0] = f(a[0]), s.start[1] = f(a[1]), s.start[2] = f(a[2]);
      s.along_min = f(a[3]), s.length = f(a[4]), s.along_at = f(a[5]), s.across = 0.0f, s.sets = 1;
    } else if (op == "L") {
      put(o, s.length), put(o, s.along_at), put(o, s.along_min);
      o += "\n";
      continue;
    } else if (op == "U") {
      s.update(w, f(a[0]), v);
    } else if (op == "M") {
      s.measure(w, v);
    } else if (op == "A") {
      s.advance(f(a[0]));
    } else if (op == "N") {
      o += s.near(w) ? "near " : "far ";
      o += s.at_end() ? "end\n" : "on\n";
    } else if (op == "R") {
      float c[3];
      s.carrot(c);
      put(o, c[0]), put(o, c[1]), put(o, c[2]);
      o += "\n";
    }
    dump(o, s);
  }
  if (static_cast<int>(o.size()) >= cap) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
