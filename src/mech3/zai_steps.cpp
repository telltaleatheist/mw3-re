#include "mech3/zai_steps.h"

#include <cstring>

namespace mech3::ai {

Behaviour* null_step() {
  static Behaviour s(StepKind::Null);  // 0x00603fa0 (0x004105d0)
  return &s;
}

Behaviour* default_behaviour() {
  static Behaviour s(StepKind::Behaviour);  // 0x00604068 (0x004c8750)
  return &s;
}

// Mech3.exe 1.2 @ 0x004106c0
znet::Element* Behaviour::miss() { return null_step(); }

// Mech3.exe 1.2 @ 0x004c5ac0: the behaviour's own +0x10, the null step.
Behaviour* Behaviour::link(int32_t) const { return null_step(); }

// Mech3.exe 1.2 @ 0x004c9e60: the slot made the step there; a slot holding
// the null step (or none) leaves the number in the exe, the null step here.
void Step::attach(CompiledNet* net) {
  if (!slot_set) return;
  Behaviour* p = net->step(next_slot);
  slot_set = false;
  if (p && p != null_step()) {
    next = p;
    return;
  }
  // FIX (bug): the exe keeps the slot number as the pointer (a link to a
  // node without a position, followed later, is a crash).
  next = null_step();
  unresolved = true;
}

// Mech3.exe 1.2 @ 0x004cb2c0 for jumps (the other value steps keep 0x004cd490).
bool ValueStep::passable(const Pilot* p, const Behaviour*) const {
  if (kind != StepKind::Jump) return true;
  return p && p->jump_jets > 0;
}

// Mech3.exe 1.2 @ 0x004ca260: only while the valve holds the step's value.
bool ValveStep::passable(const Pilot*, const Behaviour*) const {
  return valve && valve->get() == value;
}

// Mech3.exe 1.2 @ 0x004cd5d0: only the vehicle the gate is named for.
bool GateStep::passable(const Pilot* p, const Behaviour*) const {
  return has_gate && p && p->name && std::strcmp(gate.c_str(), p->name) == 0;
}

// Mech3.exe 1.2 @ 0x004d4600 (each entry 0x004d4490): while any of the
// valves holds its count.
bool UnionStep::passable(const Pilot*, const Behaviour*) const {
  for (const ValveCount& c : valves)
    if (c.valve && c.valve->get() == c.count) return true;  // FIX (bug): the exe reads a null valve
  return false;
}

// Mech3.exe 1.2 @ 0x004cf820: the links from the last one that names a slot
// (-1 is none).
NodeStep::NodeStep(StepKind k, const float p[3], const int32_t* s, int32_t n)
    : Behaviour(k), x1c(uint8_t(reinterpret_cast<uintptr_t>(s))) {
  pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
  int32_t last = n - 1;
  while (last >= 0 && s[last] == -1) --last;
  slots.assign(s, s + (last + 1));
}

// Mech3.exe 1.2 @ 0x004c93d0
Behaviour* NodeStep::link(int32_t i) const {
  if (!attached) return null_step();
  if (i < 0 || size_t(i) >= links.size()) return null_step();
  return links[size_t(i)];
}

// Mech3.exe 1.2 @ 0x004c94a0: each slot made its step, the ones naming
// nothing (or the null step) dropped.
void NodeStep::attach(CompiledNet* net) {
  links.clear();
  for (int32_t s : slots) {
    Behaviour* b = net->step(s);
    if (b && b != null_step()) links.push_back(b);
  }
  attached = true;
}

// Mech3.exe 1.2 @ 0x004cffd0
void DeployNode::attach(CompiledNet* net) {
  NodeStep::attach(net);
  target_step = net->step(target);
}

// Mech3.exe 1.2 @ 0x004c0e70: turning the mark on clears every step's
// visited mark; false when it was already as asked.
bool CompiledNet::mark(bool on) {
  if (bool(x18 & 1) == on) return false;
  x18 = uint8_t((x18 & ~1u) | (on ? 1u : 0u));
  if (on)
    for (znet::Element* e : elems)
      if (Behaviour* b = e ? static_cast<AiElem*>(e)->as_behaviour() : nullptr) b->set_visited(false);
  return true;
}

// Mech3.exe 1.2 @ 0x004bf910: room for `slots`; the exe writes the default
// behaviour into the room before the slots are made (it is overwritten).
CompiledNet::CompiledNet(int32_t id, int32_t slots) : AiNet(id) {
  if (slots > 0) elems.reserve(size_t(slots));
}

// Mech3.exe 1.2 @ 0x004bff20: each step released through its +0xa0
// (0x00410670 deletes; the null step's is an empty slot).
CompiledNet::~CompiledNet() {
  for (znet::Element*& e : elems) {
    if (e != null_step() && e != default_behaviour()) delete e;
    e = nullptr;
  }
}

}  // namespace mech3::ai
