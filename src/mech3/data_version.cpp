#include "mech3/data_version.h"

#include <cstdio>
#include <vector>

#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"

namespace mech3 {
namespace {

// The 1.2 patch's data files (its data1.cab, "Patch Files\zbd"), the
// message table and the force-feedback effects the port reads, each as the
// 1.0 disc and the 1.2 patch have it. CRCs by zutl_zar::crc32 from 0.
constexpr DataFile kFiles[] = {
    {"zbd/reader.zbd", 2143369, 0xea3cca83, 2146707, 0x19ef6c8c,
     "5e17037f3634480f74265b3e378e4513b84cae518e20bc55cd5c6e9c6683851a"},
    {"zbd/interp.zbd", 99662, 0x41a8aabd, 99662, 0x36ebd755,
     "f4ae013c419984cd1139dcbe06695efddd1875e88baa55bf0997adf36907b171"},
    {"zbd/c1/reader.zbd", 771601, 0x948e1eff, 774352, 0xb01a0321,
     "482501ffbb210ccf516b8655ece2df27e0558d24cc7c9977ef9edf9ea21c4046"},
    {"zbd/c2/reader.zbd", 758993, 0xb3a52dcc, 759298, 0x989527dc,
     "9852cbcca859adcc377a23bca19193daec918f6c9d495056ec6a29c1e74946f8"},
    {"zbd/c2/readerm3.zbd", 68026, 0x47909bb0, 73391, 0xf87ae650,
     "e263d938c9caa7feae09936b1fd6affce9909a2c1d10a86027db872e698e940f"},
    {"zbd/c3/readerm2.zbd", 64538, 0xd335bad6, 64542, 0xeafeea59,
     "019b13a3e384b5506328fab6e408fc63423d4a81f9e73ccdff3a4af066b60d42"},
    {"zbd/c3/readerm3.zbd", 177365, 0xc87581b4, 177365, 0x164a18e1,
     "54fe45f8b378d5287a148210d2fb87f04485d96c547a9e1d3314edde719ff3fd"},
    {"zbd/c3/readerm4.zbd", 116679, 0xfff8b52a, 116699, 0x007f3be3,
     "10c837259ef8eccd686f84195e6359d381e2abd8f04345f0e010208c0edb82ee"},
    {"zbd/c3/readerm5.zbd", 107888, 0xf9dd200d, 107912, 0x7d30e8d3,
     "89043c04714b2f38bf806190d2d418cd25b1f02a124096e2aefe1f4151cd0ea6"},
    {"zbd/c3/readerm6.zbd", 125891, 0x55bb9635, 125863, 0x15f9b23c,
     "3e73aa429aa8461387322e245ef41337830c4dcd7def9afd47d0384a426c1ef1"},
    {"zbd/c3/readermp3.zbd", 0, 0, 7262, 0xfd4b0956,
     "2ba33f7279334ebee6d2ba466fba0223054119ee4dd30bfbc8a83e1962e8d251"},
    {"zbd/c3/readermp4.zbd", 0, 0, 6662, 0xf1bfc250,
     "ff567a76c1aea1417d6062b5f8445120fe427be3a9dc451d6d9c538d4002d569"},
    {"zbd/c3/gamez.zbd", 12999820, 0x9798b442, 12994852, 0x41d99b82,
     "3110b96b7ff30489da339038770a5fb08b8dc7f91b1aab89342c35bc97a0204e"},
    {"zbd/c3/anim.zbd", 3879480, 0x9769911d, 3879480, 0x43014a87,
     "69eb5a140e34c82cd9675c76789ca00b7c5cabb6c56c919a95d2659a9ee6fc09"},
    {"zbd/c4b/readerm4.zbd", 34205, 0x3462d142, 41618, 0x48c8f206,
     "a2d04807664e874e2582563f5c0fbc8945ee036532207d43b43d822a52f7f2c2"},
    {"zbd/c4b/readerm5.zbd", 26025, 0xc1b07763, 50984, 0x1c5e96dc,
     "10d57108b2385fbb504e057aef82c4d2e5cc7ef124db8a3282e8f1c6089e4d98"},
    {"zbd/mech_engine_np.wav", 0, 0, 107850, 0x15b317cc,
     "099ef778bf1e992ba2026d22cb9c653753261cc74005cebf8322878a263cbd2a"},
    {"zbd/mech_footfall_2s.wav", 0, 0, 124048, 0x26a9df29,
     "492e86565e68c0d80666c809bd8522b3511cb61ddd99a059176e395d18a39093"},
    {"zbd/soil_concrete_2s.wav", 0, 0, 124048, 0xe4e6f0e4,
     "324ba7300161a12838f96b0ce8ef0a2a41a21e95d5573facda0494c06a4d32a2"},
    {"zbd/soil_dirt_2s.wav", 0, 0, 124200, 0x2d9d975b,
     "92d568ff620453fa4d268b1acad2d4aeeaa4122c4e6930f8669fc026ae3d9f05"},
    {"zbd/soil_grass_2s.wav", 0, 0, 124048, 0x984d002d,
     "11cb30d5fed7be0e0a51b6905e1bc13ae7315ed1edf04606c325ff04a83a4f35"},
    {"zbd/soil_in_water_2s.wav", 0, 0, 123912, 0xe7c3ac31,
     "76f8b06395beb4c31425dc87c59d1709347c00e006c15f85b0f8ecb7702d70e2"},
    {"zbd/soil_metal_2s.wav", 0, 0, 124200, 0x2b7aa8ed,
     "01ea56319e77539e79e81471ab03ed5f1d366085238adb309a0d141e0c8f3b95"},
    {"zbd/soil_mud_2s.wav", 0, 0, 110474, 0xd3a4e7ae,
     "d933c94053f5bb7fbcdb91111284f23664007228753623b8cb38d55177e053ef"},
    {"zbd/soil_snow_2s.wav", 0, 0, 124200, 0x3326788d,
     "d647280939dbfd44a027f88412bc98b6010e1d6f0d1c34dd62007719b11586d2"},
    {"zbd/soil_water_2s.wav", 0, 0, 124048, 0xa04c1e75,
     "33320d09f4ea21889cd8d03d8dbc8ef82144670e41e7942df25c0bef05190ff4"},
    {"Mech3Msg.dll", 120320, 0x04a9d33f, 123392, 0xe519c565,
     "8899fabb7e57383a9a483cb048636bcc027334b48941aa49835cdcc1c4715b8c"},  // the message table (37 messages more in 1.2)
    {"force_eff.ifr", 2936, 0xd5f07541, 4113, 0x4957166c,  // the force-feedback effects (impact land, dead spring, dead dance added)
     "5d021f79378493302509ef3fad31282961e1f3c037367ae4f00faa154d3d2a26"},
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

const DataFile* data_files(int* count) {
  *count = static_cast<int>(sizeof kFiles / sizeof kFiles[0]);
  return kFiles;
}

DataVersion detect_data_version() {
  DataVersion v;
  std::vector<std::string> lost;  // files 1.0 already had, missing
  for (const DataFile& k : kFiles) {
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
