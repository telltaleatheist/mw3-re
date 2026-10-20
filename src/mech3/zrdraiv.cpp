#include "mech3/zrdraiv.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <cmath>

#include "gamez/zutil/zfix.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zutil/zutl_msg.h"
#include "mech3/mech_names.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
const char kFile[] = "C:\\zipper\\Mech3\\ZRdrAiv.cpp";

void report(int level, int line, const std::string& msg) { zerr::report(level, kFile, line, msg.c_str()); }

// The folder part of a path, drive included, as _splitpath + "%s%s" give it.
std::string folder(const char* path) {
  std::string p = path;
  const size_t cut = p.find_last_of("\\/");
  return cut == std::string::npos ? (p.size() > 1 && p[1] == ':' ? p.substr(0, 2) : "") : p.substr(0, cut + 1);
}
}  // namespace

RdrHooks& rdr_hooks() {
  static RdrHooks h;
  return h;
}

// Mech3.exe 1.2 @ 0x004de9c0
RdrFile::RdrFile(const char* n, int32_t max, int32_t a18, int32_t f, int32_t q)
    : max_refs(max), x18(a18), first(f), quiet(q) {
  std::string base = n;
  const size_t dot = base.rfind('.');
  if (dot != std::string::npos) {
    if (base.compare(dot, std::string::npos, ".zrd") == 0)
      report(0x2000, 0x43, std::string(n) + ": suffix is ignored");
    else
      report(0x2000, 0x41, std::string(n) + ": suffix should be \".zrd\" or nothing");
    base.resize(dot);
  }
  base += ".zrd";
  name = strdup(base.c_str());
}

// Mech3.exe 1.2 @ 0x004deb20. The tree is not freed here (unload does that).
RdrFile::~RdrFile() {
  std::free(dir);
  std::free(name);
}

// Mech3.exe 1.2 @ 0x004deb80
int32_t RdrFile::unload() {
  zrdr::free_node(tree);
  tree = nullptr;
  return 1;
}

// Mech3.exe 1.2 @ 0x004deb50
int32_t RdrFile::add_ref() {
  const int32_t was = refs;
  if (was < max_refs) refs = was + 1;
  if (was > 0 || tree) return 0;
  return load();
}

// Mech3.exe 1.2 @ 0x004deba0
int32_t RdrFile::release() {
  const int32_t was = refs;
  if (was > 0) refs = was - 1;
  if (was > 1 || !tree) return 0;
  return unload();
}

// Mech3.exe 1.2 @ 0x004dec80
int32_t RdrFile::load() {
  if (tree) unload();
  tree = zrdr::load(name);
  if (tree) {
    resolve_dir();
    parse();
    return tree != nullptr;
  }
  if (!quiet) report(0x200, 0x9b, std::string("Couldn't read ") + name);
  return 0;
}

// Mech3.exe 1.2 @ 0x004debd0. The exe makes the path absolute (_fullpath)
// before cutting it; FIX (A): the folder of the path as found.
const char* RdrFile::resolve_dir() {
  if (dir) return dir;
  const char* found = rdr_hooks().find_loose ? rdr_hooks().find_loose(name) : nullptr;
  if (found) dir = strdup(folder(found).c_str());
  return dir;
}

// Mech3.exe 1.2 @ 0x004decf0. Only a tree that is one list holding one list
// has records; a dangling last key counts as a record. FIX (bug): the exe
// reads a non-list inner item's value as the list and crashes.
int32_t RdrFile::record_count() const {
  if (!tree || tree->type != zrdr::List || tree->list[0].i != 2) return 0;
  if (tree->list[1].type != zrdr::List) return 0;
  return (tree->list[1].list[0].i - first) >> 1;
}

// Mech3.exe 1.2 @ 0x004ded20
zrdr::Node* RdrFile::record(int32_t i, const char** key) const {
  if (i < 0 || i >= record_count()) return nullptr;
  zrdr::Node* item = &tree->list[1].list[first + 2 * i + 1];
  // FIX (bug): the exe hands back any key's value as the name; a non-string
  // one crashes the callers that copy it.
  *key = item->type == zrdr::String ? item->s : nullptr;
  return item + 1;
}

// Mech3.exe 1.2 @ 0x004ded60
zrdr::Node* RdrFile::find(const char* key) const { return zrdr::find(tree, key); }

// --- aiv.zrd -----------------------------------------------------------------

namespace {
AivFile*& aiv_slot() {
  static AivFile* f = new AivFile;  // 0x0060b5e0, made by the static initializer 0x004df610
  return f;
}
}  // namespace

AivFile& aiv_file() { return *aiv_slot(); }

void aiv_file_reset() {
  AivFile*& f = aiv_slot();
  if (f->tree) f->RdrFile::unload();
  delete f;
  f = new AivFile;
}

// Mech3.exe 1.2 @ 0x004ded70
AivFile::~AivFile() {
  if (cache) {
    for (auto& [k, r] : *cache) delete r;
    delete cache;
  }
}

// Mech3.exe 1.2 @ 0x004df5f0 (0x004df5d0 empties the cache)
int32_t AivFile::unload() {
  if (cache) {
    for (auto& [k, r] : *cache) delete r;
    cache->clear();
  }
  return RdrFile::unload();
}

// Mech3.exe 1.2 @ 0x004def10
AivRecord* AivFile::cached(const char* n) const {
  if (!cache || !n) return nullptr;
  auto it = cache->find(n);
  return it == cache->end() ? nullptr : it->second;
}

// Mech3.exe 1.2 @ 0x004e0490
AivRecord::AivRecord() { aiv_file().add_ref(); }

// Mech3.exe 1.2 @ 0x004e04f0
AivRecord::~AivRecord() { aiv_file().release(); }

// Mech3.exe 1.2 @ 0x004e0600
AivRecord* AivRecord::read(int32_t i, const char** name) {
  zrdr::Node* value = aiv_file().record(i, name);
  if (!value) return nullptr;
  if (AivRecord* c = aiv_file().cached(*name)) {
    AiParams::operator=(*c);
    mech3_set = c->mech3_set, mech3 = c->mech3;
    return this;
  }
  return parse(value, *name);
}

// Mech3.exe 1.2 @ 0x004e0680. The name is found anywhere in the file.
AivRecord* AivRecord::read_named(const char* name) {
  if (AivRecord* c = aiv_file().cached(name)) {
    AiParams::operator=(*c);
    mech3_set = c->mech3_set, mech3 = c->mech3;
    return this;
  }
  return parse(name ? aiv_file().find(name) : nullptr, name);
}

namespace {
uint32_t word(const zrdr::Node& n) {
  if (n.type == zrdr::Int) return static_cast<uint32_t>(n.i);
  if (n.type == zrdr::Float) {
    uint32_t u;
    std::memcpy(&u, &n.f, 4);
    return u;
  }
  return 0;  // FIX (A): a string's or list's pointer bits in the exe
}
float word_f(const zrdr::Node& n) {
  const uint32_t u = word(n);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
float as_float(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
// FIX (bug): the exe takes any value as a string pointer.
const char* text(const zrdr::Node& n) { return n.type == zrdr::String ? n.s : nullptr; }

void* region(const char* name) { return rdr_hooks().find_region ? rdr_hooks().find_region(name) : nullptr; }
const char* localize(const char* s) { return rdr_hooks().localize ? rdr_hooks().localize(s) : zutl::localize(s); }
char* dup(const char* s) { return s ? strdup(s) : nullptr; }

// fcomp's equality: equal, or either one NaN.
bool same(float a, float b) { return a == b || std::isnan(a) || std::isnan(b); }

template <class... A>
void report_f(int line, const char* fmt, A... args) {
  char msg[512];
  std::snprintf(msg, sizeof msg, fmt, args...);
  report(0x2000, line, msg);
}

uint8_t with(uint8_t byte, int bit, bool on) {
  return static_cast<uint8_t>((byte & ~(1u << bit)) | (on ? 1u << bit : 0u));
}
}  // namespace

// Mech3.exe 1.2 @ 0x004df730 (Ghidra splits it at 0x004e001a, 0x004e0020 and
// 0x004e0082). The record is (kind (x y z) yaw [team] tag value ...). The
// tags p3, p4, p5 and p7 are an older form p8 replaced; no shipped file has
// them (docs/ai.md).
AivRecord* AivRecord::parse(zrdr::Node* value, const char* name) {
  if (!value) return nullptr;
  reset();
  to_file_units();
  if (value->type != zrdr::List) {  // FIX (bug): the exe reads it as a list
    finish();
    return this;
  }
  zrdr::Node* L = value->list;
  put_raw(0x04, word(L[1]));
  if (L[2].type == zrdr::List)  // FIX (bug)
    for (int k = 0; k < 3; ++k) put_raw(0x08 + 4 * k, word(L[2].list[k + 1]));
  put_raw(0x14, word(L[3]));
  put<int32_t>(0x18, L[0].i > 4 ? static_cast<int32_t>(word(L[4])) : team_for_name(name));
  mech3_set = 0;
  if (L[0].i > 5) {
    // FIX (bug): the old tags' scratch values are uninitialised stack in the
    // exe when the tag that sets them is absent; 0 here.
    uint32_t scan[4] = {};  // [esp+0x18], [esp+0x1c], [esp+0x20], [esp+0x24]
    uint32_t jump[2] = {};  // [esp+0x34], [esp+0x38]
    bool p3 = false, p4 = false, p5 = false, p7 = false, p8 = false;
    auto find = [&](const char* key) -> zrdr::Node* {
      zrdr::Node* f = zrdr::find(value, key);
      return f && f->type == zrdr::List ? f->list : nullptr;  // FIX (bug): a non-list value is skipped
    };
    auto w = [](const zrdr::Node* f, int k) { return word(f[k]); };
    auto nz = [](const zrdr::Node* f, int k) { return word(f[k]) != 0; };
    zrdr::Node* f;
    if ((f = find("params"))) {
      put_raw(0x1c, w(f, 1)), put_raw(0x3c, w(f, 2));
      for (int k = 3; k <= 9; ++k) put_raw(0x20 + 4 * (k - 3), w(f, k));
    }
    if ((f = find("p2"))) {
      put_raw(0x40, w(f, 1));
      set_bit(0xcc, 0, nz(f, 2)), set_bit(0xcc, 1, nz(f, 3)), set_bit(0xcc, 2, nz(f, 4));
      for (int k = 5; k <= 8; ++k) put_raw(0x44 + 4 * (k - 5), w(f, k));
    }
    if ((f = find("p3"))) {
      p3 = true;
      for (int k = 1; k <= 6; ++k) put_raw(0x64 + 4 * (k - 1), w(f, k));
    }
    if ((f = find("p4"))) {  // the old form of one order, inline
      p4 = true;
      for (int k = 1; k <= 4; ++k) put_raw(0x88 + 4 * (k - 1), w(f, k));
      scan[1] = w(f, 5), scan[0] = w(f, 6), scan[2] = w(f, 7), scan[3] = w(f, 8);
      if (const char* valve_name = text(f[23])) {
        if (!vlist) vlist = new std::list<VListEntry>;
        VListEntry e;  // FIX (bug): uninitialised in the exe but for the bits set here
        uint8_t fl = with(e.flags, 0, true);
        fl = with(fl, 1, nz(f, 9)), fl = with(fl, 2, nz(f, 10));
        e.f00 = word_f(f[11]);
        fl = with(fl, 3, nz(f, 12)), fl = with(fl, 4, nz(f, 13));
        e.f04 = word_f(f[14]);
        fl = with(fl, 5, nz(f, 15));
        e.f08 = word_f(f[16]);
        e.flags2 = with(e.flags2, 0, nz(f, 17));
        e.region = f[18].type == zrdr::String ? region(f[18].s) : nullptr;
        fl = with(fl, 6, nz(f, 19));
        e.f0c = word_f(f[20]);
        fl = with(fl, 7, nz(f, 21));
        e.f10 = word_f(f[22]);
        e.flags = fl;
        e.valve = valve_get(valve_name);  // 0x004d8dc0
        vlist->push_back(e);
      }
    }
    if (zrdr::Node* pilot_node = zrdr::find(value, "pilot")) {
      if (pilot_node->type == zrdr::List && pilot_node->list[1].type == zrdr::String)
        std::free(pilot), pilot = dup(localize(pilot_node->list[1].s));
      bool configure = (raw[0xcc] >> 7) != 0;
      zrdr::get_bool(pilot_node, "can_configure", &configure);
      set_bit(0xcc, 7, configure);
    }
    if ((f = find("p5"))) {
      p5 = true;
      put_raw(0x7c, w(f, 1)), put_raw(0x84, w(f, 2)), put_raw(0x80, w(f, 3));
      jump[1] = w(f, 4), jump[0] = w(f, 5);
    }
    if ((f = find("p6")))
      for (int k = 1; k <= 4; ++k) put_raw(0x54 + 4 * (k - 1), w(f, k));
    if ((f = find("p7"))) {
      p7 = true;
      for (int k = 1; k <= 4; ++k) put_raw(0x88 + 4 * (k - 1), w(f, k));
      scan[1] = w(f, 5), scan[0] = w(f, 6), scan[2] = w(f, 7), scan[3] = w(f, 8);
    }
    if ((f = find("p8"))) {
      p8 = true;
      put_raw(0x64, w(f, 1)), put_raw(0x68, w(f, 2)), put_raw(0x6c, w(f, 3)), put_raw(0x70, w(f, 4));
      put_raw(0x80, w(f, 5)), put_raw(0x74, w(f, 6)), put_raw(0x78, w(f, 7)), put_raw(0x7c, w(f, 8));
      put_raw(0x84, w(f, 9));
      for (int k = 10; k <= 17; ++k) put_raw(0x88 + 4 * (k - 10), w(f, k));
    }
    if ((f = find("p9"))) {
      put_raw(0xa8, w(f, 1)), put_raw(0xac, w(f, 2)), put_raw(0xb0, w(f, 3));
      set_bit(0xcc, 3, nz(f, 4));
      put_raw(0xb4, w(f, 5));
    }
    if ((f = find("p10"))) put_raw(0xb8, w(f, 1)), put_raw(0xbc, w(f, 2));
    if ((f = find("p11"))) put_raw(0xc0, w(f, 1));
    if ((f = find("p12"))) {
      const char* mech_name = text(f[1]);
      put<int32_t>(0xc4, mech_name ? mech_index(mech_name) : -1);  // FIX (bug): the exe crashes on none
      set_bit(0xcc, 4, nz(f, 2)), set_bit(0xcc, 5, nz(f, 3));
    }
    if ((f = find("p13"))) set_bit(0xcc, 6, nz(f, 1));
    if (zrdr::Node* tag = zrdr::find(value, "sound_tag")) {
      const char* s = nullptr;
      if (zrdr::get_string_value(tag, &s)) std::free(sound_tag), sound_tag = dup(s);
    }
    // The three lancemates' radio voices when the file names none: the record
    // must be spelled just so (docs/discoveries.md).
    if (!sound_tag && name) {
      if (std::strcmp(name, "Epona_Rhi") == 0)
        sound_tag = dup("epona");
      else if (std::strcmp(name, "Alan_Mattila") == 0 ||
               // FIX (bug): two records misspell him (c1 Instant Action map 1,
               // c3 mission 6), so he had no radio voice there (Owen, 2026-10-20).
               (zfix::options().bugs && (std::strcmp(name, "Alan_Mattilla") == 0 || std::strcmp(name, "Alan_Matilla") == 0)))
        sound_tag = dup("alan");
      else if (std::strcmp(name, "Dominic_Paine") == 0)
        sound_tag = dup("dom");
    }
    if ((f = find("skin"))) put_raw(0xc8, w(f, 1));
    if (zrdr::Node* lm = zrdr::find(value, "lancemate")) {
      int32_t v = 0;
      if (zrdr::get_tristate(lm, &v)) set_bit(0xcd, 1, (v >> 1) & 1), set_bit(0xcd, 0, v & 1);
    }
    auto add_order = [&](const zrdr::Node* s) {
      VListEntry e;  // 0x004d8d30
      uint8_t fl = e.flags;
      fl = with(fl, 1, nz(s, 1)), fl = with(fl, 2, nz(s, 2));
      e.f00 = word_f(s[3]);
      fl = with(fl, 3, nz(s, 4)), fl = with(fl, 4, nz(s, 5));
      e.f04 = word_f(s[6]);
      fl = with(fl, 5, nz(s, 7));
      e.f08 = word_f(s[8]);
      e.flags2 = with(e.flags2, 0, nz(s, 9));
      if (s[0].i >= 0x12) e.flags2 = with(e.flags2, 1, nz(s, 17));
      const char* region_name = text(s[10]);
      fl = with(fl, 6, nz(s, 11));
      e.f0c = word_f(s[12]);
      fl = with(fl, 7, nz(s, 13));
      e.f10 = word_f(s[14]);
      e.flags = with(fl, 0, nz(s, 15));
      const char* valve_name = text(s[16]);
      e.region = region_name ? region(region_name) : nullptr;
      e.valve = valve_name ? valve_get(valve_name) : nullptr;
      if (!vlist) vlist = new std::list<VListEntry>;  // 0x004d9480
      vlist->push_back(e);
    };
    if ((f = find("vlist")) && f[0].i > 1) {
      for (int32_t k = 1; k < f[0].i; ++k) {
        if (f[k].type != zrdr::List) continue;  // FIX (bug)
        add_order(f[k].list);
      }
    } else if (zfix::options().bugs) {
      // FIX (bug): two Instant Action records on c4 (Enemy_12 in readeria1,
      // Enemy_06 in readeria2) list their orders loose in the record, with no
      // "vlist" around them; the exe skips them (Owen, 2026-10-20).
      for (int32_t k = 5; k < L[0].i; ++k)
        if (L[k].type == zrdr::List && L[k - 1].type != zrdr::String && L[k].list[0].i >= 0x12 &&
            L[k].list[1].type != zrdr::String)
          add_order(L[k].list);
    }
    if ((f = find("mech3"))) mech3_set = 1, mech3 = nz(f, 1);
    if (p8 && (p3 || p4 || p5 || p7))
      report_f(0x357, "record \"%s\": tag p8 is incompatible with tags p3, p4, p5, and p7", name);
    if (!p8) {
      const int which = (p3 ? 1 : 0) | (p7 ? 2 : 0);
      if (which == 1) {
        put_raw(0x64, scan[0]), put_raw(0x68, scan[1]), put_raw(0x6c, scan[2]), put_raw(0x70, scan[3]);
      } else if (which == 3) {
        if (!(same(get<float>(0x64), as_float(scan[0])) && same(get<float>(0x68), as_float(scan[1])) &&
              same(get<float>(0x6c), as_float(scan[2])) && same(get<float>(0x70), as_float(scan[3]))))
          report_f(0x36b, "%s: p3 scan parameters are ignored in favor of p7 parameters", name);
      }
      if (p5) {
        switch (jump[0] + jump[1]) {
          case 1:
            put<int32_t>(0xa4, 0);
            break;
          case 2:
            put<int32_t>(0xa4, 1);
            break;
          case 3:
            report_f(0x375, "%s: p3: setting both scan and jump is not allowed, ignoring jump", name);
            put<int32_t>(0xa4, 0);
            break;
          default:
            break;
        }
      }
    }
  }
  finish();
  return this;
}

// --- targets.zrd and traps.zrd ---------------------------------------------------

namespace {
RecordFile*& targets_slot() {
  static RecordFile* f = new RecordFile("targets");  // 0x0060b608, 0x004e0810
  return f;
}
RecordFile*& traps_slot() {
  static RecordFile* f = new RecordFile("traps");  // 0x0060b630, 0x004e0710
  return f;
}
}  // namespace

RecordFile& targets_file() { return *targets_slot(); }
RecordFile& traps_file() { return *traps_slot(); }

void record_files_reset() {
  for (RecordFile** slot : {&targets_slot(), &traps_slot()}) {
    if ((*slot)->tree) (*slot)->unload();
    const std::string base((*slot)->name, std::strlen((*slot)->name) - 4);
    delete *slot;
    *slot = new RecordFile(base.c_str());
  }
}

// Mech3.exe 1.2 @ 0x004e0890
TargetRec::~TargetRec() { delete health_below; }

// Mech3.exe 1.2 @ 0x004e08c0 (through 0x004e07e0, which returns the record or
// none). The defaults first, the old list freed; a key's value that is not a
// list is passed over (FIX (bug): the exe reads it as one).
TargetRec* TargetRec::parse(zrdr::Node* value, const char* record) {
  delete health_below;
  kind = 2, name = nullptr, health = 100.0f, destroy_anim = target = centroid = nullptr;
  for (int k = 0; k < 3; ++k) target_offset[k] = centroid_offset[k] = 0;
  health_below = nullptr, height = 0, flags = 0x30;
  if (!value) return nullptr;
  if (value->type != zrdr::List) return this;  // FIX (bug)
  if (value->list[0].i > 1) kind = static_cast<int32_t>(word(value->list[1]));
  auto find = [&](const char* key) -> zrdr::Node* {
    zrdr::Node* f = zrdr::find(value, key);
    return f && f->type == zrdr::List ? f->list : nullptr;
  };
  zrdr::Node* f;
  if ((f = find("health"))) flags |= 0x01, health = word_f(f[1]);
  if (zrdr::find(value, "translate_to_anim_root")) flags |= 0x02;
  if ((f = find("destroy_anim"))) destroy_anim = text(f[1]);
  if ((f = find("target"))) {
    if (f[1].type == zrdr::String) target = f[1].s;
    if (f[0].i > 2) zrdr::get_vec3(&f[2], target_offset);
  }
  if ((f = find("centroid"))) {
    if (f[1].type == zrdr::String) centroid = f[1].s;
    if (f[0].i > 2) zrdr::get_vec3(&f[2], centroid_offset);
  }
  if ((f = find("name")) && text(f[1])) name = localize(f[1].s);
  struct { const char* key; uint8_t bit; } kBools[] = {
      {"radar", 0x04}, {"display", 0x08}, {"reticule_select", 0x10}, {"zoom_select", 0x20}};
  for (const auto& b : kBools) {
    int32_t v = 0;
    if ((f = find(b.key)) && zrdr::get_bool_int(&f[1], &v))
      flags = static_cast<uint8_t>((flags & ~b.bit) | (v == 1 ? b.bit : 0));
  }
  if (zrdr::Node* h = zrdr::find(value, "height")) zrdr::get_float_value(h, &height);
  if ((f = find("health_below")) && f[0].i > 1) {
    for (int32_t i = 1; i < f[0].i; i += 3) {
      HealthBelow hb;
      hb.valve = valve_get(text(f[i]));
      hb.a = static_cast<int32_t>(word(f[i + 1]));
      hb.health = word_f(f[i + 2]);
      if (!hb.valve) {
        const char* v = text(f[i]);
        char msg[512];
        std::snprintf(msg, sizeof msg, "%s: %s: could not find a valve named \"%s\"", record ? record : "(null)",
                      "health_below", v ? v : "(null)");
        report(0x200, 0x5ec, msg);
        continue;
      }
      if (!health_below) health_below = new std::vector<HealthBelow>;  // 0x004d1f60
      health_below->push_back(hb);                                      // 0x004d1f80
    }
  }
  return this;
}

}  // namespace mech3::ai
