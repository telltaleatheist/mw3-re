#include "mech3/zainet_node.h"

#include <cstdlib>
#include <cstring>

namespace mech3::ai {

namespace {
char* dup(const char* s) { return s ? strdup(s) : nullptr; }

// The plain part, +0x04..+0x9c, copied as one block (rep movsd, 0x26 dwords).
void copy_plain(NetNodeRec& to, const NetNodeRec& from) {
  to.kind = from.kind, to.index = from.index;
  std::memcpy(to.pos, from.pos, sizeof to.pos);
  to.speed_on = from.speed_on, to.speed = from.speed;
  to.wait_interval_on = from.wait_interval_on, to.wait_interval = from.wait_interval;
  to.wait_time_on = from.wait_time_on, to.wait_time = from.wait_time;
  to.valve_on = from.valve_on, to.valve = from.valve;
  to.valve_has_value = from.valve_has_value, to.valve_value = from.valve_value;
  to.canleave[0] = from.canleave[0], to.canleave[1] = from.canleave[1];
  to.jump_on = from.jump_on, to.jump = from.jump;
  for (int i = 0; i < 2; ++i)
    to.scan_aim[i] = from.scan_aim[i], to.scan_hit[i] = from.scan_hit[i], to.scan_time[i] = from.scan_time[i];
  to.hidden_set = from.hidden_set, to.hidden = from.hidden;
  to.fixed_set = from.fixed_set, to.fixed = from.fixed;
  to.autonext_set = from.autonext_set, to.autonext = from.autonext;
  to.visible_set = from.visible_set, to.visible = from.visible;
  to.deploy_set = from.deploy_set, to.deploy_a = from.deploy_a, to.deploy_b = from.deploy_b;
  to.deploy_c = from.deploy_c;
  to.deploy_valve_a = from.deploy_valve_a, to.deploy_valve_b = from.deploy_valve_b;
  to.arrange_set = from.arrange_set, to.arrange = from.arrange;
  to.mcbwait_set = from.mcbwait_set, to.mcbwait_b = from.mcbwait_b;
  to.mcbwait_time = from.mcbwait_time, to.mcbwait_valve = from.mcbwait_valve;
  to.sound = from.sound;
  to.backup = from.backup, to.shunt = from.shunt;
  to.via_radius2 = from.via_radius2, to.via_number = from.via_number, to.terminus = from.terminus;
  to.static_mfb_set = from.static_mfb_set, to.static_mfb = from.static_mfb;
}

ValveUnion* copy_union(const ValveUnion* u) {
  if (!u) return nullptr;
  auto* c = new ValveUnion;  // 0x004d44b0(size) then 0x004c75c0
  c->valves = u->valves;
  return c;
}

ValveAssign* copy_assign(const ValveAssign* a) { return a ? new ValveAssign(*a) : nullptr; }
}  // namespace

// Mech3.exe 1.2 @ 0x004c6050
NetNodeRec::NetNodeRec(const NetNodeRec& o) : links(o.links) {
  copy_plain(*this, o);
  name = dup(o.name);
  auxname = dup(o.auxname);
  gate = dup(o.gate);
  via = dup(o.via);
  valve_union = copy_union(o.valve_union);
  valve_assign = copy_assign(o.valve_assign);
}

// Mech3.exe 1.2 @ 0x004c63a0. Quirk kept: the auxname is freed and not copied,
// so an assigned record has none (only the editor's paths assign records).
NetNodeRec& NetNodeRec::operator=(const NetNodeRec& o) {
  std::free(name), name = nullptr;
  std::free(auxname), auxname = nullptr;
  std::free(gate), gate = nullptr;
  copy_plain(*this, o);
  if (&links != &o.links) links = o.links;
  name = dup(o.name);
  gate = dup(o.gate);
  std::free(via), via = dup(o.via);
  delete valve_union;
  valve_union = copy_union(o.valve_union);
  delete valve_assign;
  valve_assign = copy_assign(o.valve_assign);
  return *this;
}

namespace {
bool same_text(const char* a, const char* b) { return a == b || (a && b && std::strcmp(a, b) == 0); }
}  // namespace

// Mech3.exe 1.2 @ 0x004c6680: kind and number, the place (kind 0), the names,
// the properties, the gate, the valve union and assignment, the links.
// FIX (bug): the exe leaves out wait_time and the valve, so an edit of only
// those reads as the file's own and the cache drops it (docs/fixes.md).
bool NetNodeRec::same(const NetNodeRec& o) const {
  if (kind != o.kind || index != o.index) return false;
  if (kind == 0 ? !(pos[0] == o.pos[0] && pos[1] == o.pos[1] && pos[2] == o.pos[2]) : kind != 1) return false;
  if (!same_text(name, o.name) || !same_text(auxname, o.auxname)) return false;
  if (speed_on != o.speed_on || speed != o.speed || wait_interval_on != o.wait_interval_on || wait_interval != o.wait_interval)
    return false;
  if (wait_time_on != o.wait_time_on || wait_time != o.wait_time) return false;  // ours
  if (valve_on != o.valve_on || valve != o.valve || valve_has_value != o.valve_has_value || valve_value != o.valve_value)
    return false;  // ours
  if (canleave[0] != o.canleave[0] || canleave[1] != o.canleave[1] || scan_aim[0] != o.scan_aim[0] ||
      scan_aim[1] != o.scan_aim[1] || scan_time[0] != o.scan_time[0] || scan_time[1] != o.scan_time[1] ||
      scan_hit[0] != o.scan_hit[0] || scan_hit[1] != o.scan_hit[1] || jump_on != o.jump_on || jump != o.jump)
    return false;
  if (hidden_set != o.hidden_set || hidden != o.hidden || fixed_set != o.fixed_set || fixed != o.fixed ||
      autonext_set != o.autonext_set || autonext != o.autonext || visible_set != o.visible_set || visible != o.visible)
    return false;
  if (deploy_set != o.deploy_set || deploy_valve_a != o.deploy_valve_a || deploy_a != o.deploy_a ||
      deploy_valve_b != o.deploy_valve_b || deploy_b != o.deploy_b || deploy_c != o.deploy_c)
    return false;
  if (mcbwait_set != o.mcbwait_set || mcbwait_valve != o.mcbwait_valve || mcbwait_b != o.mcbwait_b ||
      mcbwait_time != o.mcbwait_time || arrange_set != o.arrange_set || arrange != o.arrange)
    return false;
  if (sound != o.sound || backup != o.backup || !same_text(gate, o.gate) || shunt != o.shunt) return false;
  if (valve_union != o.valve_union) {  // 0x004c7530 -> 0x004d4630
    if (!valve_union || !o.valve_union || valve_union->valves.size() != o.valve_union->valves.size()) return false;
    for (size_t i = 0; i < valve_union->valves.size(); ++i)
      if (valve_union->valves[i].valve != o.valve_union->valves[i].valve ||
          valve_union->valves[i].count != o.valve_union->valves[i].count)
        return false;
  }
  if (valve_assign != o.valve_assign) {  // 0x004c7570
    if (!valve_assign || !o.valve_assign || valve_assign->valve != o.valve_assign->valve ||
        valve_assign->value != o.valve_assign->value)
      return false;
  }
  return links == o.links;
}

// Mech3.exe 1.2 @ 0x004c62d0
NetNodeRec::~NetNodeRec() {
  std::free(name);
  std::free(auxname);
  std::free(gate);
  std::free(via);
  delete valve_union;
  delete valve_assign;
}

// Mech3.exe 1.2 @ 0x004c6bd0
void NetNodeRec::reset() {
  std::free(name), name = nullptr;
  std::free(gate), gate = nullptr;
  std::free(auxname), auxname = nullptr;
  std::free(via), via = nullptr;
  delete valve_union, valve_union = nullptr;
  delete valve_assign, valve_assign = nullptr;
  links.clear();
  copy_plain(*this, NetNodeRec{});  // the defaults at 0x005e8af8
}

}  // namespace mech3::ai
