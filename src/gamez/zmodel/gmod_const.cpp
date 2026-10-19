#include "gamez/zmodel/gmod_const.h"

#include <cstdlib>
#include <cstring>

#include "crt/msvc_math.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zvideo/zvid_format.h"

namespace zmod {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zModel\\gmod_const.c";
ModelPool g_pool;

template <typename T>
T* read_array(std::FILE* f, uint32_t count, uint32_t size, bool* ok) {
  auto* p = static_cast<T*>(std::malloc(static_cast<size_t>(count) * size));
  *ok = std::fread(p, static_cast<size_t>(count) * size, 1, f) == 1;
  return p;
}
}  // namespace

ModelPool& models() { return g_pool; }

uint32_t Model::count(int off) const {
  uint32_t v;
  std::memcpy(&v, header + off, 4);
  return v;
}

// Mech3.exe 1.2 @ 0x005380c0. The exe has no upper bound; past the pool is null here.
Model* model_at(int32_t index) {
  if (index < 0 || index >= static_cast<int32_t>(g_pool.slots.size())) return nullptr;
  return &g_pool.slots[index];
}

// Mech3.exe 1.2 @ 0x00538a70. Every read error returns -1, which the caller
// ignores.
int read_model_data(std::FILE* f, Model& m) {
  bool ok = true;
  const uint32_t polys = m.count(0x10), verts = m.count(0x14), norms = m.count(0x18);
  const uint32_t morphs = m.count(0x1c), lights = m.count(0x20);
  if (static_cast<int32_t>(verts) > 0 && (m.vertices = read_array<float>(f, verts, 12, &ok), !ok)) return -1;
  if (static_cast<int32_t>(norms) > 0 && (m.normals = read_array<float>(f, norms, 12, &ok), !ok)) return -1;
  if (static_cast<int32_t>(morphs) > 0 && (m.morphs = read_array<float>(f, morphs, 12, &ok), !ok)) return -1;
  if (static_cast<int32_t>(lights) > 0) {
    m.lights = static_cast<Light*>(std::calloc(lights, sizeof(Light)));
    for (uint32_t i = 0; i < lights; ++i)
      if (std::fread(m.lights[i].raw, 0x4c, 1, f) != 1) return -1;
    for (uint32_t i = 0; i < lights; ++i) {
      Light& l = m.lights[i];
      float rgb[3];
      std::memcpy(rgb, l.raw + 0x1c, 12);
      const uint8_t b = static_cast<uint8_t>(crt::msvc_ftol(rgb[2] + 0.5f));  // fsub of -0.5 @ 0x005ce178: rounds
      const uint8_t g = static_cast<uint8_t>(crt::msvc_ftol(rgb[1] + 0.5f));
      const uint8_t r = static_cast<uint8_t>(crt::msvc_ftol(rgb[0] + 0.5f));
      l.color = zvid::pack_bytes(r, g, b);
      int32_t n;
      std::memcpy(&n, l.raw + 0x0c, 4);
      if (n > 0 && (l.vertices = read_array<float>(f, n, 12, &ok), !ok)) {
        zerr::report(0x200, kFile, 0, "Error reading GameZ Model3D point light data.");
        return -1;
      }
    }
  }
  if (static_cast<int32_t>(polys) <= 0) return 0;
  std::vector<uint8_t> raw(static_cast<size_t>(polys) * 0x24);
  if (std::fread(raw.data(), raw.size(), 1, f) != 1) return -1;
  m.polygons = new Polygon[polys]{};
  for (uint32_t i = 0; i < polys; ++i) {
    const uint8_t* p = raw.data() + i * 0x24;
    Polygon& q = m.polygons[i];
    int32_t index;
    std::memcpy(&q.flags, p, 4), std::memcpy(&q.priority, p + 4, 4);
    std::memcpy(&index, p + 0x1c, 4), std::memcpy(&q.zone_set, p + 0x20, 4);
    q.material = take_material(index);
  }
  for (uint32_t i = 0; i < polys; ++i) {
    Polygon& q = m.polygons[i];
    const uint32_t k = q.flags & 0xff;
    if (k) {
      if ((q.vertex_indices = read_array<uint32_t>(f, k, 4, &ok), !ok)) return -1;
      if ((q.flags & 0x200) && (q.normal_indices = read_array<uint32_t>(f, k, 4, &ok), !ok)) return -1;
    }
    if (q.material && (q.material->flags & 1) && (q.uvs = read_array<float>(f, k, 8, &ok), !ok)) return -1;
    if ((q.colors = read_array<float>(f, k, 12, &ok), !ok)) return -1;
    q.scratch = static_cast<uint32_t*>(std::malloc(k * 4));
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00538e30. FIX (A): a vector of slots; the data arrays are
// read in slot order, free slots having no counts.
int read_models(std::FILE* f) {
  ModelPool& p = g_pool;
  int32_t size, count, free_head;
  if (std::fread(&size, 4, 1, f) != 1 || std::fread(&count, 4, 1, f) != 1 ||
      std::fread(&free_head, 4, 1, f) != 1) {
    zerr::report(0x200, kFile, 0, "Error reading GameZ Model3D buffer data.");
    return -1;
  }
  p.size = size, p.count = count, p.free_head = free_head;
  if (size == 0) return 0;
  std::vector<uint8_t> raw(static_cast<size_t>(size) * 0x60);
  if (std::fread(raw.data(), raw.size(), 1, f) != 1) {
    zerr::report(0x200, kFile, 0, "Error reading GameZ Model3D buffer data.");
    return -1;
  }
  p.slots.assign(size, Model{});
  for (int i = 0; i < size; ++i) {
    std::memcpy(p.slots[i].header, raw.data() + i * 0x60, 0x5c);
    std::memcpy(&p.slots[i].link, raw.data() + i * 0x60 + 0x5c, 4);
  }
  for (int i = 0; i < size; ++i) read_model_data(f, p.slots[i]);
  return size;
}

}  // namespace zmod
