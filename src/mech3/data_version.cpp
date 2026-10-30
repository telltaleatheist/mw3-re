#include "mech3/data_version.h"

#include <cstdio>
#include <vector>

#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"

namespace mech3 {
namespace {

struct Known {
  const char* path;
  uint32_t size_10, crc_10;  // 0, 0: not in 1.0 (the patch adds it)
  uint32_t size_12, crc_12;
};

// The 1.2 patch's data files (its data1.cab, "Patch Files\zbd") and the
// message table the port reads, each as the 1.0 disc and the 1.2 patch have
// it. CRCs by zutl_zar::crc32 from 0.
constexpr Known kFiles[] = {
    {"zbd/reader.zbd", 2143369, 0xea3cca83, 2146707, 0x19ef6c8c},
    {"zbd/interp.zbd", 99662, 0x41a8aabd, 99662, 0x36ebd755},
    {"zbd/c1/reader.zbd", 771601, 0x948e1eff, 774352, 0xb01a0321},
    {"zbd/c2/reader.zbd", 758993, 0xb3a52dcc, 759298, 0x989527dc},
    {"zbd/c2/readerm3.zbd", 68026, 0x47909bb0, 73391, 0xf87ae650},
    {"zbd/c3/readerm2.zbd", 64538, 0xd335bad6, 64542, 0xeafeea59},
    {"zbd/c3/readerm3.zbd", 177365, 0xc87581b4, 177365, 0x164a18e1},
    {"zbd/c3/readerm4.zbd", 116679, 0xfff8b52a, 116699, 0x007f3be3},
    {"zbd/c3/readerm5.zbd", 107888, 0xf9dd200d, 107912, 0x7d30e8d3},
    {"zbd/c3/readerm6.zbd", 125891, 0x55bb9635, 125863, 0x15f9b23c},
    {"zbd/c3/readermp3.zbd", 0, 0, 7262, 0xfd4b0956},
    {"zbd/c3/readermp4.zbd", 0, 0, 6662, 0xf1bfc250},
    {"zbd/c3/gamez.zbd", 12999820, 0x9798b442, 12994852, 0x41d99b82},
    {"zbd/c3/anim.zbd", 3879480, 0x9769911d, 3879480, 0x43014a87},
    {"zbd/c4b/readerm4.zbd", 34205, 0x3462d142, 41618, 0x48c8f206},
    {"zbd/c4b/readerm5.zbd", 26025, 0xc1b07763, 50984, 0x1c5e96dc},
    {"zbd/mech_engine_np.wav", 0, 0, 107850, 0x15b317cc},
    {"zbd/mech_footfall_2s.wav", 0, 0, 124048, 0x26a9df29},
    {"zbd/soil_concrete_2s.wav", 0, 0, 124048, 0xe4e6f0e4},
    {"zbd/soil_dirt_2s.wav", 0, 0, 124200, 0x2d9d975b},
    {"zbd/soil_grass_2s.wav", 0, 0, 124048, 0x984d002d},
    {"zbd/soil_in_water_2s.wav", 0, 0, 123912, 0xe7c3ac31},
    {"zbd/soil_metal_2s.wav", 0, 0, 124200, 0x2b7aa8ed},
    {"zbd/soil_mud_2s.wav", 0, 0, 110474, 0xd3a4e7ae},
    {"zbd/soil_snow_2s.wav", 0, 0, 124200, 0x3326788d},
    {"zbd/soil_water_2s.wav", 0, 0, 124048, 0xa04c1e75},
    {"Mech3Msg.dll", 120320, 0x04a9d33f, 123392, 0xe519c565},  // the message table (37 messages more in 1.2)
};

// The file's size and CRC; false when no mount has it.
bool measure(const char* path, uint32_t* size, uint32_t* crc) {
  std::FILE* f = zutl::open(path, "rb");
  if (!f) return false;
  std::vector<unsigned char> buf(1 << 16);
  uint32_t n = 0, c = 0;
  size_t got;
  while ((got = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
    c = zutl_zar::crc32(c, buf.data(), got);
    n += static_cast<uint32_t>(got);
  }
  std::fclose(f);
  *size = n, *crc = c;
  return true;
}

}  // namespace

DataVersion detect_data_version() {
  DataVersion v;
  std::vector<std::string> lost;  // files 1.0 already had, missing
  for (const Known& k : kFiles) {
    uint32_t size = 0, crc = 0;
    const bool have = measure(k.path, &size, &crc);
    const bool is_12 = have && size == k.size_12 && crc == k.crc_12;
    const bool is_10 = k.size_10 ? have && size == k.size_10 && crc == k.crc_10 : !have;
    if (is_12) {
      ++v.of_12;
      v.from_12.push_back(k.path);
    } else if (is_10) {
      ++v.of_10;
    } else if (!have) {
      ++v.missing;
      lost.push_back(k.path);
    } else {
      v.unknown.push_back(k.path);
    }
  }
  const int n = static_cast<int>(sizeof kFiles / sizeof kFiles[0]);
  if (!v.unknown.empty()) v.name = "unknown";
  else if (v.missing > 0) v.name = "incomplete", v.unknown = lost;
  else if (v.of_12 == n) v.name = "1.2";
  else if (v.of_10 == n) v.name = "1.0";
  else v.name = "1.0 with 1.2 files";
  if (v.name != "1.0 with 1.2 files") v.from_12.clear();
  return v;
}

std::string DataVersion::describe() const {
  std::string s = "data version " + name;
  if (name == "1.0 with 1.2 files")
    s += " (" + std::to_string(of_12) + " of " + std::to_string(of_10 + of_12) + " patched, " + std::to_string(of_10) + " not)";
  if (name == "unknown") s += " (neither 1.0's nor 1.2's:";
  if (name == "incomplete") s += " (missing:";
  if (name == "unknown" || name == "incomplete") {
    for (const std::string& f : unknown) s += " " + f;
    s += ")";
  }
  return s;
}

}  // namespace mech3
