#pragma once
#include <cstdint>
#include <cstdio>
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
  uint32_t unk_14;                 // +0x14
  zimg::TextureEntry** textures;   // +0x18
};

struct Material {                  // 0x2c bytes
  uint8_t alpha;                   // +0x00
  uint8_t flags;                   // +0x01  1 textured, 4 cycled, 0x10 referenced, 0x20 free
  uint16_t rgb;                    // +0x02  display-packed colour
  float r, g, b;                   // +0x04
  zimg::TextureEntry* texture;     // +0x10  an index in the file
  uint32_t texture_index;          // the file's value, kept for coloured materials
  float unk_14[3];                 // +0x14
  uint32_t soil;                   // +0x20
  Cycle* cycle;                    // +0x24
  uint32_t file_cycle;             // the file's stale pointer
  int16_t prev, next;              // +0x28, +0x2a
};

struct MaterialTable {
  std::vector<Material> slots;     // blocks of 1000 at 0x0063fbc8
  int32_t capacity = 0;            // 0x0063fbc4
  int32_t count = 0;               // 0x0063fbcc
  int32_t free_head = -1;          // 0x005edd54
  int32_t used_head = -1;          // 0x005edd58
};
MaterialTable& materials();

int read_materials(std::FILE* f);  // 0x005363d0
Material* take_material(int32_t index);  // 0x00536070, marks it referenced

}  // namespace zmod
