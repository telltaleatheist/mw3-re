#pragma once
#include <cstdint>
#include <cstdio>

// The model library reader for mechlib.zbd (docs/mechlib.md). Original file
// unknown: it sits between zClass\object3d.c and zClass\switch.c.
namespace zcls {

struct Polygon {                 // 0x24 bytes in the file
  uint32_t flags;                // +0x00  bits 0-7 vertex count, 0x200 normals
  int32_t priority;              // +0x04
  uint32_t* vertex_indices;      // +0x08
  uint32_t* normal_indices;      // +0x0c
  float* uvs;                    // +0x10  2 per vertex
  float* colors;                 // +0x14  3 per vertex
  uint32_t* scratch;             // +0x18  allocated, never read from the file
  uint32_t material;             // +0x1c  index, later the resolved material
  uint32_t zone_set;             // +0x20
  uint32_t file_ptrs[5];         // the file's pointer fields, used as "present" flags
};

struct Model {                   // 0x5c bytes in the file, from the pool 0x00538fe0
  uint8_t header[0x5c];          // as read; counts at +0x10..+0x20
  uint32_t polygon_count, vertex_count, normal_count, morph_count;
  Polygon* polygons;             // +0x34
  float* vertices;               // +0x38
  float* normals;                // +0x3c
  float* morphs;                 // +0x44
};

struct Material {                // 0xc bytes
  uint8_t* raw;                  // +0x00  the 0x28 file bytes
  bool resolved;                 // +0x04
  char* texture;                 // +0x08
};

bool read_material(std::FILE* f, Material& m);   // 0x004fc570
Model* read_model(std::FILE* f);                 // 0x004fcf20, when node +0x3c != 0
void free_model(Model* m);

}  // namespace zcls
