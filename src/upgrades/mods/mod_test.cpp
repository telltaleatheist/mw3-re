#include "upgrades/mods/mod_test.h"

#include "mech3/mech_reader.h"

namespace mech3 {

int32_t& mod_test_reloads() {
  static int32_t n = 0;
  return n;
}

void mod_test_reload() {
  free_dfns();
  ++mod_test_reloads();
}

}  // namespace mech3
