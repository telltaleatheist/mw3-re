#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Ours (docs/modding.md, mech3ax's JSON): a motion as unzbd writes it
// (`<motion>.json`: loop_time, frame_count, parts with their frames'
// translation x, y, z and rotation x, y, z, w) and the motion.zbd entry it
// stands for, both ways. The entry's bytes go through Script::read as the
// archive's do, so a JSON motion is read by the same bounded reader.
namespace zsis {

// The entry's bytes, or false with `why` "line N (at $.parts[2].frames[7].rotation.w): ...".
bool motion_from_json(const std::string& json, std::vector<uint8_t>* entry, std::string* why);
// The entry as unzbd's JSON; false when the bytes are not a motion unzbd writes
// (version 4, flags 12, the last keys the first's, the header's -1 and 1).
bool motion_to_json(const uint8_t* entry, size_t size, std::string* json);

}  // namespace zsis
