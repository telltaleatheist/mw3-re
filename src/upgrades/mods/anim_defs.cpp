#include "upgrades/mods/anim_defs.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <cstdio>
#include <set>

#include "gamez/zclass/cls_mechlib.h"
#include "gamez/zclass/cls_mechlib_json.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zeffect/zeff_anim_json.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zutil/zutl_zip.h"
#include "gamez/zutil/zutl_json.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mods.h"

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
bool ends(const std::string& s, const char* tail) {
  const size_t n = std::strlen(tail);
  return s.size() > n && s.compare(s.size() - n, n, tail) == 0;
}
// A def file's name (lower case): a .json that is no script (si-script-*) and no node file (*.flt.json).
bool is_def_name(const std::string& n) { return ends(n, ".json") && n.rfind("si-script-", 0) != 0 && !ends(n, ".flt.json"); }

// A JSON value as text again (the node file's materials list for the materials reader).
std::string json_text(const zutl_json::Value& v) {
  using V = zutl_json::Value;
  const auto quoted = [](const std::string& t) {
    std::string o = "\"";
    for (char c : t) {
      if (c == '"' || c == '\\') o += '\\';
      if (static_cast<unsigned char>(c) < 0x20) {
        char b[8];
        std::snprintf(b, sizeof b, "\\u%04x", c);
        o += b;
      } else {
        o += c;
      }
    }
    return o + "\"";
  };
  switch (v.kind) {
    case V::Null: return "null";
    case V::Bool: return v.b ? "true" : "false";
    case V::Number: return v.text;
    case V::String: return quoted(v.text);
    case V::Array: {
      std::string o = "[";
      for (size_t i = 0; i < v.items.size(); ++i) o += (i ? "," : "") + json_text(v.items[i]);
      return o + "]";
    }
    case V::Object: {
      std::string o = "{";
      for (size_t i = 0; i < v.members.size(); ++i) o += (i ? "," : "") + quoted(v.members[i].first) + ":" + json_text(v.members[i].second);
      return o + "}";
    }
  }
  return "null";
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
    if (!mod_defs_only(m) && (!zutl::find_in_mod(m, dir + stem + ".zbd").empty() || !zutl::find_in_mod(m, dir + stem + "/metadata.json").empty() ||
        !zutl::find_in_mod(m, dir + stem + ".zip").empty()))
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
        if (e.is_regular_file(ec) && is_def_name(n)) files.push_back(e.path());
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
  if (!is_def_name(name)) return false;
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

bool mod_defs_only(size_t m) {
  const zutl::Mounts& mt = zutl::mounts();
  if (m >= mt.mods.size()) return false;
  ModInfo info;
  const std::string text = text_of(fs::path(mt.mods[m]) / "mod.txt");  // small, and read at a world's load only
  return !text.empty() && (mod_parse(text, &info), info.anim_defs_only);
}

std::string whole_file_past_defs_only(const char* game_path) {
  const zutl::Mounts& mt = zutl::mounts();
  if (!game_path || mt.mods.empty()) return {};
  const std::string rel = zutl::host(game_path);
  bool blocked = false;
  for (size_t m = 0; m < mt.mods.size(); ++m) {
    const std::string at = zutl::find_in_mod(m, rel);
    if (at.empty()) continue;
    if (!mod_defs_only(m)) return blocked ? at : std::string();
    blocked = true;
  }
  if (!blocked) return {};
  for (const std::string& r : mt.read) {  // the install's (a read mount that is no mod)
    if (std::find(mt.mods.begin(), mt.mods.end(), r) != mt.mods.end()) continue;
    std::error_code ec;
    const std::string p = zutl::any_case(r, rel);
    if (!p.empty() && fs::is_regular_file(p, ec)) return p;
  }
  return {};
}

int build_for_original(const std::string& mod_dir, const std::string& install, std::vector<std::string>* lines) {
  int errors = 0;
  std::error_code ec;
  const fs::path zbd = fs::path(install) / "zbd";
  std::vector<std::string> worlds;
  for (const auto& e : fs::directory_iterator(zbd, ec))
    if (e.is_directory(ec) && fs::is_regular_file(e.path() / "anim.zbd", ec)) worlds.push_back(e.path().filename().string());
  std::sort(worlds.begin(), worlds.end());
  for (const std::string& w : worlds) {
    std::vector<DefFile> defs;
    std::set<std::string> keys;
    for (const fs::path& folder : {fs::path(mod_dir) / "zbd" / w / "anim", fs::path(mod_dir) / "zbd" / "anim"}) {
      if (!fs::is_directory(folder, ec) || fs::exists(folder / "metadata.json", ec)) continue;
      std::vector<fs::path> files;
      for (const auto& e : fs::directory_iterator(folder, ec)) {
        const std::string n = lower(e.path().filename().string());
        if (e.is_regular_file(ec) && is_def_name(n)) files.push_back(e.path());
      }
      std::sort(files.begin(), files.end());
      for (const fs::path& f : files) {
        DefFile d;
        std::string why;
        d.from = f.string();
        const auto beside = [&](const std::string& n) { return text_of(folder / n); };
        if (!zeff::anim_def_from_json(text_of(f), beside, &d.bytes, &d.key, &why)) {
          lines->push_back("error: " + d.from + ": " + why + " (left out)");
          ++errors;
        } else if (keys.insert(d.key).second) {
          defs.push_back(std::move(d));
        }
      }
    }
    for (const fs::path& folder : {fs::path(mod_dir) / "zbd" / w / "anim", fs::path(mod_dir) / "zbd" / "anim"})
      for (const auto& e : fs::directory_iterator(folder, ec))
        if (ends(lower(e.path().filename().string()), ".flt.json"))
          lines->push_back(w + ": " + e.path().filename().string() + " is a node file: the original reads nodes only from the "
                           "world's gamez.zbd, so its defs need that node there");
    if (defs.empty()) continue;
    std::ifstream in(zbd / w / "anim.zbd", std::ios::binary);
    const std::vector<uint8_t> base((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string why;
    const std::vector<uint8_t> out = with_defs(base, defs, &why);
    if (out.empty()) {
      lines->push_back("error: " + w + ": " + why);
      ++errors;
      continue;
    }
    const fs::path to = fs::path(mod_dir) / "original" / "zbd" / w / "anim.zbd";
    fs::create_directories(to.parent_path(), ec);
    if (fs::exists(to, ec)) fs::copy_file(to, to.string() + ".bak", fs::copy_options::overwrite_existing, ec);
    std::ofstream(to, std::ios::binary | std::ios::trunc).write(reinterpret_cast<const char*>(out.data()),
                                                                static_cast<std::streamsize>(out.size()));
    std::vector<DefSpan> was, now;
    def_spans(base.data(), base.size(), &was), def_spans(out.data(), out.size(), &now);
    lines->push_back(w + ": " + std::to_string(defs.size()) + " def files, " + std::to_string(now.size() - was.size()) +
                     " added, written to " + to.generic_string());
  }
  return errors;
}

bool is_node_file(const std::string& rel) {
  const std::string low = lower(rel);
  if (!ends(low, ".flt.json") || low.rfind("zbd/", 0) != 0) return false;
  const size_t a = low.find("/anim/");
  if (a == std::string::npos || low.find('/', a + 6) != std::string::npos) return false;  // directly in an anim folder
  const std::string mid = low.substr(4, a - 4);  // "" (zbd/anim/) or a world
  return a == 3 || mid.find('/') == std::string::npos;
}

bool node_file_reads(const std::string& path, std::string* why) {
  const std::string text = text_of(path);
  zutl_json::Value doc;
  if (!zutl_json::parse(text, &doc, why)) return false;
  const zutl_json::Value* mats = doc.get("materials");
  if (!mats || mats->kind != zutl_json::Value::Array) return *why = "no materials list (its own, in mechlib's materials.json form)", false;
  std::vector<uint8_t> bytes;
  if (!zcls::mechlib_materials_from_json(json_text(*mats), &bytes, why)) return *why = "materials: " + *why, false;
  return zcls::mechlib_model_from_json(text, &bytes, why);
}

zcls::Node* load_node_file(const std::string& path, std::string* why) {
  const std::string text = text_of(path);
  zutl_json::Value doc;
  if (!zutl_json::parse(text, &doc, why)) return nullptr;
  const zutl_json::Value* mats = doc.get("materials");
  if (!mats || mats->kind != zutl_json::Value::Array) return *why = "no materials list (its own, in mechlib's materials.json form)", nullptr;
  std::vector<uint8_t> mat_bytes, tree_bytes;
  if (!zcls::mechlib_materials_from_json(json_text(*mats), &mat_bytes, why)) return *why = "materials: " + *why, nullptr;
  if (!zcls::mechlib_model_from_json(text, &tree_bytes, why)) return nullptr;
  zcls::MechLib lib;  // the node file's own library: its materials, its one tree
  {
    zutl_zip::ScratchFile f(mat_bytes.data(), mat_bytes.size());
    uint32_t count = 0;
    if (!f.file || std::fread(&count, 4, 1, f.file) != 1 || static_cast<uint64_t>(count) * 0x28 + 4 > mat_bytes.size())
      return *why = "its materials do not read", nullptr;
    for (uint32_t i = 0; i < count; ++i) {
      auto* m = new zcls::Material{};
      zcls::read_material(f.file, *m);
      lib.materials.push_back(m);
    }
  }
  zcls::Node* root = nullptr;
  {
    zutl_zip::ScratchFile f(tree_bytes.data(), tree_bytes.size());
    if (f.file) root = zcls::read_tree(lib.reader, f.file, nullptr);
  }
  // Every polygon's material within its own list (mechlib's own check).
  bool fit = root != nullptr;
  std::vector<zcls::Node*> stack;
  if (root) stack.push_back(root);
  while (fit && !stack.empty()) {
    zcls::Node* n = stack.back();
    stack.pop_back();
    if (zmod::Model* m = n->model)
      for (uint32_t i = 0; i < m->count(0x10) && fit; ++i) fit = m->polygons[i].material_index < lib.materials.size();
    for (int32_t i = 0; i < n->child_count(); ++i) stack.push_back(n->children[i]);
  }
  if (fit) zcls::mechlib_resolve(lib, root);
  for (zcls::Material* m : lib.materials) zcls::material_free(*m), delete m;
  lib.materials.clear();
  if (!root) return *why = "its tree does not read", nullptr;
  if (!fit) return *why = "a polygon names a material its list lacks", nullptr;  // the tree stays, unused
  return root;
}

void load_mod_node_files(const char* game_path, std::vector<std::string>* notes) {
  const zutl::Mounts& mt = zutl::mounts();
  if (mt.mods.empty() || !game_path) return;
  std::string dir = zutl::host(game_path);
  const size_t slash = dir.rfind('/');
  std::string stem = slash == std::string::npos ? dir : dir.substr(slash + 1);
  dir = slash == std::string::npos ? std::string{} : dir.substr(0, slash + 1);
  if (const size_t dot = stem.rfind('.'); dot != std::string::npos) stem.resize(dot);
  std::string up = dir.empty() ? std::string{} : dir.substr(0, dir.size() - 1);
  const size_t s2 = up.rfind('/');
  up = s2 == std::string::npos ? std::string{} : up.substr(0, s2 + 1);
  std::set<std::string> done;
  std::error_code ec;
  for (size_t m = 0; m < mt.mods.size(); ++m)
    for (const std::string& folder : {dir + stem + "/", up + stem + "/"}) {
      const fs::path at = fs::path(mt.mods[m]) / folder;
      if (!fs::is_directory(at, ec)) continue;
      std::vector<fs::path> files;
      for (const auto& e : fs::directory_iterator(at, ec))
        if (e.is_regular_file(ec) && ends(lower(e.path().filename().string()), ".flt.json")) files.push_back(e.path());
      std::sort(files.begin(), files.end());
      for (const fs::path& f : files) {
        const std::string fname = f.filename().string();
        const std::string node = fname.substr(0, fname.size() - 5);  // "<node>.flt"
        if (!done.insert(lower(node)).second) continue;
        if (zcls::list_find_by_name(zcls::kAllNodes, node.c_str())) {
          if (notes) notes->push_back(f.string() + ": the world has a " + node + " already; its own kept");
          continue;
        }
        std::string why;
        zcls::Node* root = load_node_file(f.string(), &why);
        if (!root) {
          if (notes) notes->push_back(f.string() + ": " + why + "; left out");
          continue;
        }
        zcls::node_set_name(root, node.c_str());
        if (notes) notes->push_back(f.string() + ": " + node + " read");
      }
    }
}

bool def_nodes_found(const std::vector<uint8_t>& def, std::string* missing) {
  if (def.size() < 0x13c) return *missing = "(a short def)", false;
  const uint8_t* r = def.data();
  // Its own lights' and sounds' names, which its events find before the world's.
  std::set<std::string> own;
  static const size_t kList[9] = {0x60, 0x28, 0x2c, 0x2c, 0x2c, 0x24, 0x24, 0x30, 0x48};
  std::vector<std::pair<size_t, size_t>> lists;  // each list's start and count
  size_t q = 0x13c;
  for (int k = 0; k < 9; ++k) {
    const size_t n = k < 8 ? r[0x109 + k] : r[0x112];
    lists.emplace_back(q, n);
    q += n * kList[k];
  }
  if (q > def.size()) return *missing = "(a short def)", false;
  for (int k : {2, 4})  // lights, dynamic sounds: entry 0 a placeholder
    for (size_t i = 1; i < lists[k].second; ++i) own.insert(name_at(r + lists[k].first + i * kList[k]));
  const auto found = [&](const std::string& n) {
    return n.empty() || own.count(n) || zcls::list_find_by_name(zcls::kAllNodes, n.c_str()) != nullptr;
  };
  std::vector<std::string> names = {name_at(r + 0x20), name_at(r + 0x44)};
  for (int k : {0, 1})  // object and node refs, from 1
    for (size_t i = 1; i < lists[k].second; ++i) names.push_back(name_at(r + lists[k].first + i * kList[k]));
  for (const std::string& n : names)
    if (!found(n)) return *missing = n, false;
  return true;
}

}  // namespace mech3::anim_defs
