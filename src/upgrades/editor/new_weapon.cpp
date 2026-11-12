#include "upgrades/editor/new_weapon.h"
#include "upgrades/editor/save_as.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>

#include "crt/msvc_string.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/game_editor.h"
#include "mech3/game_editor_net.h"
#include "mech3/mech_names.h"
#include "mech3/net_save.h"
#include "upgrades/editor/form.h"
#include "upgrades/editor/mods_edit.h"
#include "upgrades/editor/zrd_bytes.h"

namespace mech3 {

namespace {
namespace fs = std::filesystem;
using namespace zrdbytes;

// A zrd node as the file holds it, read whole and written back the same.
uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
float from_bits(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

struct Node {
  int32_t type = zrdr::Int;
  uint32_t value = 0;  // Int and Float: the 32 bits
  std::string s;
  std::vector<Node> kids;
  float f() const { return type == zrdr::Float ? from_bits(value) : static_cast<float>(static_cast<int32_t>(value)); }
  int32_t i() const { return type == zrdr::Float ? static_cast<int32_t>(from_bits(value)) : static_cast<int32_t>(value); }
};

bool read(const uint8_t* p, size_t size, size_t* at, Node* out, int depth = 0) {
  if (*at + 8 > size || depth > 64) return false;
  out->type = static_cast<int32_t>(get32(p + *at));
  const uint32_t v = get32(p + *at + 4);
  *at += 8;
  switch (out->type) {
    case zrdr::Int:
    case zrdr::Float: out->value = v; return true;
    case zrdr::String:
      if (v > size - *at) return false;
      out->s.assign(reinterpret_cast<const char*>(p + *at), v);
      *at += v;
      return true;
    case zrdr::List:
      if (v == 0) return false;
      out->kids.resize(v - 1);
      for (Node& k : out->kids)
        if (!read(p, size, at, &k, depth + 1)) return false;
      return true;
    default: return false;
  }
}
void write(std::vector<uint8_t>& o, const Node& n) {
  put32(o, static_cast<uint32_t>(n.type));
  switch (n.type) {
    case zrdr::String:
      put32(o, static_cast<uint32_t>(n.s.size()));
      o.insert(o.end(), n.s.begin(), n.s.end());
      break;
    case zrdr::List:
      put32(o, static_cast<uint32_t>(n.kids.size() + 1));
      for (const Node& k : n.kids) write(o, k);
      break;
    default: put32(o, n.value); break;
  }
}
Node str(const std::string& s) {
  Node n;
  n.type = zrdr::String, n.s = s;
  return n;
}
Node num(int32_t type, float f) {  // as the copy's own number: an Int stays an Int
  Node n;
  n.type = type;
  n.value = type == zrdr::Float ? bits(f) : static_cast<uint32_t>(static_cast<int32_t>(std::lround(f)));
  return n;
}
Node list(std::vector<Node> kids) {
  Node n;
  n.type = zrdr::List, n.kids = std::move(kids);
  return n;
}
bool same(const std::string& a, const std::string& b) { return crt::msvc_stricmp(a.c_str(), b.c_str()) == 0; }

// A pairs list's value under `key` (any case), null when none.
Node* value_of(Node& l, const std::string& key) {
  for (size_t k = 0; k + 1 < l.kids.size(); k += 2)
    if (l.kids[k].type == zrdr::String && same(l.kids[k].s, key)) return &l.kids[k + 1];
  return nullptr;
}
const Node* value_of(const Node& l, const std::string& key) { return value_of(const_cast<Node&>(l), key); }

// The file: its top list holds (VERSION ..., BALLISTICS (name def ...)).
struct File {
  Node root;
  size_t bal_at = 0, bal_end = 0;  // BALLISTICS' value in the bytes
};
bool parse(const uint8_t* p, size_t size, File* f) {
  size_t at = 0;
  if (!read(p, size, &at, &f->root) || f->root.type != zrdr::List || f->root.kids.empty() ||
      f->root.kids[0].type != zrdr::List)
    return false;
  std::vector<Pair> pairs;
  size_t end = 0;
  if (!pairs_of(p, size, &pairs, &end)) return false;
  for (const Pair& pr : pairs)
    if (same(pr.key, "BALLISTICS")) f->bal_at = pr.value, f->bal_end = pr.end;
  return f->bal_at && get32(p + f->bal_at) == zrdr::List && value_of(f->root.kids[0], "BALLISTICS");
}
const Node& ballistics(const File& f) { return *value_of(f.root.kids[0], "BALLISTICS"); }

// A list value's first item as a number (DAMAGE (17.0)), else a bare number.
Node* first(Node* v) {
  if (!v) return nullptr;
  if (v->type == zrdr::List) return v->kids.empty() ? nullptr : &v->kids[0];
  return v;
}
void set_first(Node& def, const char* key, float f) {
  if (Node* n = first(value_of(def, key))) {
    *n = num(n->type, f);
  } else {
    def.kids.push_back(str(key));
    def.kids.push_back(list({num(zrdr::Float, f)}));
  }
}
// MASS (tech (tons slots) ... ammo (rounds)): one tech's pair or the rounds set, added when missing.
void set_mass(Node& def, const char* tech, std::vector<int32_t> values) {
  Node* mass = value_of(def, "MASS");
  if (!mass || mass->type != zrdr::List) {
    def.kids.push_back(str("MASS"));
    def.kids.push_back(list({}));
    mass = &def.kids.back();
  }
  std::vector<Node> nums;
  Node* was = value_of(*mass, tech);
  for (size_t k = 0; k < values.size(); ++k) {
    const Node* w = was && was->type == zrdr::List && k < was->kids.size() ? &was->kids[k] : nullptr;
    nums.push_back(num(w ? w->type : zrdr::Int, static_cast<float>(values[k])));
  }
  if (was) *was = list(std::move(nums));
  else mass->kids.push_back(str(tech)), mass->kids.push_back(list(std::move(nums)));
}

const EffectSlotInfo kSlots[kEffectSlots] = {
    {"muzzle", "FIRE", "ANIMATION", 'a'},       {"firing sound", "FIRE", "SOUND", 's'},
    {"projectile", "FLYOUT", "MODEL", 'm'},     {"projectile animation", "FLYOUT", "MODEL_ANIMATION", 'a'},
    {"trail", "FLYOUT", "ANIMATION", 'a'},      {"flyby sound", "FLYOUT", "SOUND", 's'},
    {"impact", "IMPACT", "ANIMATION", 'a'},     {"impact sound", "IMPACT", "SOUND", 's'},
};
// A slot's block in a definition: FIRE, FLYOUT, or IMPACT's `default`; made when `make`.
Node* slot_block(Node& def, int slot, bool make) {
  const char* b = kSlots[slot].block;
  Node* v = value_of(def, b);
  if (!v || v->type != zrdr::List) {
    if (!make) return nullptr;
    def.kids.push_back(str(b)), def.kids.push_back(list({}));
    v = &def.kids.back();
  }
  if (std::strcmp(b, "IMPACT") != 0) return v;
  Node* d = value_of(*v, "default");
  if (!d || d->type != zrdr::List) {
    if (!make) return nullptr;
    v->kids.push_back(str("default")), v->kids.push_back(list({}));
    d = &v->kids.back();
  }
  return d;
}
std::string slot_value(Node& def, int slot) {
  Node* b = slot_block(def, slot, false);
  const Node* n = b ? first(value_of(*b, kSlots[slot].key)) : nullptr;
  return n && n->type == zrdr::String ? n->s : std::string();
}
void set_slot(Node& def, int slot, const std::string& value) {
  if (value.empty()) return;  // the copy's
  Node* b = slot_block(def, slot, value != kNoEffect);
  if (!b) return;
  for (size_t k = 0; k + 1 < b->kids.size(); k += 2)
    if (b->kids[k].type == zrdr::String && same(b->kids[k].s, kSlots[slot].key)) {
      if (value == kNoEffect) {
        b->kids.erase(b->kids.begin() + static_cast<std::ptrdiff_t>(k), b->kids.begin() + static_cast<std::ptrdiff_t>(k + 2));
      } else if (b->kids[k + 1].type == zrdr::List && !b->kids[k + 1].kids.empty()) {
        b->kids[k + 1].kids[0] = str(value);  // its other items (the muzzle's "follows") kept
      } else {
        b->kids[k + 1] = list({str(value)});
      }
      return;
    }
  if (value != kNoEffect) b->kids.push_back(str(kSlots[slot].key)), b->kids.push_back(list({str(value)}));
}

std::string text_of_node(const Node& n) {
  char b[64];
  switch (n.type) {
    case zrdr::List: {
      std::string t = "(";
      for (size_t i = 0; i < n.kids.size(); ++i) t += (i ? " " : "") + text_of_node(n.kids[i]);
      return t + ")";
    }
    case zrdr::String: return "\"" + n.s + "\"";
    case zrdr::Float:
      std::snprintf(b, sizeof b, "%.9g", static_cast<double>(n.f()));
      if (std::strchr(b, 'e')) std::snprintf(b, sizeof b, "%.9f", static_cast<double>(n.f()));
      if (!std::strchr(b, '.')) std::strcat(b, ".0");
      return b;
    default: return std::to_string(n.i());
  }
}

// ---- the form ----
struct Form {
  bool open = false;
  std::vector<uint8_t> zrd;            // weapons.zrd as the mod's save stands
  std::vector<std::string> copies, defined;
  int32_t copy = 0, ammo = 0, tech = 0;  // tech: 0 Clan, 1 Inner Sphere, 2 any, 3 none
  std::string name, title, error;
  WeaponNumbers n;
  int32_t name_limit = 31;
  int32_t fx[kEffectSlots] = {};                  // each slot's pick: 0 none, then fx_names[k]'s
  std::vector<std::string> fx_names[kEffectSlots];  // "(none)" first, then every name the file's weapons use there
  bool loose = false;                             // the mod's loose zbd\weapons.zrd is what is written
  EditorForm form;
};
Form g;
std::string g_last;
const int32_t kTechs[4] = {kClan, kInnerSphere, kAnyTech, 0};
const char* const kTechWords[4] = {"clan", "sphere", "any", "none"};

void build();
void show_error(const std::string& e) {
  g.error = e;
  if (!g.form.fields.empty()) g.form.fields.back().label = e;
}

std::string text_of_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void close_form() {
  form_hide(&g.form);
  g.open = false;
}

// The weapons.zrd New Weapon builds on: the mod's own reader.zbd as its last
// save left it, else the one the game reads; `rel` its archive.
bool source(std::vector<uint8_t>* out, std::string* why) {
  const std::string rel = "zbd\\reader.zbd";
  const std::string& mod = mods_edit_session().mod;
  // A loose zbd\weapons.zrd wins in a mod (modding.md, Readable files read directly): its text, read.
  const fs::path loose = fs::path(zutl::mounts().write) / "mods" / mod / "zbd" / "weapons.zrd";
  std::error_code lec;
  if (fs::exists(loose, lec)) {
    g.loose = true;
    *out = weapons_text_bytes(text_of_file(loose), why);
    if (out->empty()) return *why = "the mod's zbd\\weapons.zrd: " + *why, false;
    return true;
  }
  const fs::path own = fs::path(zutl::mounts().write) / "mods" / mod / "zbd" / "reader.zbd";
  std::error_code ec;
  const std::string path = fs::exists(own, ec) ? own.string() : zutl::find(rel.c_str());
  if (path.empty()) return *why = "zbd\\reader.zbd is not found", false;
  std::ifstream in(path, std::ios::binary);
  const std::vector<uint8_t> a((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const size_t n = a.size();
  if (n < 8 || get32(a.data() + n - 8) != 1) return *why = path + " is not an archive the game reads", false;
  const uint32_t count = get32(a.data() + n - 4);
  if (static_cast<size_t>(count) * sizeof(zutl_zar::Entry) > n - 8) return *why = path + " is not an archive the game reads", false;
  const size_t table = n - 8 - count * sizeof(zutl_zar::Entry);
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry e;
    std::memcpy(&e, a.data() + table + i * sizeof e, sizeof e);
    e.name[sizeof e.name - 1] = 0;
    if (same(e.name, "weapons.zrd") && e.start <= table && e.length <= table - e.start)
      return out->assign(a.begin() + e.start, a.begin() + e.start + e.length), true;
  }
  return *why = path + " holds no weapons.zrd", false;
}

void pick_copy() {
  if (g.copies.empty()) return;
  g.copy = std::clamp(g.copy, 0, static_cast<int32_t>(g.copies.size()) - 1);
  const std::string& c = g.copies[size_t(g.copy)];
  g.n = WeaponNumbers{};
  weapon_numbers_of(g.zrd.data(), g.zrd.size(), c, &g.n);
  g.name = weapon_free_name(c, g.defined);
  g.title = g.name;
  const int32_t w = weapon_index(c.c_str());
  g.ammo = w >= 0 && weapon_uses_ammo(w) ? 0 : 1;  // 0 yes, 1 no
  const bool clan = w >= 0 && weapon_has_tech(w, kClan), sphere = w >= 0 && weapon_has_tech(w, kInnerSphere);
  g.tech = clan && sphere ? 2 : clan ? 0 : sphere ? 1 : 3;
  WeaponEffects e;
  weapon_effects_of(g.zrd.data(), g.zrd.size(), c, &e);
  for (int k = 0; k < kEffectSlots; ++k) {
    const std::vector<std::string>& names = g.fx_names[k];
    const auto it = std::find(names.begin(), names.end(), e.value[k]);
    g.fx[k] = e.value[k].empty() || it == names.end() ? 0 : static_cast<int32_t>(it - names.begin());
  }
  show_error("");
}

// A slot's pick shown once at the view's pick, else at its camera.
void preview(int slot) {
  const int32_t i = g.fx[slot];
  if (i <= 0 || static_cast<size_t>(i) >= g.fx_names[slot].size() || kSlots[slot].kind == 'm') return;
  const std::string& name = g.fx_names[slot][size_t(i)];
  float at[3] = {};
  if (!editor_hooks().ground_pick(at))
    if (const float* c = editor_hooks().camera_pos()) at[0] = c[0], at[1] = c[1], at[2] = c[2];
  NewWeaponPreview& p = new_weapon_preview();
  bool shown;
  if (kSlots[slot].kind == 's') {
    if (p.sound) shown = p.sound(name, at);
    else if (zsnd::Object* o = zsnd::find(name.c_str())) shown = zsnd::play_at(o, at, 1.0f, nullptr) != nullptr;
    else shown = false;
  } else {
    static const float zero[3] = {};
    if (p.anim) shown = p.anim(name, at);
    else if (zeff::AnimDef* d = zeff::anim_find(name.c_str())) shown = zeff::anim_start_at(d, nullptr, at, zero, zero) != nullptr;
    else shown = false;
  }
  if (!shown) editor_status(name + " is not loaded in this mission, so it cannot be shown here");
}

WeaponEffects effects_picked() {
  WeaponEffects e;
  for (int k = 0; k < kEffectSlots; ++k) {
    const int32_t i = g.fx[k];
    e.value[k] = i > 0 && static_cast<size_t>(i) < g.fx_names[k].size() ? g.fx_names[k][size_t(i)] : std::string(kNoEffect);
  }
  return e;
}

// The row as mod.txt's reader takes it; "" and `why` when refused.
std::string row_line(ModWeapon* r, std::string* why) {
  std::string line = g.name + " ammo=" + (g.ammo == 0 ? "yes" : "no") + " tech=" + kTechWords[std::clamp(g.tech, 0, 3)];
  if (!g.title.empty() && g.title != g.name) line += " title=" + g.title;
  if (!weapon_row_parse(line, r, why)) return {};
  if (weapon_row_taken(r->name)) return *why = "there is a weapon " + r->name + " already", std::string();
  for (const std::string& d : g.defined)
    if (same(d, r->name)) return *why = "weapons.zrd has a " + d + " already", std::string();
  return "weapon=" + line;
}

void ok() {
  if (!editor_save_ready([] { ok(); })) return;  // ours (save_as.h): the copy's name first
  ModWeapon r;
  std::string why;
  const std::string line = row_line(&r, &why);
  if (line.empty() || g.copies.empty()) {
    return show_error(line.empty() ? why : "nothing to copy");  // the form stays, the reason on it
  }
  const std::string& mod = mods_edit_session().mod;
  const fs::path dir = fs::path(zutl::mounts().write) / "mods" / mod;
  std::error_code ec;
  if (!editor_owns_mod(zutl::mounts().write, mod))  // never an imported mod's or a hand-made folder's files (save_as.h)
    return show_error("mods\\" + mod + " was not made by the editor, so it is never written");
  const WeaponEffects fx = effects_picked();
  const std::string& copy = g.copies[size_t(g.copy)];
  std::string where = "zbd\\reader.zbd";
  if (g.loose) {  // the loose file wins in a mod: the pair put in its text, the rest of it as the modder wrote it
    const fs::path loose = dir / "zbd" / "weapons.zrd";
    const std::vector<uint8_t> def = weapon_definition(g.zrd.data(), g.zrd.size(), copy, r.name, g.n, r.tech, r.ammo, &fx);
    std::string text = def.empty() ? std::string() : weapons_text_with_definition(text_of_file(loose), r.name, zrd_text_of(def.data(), def.size()), &why);
    std::string read_why;
    const std::vector<uint8_t> back = text.empty() ? std::vector<uint8_t>{} : weapons_text_bytes(text, &read_why);
    const std::vector<std::string> names = weapons_defined(back.data(), back.size());
    if (names.empty() || names.back() != r.name) return show_error("the mod's zbd\\weapons.zrd: " + (why.empty() ? read_why : why));
    fs::copy_file(loose, loose.string() + ".bak", fs::copy_options::overwrite_existing, ec);
    std::ofstream(loose, std::ios::binary | std::ios::trunc) << text;
    where = "zbd\\weapons.zrd";
  } else {
    const std::vector<uint8_t> out = weapons_with_definition(g.zrd.data(), g.zrd.size(), copy, r.name, g.n, r.tech, r.ammo, &fx);
    if (out.empty()) return show_error("weapons.zrd does not read as one");
    const ai::NetSave s = ai::save_archive_edit("zbd\\reader.zbd", "weapons.zrd", out);
    if (!s.ok) return show_error("not saved: " + s.error);
  }
  // mod.txt: the row, its earlier text kept as mod.txt.bak.
  const fs::path txt = dir / "mod.txt";
  std::string text;
  {
    std::ifstream in(txt, std::ios::binary);
    text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }
  const std::string now = mod_txt_with_weapon(text, r.name, line);
  if (fs::exists(txt, ec)) fs::copy_file(txt, txt.string() + ".bak", fs::copy_options::overwrite_existing, ec);
  {
    std::ofstream o(txt, std::ios::binary | std::ios::trunc);
    o << now;
  }
  g_last = r.name;
  close_form();
  editor_status(r.name + " written into " + mod + " (mod.txt, " + where + "); the mech lab offers it from the next start");
}

void cancel() { close_form(); }

void build() {
  g.form = EditorForm{};
  g.form.title = "New Weapon";
  FormField copy{"copy of", FormField::kChoice};
  copy.i = &g.copy, copy.choices = g.copies, copy.changed = pick_copy;
  FormField name{"name", FormField::kText};
  name.text = &g.name, name.limit = &g.name_limit;
  FormField ammo{"ammo", FormField::kChoice};
  ammo.i = &g.ammo, ammo.choices = {"yes", "no"};
  FormField tech{"tech", FormField::kChoice};
  tech.i = &g.tech, tech.choices = {"Clan", "Inner Sphere", "any", "none"};
  FormField title{"title", FormField::kText};
  title.text = &g.title;
  const auto flt = [](const char* label, float* v) {
    FormField f{label, FormField::kFloat};
    f.f = v;
    return f;
  };
  const auto in = [](const char* label, int32_t* v) {
    FormField f{label, FormField::kInt};
    f.i = v;
    return f;
  };
  g.form.fields = {copy, name, ammo, tech, title,
                   flt("damage", &g.n.damage), flt("heat", &g.n.heat), flt("range", &g.n.range),
                   flt("fire rate", &g.n.fire_rate), flt("velocity", &g.n.velocity),
                   in("Clan tons", &g.n.clan_tons), in("Clan slots", &g.n.clan_slots),
                   in("Inner Sphere tons", &g.n.sphere_tons), in("Inner Sphere slots", &g.n.sphere_slots),
                   in("rounds a ton", &g.n.rounds)};
  for (int k = 0; k < kEffectSlots; ++k) {
    FormField f{kSlots[k].label, FormField::kChoice};
    f.i = &g.fx[k], f.choices = g.fx_names[k];
    f.changed = [k] { preview(k); };
    g.form.fields.push_back(f);
  }
  g.form.fields.push_back({g.error, FormField::kLabel});  // last: why OK was refused (show_error)
  g.form.buttons = {{"OK", ok}, {"Cancel", cancel}};
}
}  // namespace

std::vector<std::string> weapons_defined(const uint8_t* zrd, size_t size) {
  File f;
  std::vector<std::string> out;
  if (!parse(zrd, size, &f)) return out;
  const Node& b = ballistics(f);
  for (size_t k = 0; k + 1 < b.kids.size(); k += 2)
    if (b.kids[k].type == zrdr::String) out.push_back(b.kids[k].s);
  return out;
}

std::vector<std::string> weapon_copies(const uint8_t* zrd, size_t size) {
  std::vector<std::string> out;
  for (const std::string& d : weapons_defined(zrd, size))
    if (weapon_index(d.c_str()) >= 0) out.push_back(d);
  return out;
}

bool weapon_numbers_of(const uint8_t* zrd, size_t size, const std::string& name, WeaponNumbers* out) {
  File f;
  if (!parse(zrd, size, &f)) return false;
  const Node* def = value_of(ballistics(f), name);
  if (!def || def->type != zrdr::List) return false;
  WeaponNumbers n;
  const auto get = [&](const char* key, float* v) {
    if (const Node* x = first(const_cast<Node*>(value_of(*def, key)))) *v = x->f();
  };
  n.beam = value_of(*def, "BEAM") != nullptr;
  get("DAMAGE", &n.damage), get("HEAT", &n.heat), get("RANGE", &n.range), get("FIRE_RATE", &n.fire_rate);
  get("VELOCITY", &n.velocity);
  if (const Node* mass = value_of(*def, "MASS"); mass && mass->type == zrdr::List) {
    const auto pair = [&](const char* tech, int32_t* tons, int32_t* slots) {
      const Node* p = value_of(*mass, tech);
      if (!p || p->type != zrdr::List || p->kids.size() < 2) return false;
      return *tons = p->kids[0].i(), *slots = p->kids[1].i(), true;
    };
    const bool clan = pair("clan", &n.clan_tons, &n.clan_slots);
    const bool sphere = pair("sphere", &n.sphere_tons, &n.sphere_slots);
    if (!clan) n.clan_tons = n.sphere_tons, n.clan_slots = n.sphere_slots;
    if (!sphere) n.sphere_tons = n.clan_tons, n.sphere_slots = n.clan_slots;
    if (const Node* a = value_of(*mass, "ammo"); a && a->type == zrdr::List && !a->kids.empty()) n.rounds = a->kids[0].i();
  }
  *out = n;
  return true;
}

std::string weapon_free_name(const std::string& copy, const std::vector<std::string>& defined) {
  const auto taken = [&](const std::string& s) {
    if (weapon_row_taken(s)) return true;
    return std::any_of(defined.begin(), defined.end(), [&](const std::string& d) { return same(d, s); });
  };
  for (int round = 1; round < 100; ++round)
    for (char c = 'B'; c <= 'Z'; ++c) {
      std::string s = copy + "_" + c + (round > 1 ? std::to_string(round) : std::string());
      if (s.size() <= 31 && !taken(s)) return s;
    }
  return {};
}

namespace {
// The new definition: `copy`'s with the numbers and the effects set.
bool build_def(const File& f, const std::string& copy, const std::string& name, const WeaponNumbers& n, int32_t tech,
               bool ammo, const WeaponEffects* fx, Node* out) {
  const Node* from = value_of(ballistics(f), copy);
  if (!from || from->type != zrdr::List) return false;
  Node def = *from;
  if (Node* nm = first(value_of(def, "NAME")); nm && nm->type == zrdr::String) nm->s = name;  // found by NAME (0x581ed0)
  set_first(def, "DAMAGE", n.damage);
  set_first(def, "HEAT", n.heat);
  set_first(def, "RANGE", n.range);
  set_first(def, "FIRE_RATE", n.fire_rate);
  if (!n.beam) set_first(def, "VELOCITY", n.velocity);
  if (tech & kClan) set_mass(def, "clan", {n.clan_tons, n.clan_slots});
  if (tech & kInnerSphere) set_mass(def, "sphere", {n.sphere_tons, n.sphere_slots});
  if (ammo) set_mass(def, "ammo", {n.rounds});
  if (fx)
    for (int k = 0; k < kEffectSlots; ++k) set_slot(def, k, fx->value[k]);
  *out = std::move(def);
  return true;
}
}  // namespace

std::vector<uint8_t> weapon_definition(const uint8_t* zrd, size_t size, const std::string& copy, const std::string& name,
                                       const WeaponNumbers& n, int32_t tech, bool ammo, const WeaponEffects* fx) {
  File f;
  Node def;
  std::vector<uint8_t> out;
  if (parse(zrd, size, &f) && build_def(f, copy, name, n, tech, ammo, fx, &def)) write(out, def);
  return out;
}

std::vector<uint8_t> weapons_with_definition(const uint8_t* zrd, size_t size, const std::string& copy,
                                             const std::string& name, const WeaponNumbers& n, int32_t tech,
                                             bool ammo, const WeaponEffects* fx) {
  File f;
  Node def;
  if (!parse(zrd, size, &f) || !build_def(f, copy, name, n, tech, ammo, fx, &def)) return {};
  // BALLISTICS' count two more, its items as they were, the new pair after them.
  std::vector<uint8_t> out(zrd, zrd + f.bal_at);
  put32(out, zrdr::List), put32(out, get32(zrd + f.bal_at + 4) + 2);
  out.insert(out.end(), zrd + f.bal_at + 8, zrd + f.bal_end);
  write(out, str(name));
  write(out, def);
  out.insert(out.end(), zrd + f.bal_end, zrd + size);
  return out;
}

const EffectSlotInfo& effect_slot(int slot) { return kSlots[std::clamp(slot, 0, kEffectSlots - 1)]; }

bool weapon_effects_of(const uint8_t* zrd, size_t size, const std::string& name, WeaponEffects* out) {
  File f;
  if (!parse(zrd, size, &f)) return false;
  Node* def = value_of(const_cast<Node&>(ballistics(f)), name);
  if (!def || def->type != zrdr::List) return false;
  for (int k = 0; k < kEffectSlots; ++k) out->value[k] = slot_value(*def, k);
  return true;
}

std::vector<std::string> effect_choices(const uint8_t* zrd, size_t size, int slot) {
  File f;
  std::set<std::string> seen;
  if (parse(zrd, size, &f)) {
    Node& b = const_cast<Node&>(ballistics(f));
    for (size_t k = 0; k + 1 < b.kids.size(); k += 2)
      if (b.kids[k + 1].type == zrdr::List)
        if (std::string v = slot_value(b.kids[k + 1], slot); !v.empty()) seen.insert(v);
  }
  return std::vector<std::string>(seen.begin(), seen.end());
}

std::string zrd_text_of(const uint8_t* p, size_t size) {
  Node n;
  size_t at = 0;
  return read(p, size, &at, &n) ? text_of_node(n) : std::string();
}

std::string weapons_text_with_definition(const std::string& text, const std::string& name, const std::string& def_text,
                                         std::string* why) {
  // The text's tokens by depth, past strings and ';' comments: BALLISTICS, then its list's closing parenthesis.
  int depth = 0, list_depth = -1;
  bool after_key = false;
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == ';') {
      while (i < text.size() && text[i] != '\n') ++i;
      continue;
    }
    if (c == '"') {
      const size_t e = text.find('"', i + 1);
      if (e == std::string::npos) break;
      after_key = list_depth < 0 && same(text.substr(i + 1, e - i - 1), "BALLISTICS");
      i = e;
      continue;
    }
    if (c == '(') {
      ++depth;
      if (after_key) list_depth = depth, after_key = false;
      continue;
    }
    if (c == ')') {
      if (depth == list_depth) return text.substr(0, i) + "\n  \"" + name + "\" " + def_text + "\n" + text.substr(i);
      --depth;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c))) continue;
    size_t e = i;  // a bare word or number
    while (e < text.size() && !std::isspace(static_cast<unsigned char>(text[e])) && text[e] != '(' && text[e] != ')' && text[e] != ';' && text[e] != '"') ++e;
    after_key = list_depth < 0 && same(text.substr(i, e - i), "BALLISTICS");
    i = e - 1;
  }
  return *why = "it has no BALLISTICS list", std::string();
}

std::vector<uint8_t> weapons_text_bytes(const std::string& text, std::string* why) {
  std::FILE* f = std::tmpfile();
  if (!f) return *why = "no temporary file", std::vector<uint8_t>{};
  std::fwrite(text.data(), 1, text.size(), f);
  std::rewind(f);
  zrdr::Node* root = zrdr::parse_text(f);
  std::fclose(f);
  std::vector<uint8_t> out;
  if (!root) return *why = "it does not read as a reader file", out;
  ai::put_node(out, *root);
  zrdr::free_node(root);
  return out;
}

NewWeaponPreview& new_weapon_preview() {
  static NewWeaponPreview p;
  return p;
}

std::string mod_txt_with_weapon(const std::string& text, const std::string& name, const std::string& line) {
  std::istringstream in(text);
  std::string out, l;
  bool put = false;
  while (std::getline(in, l)) {
    std::string bare = l;
    if (!bare.empty() && bare.back() == '\r') bare.pop_back();
    const size_t eq = bare.find('=');
    if (!put && eq != std::string::npos && same(bare.substr(0, eq), "weapon")) {
      std::istringstream w(bare.substr(eq + 1));
      std::string first;
      w >> first;
      if (same(first, name)) {
        out += line + (l.size() != bare.size() ? "\r\n" : "\n");
        put = true;
        continue;
      }
    }
    out += l + "\n";
  }
  if (!put) out += line + "\n";
  return out;
}

void new_weapon_open(int32_t) {
  if (!mods_edit_session().active) {
    editor_status("EDIT WEAPONS writes into a mod: open a mission from MODS, MISSIONS");
    return;
  }
  std::string why;
  g = Form{};
  if (!source(&g.zrd, &why)) {
    editor_status("New Weapon: " + why);
    return;
  }
  g.copies = weapon_copies(g.zrd.data(), g.zrd.size());
  g.defined = weapons_defined(g.zrd.data(), g.zrd.size());
  for (int k = 0; k < kEffectSlots; ++k) {
    g.fx_names[k] = {kNoEffect};
    for (const std::string& n : effect_choices(g.zrd.data(), g.zrd.size(), k)) g.fx_names[k].push_back(n);
  }
  g.open = true;
  build();
  pick_copy();
  form_show(&g.form);
}

EditorForm& new_weapon_form() { return g.form; }
const std::string& new_weapon_last() { return g_last; }

}  // namespace mech3
