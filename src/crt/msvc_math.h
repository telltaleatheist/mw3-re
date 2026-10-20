#pragma once
#include <cstdint>

namespace crt {

int64_t msvc_ftol64(double x);

inline int32_t msvc_ftol(double x) {
  return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(msvc_ftol64(x))));
}

// In play the x87 runs at 24-bit precision (CLAUDE.md): a product or quotient
// of a float and a double is rounded once, straight to a float. Doing it in
// double and then casting rounds twice, which differs in rare ties.
float x87_mul_f24(double a, double b);
float x87_div_f24(double a, double b);
float x87_add_f24(double a, double b);  // a + b, the same single rounding

}  // namespace crt
