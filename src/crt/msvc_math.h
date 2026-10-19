#pragma once
#include <cstdint>

// MSVCRT's float-to-integer conversion, which the compiler calls for every
// (int) cast of a float. Host casts are undefined out of range; this is not.
namespace crt {

// _ftol: truncates toward zero into 64 bits (FISTP with the rounding mode set to
// chop) and returns them in EDX:EAX. NaN and values outside the 64-bit range give
// the x87 "integer indefinite", 0x8000000000000000.
int64_t msvc_ftol64(double x);

// What the caller sees when it keeps only EAX, as an (int) cast does: the low 32
// bits, so 3e9 comes out as -1294967296 and 1e20 as 0.
inline int32_t msvc_ftol(double x) {
  return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(msvc_ftol64(x))));
}

}  // namespace crt
