#pragma once
#include <cstdint>
#include <list>
#include <vector>

namespace zrdr { struct Node; }

// mech3\mech_skin.cpp: a mech's camos, each a list of texture slots
// (docs/mechdef-skins.md).
namespace mech3 {

struct SkinEntry {                 // 0x1c bytes
  void* texture = nullptr;         // +0x00  0 until loaded
  uint8_t cached = 0;              // +0x04  texture came from the zImage cache
  char* name = nullptr;            // +0x08
  char* alt = nullptr;             // +0x0c  the same pointer as name when none was given
  SkinEntry* damage = nullptr;     // +0x10
  uint8_t b14 = 0;                 // +0x14
  int32_t refs = 1;                // +0x18
  SkinEntry(const char* name, const char* alt);  // 0x00473a30
};

struct Skin {                      // 0x18 bytes
  char* name = nullptr;            // +0x00
  std::vector<SkinEntry*> entries; // +0x08
  uint8_t loaded = 0;              // +0x14
  Skin(const char* name, uint32_t count);              // 0x00473e50
  void add_entry(const char* name, const char* alt);   // 0x00474000
  Skin* derive(const char* name, const char* suffix);  // 0x00474500
};

struct SkinSet {                   // 0x14 bytes
  std::vector<Skin*> skins;        // +0x04  every skin but "damage", in file order
  Skin* damage = nullptr;          // +0x10
  explicit SkinSet(int32_t count); // 0x00474f60
  bool add(Skin* skin);            // 0x004752e0
};

struct SkinGlobals {
  std::list<SkinEntry*> entries;   // 0x006061bc  every live entry
  int32_t entry_count = 0;         // 0x006061c0
  std::vector<void*> holes;        // 0x006061a8  the eight bullet-hole textures
  int32_t sets_built = 0;          // 0x006061c8
  uint8_t damage_mode = 0;         // 0x006061cc  set at mission load
  void* (*fetch_texture)(const char* name) = nullptr;  // 0x005179d0, zImage's cache (not ported)
};
SkinGlobals& skin_globals();

// The skins block of the mech definition reader (0x0046ba40, mech_reader.cpp,
// 0x0046be07..0x0046bfd8), until that reader is ported: MechDfn +0x1c.
SkinSet* read_skins(zrdr::Node* dfn);

}  // namespace mech3
