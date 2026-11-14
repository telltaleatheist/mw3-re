#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "../../../gamez/zmodel/gmod_const.h"
#include "../../../gamez/zmodel/gmod_matl.h"

// C:\zipper\branchep1\gamez\zModel\gmod_const.c: Pirate's Moon's world models
// (docs/pm-map.md, gamez.zbd 41). PM's own types, not MW3's: a polygon has a
// list of materials (UVs per material), a model a list of the materials it
// uses. Read as the file has them; the Direct3D strip conversion PM ran next
// (0x00570b70) is left out (pm-map.md, the models' form).
namespace pm::zmod {

struct Polygon {                       // 0x28 bytes
  uint32_t flags;                      // +0x00  bits 0-9 vertex count, 0x400 back face, 0x1000 normals, 0x2000 strip
  int32_t priority;                    // +0x04
  uint32_t* vertex_indices = nullptr;  // +0x08
  uint32_t* normal_indices = nullptr;  // +0x0c  with 0x1000
  int32_t material_count;              // +0x10
  ::zmod::Material** materials = nullptr;  // +0x14  indices in the file, offset by the file's material base
  float** uvs = nullptr;               // +0x18  one array per material, when the file's +0x18 is set
  float* colors = nullptr;             // +0x1c  always
  uint32_t* scratch = nullptr;         // +0x20
  uint32_t zone_set;                   // +0x24
  uint32_t file_uvs;                   // the file's +0x18 (0: no UVs)
  // mechlib's: the +0x14 array as read, the library's material indices, until
  // mechlib_resolve makes them `materials` (the exe overwrites them in place).
  int32_t* library_indices = nullptr;
};

struct MaterialRef {                   // 0x0c bytes
  ::zmod::Material* material;          // +0x00  an index in the file
  int32_t usage_count;                 // +0x04  polygons
  Polygon* polygon;                    // +0x08  the first of them
};

struct Model {                         // 0x68-byte slot
  uint8_t header[0x64] = {};           // as read: flags +0x08, counts +0x0c..+0x20, material refs +0x5c
  int32_t link = 0;                    // +0x64  the free list
  Polygon* polygons = nullptr;         // +0x34
  float* vertices = nullptr;           // +0x38
  float* normals = nullptr;            // +0x3c
  ::zmod::Light* lights = nullptr;     // +0x40
  float* morphs = nullptr;             // +0x44
  MaterialRef* refs = nullptr;         // +0x60
  int32_t count(int off) const {
    int32_t v;
    std::memcpy(&v, header + off, 4);
    return v;
  }
};

struct ModelPool {
  // FIX (A): owned blocks for the exe's realloc'd array of malloc'd ones; a
  // model keeps its address as the pool grows.
  std::vector<std::unique_ptr<Model[]>> blocks;  // 0x008d4af4, count 0x008d4aec
  int32_t block = 750;                 // 0x008d4ae8  slots a block (0x0053fe30)
  int32_t capacity = 0;                // 0x008d4af0
  int32_t count = 0;                   // 0x008d4af8
  int32_t free_head = -1;              // 0x008d4afc
  Model& at(int32_t i) { return blocks[static_cast<size_t>(i / block)][i % block]; }
};
ModelPool& models();

int model_scroll_tick(Model* m);                       // 0x00547ab0: the UVs moved, -1 for no model
int model_set_scroll(Model* m, int on, float u, float v);  // MW3's 0x00526ae0
bool owns(const void* model);                           // ours: a model of this pool, not MW3's
int model_set_priority(Model* m, int32_t priority);     // 0x0055b7e0
void cycle_reset(Model* m);                             // 0x0055ba20
void cycle_set_frame(Model* m, int32_t n);              // 0x0055bb20
int cycle_set_on(Model* m, int32_t count);              // 0x0055ba60, -1 for no model
int cycle_set_looping(Model* m, uint32_t on);           // 0x0055bba0
int cycle_set_speed(Model* m, float speed);             // 0x0055bbc0
int cycle_set_map(Model* m, ::zimg::TextureEntry* t);   // 0x0055bc10
// Pirate's Moon 1.0 @ 0x00557ba0: the slot of an index, null below 0.
Model* model_at(int32_t index);
// Pirate's Moon 1.0 @ 0x00558f50: the free head, zeroed but its link, flags
// 3; a full pool grows by a block first (0x00557be0; MW3's aborted). The exe
// returns null before the pool's set-up ([0x008d4ae4], always done in play).
Model* model_alloc();
// Pirate's Moon 1.0 @ 0x00559310: MW3's 0x005392c0 (the models nothing holds, released) left empty: frees none, 0.
int32_t models_free_unused();
// Pirate's Moon 1.0 @ 0x00559320 (MW3's 0x005394c0): the models (the empty 0x00559310), then the materials (0x00557890).
void models_purge(bool models, bool materials);
// Pirate's Moon 1.0 @ 0x005590f0: every array freed, the slot back on the
// free list. 5 for no model, 1 while a node uses it.
int model_release(Model* m);
// The arguments 0x0055cb70 takes on the stack (the crater's polygons). The
// exe's morph arguments are 0 from every caller here.
struct PolygonAdd {
  float* points = nullptr;              // the caller's: colinear points are dropped in place
  const float* normals = nullptr;       // optional (polygon flag 0x1000)
  const float* const* uvs = nullptr;    // one array a material, 2 floats a point; optional
  int32_t material_count = 0;
  ::zmod::Material* const* materials = nullptr;
  int32_t priority = 0;
  uint32_t back = 0;                    // bit 0: polygon flag 0x400
  uint32_t flag_800 = 0;                // bit 0: polygon flag 0x800
  const uint8_t* zone = nullptr;        // 4 bytes, the polygon's +0x24
  const float* colors = nullptr;        // 3 floats a point; null: white
};
// Pirate's Moon 1.0 @ 0x0055cb70. As MW3's 0x0053c6c0: 0 added, 1 refused, 2
// added in pieces.
int model_add_polygon(Model* m, int32_t n, const PolygonAdd& a);
// Pirate's Moon 1.0 @ 0x0055c470: the material refs rebuilt from each
// polygon's first material, the polygons regrouped to match: a cycled
// material's polygons one ref each, first; then one ref per other material,
// in the order first used. One ref keeps the polygons as they are.
void sort_by_material(Model& m);
// Pirate's Moon 1.0 @ 0x005726f0: a type-0 model's strips (0x2000) made triangles in place, as the crater's clip
// (0x00528920, 0x005290b0) asks before it reads the polygons. A strip keeps its first triangle with its first
// material; each other triangle that is not degenerate (by index), with each material, is appended as a polygon of
// one material. Then the model's 0x80 is cleared, and when a strip had more than three points the refs rebuilt
// (0x0055c470).
void model_unstrip(Model* m);
// Pirate's Moon 1.0 @ 0x00558ba0: a gamez.zbd's models appended in new blocks
// (the count and free head taken on the first load only), each one's data
// read (0x005587e0) with the file's material base. The first one's index, 0
// for none, -1 on a read error.
int read_models(std::FILE* f, int32_t material_base);
// Pirate's Moon 1.0 @ 0x00558440 (MW3's 0x00538640): model `index` of the file at its models' table read again
// into a new slot (0x68-byte records, the data at the record's +0x64); null when the file has no such model.
Model* reread_model(std::FILE* f, int32_t index, int32_t material_base);
int model_changes(const Model& m);                                       // 0x0055b0b0
Model* model_copy(const Model& m, int32_t own_materials, int32_t shared);  // 0x00559340
// Pirate's Moon 1.0 @ 0x005587e0: one model's arrays, its material indices
// offset by `material_base`. The exe checks no read.
void read_model_data(std::FILE* f, Model& m, int32_t material_base);

}  // namespace pm::zmod
