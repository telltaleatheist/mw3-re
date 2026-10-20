// zTwoD's HUD primitives (0x005a3170..0x005a3bc0, 0x005a67e0..0x005a6d10;
// file name unknown, named after what it holds): built and set up only, their
// drawing not ported yet (docs/twod.md).
#include "crt/msvc_math.h"
#include "gamez/ztwod/twod.h"

#pragma STDC FP_CONTRACT OFF

namespace ztwod {

// Mech3.exe 1.2 @ 0x005a6bd0
Polygon::Polygon() : Object2D(0, 0) { dirty(); }

// Mech3.exe 1.2 @ 0x005a6c30
void Polygon::set_point(int32_t i, float px, float py) {
  if (i < 0x15 && i > -1) {
    pts[i][0] = px;
    pts[i][1] = py;
    if (count < i + 1) count = i + 1;
    if (i == 0) {
      const int32_t ty = crt::msvc_ftol(py);
      set_pos(crt::msvc_ftol(px), ty);
    }
  }
  dirty();
}

}  // namespace ztwod
