// The net file's record cache for test_net_cache.py: the record compare
// NetNodeRec::same (0x004c6680) on two records given as the exe's words.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "mech3/zainet_node.h"

namespace {
using mech3::ai::NetNodeRec;

template <class T>
T* ptr(uint32_t v) {
  return reinterpret_cast<T*>(static_cast<uintptr_t>(v));
}
char* text(const std::string& s) { return s == "-" ? nullptr : strdup(s.c_str()); }

// A line: 45 words (the record at its offsets, +0x00 the vtable, the owned
// pointers ignored), then the name, auxname and gate ("-": none), the links
// ("-" or a,b,c), the valve union ("-" or v:n,v:n with v a valve's word) and
// the assignment ("-" or v:value).
void fill(NetNodeRec& r, const std::string& line) {
  std::istringstream in(line);
  uint32_t w[45];
  for (uint32_t& x : w) in >> x;
  uint8_t b[0xb4];
  std::memcpy(b, w, sizeof b);
  const auto f = [&](int off) { float v; std::memcpy(&v, b + off, 4); return v; };
  const auto i = [&](int off) { int32_t v; std::memcpy(&v, b + off, 4); return v; };
  const auto u = [&](int off) { uint32_t v; std::memcpy(&v, b + off, 4); return v; };
  r.kind = i(0x04), r.index = i(0x08);
  for (int k = 0; k < 3; ++k) r.pos[k] = f(0x0c + 4 * k);
  r.speed_on = i(0x18), r.speed = f(0x1c), r.wait_interval_on = i(0x20), r.wait_interval = f(0x24);
  r.wait_time_on = i(0x28), r.wait_time = f(0x2c), r.valve_on = i(0x30), r.valve = ptr<mech3::ai::Valve>(u(0x34));
  r.valve_has_value = b[0x38], r.valve_value = i(0x3c);
  r.canleave[0] = i(0x40), r.canleave[1] = i(0x44), r.jump_on = i(0x48), r.jump = f(0x4c);
  r.scan_aim[0] = i(0x50), r.scan_aim[1] = i(0x54), r.scan_hit[0] = i(0x58), r.scan_hit[1] = i(0x5c);
  r.scan_time[0] = i(0x60), r.scan_time[1] = i(0x64);
  r.hidden_set = b[0x68], r.hidden = b[0x69], r.fixed_set = b[0x6a], r.fixed = b[0x6b];
  r.autonext_set = b[0x6c], r.autonext = b[0x6d], r.visible_set = b[0x6e], r.visible = b[0x6f];
  r.deploy_set = b[0x70], r.deploy_a = b[0x71], r.deploy_b = b[0x72];
  std::memcpy(&r.deploy_c, b + 0x74, 2);
  r.deploy_valve_a = ptr<mech3::ai::Valve>(u(0x78)), r.deploy_valve_b = ptr<mech3::ai::Valve>(u(0x7c));
  r.arrange_set = b[0x80], r.arrange = f(0x84), r.mcbwait_set = b[0x88], r.mcbwait_b = b[0x89];
  r.mcbwait_time = f(0x8c), r.mcbwait_valve = ptr<mech3::ai::Valve>(u(0x90)), r.sound = ptr<void>(u(0x94));
  r.backup = b[0x98], r.shunt = b[0x99];
  std::string name, aux, gate, links, uni, assign;
  in >> name >> aux >> gate >> links >> uni >> assign;
  r.name = text(name), r.auxname = text(aux), r.gate = text(gate);
  if (links != "-") {
    std::istringstream l(links);
    for (std::string t; std::getline(l, t, ',');) r.links.push_back(std::stoi(t));
  }
  if (uni != "-") {
    r.valve_union = new mech3::ai::ValveUnion;
    std::istringstream l(uni);
    for (std::string t; std::getline(l, t, ',');) {
      const size_t c = t.find(':');
      r.valve_union->valves.push_back({ptr<mech3::ai::Valve>(uint32_t(std::stoul(t.substr(0, c)))), std::stoi(t.substr(c + 1))});
    }
  }
  if (assign != "-") {
    const size_t c = assign.find(':');
    r.valve_assign = new mech3::ai::ValveAssign{ptr<mech3::ai::Valve>(uint32_t(std::stoul(assign.substr(0, c)))),
                                                std::stoi(assign.substr(c + 1))};
  }
}

// The records' own dtor frees the valve objects; these are not real ones.
void unhook(NetNodeRec& r) {
  r.valve = r.deploy_valve_a = r.deploy_valve_b = r.mcbwait_valve = nullptr;
  r.sound = nullptr;
}
}  // namespace

extern "C" int probe_net_rec_same(const char* a_line, const char* b_line) {
  NetNodeRec a, b;
  fill(a, a_line), fill(b, b_line);
  const int r = a.same(b) ? 1 : 0;
  unhook(a), unhook(b);
  return r;
}
