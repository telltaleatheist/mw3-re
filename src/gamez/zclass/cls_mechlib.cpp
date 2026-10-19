#include "gamez/zclass/cls_mechlib.h"

#include <cstdlib>
#include <cstring>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zutil/zutl_zar.h"

namespace zcls {

namespace {
template <typename T>
T* read_array(std::FILE* f, uint32_t count, uint32_t size) {
  auto* p = static_cast<T*>(std::malloc(static_cast<size_t>(count) * size));
  std::fread(p, size, count, f);
  return p;
}

uint32_t u32_at(const uint8_t* p, int off) {
  uint32_t v;
  std::memcpy(&v, p + off, 4);
  return v;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004fc570. A non-zero texture field means a name follows.
bool read_material(std::FILE* f, Material& m) {
  m.raw = static_cast<uint8_t*>(std::malloc(0x28));
  std::fread(m.raw, 0x28, 1, f);
  m.resolved = false;
  m.texture = nullptr;
  if (u32_at(m.raw, 0x10) != 0) {
    std::memset(m.raw + 0x10, 0, 4);
    zutl_zar::read_string(f, &m.texture);
  }
  return true;
}

// Mech3.exe 1.2 @ 0x004fcf20. FIX (A): the model comes from new, not the
// 0x60-byte pool at 0x007402e4 (which quits the game when empty).
Model* read_model(std::FILE* f) {
  auto* m = new Model{};
  std::fread(m->header, 0x5c, 1, f);
  m->polygon_count = u32_at(m->header, 0x10);
  m->vertex_count = u32_at(m->header, 0x14);
  m->normal_count = u32_at(m->header, 0x18);
  m->morph_count = u32_at(m->header, 0x1c);
  if (m->vertex_count) m->vertices = read_array<float>(f, m->vertex_count, 12);
  if (m->normal_count) m->normals = read_array<float>(f, m->normal_count, 12);
  if (m->morph_count) m->morphs = read_array<float>(f, m->morph_count, 12);
  if (m->polygon_count) {
    m->polygons = new Polygon[m->polygon_count]{};
    for (uint32_t i = 0; i < m->polygon_count; ++i) {
      uint8_t raw[0x24];
      std::fread(raw, 0x24, 1, f);
      Polygon& p = m->polygons[i];
      p.flags = u32_at(raw, 0);
      p.priority = static_cast<int32_t>(u32_at(raw, 4));
      for (int k = 0; k < 5; ++k) p.file_ptrs[k] = u32_at(raw, 8 + 4 * k);
      p.material = u32_at(raw, 0x1c);
      p.zone_set = u32_at(raw, 0x20);
    }
  }
  for (uint32_t i = 0; i < m->polygon_count; ++i) {
    Polygon& p = m->polygons[i];
    const uint32_t k = p.flags & 0xff;
    if (k) p.vertex_indices = read_array<uint32_t>(f, k, 4);
    if ((p.flags & 0x200) && p.file_ptrs[1]) p.normal_indices = read_array<uint32_t>(f, k, 4);
    if (p.file_ptrs[2]) p.uvs = read_array<float>(f, k, 8);
    if (p.file_ptrs[3]) p.colors = read_array<float>(f, k, 12);
    p.scratch = static_cast<uint32_t*>(std::malloc(k * 4));
  }
  return m;
}

void free_model(Model* m) {
  if (!m) return;
  for (uint32_t i = 0; i < m->polygon_count; ++i) {
    Polygon& p = m->polygons[i];
    std::free(p.vertex_indices), std::free(p.normal_indices), std::free(p.uvs);
    std::free(p.colors), std::free(p.scratch);
  }
  delete[] m->polygons;
  std::free(m->vertices), std::free(m->normals), std::free(m->morphs);
  delete m;
}

// Mech3.exe 1.2 @ 0x004fce50. The file's 0xd0 bytes replace the new node
// except its data, flag bits 0-1, list-7 item and parents. Only object3d
// nodes are made: the exe dereferences null for any other class.
Node* read_node(std::FILE* f, Node* parent) {
  uint8_t file_node[0xd0];
  std::fread(file_node, sizeof file_node, 1, f);
  int32_t cls;
  std::memcpy(&cls, file_node + 0x34, 4);
  if (cls != 5) return nullptr;
  Node* node = object3d_create();
  std::fread(static_cast<Object3d*>(node->data)->raw, 0x90, 1, f);
  const uint32_t flags = node->flags();
  std::memcpy(node->raw, file_node, sizeof file_node);
  node->set_flags((node->flags() & ~3u) | (flags & 3));
  node->put<int32_t>(0x54, 0), node->put<int32_t>(0x58, 0);
  node->parents = nullptr;
  if (parent) node_add_child(parent, node);
  return node;
}

// Mech3.exe 1.2 @ 0x004fcdc0. Each node, its model, then its children, which
// add themselves back as they are read.
Node* read_tree(TreeReader& lib, std::FILE* f, Node* parent) {
  if (!f) return nullptr;
  ++lib.depth;
  Node* node = read_node(f, parent);
  if (node->get<int32_t>(0x3c) != 0) node->lib_model = read_model(f);
  const int32_t children = node->child_count();
  node->put<int32_t>(0x5c, 0), node->put<int32_t>(0x60, 0);
  node->children = nullptr;
  for (int32_t i = 0; i < children; ++i) read_tree(lib, f, node);
  --lib.depth;
  return node;
}

}  // namespace zcls
