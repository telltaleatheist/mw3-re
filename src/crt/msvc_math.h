#pragma once
#include <cstdint>

namespace crt {

int64_t msvc_ftol64(double x);

inline int32_t msvc_ftol(double x) {
  return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(msvc_ftol64(x))));
}

}  // namespace crt
