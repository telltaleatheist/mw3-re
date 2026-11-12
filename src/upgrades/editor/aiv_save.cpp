#include "upgrades/editor/aiv_save.h"
#include "upgrades/editor/save_as.h"

#include <cstdio>
#include <cstring>
#include <map>

#include "gamez/zreader/zreader.h"
#include "mech3/mech_names.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_region.h"
#include "mech3/zrdraiv.h"
#include "upgrades/editor/add_vehicle.h"
#include "upgrades/editor/zrd_bytes.h"

namespace mech3 {

namespace {
using namespace zrdbytes;

// A value's bytes read as a node (as add_vehicle's spawn reads one).
struct Tree {
  zrdr::Node* node = nullptr;
  explicit Tree(const uint8_t* p, size_t size) {
    if (!p || !size) return;
    std::FILE* f = std::tmpfile();
    if (!f) return;
    std::fwrite(p, 1, size, f);
    std::rewind(f);
    node = new zrdr::Node{};
    zrdr::read_node(f, node);
    std::fclose(f);
  }
  ~Tree() {
    if (node) zrdr::free_node(node);
  }
};

// The words the reader stores as ints (an Int node's bits); the rest are floats.
bool int_word(int off) {
  switch (off) {
    case 0x04: case 0x18: case 0x3c: case 0x40: case 0x54: case 0x5c: case 0x60: case 0x80: case 0x88: case 0x8c:
    case 0x98: case 0x9c: case 0xa0: case 0xa4: case 0xbc: case 0xc4: case 0xc8:
      return true;
    default:
      return false;
  }
}

void put_word(std::vector<uint8_t>& o, const ai::AiParams& f, int off) {
  if (int_word(off))
    put_int(o, f.get<int32_t>(off));
  else
    put_float(o, f.get<float>(off));
}
bool bit(const ai::AiParams& p, int off, int b) { return (p.raw[off] >> b) & 1; }

void list_of(std::vector<uint8_t>& o, const std::vector<std::vector<uint8_t>>& items) {
  put32(o, zrdr::List), put32(o, static_cast<uint32_t>(items.size() + 1));
  for (const auto& i : items) o.insert(o.end(), i.begin(), i.end());
}
std::vector<uint8_t> w(const ai::AiParams& f, int off) {
  std::vector<uint8_t> o;
  put_word(o, f, off);
  return o;
}
std::vector<uint8_t> i32(int32_t v) {
  std::vector<uint8_t> o;
  put_int(o, v);
  return o;
}
std::vector<uint8_t> f32(float v) {
  std::vector<uint8_t> o;
  put_float(o, v);
  return o;
}
std::vector<uint8_t> str(const std::string& s) {
  std::vector<uint8_t> o;
  put_string(o, s);
  return o;
}
std::vector<uint8_t> name_or_0(const char* s) { return s ? str(s) : i32(0); }

// The tags the reader knows, each a group of the settings it sets.
struct Group {
  const char* tag;
  std::vector<int> words;
  std::vector<std::pair<int, int>> bits;  // (byte, bit)
};
const Group kGroups[] = {
    {"params", {0x1c, 0x3c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38}, {}},
    {"p2", {0x40, 0x44, 0x48, 0x4c, 0x50}, {{0xcc, 0}, {0xcc, 1}, {0xcc, 2}}},
    {"p6", {0x54, 0x58, 0x5c, 0x60}, {}},
    {"p8", {0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84, 0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c, 0xa0, 0xa4}, {}},
    {"p9", {0xa8, 0xac, 0xb0, 0xb4}, {{0xcc, 3}}},
    {"p10", {0xb8, 0xbc}, {}},
    {"p11", {0xc0}, {}},
    {"p12", {0xc4}, {{0xcc, 4}, {0xcc, 5}}},
    {"p13", {}, {{0xcc, 6}}},
    {"skin", {0xc8}, {}},
    {"lancemate", {}, {{0xcd, 0}, {0xcd, 1}}},
};
// The old forms p8 replaces.
bool old_form(const std::string& t) { return t == "p3" || t == "p4" || t == "p5" || t == "p7"; }

bool same_word(const ai::AiParams& a, const ai::AiParams& b, int off) {
  return a.get<uint32_t>(off) == b.get<uint32_t>(off);
}
bool same_group(const Group& g, const ai::AiParams& a, const ai::AiParams& b) {
  for (int off : g.words)
    if (!same_word(a, b, off)) return false;
  for (const auto& [byte, b2] : g.bits)
    if (bit(a, byte, b2) != bit(b, byte, b2)) return false;
  return true;
}
bool same_text(const char* a, const char* b) { return (!a && !b) || (a && b && std::strcmp(a, b) == 0); }

// A group's tag value from the record in the file's units.
std::vector<uint8_t> group_value(const std::string& tag, const ai::AiParams& f) {
  std::vector<uint8_t> o;
  if (tag == "params") {
    list_of(o, {w(f, 0x1c), w(f, 0x3c), w(f, 0x20), w(f, 0x24), w(f, 0x28), w(f, 0x2c), w(f, 0x30), w(f, 0x34), w(f, 0x38)});
  } else if (tag == "p2") {
    list_of(o, {w(f, 0x40), i32(bit(f, 0xcc, 0)), i32(bit(f, 0xcc, 1)), i32(bit(f, 0xcc, 2)), w(f, 0x44), w(f, 0x48),
                w(f, 0x4c), w(f, 0x50)});
  } else if (tag == "p6") {
    list_of(o, {w(f, 0x54), w(f, 0x58), w(f, 0x5c), w(f, 0x60)});
  } else if (tag == "p8") {
    std::vector<std::vector<uint8_t>> items = {w(f, 0x64), w(f, 0x68), w(f, 0x6c), w(f, 0x70), w(f, 0x80),
                                               w(f, 0x74), w(f, 0x78), w(f, 0x7c), w(f, 0x84)};
    for (int off = 0x88; off <= 0xa4; off += 4) items.push_back(w(f, off));
    list_of(o, items);
  } else if (tag == "p9") {
    list_of(o, {w(f, 0xa8), w(f, 0xac), w(f, 0xb0), i32(bit(f, 0xcc, 3)), w(f, 0xb4)});
  } else if (tag == "p10") {
    list_of(o, {w(f, 0xb8), w(f, 0xbc)});
  } else if (tag == "p11") {
    list_of(o, {w(f, 0xc0)});
  } else if (tag == "p12") {
    // No chassis is "default", which no lookup knows (-1); the exe crashes
    // on a p12 without a name (zrdraiv's FIX), so one is always written.
    const int32_t m = f.get<int32_t>(0xc4);
    const char* n = m >= 0 ? mech_name(m) : "default";
    list_of(o, {str(n), i32(bit(f, 0xcc, 4)), i32(bit(f, 0xcc, 5))});
  } else if (tag == "p13") {
    list_of(o, {i32(bit(f, 0xcc, 6))});
  } else if (tag == "skin") {
    list_of(o, {w(f, 0xc8)});
  } else if (tag == "lancemate") {
    list_of(o, {i32(bit(f, 0xcd, 0) | bit(f, 0xcd, 1) << 1)});
  }
  return o;
}

// vlist's orders, each (17 items) as the reader takes them (zrdraiv add_order).
std::vector<uint8_t> vlist_value(const ai::AiParams& f) {
  std::vector<std::vector<uint8_t>> orders;
  if (f.vlist)
    for (const ai::VListEntry& e : *f.vlist) {
      const auto b = [&](int k) { return i32((e.flags >> k) & 1); };
      std::vector<uint8_t> o;
      list_of(o, {b(1), b(2), f32(e.f00), b(3), b(4), f32(e.f04), b(5), f32(e.f08), i32(e.flags2 & 1),
                  name_or_0(e.region ? e.region->name() : nullptr), b(6), f32(e.f0c), b(7), f32(e.f10), b(0),
                  name_or_0(e.valve ? e.valve->name : nullptr), i32((e.flags2 >> 1) & 1)});
      orders.push_back(o);
    }
  std::vector<uint8_t> o;
  list_of(o, orders);
  return o;
}

std::vector<uint8_t> pilot_value(const ai::AiParams& f) {
  std::vector<uint8_t> o;
  list_of(o, {str(f.pilot ? f.pilot : ""), str("can_configure"), [&] {
                std::vector<uint8_t> v;
                list_of(v, {str(bit(f, 0xcc, 7) ? "true" : "false")});
                return v;
              }()});
  return o;
}

// A pilot tag is wanted for a name, or for can_configure away from the
// defaults' (its only home). With no name then it is "", which reads as an
// empty name rather than none: nothing else can carry the flag.
bool pilot_needed(const ai::AiParams& rec) {
  static const ai::AiParams defaults;  // 0x004d98c0 (an AivRecord would hold aiv.zrd for good)
  return rec.pilot || bit(rec, 0xcc, 7) != bit(defaults, 0xcc, 7);
}

std::vector<uint8_t> text_value(const char* s) {
  std::vector<uint8_t> o;
  list_of(o, {name_or_0(s)});
  return o;
}

struct Item {
  size_t at = 0, end = 0;
};
}  // namespace

std::vector<uint8_t> aiv_record_written(const uint8_t* was, size_t was_size, const ai::AiParams& rec, int32_t mech3_set,
                                        int32_t mech3, const char* name) {
  // The record as the file has it, read as the game reads it.
  ai::AivRecord orig;
  std::vector<Item> items;  // the value's items (header, then keys and values)
  if (was && was_size) {
    if (get32(was) != zrdr::List) return {};
    const uint32_t n = get32(was + 4);
    size_t q = 8;
    for (uint32_t i = 1; i < n; ++i) {
      const size_t e = skip(was, was_size, q);
      if (!e) return {};
      items.push_back({q, e});
      q = e;
    }
    Tree t(was, was_size);
    if (!t.node || !orig.parse(t.node, name)) return {};
  }  // else the defaults (0x004e0490), as a record the file lacks reads
  ai::AivRecord fu;  // the record in the file's units
  static_cast<ai::AiParams&>(fu) = rec;
  fu.to_file_units();
  const bool fresh = items.empty();
  const auto keep = [&](size_t i, std::vector<uint8_t>& o) {
    o.insert(o.end(), was + items[i].at, was + items[i].end);
  };
  std::vector<std::vector<uint8_t>> out;
  // The header: kind, (x y z), yaw, team (given when the file gives it or it
  // is not the name's own).
  const auto header = [&](size_t i, bool same, std::vector<uint8_t> v) {
    if (!fresh && i < items.size() && same) {
      std::vector<uint8_t> o;
      keep(i, o);
      out.push_back(o);
    } else {
      out.push_back(std::move(v));
    }
  };
  header(0, same_word(orig, rec, 0x04), w(fu, 0x04));
  {
    std::vector<uint8_t> xyz;
    list_of(xyz, {w(fu, 0x08), w(fu, 0x0c), w(fu, 0x10)});
    header(1, same_word(orig, rec, 0x08) && same_word(orig, rec, 0x0c) && same_word(orig, rec, 0x10), xyz);
  }
  header(2, same_word(orig, rec, 0x14), w(fu, 0x14));
  const bool team_given = !fresh && items.size() > 3;
  if (team_given || rec.team() != ai::team_for_name(name)) header(3, same_word(orig, rec, 0x18), w(fu, 0x18));
  // The tags: each known one kept or written again where it stands; old forms
  // dropped when p8 is written; then the groups the file lacks.
  bool done[sizeof kGroups / sizeof kGroups[0]] = {};
  bool pilot_done = false, sound_done = false, vlist_done = false, mech3_done = false;
  const bool p8_new = !same_group(kGroups[3], orig, rec);
  const bool vlist_same = orig.same_vlist(rec);
  bool had_p4 = false;
  for (size_t i = 4; i < items.size(); ++i) {
    if (get32(was + items[i].at) == zrdr::String && i + 1 < items.size())
      had_p4 = had_p4 || string_at(was, was_size, items[i].at) == "p4";
  }
  for (size_t i = 4; i < items.size(); ++i) {
    const bool key = get32(was + items[i].at) == zrdr::String && i + 1 < items.size();
    if (!key) {  // an item that is no key (the loose orders of two c4 records): as it is
      std::vector<uint8_t> o;
      keep(i, o);
      out.push_back(o);
      continue;
    }
    const std::string tag = string_at(was, was_size, items[i].at);
    std::vector<uint8_t> value;
    bool known = false, same = true;
    for (size_t g = 0; g < sizeof kGroups / sizeof kGroups[0]; ++g)
      if (tag == kGroups[g].tag) {
        known = true, done[g] = true;
        same = same_group(kGroups[g], orig, rec);
        if (!same) value = group_value(tag, fu);
      }
    if (old_form(tag)) {
      if (p8_new || (tag == "p4" && !vlist_same)) {
        ++i;  // dropped: p8 (and vlist) carry it now
        continue;
      }
      known = true;
    } else if (tag == "pilot") {
      known = pilot_done = true;
      same = same_text(orig.pilot, rec.pilot) && bit(orig, 0xcc, 7) == bit(rec, 0xcc, 7);
      if (!same && pilot_needed(rec)) value = pilot_value(fu);
    } else if (tag == "sound_tag") {
      known = sound_done = true;
      same = same_text(orig.sound_tag, rec.sound_tag);
      if (!same && rec.sound_tag) value = text_value(rec.sound_tag);
    } else if (tag == "vlist") {
      known = vlist_done = true;
      same = vlist_same && !(had_p4 && p8_new);
      if (!same) value = vlist_value(fu);
    } else if (tag == "mech3") {
      known = mech3_done = true;
      same = orig.mech3_set == mech3_set && orig.mech3 == mech3;
      if (!same) list_of(value, {i32(mech3)});
    }
    if (known && !same && value.empty()) {  // none: the tag dropped (the reader's default then)
      ++i;
      continue;
    }
    std::vector<uint8_t> k;
    keep(i, k);
    out.push_back(k);
    if (!known || same) {
      std::vector<uint8_t> o;
      keep(i + 1, o);
      out.push_back(o);
    } else {
      out.push_back(value);
    }
    ++i;
  }
  // The groups the file lacks, where the record differs from what reading
  // without them gives.
  for (size_t g = 0; g < sizeof kGroups / sizeof kGroups[0]; ++g)
    if (!done[g] && !same_group(kGroups[g], orig, rec)) out.push_back(str(kGroups[g].tag)), out.push_back(group_value(kGroups[g].tag, fu));
  if (!pilot_done && !(same_text(orig.pilot, rec.pilot) && bit(orig, 0xcc, 7) == bit(rec, 0xcc, 7)) &&
      pilot_needed(rec))
    out.push_back(str("pilot")), out.push_back(pilot_value(fu));
  if (!sound_done && !same_text(orig.sound_tag, rec.sound_tag) && rec.sound_tag)
    out.push_back(str("sound_tag")), out.push_back(text_value(rec.sound_tag));
  if (!vlist_done && (!vlist_same || (had_p4 && p8_new))) out.push_back(str("vlist")), out.push_back(vlist_value(fu));
  if (!mech3_done && mech3_set && !(orig.mech3_set == mech3_set && orig.mech3 == mech3)) {
    out.push_back(str("mech3"));
    std::vector<uint8_t> v;
    list_of(v, {i32(mech3)});
    out.push_back(v);
  }
  std::vector<uint8_t> o;
  list_of(o, out);
  return o;
}

std::vector<uint8_t> aiv_with_edits(const uint8_t* p, size_t size, const std::vector<AivEdit>& edits) {
  std::vector<Pair> pairs;
  size_t list_end = 0;
  if (!record_pairs_of(p, size, &pairs, &list_end)) return {};
  std::vector<uint8_t> body;
  std::vector<bool> used(edits.size());
  size_t q = 16;
  for (const Pair& pr : pairs) {
    size_t k = edits.size();
    for (size_t e = 0; e < edits.size(); ++e)
      if (!used[e] && edits[e].name == pr.key) k = e;  // the reader's own match (0x004e0680): exact
    if (k == edits.size()) continue;
    used[k] = true;
    body.insert(body.end(), p + q, p + pr.value);
    body.insert(body.end(), edits[k].value.begin(), edits[k].value.end());
    q = pr.end;
  }
  body.insert(body.end(), p + q, p + list_end);
  uint32_t added = 0;
  for (size_t e = 0; e < edits.size(); ++e)
    if (!used[e]) {
      put_string(body, edits[e].name);
      body.insert(body.end(), edits[e].value.begin(), edits[e].value.end());
      added += 2;
    }
  std::vector<uint8_t> o;
  put32(o, zrdr::List), put32(o, get32(p + 4));
  put32(o, zrdr::List), put32(o, get32(p + 12) + added);
  o.insert(o.end(), body.begin(), body.end());
  o.insert(o.end(), p + list_end, p + size);  // the rest of the top list and whatever follows it
  return o;
}

namespace {
// Each record noted as its value's bytes, written when it was Applied over the
// file as it stood then (no pointer into the mission is kept: its orders'
// valves and regions go with it).
std::map<std::string, std::vector<uint8_t>>& notes() {
  static std::map<std::string, std::vector<uint8_t>> n;
  return n;
}
std::string g_note_error;

// The record's value bytes in the mission's aiv.zrd as it stands (as this run
// last saved it), and where its entry is.
bool record_in_mission(const std::string& name, std::vector<uint8_t>* aiv, std::string* archive, size_t* at, size_t* end) {
  if (!aiv_entry_of_mission(archive, aiv)) return false;
  std::vector<Pair> pairs;
  size_t list_end = 0;
  if (!record_pairs_of(aiv->data(), aiv->size(), &pairs, &list_end)) return false;
  *at = *end = 0;
  for (const Pair& pr : pairs)
    if (pr.key == name) *at = pr.value, *end = pr.end;
  return true;
}
}  // namespace

void aiv_note_edit(const std::string& name, const ai::AiParams& rec, int32_t mech3_set, int32_t mech3) {
  std::vector<uint8_t> aiv;
  std::string archive;
  size_t at = 0, end = 0;
  if (!record_in_mission(name, &aiv, &archive, &at, &end)) {
    g_note_error = "no aiv.zrd in the open archives";
    return;
  }
  std::vector<uint8_t> v = aiv_record_written(end ? aiv.data() + at : nullptr, end - at, rec, mech3_set, mech3, name.c_str());
  if (v.empty()) {
    g_note_error = "record " + name + " does not read";
    return;
  }
  notes()[name] = std::move(v);
}
size_t aiv_noted() { return notes().size(); }
void aiv_notes_clear() { notes().clear(), g_note_error.clear(); }

ai::NetSave aiv_save_edits() {
  ai::NetSave s;
  if (!editor_save_ready([] { aiv_save_edits(); })) return s.error = "the copy's name is asked first", s;  // ours (save_as.h)
  std::string archive;
  std::vector<uint8_t> aiv;
  if (notes().empty()) return s.error = g_note_error.empty() ? "no vehicle applied" : g_note_error, s;
  if (!aiv_entry_of_mission(&archive, &aiv)) return s.error = "no aiv.zrd in the open archives", s;
  std::vector<AivEdit> edits;
  for (const auto& [name, v] : notes()) edits.push_back({name, v});
  const std::vector<uint8_t> out = aiv_with_edits(aiv.data(), aiv.size(), edits);
  if (out.empty()) return s.error = "aiv.zrd does not read as a reader file", s;
  s = ai::save_archive_edit(archive.c_str(), "aiv.zrd", out);
  if (s.ok) aiv_entry_saved(archive, out);
  return s;
}

}  // namespace mech3
