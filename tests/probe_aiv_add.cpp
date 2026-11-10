// Add vehicle's aiv.zrd record (upgrades/editor/add_vehicle) for
// test_aiv_add_mech3ax.py: an archive's aiv.zrd with a record of `type`
// added as Add vehicle adds it, written as a whole archive.
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gamez/zutil/zutl_zar.h"
#include "mech3/net_save.h"
#include "upgrades/editor/add_vehicle.h"

extern "C" int probe_aiv_add(const char* in, const char* out, const char* type, int net, float x, float y, float z,
                             float yaw, int team, char* name, int cap) {
  std::ifstream f(in, std::ios::binary);
  const std::vector<uint8_t> a((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (a.size() < 8) return -1;
  uint32_t count;
  std::memcpy(&count, a.data() + a.size() - 4, 4);
  const size_t table = a.size() - 8 - count * sizeof(zutl_zar::Entry);
  std::vector<uint8_t> aiv;
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry e;
    std::memcpy(&e, a.data() + table + i * sizeof e, sizeof e);
    if (std::strncmp(e.name, "aiv.zrd", sizeof e.name) == 0) aiv.assign(a.begin() + e.start, a.begin() + e.start + e.length);
  }
  if (aiv.empty()) return -2;
  int32_t items = 0;
  const std::vector<uint8_t> tags = mech3::aiv_tags_of_type(aiv.data(), aiv.size(), type, &items);
  const float pos[3] = {x, y, z};
  const std::vector<uint8_t> value = mech3::aiv_record(net, pos, yaw, team, tags, items);
  const std::string n = mech3::aiv_free_name(aiv.data(), aiv.size(), type);
  const std::vector<uint8_t> edited = mech3::aiv_with_record(aiv.data(), aiv.size(), n, value);
  if (edited.empty()) return -3;
  const std::vector<uint8_t> saved = mech3::ai::archive_with_entry(a, "aiv.zrd", edited);
  if (saved.empty()) return -4;
  std::ofstream(out, std::ios::binary).write(reinterpret_cast<const char*>(saved.data()), static_cast<std::streamsize>(saved.size()));
  if (static_cast<int>(n.size()) >= cap) return -5;
  std::memcpy(name, n.c_str(), n.size() + 1);
  return items;
}
