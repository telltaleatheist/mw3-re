// test_pm_ai_link.py: PM's link pick 0x004d60d0 on a node whose links are given.
#include <cstdint>
#include <vector>

#include "mech3/zai_node_pm.h"

using namespace mech3::ai;
using zgame::Handle;

namespace {
struct Link {
  int kind;  // 0 a link, 1 none, 2 the null step
  float weight, dir[3];
};
struct Hooks : LinkPickPmHooks {
  const Link* links = nullptr;
  int n = 0, path = 0, roll = 0, rolled = 0, wrong = 0;
  float point[3] = {};
  float seen[3] = {};
  Handle path_next(Handle) override { return static_cast<Handle>(path); }
  int32_t link_count(Handle) override { return n; }
  Handle link(Handle, int32_t i) override {
    if (i >= n) return 0;
    if (links[i].kind == 2) return 0x1000 + 99;  // the null step
    return links[i].kind == 1 ? 0 : static_cast<Handle>(0x1000 + i);
  }
  Handle null_step(Handle) override { return 0x1000 + 99; }
  const float* way_point(Handle) override { return point; }
  float weight(Handle l, Handle brain, Handle at, LinkWayPm* way) override {
    if (brain != 7 || at != 8) ++wrong;
    seen[0] = way->from[0], seen[1] = way->from[1], seen[2] = way->from[2];
    if (l == 0x1000 + 99) return ++wrong, 1.0f;  // the null step is never weighed
    const Link& k = links[l - 0x1000];
    way->dir[0] = k.dir[0], way->dir[1] = k.dir[1], way->dir[2] = k.dir[2];
    return k.weight;
  }
  int32_t rand_int() override { return ++rolled, roll; }
};
}  // namespace

// kinds, weights and dirs by link. Returns the pick (0 none, else 1 + the link's index, or the path's), `sums` its sums.
extern "C" int probe_pm_ai_link(int n, const int* kinds, const float* weights, const float* dirs, const float* heading,
                                const float* point, int path, int roll, float* sums, float* seen, int* rolled) {
  std::vector<Link> links(static_cast<size_t>(n > 0 ? n : 0));
  for (int i = 0; i < n; ++i) {
    links[i].kind = kinds[i], links[i].weight = weights[i];
    for (int k = 0; k < 3; ++k) links[i].dir[k] = dirs[3 * i + k];
  }
  Hooks h;
  h.links = links.data(), h.n = n, h.path = path, h.roll = roll;
  for (int k = 0; k < 3; ++k) h.point[k] = point[k];
  std::vector<float> s;
  const Handle r = pick_link_pm(h, 7, 8, heading, &s);
  for (size_t i = 0; i < s.size(); ++i) sums[i] = s[i];
  for (int k = 0; k < 3; ++k) seen[k] = h.seen[k];
  *rolled = h.rolled;
  if (h.wrong) return -1;
  if (path && r == static_cast<Handle>(path)) return path;
  if (r >= 0x1000) return 1 + static_cast<int>(r - 0x1000);
  return static_cast<int>(r);
}
