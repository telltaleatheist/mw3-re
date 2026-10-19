#include "mech3/mech_names.h"

#include <cstring>

namespace mech3 {

// Mech3.exe 1.2 @ 0x00484e40. lleg and rleg are aliases of the thighs.
int32_t part_id(const char* name) {
  static const struct { const char* name; int32_t id; } kIds[] = {
      {"hip", 15},    {"torso", 1},   {"larm", 2},   {"rarm", 3},   {"lhand", 9},  {"rhand", 10},
      {"lthigh", 4},  {"rthigh", 5},  {"lleg", 4},   {"rleg", 5},   {"lcalf", 11}, {"rcalf", 12},
      {"lfoot", 13},  {"rfoot", 14},  {"head", 6},   {"ltorso", 7}, {"rtorso", 8}};
  if (!name) return -1;
  for (const auto& e : kIds)
    if (std::strcmp(name, e.name) == 0) return e.id;
  return -1;
}

// Mech3.exe 1.2 @ 0x00484cc0
const char* part_name(int32_t id) {
  static const char* const kNames[] = {"torso", "larm",   "rarm",  "lthigh", "rthigh", "head",  "ltorso", "rtorso",
                                       "lhand", "rhand",  "lcalf", "rcalf",  "lfoot",  "rfoot", "hip"};
  if (id < 1 || id > 15) return "none";
  return kNames[id - 1];
}

}  // namespace mech3
