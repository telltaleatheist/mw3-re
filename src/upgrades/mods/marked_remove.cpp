#include "upgrades/mods/marked_remove.h"

#include <cctype>
#include <filesystem>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace mech3 {

namespace {
namespace fs = std::filesystem;

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool plain_name(const std::string& n) {
  return !n.empty() && n != "." && n != ".." && n.find_first_of("/\\") == std::string::npos && n.find('\0') == std::string::npos;
}
bool is_link(const fs::path& p) {
#if defined(_WIN32)
  // Any reparse point: a junction is one whatever the library's symlink_status says of it.
  const DWORD a = GetFileAttributesW(p.c_str());
  if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT)) return true;
#endif
  std::error_code ec;
  return fs::is_symlink(fs::symlink_status(p, ec));
}
// The link itself removed, never what it points at.
bool remove_link(const fs::path& p) {
#if defined(_WIN32)
  const DWORD a = GetFileAttributesW(p.c_str());
  if (a == INVALID_FILE_ATTRIBUTES) return false;
  return (a & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(p.c_str()) != 0 : DeleteFileW(p.c_str()) != 0;
#else
  std::error_code ec;
  return fs::remove(p, ec) && !ec;
#endif
}
bool real_dir(const fs::path& p) {
  std::error_code ec;
  return !is_link(p) && fs::is_directory(fs::symlink_status(p, ec));
}
bool real_file(const fs::path& p) {
  std::error_code ec;
  return !is_link(p) && fs::is_regular_file(fs::symlink_status(p, ec));
}
// Everything in `dir` removed, then `dir`. Only a real folder is entered.
bool remove_tree(const fs::path& dir, std::string* why) {
  std::error_code ec;
  std::vector<fs::path> kids;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) kids.push_back(it->path());
  for (const fs::path& k : kids) {
    bool ok;
    if (is_link(k)) ok = remove_link(k);
    else if (real_dir(k)) ok = remove_tree(k, why);
    else ok = fs::remove(k, ec) && !ec;
    if (!ok) {
      if (why && why->empty()) *why = "could not remove " + k.generic_string();
      return false;
    }
  }
  if (!fs::remove(dir, ec) || ec) {
    if (why) *why = "could not remove " + dir.generic_string();
    return false;
  }
  return true;
}
}  // namespace

bool path_is_link(const std::string& path) { return is_link(fs::path(path)); }

bool remove_marked_tree(const std::string& dir, const char* marker, std::string* why) {
  std::string none;
  std::string& said = why ? *why : none;
  said.clear();
  const fs::path d(dir);
  if (dir.empty() || !marker || !*marker || !plain_name(marker)) return said = "no folder or marker named", false;
  if (is_link(d)) return said = d.generic_string() + " is a link: not removed", false;
  if (!real_dir(d)) return said = d.generic_string() + " is not there", false;
  if (!real_file(d / marker)) return said = d.generic_string() + " has no " + marker + ": not made by the game, so not removed", false;
  return remove_tree(d, &said);
}

bool remove_marked_folder(const std::string& write_dir, const char* under, const std::string& name, const char* marker, std::string* why) {
  std::string none;
  std::string& said = why ? *why : none;
  said.clear();
  const std::string u = under ? under : "";
  if (write_dir.empty() || (u != "mods" && u != "campaigns")) return said = "not a folder of the game's", false;
  if (!plain_name(name)) return said = "\"" + name + "\" is not a folder's name", false;
  if (!marker || !*marker || !plain_name(marker)) return said = "no marker named", false;
  const fs::path parent = fs::path(write_dir) / u, dir = parent / name;
  if (is_link(parent) || is_link(dir)) return said = dir.generic_string() + " is a link: not removed", false;
  if (!real_dir(dir)) return said = dir.generic_string() + " is not there", false;
  if (!real_file(dir / marker)) return said = dir.generic_string() + " has no " + marker + ": not made by the game, so not removed", false;
  if (u == "campaigns") {
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
      const std::string n = lower(it->path().filename().string());
      if (real_dir(it->path()) && n != "pilots" && n != "configs")
        return said = dir.generic_string() + " holds a folder the game did not put there (" + it->path().filename().string() + "): not removed", false;
    }
  }
  return remove_tree(dir, &said);
}

}  // namespace mech3
