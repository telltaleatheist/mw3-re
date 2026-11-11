#include "upgrades/mods/import_loadout.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "upgrades/mods/marked_remove.h"

namespace fs = std::filesystem;

namespace mech3 {
namespace {
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
constexpr uintmax_t kMost = 1 << 20;  // a loadout is a few hundred bytes
// A folder's entry of that name in any letter case.
bool there(const fs::path& dir, const std::string& name) {
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (lower(it->path().filename().string()) == lower(name)) return true;
  return false;
}
}  // namespace

bool loadout_name(const std::string& path) {
  const std::string l = lower(path);
  return l.size() > 4 && l.compare(l.size() - 4, 4, ".m3c") == 0;
}

bool loadout_reads(const std::string& path, std::string* why) {
  std::string none;
  std::string& said = why ? *why : none;
  std::error_code ec;
  const uintmax_t size = fs::file_size(path, ec);
  if (ec || fs::is_symlink(path, ec) || path_is_link(path) || !fs::is_regular_file(path, ec)) return said = "it is not a file", false;
  if (size > kMost) return said = "it is too big to be a loadout", false;
  std::ifstream f(path, std::ios::binary);
  // Two names, each its length then its letters (the file's, then the loadout's own: 0x004260c0, 0x00424600),
  // then the loadout's items and numbers: at least its fixed part.
  uintmax_t at = 0;
  for (int k = 0; k < 2; ++k) {
    uint32_t n = 0;
    f.read(reinterpret_cast<char*>(&n), 4);
    if (!f || n > 255 || at + 4 + n > size) return said = "it does not read as a mech loadout (its name)", false;
    f.seekg(n, std::ios::cur);
    at += 4 + n;
  }
  if (size - at < 40) return said = "it does not read as a mech loadout (it ends too soon)", false;
  return true;
}

ImportLoadouts import_loadouts(const std::string& write_dir, const std::vector<std::string>& files) {
  ImportLoadouts r;
  std::error_code ec;
  const fs::path dir = fs::path(write_dir) / "import";
  for (const std::string& file : files) {
    const fs::path src(file);
    const std::string leaf = src.filename().string();
    std::string why;
    if (write_dir.empty()) why = "there is no settings folder";
    else if (!loadout_name(leaf)) why = "it is not a .m3c file";
    else loadout_reads(file, &why);
    if (why.empty() && (fs::is_symlink(dir, ec) || path_is_link(dir.string()))) why = "the import folder is a link";
    if (why.empty()) {
      fs::create_directories(dir, ec);
      const std::string stem = leaf.substr(0, leaf.size() - 4), ext = leaf.substr(leaf.size() - 4);
      std::string name = leaf;
      for (int n = 2; n < 1000 && (there(dir, name) || there(dir, name + ".done") || there(dir, name + ".rows")); ++n)
        name = stem + " (" + std::to_string(n) + ")" + ext;
      if (there(dir, name)) why = "no free name for it in the import folder";
      else if (!fs::copy_file(src, dir / name, fs::copy_options::none, ec) || ec) {
        fs::remove(dir / name, ec);  // a part of one, made by this call
        why = "it could not be copied into the import folder";
      } else {
        const fs::path rows = file + ".rows";  // the mods' rows it names
        if (fs::is_regular_file(rows, ec) && !fs::is_symlink(rows, ec)) fs::copy_file(rows, dir / (name + ".rows"), fs::copy_options::none, ec);
        r.added.push_back(name);
      }
    }
    if (!why.empty()) r.refused.push_back(leaf + ": " + why);
  }
  const size_t n = r.added.size();
  if (n) r.message = "Added " + std::to_string(n) + (n == 1 ? " mech loadout" : " mech loadouts") + ": in a mech lab, Ctrl+I takes " + (n == 1 ? "it." : "the next one.");
  if (!r.refused.empty()) r.message += (n ? " Not added: " : "Not added: ") + r.refused.front() + (r.refused.size() > 1 ? " (and " + std::to_string(r.refused.size() - 1) + " more)." : ".");
  return r;
}

}  // namespace mech3
