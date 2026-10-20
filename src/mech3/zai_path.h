#pragma once
#include <cstdint>
#include <list>

#include "mech3/zai_steps.h"

// How a pilot finds its way along a compiled net (docs/ai.md, Finding the
// way): the nearest step to a point, and the shortest way from one step to
// another that this pilot may take. Original file unknown: the finder sits
// between zainetmechinout.cpp and zaitarget.cpp (0x004c7b80..0x004c8430), the
// nearest step with the net base (0x004d51d0).
namespace mech3::ai {

// 0x004d51d0: the element with a position nearest `p` (squared distance,
// the first of equals); element 0 when none has one.
Behaviour* nearest_step(const AiNet& net, const float p[3]);

// 0x004c7d00 (0x38 bytes, vtable 0x005cb178). A search runs from a step to
// a target over the net's links, depth first, measuring the way from the
// pilot's position through each step that has one, and keeps the shortest
// way found; a branch is cut once it is no shorter than the best so far.
class PathFinder {
 public:
  PathFinder() = default;  // 0x004c7d00
  // 0x004c7f70 -> 0x004c7e80, measured from where the pilot stands
  // (Pilot::pos). False when no way is found, when the pilot has no
  // position, or when the net is already being searched.
  bool search(const Pilot* pilot, CompiledNet* net, Behaviour* start, Behaviour* target);
  bool search_again(Behaviour* start);  // 0x004c7e80 with the same pilot, net and target
  // 0x004c8430: the step after `at` on the way kept, when every step of the
  // rest (and the target) still lets the pilot by; null otherwise.
  Behaviour* follow(Behaviour* at);
  Behaviour* next_after(Behaviour* at);  // 0x004c83c0: the same without the check

  std::list<Behaviour*> best;     // +0x04  the shortest way found, start first
  float best_length = 0;          // +0x10
  bool found = false;             // +0x14
  const Pilot* pilot = nullptr;   // +0x18  in the exe the pilot's brain
  CompiledNet* net = nullptr;     // +0x1c
  Behaviour* target = nullptr;    // +0x20
  const float* at_pos = nullptr;  // +0x24  where the way measured so far ends
  std::list<Behaviour*> way;      // +0x28  the way being tried
  float length = 0;               // +0x34

 private:
  bool walk(Behaviour* s);        // 0x004c7fa0
  bool keep();                    // +0x04 0x004c8140: the way tried, if shorter
  void drop_to(Behaviour* at);    // 0x004c8330
  bool still_open() const;        // 0x004c8270
};

// The finder a pilot keeps (0x004c7c10, 0x004c7cb0): made on the first
// search, dropped when a search fails or the way runs out.
struct PathHolder {
  PathFinder* finder = nullptr;
  ~PathHolder() { reset(); }
  bool find(const Pilot* pilot, CompiledNet* net, Behaviour* start, Behaviour* target);
  Behaviour* next(Behaviour* at);
  void reset() {  // 0x004040d0
    delete finder;
    finder = nullptr;
  }
};

}  // namespace mech3::ai
