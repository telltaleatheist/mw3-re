#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Ours (docs/modding.md, IMPORT from an installer): a bzip2 stream unpacked,
// written from the format (no third-party code), for the files inside a mod's
// installer. The data comes from the internet, so everything is bounded: the
// output never passes `max_out` (the installer entry's declared size), every
// table and index is checked, each block's CRC and the stream's are checked,
// and anything wrong is a refusal with `why`. One stream: bytes after its end
// are not read (`*used`, when given, says how many were).
namespace mech3 {

bool bz2_decompress(const uint8_t* data, size_t size, size_t max_out, std::vector<uint8_t>* out, std::string* why = nullptr,
                    size_t* used = nullptr);

}  // namespace mech3
