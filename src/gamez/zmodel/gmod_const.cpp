#include "gamez/zmodel/gmod_const.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "crt/msvc_math.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zvideo/zvid_format.h"

#pragma STDC FP_CONTRACT OFF

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

// Mech3.exe 1.2 @ 0x00526800 (the pool's part; the draw table and lighting
// settings are the caller's).
void reset_pool() {
  ModelPool& p = g_pool;
  if (p.size == 0) p.size = 0x6d6;
  p.slots.assign(p.size, Model{});
  for (int32_t i = 0; i < p.size; ++i) p.slots[i].link = static_cast<uint32_t>(i + 1 < p.size ? i + 1 : -1);
  p.free_head = 0;
  p.count = 0;
}

// Mech3.exe 1.2 @ 0x00538fe0
Model* model_alloc() {
  ModelPool& p = g_pool;
  if (p.free_head < 0) zcls::gamez_abort("GameZ model resource failure: restarting your game usually solves this.");
  Model* m = &p.slots[p.free_head];
  ++p.count;
  p.free_head = static_cast<int32_t>(m->link);
  const uint32_t link = m->link;
  *m = Model{};
  m->link = link;
  const uint32_t flags = 3;  // (+8 & ~0x10) | 3 on the cleared header
  std::memcpy(m->header + 8, &flags, 4);
  return m;
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

// Mech3.exe 1.2 @ 0x00539150. 1 while a node still uses it. The polygons'
// arrays, the vertices, normals, morphs and lights are freed, the header
// zeroed and the slot put back on the pool's free list.
int model_release(Model* m) {
  if (!m) return 5;
  if (m->count(0xc) != 0) return 1;
  for (uint32_t i = 0; m->polygons && i < m->count(0x10); ++i) {
    Polygon& q = m->polygons[i];
    std::free(q.vertex_indices), std::free(q.normal_indices), std::free(q.uvs);
    std::free(q.colors), std::free(q.scratch);
    q.vertex_indices = q.normal_indices = q.scratch = nullptr;
    q.uvs = q.colors = nullptr;
  }
  std::memset(m->header + 0x10, 0, 4);
  delete[] m->polygons;
  std::free(m->vertices), std::free(m->normals), std::free(m->morphs);
  for (uint32_t i = 0; m->lights && i < m->count(0x20); ++i) std::free(m->lights[i].vertices);
  std::free(m->lights);
  m->polygons = nullptr, m->vertices = m->normals = m->morphs = nullptr, m->lights = nullptr;
  std::memset(m->header, 0, sizeof m->header);
  const int32_t index = static_cast<int32_t>(m - g_pool.slots.data());
  m->link = static_cast<uint32_t>(g_pool.free_head);
  --g_pool.count;
  g_pool.free_head = index;
  return 0;
}

namespace {
template <typename T>
T* dup(const T* src, size_t count) {
  auto* p = static_cast<T*>(std::malloc(count * sizeof(T)));
  if (count) std::memcpy(p, src, count * sizeof(T));
  return p;
}
void put32(uint8_t* h, int off, uint32_t v) { std::memcpy(h + off, &v, 4); }
}  // namespace

// Mech3.exe 1.2 @ 0x005396c0. Flag bits 0..4 come from m (the rest stay the
// pool's 3, which has none), then the type, mode, +0x24..+0x30, the lights
// with their points, morphs, vertices, normals and polygons; UVs only where
// the (copied) material is textured.
Model* model_copy(const Model& m, int32_t own_materials, int32_t shared) {
  Model* c = model_alloc();
  uint8_t* h = c->header;
  std::memcpy(h, m.header, 8);  // +0 type, +4 mode
  put32(h, 0xc, 0);
  put32(h, 8, (m.count(8) & 0x1f) | (c->count(8) & ~0x1fu));
  std::memcpy(h + 0x24, m.header + 0x24, 0x10);
  const uint32_t lights = m.count(0x20);
  put32(h, 0x20, lights);
  if (lights) {
    c->lights = static_cast<Light*>(std::malloc(lights * sizeof(Light)));
    for (uint32_t i = 0; i < lights; ++i) {
      c->lights[i] = m.lights[i];
      int32_t n;
      std::memcpy(&n, c->lights[i].raw + 0xc, 4);
      c->lights[i].vertices = dup(m.lights[i].vertices, n > 0 ? 3 * static_cast<size_t>(n) : 0);
    }
  }
  const int32_t morphs = static_cast<int32_t>(m.count(0x1c)), verts = static_cast<int32_t>(m.count(0x14));
  const int32_t norms = static_cast<int32_t>(m.count(0x18)), polys = static_cast<int32_t>(m.count(0x10));
  if (morphs > 0) c->morphs = dup(m.morphs, 3 * static_cast<size_t>(morphs));
  put32(h, 0x1c, m.count(0x1c));
  if (verts > 0) c->vertices = dup(m.vertices, 3 * static_cast<size_t>(verts));
  put32(h, 0x14, m.count(0x14));
  if (norms > 0) c->normals = dup(m.normals, 3 * static_cast<size_t>(norms));
  put32(h, 0x18, m.count(0x18));
  if (polys > 0) c->polygons = new Polygon[polys]{};
  put32(h, 0x10, m.count(0x10));
  std::vector<std::pair<Material*, Material*>> made;  // this copy's material copies
  for (int32_t i = 0; i < polys; ++i) {
    const Polygon& s = m.polygons[i];
    Polygon& d = c->polygons[i];
    d.priority = s.priority;
    d.flags = s.flags & 0x700;
    d.zone_set = s.zone_set;
    d.material_index = s.material_index;
    d.material = s.material;
    if (own_materials && (!shared || material_changes(*s.material))) {
      Material* copy = nullptr;
      for (const auto& p : made)
        if (p.first == s.material) copy = p.second;
      if (!copy) {
        copy = material_copy(*s.material);
        made.emplace_back(s.material, copy);
      }
      d.material = copy;
    }
    const uint32_t k = s.flags & 0xff;
    if (k) {
      d.vertex_indices = dup(s.vertex_indices, k);
      if ((s.flags & 0x200) && s.normal_indices) d.normal_indices = dup(s.normal_indices, k);
    }
    d.flags = (d.flags & ~0xffu) | k;
    if (d.material->flags & 1) d.uvs = dup(s.uvs, 2 * static_cast<size_t>(k));
    d.colors = dup(s.colors, 3 * static_cast<size_t>(k));
    d.scratch = static_cast<uint32_t*>(std::malloc(k * 4));
  }
  return c;
}

// Mech3.exe 1.2 @ 0x0053b3c0
int model_changes(const Model& m) {
  if (m.count(8) & 0x1c) return 1;
  for (uint32_t i = 0; i < m.count(0x10); ++i)
    if (material_changes(*m.polygons[i].material)) return 1;
  return 0;
}

// Mech3.exe 1.2 @ 0x0053bc10
void cycle_reset(Model* m) {
  Material* mat = m->polygons[0].material;
  Cycle* c = mat->cycle;
  if (!c) return;
  c->index = 0.0f;
  mat->texture = c->textures[0];
}

// Mech3.exe 1.2 @ 0x0053bc90. A frame past the end wraps by the count.
void cycle_set_frame(Model* m, int32_t n) {
  Material* mat = m->polygons[0].material;
  Cycle* c = mat->cycle;
  if (!c) return;
  const int32_t count = static_cast<int32_t>(c->texture_count);
  if (n >= count) n = n % count;
  else if (n < 0) n = 0;
  mat->texture = c->textures[n];
  c->index = static_cast<float>(n);
}

namespace {
// One point into the box. The x87 compares differ per side, so a NaN
// coordinate lands only on some (0x0053b733..0x0053b7d2 for the lights).
void grow_light(float* b, const float* p) {
  if (!(p[0] >= b[0])) b[0] = p[0];
  if (!(b[3] >= p[0])) b[3] = p[0];
  if (b[1] > p[1]) b[1] = p[1];
  if (!(b[4] >= p[1])) b[4] = p[1];
  if (b[2] > p[2]) b[2] = p[2];
  if (!(b[5] >= p[2])) b[5] = p[2];
}
// The vertices and the morphed ones (0x0053b80d.., 0x0053b8c8..).
void grow_vertex(float* b, const float* p) {
  if (!(p[0] >= b[0])) b[0] = p[0];
  if (p[0] > b[3]) b[3] = p[0];
  if (b[1] > p[1]) b[1] = p[1];
  if (!(b[4] >= p[1])) b[4] = p[1];
  if (b[2] > p[2]) b[2] = p[2];
  if (!(b[5] >= p[2])) b[5] = p[2];
}
int32_t hdr_i(const uint8_t* h, int off) {
  int32_t v;
  std::memcpy(&v, h + off, 4);
  return v;
}
void hdr_f(uint8_t* h, int off, float v) { std::memcpy(h + off, &v, 4); }

// Mech3.exe 1.2 @ 0x0053b680. Starts from the first vertex, else the first
// light's first point.
void model_extent(const uint8_t* h, const float* vertices, const Light* lights, const float* morphs, float* b) {
  const int32_t nv = hdr_i(h, 0x14), nl = lights ? hdr_i(h, 0x20) : 0, nm = hdr_i(h, 0x1c);
  const float* first = nv >= 1 ? vertices : nl >= 1 ? lights[0].vertices : nullptr;
  if (first) {
    b[0] = b[3] = first[0];
    b[1] = b[4] = first[1];
    b[2] = b[5] = first[2];
  }
  for (int32_t i = 0; i < nl; ++i) {
    const Light& l = lights[i];
    const int32_t n = hdr_i(l.raw, 0xc);
    for (int32_t j = 0; j < n; ++j) grow_light(b, l.vertices + 3 * j);
  }
  for (int32_t i = 1; i < nv; ++i) grow_vertex(b, vertices + 3 * i);
  if (nm != 0 && morphs) {
    std::vector<zmth::Vec3> out(static_cast<uint32_t>(nm));  // the scratch at [0x0074a288]
    zmth::morph(out.data(), reinterpret_cast<const zmth::Vec3*>(vertices),
                reinterpret_cast<const zmth::Vec3*>(morphs), static_cast<uint32_t>(nm), 1.0f);
    for (int32_t i = 0; i < nm; ++i) grow_vertex(b, &out[i].x);
  }
}
}  // namespace

// Mech3.exe 1.2 @ 0x0053b4e0. Type 0 keeps the extent; type 1 (a facade)
// takes the larger of each axis's ends: mode 0 a box as wide in z as in x,
// otherwise a cube. Other types keep the box as given.
void model_box(uint8_t* h, const float* vertices, const Light* lights, const float* morphs, float box[6]) {
  if (!h || !box) return;
  const int32_t type = hdr_i(h, 0);
  if (type == 0) {
    model_extent(h, vertices, lights, morphs, box);
  } else if (type == 1) {
    model_extent(h, vertices, lights, morphs, box);
    float x = box[3], y = box[4], z = box[5];
    if (std::fabs(box[0]) > x) x = std::fabs(box[0]);
    if (std::fabs(box[1]) > y) y = std::fabs(box[1]);
    if (std::fabs(box[2]) > z) z = std::fabs(box[2]);
    if (hdr_i(h, 4) != 0) {
      float m = x;
      if (y > m) m = y;
      if (z > m) m = z;
      x = y = z = m;
    } else {
      if (x > z) z = x;
      else x = z;
    }
    box[0] = -x, box[1] = -y, box[2] = -z;
    box[3] = x, box[4] = y, box[5] = z;
  }
  const float hx = (box[3] - box[0]) * 0.5f, hy = (box[4] - box[1]) * 0.5f, hz = (box[5] - box[2]) * 0.5f;
  hdr_f(h, 0x48, hx + box[0]);
  hdr_f(h, 0x4c, hy + box[1]);
  hdr_f(h, 0x50, hz + box[2]);
  const float r2 = (hx * hx + hy * hy) + hz * hz;
  uint32_t bits;
  std::memcpy(&bits, &r2, 4);
  bits = static_cast<uint32_t>((static_cast<int32_t>(bits) >> 1) + 0x1fc00000);
  std::memcpy(h + 0x54, &bits, 4);
}

}  // namespace zmod
