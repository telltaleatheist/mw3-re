#pragma once
#include <cstdint>
#include <vector>

namespace zcls { struct Node; struct Partition; }
namespace zmod { struct Material; struct Model; struct Polygon; }

// zGeometry's model side (zGeometry\zgeo_model.cpp, 0x0050da90..0x0050f950):
// the buffers, interpolation and bookkeeping the crater clip works with
// (docs/craters.md, sections 2 and 5). The clip itself (0x0050e950,
// 0x0050ea50, 0x0050ec10, 0x0050ef70, 0x0050f120, 0x0050f7b0) comes with
// Weiler–Atherton (craters parts 3 and 5).
//
// The clip works in a plane: points (x, y, z) become (x, -z, y), so the
// outline's (x, z) is its 2D (x, y), and back.
namespace zgeo {

// The vertex colours of the polygons cut near the crater being made, for
// the crater's own polygons to take on (0x24-byte entries at 0x0060cb50).
struct ColorEntry {
  float pos[3];
  float unused[3];
  float color[3];                  // +0x18
};
constexpr int32_t kColorEntries = 63;
struct ColorCache {
  float reach = 0;                 // 0x0060cb38  the crater's radius + 4
  int32_t count = 0;               // 0x0060cb3c
  float center[3] = {};            // 0x0060cb40
  ColorEntry entries[kColorEntries] = {};  // 0x0060cb50
};
ColorCache& colors();
void colors_begin(float x, float y, float z, float radius);  // 0x0050da90
// Kept when within reach of the centre in x and z, not full and no entry
// has the same x and z.
void colors_add(const float pos[3], const float color[3]);   // 0x0050dad0 ECX pos
// The first entry within 0.5 in x and z, else entry 0; 0 when empty.
int colors_find(const float pos[3], float out[3]);           // 0x0050dbc0 ECX pos

void to_plane(float* points, int32_t n);    // 0x0050dc90 ECX n, EDX points: (x, -z, y)
void from_plane(float* points, int32_t n);  // 0x0050dc70 ECX n, EDX points: back
// Boxes are min x, min y, max x, max y. Overlap with 1 to spare.
int boxes_touch(const float a[4], const float b[4]);        // 0x0050dcb0 ECX a, EDX b
// Over the points' (x, y), 3 floats apart; the first point even when n is 0.
void box_of(float box[4], const float* points, int32_t n);  // 0x0050e3d0 ECX box, EDX points

// A material of a random colour for an untextured polygon (the 16-bit
// colour packed from the 0..255 values' low bits).
zmod::Material* random_material();                          // 0x0050dd20
zmod::Material* color_material(float r, float g, float b);  // 0x0050de00

// The gradient (d/dx, d/dz) of a value s over the triangle p0 p1 p2 in x
// and z; both 0 when the triangle has no area there.
void gradient(const float p0[3], const float p1[3], const float p2[3], float s0, float s1, float s2,
              float out[2]);  // 0x0050e020 ECX p0, EDX p1
// A polygon's UVs, colours and normals carried over to new points by the
// gradients over its first three vertices (normals made unit length).
std::vector<float> uvs_at(const float* points, int32_t n, const zmod::Model& m,
                          const zmod::Polygon& p);  // 0x0050df30 ECX n, EDX points
std::vector<float> colors_at(const float* points, int32_t n, const zmod::Model& m,
                             const zmod::Polygon& p);  // 0x0050e100
std::vector<float> normals_at(const float* points, int32_t n, const zmod::Model& m,
                              const zmod::Polygon& p);  // 0x0050e240

// A polygon's vertex positions in order (0x0050f0a0 ECX model, EDX polygon,
// reallocating the caller's buffer).
void polygon_points(const zmod::Model& m, const zmod::Polygon& p, std::vector<float>& out);

// The crater's outline (or a polygon) in the plane, 0x1c bytes.
struct ClipPolygon {
  void* weiler = nullptr;          // +0x00  Weiler–Atherton's polygon (0x0050f960)
  std::vector<float> points;       // +0x04  3 floats each, in the plane
  int32_t count = 0;               // +0x08
  float box[4] = {};               // +0x0c
};
ClipPolygon* clip_polygon_new(const float* points, int32_t n);  // 0x0050e450 ECX n, EDX points
// Its points back out of the plane into `out` (reallocated), their count
// into `count`.
int clip_polygon_out(const ClipPolygon& c, int32_t* count, std::vector<float>& out);  // 0x0050e4c0
void clip_polygon_free(ClipPolygon* c);  // 0x0050e520
// The index of the point within 0.01 in x and y, else -1.
int find_point(const ClipPolygon& c, const float point[3]);  // 0x0050e550 ECX c, EDX point
// The edge (from point i) the point lies on strictly between its ends,
// within 0.01, else -1.
int find_edge(const ClipPolygon& c, const float point[3]);   // 0x0050e690 ECX c, EDX point
// Each point replaces the point it matches, or goes in after the edge it lies
// on; 1 when any did.
int insert_points(ClipPolygon& c, const float* points, int32_t n);  // 0x0050e5a0 ECX c, EDX n

// A node of the cell whose model the clip replaced, and the new model.
struct NodeModel {
  zcls::Node* node = nullptr;
  zmod::Model* model = nullptr;
};
struct CellClip {                  // 0x0c bytes
  std::vector<NodeModel> nodes;    // +0x00 count, +0x04
  zcls::Partition* cell = nullptr; // +0x08
};
// What a crater's clip changed, 0x10 bytes.
struct ClipSet {
  int32_t point_count = 0;         // +0x00  the outline as the clip left it
  std::vector<float> points;       // +0x04
  std::vector<CellClip> cells;     // +0x08 count, +0x0c
};
ClipSet* clip_set_new();               // 0x0050e910
void clip_set_free(ClipSet* s);        // 0x0050e930
// The new models onto their nodes (the old ones released), each cell's
// crater count (+0x39) up by one and its list emptied.
int commit(ClipSet& s);                // 0x0050e850 ECX s

// Polygons into a model through zModel's 0x0053c6c0. add_polygon: a
// polygon of n points (no UVs and no material: a random colour);
// add_child: a cut piece of src's polygon p, its UVs, colours and normals
// carried over (the colours noted for the crater); copy_polygon: p as is.
int add_polygon(zmod::Model* m, int32_t n, const float* points, zmod::Material* material, const float* uvs,
                const float* normals, const float* colors);  // 0x0050dea0 ECX m, EDX n
int add_child(zmod::Model* m, int32_t n, const float* points, const zmod::Model& src,
              const zmod::Polygon& p);  // 0x0050f4e0 ECX m, EDX n
int copy_polygon(zmod::Model* m, const zmod::Model& src, const zmod::Polygon& p);  // 0x0050f640 ECX m, EDX src

struct Hooks {
  // zModel's add-a-polygon (not ported yet).
  int (*model_add)(zmod::Model* m, int32_t n, const float* points, const float* normals, const float* uvs,
                   uint32_t a, uint32_t b, uint32_t c, zmod::Material* material, int32_t priority, uint32_t flag,
                   const uint8_t zone[4], const float* colors) = nullptr;  // 0x0053c6c0 ECX m, EDX n
  zmod::Material* (*material_add)(zmod::Material& m);                 // 0x00536ad0
  int (*rand)();                                                      // MSVCRT's
  zmod::Model* (*node_model)(zcls::Node* node);                       // 0x004eb420
  int (*node_set_model)(zcls::Node* node, zmod::Model* model);        // 0x004eb370
  int (*model_release)(zmod::Model* m);                               // 0x00539150
  void (*weiler_free)(void* polygon) = nullptr;                       // 0x0050fab0
  Hooks();
};
Hooks& hooks();

struct State {
  zmod::Material* random = nullptr;  // 0x0060d450  the last random_material
  zmod::Material* colored = nullptr; // 0x0060d454  the last color_material
};
State& state();

}  // namespace zgeo
