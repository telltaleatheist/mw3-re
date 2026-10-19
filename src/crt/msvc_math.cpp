#include "crt/msvc_math.h"

#include <cmath>

namespace crt {

int64_t msvc_ftol64(double x) {
  if (std::isnan(x) || x >= 9223372036854775808.0 || x < -9223372036854775808.0) return INT64_MIN;
  return static_cast<int64_t>(std::trunc(x));
}

namespace {
// The float nearest p + e, where e is the exact error of p (|e| <= half a
// double ulp): (float)p is right unless p sits exactly on a float tie.
float round_once(double p, double e) {
  const float f = static_cast<float>(p);
  if (e == 0.0 || static_cast<double>(f) == p) return f;
  const float g = std::nextafter(f, p > f ? INFINITY : -INFINITY);
  const double mid = (static_cast<double>(f) + static_cast<double>(g)) / 2;
  if (p != mid) return f;
  return e > 0 ? std::fmax(f, g) : std::fmin(f, g);
}
}  // namespace

float x87_mul_f24(double a, double b) {
  const double p = a * b;
  return round_once(p, std::fma(a, b, -p));
}

float x87_div_f24(double a, double b) {
  const double q = a / b;
  const double r = std::fma(-q, b, a);  // a - q*b, exact
  return round_once(q, b > 0 ? r : -r);
}

}  // namespace crt
