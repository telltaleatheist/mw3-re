#include "check.h"
#include "crt/msvc_rand.h"

// MSVCRT's first values with the default seed of 1 (documented, and the values every
// Visual C++ program of the era produced without calling srand).
TEST(msvc_rand_default_seed_sequence) {
  crt::msvc_srand(1);
  EXPECT(crt::msvc_rand() == 41);
  EXPECT(crt::msvc_rand() == 18467);
  EXPECT(crt::msvc_rand() == 6334);
  EXPECT(crt::msvc_rand() == 26500);
  EXPECT(crt::msvc_rand() == 19169);
}

#include <cmath>

#include "crt/msvc_math.h"

TEST(msvc_ftol_truncates_and_wraps) {
  EXPECT(crt::msvc_ftol(2.9) == 2);
  EXPECT(crt::msvc_ftol(-2.9) == -2);
  EXPECT(crt::msvc_ftol(3e9) == -1294967296);
  EXPECT(crt::msvc_ftol(1e20) == 0);
  EXPECT(crt::msvc_ftol(std::nan("")) == 0);
  EXPECT(crt::msvc_ftol64(-1e20) == INT64_MIN);
}
