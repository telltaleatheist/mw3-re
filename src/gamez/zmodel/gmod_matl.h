#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "gamez/zimage/zimg_texture.h"

// World materials (zModel\gmod_matl.c): the table a gamez.zbd fills, which
// mechlib materials are later added to (docs/gamez.md).
namespace zmod {

struct Cycle {                     // 0x1c bytes
  uint32_t looping;                // +0x00
  uint32_t frame;                  // +0x04
  float index;                     // +0x08
  float speed;                     // +0x0c
  uint32_t texture_count;          // +0x10
  uint32_t added;                  // +0x14  slots filled by cycle_add
  zimg::TextureEntry** textures;   // +0x18
};

struct Material {                  // 0x2c bytes
  uint8_t alpha;                   // +0x00
  uint8_t flags;                   // +0x01  1 textured, 4 cycled, 0x10 referenced, 0x20 free
  uint16_t rgb;                    // +0x02  display-packed colour
  float r, g, b;                   // +0x04
  zimg::TextureEntry* texture;     // +0x10  an index in the file
  uint32_t texture_index;          // the file's value, kept for coloured materials
                                   // (the exe's +0x10 then: material_differs compares it)
  float unk_14[3];                 // +0x14
  uint32_t soil;                   // +0x20
  Cycle* cycle;                    // +0x24
  uint32_t file_cycle;             // the file's stale pointer (the exe's +0x24 without a
                                   // cycle; 0 on materials made at run time)
  int16_t prev, next;              // +0x28, +0x2a
};

constexpr int32_t kMaterialBlock = 1000;  // 0x005edd50

struct MaterialTable {
  // FIX (A): owned blocks for the exe's realloc'd array of malloc'd ones;
  // a material keeps its address as the table grows.
  std::vector<std::unique_ptr<Material[]>> blocks;  // 0x0063fbc8, count 0x0063fbc0
  int32_t capacity = 0;            // 0x0063fbc4
  int32_t count = 0;               // 0x0063fbcc
  int32_t free_head = -1;          // 0x005edd54
  int32_t used_head = -1;          // 0x005edd58
  Material* last = nullptr;        // 0x0063fbd0  the last one material_add made
  Material& at(int32_t i) { return blocks[i / kMaterialBlock][i % kMaterialBlock]; }
  const Material& at(int32_t i) const { return blocks[i / kMaterialBlock][i % kMaterialBlock]; }
  int32_t index_of(const Material* m) const;  // -1 when not in the table
};
MaterialTable& materials();

int read_materials(std::FILE* f);  // 0x005363d0
Material* take_material(int32_t index);  // 0x00536070, marks it referenced
// The table's equal material (0x00536f10: all but the bookkeeping flags; a
// soil of 0 on either side takes the other's), else a new one copied from
// `m` (a cycle copied too); referenced (flag 0x10) either way. The last one
// made is tried first.
Material* material_add(Material& m);       // 0x00536ad0
Material* material_copy(const Material& m);  // 0x00537830: always a new one
int material_differs(Material& entry, Material& m);  // 0x00536f10 ECX entry, EDX m
// Flag 2 or 4, or a cycle: a material each copy of a model must own.
int material_changes(const Material& m);     // 0x00536ab0

Material& default_material();                // 0x0063fb98
void material_init(Material& m);             // 0x00536a70: white, opaque, untextured
int material_set_soil(Material* m, uint32_t soil);  // 0x005375c0
// 0x005375d0: makes m cycled (flags 5) with `count` slots of the default
// texture; 0 for the default material or a cycle already that long.
int cycle_create(Material& m, int32_t count);
int cycle_add(Material& m, zimg::TextureEntry* t);  // 0x00537680: the next slot
int cycle_set_loop(Material* m, uint32_t v);        // 0x005377a0
int cycle_set_speed(Material* m, float v);          // 0x005377e0
// 0x00537be0: the used material showing texture t, marked referenced.
Material* material_find_texture(zimg::TextureEntry* t);

// The soil names a material's +0x20 indexes (0x005edbc0, count 0x005edbb8,
// at most 100): the engine's six, then the mission script's `LoadSoils
// soils.zrd` adds dirt, mud, grass, concrete, snow, mech, silt and noslip
// (6..13). zWeapon reads one IMPACT block per name (docs/weapons.md).
// FIX (A): std::string for the exe's malloc'd copies.
std::vector<std::string>& soil_names();
int32_t soil_index(const char* name);  // 0x00535f20: a whole-word, case-blind prefix match, -1 none
int load_soils(const char* file);      // 0x00537cb0, the LoadSoils command
int reset_soils();                     // 0x00537270 (0x00537350 too): back to the six

}  // namespace zmod
