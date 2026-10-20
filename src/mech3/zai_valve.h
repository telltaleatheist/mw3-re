#pragma once
#include <cstdint>
#include <list>

// The mission's valves: named integer variables the AI nets, the vehicles'
// lists, the targets and the mission triggers read and set (docs/ai.md,
// Valves). Original file unknown: the registry and the reactions' list sit
// between zaitarget.cpp and zrdrainet.cpp (0x004d3b20..0x004d4490), the value
// holder in zrdrvalve.cpp (0x004e4460). What valves.zrd attaches to a valve
// (its conditions and actions) is mech3/zrdrvalve.
namespace mech3::ai {

struct Valve;
struct Composite;

// What a valve holds (8 bytes, vtable 0x005cd558, made by 0x004e4460): a
// number. valves.zrd may put a condition in its place (mech3/zrdrvalve),
// whose value is computed and cannot be set.
struct ValveValue {
  virtual ~ValveValue() = default;                    // +0x00 0x004e4490
  virtual int32_t get() const { return value; }       // +0x04 0x0040a850
  virtual void set(int32_t v) { value = v; }          // +0x08 0x0040e820
  virtual bool replaceable() const { return true; }   // +0x0c 0x004b3d30
  int32_t value = 0;                                  // +0x04
};

// What happens when a valve is set: an action, a timer or a list of them
// (every one's vtable: +0x00 the deleting dtor, +0x04 tick, +0x08 fire,
// +0x0c the list cast).
struct Reaction {
  virtual ~Reaction() = default;
  // +0x04 (0x0040bad0: no): a timer whose time has come does its work,
  // deletes itself and says yes.
  virtual bool tick(float now) {
    (void)now;
    return false;
  }
  virtual void fire(Valve& v) { (void)v; }          // +0x08 (0x004a5bc0: nothing)
  virtual Composite* as_composite() { return nullptr; }  // +0x0c 0x0040bae0
};

// 0x004d3fd0 (0x10 bytes, vtable 0x005ccd70): several reactions, in the
// order they were added.
struct Composite : Reaction {
  ~Composite() override;                    // 0x004d4080: each deleted
  bool tick(float now) override;            // 0x004d41e0
  void fire(Valve& v) override;             // 0x004d41a0
  Composite* as_composite() override { return this; }  // 0x004d2a70
  std::list<Reaction*> items;               // +0x08 head, +0x0c size
};

// 0x004d4320: `r` joins `slot`: the first one alone, the second makes a list
// of both, later ones join the list. False (nothing changed) when the list
// cannot be made.
bool add_reaction(Reaction*& slot, Reaction* r);

struct Valve {           // 0x0c bytes, ctor 0x004d3b20, dtor 0x004d3b80
  char* name = nullptr;              // +0x00  strdup'd
  ValveValue* value = nullptr;       // +0x04  0x004e4460
  Reaction* chain = nullptr;         // +0x08

  explicit Valve(const char* name);  // 0x004d3b20
  ~Valve();                          // 0x004d3b80
  Valve(const Valve&) = delete;
  Valve& operator=(const Valve&) = delete;

  int32_t get() const { return value->get(); }
  // 0x004d3c00: the old value; a new one is stored and the reactions fire
  // (0x004d3bf0). A condition ignores the set but still fires.
  int32_t set(int32_t v);
  void fire();                                            // 0x004d3bf0
  bool add(Reaction* r) { return add_reaction(chain, r); }  // 0x004d4310
};

// The registry, a std::list at 0x0060b500 (head 0x0060b504, size 0x0060b508),
// in the order the valves were first named.
std::list<Valve*>& valves();
Valve* valve_find(const char* name);  // 0x004d3d20 -> 0x004d3e50: the first of that exact name
Valve* valve_get(const char* name);   // 0x004d3c40: found or made; nullptr for no name
void valves_clear();                  // 0x004d3ef0: every valve deleted, the list emptied

// The delayed updates waiting ([0x0060b514], one reaction or a list).
Reaction*& valve_timers();
void valve_timers_add(Reaction* r);   // 0x004d4480
// 0x004d3d30, from the missions' update (0x0048f470) each frame: the timers
// ticked at the game time [0x0060991c] (dt is not used).
void valves_tick(float now);
void valves_end();                    // 0x004d3fa0: the timers deleted, then valves_clear

}  // namespace mech3::ai
