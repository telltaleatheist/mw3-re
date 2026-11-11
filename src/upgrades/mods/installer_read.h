#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Ours (docs/modding.md, IMPORT from an installer; docs/installer-format.md):
// a mod that ships as a Clickteam Install Creator program read as an archive.
// Nothing of the program runs and no file of it is written anywhere: its
// appended blocks are parsed, its file list read, and a file's bytes unpacked
// on request. The file comes from the internet, so every size and offset is
// checked against the file and against caps; anything wrong is a refusal.
namespace mech3 {

struct InstallerFile {
  std::string name;      // as the installer names it, '/'-separated and checked (nothing outside its folder)
  std::string path;      // under the game folder ("" when it is outside it): installer_map_to_game sets it
  uint64_t size = 0;     // unpacked
  uint64_t data_at = 0;  // in the installer file: its method byte, then its stream
  uint64_t packed = 0;   // with the method byte
};

struct Installer {
  bool ok = false;
  std::string error;      // why it was refused ("" for a file that is not such an installer at all)
  bool recognised = false;  // it has the maker's signature after its program
  std::string title;      // its texts' first line, when the maker's version has one
  std::string default_folder;  // the folder it would install into (its texts' first "X:\..." line), '/'-separated
  std::string game_root;  // what installer_map_to_game found: the part of each name before the game's folder
  std::vector<InstallerFile> files;
  int others = 0;         // entries that are not files to place (its uninstaller, folders)
};

struct InstallerLimits {
  uint64_t max_files = 20000;
  uint64_t max_list_bytes = 16u << 20;      // the file list, unpacked
  uint64_t max_file_bytes = uint64_t{2} << 30;  // one file, unpacked
};

// True when the file is a program with Clickteam Install Creator's blocks
// after it (by its bytes, never its name).
bool installer_is_clickteam(const std::string& path);
bool installer_bytes_are_clickteam(const uint8_t* data, size_t size);  // the same on a program held in memory
// Its file list. `recognised` false: not one (error ""); recognised and !ok: one we refuse, with why.
Installer installer_open(const std::string& path, const InstallerLimits& limits = {});
// Each file's place under a game folder (`path`), from where zbd sits in the names, else from the default
// folder's tail; files outside the game's folder keep an empty path. False, with why, when it cannot be told.
bool installer_map_to_game(Installer* in, std::string* why);
// One file's bytes: exactly `f.size` of them, or a refusal.
bool installer_extract(const std::string& path, const InstallerFile& f, std::vector<uint8_t>* out, std::string* why = nullptr);

}  // namespace mech3
