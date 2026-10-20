#include "mech3/zai_valve.h"

#include <cstdlib>
#include <cstring>

namespace mech3::ai {

std::list<Valve*>& valves() {
  static std::list<Valve*> list;  // 0x0060b500
  return list;
}

// Mech3.exe 1.2 @ 0x004d3b20
Valve::Valve(const char* n) : value(new ValveValue) {
  name = n ? strdup(n) : nullptr;
}

// Mech3.exe 1.2 @ 0x004d3b80. The chain's slot 0 is its deleting destructor;
// with zrdrvalve.cpp not ported the registry never makes one.
Valve::~Valve() {
  std::free(name);
  delete value;
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

void valves_clear() {
  for (Valve* v : valves()) delete v;
  valves().clear();
}

}  // namespace mech3::ai
