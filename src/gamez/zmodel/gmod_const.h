#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

#include "gamez/zmodel/gmod_matl.h"

// The world model pool (zModel\gmod_const.c) that gamez.zbd fills (docs/gamez.md).
namespace zmod {

struct Light {                   // 0x4c bytes
  uint8_t raw[0x4c];             // as read; colour floats at +0x1c/+0x20/+0x24
  uint16_t color;                // +0x28  display-packed
  float* vertices;               // +0x2c  count at +0x0c
};

struct Polygon {                 // 0x24 bytes
  uint32_t flags;                // +0x00  bits 0-7 vertex count, 0x200 normals
  int32_t priority;              // +0x04
  uint32_t* vertex_indices;      // +0x08
  uint32_t* normal_indices;      // +0x0c
  float* uvs;                    // +0x10  when the material is textured
  float* colors;                 // +0x14  always
  uint32_t* scratch;             // +0x18
  Material* material;            // +0x1c  an index in the file
  uint32_t zone_set;             // +0x20
};

struct Model {                   // 0x60-byte slot
  uint8_t header[0x5c];          // as read; counts at +0x0c..+0x20
  uint32_t link;                 // +0x5c  free link, or the data's file offset
  Polygon* polygons = nullptr;   // +0x34
  float* vertices = nullptr;     // +0x38
  float* normals = nullptr;      // +0x3c
  Light* lights = nullptr;       // +0x40
  float* morphs = nullptr;       // +0x44
  uint32_t count(int off) const;
};

struct ModelPool {
  std::vector<Model> slots;      // 0x007402e4
  int32_t size = 0;              // 0x007402e0
  int32_t count = 0;             // 0x007402e8
  int32_t free_head = -1;        // 0x007402ec
};
ModelPool& models();

int read_models(std::FILE* f);                // 0x00538e30
int read_model_data(std::FILE* f, Model& m);  // 0x00538a70

}  // namespace zmod
