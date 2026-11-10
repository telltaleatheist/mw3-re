#include "mech3/net_save.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "crt/msvc_string.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "upgrades/mods/campaign_entries.h"
#include "mech3/mods.h"

namespace mech3::ai {

namespace {
void put32(std::vector<uint8_t>& out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void put_int(std::vector<uint8_t>& out, int32_t v) { put32(out, zrdr::Int), put32(out, static_cast<uint32_t>(v)); }
void put_float(std::vector<uint8_t>& out, float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  put32(out, zrdr::Float), put32(out, u);
}
void put_string(std::vector<uint8_t>& out, const char* s) {
  const size_t n = s ? std::strlen(s) : 0;
  put32(out, zrdr::String), put32(out, static_cast<uint32_t>(n));
  out.insert(out.end(), s, s + n);
}
// A valve by name; none as an Int 0, which the reader takes for no name too.
void put_valve(std::vector<uint8_t>& out, const Valve* v) {
  if (v && v->name) put_string(out, v->name);
  else put_int(out, 0);
}
// A list of `items` items: its header (the count includes itself).
void put_list(std::vector<uint8_t>& out, int32_t items) { put32(out, zrdr::List), put32(out, static_cast<uint32_t>(items + 1)); }

uint32_t get32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }

// The end of the node at `at`, or 0 when it runs past `size` (or nests past 64).
size_t skip(const uint8_t* p, size_t size, size_t at, int depth = 0) {
  if (depth > 64 || at + 8 > size) return 0;
  const uint32_t type = get32(p + at), v = get32(p + at + 4);
  at += 8;
  if (type == zrdr::Int || type == zrdr::Float) return at;
  if (type == zrdr::String) return v <= size - at ? at + v : 0;
  if (type != zrdr::List || v < 1) return 0;
  for (uint32_t i = 1; i < v; ++i)
    if (!(at = skip(p, size, at, depth + 1))) return 0;
  return at;
}

const zrdr::Node* value_of(const zrdr::Node* tree, const char* key) {
  if (!tree || tree->type != zrdr::List || tree->list[0].i < 2 || tree->list[1].type != zrdr::List) return nullptr;
  const zrdr::Node& kv = tree->list[1];
  for (int32_t i = 1; i + 1 < kv.list[0].i; i += 2)
    if (kv.list[i].type == zrdr::String && std::strcmp(kv.list[i].s, key) == 0) return &kv.list[i + 1];
  return nullptr;
}
// The first item of a node's property `key`, as read_props finds it (anywhere
// in the value), when it is a string.
const char* prop_string(const zrdr::Node* was, const char* key) {
  if (!was) return nullptr;
  zrdr::Node* f = zrdr::find(const_cast<zrdr::Node*>(was), key);
  if (!f || f->type != zrdr::List || f->list[0].i < 2 || f->list[1].type != zrdr::String) return nullptr;
  return f->list[1].s;
}
}  // namespace

void put_node(std::vector<uint8_t>& out, const zrdr::Node& n) {
  switch (n.type) {
    case zrdr::Int: put_int(out, n.i); break;
    case zrdr::Float: put_float(out, n.f); break;
    case zrdr::String: put_string(out, n.s); break;
    case zrdr::List:
      put_list(out, n.list[0].i - 1);
      for (int32_t i = 1; i < n.list[0].i; ++i) put_node(out, n.list[i]);
      break;
    default: break;
  }
}

void put_record(std::vector<uint8_t>& out, const NetNodeRec& r, const zrdr::Node* was) {
  static const NetNodeRec d;
  std::vector<uint8_t> props;
  int32_t keys = 0;  // key and value items
  const auto key = [&](const char* k, int32_t items) {
    put_string(props, k), put_list(props, items), keys += 2;
  };
  // The name as written: the read one while it still localizes to the
  // record's, else the record's own text. auxname when it differs from it.
  const char* written = prop_string(was, "name");
  const char* name = r.name;
  if (name && written) {
    const char* shown = net_localize(written);
    if (shown && std::strcmp(shown, r.name) == 0) name = written;
  }
  if (name) key("name", 1), put_string(props, name);
  const char* aux = r.auxname;
  if (aux && (!name || std::strcmp(aux, name) != 0)) key("auxname", 1), put_string(props, aux);
  const auto same = [](float a, float b) { return !(a < b) && !(a > b); };
  if (r.speed_on != d.speed_on || !same(r.speed, d.speed)) key("speed", 2), put_int(props, r.speed_on), put_float(props, r.speed);
  if (r.wait_interval_on != d.wait_interval_on || !same(r.wait_interval, d.wait_interval))
    key("wait_interval", 2), put_int(props, r.wait_interval_on), put_float(props, r.wait_interval);
  if (r.wait_time_on != d.wait_time_on || !same(r.wait_time, d.wait_time))
    key("wait_time", 2), put_int(props, r.wait_time_on), put_float(props, r.wait_time);
  if (r.valve || r.valve_on != d.valve_on || r.valve_has_value != d.valve_has_value ||
      r.valve_value != d.valve_value) {
    key("valve", r.valve_has_value ? 3 : 2), put_int(props, r.valve_on), put_valve(props, r.valve);
    if (r.valve_has_value) put_int(props, r.valve_value);
  }
  if (r.canleave[0] != d.canleave[0] || r.canleave[1] != d.canleave[1])
    key("canleave", 2), put_int(props, r.canleave[0]), put_int(props, r.canleave[1]);
  if (r.jump_on != d.jump_on || !same(r.jump, d.jump)) key("jump", 2), put_int(props, r.jump_on), put_float(props, r.jump);
  const auto pair = [&](const char* k, const int32_t v[2], const int32_t dv[2]) {
    if (v[0] != dv[0] || v[1] != dv[1]) key(k, 2), put_int(props, v[0]), put_int(props, v[1]);
  };
  pair("scan_aim", r.scan_aim, d.scan_aim);
  pair("scan_time", r.scan_time, d.scan_time);
  pair("scan_hit", r.scan_hit, d.scan_hit);
  const auto flag = [&](const char* k, uint8_t set, uint8_t v) {
    if (set) key(k, 1), put_int(props, v != 0);
  };
  flag("hidden", r.hidden_set, r.hidden);
  flag("fixed", r.fixed_set, r.fixed);
  flag("autonext", r.autonext_set, r.autonext);
  flag("visible", r.visible_set, r.visible);
  if (r.deploy_set) {
    key("deploy", 5), put_valve(props, r.deploy_valve_a), put_int(props, r.deploy_a);
    put_valve(props, r.deploy_valve_b), put_int(props, r.deploy_b), put_int(props, r.deploy_c);
  }
  if (r.mcbwait_set)
    key("mcbwait", 3), put_valve(props, r.mcbwait_valve), put_int(props, r.mcbwait_b), put_float(props, r.mcbwait_time);
  if (r.arrange_set) key("arrangenodes", 1), put_float(props, r.arrange);
  if (const char* s = r.sound ? prop_string(was, "sound") : nullptr) key("sound", 1), put_string(props, s);
  if (r.backup) key("backup", 0);
  if (r.gate) key("gate", 1), put_string(props, r.gate);
  if (r.shunt) key("shunt", 0);
  if (r.valve_assign) key("valve_assign", 2), put_valve(props, r.valve_assign->valve), put_int(props, r.valve_assign->value);
  // Pirate's Moon's keys (its reader at 0x004ec92b): via ( name number radius ),
  // the radius kept squared, so the one read is written back while it still
  // squares to the record's; a missing name as an Int 0 (none to the reader).
  if (r.via || r.via_number != d.via_number || !same(r.via_radius2, d.via_radius2)) {
    float radius = std::sqrt(r.via_radius2);
    if (was) {
      zrdr::Node* f = zrdr::find(const_cast<zrdr::Node*>(was), "via");
      float v;
      if (f && f->type == zrdr::List && f->list[0].i > 3 && zrdr::get_float_value(&f->list[3], &v) &&
          v * v == r.via_radius2)
        radius = v;
    }
    key("via", 3);
    if (r.via) put_string(props, r.via);
    else put_int(props, 0);
    put_int(props, r.via_number), put_float(props, radius);
  }
  if (r.terminus) key("terminus", 0);
  if (r.static_mfb_set) key("static_mfb", 1), put_int(props, r.static_mfb);
  if (r.valve_union) {  // last: read_props finds keys anywhere in the value
    const int32_t n = static_cast<int32_t>(r.valve_union->valves.size());
    key("valveunion", n ? n : 1);
    for (const ValveCount& v : r.valve_union->valves)
      put_list(props, 3), put_int(props, 1), put_valve(props, v.valve), put_int(props, v.count);
    if (!n) put_list(props, 1), put_int(props, 0);  // an empty union: one item read past (its first is 0)
  }
  if (r.kind == 1) {
    put_list(out, 1 + keys);
  } else {
    put_list(out, 3 + keys);
    put_int(out, 12);
    put_list(out, 3);
    for (float p : r.pos) put_float(out, p);
  }
  put_list(out, static_cast<int32_t>(r.links.size()));
  for (int32_t l : r.links) put_int(out, l);
  out.insert(out.end(), props.begin(), props.end());
}

std::vector<uint8_t> net_with_edits(const uint8_t* p, size_t size, const zrdr::Node* tree,
                                    const std::vector<std::pair<std::string, const NetNodeRec*>>& edits) {
  // The top list: [count, the key list, anything else].
  if (size < 8 || get32(p) != zrdr::List || get32(p + 4) < 2 || 16 > size) return {};
  const uint32_t top = get32(p + 4);
  size_t at = 8;
  if (get32(p + at) != zrdr::List) return {};
  const uint32_t pairs_count = get32(p + at + 4);
  size_t item = at + 8;
  struct Span {
    size_t key_start, value_start, value_end;
    std::string key;
  };
  std::vector<Span> spans;
  for (uint32_t i = 1; i < pairs_count;) {
    const size_t key_end = skip(p, size, item);
    if (!key_end) return {};
    std::string key;
    if (get32(p + item) == zrdr::String) key.assign(reinterpret_cast<const char*>(p + item + 8), key_end - item - 8);
    if (i + 1 >= pairs_count) {  // a key without a value: kept as it is
      spans.push_back({item, key_end, key_end, std::string()});
      item = key_end, ++i;
      break;
    }
    const size_t value_end = skip(p, size, key_end);
    if (!value_end) return {};
    spans.push_back({item, key_end, value_end, key});
    item = value_end, i += 2;
  }
  const size_t list_end = item;
  const size_t top_end = [&] {
    size_t e = list_end;
    for (uint32_t i = 2; i < top && e; ++i) e = skip(p, size, e);
    return e;
  }();
  if (!top_end) return {};
  std::vector<uint8_t> out;
  std::vector<bool> used(edits.size());
  std::vector<uint8_t> body;
  uint32_t count = pairs_count;
  for (const Span& s : spans) {
    size_t k = 0;
    for (; k < edits.size(); ++k)
      if (!used[k] && !s.key.empty() && edits[k].first == s.key) break;
    if (k < edits.size() && !edits[k].second) {  // the node gone: its key and value left out
      used[k] = true;
      count -= 2;
      continue;
    }
    body.insert(body.end(), p + s.key_start, p + s.value_start);
    if (k < edits.size()) {
      used[k] = true;
      put_record(body, *edits[k].second, value_of(tree, s.key.c_str()));
    } else {
      body.insert(body.end(), p + s.value_start, p + s.value_end);
    }
  }
  for (size_t k = 0; k < edits.size(); ++k) {
    if (used[k] || !edits[k].second) continue;
    put_string(body, edits[k].first.c_str());
    put_record(body, *edits[k].second, nullptr);
    count += 2;
  }
  put32(out, zrdr::List), put32(out, top);
  put32(out, zrdr::List), put32(out, count);
  out.insert(out.end(), body.begin(), body.end());
  out.insert(out.end(), p + list_end, p + top_end);
  out.insert(out.end(), p + top_end, p + size);  // whatever followed the tree
  return out;
}

std::vector<uint8_t> archive_with_entry(const std::vector<uint8_t>& a, const char* name,
                                        const std::vector<uint8_t>& data, bool add) {
  const size_t n = a.size();
  // Pirate's Moon's version 2 (pm/gamez/zutil/zutl_zar.h): version, count and a
  // CRC after the table.
  const bool v2 = n >= 12 && get32(a.data() + n - 12) == 2;
  const size_t tail = v2 ? 12 : 8;
  if (n < 8 || (!v2 && get32(a.data() + n - 8) != 1)) return {};
  uint32_t count = get32(a.data() + n - tail + 4);
  const size_t table_size = static_cast<size_t>(count) * sizeof(zutl_zar::Entry);
  if (table_size > n - tail) return {};
  const size_t table = n - tail - table_size;
  std::vector<zutl_zar::Entry> entries(count);
  if (count) std::memcpy(entries.data(), a.data() + table, table_size);
  int index = -1;
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry& e = entries[i];
    if (e.start > table || e.length > table - e.start) return {};
    char n[sizeof e.name + 1] = {};
    std::memcpy(n, e.name, sizeof e.name);
    if (index < 0 && crt::msvc_stricmp(name, n) == 0) index = static_cast<int>(i);  // as zutl_zar::find
  }
  if (index < 0 && !add) return {};
  if (index < 0) {
    zutl_zar::Entry e{};
    std::strncpy(e.name, name, sizeof e.name - 1);
    entries.push_back(e);
    index = static_cast<int>(count++);
  }
  const size_t new_table_size = static_cast<size_t>(count) * sizeof(zutl_zar::Entry);
  std::vector<uint8_t> out;
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry& e = entries[i];
    const uint32_t start = static_cast<uint32_t>(out.size());
    if (static_cast<int>(i) == index) out.insert(out.end(), data.begin(), data.end());
    else out.insert(out.end(), a.begin() + e.start, a.begin() + e.start + e.length);
    e.length = static_cast<uint32_t>(out.size()) - start;
    e.start = start;
  }
  const size_t at = out.size();
  out.resize(at + new_table_size);
  if (count) std::memcpy(out.data() + at, entries.data(), new_table_size);
  if (!v2) return put32(out, 1), put32(out, count), out;
  // The CRC of every entry's data in table order: made anew when the source's
  // was right (the readers', which the original checks on reader.zbd), else
  // kept (the 0 of the rest).
  const auto crc_of = [&](const std::vector<uint8_t>& b, const std::vector<zutl_zar::Entry>& es) {
    uint32_t c = 0;
    for (const zutl_zar::Entry& e : es) c = zutl_zar::crc32(c, b.data() + e.start, e.length);
    return c;
  };
  uint32_t crc = get32(a.data() + n - 4);
  std::vector<zutl_zar::Entry> old(table_size / sizeof(zutl_zar::Entry));
  if (!old.empty()) std::memcpy(old.data(), a.data() + table, table_size);
  if (crc == crc_of(a, old)) crc = crc_of(out, entries);
  return put32(out, 2), put32(out, count), put32(out, crc), out;
}

std::vector<uint8_t> net_new_entry(const char* name) {
  std::vector<uint8_t> out;
  put_list(out, 1);
  put_list(out, 4);
  put_string(out, "version"), put_list(out, 1), put_int(out, 0x6a);
  put_string(out, "name"), put_list(out, 1), put_string(out, name);
  return out;
}

namespace {
namespace fs = std::filesystem;

std::string read_text(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// The mod folder (under write_dir's mods) a host path lies in, "" when none.
std::string mod_folder_of(const std::string& write_dir, const std::string& host) {
  std::error_code ec;
  const fs::path mods = fs::weakly_canonical(fs::path(write_dir) / "mods", ec);
  const fs::path p = fs::weakly_canonical(fs::path(host), ec);
  auto m = mods.begin();
  auto q = p.begin();
  for (; m != mods.end(); ++m, ++q)
    if (q == p.end() || *m != *q) return {};
  return q == p.end() ? std::string() : q->string();
}

// A folder name from a mod's name: what Windows refuses in a name dropped.
std::string folder_name(const std::string& name) {
  std::string out;
  for (char c : name)
    if (!std::strchr("\\/:*?\"<>|", c) && static_cast<unsigned char>(c) >= 0x20) out += c;
  while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
  return out.empty() ? std::string("editor") : out;
}
}  // namespace

std::string editor_mod(const std::string& write_dir, const std::string& source, std::string* error) {
  const fs::path mods = fs::path(write_dir) / "mods";
  std::error_code ec;
  if (!source.empty() && fs::is_regular_file(mods / source / kEditorMarker, ec)) return source;
  std::string name = "editor", game = "mw3 1.2";
  if (!source.empty()) {
    ModInfo info;
    mod_parse(read_text(mods / source / "mod.txt"), &info);
    name = info.name.empty() ? source : info.name;
    if (!info.game.empty()) game = info.game;
  }
  const std::string base = folder_name(name + " edits");
  for (int k = 1; k < 1000; ++k) {
    const std::string folder = k == 1 ? base : base + " (" + std::to_string(k) + ")";
    const fs::path dir = mods / folder;
    if (fs::exists(dir, ec)) {
      if (fs::is_regular_file(dir / kEditorMarker, ec)) return folder;
      continue;  // someone else's
    }
    fs::create_directories(dir, ec);
    std::ofstream marker(dir / kEditorMarker, std::ios::binary);
    marker << "Made by the game editor's Save Changes: it saves over its own files here (docs/editor.md).\n";
    std::ofstream txt(dir / "mod.txt", std::ios::binary);
    txt << "name=" << name << " edits\nfor=" << game << "\n";
    if (!marker || !txt) break;
    return folder;
  }
  if (error) *error = "no folder for the edits under " + mods.string();
  return {};
}

std::string& net_save_into_mod() {
  static std::string mod;
  return mod;
}

NetSave save_archive_edit(const char* rel, const char* name, const std::vector<uint8_t>& data, bool add) {
  NetSave r;
  const std::string write_dir = zutl::mounts().write;
  if (write_dir.empty()) return r.error = "no write folder", r;
  const std::string from = zutl::find(rel);
  if (from.empty()) return r.error = std::string(rel) + " is not found", r;
  const std::string source = mod_folder_of(write_dir, from);
  const std::string& into = net_save_into_mod();
  r.folder = into.empty() ? editor_mod(write_dir, source, &r.error) : into;
  if (r.folder.empty()) return r;
  std::string host = rel;
  for (char& c : host) c = c == '\\' ? '/' : c;
  const fs::path to = fs::path(write_dir) / "mods" / r.folder / host;
  std::error_code ec;
  // The save goes on from the last one this folder holds (its other entries
  // as an earlier save left them: an added vehicle kept by a net's save),
  // else from the archive the game reads.
  const std::string base = fs::exists(to, ec) ? to.string() : from;
  const std::vector<uint8_t> bytes = [&] {
    std::ifstream in(base, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }();
  const std::vector<uint8_t> out = archive_with_entry(bytes, name, data, add);
  if (out.empty()) return r.error = base + " does not hold " + name + " as an archive the game reads", r;
  fs::create_directories(to.parent_path(), ec);
  const fs::path tmp = to.string() + ".saving";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!f) return fs::remove(tmp, ec), r.error = "could not write " + tmp.string(), r;
  }
  if (fs::exists(to, ec)) fs::rename(to, to.string() + ".bak", ec);  // the previous save, one kept
  fs::rename(tmp, to, ec);
  if (ec) return fs::remove(tmp, ec), r.error = "could not write " + to.string(), r;
  r.path = to.string();
  // On, just before its source (the install's: after every mod), so it wins.
  std::vector<std::string> on = entry_mods_on(write_dir);  // the run's entry's ticks (campaign_entries.h)
  if (!into.empty()) {  // the mod itself: on where it stands
    if (std::find(on.begin(), on.end(), r.folder) == on.end()) on.push_back(r.folder);
  } else if (source != r.folder || std::find(on.begin(), on.end(), r.folder) == on.end()) {  // its own copy: where it is
    on.erase(std::remove(on.begin(), on.end(), r.folder), on.end());
    on.insert(source.empty() || source == r.folder ? on.end() : std::find(on.begin(), on.end(), source), r.folder);
  }
  if (!entry_mods_save(write_dir, on)) return r.error = "saved, but its campaign's mods.txt could not be written to turn it on", r;
  r.ok = true;
  r.message = "Saved to mods\\" + r.folder + "\\" + rel + (into.empty() ? ": " + entry_on_words(write_dir) : ": in the mod, " + entry_on_words(write_dir));
  return r;
}

}  // namespace mech3::ai
