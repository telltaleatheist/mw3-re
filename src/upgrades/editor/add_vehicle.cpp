#include "upgrades/editor/add_vehicle.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "crt/msvc_string.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/game_editor_net.h"
#include "mech3/mech_names.h"
#include "mech3/net_save.h"
#include "mech3/player.h"
#include "mech3/zainet.h"
#include "mech3/zainetedit.h"
#include "mech3/zrdraiv.h"

namespace mech3 {

namespace {
// ---- zrd's binary form (docs/zreader.md) ----
uint32_t get32(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  return v;
}
void put32(std::vector<uint8_t>& o, uint32_t v) {
  const uint8_t* b = reinterpret_cast<const uint8_t*>(&v);
  o.insert(o.end(), b, b + 4);
}
void put_int(std::vector<uint8_t>& o, int32_t v) { put32(o, zrdr::Int), put32(o, static_cast<uint32_t>(v)); }
void put_float(std::vector<uint8_t>& o, float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  put32(o, zrdr::Float), put32(o, u);
}
void put_string(std::vector<uint8_t>& o, const std::string& s) {
  put32(o, zrdr::String), put32(o, static_cast<uint32_t>(s.size()));
  o.insert(o.end(), s.begin(), s.end());
}
// The end of the node at `at`, 0 when it runs past `size` or does not read.
size_t skip(const uint8_t* p, size_t size, size_t at, int depth = 0) {
  if (at + 8 > size || depth > 64) return 0;
  const uint32_t t = get32(p + at), v = get32(p + at + 4);
  if (t == zrdr::Int || t == zrdr::Float) return at + 8;
  if (t == zrdr::String) return v <= size - at - 8 ? at + 8 + v : 0;
  if (t != zrdr::List || v == 0) return 0;
  size_t q = at + 8;
  for (uint32_t i = 1; i < v; ++i)
    if (!(q = skip(p, size, q, depth + 1))) return 0;
  return q;
}
std::string string_at(const uint8_t* p, size_t size, size_t at) {
  if (at + 8 > size || get32(p + at) != zrdr::String) return {};
  const uint32_t n = get32(p + at + 4);
  return n <= size - at - 8 ? std::string(reinterpret_cast<const char*>(p + at + 8), n) : std::string();
}
// The pairs list's items: each key's start and its value's start and end.
struct Pair {
  std::string key;
  size_t value = 0, end = 0;
};
bool pairs_of(const uint8_t* p, size_t size, std::vector<Pair>* out, size_t* list_end) {
  if (size < 16 || get32(p) != zrdr::List || get32(p + 8) != zrdr::List) return false;
  const uint32_t n = get32(p + 12);
  size_t q = 16;
  for (uint32_t i = 1; i < n; i += 2) {
    const size_t k_end = skip(p, size, q);
    if (!k_end) return false;
    Pair pr;
    pr.key = string_at(p, size, q);
    pr.value = k_end;
    if (i + 1 >= n) {  // a key without a value
      q = k_end;
      break;
    }
    if (!(pr.end = skip(p, size, k_end))) return false;
    out->push_back(pr);
    q = pr.end;
  }
  *list_end = q;
  return true;
}
bool prefixed(const std::string& key, const std::string& type) {
  if (key.size() <= type.size() || key[type.size()] != '_') return false;
  return crt::msvc_stricmp(key.substr(0, type.size()).c_str(), type.c_str()) == 0;
}

// ---- the form ----
enum Kind { kType, kFloat, kInt, kSide };
struct Field {
  const char* label;
  Kind kind;
  float* f = nullptr;
  int32_t* i = nullptr;
};
struct Form {
  bool open = false, choosing = false;
  int32_t first = 0;
  std::vector<std::string> types;
  int32_t type = 0;
  float pos[3] = {}, yaw = 0;
  int32_t team = 1, side = 0;  // 0 Friend (1052), 1 Foe, 2 Other
  std::vector<Field> fields;
  int32_t typing = -1;
  std::string buffer;
  std::vector<std::string> texts;
  std::vector<EditorEntry> rows;
  EditorMenu menu{0, nullptr};
};
Form g;
std::string g_last;
std::map<std::string, std::vector<uint8_t>> g_written;  // the aiv.zrd each archive was last saved with
constexpr int32_t kTypeRows = 18;
const char* const kSides[3] = {"Friend", "Foe", "Other"};
const int32_t kSideTeam[3] = {1, 2, 0};  // the shipped records' teams: the player's 1, the enemy's 2

void fill();
void press(int32_t y);
void ok(int32_t);
void cancel(int32_t);
void pick_type(int32_t y);
void types_up(int32_t) { g.first = std::max(0, g.first - kTypeRows), fill(); }
void types_down(int32_t) { g.first += kTypeRows, fill(); }

std::string shown(const Field& f) {
  char b[48];
  switch (f.kind) {
    case kType: return g.types.empty() ? "(none)" : g.types[size_t(g.type)];
    case kFloat: std::snprintf(b, sizeof b, "%.2f", *f.f); return b;
    case kInt: std::snprintf(b, sizeof b, "%d", *f.i); return b;
    case kSide: return kSides[g.side];
  }
  return {};
}

void fill() {
  g.texts.clear(), g.rows.clear();
  const auto add = [](const std::string& t, void (*h)(int32_t)) { g.texts.push_back(t), g.rows.push_back({nullptr, h}); };
  if (g.choosing) {
    const int32_t n = static_cast<int32_t>(g.types.size()), to = std::min(n, g.first + kTypeRows);
    add("Vehicle Type:", nullptr);
    if (g.first > 0) add("   (up)", types_up);
    for (int32_t i = g.first; i < to; ++i) add("   " + g.types[size_t(i)], pick_type);
    if (to < n) add("   (down)", types_down);
  } else {
    add("Node Edit - Vehicle", nullptr);
    for (size_t i = 0; i < g.fields.size(); ++i) {
      const Field& f = g.fields[i];
      const bool t = static_cast<int32_t>(i) == g.typing;
      add(std::string(t ? " > " : "   ") + f.label + ": " + (t ? g.buffer + "_" : shown(f)), press);
    }
    add(" OK", ok);
    add(" Cancel", cancel);
  }
  for (size_t i = 0; i < g.rows.size(); ++i) g.rows[i].text = g.texts[i].c_str();
  g.menu = EditorMenu{static_cast<int32_t>(g.rows.size()), g.rows.data()};
  editor_install(1, g.menu);
}

void take() {
  if (g.typing < 0) return;
  Field& f = g.fields[size_t(g.typing)];
  char* end = nullptr;
  if (f.kind == kFloat) {
    const float v = std::strtof(g.buffer.c_str(), &end);
    if (!g.buffer.empty() && end && !*end) *f.f = v;
  } else if (f.kind == kInt) {
    const long v = std::strtol(g.buffer.c_str(), &end, 10);
    if (!g.buffer.empty() && end && !*end) *f.i = static_cast<int32_t>(v);
  }
  g.typing = -1, g.buffer.clear();
}

void on_char(int32_t ch, int32_t key, uintptr_t) { add_vehicle_char(ch, key); }

void press(int32_t y) {
  const int32_t i = (y - 0x5f) / 12 - 1;
  if (i < 0 || i >= static_cast<int32_t>(g.fields.size())) return;
  if (g.typing >= 0) take();
  Field& f = g.fields[size_t(i)];
  if (f.kind == kType) {
    g.choosing = true, g.first = 0;
  } else if (f.kind == kSide) {  // the radio buttons; the team follows (the edit stays the player's)
    g.side = (g.side + 1) % 3, g.team = kSideTeam[g.side];
  } else {
    g.typing = i, g.buffer.clear();  // typed text replaces the value; Enter on none keeps it
  }
  fill();
}

void pick_type(int32_t y) {
  const int32_t i = (y - 0x5f) / 12 - 1 - (g.first > 0 ? 1 : 0) + g.first;
  if (i >= 0 && i < static_cast<int32_t>(g.types.size())) g.type = i;
  g.choosing = false;
  fill();
}

void close_form() {
  zin::set_char_callback(nullptr, 0);
  g = Form{};
  net_editor_menu();
}

// The aiv.zrd entry of the first open archive holding one, and its path: as
// this run last saved it when it did.
bool aiv_entry(std::string* archive, std::vector<uint8_t>* bytes) {
  for (zutl_zar::Archive* a : zrdr::state().archives) {
    const int i = a && a->path ? zutl_zar::find(*a, "aiv.zrd") : -1;
    if (i < 0) continue;
    *archive = a->path;
    auto w = g_written.find(*archive);
    if (w != g_written.end()) return *bytes = w->second, true;
    uint32_t size = a->entries[i].length;
    bytes->assign(size, 0);
    if (zutl_zar::read(*a, i, bytes->data(), &size) != 0) return false;
    bytes->resize(size);
    return true;
  }
  return false;
}

// The vehicle made now, as the mission's start makes each record's
// (player.cpp: the type up to the first '_', the rest the suffix).
bool spawn(const std::string& name, const std::vector<uint8_t>& value) {
  std::FILE* f = std::tmpfile();
  if (!f) return false;
  std::fwrite(value.data(), 1, value.size(), f);
  std::rewind(f);
  auto* node = new zrdr::Node{};
  zrdr::read_node(f, node);
  std::fclose(f);
  ai::AivRecord rec;
  const bool read = rec.parse(node, name.c_str()) != nullptr;
  std::string type = name, suffix;
  if (const size_t u = name.find('_'); u != std::string::npos) type = name.substr(0, u), suffix = name.substr(u + 1);
  bool made = false;
  if (read) {
    PlayerHooks& h = player_hooks();
    const zgame::Handle vehicles = h.reader_load("vehicle.zrd");
    made = create_vehicle(h.reader_find(vehicles, type.c_str()), &rec, type.c_str(), name.c_str(), suffix.c_str());
    h.reader_free(vehicles);
  }
  zrdr::free_node(node);
  return made;
}

// The record: copied from one of its type in the entry (its tags as the
// shipped ones are), its net, place, yaw and team ours; made now, and the
// entry saved into the editor's mod.
void ok(int32_t) {
  if (g.typing >= 0) take();
  NetEditor* e = net_editor();
  std::string archive;
  std::vector<uint8_t> aiv;
  if (!e || !e->net || g.types.empty()) return close_form();
  if (!aiv_entry(&archive, &aiv)) {
    editor_status("no aiv.zrd in the open archives");
    return close_form();
  }
  const std::string type = g.types[size_t(g.type)];
  int32_t items = 0;
  const std::vector<uint8_t> tags = aiv_tags_of_type(aiv.data(), aiv.size(), type, &items);
  const std::vector<uint8_t> value = aiv_record(e->net->header.net, g.pos, g.yaw, g.team, tags, items);
  const std::string name = aiv_free_name(aiv.data(), aiv.size(), type);
  const std::vector<uint8_t> out = aiv_with_record(aiv.data(), aiv.size(), name, value);
  if (out.empty()) {
    editor_status("aiv.zrd does not read as a reader file");
    return close_form();
  }
  const bool made = spawn(name, value);
  const ai::NetSave s = ai::save_archive_edit(archive.c_str(), "aiv.zrd", out);
  if (s.ok) g_written[archive] = out;
  g_last = name;
  char line[160];
  std::snprintf(line, sizeof line, "%s on net %d: %s; %s", name.c_str(), e->net->header.net, made ? "made" : "not made",
                s.ok ? s.message.c_str() : ("not saved: " + s.error).c_str());
  close_form();
  editor_status(line);  // after the menu's refill
}

void cancel(int32_t) { close_form(); }
}  // namespace

std::vector<std::string> add_vehicle_types() {
  std::vector<std::string> t;
  for (int32_t i = 0; i < 19; ++i) t.push_back(mech_name(i));  // 0x00484650, the original 19
  if (zrdr::Node* v = zrdr::load("vehicle.zrd")) {
    if (v->type == zrdr::List && v->list[0].i > 1 && v->list[1].type == zrdr::List) {
      const zrdr::Node& pairs = v->list[1];
      for (int32_t i = 1; i < pairs.list[0].i; i += 2)
        if (pairs.list[i].type == zrdr::String) t.push_back(pairs.list[i].s);
    }
    zrdr::free_node(v);
  }
  return t;
}

std::vector<uint8_t> aiv_record(int32_t net, const float pos[3], float yaw, int32_t team,
                                const std::vector<uint8_t>& rest, int32_t rest_items) {
  std::vector<uint8_t> o;
  put32(o, zrdr::List), put32(o, static_cast<uint32_t>(1 + 4 + rest_items));
  put_int(o, net);
  put32(o, zrdr::List), put32(o, 4);
  for (int k = 0; k < 3; ++k) put_float(o, pos[k]);
  put_float(o, yaw);
  put_int(o, team);
  o.insert(o.end(), rest.begin(), rest.end());
  return o;
}

std::vector<uint8_t> aiv_tags_of_type(const uint8_t* p, size_t size, const std::string& type, int32_t* items) {
  *items = 0;
  std::vector<Pair> pairs;
  size_t list_end = 0;
  if (!pairs_of(p, size, &pairs, &list_end)) return {};
  for (const Pair& pr : pairs) {
    if (!prefixed(pr.key, type) || get32(p + pr.value) != zrdr::List) continue;
    const uint32_t n = get32(p + pr.value + 4);
    if (n < 6) return {};  // four items, no tags
    size_t q = pr.value + 8;
    for (int k = 0; k < 4 && q; ++k) q = skip(p, size, q);
    if (!q) return {};
    *items = static_cast<int32_t>(n - 5);
    return std::vector<uint8_t>(p + q, p + pr.end);
  }
  return {};
}

std::vector<uint8_t> aiv_with_record(const uint8_t* p, size_t size, const std::string& name,
                                     const std::vector<uint8_t>& value) {
  std::vector<Pair> pairs;
  size_t list_end = 0;
  if (!pairs_of(p, size, &pairs, &list_end)) return {};
  std::vector<uint8_t> o;
  put32(o, zrdr::List), put32(o, get32(p + 4));
  put32(o, zrdr::List), put32(o, get32(p + 12) + 2);
  o.insert(o.end(), p + 16, p + list_end);
  put_string(o, name);
  o.insert(o.end(), value.begin(), value.end());
  o.insert(o.end(), p + list_end, p + size);  // the rest of the top list and whatever follows it
  return o;
}

std::string aiv_free_name(const uint8_t* p, size_t size, const std::string& type) {
  std::vector<Pair> pairs;
  size_t list_end = 0;
  pairs_of(p, size, &pairs, &list_end);
  for (int32_t n = 1; n < 1000; ++n) {
    char b[64];
    std::snprintf(b, sizeof b, "%s_%02d", type.c_str(), n);
    bool used = false;
    for (const Pair& pr : pairs) used = used || crt::msvc_stricmp(pr.key.c_str(), b) == 0;
    if (!used) return b;
  }
  return type + "_new";
}

void add_vehicle_open(int32_t) {
  NetEditor* e = net_editor();
  if (!e) return;
  znet::Mark* m = net_editor_first_pick();  // 0x00403770
  ai::NetNode* n = m && m->elem ? static_cast<ai::AiElem*>(m->elem)->as_node() : nullptr;
  if (!n || n->placeholder) {
    editor_status("Select a node first");  // 0x00402f10(0x30): a box in the exe
    return;
  }
  g = Form{};
  g.open = true;
  g.types = add_vehicle_types();
  for (int k = 0; k < 3; ++k) g.pos[k] = n->pos()[k];  // the node's place, yaw 0 (0x0049d350's)
  g.fields = {{"vehicle type", kType},   {"x", kFloat, &g.pos[0]}, {"y", kFloat, &g.pos[1]}, {"z", kFloat, &g.pos[2]},
              {"yaw", kFloat, &g.yaw},   {"team id", kInt, nullptr, &g.team}, {"side", kSide}};
  zin::set_char_callback(on_char, 0);
  fill();
}

bool add_vehicle_open_now() { return g.open; }
const std::string& add_vehicle_last() { return g_last; }

void add_vehicle_char(int32_t ch, int32_t key) {
  if (!g.open || g.typing < 0) return;
  if (ch == '\r' || ch == '\n' || key == 0x1c) take();
  else if (ch == 27 || key == 0x01) g.typing = -1, g.buffer.clear();
  else if (ch == 8 || key == 0x0e) { if (!g.buffer.empty()) g.buffer.pop_back(); }
  else if (ch >= 0x20 && ch < 0x7f && g.buffer.size() < 20) g.buffer += static_cast<char>(ch);
  else return;
  fill();
}

void add_vehicle_press(int32_t row) {
  if (row >= 0 && row < g.menu.count && g.rows[size_t(row)].handler) g.rows[size_t(row)].handler(0x5f + 12 * row + 2);
}

}  // namespace mech3
