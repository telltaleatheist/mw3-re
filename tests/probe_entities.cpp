// The entity manager's walk (gamez/zentity, 0x005a8850) for
// tests/differential/test_entity_walk.py: entities 0..n-1, each live (+0x09)
// or not, each spending a full update (0x005a8960) or not when allowed
// (0x005a8910). Pointers go back and forth as indices (-1: outside the list).
#include <cstdint>
#include <vector>

#include "gamez/zentity/zent_manager.h"

extern "C" {

// b: updating, resume, current, left, per_frame, in and out. log: each
// entity's update as (entity, allowed). Returns the log's length.
int32_t probe_entity_walk(int32_t n, const uint8_t* live, const uint8_t* spends, int32_t* b, int32_t* log) {
  std::vector<int32_t> list(static_cast<size_t>(n));
  for (int32_t i = 0; i < n; ++i) list[i] = i;
  zent::Budget& g = zent::budget();
  const zent::Budget saved = g;
  auto ptr = [&](int32_t i) -> void* { return i >= 0 && i < n ? &list[i] : nullptr; };
  auto idx = [&](const void* p) -> int32_t {
    const int32_t* q = static_cast<const int32_t*>(p);
    return q >= list.data() && q < list.data() + n ? static_cast<int32_t>(q - list.data()) : -1;
  };
  g.updating = b[0] != 0, g.resume = ptr(b[1]), g.current = ptr(b[2]), g.left = b[3], g.per_frame = b[4];
  int32_t k = 0;
  g.update_all(list, [&](int32_t e) {
    if (!live[e]) return;
    const bool ok = g.allows();
    log[k++] = e, log[k++] = ok;
    if (ok && spends[e]) g.spend();
  });
  b[0] = g.updating, b[1] = idx(g.resume), b[2] = idx(g.current), b[3] = g.left, b[4] = g.per_frame;
  g = saved;
  return k;
}

}  // extern "C"
