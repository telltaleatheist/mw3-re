#include "mech3/zai_valve.h"

#include <cstdlib>
#include <cstring>
#include <iterator>

#include "gamez/zutil/zfix.h"

namespace mech3::ai {

std::list<Valve*>& valves() {
  static std::list<Valve*> list;  // 0x0060b500
  return list;
}

Reaction*& valve_timers() {
  static Reaction* t = nullptr;  // 0x0060b514
  return t;
}

// Mech3.exe 1.2 @ 0x004d4080
Composite::~Composite() {
  for (Reaction*& r : items) {
    delete r;
    r = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x004d41a0
void Composite::fire(Valve& v) {
  for (Reaction* r : items) r->fire(v);
}

// Mech3.exe 1.2 @ 0x004d41e0: every reaction ticked in order (one added
// meanwhile, at the end, too); those done (they delete themselves) leave the
// list. Empty, the list deletes itself and says so.
bool Composite::tick(float now) {
  const auto end = items.end();
  auto p = items.begin();
  while (p != end && !(*p)->tick(now)) ++p;
  if (p != end)
    for (auto q = std::next(p); q != end; ++q)
      if (!(*q)->tick(now)) *p++ = *q;
  items.erase(p, end);
  if (!items.empty()) return false;
  delete this;
  return true;
}

// Mech3.exe 1.2 @ 0x004d4320
bool add_reaction(Reaction*& slot, Reaction* r) {
  if (!slot) {
    slot = r;
    return true;
  }
  if (Composite* c = slot->as_composite()) {
    c->items.push_back(r);
    return true;
  }
  Composite* c = new Composite;
  c->items.push_back(slot);
  c->items.push_back(r);
  slot = c;
  return true;
}

// Mech3.exe 1.2 @ 0x004d3b20
Valve::Valve(const char* n) : value(new ValveValue) {
  name = n ? strdup(n) : nullptr;
}

// Mech3.exe 1.2 @ 0x004d3b80
Valve::~Valve() {
  std::free(name);
  delete chain;
  chain = nullptr;
  delete value;
}

int32_t& valve_reentries() {
  static int32_t n = 0;
  return n;
}

// Mech3.exe 1.2 @ 0x004d3bf0. FIX (bug): a valve that fires again while its
// reactions run (an update of itself, directly or round a loop of updates
// and conditions) recursed until the stack ran out; here the inner fire is
// skipped (docs/ai.md section 8).
void Valve::fire() {
  if (!chain) return;
  if (firing) {
    ++valve_reentries();
    return;
  }
  firing = true;
  chain->fire(*this);
  firing = false;
}

// Mech3.exe 1.2 @ 0x004d3c00
int32_t Valve::set(int32_t v) {
  const int32_t old = value->get();
  if (value->get() != v) {
    value->set(v);
    fire();
  }
  return old;
}

// Mech3.exe 1.2 @ 0x004d3e50, through 0x004d3d20. strcmp: case counts.
Valve* valve_find(const char* name) {
  for (Valve* v : valves())
    if (std::strcmp(name, v->name) == 0) return v;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x004d3c40
Valve* valve_get(const char* name) {
  if (!name) return nullptr;
  Valve* v = valve_find(name);
  if (v) return v;
  v = new Valve(name);
  if (!v->name) {  // strdup failed
    delete v;
    return nullptr;
  }
  valves().push_back(v);
  return v;
}

// Mech3.exe 1.2 @ 0x004d3ef0
void valves_clear() {
  for (Valve*& v : valves()) {
    delete v;
    v = nullptr;
  }
  valves().clear();
}

// Mech3.exe 1.2 @ 0x004d4480
void valve_timers_add(Reaction* r) { add_reaction(valve_timers(), r); }

// Mech3.exe 1.2 @ 0x004d3d30. A lone timer whose update starts another
// delayed update is replaced, as it runs, by a list of itself and the new
// one; the exe then clears the slot, losing the new timer (and the list).
// FIX (bug), behind zfix bugs: the list stays, without the finished timer.
void valves_tick(float now) {
  Reaction*& slot = valve_timers();
  Reaction* t = slot;
  if (!t || !t->tick(now)) return;
  if (zfix::options().bugs && slot != t) {
    if (Composite* c = slot->as_composite()) c->items.remove(t);
    return;
  }
  slot = nullptr;
}

// Mech3.exe 1.2 @ 0x004d3fa0
void valves_end() {
  delete valve_timers();
  valve_timers() = nullptr;
  valves_clear();
}

}  // namespace mech3::ai
