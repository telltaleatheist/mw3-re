#include "upgrades/mods/entry_archives.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/net_save.h"
#include "upgrades/mods/marked_remove.h"

namespace fs = std::filesystem;

namespace mech3 {
namespace {
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool ends_with(const std::string& s, const char* e) {
  const size_t n = std::strlen(e);
  return s.size() >= n && s.compare(s.size() - n, n, e) == 0;
}
std::vector<uint8_t> bytes_of(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
bool write_bytes(const fs::path& p, const std::vector<uint8_t>& b) {
  std::ofstream o(p, std::ios::binary | std::ios::trunc);
  o.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  o.close();
  return static_cast<bool>(o);
}
// Names as the archive's find matches them; false when two entries share one (which of them the game reads is
// the table's order: not ours to reorder).
bool by_name(const std::vector<ArchiveEntry>& entries, std::map<std::string, const ArchiveEntry*>* out) {
  for (const ArchiveEntry& e : entries)
    if (!out->emplace(lower(e.name), &e).second) return false;
  return true;
}
// The reader archives under a mod's zbd folder (itself and one folder down), and their .entries files, as
// paths relative to the mod.
std::vector<std::string> files_in_zbd(const fs::path& root) {
  std::vector<std::string> out;
  std::error_code ec;
  const std::string zbd = zutl::any_case(root.generic_string(), "zbd");
  if (zbd.empty()) return out;
  auto look = [&](const fs::path& dir) {
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
      if (it->is_regular_file(ec) && !it->is_symlink(ec) && !path_is_link(it->path().string()))
        out.push_back(fs::relative(it->path(), root, ec).generic_string());
  };
  look(zbd);
  std::vector<fs::path> worlds;
  for (fs::directory_iterator it(zbd, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_directory(ec) && !it->is_symlink(ec) && !path_is_link(it->path().string())) worlds.push_back(it->path());
  for (const fs::path& w : worlds) look(w);
  std::sort(out.begin(), out.end());
  return out;
}
// The archives here are opened by host paths of ours (a staging copy, a mod's folder, the install), which the
// running game's confined names refuse (zutl::Mounts::confined, as mod_check's): unconfined for the call.
struct Unconfined {
  bool was = zutl::mounts().confined;
  Unconfined() { zutl::mounts().confined = false; }
  ~Unconfined() { zutl::mounts().confined = was; }
};
uint64_t mix(uint64_t h, const std::string& s) {
  for (unsigned char c : s) h = (h ^ c) * 1099511628211ull;
  return (h ^ 0xff) * 1099511628211ull;
}
uint64_t mix_file(uint64_t h, const std::string& path) {
  std::error_code ec;
  h = mix(h, path);
  h = mix(h, std::to_string(static_cast<unsigned long long>(fs::file_size(path, ec))));
  return mix(h, std::to_string(static_cast<long long>(fs::last_write_time(path, ec).time_since_epoch().count())));
}
}  // namespace

std::string entries_archive_of(const std::string& rel) {
  const std::string l = lower(rel);
  if (!ends_with(l, ".zbd.entries")) return {};
  return rel.substr(0, rel.size() - std::strlen(kEntriesSuffix));
}

bool record_archive(const std::string& rel) {
  std::string l = lower(rel);
  std::replace(l.begin(), l.end(), '\\', '/');
  if (l.rfind("zbd/", 0) != 0 || !ends_with(l, ".zbd") || std::count(l.begin(), l.end(), '/') > 2) return false;
  return l.compare(l.rfind('/') + 1, 6, "reader") == 0;
}

bool archive_read_entries(const std::string& host, std::vector<ArchiveEntry>* out) {
  out->clear();
  const Unconfined unconfined;
  std::error_code ec;
  const uintmax_t size = fs::file_size(host, ec);
  if (ec) return false;
  zutl_zar::Archive a;
  zutl_zar::init(a);
  bool ok = zutl_zar::open(a, host.c_str());
  for (int i = 0; ok && i < a.count; ++i) {
    const zutl_zar::Entry& e = a.entries[i];
    if (e.start > size || e.length > size - e.start) {
      ok = false;
      break;
    }
    ArchiveEntry x;
    x.name.assign(e.name, strnlen(e.name, sizeof e.name));
    x.data.resize(e.length);
    uint32_t n = e.length;
    if (e.length && zutl_zar::read(a, i, x.data.data(), &n) != 0) ok = false;
    out->push_back(std::move(x));
  }
  zutl_zar::destroy(a);
  if (!ok) out->clear();
  return ok;
}

std::vector<std::string> archive_entry_names(const std::string& host) {
  std::vector<ArchiveEntry> all;
  std::vector<std::string> names;
  if (archive_read_entries(host, &all))
    for (const ArchiveEntry& e : all) names.push_back(e.name);
  return names;
}

EntriesCut entries_cut(const std::string& mod_file, const std::string& own_file) {
  EntriesCut r;
  const Unconfined unconfined;
  std::vector<ArchiveEntry> theirs, mine;
  if (!archive_read_entries(mod_file, &theirs)) return r.why = "its table does not read", r;
  if (!archive_read_entries(own_file, &mine)) return r.why = "the game's own does not read to compare it with", r;
  std::map<std::string, const ArchiveEntry*> a, b;
  if (!by_name(theirs, &a) || !by_name(mine, &b)) return r.why = "two of its entries have one name", r;
  std::vector<const ArchiveEntry*> keep;
  for (const ArchiveEntry& e : theirs) {
    const auto it = b.find(lower(e.name));
    if (it == b.end()) ++r.added, keep.push_back(&e);
    else if (it->second->data != e.data) ++r.changed, keep.push_back(&e);
    else ++r.same;
  }
  for (const auto& [name, e] : b)
    if (!a.count(name)) ++r.missing;
  std::error_code ec;
  if (keep.empty()) {
    fs::remove(mod_file, ec);
    r.what = ec ? EntriesCut::kLeft : EntriesCut::kSame;
    if (ec) r.why = "it could not be removed";
    return r;
  }
  const std::string out = mod_file + kEntriesSuffix;
  if (fs::exists(fs::symlink_status(out, ec))) return r.why = "there is a file " + fs::path(out).filename().string() + " beside it already", r;
  zutl_zar::Archive z;
  zutl_zar::init(z);
  bool ok = zutl_zar::create(z, out.c_str());
  for (const ArchiveEntry* e : keep)
    if (ok) zutl_zar::add(z, e->name.c_str(), e->data.data(), static_cast<uint32_t>(e->data.size()));
  ok = ok && zutl_zar::close(z);
  zutl_zar::destroy(z);
  // Read back: every kept entry's name and bytes as they were, or the archive stays whole.
  std::vector<ArchiveEntry> back;
  ok = ok && archive_read_entries(out, &back) && back.size() == keep.size();
  for (size_t i = 0; ok && i < keep.size(); ++i) ok = lower(back[i].name) == lower(keep[i]->name) && back[i].data == keep[i]->data;
  if (!ok) {
    fs::remove(out, ec);
    return r.why = "its changed entries could not be written", r;
  }
  fs::remove(mod_file, ec);
  if (ec) {  // both would then be there: the whole one is the one kept
    fs::remove(out, ec);
    return r.why = "it could not be removed", r;
  }
  r.what = EntriesCut::kCut;
  return r;
}

ModCut entries_cut_mod(const std::string& root, const std::string& install) {
  ModCut t;
  if (install.empty()) return t;
  for (const std::string& rel : files_in_zbd(root)) {
    if (!record_archive(rel)) continue;
    const std::string own = zutl::any_case(install, rel);
    std::error_code ec;
    if (own.empty() || !fs::is_regular_file(own, ec)) {  // a new archive (a new world's): whole
      ++t.whole;
      continue;
    }
    const EntriesCut c = entries_cut((fs::path(root) / rel).string(), own);
    if (c.what == EntriesCut::kCut) ++t.archives, t.entries += c.changed + c.added;
    else if (c.what == EntriesCut::kSame) ++t.same;
    else ++t.whole;
  }
  return t;
}

int entries_merged_clear(const std::string& write_dir, const std::string& keep) {
  std::error_code ec;
  const fs::path base = fs::path(write_dir) / kModMergedFolder;
  std::vector<fs::path> dirs;
  if (!write_dir.empty() && fs::is_directory(base, ec) && !path_is_link(base.string()))
    for (fs::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory(ec) && !it->is_symlink(ec) && it->path().filename().string() != keep) dirs.push_back(it->path());
  int gone = 0;
  for (const fs::path& d : dirs)
    if (remove_marked_tree(d.string(), kModMergedMarker)) ++gone;  // one without our marker is not ours: left
  return gone;
}

std::string entries_merge(const std::string& write_dir, const std::vector<std::string>& mods, const std::vector<std::string>& later,
                          std::vector<std::string>* log) {
  std::vector<std::string> none;
  std::vector<std::string>& said = log ? *log : none;
  if (write_dir.empty()) return {};
  std::error_code ec;
  // Which archives any mounted mod holds as its changed entries, by the archive's path in lower case.
  std::map<std::string, std::string> wanted;  // lower -> as first spelt
  for (const std::string& m : mods)
    for (const std::string& rel : files_in_zbd(m))
      if (const std::string of = entries_archive_of(rel); !of.empty() && record_archive(of)) wanted.emplace(lower(of), of);
  const fs::path base = fs::path(write_dir) / kModMergedFolder;
  // The folders of earlier runs, ours by their marker: gone at a program's first mount (none is in use then).
  static bool cleared = false;
  const bool clear = !cleared;
  cleared = true;
  struct Layer {
    std::string file;
    bool entries;
  };
  std::map<std::string, std::vector<Layer>> plan;  // per archive, the lowest first
  uint64_t key = mix(1469598103934665603ull, "entries 1");
  for (const auto& [low, rel] : wanted) {
    std::vector<Layer> layers;
    for (const std::string& root : later)
      if (const std::string f = zutl::any_case(root, rel); !f.empty() && fs::is_regular_file(f, ec)) {
        layers.push_back({f, false});
        break;
      }
    const bool has_base = !layers.empty();
    for (auto m = mods.rbegin(); m != mods.rend(); ++m) {
      if (const std::string f = zutl::any_case(*m, rel); !f.empty() && fs::is_regular_file(f, ec)) layers.push_back({f, false});
      if (const std::string f = zutl::any_case(*m, rel + kEntriesSuffix); !f.empty() && fs::is_regular_file(f, ec)) layers.push_back({f, true});
    }
    // From the last whole archive up: what is under it never shows.
    size_t from = 0;
    for (size_t i = 0; i < layers.size(); ++i)
      if (!layers[i].entries) from = i;
    layers.erase(layers.begin(), layers.begin() + static_cast<std::ptrdiff_t>(from));
    if (layers.empty() || layers.front().entries) {
      said.push_back("mods: " + rel + ": no game's archive to lay its changed entries over (" + (has_base ? "" : "none in the install") + "), left out");
      continue;
    }
    if (layers.size() == 1) continue;  // a whole archive wins it: the game reads that one as it is
    key = mix(key, low);
    for (const Layer& l : layers) key = mix_file(key, l.file);
    plan[rel] = layers;
  }
  char name[24];
  std::snprintf(name, sizeof name, "%016llx", static_cast<unsigned long long>(key));
  if (clear) entries_merged_clear(write_dir, plan.empty() ? std::string() : std::string(name));
  if (plan.empty()) return {};
  const fs::path out = base / name;
  const fs::path done = out / "complete.txt";
  if (fs::is_regular_file(done, ec) && fs::is_regular_file(out / kModMergedMarker, ec)) {
    said.push_back("mods: " + std::to_string(plan.size()) + (plan.size() == 1 ? " archive" : " archives") + " with the mods' changed entries, as built before");
    return out.string();
  }
  fs::create_directories(out, ec);
  { std::ofstream(out / kModMergedMarker) << "Archives the game built from the mounted mods' changed entries; made again when the mods change.\n"; }
  int built = 0;
  for (const auto& [rel, layers] : plan) {
    std::vector<uint8_t> bytes = bytes_of(layers.front().file);
    int laid = 0;
    bool ok = !bytes.empty();
    for (size_t i = 1; ok && i < layers.size(); ++i) {
      if (!layers[i].entries) {
        bytes = bytes_of(layers[i].file);
        continue;
      }
      std::vector<ArchiveEntry> entries;
      if (!archive_read_entries(layers[i].file, &entries)) {
        said.push_back("mods: " + layers[i].file + ": its entries do not read, left out");
        continue;
      }
      for (const ArchiveEntry& e : entries) {
        std::vector<uint8_t> next = ai::archive_with_entry(bytes, e.name.c_str(), e.data, true);
        if (next.empty()) {
          ok = false;
          break;
        }
        bytes.swap(next);
        ++laid;
      }
    }
    // Whole under another name first, then renamed: never a half-written archive where the game looks. One that
    // cannot be built is not there at all, so the game reads the first mod's whole archive or its own.
    const fs::path to = out / fs::path(rel), part = to.string() + ".part";
    fs::create_directories(to.parent_path(), ec);
    fs::remove(to, ec);  // one left by a build that stopped: ours, in our marked folder
    bool written = ok && write_bytes(part, bytes);
    if (written) {
      fs::rename(part, to, ec);
      written = !ec;
    }
    if (!written) {
      fs::remove(part, ec);
      said.push_back("mods: " + rel + ": " + (ok ? "could not be written into " + out.generic_string() : "the game's archive could not take the mods' entries (its table)") +
                     "; the mods' changed entries of it are not applied, the game's own is read");
      continue;
    }
    ++built;
    said.push_back("mods: " + rel + ": " + std::to_string(laid) + (laid == 1 ? " changed entry" : " changed entries") + " laid over the game's");
  }
  if (!built) return {};
  { std::ofstream(done) << "built\n"; }
  return out.string();
}

}  // namespace mech3
