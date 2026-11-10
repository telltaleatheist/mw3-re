#include "upgrades/mods/anim_defs.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

#include "gamez/zeffect/zeff_anim_json.h"
#include "gamez/zutil/zutl_json.h"
#include "gamez/zutil/zutl_path.h"

namespace mech3::anim_defs {

namespace {
namespace fs = std::filesystem;

uint32_t u32(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  return v;
}
int32_t i32(const uint8_t* p) { return static_cast<int32_t>(u32(p)); }
std::string name_at(const uint8_t* p) {  // a 0x20-byte name up to its NUL
  return std::string(reinterpret_cast<const char*>(p), strnlen(reinterpret_cast<const char*>(p), 0x20));
}
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string text_of(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// The file's parts (docs/anim.md, 0x50a0e0): the header (0x0c), the source
// list (its count x 0x54), the global block (0x44: the def count, s16, at
// +0x0a), the defs, then what follows them (the messages, none shipped).
struct Layout {
  size_t block = 0, defs = 0, end = 0;  // end: past the last def
  std::vector<DefSpan> spans;
};
bool layout(const uint8_t* p, size_t n, Layout* l) {
  if (n < 0x0c || u32(p) != 0x08170616 || u32(p + 4) != 39) return false;
  const uint64_t sources = u32(p + 8);
  l->block = 0x0c + sources * 0x54;
  if (l->block + 0x44 > n) return false;
  int16_t count;
  std::memcpy(&count, p + l->block + 0x0a, 2);
  if (count < 0) return false;
  size_t at = l->defs = l->block + 0x44;
  static const size_t kList[9] = {0x60, 0x28, 0x2c, 0x2c, 0x2c, 0x24, 0x24, 0x30, 0x48};
  for (int d = 0; d < count; ++d) {
    DefSpan s;
    s.at = at;
    if (at + 0x13c > n) return false;
    const uint8_t* r = p + at;
    size_t q = at + 0x13c;
    for (int k = 0; k < 9; ++k) q += (k < 8 ? r[0x109 + k] : r[0x112]) * kList[k];
    const auto seq = [&]() {  // a seq def (0x40) and its events
      if (q + 0x40 > n) return false;
      const int32_t size = i32(p + q + 0x3c);
      q += 0x40;
      if (size > 0) q += static_cast<size_t>(size);
      return q <= n;
    };
    if (!seq()) return false;  // the reset sequence
    for (int k = 0; k < r[0x108]; ++k)
      if (!seq()) return false;
    s.size = q - at;
    if (d > 0) s.key = zeff::anim_def_key(name_at(r + 0x20), name_at(r), name_at(r + 0x44));
    l->spans.push_back(s);
    at = q;
  }
  l->end = at;
  return true;
}
}  // namespace

bool def_spans(const uint8_t* p, size_t n, std::vector<DefSpan>* out) {
  Layout l;
  if (!layout(p, n, &l)) return false;
  *out = std::move(l.spans);
  return true;
}

std::vector<uint8_t> with_defs(const std::vector<uint8_t>& base, const std::vector<DefFile>& defs, std::string* why) {
  Layout l;
  if (!layout(base.data(), base.size(), &l)) return *why = "the anim.zbd does not read", std::vector<uint8_t>{};
  std::map<std::string, const DefFile*> by_key;
  for (const DefFile& d : defs) by_key.emplace(d.key, &d);
  std::vector<uint8_t> out(base.begin(), base.begin() + static_cast<std::ptrdiff_t>(l.defs));
  std::set<std::string> used;
  for (const DefSpan& s : l.spans) {
    auto hit = s.key.empty() ? by_key.end() : by_key.find(s.key);
    if (hit != by_key.end() && used.insert(s.key).second) {
      out.insert(out.end(), hit->second->bytes.begin(), hit->second->bytes.end());
    } else {
      out.insert(out.end(), base.begin() + static_cast<std::ptrdiff_t>(s.at),
                 base.begin() + static_cast<std::ptrdiff_t>(s.at + s.size));
    }
  }
  size_t count = l.spans.size();
  for (const DefFile& d : defs)
    if (!used.count(d.key) && used.insert(d.key).second) out.insert(out.end(), d.bytes.begin(), d.bytes.end()), ++count;
  if (count > 0x7fff) return *why = "more than 32767 animations", std::vector<uint8_t>{};
  const int16_t c = static_cast<int16_t>(count);
  std::memcpy(out.data() + l.block + 0x0a, &c, 2);
  out.insert(out.end(), base.begin() + static_cast<std::ptrdiff_t>(l.end), base.end());
  return out;
}

std::vector<DefFile> mod_def_files(const char* game_path, const std::vector<uint8_t>& base, std::vector<std::string>* notes) {
  std::vector<DefFile> out;
  const zutl::Mounts& mt = zutl::mounts();
  if (mt.mods.empty() || !game_path || !game_path[0]) return out;
  std::string dir = zutl::host(game_path);  // zbd/c1/anim.zbd
  const size_t slash = dir.rfind('/');
  std::string stem = slash == std::string::npos ? dir : dir.substr(slash + 1);
  dir = slash == std::string::npos ? std::string{} : dir.substr(0, slash + 1);
  if (const size_t dot = stem.rfind('.'); dot != std::string::npos) stem.resize(dot);
  // The world-less folder: the world's folder's parent (zbd/anim/).
  std::string up = dir.empty() ? std::string{} : dir.substr(0, dir.size() - 1);
  const size_t s2 = up.rfind('/');
  up = s2 == std::string::npos ? std::string{} : up.substr(0, s2 + 1);
  // The base's mod: the first with a whole file for this world (its anim.zbd, JSON folder or zip).
  size_t base_mod = mt.mods.size();
  for (size_t m = 0; m < mt.mods.size() && base_mod == mt.mods.size(); ++m)
    if (!zutl::find_in_mod(m, dir + stem + ".zbd").empty() || !zutl::find_in_mod(m, dir + stem + "/metadata.json").empty() ||
        !zutl::find_in_mod(m, dir + stem + ".zip").empty())
      base_mod = m;
  std::set<std::string> base_keys;
  std::map<std::string, std::string> taken;  // key -> the file that has it
  std::vector<DefSpan> spans;
  if (def_spans(base.data(), base.size(), &spans))
    for (const DefSpan& s : spans) base_keys.insert(s.key);
  std::error_code ec;
  for (size_t m = 0; m < mt.mods.size(); ++m) {
    for (const std::string& folder : {dir + stem + "/", up + stem + "/"}) {
      const fs::path at = fs::path(mt.mods[m]) / folder;
      if (!fs::is_directory(at, ec)) continue;
      if (folder == dir + stem + "/" && fs::exists(at / "metadata.json", ec)) continue;  // the world's whole JSON
      std::vector<fs::path> files;
      for (const auto& e : fs::directory_iterator(at, ec)) {
        const std::string n = lower(e.path().filename().string());
        if (e.is_regular_file(ec) && n.size() > 5 && n.compare(n.size() - 5, 5, ".json") == 0 && n.rfind("si-script-", 0) != 0)
          files.push_back(e.path());
      }
      std::sort(files.begin(), files.end());
      for (const fs::path& f : files) {
        DefFile d;
        std::string why;
        d.from = f.string();
        const auto beside = [&](const std::string& n) { return text_of(at / n); };
        if (!zeff::anim_def_from_json(text_of(f), beside, &d.bytes, &d.key, &why)) {
          if (notes) notes->push_back(d.from + ": " + why + "; left out");
          continue;
        }
        if (auto t = taken.find(d.key); t != taken.end()) {
          if (notes) notes->push_back(d.from + ": " + d.key + " is " + t->second + "'s, which comes first; left out");
          continue;
        }
        if (m >= base_mod && base_keys.count(d.key)) {
          if (notes) notes->push_back(d.from + ": " + d.key + " is " + mt.mods[base_mod] + "'s whole " + stem + " file's, which comes first; left out");
          continue;
        }
        taken.emplace(d.key, d.from);
        out.push_back(std::move(d));
      }
    }
  }
  return out;
}

bool is_def_file(const std::string& mod_dir, const std::string& rel, std::string* world) {
  const std::string low = lower(rel);
  std::vector<std::string> parts;
  for (size_t at = 0; at <= low.size();) {
    size_t e = low.find('/', at);
    if (e == std::string::npos) e = low.size();
    parts.push_back(low.substr(at, e - at));
    at = e + 1;
  }
  const std::string& name = parts.back();
  if (name.size() <= 5 || name.compare(name.size() - 5, 5, ".json") != 0 || name.rfind("si-script-", 0) == 0) return false;
  if (parts.size() == 3 && parts[0] == "zbd" && parts[1] == "anim") return *world = "", true;
  if (parts.size() != 4 || parts[0] != "zbd" || parts[2] != "anim") return false;
  std::error_code ec;
  const fs::path folder = (fs::path(mod_dir) / rel).parent_path();
  if (fs::exists(folder / "metadata.json", ec) || name == "metadata.json") return false;  // the world's whole JSON
  *world = parts[1];
  return true;
}

std::string def_file_key(const std::string& path) {
  zutl_json::Value d;
  std::string why;
  if (!zutl_json::parse(text_of(path), &d, &why)) return {};
  const auto field = [&](const char* k) {
    const zutl_json::Value* v = d.get(k);
    return v && v->kind == zutl_json::Value::String ? v->text : std::string();
  };
  const std::string n = field("name"), a = field("anim_name");
  return n.empty() || a.empty() ? std::string() : zeff::anim_def_key(n, a, field("anim_root_name"));
}

}  // namespace mech3::anim_defs
