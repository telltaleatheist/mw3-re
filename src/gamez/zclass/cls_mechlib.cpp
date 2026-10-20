#include "gamez/zclass/cls_mechlib.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zmodel/gmod_const.h"
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

// Mech3.exe 1.2 @ 0x004fcf20. The file's 0x5c bytes over a pool slot (its
// link stays). The file's pointer fields only say whether an array follows.
zmod::Model* read_model(std::FILE* f) {
  uint8_t header[0x5c];
  std::fread(header, sizeof header, 1, f);
  zmod::Model* m = zmod::model_alloc();
  std::memcpy(m->header, header, sizeof header);
  const uint32_t polys = u32_at(header, 0x10), verts = u32_at(header, 0x14);
  const uint32_t norms = u32_at(header, 0x18), morphs = u32_at(header, 0x1c);
  if (static_cast<int32_t>(verts) > 0) m->vertices = read_array<float>(f, verts, 12);
  if (static_cast<int32_t>(norms) > 0) m->normals = read_array<float>(f, norms, 12);
  if (static_cast<int32_t>(morphs) > 0) m->morphs = read_array<float>(f, morphs, 12);
  if (static_cast<int32_t>(polys) <= 0) return m;
  std::vector<uint8_t> raw(static_cast<size_t>(polys) * 0x24);
  std::fread(raw.data(), raw.size(), 1, f);
  m->polygons = new zmod::Polygon[polys]{};
  for (uint32_t i = 0; i < polys; ++i) {
    const uint8_t* r = raw.data() + i * 0x24;
    zmod::Polygon& p = m->polygons[i];
    p.flags = u32_at(r, 0);
    p.priority = static_cast<int32_t>(u32_at(r, 4));
    p.material_index = u32_at(r, 0x1c);
    p.zone_set = u32_at(r, 0x20);
    const uint32_t k = p.flags & 0xff;
    if (k) p.vertex_indices = read_array<uint32_t>(f, k, 4);
    if ((p.flags & 0x200) && u32_at(r, 0xc)) p.normal_indices = read_array<uint32_t>(f, k, 4);
    if (u32_at(r, 0x10)) p.uvs = read_array<float>(f, k, 8);
    if (u32_at(r, 0x14)) p.colors = read_array<float>(f, k, 12);
    p.scratch = static_cast<uint32_t*>(std::malloc(k * 4));
  }
  return m;
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
  if (node->get<int32_t>(0x3c) != 0) node->model = read_model(f);
  const int32_t children = node->child_count();
  node->put<int32_t>(0x5c, 0), node->put<int32_t>(0x60, 0);
  node->children = nullptr;
  for (int32_t i = 0; i < children; ++i) read_tree(lib, f, node);
  --lib.depth;
  return node;
}

}  // namespace zcls
