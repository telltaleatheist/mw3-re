#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "gamez/zutil/zutl_zip.h"

// A zip of stored (uncompressed) files, as a modder's tool may write one.
inline void write_stored_zip(const std::string& path, const std::vector<std::pair<std::string, std::string>>& files) {
  std::string out, dir;
  auto le = [](std::string& b, uint32_t v, int n) {
    for (int i = 0; i < n; ++i) b.push_back(static_cast<char>(v >> (8 * i)));
  };
  for (const auto& [name, data] : files) {
    const uint32_t crc = zutl_zip::zip_crc(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    const uint32_t at = static_cast<uint32_t>(out.size()), n = static_cast<uint32_t>(data.size());
    le(out, 0x04034b50, 4), le(out, 20, 2), le(out, 0, 2), le(out, 0, 2), le(out, 0, 4), le(out, crc, 4), le(out, n, 4), le(out, n, 4);
    le(out, static_cast<uint32_t>(name.size()), 2), le(out, 0, 2), out += name, out += data;
    le(dir, 0x02014b50, 4), le(dir, 20, 2), le(dir, 20, 2), le(dir, 0, 2), le(dir, 0, 2), le(dir, 0, 4), le(dir, crc, 4), le(dir, n, 4);
    le(dir, n, 4), le(dir, static_cast<uint32_t>(name.size()), 2), le(dir, 0, 2), le(dir, 0, 2), le(dir, 0, 2), le(dir, 0, 2), le(dir, 0, 4);
    le(dir, at, 4), dir += name;
  }
  const uint32_t start = static_cast<uint32_t>(out.size());
  out += dir;
  le(out, 0x06054b50, 4), le(out, 0, 2), le(out, 0, 2), le(out, static_cast<uint32_t>(files.size()), 2), le(out, static_cast<uint32_t>(files.size()), 2);
  le(out, static_cast<uint32_t>(dir.size()), 4), le(out, start, 4), le(out, 0, 2);
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
  std::ofstream(path, std::ios::binary) << out;
}
