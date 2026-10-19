#include "mech3/mech_skin.h"

#include <cstring>
#include <string>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_msg.h"

namespace mech3 {

namespace {
SkinGlobals g_skins;

char* dup(const char* s) {
  char* d = new char[std::strlen(s) + 1];
  std::strcpy(d, s);
  return d;
}
}  // namespace

SkinGlobals& skin_globals() { return g_skins; }

// Mech3.exe 1.2 @ 0x00473a30. With no alternate the two names share one copy.
SkinEntry::SkinEntry(const char* n, const char* a) {
  if (n) name = dup(n);
  alt = a ? dup(a) : name;
  g_skins.entries.push_back(this);
  ++g_skins.entry_count;
}

// Mech3.exe 1.2 @ 0x00473ca0. Every list item naming this entry is dropped.
SkinEntry::~SkinEntry() {
  unload();
  if (alt && alt != name) delete[] alt;
  delete[] name;
  name = alt = nullptr;
  for (auto it = g_skins.entries.begin(); it != g_skins.entries.end();) {
    if (*it == this) {
      it = g_skins.entries.erase(it);
      --g_skins.entry_count;
    } else {
      ++it;
    }
  }
}

// Mech3.exe 1.2 @ 0x00473e10. Only while +0x14 is set.
void SkinEntry::unload() {
  if (!texture || !b14) return;
  void (*fn)(void*) = cached ? g_skins.release_cached : g_skins.release_texture;
  if (fn) fn(texture);
  texture = nullptr;
}

// Mech3.exe 1.2 @ 0x00473c60
bool release(SkinEntry* e) {
  if (!e) return true;
  if (--e->refs != 0) return false;
  delete e;
  return true;
}

// Mech3.exe 1.2 @ 0x00473e50. No entries yet; `count` is only reserved.
Skin::Skin(const char* n, uint32_t count) : name(dup(n)) {
  entries.reserve(count);
}

// Mech3.exe 1.2 @ 0x00474000
void Skin::add_entry(const char* n, const char* a) { entries.push_back(new SkinEntry(n, a)); }

// Mech3.exe 1.2 @ 0x004744a0. The exe passes an argument the entries ignore.
void Skin::unload() {
  if (entries.empty() || !loaded) return;
  for (SkinEntry* e : entries) e->unload();
  loaded = 0;
}

// Mech3.exe 1.2 @ 0x00473f40
Skin::~Skin() {
  unload();
  for (SkinEntry*& e : entries) release(e), e = nullptr;
  entries.clear();
  delete[] name;
  name = nullptr;
}

// Mech3.exe 1.2 @ 0x00474500. Each base name plus the suffix (64-byte buffers
// in the exe, unchecked); an alternate only where the base has its own.
Skin* Skin::derive(const char* n, const char* suffix) {
  if (entries.empty()) return nullptr;
  Skin* s = new Skin(n, static_cast<uint32_t>(entries.size()));
  for (const SkinEntry* e : entries) {
    const std::string main = std::string(e->name) + suffix;
    if (e->alt != e->name) {
      s->add_entry(main.c_str(), (std::string(e->alt) + suffix).c_str());
    } else {
      s->add_entry(main.c_str(), nullptr);
    }
  }
  return s;
}

// Mech3.exe 1.2 @ 0x00474f60. The first set built in damage mode fetches the
// bullet-hole textures.
SkinSet::SkinSet(int32_t count) {
  if (count != 0) skins.reserve(count > 0 ? count : 0);
  if (g_skins.damage_mode && g_skins.holes.empty()) {
    static const char* const kHoles[] = {"hole_8x8",   "hole_8x8a",   "hole_8x8b",   "hole_8x8c",
                                         "hole_16x16", "hole_16x16a", "hole_16x16b", "hole_16x16c"};
    for (const char* h : kHoles)
      if (void* t = g_skins.fetch_texture ? g_skins.fetch_texture(h) : nullptr) g_skins.holes.push_back(t);
  }
  ++g_skins.sets_built;
}

// Mech3.exe 1.2 @ 0x004751a0. The last set gone releases the bullet holes.
SkinSet::~SkinSet() {
  for (Skin*& s : skins) delete s, s = nullptr;
  skins.clear();
  delete damage;
  damage = nullptr;
  if (--g_skins.sets_built == 0) {
    for (void* t : g_skins.holes)
      if (g_skins.release_cached) g_skins.release_cached(t);
    g_skins.holes.clear();
  }
}

// Mech3.exe 1.2 @ 0x004752e0. A second "damage" skin replaces the first.
bool SkinSet::add(Skin* skin) {
  if (!skin || !skin->name) return false;
  if (std::strcmp(skin->name, "damage") == 0) {
    damage = skin;
    return true;
  }
  skins.push_back(skin);
  return true;
}

// The first pair is the full skin whatever its name; every later one except
// "damage" is the first skin plus its suffix. Values are not type-checked.
SkinSet* read_skins(zrdr::Node* dfn) {
  zrdr::Node* found = zrdr::find(dfn, "skins");
  if (!found) return nullptr;
  zrdr::Node* list = found->list;
  const int32_t pairs = (list[0].i - 1) / 2;
  auto* set = new SkinSet(pairs);
  for (int32_t i = 0; i < pairs; ++i) {
    const char* name = zutl::localize(list[1 + 2 * i].s);
    zrdr::Node* items = list[2 + 2 * i].list;
    if (i == 0 || std::strcmp(name, "damage") == 0) {
      int32_t n = items[0].i - 1;
      int32_t first = 1;
      // FIX (bug): the Mad Cat's damage list has two zeros too many and the
      // Supernova's one, shifting every mask off its texture; leading empty
      // slots are dropped until the list is as long as the first skin's.
      if (zfix::options().bugs && i != 0 && !set->skins.empty()) {
        const auto want = static_cast<int32_t>(set->skins[0]->entries.size());
        while (n > want && items[first].type == zrdr::Int) ++first, --n;
      }
      Skin* skin = new Skin(name, static_cast<uint32_t>(n));
      set->add(skin);
      for (int32_t k = first; k < first + n; ++k) {
        const zrdr::Node& item = items[k];
        if (item.type == zrdr::Int) skin->add_entry(nullptr, nullptr);
        else if (item.type == zrdr::String) skin->add_entry(item.s, nullptr);
        else if (item.type == zrdr::List) skin->add_entry(item.list[1].s, item.list[2].s);
      }
    } else {
      // The exe calls derive on a null base when the set is empty (never in the data).
      Skin* base = set->skins.empty() ? nullptr : set->skins[0];
      if (Skin* skin = base ? base->derive(name, items[1].s) : nullptr) set->add(skin);
    }
  }
  return set;
}

}  // namespace mech3
