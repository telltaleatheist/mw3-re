#include "pm/gamez/zmodel/gmod_const.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>
#include <initializer_list>

#include "crt/msvc_math.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zgeometry/zgeo_x87.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zvideo/zvid_format.h"

namespace pm::zmod {
using zgeo::X;

namespace {
const char kFile[] = "C:\\zipper\\branchep1\\gamez\\zModel\\gmod_const.c";
ModelPool g_pool;

// malloc and an unchecked fread, as the exe; zeroed, so a short read leaves
// zeros (the tests' allocator gives the exe zeroed memory too).
// FIX (A): null when the shared bound refuses it (::zmod::ReadBound, docs/fixes.md).
template <typename T>
T* read_array(std::FILE* f, size_t bytes) {
  if (!::zmod::read_bound().fits(f, bytes, "an array")) return nullptr;
  T* p = static_cast<T*>(std::calloc(bytes ? bytes : 1, 1));
  std::fread(p, bytes, 1, f);
  return p;
}

::zmod::Material* take(int32_t index, int32_t base) {
  if (index >= 0) index += base;
  return ::zmod::take_material(index);  // 0x00555a90: flag 0x10
}
}  // namespace

ModelPool& models() { return g_pool; }

namespace {
// The step less the whole units that bring the range's middle back to 0. At 53 bits (zfix fpu53, as in
// play) the sum isn't rounded to float before the cut, which can land a unit away on a large UV.
float scroll_step(float d, float lo, float hi) {
  if (zfix::options().fpu53) {
    const double step = d;
    return static_cast<float>(step - crt::msvc_ftol(step - (static_cast<double>(hi) + lo) * -0.5));
  }
  return d - static_cast<float>(crt::msvc_ftol(d - (hi + lo) * -0.5f));
}
}  // namespace

// Pirate's Moon 1.0 @ 0x00547ab0: MW3's 0x00541ad0 on a polygon's first material and its UVs, its
// count in ten bits. The UVs move by dt times +0x28 / +0x2c, kept near 0. -1 for no model.
int model_scroll_tick(Model* m) {
  if (!m) return -1;
  const auto get = [&](int off) { float v; std::memcpy(&v, m->header + off, 4); return v; };
  uint32_t last;
  std::memcpy(&last, m->header + 0x30, 4);
  const uint32_t frame = ::zcls::frames().frame;
  if (last == frame) return 0;
  std::memcpy(m->header + 0x30, &frame, 4);
  const float su = get(0x28), sv = get(0x2c);
  const bool no_u = !(su < 0.0f || su > 0.0f), no_v = !(sv < 0.0f || sv > 0.0f);
  if (no_u && no_v) return 0;
  const float dt = ztimer::state().dt;
  const float du = dt * su, dv = dt * sv;
  for (int32_t k = 0; k < m->count(0x10); ++k) {
    Polygon& p = m->polygons[k];
    if (!(p.materials[0]->flags & 1)) continue;  // textured
    float* uv = p.uvs[0];
    const int32_t count = static_cast<int32_t>(p.flags & 0x3ff);
    if (!no_u && !no_v) {
      float umin = uv[0], umax = uv[0], vmin = uv[1], vmax = uv[1];
      for (int32_t j = 1; j < count; ++j) {
        const float u = uv[2 * j], v = uv[2 * j + 1];
        if (!(u >= umin)) umin = u;  // C0: below, or unordered
        else if (u > umax) umax = u;
        if (!(v >= vmin)) vmin = v;
        else if (v > vmax) vmax = v;
      }
      const float ou = scroll_step(du, umin, umax), ov = scroll_step(dv, vmin, vmax);
      for (int32_t j = 0; j < count; ++j) uv[2 * j] = ou + uv[2 * j], uv[2 * j + 1] = ov + uv[2 * j + 1];
      continue;
    }
    const int a = no_u ? 1 : 0;
    const float d = no_u ? dv : du;
    float lo = uv[a], hi = uv[a];
    for (int32_t j = 1; j < count; ++j) {
      const float x = uv[2 * j + a];
      if (!(x >= lo)) lo = x;
      else if (!(hi >= x)) hi = x;
    }
    const float o = scroll_step(d, lo, hi);
    for (int32_t j = 0; j < count; ++j) uv[2 * j + a] = o + uv[2 * j + a];
  }
  return 0;
}

// A model of this pool, not MW3's: shared code holding either asks here.
bool owns(const void* model) {
  const auto* at = static_cast<const Model*>(model);
  for (const auto& b : g_pool.blocks)
    if (at >= b.get() && at < b.get() + g_pool.block) return true;
  return false;
}

namespace {
::zmod::Material* first_of(Model* m, int32_t k) { return m->polygons[k].materials[0]; }
}  // namespace

// Pirate's Moon 1.0 @ 0x0055b7e0
int model_set_priority(Model* m, int32_t priority) {
  if (!m) return 0;
  for (int32_t i = 0; i < m->count(0x10); ++i) m->polygons[i].priority = priority;
  return 0;
}

// Pirate's Moon 1.0 @ 0x0055ba20: the first polygon's materials.
void cycle_reset(Model* m) {
  const Polygon& p = m->polygons[0];
  for (int32_t i = 0; i < p.material_count; ++i)
    if (::zmod::Cycle* c = p.materials[i]->cycle) c->index = 0.0f, p.materials[i]->texture = c->textures[0];
}

// Pirate's Moon 1.0 @ 0x0055bb20. A frame past the end wraps by the count.
void cycle_set_frame(Model* m, int32_t n) {
  ::zmod::Material* last = nullptr;
  for (int32_t k = 0; k < m->count(0x10); ++k) {
    ::zmod::Material* mat = first_of(m, k);
    if (mat == last) continue;
    last = mat;
    ::zmod::Cycle* c = mat->cycle;
    if (!c) continue;
    const int32_t count = static_cast<int32_t>(c->texture_count);
    if (n >= count) n = n % count;
    else if (n < 0) n = 0;
    mat->texture = c->textures[n];
    c->index = static_cast<float>(n);
  }
}

// Pirate's Moon 1.0 @ 0x0055ba60: MW3's, then the refs regrouped for the cycled material.
int cycle_set_on(Model* m, int32_t count) {
  if (!m) {
    std::fprintf(stderr, "%s(%d): ERROR setting model cycle texture. Model 3D pointer is NULL.\n",
                 "C:\\zipper\\branchep1\\gamez\\zModel\\gmod_const.c", 0x1093);
    return -1;
  }
  ::zmod::cycle_create(*first_of(m, 0), count);
  sort_by_material(*m);
  return 0;
}

// Pirate's Moon 1.0 @ 0x0055bba0
int cycle_set_looping(Model* m, uint32_t on) { return m ? ::zmod::cycle_set_loop(first_of(m, 0), on) : 0; }

// Pirate's Moon 1.0 @ 0x0055bbc0: every material in turn, where MW3's took the first.
int cycle_set_speed(Model* m, float speed) {
  if (!m) return 0;
  ::zmod::Material* last = nullptr;
  for (int32_t k = 0; k < m->count(0x10); ++k) {
    ::zmod::Material* mat = first_of(m, k);
    if (mat != last) ::zmod::cycle_set_speed(mat, speed), last = mat;
  }
  return 1;
}

// Pirate's Moon 1.0 @ 0x0055bc10
int cycle_set_map(Model* m, ::zimg::TextureEntry* t) { return m ? ::zmod::cycle_add(*first_of(m, 0), t) : 0; }

// Pirate's Moon: MW3's 0x00526ae0.
int model_set_scroll(Model* m, int on, float u, float v) {
  if (!m) return 1;
  uint32_t f;
  std::memcpy(&f, m->header + 8, 4);
  f = (static_cast<uint32_t>(on) & 1) << 4 | (f & ~0x10u);
  std::memcpy(m->header + 8, &f, 4);
  std::memcpy(m->header + 0x2c, &v, 4);
  std::memcpy(m->header + 0x28, &u, 4);
  return 0;
}

// Pirate's Moon 1.0 @ 0x00557ba0
Model* model_at(int32_t index) { return index < 0 ? nullptr : &g_pool.at(index); }

// Pirate's Moon 1.0 @ 0x00558f50, with the pool's growth 0x00557be0.
Model* model_alloc() {
  ModelPool& p = g_pool;
  if (p.free_head < 0) {
    p.blocks.emplace_back(new Model[static_cast<size_t>(p.block)]{});
    const int32_t first = p.capacity;
    p.free_head = first;
    p.capacity += p.block;
    for (int32_t i = first; i < p.capacity - 1; ++i) p.at(i).link = i + 1;
    p.at(p.capacity - 1).link = -1;
  }
  Model& m = p.at(p.free_head);
  p.free_head = m.link;
  ++p.count;
  const int32_t link = m.link;
  m = Model{};
  m.link = link;
  m.header[8] = 3;  // flags: (0 & ~0x10) | 3
  return &m;
}

namespace {
bool cycled(const Polygon& q) {  // its first material's flag 4
  return q.material_count > 0 && q.materials[0] && (q.materials[0]->flags & 4);
}
::zmod::Material* first_material(const Polygon& q) { return q.material_count > 0 ? q.materials[0] : nullptr; }
}  // namespace

// Pirate's Moon 1.0 @ 0x0055c470. The exe's list of seen materials is 1024
// on the stack (0x1000 bytes); no shipped model comes near it.
void sort_by_material(Model& m) {
  const int32_t polys = m.count(0x10);
  std::vector<::zmod::Material*> seen;
  int32_t refs = 0;
  for (int32_t i = 0; i < polys; ++i) {
    const Polygon& q = m.polygons[i];
    if (!cycled(q)) {
      ::zmod::Material* mat = first_material(q);
      if (std::find(seen.begin(), seen.end(), mat) != seen.end()) continue;
      seen.push_back(mat);
    }
    ++refs;
  }
  if (refs != m.count(0x5c)) {  // realloc in the exe
    MaterialRef* grown = new MaterialRef[static_cast<size_t>(refs > 0 ? refs : 1)]{};
    if (m.refs) std::memcpy(grown, m.refs, sizeof(MaterialRef) * static_cast<size_t>(std::min(refs, m.count(0x5c))));
    delete[] m.refs;
    m.refs = grown;
    std::memcpy(m.header + 0x5c, &refs, 4);
  }
  if (refs == 0) return;
  if (refs == 1) {
    m.refs[0] = MaterialRef{first_material(m.polygons[0]), polys, m.polygons};
    return;
  }
  Polygon* old = m.polygons;
  m.polygons = new Polygon[static_cast<size_t>(polys)]{};
  int32_t at = 0, r = 0;
  for (int32_t i = 0; i < polys; ++i) {
    if (!cycled(old[i])) continue;
    m.polygons[at] = old[i];
    m.refs[r++] = MaterialRef{first_material(m.polygons[at]), 1, m.polygons + at};
    ++at;
  }
  for (::zmod::Material* mat : seen) {
    MaterialRef& ref = m.refs[r++];
    ref = MaterialRef{mat, 0, m.polygons + at};
    for (int32_t i = 0; i < polys; ++i)
      if (!cycled(old[i]) && first_material(old[i]) == mat) m.polygons[at++] = old[i], ++ref.usage_count;
  }
  delete[] old;
}

// Pirate's Moon 1.0 @ 0x005587e0. MW3's 0x00538a70 with PM's polygons: the
// material lists, UVs per material, the model's material refs.
namespace {
void read_model_body(std::FILE* f, Model& m, int32_t base) {
  ::zmod::ReadBound& bound = ::zmod::read_bound();
  // FIX (A): a refused array (the bound) leaves its count and the later ones 0, so nothing walks what was not read.
  const auto stop = [&](std::initializer_list<int> counts) {
    for (int off : counts) std::memset(m.header + off, 0, 4);
  };
  if (m.count(0x14) > 0 && !(m.vertices = read_array<float>(f, static_cast<size_t>(m.count(0x14)) * 12)))
    return stop({0x14, 0x18, 0x1c, 0x20, 0x10, 0x5c});
  if (m.count(0x18) > 0 && !(m.normals = read_array<float>(f, static_cast<size_t>(m.count(0x18)) * 12)))
    return stop({0x18, 0x1c, 0x20, 0x10, 0x5c});
  if (m.count(0x1c) > 0 && !(m.morphs = read_array<float>(f, static_cast<size_t>(m.count(0x1c)) * 12)))
    return stop({0x1c, 0x20, 0x10, 0x5c});
  if (const int32_t n = m.count(0x20); n > 0) {
    if (!bound.fits(f, static_cast<uint64_t>(n) * 0x4c, "a light list")) return stop({0x20, 0x10, 0x5c});
    m.lights = static_cast<::zmod::Light*>(std::calloc(static_cast<size_t>(n), sizeof(::zmod::Light)));
    for (int32_t i = 0; i < n; ++i) std::fread(m.lights[i].raw, 0x4c, 1, f);  // one read of n * 0x4c in the exe
    for (int32_t i = 0; i < n; ++i) {
      ::zmod::Light& l = m.lights[i];
      float rgb[3];
      std::memcpy(rgb, l.raw + 0x1c, 12);
      const uint8_t b = static_cast<uint8_t>(crt::msvc_ftol(rgb[2] + 0.5f));  // [0x005de818] 0.5: rounds
      const uint8_t g = static_cast<uint8_t>(crt::msvc_ftol(rgb[1] + 0.5f));
      const uint8_t r = static_cast<uint8_t>(crt::msvc_ftol(rgb[0] + 0.5f));
      l.color = zvid::pack_bytes(r, g, b);  // 0x005983e0
      int32_t k;
      std::memcpy(&k, l.raw + 0x0c, 4);
      if (k > 0 && !(l.vertices = read_array<float>(f, static_cast<size_t>(k) * 12))) {
        for (int32_t j = i; j < n; ++j) std::memset(m.lights[j].raw + 0x0c, 0, 4);  // FIX (A): no points from here
        return stop({0x10, 0x5c});
      }
    }
  }
  const int32_t polys = m.count(0x10);
  if (polys > 0) {
    uint8_t* raw = read_array<uint8_t>(f, static_cast<size_t>(polys) * 0x28);
    if (!raw) return stop({0x10, 0x5c});
    m.polygons = new Polygon[static_cast<size_t>(polys)]{};
    for (int32_t i = 0; i < polys; ++i) {
      const uint8_t* p = raw + i * 0x28;
      Polygon& q = m.polygons[i];
      std::memcpy(&q.flags, p, 4), std::memcpy(&q.priority, p + 4, 4);
      std::memcpy(&q.material_count, p + 0x10, 4), std::memcpy(&q.file_uvs, p + 0x18, 4);
      std::memcpy(&q.zone_set, p + 0x24, 4);
    }
    std::free(raw);
    std::vector<std::vector<int32_t>> indices(static_cast<size_t>(polys));
    int32_t read = polys;  // FIX (A): refused, the polygons before the one refused are the model's
    for (int32_t i = 0; i < polys && read == polys; ++i) {
      Polygon& q = m.polygons[i];
      const uint32_t k = q.flags & 0x3ff;
      const size_t n = static_cast<size_t>(q.material_count > 0 ? q.material_count : 0);
      bool ok = true;
      if (k) {
        ok = (q.vertex_indices = read_array<uint32_t>(f, k * 4)) != nullptr;
        if (ok && (q.flags & 0x1000)) ok = (q.normal_indices = read_array<uint32_t>(f, k * 4)) != nullptr;
      }
      if (ok && !bound.fits(f, n * 4, "a polygon's material list")) ok = false;
      if (ok) {
        indices[static_cast<size_t>(i)].assign(n, 0);
        // FIX (A): the n it holds; a negative count (as size_t) had fread run to the file's end into it.
        if (n) std::fread(indices[static_cast<size_t>(i)].data(), n * 4, 1, f);
      }
      if (ok && q.file_uvs) {
        q.uvs = static_cast<float**>(std::calloc(n ? n : 1, sizeof(float*)));
        for (size_t j = 0; j < n && ok; ++j) ok = (q.uvs[j] = read_array<float>(f, k * 8)) != nullptr;
      }
      if (ok) ok = (q.colors = read_array<float>(f, k * 12)) != nullptr;
      if (ok) {
        q.scratch = static_cast<uint32_t*>(std::calloc(k ? k : 1, 4));
        continue;
      }
      std::free(q.vertex_indices), std::free(q.normal_indices);
      if (q.uvs)
        for (size_t j = 0; j < n; ++j) std::free(q.uvs[j]);
      std::free(q.uvs);
      q = Polygon{};
      read = i;
    }
    if (read != polys) std::memcpy(m.header + 0x10, &read, 4);
    for (int32_t i = 0; i < read; ++i) {
      Polygon& q = m.polygons[i];
      const std::vector<int32_t>& idx = indices[static_cast<size_t>(i)];
      q.materials = static_cast<::zmod::Material**>(std::calloc(idx.empty() ? 1 : idx.size(), sizeof(void*)));
      for (size_t j = 0; j < idx.size(); ++j) q.materials[j] = take(idx[j], base);
    }
    if (read != polys) return stop({0x5c});
  }
  if (const int32_t n = m.count(0x5c); n > 0) {
    uint8_t* raw = read_array<uint8_t>(f, static_cast<size_t>(n) * 12);
    if (!raw) return stop({0x5c});
    m.refs = new MaterialRef[static_cast<size_t>(n)]{};
    int32_t first = 0;
    for (int32_t i = 0; i < n; ++i) {
      int32_t index;
      std::memcpy(&index, raw + i * 12, 4), std::memcpy(&m.refs[i].usage_count, raw + i * 12 + 4, 4);
      m.refs[i].material = take(index, base);
      m.refs[i].polygon = m.polygons ? m.polygons + first : nullptr;  // the exe: +0x34 plus first * 0x28
      first += m.refs[i].usage_count;
    }
    std::free(raw);
  }
  // The exe then converts the model for the renderer: under Direct3D
  // (0x00572ae0) one without flag 0x80 into strips (0x00570b70), in software
  // one with it back (0x005726f0). Left out: the file's form is kept
  // (docs/pm-map.md, the models' form).
}
}  // namespace

// Pirate's Moon 1.0 @ 0x005587e0 (the body above); FIX (A): whatever it read, the model is checked on every way out:
// one the code would walk past its own lists is left empty, with a warning.
void read_model_data(std::FILE* f, Model& m, int32_t base) {
  read_model_body(f, m, base);
  if (const char* why = model_inconsistent(m)) {
    std::printf("mw3: a model file at byte %ld: a model whose %s. The rebuild: left this model empty. The original "
                "game: reads past the model's own lists when it draws or tests it (can crash).\n", std::ftell(f), why);
    std::fflush(stdout);
    model_empty(m);
  }
}

// Pirate's Moon 1.0 @ 0x005726f0
void model_unstrip(Model* m) {
  if (!m || m->count(0) != 0) return;
  bool appended = false;
  const int32_t count = m->count(0x10);
  for (int32_t at = 0; at < count; ++at) {
    if (!(m->polygons[at].flags & 0x2000)) continue;
    m->polygons[at].flags &= ~0x2000u;
    const int32_t n = static_cast<int32_t>(m->polygons[at].flags & 0x3ff);
    if (n == 3) continue;
    appended = true;
    for (int32_t i = n - 3; i >= 0; --i) {
      for (int32_t j = 0; j < m->polygons[at].material_count; ++j) {
        if (i == 0 && j == 0) continue;  // the strip's own triangle and material, kept in place below
        const Polygon& s = m->polygons[at];
        const int32_t a = i, b = (i + 1) | 1, c = (i + 2) & ~1;  // the strip's winding
        const uint32_t va = s.vertex_indices[a], vb = s.vertex_indices[b], vc = s.vertex_indices[c];
        if (va == vb || va == vc || vb == vc) continue;
        const int32_t k = m->count(0x10);
        Polygon* grown = new Polygon[static_cast<size_t>(k) + 1]{};
        for (int32_t q = 0; q < k; ++q) grown[q] = m->polygons[q];
        delete[] m->polygons;
        m->polygons = grown;
        { const int32_t c1 = k + 1; std::memcpy(m->header + 0x10, &c1, 4); }
        const Polygon& src = m->polygons[at];
        Polygon& t = m->polygons[k];
        t.flags = 3u | (src.flags & 0x800) | (src.flags & 0x1000);
        t.priority = src.priority;
        const int32_t pick[3] = {a, b, c};
        t.vertex_indices = static_cast<uint32_t*>(std::malloc(12));
        for (int q = 0; q < 3; ++q) t.vertex_indices[q] = src.vertex_indices[pick[q]];
        if (t.flags & 0x1000) {
          t.normal_indices = static_cast<uint32_t*>(std::malloc(12));
          for (int q = 0; q < 3; ++q) t.normal_indices[q] = src.normal_indices[pick[q]];
        }
        t.material_count = 1;
        t.materials = static_cast<::zmod::Material**>(std::malloc(sizeof(void*)));
        t.materials[0] = src.materials[j];
        if (src.uvs) {
          t.uvs = static_cast<float**>(std::malloc(sizeof(float*)));
          t.uvs[0] = static_cast<float*>(std::malloc(0x18));
          for (int q = 0; q < 3; ++q) std::memcpy(t.uvs[0] + 2 * q, src.uvs[j] + 2 * pick[q], 8);
          t.file_uvs = src.file_uvs;
        }
        t.colors = static_cast<float*>(std::malloc(0x24));
        for (int q = 0; q < 3; ++q) std::memcpy(t.colors + 3 * q, src.colors + 3 * pick[q], 12);
        t.scratch = static_cast<uint32_t*>(std::malloc(12));
        for (int q = 0; q < 3; ++q) t.scratch[q] = src.scratch[pick[q]];
        t.zone_set = src.zone_set;
      }
    }
    Polygon& p = m->polygons[at];  // the strip itself: its first triangle and first material
    p.flags = (p.flags & 0xfffffc03u) | 3;
    p.vertex_indices = static_cast<uint32_t*>(std::realloc(p.vertex_indices, 12));
    if (p.flags & 0x1000) p.normal_indices = static_cast<uint32_t*>(std::realloc(p.normal_indices, 12));
    if (p.material_count > 1) p.materials = static_cast<::zmod::Material**>(std::realloc(p.materials, sizeof(void*)));
    if (p.uvs) {
      for (int32_t j = 1; j < p.material_count; ++j) std::free(p.uvs[j]);
      if (p.material_count > 1) p.uvs = static_cast<float**>(std::realloc(p.uvs, sizeof(float*)));
      if (p.uvs[0]) p.uvs[0] = static_cast<float*>(std::realloc(p.uvs[0], 0x18));
    }
    p.material_count = 1;
    p.colors = static_cast<float*>(std::realloc(p.colors, 0x24));
    p.scratch = static_cast<uint32_t*>(std::realloc(p.scratch, 12));
  }
  { const int32_t f = m->count(8) & ~0x80; std::memcpy(m->header + 8, &f, 4); }
  if (appended) sort_by_material(*m);
}

Model* reread_model(std::FILE* f, int32_t index, int32_t material_base) {
  int32_t size, count, free_head;
  int line = 0;
  if (std::fread(&size, 4, 1, f) != 1) line = 0x2e1;
  else if (std::fread(&count, 4, 1, f) != 1) line = 0x2ee;
  else if (std::fread(&free_head, 4, 1, f) != 1) line = 0x2fb;
  if (line) {
    zerr::report(0x200, kFile, line, "Error reading GameZ Model3D buffer data.");
    zerr::report(0x200, kFile, 0x529, "Error reading GameZ Model3D buffer header data.");
    return nullptr;
  }
  if (size == 0 || size <= index) return nullptr;
  // FIX (bug): a node without a model (-1) has the exe read the 0x68 bytes before the table as one (its counts
  // garbage); the crater reset only re-reads ground nodes, which all have one (docs/fixes.md).
  if (index < 0) return nullptr;
  ::zmod::read_bound().begin(f);
  std::fseek(f, static_cast<long>(index) * 0x68, SEEK_CUR);
  uint8_t raw[0x68];
  if (std::fread(raw, sizeof raw, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x542, "Error reading GameZ Model3D buffer data.");
    return nullptr;
  }
  Model* m = model_alloc();  // grows the pool: never null here (the exe's 0x00558f50 can answer none)
  std::memcpy(m->header, raw, 0x64);
  uint32_t offset;
  std::memcpy(&offset, raw + 0x64, 4);
  std::fseek(f, static_cast<long>(offset), SEEK_SET);
  read_model_data(f, *m, material_base);  // 0x005587e0 always answers 0: the exe's clean-up after it never runs
  // Under Direct3D (0x00572ae0) the exe strips the model here (0x00570b70 with 0x7fffffff, 4; no 0x80 test), the
  // only shipped-data reach of PM's stripper; not ported (docs/pm-map.md, the models' form).
  return m;
}

// Pirate's Moon 1.0 @ 0x00558ba0, MW3's 0x00538e30 appending.
int read_models(std::FILE* f, int32_t material_base) {
  ModelPool& p = g_pool;
  const int32_t base = p.capacity;
  int32_t size, count, free_head;
  int line = 0;
  if (std::fread(&size, 4, 1, f) != 1) line = 0x2e1;
  else if (std::fread(&count, 4, 1, f) != 1) line = 0x2ee;
  else if (std::fread(&free_head, 4, 1, f) != 1) line = 0x2fb;
  if (line) {
    zerr::report(0x200, kFile, line, "Error reading GameZ Model3D buffer data.");
    zerr::report(0x200, kFile, 0x594, "Error reading GameZ Model3D buffer header data.");
    return -1;
  }
  if (size == 0) return 0;
  ::zmod::read_bound().begin(f);
  if (base == 0) p.count = count, p.free_head = free_head;
  for (int32_t done = 0; done < size; done += p.block) {
    const int32_t b = static_cast<int32_t>(p.blocks.size());
    p.blocks.emplace_back(new Model[static_cast<size_t>(p.block)]{});
    p.capacity += p.block;
    const int32_t n = size < done + p.block ? size - done : p.block;
    const size_t bytes = static_cast<size_t>(n) * 0x68;
    std::unique_ptr<uint8_t[]> raw(new uint8_t[bytes]());
    const bool ok = std::fread(raw.get(), 1, bytes, f) == bytes;  // a short read keeps what it read
    for (int32_t k = 0; k < n; ++k) {
      Model& m = p.blocks[static_cast<size_t>(b)][k];
      std::memcpy(m.header, raw.get() + k * 0x68, 0x64);
      std::memcpy(&m.link, raw.get() + k * 0x68 + 0x64, 4);
    }
    if (!ok) {
      zerr::report(0x200, kFile, 0x5c9, "Error reading GameZ Model3D buffer data.");
      return -1;
    }
    if (base == 0) continue;
    for (int32_t k = 0; k < n; ++k) {  // a later file's empty slots onto the free list
      Model& m = p.blocks[static_cast<size_t>(b)][k];
      if (m.count(0xc) != 0) continue;
      m.link = p.free_head;
      p.free_head = b * p.block + k;
    }
  }
  for (int32_t i = size + base; i < p.capacity; ++i) {  // the last block's rest
    Model& m = p.at(i);
    m = Model{};
    m.link = p.free_head;
    p.free_head = i;
  }
  static const uint8_t kZero[0x64] = {};
  for (int32_t i = base; i < size + base; ++i) {
    Model& m = p.at(i);
    if (m.count(0xc) < 1 && std::memcmp(m.header, kZero, 0x64) != 0)
      zerr::report(0x200, kFile, 0x606, "Reading GameZ Model3D buffer data for model that has no parents.");
    read_model_data(f, m, material_base);
  }
  return base;
}

}  // namespace pm::zmod

namespace pm::zmod {
namespace {
constexpr int32_t kMaxVerts = 48;         // 0x00611c04
constexpr double kMaxModelVerts = 921.6;  // 0x005f0a98, 0.9 x 1024

bool zeroish(float v) { return !(v < 0.0f || v > 0.0f); }  // the exe's fcom, as MW3's
void put_count(Model& m, int off, int32_t v) { std::memcpy(m.header + off, &v, 4); }

template <typename... A>
std::string format(const char* f, A... a) {
  char text[0x400];
  std::snprintf(text, sizeof text, f, a...);
  return text;
}
// 0x00527250: zGeometry's report of the shared text buffer.
void report_buffer(const std::string& text) {
  zerr::report(1, "C:\\zipper\\branchep1\\gamez\\zError\\zerr_old.c", 0x23, text.c_str());
}

// Pirate's Moon 1.0 @ 0x005599d0
int32_t find_vertex(Model& m, const float p[3]) {
  const int32_t n = m.count(0x14);
  const float precision = ::zmod::vertex_precision();
  for (int32_t i = 0; i < n; ++i) {
    const float* v = m.vertices + 3 * i;
    if (abs(X(v[0]) - p[0]) <= precision && abs(X(v[1]) - p[1]) <= precision && abs(X(v[2]) - p[2]) <= precision)
      return i;
  }
  if (kMaxModelVerts < n) {
    report_buffer(format("%s: Line %d: WARNING: Model vertex count = %d\n         Approaching max allowable: %d\n", kFile,
                         0x90d, n, 0x400));
    return -1;
  }
  m.vertices = static_cast<float*>(std::realloc(m.vertices, 12 * static_cast<size_t>(n + 1)));
  std::memcpy(m.vertices + 3 * n, p, 12);
  put_count(m, 0x14, n + 1);
  return n;
}

// Pirate's Moon 1.0 @ 0x00559ca0
int32_t find_normal(Model& m, const float p[3]) {
  const int32_t n = m.count(0x18);
  for (int32_t i = 0; i < n; ++i) {
    const float* v = m.normals + 3 * i;
    if (abs(X(v[0]) - p[0]) < 0.0001f && abs(X(v[1]) - p[1]) < 0.0001f && abs(X(v[2]) - p[2]) < 0.0001f)
      return i;
  }
  m.normals = static_cast<float*>(std::realloc(m.normals, 12 * static_cast<size_t>(n + 1)));
  std::memcpy(m.normals + 3 * n, p, 12);
  put_count(m, 0x18, n + 1);
  if (kMaxModelVerts < n + 1) {
    report_buffer(format("%s: Line %d: WARNING: Model normal count = %d\n         Approaching max allowable: %d\n", kFile,
                         0x9ca, n + 1, 0x400));
    return -1;
  }
  return n;
}

// Pirate's Moon 1.0 @ 0x0055c6d0. The next polygon record with its vertex
// and normal indices; 1 when the model is full (the record stays, uncounted).
// The exe also copies the materials and UVs here and again in 0x0055c910,
// leaking the first copy; they are copied there only.
int add_record(Model& m, int32_t n, const PolygonAdd& a) {
  const int32_t count = m.count(0x10);
  Polygon* grown = new Polygon[static_cast<size_t>(count) + 1]{};
  for (int32_t i = 0; i < count; ++i) grown[i] = m.polygons[i];
  delete[] m.polygons;
  m.polygons = grown;
  Polygon& q = m.polygons[count];
  q.priority = a.priority;
  q.flags = static_cast<uint32_t>(n & 0x3ff) | ((a.flag_800 & 1) << 1 | (a.back & 1)) << 10 | (a.normals ? 0x1000u : 0u);
  q.vertex_indices = static_cast<uint32_t*>(std::malloc(4 * static_cast<size_t>(n)));
  if (a.normals) q.normal_indices = static_cast<uint32_t*>(std::malloc(4 * static_cast<size_t>(n)));
  for (int32_t i = 0; i < n; ++i) {
    const int32_t v = find_vertex(m, a.points + 3 * i);
    q.vertex_indices[i] = static_cast<uint32_t>(v);
    if (v < 0) return 1;
    if (a.normals) {
      const int32_t k = find_normal(m, a.normals + 3 * i);
      q.normal_indices[i] = static_cast<uint32_t>(k);
      if (k < 0) return 1;
    }
  }
  return 0;
}

// Pirate's Moon 1.0 @ 0x0055be60: MW3's 0x0053bdc0 for each textured
// material's UVs.
void flatten_uvs(Model& m, int32_t index) {
  Polygon& q = m.polygons[index];
  const int32_t n = static_cast<int32_t>(q.flags & 0x3ff);
  if (!q.uvs || n <= 3) return;
  auto P = [&](int32_t k) { return m.vertices + 3 * q.vertex_indices[k]; };
  for (int32_t j = 0; j < q.material_count; ++j) {
    if (!(q.materials[j]->flags & 1)) continue;
    float uv[2 * 0x40];
    std::memcpy(uv, q.uvs[j], 8 * static_cast<size_t>(n));
    zmth::Vec3 normal;
    zmth::poly_normal(P(0), P(1), P(2), reinterpret_cast<float*>(&normal));
    zmth::normalize(normal);
    const float ax = std::fabs(normal.x), ay = std::fabs(normal.y), az = std::fabs(normal.z);
    int a, b;  // the two axes the gradients run along
    if (ax < ay || ax < az) {
      if (ay < ax || ay < az) a = 0, b = 1;
      else a = 2, b = 0;
    } else {
      a = 1, b = 2;
    }
    float gu[2], gv[2];
    ::zmod::uv_gradient(P(0)[a], P(0)[b], P(1)[a], P(1)[b], P(2)[a], P(2)[b], uv[0], uv[2], uv[4], gu);
    ::zmod::uv_gradient(P(0)[a], P(0)[b], P(1)[a], P(1)[b], P(2)[a], P(2)[b], uv[1], uv[3], uv[5], gv);
    for (int32_t k = 3; k < n; ++k) {
      if (zfix::options().fpu53) {  // the x87 keeps these as doubles; a float's rounding can cross 0x0055ac70's step
        const double da = static_cast<double>(P(k)[a]) - P(0)[a], db = static_cast<double>(P(k)[b]) - P(0)[b];
        q.uvs[j][2 * k] = static_cast<float>(da * gu[0] + db * gu[1] + uv[0]);
        q.uvs[j][2 * k + 1] = static_cast<float>(da * gv[0] + db * gv[1] + uv[1]);
        continue;
      }
      const float da = P(k)[a] - P(0)[a], db = P(k)[b] - P(0)[b];
      q.uvs[j][2 * k] = db * gu[1] + da * gu[0] + uv[0];
      q.uvs[j][2 * k + 1] = db * gv[1] + da * gv[0] + uv[1];
    }
  }
}

// Pirate's Moon 1.0 @ 0x0055c910. The record's materials, UVs (shifted near
// 0, then flattened and snapped), colours (white without), scratch and zone;
// the polygon counted.
int fill_record(Model& m, int32_t n, const PolygonAdd& a) {
  const int32_t index = m.count(0x10);
  Polygon& q = m.polygons[index];
  const size_t count = static_cast<size_t>(a.material_count > 0 ? a.material_count : 0);
  q.material_count = a.material_count;
  q.materials = static_cast<::zmod::Material**>(std::malloc((count ? count : 1) * sizeof(void*)));
  for (size_t j = 0; j < count; ++j) q.materials[j] = a.materials[j];
  if (a.uvs) {
    q.file_uvs = 1;
    q.uvs = static_cast<float**>(std::malloc((count ? count : 1) * sizeof(float*)));
    for (size_t j = 0; j < count; ++j) {
      float* uv = q.uvs[j] = static_cast<float*>(std::calloc(static_cast<size_t>(n), 8));
      if (!a.uvs[j]) continue;
      std::memcpy(uv, a.uvs[j], 8 * static_cast<size_t>(n));
      float mu = uv[0], mv = uv[1];
      for (int32_t i = 1; i < n; ++i) {
        if (uv[2 * i] < mu) mu = uv[2 * i];
        if (uv[2 * i + 1] < mv) mv = uv[2 * i + 1];
      }
      const double fu = std::floor(static_cast<double>(mu)), fv = std::floor(static_cast<double>(mv));
      for (int32_t i = 0; i < n; ++i) {
        uv[2 * i] = uv[2 * i] - static_cast<float>(fu);
        uv[2 * i + 1] = uv[2 * i + 1] - static_cast<float>(fv);
      }
    }
  }
  q.colors = static_cast<float*>(std::malloc(12 * static_cast<size_t>(n)));
  for (int32_t i = 0; i < 3 * n; ++i) q.colors[i] = a.colors ? a.colors[i] : 255.0f;
  q.scratch = static_cast<uint32_t*>(std::malloc(4 * static_cast<size_t>(q.flags & 0x3ff)));
  flatten_uvs(m, index);
  for (size_t j = 0; a.uvs && j < count; ++j)
    if (q.materials[j]->flags & 1) ::zmod::snap_uvs(n, q.uvs[j]);
  std::memcpy(&q.zone_set, a.zone, 4);
  put_count(m, 0x10, index + 1);
  return 0;
}

// The one piece's arrays for 0x0055a4d0 and 0x0055a870.
struct Piece {
  float pts[3 * 4], nrm[3 * 4], col[3 * 4];
  std::vector<std::vector<float>> uv;
  std::vector<const float*> uv_of;
  PolygonAdd add;
  explicit Piece(const PolygonAdd& a) : add(a) {
    if (a.uvs) {
      uv.assign(static_cast<size_t>(a.material_count), std::vector<float>(2 * 4));
      for (auto& u : uv) uv_of.push_back(u.data());
      add.uvs = uv_of.data();
    }
    add.points = pts, add.normals = a.normals ? nrm : nullptr, add.colors = a.colors ? col : nullptr;
  }
  void take(const PolygonAdd& a, int to, int from) {
    std::memcpy(pts + 3 * to, a.points + 3 * from, 12);
    if (a.normals) std::memcpy(nrm + 3 * to, a.normals + 3 * from, 12);
    if (a.colors) std::memcpy(col + 3 * to, a.colors + 3 * from, 12);
    for (size_t j = 0; j < uv.size(); ++j) std::memcpy(&uv[j][2 * static_cast<size_t>(to)], a.uvs[j] + 2 * from, 8);
  }
};

// Pirate's Moon 1.0 @ 0x0055a4d0. A non-planar polygon as the fan of triangles
// from its first point, each added on its own.
void add_fan(Model& m, int32_t n, const PolygonAdd& a) {
  Piece t(a);
  t.take(a, 0, 0);
  for (int32_t k = 1; k + 1 < n; ++k) {
    t.take(a, 1, k), t.take(a, 2, k + 1);
    float pts[9];  // 0x0055cb70 may drop points from what it is given
    std::memcpy(pts, t.pts, sizeof pts);
    PolygonAdd add = t.add;
    add.points = pts;
    model_add_polygon(&m, 3, add);
  }
}

// Pirate's Moon 1.0 @ 0x0055a870. A polygon past the vertex limit as fans of
// at most 4 points from its first point.
void add_pieces(Model& m, int32_t n, const PolygonAdd& a) {
  Piece t(a);
  t.take(a, 0, 0);
  int32_t piece = 4;
  for (int32_t i = 1; i < n - 1; i += piece - 2) {
    if (n + 1 < i + piece) piece = n - i + 1;
    for (int32_t k = 1; k < piece; ++k) t.take(a, k, i + k - 1);
    if (piece < 3) zerr::report(0x400, kFile, 0xc67, format("Attempting to add polygon with only %d verts", piece).c_str());
    float pts[12];
    std::memcpy(pts, t.pts, sizeof pts);
    PolygonAdd add = t.add;
    add.points = pts;
    model_add_polygon(&m, piece, add);
  }
}

template <typename T>
T* dup_array(const T* src, size_t n) {
  if (!src || n == 0) return nullptr;
  T* p = static_cast<T*>(std::malloc(n * sizeof(T)));
  std::memcpy(p, src, n * sizeof(T));
  return p;
}
}  // namespace

// Pirate's Moon 1.0 @ 0x0055cb70
int model_add_polygon(Model* m, int32_t n, const PolygonAdd& a) {
  if (n < 3) {
    zerr::report(0x200, kFile, 0x151b, format("ERROR: You're trying to add a Polygon with only (%d) verts", n).c_str());
    return 1;
  }
  if (57.6 < n) {
    zerr::report(0x200, kFile, 0x1525, format("Poly vertex count approaching limit (%d / %d)", n, 0x40).c_str());
    return 1;
  }
  int32_t k = n;
  std::vector<int32_t> kept(static_cast<size_t>(n));
  for (int32_t i = 0; i < n; ++i) kept[static_cast<size_t>(i)] = i;
  if (::zmod::drop_colinear(&k, a.points, &kept) && k < 3) {
    zerr::report(0x100, kFile, 0x1548,
                 format("Discarding Polygon: (%d of %d) verts after 'check_colinearity()'", k, n).c_str());
    return 1;
  }
  // FIX (bug): as MW3's 0x0053c6c0, the exe drops colinear points from the
  // points only, so what follows one belongs to the next point. zfix bugs:
  // they go too.
  std::vector<float> nrm, col;
  std::vector<std::vector<float>> uv;
  std::vector<const float*> uv_of;
  PolygonAdd use = a;
  if (k < n && zfix::options().fixed) {
    auto take = [&](const float* src, int width, std::vector<float>& to) {
      for (int32_t i : kept) to.insert(to.end(), src + width * i, src + width * (i + 1));
      return to.data();
    };
    if (a.normals) use.normals = take(a.normals, 3, nrm);
    if (a.colors) use.colors = take(a.colors, 3, col);
    if (a.uvs) {
      uv.resize(static_cast<size_t>(a.material_count));
      for (size_t j = 0; j < uv.size(); ++j) uv_of.push_back(a.uvs[j] ? take(a.uvs[j], 2, uv[j]) : nullptr);
      use.uvs = uv_of.data();
    }
  }
  if (k > 3) {
    const float* p = a.points;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f, sx = 0.0f, sy = 0.0f, sz = 0.0f;
    for (int32_t i = 0; i < k; ++i) {
      const float* c = p + 3 * i;
      const float* q = p + 3 * ((i + 1) % k);
      nx = ((X(c[1]) - q[1]) * (X(q[2]) + c[2]) + nx).f();
      ny = ((X(c[2]) - q[2]) * (X(q[0]) + c[0]) + ny).f();
      nz = ((X(c[0]) - q[0]) * (X(q[1]) + c[1]) + nz).f();
      sx = sx + c[0], sy = sy + c[1], sz = sz + c[2];
    }
    float len = 0.0f;
    if (!zeroish(nx) || !zeroish(ny) || !zeroish(nz)) len = sqrt(X(nx) * nx + X(ny) * ny + X(nz) * nz).f();
    const X inv = zeroish(len) ? X(0.0f) : X(1.0f) / len;
    const float ux = (inv * nx).f(), uy = (inv * ny).f(), uz = (inv * nz).f();
    const X nlen = X(static_cast<float>(k)) * len;
    const float dot = (X(sx) * nx + X(sy) * ny + X(sz) * nz).f();
    const float d = (-(X(dot) / nlen)).f();
    for (int32_t i = 0; i < k; ++i) {
      const float* c = p + 3 * i;
      if (abs(X(uz) * c[2] + X(c[0]) * ux + X(c[1]) * uy + d) > ::zmod::coplanar_tolerance()) {
        zerr::report(0x100, kFile, 0x1555,
                     format("Attempting to add non-planar polygon (%d verts), triangulating...", k).c_str());
        add_fan(*m, k, use);
        return 2;
      }
    }
  }
  if (k > kMaxVerts) {
    add_pieces(*m, k, use);
    return 2;
  }
  add_record(*m, k, use);
  int full = 0;
  std::string text;
  if (kMaxModelVerts < m->count(0x14)) {
    text = format("%s: Line %d: WARNING: Model vertex count = %d\n         Approaching max allowable: %d\n", kFile,
                  0x14c4, m->count(0x14), 0x400);
    report_buffer(text);
    full = 1;
  }
  if (kMaxModelVerts < m->count(0x18)) {
    text += format("%s: Line %d: WARNING: Model normal count = %d\n         Approaching max allowable: %d\n", kFile,
                   0x14d1, m->count(0x18), 0x400);
    report_buffer(text);
    full = 1;
  }
  if (full) return 1;
  return fill_record(*m, k, use);
}

// FIX (A) (docs/fixes.md, ours): ::zmod::model_inconsistent on PM's model, with its 10-bit point counts, its
// normals flag 0x1000 and the material refs' runs of polygons, which must lie in the polygon list.
const char* model_inconsistent(const Model& m) {
  const int32_t verts = m.vertices ? m.count(0x14) : 0, norms = m.normals ? m.count(0x18) : 0;
  const int32_t polys = m.polygons ? m.count(0x10) : 0;
  if (m.count(0x1c) > 0 && m.count(0x1c) > verts) return "morph list is longer than its points";
  for (int32_t i = 0; i < polys; ++i) {
    const Polygon& q = m.polygons[i];
    const uint32_t k = q.flags & 0x3ff;
    for (uint32_t j = 0; j < k; ++j) {
      if (!q.vertex_indices || q.vertex_indices[j] >= static_cast<uint32_t>(verts)) return "polygon names a point past its point list";
      if ((q.flags & 0x1000) && (!q.normal_indices || q.normal_indices[j] >= static_cast<uint32_t>(norms)))
        return "polygon names a normal past its normal list";
    }
  }
  int64_t used = 0;
  for (int32_t i = 0; m.refs && i < m.count(0x5c); ++i) {
    if (m.refs[i].usage_count < 0) return "material list has a negative count";
    used += m.refs[i].usage_count;
  }
  if (used > polys) return "material list covers more polygons than it has";
  return nullptr;
}

// FIX (A): a model's arrays freed and its counts 0 (polygons, points, normals, morphs, lights, refs), its slot kept.
void model_empty(Model& m) {
  for (int32_t i = 0; m.polygons && i < m.count(0x10); ++i) {
    Polygon& q = m.polygons[i];
    std::free(q.vertex_indices), std::free(q.normal_indices);
    if (q.uvs) {
      for (int32_t j = 0; j < q.material_count; ++j) std::free(q.uvs[j]);
      std::free(q.uvs);
    }
    std::free(q.materials), std::free(q.colors), std::free(q.scratch), std::free(q.library_indices);
  }
  delete[] m.polygons;
  std::free(m.vertices), std::free(m.normals), std::free(m.morphs);
  for (int32_t i = 0; m.lights && i < m.count(0x20); ++i) std::free(m.lights[i].vertices);
  std::free(m.lights);
  delete[] m.refs;
  m.polygons = nullptr, m.vertices = m.normals = m.morphs = nullptr, m.lights = nullptr, m.refs = nullptr;
  std::memset(m.header + 0x10, 0, 0x14);
  std::memset(m.header + 0x5c, 0, 4);
}

// Pirate's Moon 1.0 @ 0x005590f0
int model_release(Model* m) {
  if (!m) return 5;
  if (m->count(0xc) != 0) return 1;
  for (int32_t i = 0; i < m->count(0x10); ++i) {
    Polygon& q = m->polygons[i];
    std::free(q.vertex_indices), std::free(q.normal_indices);
    if (q.uvs) {
      for (int32_t j = 0; j < q.material_count; ++j) std::free(q.uvs[j]);
      std::free(q.uvs);
    }
    std::free(q.materials), std::free(q.colors), std::free(q.scratch), std::free(q.library_indices);
  }
  delete[] m->polygons;
  std::free(m->vertices), std::free(m->normals), std::free(m->morphs);
  if (m->lights) {
    for (int32_t i = 0; i < m->count(0x20); ++i) std::free(m->lights[i].vertices);
    std::free(m->lights);
  }
  delete[] m->refs;
  ModelPool& p = g_pool;
  int32_t index = -1;
  for (size_t b = 0; b < p.blocks.size() && index < 0; ++b)
    if (m >= p.blocks[b].get() && m < p.blocks[b].get() + p.block)
      index = static_cast<int32_t>(b) * p.block + static_cast<int32_t>(m - p.blocks[b].get());
  *m = Model{};
  m->link = p.free_head;
  --p.count;
  p.free_head = index;
  return 0;
}

// Pirate's Moon 1.0 @ 0x0055b0b0: MW3's 0x0053b3c0 over every material of every polygon.
int model_changes(const Model& m) {
  if (m.count(8) & 0x1c) return 1;
  for (int32_t i = 0; i < m.count(0x10); ++i) {
    const Polygon& q = m.polygons[i];
    for (int32_t j = 0; j < q.material_count; ++j)
      if (::zmod::material_changes(*q.materials[j])) return 1;  // 0x00556520
  }
  return 0;
}

// Pirate's Moon 1.0 @ 0x00559340: MW3's 0x005396c0 on PM's model. The flags' low five bits, +0, +4 and
// +0x24..+0x33 are copied; a material is copied (once a model) when `own_materials` and, with
// `shared`, it changes; the refs point into the copy's polygons.
Model* model_copy(const Model& m, int32_t own_materials, int32_t shared) {
  Model* c = model_alloc();
  if (!c) return nullptr;
  std::memcpy(c->header, m.header, 8);
  const auto put = [&](int off, int32_t v) { std::memcpy(c->header + off, &v, 4); };
  put(0xc, 0);
  put(8, static_cast<int32_t>((m.count(8) & 0x1f) | (c->count(8) & ~0x1f)));  // bit by bit in the exe
  std::memcpy(c->header + 0x24, m.header + 0x24, 0x10);
  if (const int32_t n = m.count(0x20); n != 0) {
    c->lights = static_cast<::zmod::Light*>(std::malloc(static_cast<size_t>(n) * sizeof(::zmod::Light)));
    for (int32_t i = 0; i < n; ++i) {
      c->lights[i] = m.lights[i];
      int32_t k;
      std::memcpy(&k, c->lights[i].raw + 0xc, 4);
      c->lights[i].vertices = dup_array(m.lights[i].vertices, k > 0 ? 3 * static_cast<size_t>(k) : 0);
    }
  }
  put(0x20, m.count(0x20));
  if (m.count(0x1c) > 0) c->morphs = dup_array(m.morphs, 3 * static_cast<size_t>(m.count(0x1c)));
  put(0x1c, m.count(0x1c));
  if (m.count(0x14) > 0) c->vertices = dup_array(m.vertices, 3 * static_cast<size_t>(m.count(0x14)));
  put(0x14, m.count(0x14));
  if (m.count(0x18) > 0) c->normals = dup_array(m.normals, 3 * static_cast<size_t>(m.count(0x18)));
  put(0x18, m.count(0x18));
  const int32_t polys = m.count(0x10);
  if (polys > 0) c->polygons = new Polygon[static_cast<size_t>(polys)]{};
  put(0x10, polys);
  std::vector<std::pair<::zmod::Material*, ::zmod::Material*>> made;
  for (int32_t i = 0; i < polys; ++i) {
    const Polygon& s = m.polygons[i];
    Polygon& d = c->polygons[i];
    d.priority = s.priority;
    d.flags = s.flags & 0x3400;  // back face, normals, strip
    d.zone_set = s.zone_set;
    d.file_uvs = s.file_uvs;
    d.material_count = s.material_count;
    d.materials = static_cast<::zmod::Material**>(std::malloc(static_cast<size_t>(s.material_count > 0 ? s.material_count : 1) * sizeof(void*)));
    for (int32_t j = 0; j < s.material_count; ++j) {
      ::zmod::Material* mat = s.materials[j];
      if (own_materials && (!shared || ::zmod::material_changes(*mat))) {
        ::zmod::Material* copy = nullptr;
        for (const auto& p : made)
          if (p.first == mat) copy = p.second;
        if (!copy) made.emplace_back(mat, copy = ::zmod::material_copy(*mat));  // 0x00557290
        mat = copy;
      }
      d.materials[j] = mat;
    }
    const uint32_t k = s.flags & 0x3ff;
    if (k) {
      d.vertex_indices = dup_array(s.vertex_indices, k);
      if ((s.flags & 0x1000) && s.normal_indices) d.normal_indices = dup_array(s.normal_indices, k);
    }
    d.flags = (d.flags & ~0x3ffu) | k;
    if (s.uvs) {
      d.uvs = static_cast<float**>(std::malloc(static_cast<size_t>(d.material_count > 0 ? d.material_count : 1) * sizeof(float*)));
      for (int32_t j = 0; j < d.material_count; ++j) d.uvs[j] = dup_array(s.uvs[j], 2 * static_cast<size_t>(k));
    }
    if (s.colors) d.colors = dup_array(s.colors, 3 * static_cast<size_t>(k));
    d.scratch = static_cast<uint32_t*>(std::malloc((k ? k : 1) * 4));
  }
  const int32_t refs = m.count(0x5c);
  put(0x5c, refs);
  if (refs > 0) {
    c->refs = new MaterialRef[static_cast<size_t>(refs)]{};
    Polygon* at = c->polygons;
    for (int32_t i = 0; i < refs; ++i) {
      c->refs[i] = MaterialRef{at && at->material_count > 0 ? at->materials[0] : nullptr, m.refs[i].usage_count, at};
      at += m.refs[i].usage_count;
    }
  }
  return c;
}

// Pirate's Moon 1.0 @ 0x00559310: returns 0 and does nothing else. A model nothing holds stays in the pool until the
// world goes (docs/pm-map.md, The unused models).
int32_t models_free_unused() { return 0; }

// Pirate's Moon 1.0 @ 0x00559320
void models_purge(bool models, bool materials) {
  if (models) models_free_unused();
  if (materials) ::zmod::materials_purge();
}

}  // namespace pm::zmod
