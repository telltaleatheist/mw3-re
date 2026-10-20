#pragma once
#include <cstdint>
#include <list>

// The mission's valves: named integer variables the AI nets, the vehicles'
// lists and the mission triggers read and set (docs/ai.md, Valves). Original
// file unknown: the registry sits between zaitarget.cpp and zrdrainet.cpp
// (0x004d3b20..0x004d3e50), the value holder in zrdrvalve.cpp (0x004e4460).
// What a valve sets off when its value changes (valves.zrd's updates, the
// mission's ends) is zrdrvalve.cpp's, not ported yet: `chain` stays opaque.
namespace mech3::ai {

// The value, 8 bytes behind vtable 0x005cd558 (get +0x04, set +0x08).
struct ValveValue {
  int32_t value = 0;  // +0x04
};

struct ValveChain;  // zrdrvalve.cpp's reactions, vtable +0x08 fires them

struct Valve {           // 0x0c bytes, ctor 0x004d3b20, dtor 0x004d3b80
  char* name = nullptr;              // +0x00  strdup'd
  ValveValue* value = nullptr;       // +0x04  0x004e4460
  ValveChain* chain = nullptr;       // +0x08

  explicit Valve(const char* name);  // 0x004d3b20
  ~Valve();                          // 0x004d3b80
  Valve(const Valve&) = delete;
  Valve& operator=(const Valve&) = delete;
};

// The registry, a std::list at 0x0060b500 (head 0x0060b504, size 0x0060b508),
// in the order the valves were first named.
std::list<Valve*>& valves();
Valve* valve_find(const char* name);  // 0x004d3d20 -> 0x004d3e50: the first of that exact name
Valve* valve_get(const char* name);   // 0x004d3c40: found or made; nullptr for no name
void valves_clear();                  // not original: the tests start each run empty

}  // namespace mech3::ai
