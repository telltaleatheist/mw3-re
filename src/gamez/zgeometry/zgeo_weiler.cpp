#include "gamez/zgeometry/zgeo_weiler.h"

#include <cmath>

namespace zgeo {

// Mech3.exe 1.2 @ 0x00515130
int points_near(const float a[2], const float b[2], float eps) {
  return std::fabs(a[0] - b[0]) <= eps && std::fabs(a[1] - b[1]) <= eps;
}

}  // namespace zgeo
