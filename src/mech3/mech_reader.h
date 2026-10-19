#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace zrdr { struct Node; }

// mech3\mech_reader.cpp: a mech's definition from its dfn_<mech>.zrd
// (docs/mechdef-skins.md, docs/mechdef-parts.md). The reader 0x0046ba40 is
// ported a block at a time.
namespace mech3 {

struct SkinSet;

struct Firepoint {                 // 0x20 bytes
  const char* name;                // +0x00  borrowed from the reader tree
  float radius;                    // +0x04  scatter of salvo shots
  float share;                     // +0x08  offset of a second weapon on it
  int32_t part;                    // +0x0c  from the file's "part"
  int32_t model_part = -1;         // +0x10  set later, from the model (0x0046cb80)
  float position[3] = {};          // +0x14
};

struct PartSlot {                  // 0x10 bytes
  const char* name = nullptr;
  float f4 = 1.0f;                 // meaning unknown
  int32_t i8 = -1;                 // meaning unknown
  int32_t id = -1;
  bool set(int32_t id, zrdr::Node* unused);  // 0x0046ba20
};

struct MechDfn {                   // 0x1a8 bytes
  uint8_t raw[0x1a8] = {};         // the plain fields; the members below replace the pointers
  int32_t index = 0;               // +0x00
  uint8_t loaded = 0;              // +0x04
  zrdr::Node* reader = nullptr;    // +0x08
  SkinSet* skins = nullptr;        // +0x1c
  void* configurations = nullptr;  // +0x20
  std::vector<Firepoint*>* firepoints = nullptr;  // +0x24
  PartSlot parts[18];              // +0x28  by part id
  MechDfn(int32_t index, zrdr::Node* reader);    // 0x0046b890
  template <typename T> T get(int off) const {
    T v;
    std::memcpy(&v, raw + off, sizeof v);
    return v;
  }
  template <typename T> void put(int off, T v) { std::memcpy(raw + off, &v, sizeof v); }
};

// Blocks of 0x0046ba40, in its order.
void read_firepoints(MechDfn& dfn);   // 0x0046bfd8..0x0046c13a
void read_part_slots(MechDfn& dfn);   // 0x0046c13a..0x0046c1f0

}  // namespace mech3
