#pragma once
#include <string>
#include <vector>

// Which MechWarrior 3 data the mounts hold (ours, not original code;
// docs/assets.md, Data versions). The official 1.2 patch replaces or adds 26
// data files and the message table Mech3Msg.dll; each of the 27 is matched by
// size and CRC (zutl_zar::crc32) against 1.0's and 1.2's. The exe plays no
// part: the port reads only zbd\, video\ and Mech3Msg.dll.
namespace mech3 {

struct DataVersion {
  // "1.2", "1.0", "1.0 with 1.2 files" (some files patched, some not),
  // "unknown" (a file that is neither: another version, perhaps 1.1, or a
  // mod) or "incomplete" (a file 1.0 already had is missing).
  std::string name;
  int of_10 = 0, of_12 = 0, missing = 0;  // of the 27
  std::vector<std::string> from_12;       // in a mix, the files that are 1.2's
  std::vector<std::string> unknown;       // files that match neither (incomplete: the missing ones)
  std::string describe() const;           // one line for the start-up print
};

DataVersion detect_data_version();  // through zutl's mounts

}  // namespace mech3
