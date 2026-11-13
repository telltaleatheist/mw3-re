// Probe for tests/differential/explore/strip_hits.py: the port's model
// ground and segment tests (bit for bit the exe's, strips included:
// test_di.py) on a mechlib model as the exe's reader left it and as the
// hardware mode's stripper 0x00552540 leaves it (docs/mechlib.md).
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_di.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_hit.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zmodel/gmod_strip.h"
#include <cstdlib>

namespace {
struct Built {
  zmod::Model model;
  std::vector<zmod::Polygon> polys;
  std::vector<std::vector<uint32_t>> idx, nidx;
  std::vector<std::vector<float>> uvs, cols;
  std::vector<float> verts;
  std::vector<zmod::Material> mats;
};
Built g_slot[2];

template <class T>
T take(const uint8_t*& p) {
  T v;
  std::memcpy(&v, p, sizeof v);
  p += sizeof v;
  return v;
}

uint32_t mat_index(const Built& b, const zmod::Material* m) {
  return m ? static_cast<uint32_t>(m - b.mats.data()) : 0xffffffffu;
}
}  // namespace

extern "C" {
// header: the exe's 0x5c bytes; polys: per polygon flags, priority, material
// index, zone, then its vertex, normal, UV and colour lists (each prefixed by
// a present byte); mat_flags: each material's +0x01.
int strip_load(int slot, const uint8_t* header, const float* verts, int nverts, const uint8_t* polys, int npolys,
               const uint8_t* mat_flags, int nmats) {
  Built& b = g_slot[slot];
  b = Built{};
  b.mats.assign(static_cast<size_t>(nmats), zmod::Material{});
  for (int i = 0; i < nmats; ++i) b.mats[i].flags = mat_flags[i];
  b.verts.assign(verts, verts + 3 * nverts);
  b.polys.resize(static_cast<size_t>(npolys));
  b.idx.resize(npolys), b.nidx.resize(npolys), b.uvs.resize(npolys), b.cols.resize(npolys);
  const uint8_t* p = polys;
  for (int i = 0; i < npolys; ++i) {
    zmod::Polygon& q = b.polys[i];
    q.flags = take<uint32_t>(p);
    q.priority = take<int32_t>(p);
    const int32_t mi = take<int32_t>(p);
    q.zone_set = take<uint32_t>(p);
    q.material = mi >= 0 && mi < nmats ? &b.mats[mi] : nullptr;
    const uint32_t k = q.flags & 0xff;
    if (take<uint8_t>(p)) {
      b.idx[i].resize(k);
      std::memcpy(b.idx[i].data(), p, 4 * k), p += 4 * k;
      q.vertex_indices = b.idx[i].data();
    }
    if (take<uint8_t>(p)) {
      b.nidx[i].resize(k);
      std::memcpy(b.nidx[i].data(), p, 4 * k), p += 4 * k;
      q.normal_indices = b.nidx[i].data();
    }
    if (take<uint8_t>(p)) {
      b.uvs[i].resize(2 * k);
      std::memcpy(b.uvs[i].data(), p, 8 * k), p += 8 * k;
      q.uvs = b.uvs[i].data();
    }
    if (take<uint8_t>(p)) {
      b.cols[i].resize(3 * k);
      std::memcpy(b.cols[i].data(), p, 12 * k), p += 12 * k;
      q.colors = b.cols[i].data();
    }
  }
  std::memcpy(b.model.header, header, 0x5c);
  const uint32_t np = static_cast<uint32_t>(npolys), nv = static_cast<uint32_t>(nverts), zero = 0;
  std::memcpy(b.model.header + 0x10, &np, 4);
  std::memcpy(b.model.header + 0x14, &nv, 4);
  std::memcpy(b.model.header + 0x58, &zero, 4);
  b.model.polygons = b.polys.data();
  b.model.vertices = b.verts.data();
  zmth::matrix_stack().is_identity() = 1;
  return static_cast<int>(p - polys);
}

// Per probe: the count, then each hit's normal, height, zone and material
// index (6 + 1 words); the cached polygon last. Returns the words written.
int strip_ground(int slot, const float* probes, int count, float top, uint32_t* out) {
  Built& b = g_slot[slot];
  std::vector<zcls::GroundHits> rec(static_cast<size_t>(count));
  for (auto& r : rec) std::memset(&r, 0, sizeof r);
  std::vector<int32_t> mask(static_cast<size_t>(count), 1);
  zmod::model_ground(nullptr, &b.model, probes, mask.data(), count, top, rec.data());
  int n = 0;
  for (const auto& r : rec) {
    out[n++] = r.count;
    for (int j = 0; j < r.count; ++j) {
      const zcls::GroundHit& h = r.hit[j];
      std::memcpy(out + n, h.normal, 12), n += 3;
      std::memcpy(out + n, &h.height, 4), ++n;
      out[n++] = h.zone[0];
      out[n++] = mat_index(b, h.material);
    }
  }
  std::memcpy(out + n++, b.model.header + 0x58, 4);
  return n;
}

// segs: count segments as (from, to); per segment the count, then each
// crossing's normal, point and material index.
int strip_segments(int slot, const float* segs, int count, uint32_t* out) {
  Built& b = g_slot[slot];
  std::vector<zcls::SegmentHits> rec(static_cast<size_t>(count));
  for (auto& r : rec) std::memset(&r, 0, sizeof r);
  std::vector<int32_t> mask(static_cast<size_t>(count), 1);
  zmod::model_segments(nullptr, &b.model, segs, mask.data(), count, rec.data());
  int n = 0;
  for (const auto& r : rec) {
    out[n++] = r.count;
    for (int j = 0; j < r.count; ++j) {
      const zcls::SegmentHit& h = r.hit[j];
      std::memcpy(out + n, h.normal, 12), n += 3;
      std::memcpy(out + n, h.point, 12), n += 3;
      out[n++] = mat_index(b, h.material);
    }
  }
  std::memcpy(out + n++, b.model.header + 0x58, 4);
  return n;
}

// The port's stripper (zmod::strip_model) on slot `slot`'s model as the reader leaves it (each array malloc'd,
// as the exe's), then the model dumped as strip_hits.py / test_mech_strips.py dump the exe's: per polygon
// flags, priority, material index, zone, then the vertex, normal, UV and colour lists, each behind a present
// byte. Returns the bytes written to out (cap bytes at most), the polygon count in *npolys.
int strip_port(int slot, uint32_t limit, uint32_t small, uint8_t* out, int cap, int* npolys) {
  Built& b = g_slot[slot];
  zmod::Model m{};
  std::memcpy(m.header, b.model.header, 0x5c);
  const uint32_t n0 = static_cast<uint32_t>(b.polys.size());
  m.polygons = static_cast<zmod::Polygon*>(std::malloc(sizeof(zmod::Polygon) * (n0 ? n0 : 1)));
  for (uint32_t i = 0; i < n0; ++i) {
    zmod::Polygon q = b.polys[i];
    auto dup = [](const void* src, size_t bytes) -> void* {
      if (!src) return nullptr;
      void* d = std::malloc(bytes ? bytes : 1);
      std::memcpy(d, src, bytes);
      return d;
    };
    const size_t k = q.flags & 0xff;
    q.vertex_indices = static_cast<uint32_t*>(dup(q.vertex_indices, 4 * k));
    q.normal_indices = static_cast<uint32_t*>(dup(q.normal_indices, 4 * k));
    q.uvs = static_cast<float*>(dup(q.uvs, 8 * k));
    q.colors = static_cast<float*>(dup(q.colors, 12 * k));
    q.scratch = nullptr;
    m.polygons[i] = q;
  }
  m.vertices = b.verts.data();
  zmod::strip_model(m, limit, small);
  uint32_t np = 0;
  std::memcpy(&np, m.header + 0x10, 4);
  *npolys = static_cast<int>(np);
  int w = 0;
  auto put = [&](const void* src, int bytes) {
    if (w + bytes <= cap) std::memcpy(out + w, src, bytes);
    w += bytes;
  };
  for (uint32_t i = 0; i < np; ++i) {
    const zmod::Polygon& q = m.polygons[i];
    const int32_t mi = q.material ? static_cast<int32_t>(q.material - b.mats.data()) : -1;
    put(&q.flags, 4), put(&q.priority, 4), put(&mi, 4), put(&q.zone_set, 4);
    const int k = static_cast<int>(q.flags & 0xff);
    const void* arrs[4] = {q.vertex_indices, (q.flags & 0x200) ? q.normal_indices : nullptr, q.uvs, q.colors};
    const int sizes[4] = {4, 4, 8, 12};
    for (int a = 0; a < 4; ++a) {
      const uint8_t on = arrs[a] && k > 0;
      put(&on, 1);
      if (on) put(arrs[a], sizes[a] * k);
    }
  }
  for (uint32_t i = 0; i < np; ++i) {
    zmod::Polygon& q = m.polygons[i];
    std::free(q.vertex_indices), std::free(q.normal_indices), std::free(q.uvs), std::free(q.colors), std::free(q.scratch);
  }
  std::free(m.polygons);
  return w;
}
}
