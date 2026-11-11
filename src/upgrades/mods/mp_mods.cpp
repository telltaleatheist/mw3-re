#include "upgrades/mods/mp_mods.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include "gamez/zutil/zutl_path.h"
#include "upgrades/mods/entry_archives.h"

namespace mech3 {

bool mp_mounted_part(const std::string& host, const std::string& rel, const std::string& install, std::string* key, std::string* bytes) {
  namespace fs = std::filesystem;
  auto lower = [](std::string t) {
    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return t;
  };
  auto read = [](const std::string& f) {
    std::ifstream in(fs::path(f), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  const std::string of = entries_archive_of(rel);  // the archive a file of changed entries stands for
  const std::string mounted = of.empty() ? rel : of;
  *key = lower(mounted);
  bytes->clear();
  std::error_code ec;
  std::string own = install.empty() ? std::string() : zutl::any_case(install, mounted);
  if (!own.empty() && !fs::is_regular_file(own, ec)) own.clear();
  std::vector<ArchiveEntry> theirs;
  if (!record_archive(*key) || !archive_read_entries(host, &theirs)) {
    // Any other file (and an archive whose table does not read): its bytes, unless they are the install's.
    *bytes = read(host);
    return own.empty() || *bytes != read(own);
  }
  std::map<std::string, const ArchiveEntry*> mine;  // the install's entries by name
  std::vector<ArchiveEntry> ours;
  if (!own.empty() && archive_read_entries(own, &ours))
    for (const ArchiveEntry& e : ours) mine.emplace(lower(e.name), &e);
  std::map<std::string, std::string> parts;  // by name, so the table's order does not count
  for (const ArchiveEntry& e : theirs) {
    const std::string name = lower(e.name);
    const auto it = mine.find(name);
    if (it != mine.end() && it->second->data == e.data) continue;  // the install's own: changes nothing
    parts[name] = "entry " + name + '\0' + std::to_string(e.data.size()) + '\0' + std::string(e.data.begin(), e.data.end());
  }
  if (of.empty()) {  // a whole archive hides the install's entries it lacks; changed entries leave them
    std::map<std::string, bool> has;
    for (const ArchiveEntry& e : theirs) has[lower(e.name)] = true;
    for (const auto& [name, e] : mine)
      if (!has.count(name)) parts["\x01" + name] = "lacks " + name + '\0';
  }
  for (const auto& [name, part] : parts) *bytes += part;
  return !parts.empty();
}

void mp_mods_each_put(std::vector<uint32_t>& data, const std::vector<uint64_t>& hashes) {
  const size_t n = std::min<size_t>(hashes.size(), 16);
  data.push_back(kModsEachMark);
  data.push_back(static_cast<uint32_t>(n));
  for (size_t i = 0; i < n; ++i) {
    data.push_back(static_cast<uint32_t>(hashes[i]));
    data.push_back(static_cast<uint32_t>(hashes[i] >> 32));
  }
}

bool mp_mods_each_get(const std::vector<uint32_t>& d, size_t at, std::vector<uint64_t>* hashes) {
  hashes->clear();
  if (at + 2 > d.size() || d[at] != kModsEachMark) return false;
  const uint32_t n = std::min<uint32_t>(d[at + 1], 16);
  at += 2;
  for (uint32_t i = 0; i < n && at + 1 < d.size(); ++i, at += 2)
    hashes->push_back(static_cast<uint64_t>(d[at]) | static_cast<uint64_t>(d[at + 1]) << 32);
  return true;
}

namespace {
struct Parts {
  std::vector<std::string> missing, differ, extra;  // of `b` against the host `a`
  bool told = false;
};
Parts compare(const MpMods& a, const MpMods& b) {
  Parts p;
  if (a.hashes.size() != a.names.size() || b.hashes.size() != b.names.size()) return p;  // a side that sent none
  p.told = true;
  for (size_t i = 0; i < a.names.size(); ++i) {
    const auto it = std::find(b.names.begin(), b.names.end(), a.names[i]);
    if (it == b.names.end()) p.missing.push_back(a.names[i]);
    else if (b.hashes[static_cast<size_t>(it - b.names.begin())] != a.hashes[i]) p.differ.push_back(a.names[i]);
  }
  for (const std::string& n : b.names)
    if (std::find(a.names.begin(), a.names.end(), n) == a.names.end()) p.extra.push_back(n);
  return p;
}
// At most three named, then "and N more".
std::string named(const std::vector<std::string>& names) {
  std::string t;
  for (size_t i = 0; i < names.size() && i < 3; ++i) t += (i ? ", " : "") + names[i];
  if (names.size() > 3) t += " and " + std::to_string(names.size() - 3) + " more";
  return t;
}
void add(std::string& t, const std::string& sentence) { t += (t.empty() ? "" : " ") + sentence; }
}  // namespace

std::string mp_mods_difference(const MpMods& host, const MpMods& mine) {
  const Parts p = compare(host, mine);
  std::string t;
  if (!p.missing.empty()) add(t, "You do not have: " + named(p.missing) + ".");
  if (!p.differ.empty()) add(t, "Your copy of " + named(p.differ) + " differs from the host's.");
  if (!p.extra.empty()) add(t, "The host does not use: " + named(p.extra) + ".");
  return t;
}

std::string mp_mods_difference_of(const std::string& who, const MpMods& host, const MpMods& theirs) {
  const Parts p = compare(host, theirs);
  std::string t;
  if (!p.missing.empty()) add(t, who + " does not have: " + named(p.missing) + ".");
  if (!p.differ.empty()) add(t, who + "'s copy of " + named(p.differ) + " differs from yours.");
  if (!p.extra.empty()) add(t, who + " also uses: " + named(p.extra) + ".");
  return t;
}

}  // namespace mech3
