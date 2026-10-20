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
  // A mechlib polygon's +0x1c until the library resolves it (0x004fcd40):
  // the index in mechlib's own material list.
  uint32_t material_index = 0;
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

Model* model_at(int32_t index);               // 0x005380c0
// zModel's start (0x00526800's pool part): `size` slots ([0x007402e0], 1750
// when unset), all free. A world's gamez.zbd then replaces the pool.
void reset_pool();
// A free slot, its header cleared and +8 = 3 (flag 0x10 off). An empty pool
// stops the game ("GameZ model resource failure").
Model* model_alloc();                         // 0x00538fe0
int read_models(std::FILE* f);                // 0x00538e30
int read_model_data(std::FILE* f, Model& m);  // 0x00538a70
int model_release(Model* m);                  // 0x00539150, 1 while used (+0xc)
// A new pool model with m's arrays copied (its use count 0, its sphere at
// +0x48 not). With `own_materials` each polygon's material is copied too
// (one copy per material of m), unless `shared` and the material does not
// change (0x00536ab0); without, they are m's.
Model* model_copy(const Model& m, int32_t own_materials, int32_t shared);  // 0x005396c0 ECX m, EDX, stack
// Flags 4, 8 or 0x10, or a material that changes: a model a copy may not share.
int model_changes(const Model& m);            // 0x0053b3c0

// The texture cycle of the model's first polygon's material (gmod_const.c):
// back to its first texture, or to frame n (past the end it wraps, below 0
// it is 0). No cycle: nothing.
void cycle_reset(Model* m);                   // 0x0053bc10
void cycle_set_frame(Model* m, int32_t n);    // 0x0053bc90

// A model's box (min xyz, max xyz) over its vertices, its lights' points
// and its fully morphed vertices, then its bounding centre (+0x48) and
// radius (+0x54, a square root by halving the float's bits). A facade
// (type 1) turns the box into one centred on the model's origin. `header` is
// the model's 0x5c bytes; mechlib models (zcls::Model) have no lights.
// Box untouched when the model has neither vertices nor lights.
void model_box(uint8_t* header, const float* vertices, const Light* lights, const float* morphs,
               float box[6]);              // 0x0053b4e0 ECX model, EDX box (0x0053b680 the extent)
inline void model_box(Model* m, float box[6]) { model_box(m->header, m->vertices, m->lights, m->morphs, box); }

}  // namespace zmod
