// Probe for tests/differential/test_pm_mip_bias.py: Pirate's Moon's MipBias (0x00598230, the port's
// zvid::set_mip_bias): the value it keeps for a given bias.
#include <cstdint>
#include <cstring>

#include "gamez/zvideo/zvid_ddd3d.h"

extern "C" {
uint32_t probe_pm_mip_bias(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  zvid::set_mip_bias(f);
  std::memcpy(&bits, &zvid::device_state().mip_bias, 4);
  return bits;
}
}  // extern "C"
