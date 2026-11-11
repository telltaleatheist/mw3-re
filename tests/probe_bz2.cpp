// Our bzip2 reader (upgrades/mods/bz2_read.h) for tests/differential/test_bz2_read.py: Python's own bz2 module
// makes the streams.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "upgrades/mods/bz2_read.h"

extern "C" {
// The stream unpacked into `out` (room for `cap`, never more than `max_out` written): the bytes' count, or -1
// refused. `used`, when given, is how much of the stream was read.
int32_t probe_bz2(const uint8_t* in, uint32_t n, uint32_t max_out, uint8_t* out, uint32_t cap, uint32_t* used) {
  std::vector<uint8_t> o;
  std::string why;
  size_t u = 0;
  if (!mech3::bz2_decompress(in, n, max_out, &o, &why, &u)) return -1;
  if (o.size() > cap || o.size() > max_out) return -2;  // never: the reader's own bound
  if (!o.empty()) std::memcpy(out, o.data(), o.size());
  if (used) *used = static_cast<uint32_t>(u);
  return static_cast<int32_t>(o.size());
}
}
