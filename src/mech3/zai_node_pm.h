#pragma once
#include <cstdint>
#include <vector>

#include "gamez/zgame/unported.h"

// Pirate's Moon's link pick, at 53 bits (docs/pm-map.md, PM's AI at 53 bits). MW3's 0x004ccf20 with the weight's
// slot moved and the sums left in the register. Nothing runs it until the orders' switch.
namespace mech3::ai {

// 0x004c7ae0's layout, on the pick's stack: what the weight reads and writes.
struct LinkWayPm {
  int32_t x00 = -1;      // +0x00
  float dist = 0;        // +0x04
  float from[3] = {};    // +0x08
  float dir[3] = {};     // +0x14
};

class LinkPickPmHooks {
 public:
  virtual ~LinkPickPmHooks() = default;
  virtual zgame::Handle path_next(zgame::Handle at) = 0;             // 0x004d3120
  virtual int32_t link_count(zgame::Handle at) = 0;                  // vt+0x04
  virtual zgame::Handle link(zgame::Handle at, int32_t i) = 0;       // vt+0x08, its vt+0x60, vt+0x74; 0 for none
  virtual zgame::Handle null_step(zgame::Handle at) = 0;             // vt+0x10
  virtual const float* way_point(zgame::Handle at) = 0;              // vt+0x1c
  virtual float weight(zgame::Handle l, zgame::Handle brain, zgame::Handle at, LinkWayPm* way) = 0;  // vt+0xac
  virtual int32_t rand_int() = 0;
};

// 0x004d60d0. `sums`, when given, gets the running sums.
zgame::Handle pick_link_pm(LinkPickPmHooks& h, zgame::Handle brain, zgame::Handle at, const float heading[3],
                           std::vector<float>* sums = nullptr);

}  // namespace mech3::ai
