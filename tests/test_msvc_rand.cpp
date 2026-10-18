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
