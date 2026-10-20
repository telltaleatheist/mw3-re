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
  delete valve_union;
  valve_union = copy_union(o.valve_union);
  delete valve_assign;
  valve_assign = copy_assign(o.valve_assign);
  return *this;
}

// Mech3.exe 1.2 @ 0x004c62d0
NetNodeRec::~NetNodeRec() {
  std::free(name);
  std::free(auxname);
  std::free(gate);
  delete valve_union;
  delete valve_assign;
}

// Mech3.exe 1.2 @ 0x004c6bd0
void NetNodeRec::reset() {
  std::free(name), name = nullptr;
  std::free(gate), gate = nullptr;
  std::free(auxname), auxname = nullptr;
  delete valve_union, valve_union = nullptr;
  delete valve_assign, valve_assign = nullptr;
  links.clear();
  copy_plain(*this, NetNodeRec{});  // the defaults at 0x005e8af8
}

}  // namespace mech3::ai
