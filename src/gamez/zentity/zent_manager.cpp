#include "gamez/zentity/zent_manager.h"

namespace zent {

namespace {
Budget g_budget;

// The step both functions compute: -1 with a cap and some left, +1 for the
// uncapped "-1" state, else 0 (no full update).
int32_t step(const Budget& b) {
  if (b.per_frame > 0) return b.left > 0 ? -1 : 0;
  return b.left == -1 ? 1 : 0;
}
}  // namespace

Budget& budget() { return g_budget; }

// Mech3.exe 1.2 @ 0x005a8910. Outside the manager's walk every entity may update.
bool Budget::allows() const { return !updating || step(*this) != 0; }

// Mech3.exe 1.2 @ 0x005a8960. Counts a full update and makes the next frame
// start after this entity.
void Budget::spend() {
  if (!updating) return;
  if (const int32_t d = step(*this)) {
    left += d;
    resume = current;
  }
}

}  // namespace zent
