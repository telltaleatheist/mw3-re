#include "crt/msvc_math.h"

#include <cmath>

namespace crt {

int64_t msvc_ftol64(double x) {
  if (std::isnan(x) || x >= 9223372036854775808.0 || x < -9223372036854775808.0) return INT64_MIN;
  return static_cast<int64_t>(std::trunc(x));
}

}  // namespace crt
