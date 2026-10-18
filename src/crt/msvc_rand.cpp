#include "crt/msvc_rand.h"

namespace crt {

namespace {
// MSVCRT keeps one seed per thread; the game draws from its main thread only.
uint32_t g_seed = 1;
}  // namespace

void msvc_srand(uint32_t seed) { g_seed = seed; }

int msvc_rand() {
  g_seed = g_seed * 214013u + 2531011u;
  return static_cast<int>((g_seed >> 16) & 0x7fff);
}

}  // namespace crt
