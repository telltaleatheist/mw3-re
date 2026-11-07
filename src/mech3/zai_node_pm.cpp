#include "mech3/zai_node_pm.h"

#include <cmath>

namespace mech3::ai {

// Pirate's Moon 1.0 @ 0x004d60d0. Each weight stays in the register for its test and the sum; the dot, the sums and
// the roll are stored as floats.
zgame::Handle pick_link_pm(LinkPickPmHooks& h, zgame::Handle brain, zgame::Handle at, const float heading[3],
                           std::vector<float>* sums_out) {
  if (const zgame::Handle n = h.path_next(at)) return n;
  const int32_t n = h.link_count(at);
  const zgame::Handle miss = h.null_step(at);
  LinkWayPm way;
  const float* p = h.way_point(at);
  way.from[0] = p[0], way.from[1] = p[1], way.from[2] = p[2];  // 0x004d2f40
  zgame::Handle picked = 0;
  std::vector<float> scratch;
  std::vector<float>& sums = sums_out ? *sums_out : scratch;
  sums.assign(static_cast<size_t>(n > 0 ? n : 0), 0.0f);
  if (n == 1) {
    const zgame::Handle l = h.link(at, 0);
    if (l && l != miss && h.weight(l, brain, at, &way) > 0.0f) picked = l;
    return picked;
  }
  float total = 0.0f;
  int32_t counted = 0;
  for (int32_t i = 0; i < n; ++i) {
    const zgame::Handle l = h.link(at, i);
    double f = 0.0;
    if (l && l != miss) {
      const float w = h.weight(l, brain, at, &way);
      if (w > 0.0f) {
        const float d = static_cast<float>(double{heading[2]} * way.dir[2] + double{heading[0]} * way.dir[0]);
        f = ((double{d} + 1.0) * 0.495 + 0.01) * w;  // [0x005ee6c8], [0x005dfca8]
      }
    }
    total = static_cast<float>(f + total);
    sums[static_cast<size_t>(i)] = total;
    if (f != 0.0 && !std::isnan(f)) ++counted, picked = l;
  }
  if (counted > 1) {
    const float r = static_cast<float>(static_cast<double>(h.rand_int()) * total * 3.0518509447574615e-05);
    size_t first = 0, len = sums.size();
    while (len > 0) {  // the first whose sum is not below r
      const size_t half = len / 2;
      if (r > sums[first + half]) first += half + 1, len -= half + 1;
      else len = half;
    }
    picked = h.link(at, static_cast<int32_t>(first));
  }
  return picked;
}

}  // namespace mech3::ai
