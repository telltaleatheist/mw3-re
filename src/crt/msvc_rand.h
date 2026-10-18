#pragma once
#include <cstdint>

// The original links MSVCRT's rand()/srand(). Random choices (music cues, AI,
// damage spread) must draw the same sequence, so the rebuild uses this instead of
// the host C library, whose rand() differs on every platform.
namespace crt {

void msvc_srand(uint32_t seed);
int msvc_rand();  // 0..32767, same sequence as MSVCRT for the same seed

}  // namespace crt
