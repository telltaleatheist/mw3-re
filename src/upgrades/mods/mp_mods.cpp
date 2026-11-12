#include "upgrades/mods/mp_mods.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/data_version.h"
#include "mech3/patch12.h"
#include "mech3/patch12_tree.h"
#include "upgrades/mods/entry_archives.h"

namespace mech3 {

namespace {
namespace fs = std::filesystem;
std::string lower_of(std::string t) {
  for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return t;
}
std::string bytes_of(const std::string& f) {
  std::ifstream in(fs::path(f), std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
// One of the 28 files the 1.2 patch touched (data_version.h), by its path in lower case.
const DataFile* patched_file(const std::string& key) {
  int n = 0;
  const DataFile* all = data_files(&n);
  for (int i = 0; i < n; ++i)
    if (lower_of(all[i].path) == key) return &all[i];
  return nullptr;
}
bool measures(const std::string& bytes, uint32_t size, uint32_t crc) {
  return size && bytes.size() == size && zutl_zar::crc32(0, bytes.data(), bytes.size()) == crc;
}
// An archive file by its host path, for reading entries as trees.
struct Trees {
  zutl_zar::Archive a;
  bool confined = zutl::mounts().confined, ok = false;
  explicit Trees(const std::string& host) {
    zutl::mounts().confined = false;
    zutl_zar::init(a);
    ok = !host.empty() && zutl_zar::open(a, host.c_str());
  }
  ~Trees() {
    zutl_zar::close(a);
    zutl_zar::destroy(a);
    zutl::mounts().confined = confined;
  }
  zrdr::Node* read(const std::string& entry) {
    const int i = ok ? zutl_zar::find(a, entry.c_str()) : -1;
    std::FILE* f = i >= 0 ? zutl_zar::seek(a, i, nullptr) : nullptr;
    if (!f) return nullptr;
    zrdr::Node* root = zrdr::new_nodes(zrdr::List, 1);
    zrdr::read_node(f, root);
    return root;
  }
};
}  // namespace

// The base is the game as played, not the raw install (Owen, 2026-11-11: "they shouldn't need the same version
// of the original game. Ours is built on patch 1.2."): on 1.0's files the rebuild plays 1.2 (mech3/patch12.h),
// so a mod's entry counts when it differs, read as a tree, from the install's entry with the 1.2 layer's edits,
// or from the entry the layer adds; and a whole file that is 1.2's own (data_version.h, by size and CRC) over
// 1.0's counts for nothing. A mod's own entry is never edited by the layer, so what counts is its bytes.
bool mp_mounted_part(const std::string& host, const std::string& rel, const std::string& install, std::string* key, std::string* bytes) {
  const std::string of = entries_archive_of(rel);  // the archive a file of changed entries stands for
  const std::string mounted = of.empty() ? rel : of;
  *key = lower_of(mounted);
  bytes->clear();
  std::error_code ec;
  std::string own = install.empty() ? std::string() : zutl::any_case(install, mounted);
  if (!own.empty() && !fs::is_regular_file(own, ec)) own.clear();
  const DataFile* patched = patched_file(*key);
  const std::string own_bytes = own.empty() ? std::string() : bytes_of(own);
  // The install's file is 1.0's own (or, a file the patch adds, not there): the layer makes it 1.2's.
  const bool ten = patched && (patched->size_10 ? measures(own_bytes, patched->size_10, patched->crc_10) : own.empty());
  std::vector<ArchiveEntry> theirs;
  if (of.empty()) {
    *bytes = bytes_of(host);
    if (!own.empty() && *bytes == own_bytes) return false;                               // the install's own
    if (ten && measures(*bytes, patched->size_12, patched->crc_12)) return false;        // 1.2's own, as played anyway
  }
  if (!record_archive(*key) || !archive_read_entries(host, &theirs)) {
    // Any other file (and an archive whose table does not read): its bytes.
    if (!of.empty()) *bytes = bytes_of(host);
    return true;
  }
  bytes->clear();
  std::map<std::string, const ArchiveEntry*> mine;  // the install's entries by name
  std::vector<ArchiveEntry> ours;
  if (!own.empty() && archive_read_entries(own, &ours))
    for (const ArchiveEntry& e : ours) mine.emplace(lower_of(e.name), &e);
  Trees base(own), mod(host);
  // The install's entry as the game plays it against the mod's, as trees.
  const auto same_as_played = [&](const ArchiveEntry& e, const ArchiveEntry* in_install) {
    const std::string name = lower_of(e.name);
    zrdr::Node* want = nullptr;
    if (in_install) {
      want = base.read(in_install->name);
      if (want && ten) patch12::apply(*key, name, want);
    } else if (ten) {
      want = patch12::provided_with(*key, name, [&](const std::string& source, const std::string& entry) {
        const std::string file = zutl::any_case(install, source);
        Trees from(file);
        return from.read(entry);
      });
    }
    zrdr::Node* got = want ? mod.read(e.name) : nullptr;
    const bool same = want && got && patch12::tree::equal(*want, *got);
    if (want) zrdr::free_node(want);
    if (got) zrdr::free_node(got);
    return same;
  };
  std::map<std::string, std::string> parts;  // by name, so the table's order does not count
  std::map<std::string, bool> has;
  for (const ArchiveEntry& e : theirs) {
    const std::string name = lower_of(e.name);
    has[name] = true;
    const auto it = mine.find(name);
    const ArchiveEntry* in_install = it != mine.end() ? it->second : nullptr;
    // The install's own bytes change nothing, unless the layer edits that entry of 1.0's (a whole archive
    // then holds 1.0's value where the game plays 1.2's); bytes that differ may still read the same.
    const bool edited = ten && (patch12::has_edits(*key, name) || (*key == "zbd/reader.zbd" && name == "hud.zrd"));
    if (in_install && in_install->data == e.data && !(of.empty() && edited)) continue;
    if ((in_install || ten) && same_as_played(e, in_install)) continue;
    parts[name] = "entry " + name + '\0' + std::to_string(e.data.size()) + '\0' + std::string(e.data.begin(), e.data.end());
  }
  if (of.empty())  // a whole archive hides the install's entries it lacks; changed entries leave them
    for (const auto& [name, e] : mine)
      if (!has.count(name)) parts["\x01" + name] = "lacks " + name + '\0';
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
