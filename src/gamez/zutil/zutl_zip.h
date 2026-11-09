#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

// Ours: a ZIP read from its central directory, entry by entry, stored or
// deflated, its CRC checked. Moved from patch_import.cpp (the 1.2 patch's
// archives); a mod's zip of mech3ax JSON is read with it too (zreader,
// docs/modding.md).
namespace zutl_zip {

// Raw deflate into `out` (its size the expected length, or grown when 0).
// A chunk that ends on a flush, not a final block, is whole once every byte
// of it is taken.
bool inflate_raw(const uint8_t* in, size_t n, std::vector<uint8_t>* out, size_t expect);
// ZIP's CRC-32 (reflected, 0xedb88320), not zutl_zar's.
uint32_t zip_crc(const uint8_t* p, size_t n);

// A ZIP read from its central directory, entry by entry (no Zip64: the
// patch's archives are a few MB). A self-extracting exe with a ZIP appended
// reads the same, its offsets shifted by the stub.
struct Zip {
  struct Entry {
    std::string name;
    uint32_t method, crc, csize, usize, local;
  };
  std::FILE* f = nullptr;
  std::vector<Entry> entries;
  ~Zip();
  bool open(const std::filesystem::path& p);
  bool read(const Entry& en, std::vector<uint8_t>* out, std::string* error);
};

}  // namespace zutl_zip
