// Probes for test_fogsort.py (docs/drafts/fogsort.md). Self-contained: no shared helpers.
#include <cstdint>
#include <cstring>

#include "gamez/zerror/zerr.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_fogcalc.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zvideo/zvid_ddd3d.h"

namespace {

int32_t g_sends = 0;
void count_send() { ++g_sends; }

// fog: the 0x38 bytes at 0x74a22c; matrix: the 0x30 bytes at 0x73fc00.
void load_fog(const uint8_t* fog, const float* matrix) {
  static_assert(sizeof(zmod::Fog) == 0x38, "Fog mirrors 0x74a22c..0x74a263");
  std::memcpy(&zmod::fog(), fog, sizeof(zmod::Fog));
  std::memcpy(zmod::fog_calc().camera_to_world, matrix, 0x30);
}

// One call log entry: kind (1 render state, 2 stage state, 3 texture, 4 draw,
// 5 report, 6 zerr), then up to five arguments.
struct Log {
  int32_t* out = nullptr;
  int32_t cap = 0;
  int32_t n = 0;
  void add(int32_t kind, int32_t a = 0, int32_t b = 0, int32_t c = 0, int32_t d = 0, int32_t e = 0) {
    if (n < cap) {
      int32_t* r = out + n * 6;
      r[0] = kind, r[1] = a, r[2] = b, r[3] = c, r[4] = d, r[5] = e;
    }
    ++n;
  }
};
Log g_log;
int32_t g_draw_result = 0;  // DrawPrimitive's HRESULT: 0, or this for every draw

int32_t vertex_index(const zvid::TLVertex* v) {
  return static_cast<int32_t>(v - zvid::transparent_queue().verts);
}

void rs(void*, int32_t state, int32_t value) { g_log.add(1, state, value); }
void tss(void*, int32_t stage, int32_t type, int32_t value) { g_log.add(2, stage, type, value); }
void tex(void*, int32_t stage, void* t) {
  g_log.add(3, stage, static_cast<int32_t>(reinterpret_cast<intptr_t>(t)));
}
int32_t draw(void*, int32_t prim, uint32_t fvf, const zvid::TLVertex* v, int32_t count, uint32_t flags) {
  g_log.add(4, prim, static_cast<int32_t>(fvf), vertex_index(v), count, static_cast<int32_t>(flags));
  return g_draw_result;
}
void report(void*, int32_t hr, int32_t line) { g_log.add(5, hr, line); }
void zerr_sink(int level, const char*, int line, const char*) { g_log.add(6, level, line); }

zvid::D3DTexture g_textures[16];

}  // namespace

extern "C" {

float probe_fog_sphere(const uint8_t* fog, const float* matrix, const float* centre, float radius) {
  load_fog(fog, matrix);
  return zmod::fog_sphere(centre, radius);
}

// device: color[3], sent[3] in and out; *sends = calls of the device's fog call.
int32_t probe_fog_vertices(const uint8_t* fog, const float* matrix, const float* verts, int32_t count,
                           float* factor, float* device, int32_t* sends) {
  load_fog(fog, matrix);
  zmod::FogCalc& c = zmod::fog_calc();
  c.verts = reinterpret_cast<const float(*)[3]>(verts);
  std::memcpy(c.factor, factor, sizeof(float) * count);
  zrndr::DeviceFog& d = zrndr::device_fog();
  std::memcpy(d.color, device, 12);
  std::memcpy(d.sent, device + 3, 12);
  d.send = count_send;
  g_sends = 0;
  const int32_t r = zmod::fog_vertices(count);
  std::memcpy(factor, c.factor, sizeof(float) * count);
  std::memcpy(device, d.color, 12);
  std::memcpy(device + 3, d.sent, 12);
  *sends = g_sends;
  d.send = nullptr;
  return r;
}

// keys[i] = the sz bits of polygon i's first vertex; order in and out.
void probe_sort(const int32_t* keys, int32_t n, int32_t* order) {
  zvid::TransparentQueue& q = zvid::transparent_queue();
  q.count = static_cast<uint32_t>(n);
  for (int32_t i = 0; i < n; ++i) {
    q.polys[i].verts = &q.verts[i];
    std::memcpy(&q.verts[i].sz, &keys[i], 4);
  }
  std::memcpy(q.order, order, sizeof(int32_t) * n);
  zvid::sort_transparent();
  std::memcpy(order, q.order, sizeof(int32_t) * n);
}

void probe_queue_reset(uint32_t count, uint32_t vertex_count) {
  zvid::TransparentQueue& q = zvid::transparent_queue();
  q.count = count;
  q.vertex_count = vertex_count;
}

// Returns the vertex offset of the reserved slot, or -1 with *line the error line.
int32_t probe_queue(int32_t prim, int32_t count, int32_t texture, int32_t* line) {
  int32_t entry[6] = {};
  g_log = Log{entry, 1, 0};
  zerr::set_sink(zerr_sink);
  zvid::TLVertex* v = zvid::queue_transparent(prim, count, texture ? &g_textures[texture] : nullptr);
  zerr::set_sink(nullptr);
  *line = g_log.n ? entry[2] : 0;
  return v ? vertex_index(v) : -1;
}

// polys: n records {prim, count, texture slot (0 none), vertex offset};
// textures: 16 records {d3d id, type, address_u, address_v}; verts: the
// vertex pool (n_verts * 8 dwords); state: the 7 DeviceState ints in and out.
// Returns the number of log entries (6 ints each) written to log.
int32_t probe_flush(const int32_t* polys, int32_t n, const int32_t* textures, const uint8_t* verts,
                    int32_t n_verts, int32_t* state, int32_t draw_result, int32_t* log, int32_t cap,
                    int32_t* order) {
  zvid::TransparentQueue& q = zvid::transparent_queue();
  for (int i = 1; i < 16; ++i) {
    g_textures[i].d3d = reinterpret_cast<void*>(static_cast<intptr_t>(textures[i * 4]));
    g_textures[i].type = textures[i * 4 + 1];
    g_textures[i].address_u = textures[i * 4 + 2];
    g_textures[i].address_v = textures[i * 4 + 3];
  }
  std::memcpy(q.verts, verts, sizeof(zvid::TLVertex) * n_verts);
  q.count = static_cast<uint32_t>(n);
  q.vertex_count = static_cast<uint32_t>(n_verts);
  for (int32_t i = 0; i < n; ++i) {
    const int32_t* r = polys + i * 4;
    q.polys[i] = {r[0], r[1], r[2] ? &g_textures[r[2]] : nullptr, &q.verts[r[3]]};
  }
  zvid::DeviceState& s = zvid::device_state();
  s = {state[0], state[1], state[2], state[3], state[4], state[5], state[6]};
  zvid::Device& d = zvid::device();
  d = {nullptr, rs, tss, tex, draw, report};
  g_log = Log{log, cap, 0};
  g_draw_result = draw_result;
  zvid::flush_transparent();
  d = zvid::Device{};
  int32_t out[7] = {s.primitive, s.alpha_blend, s.shade_mode, s.alpha_op, s.address_u, s.address_v, s.z_write};
  std::memcpy(state, out, sizeof out);
  std::memcpy(order, q.order, sizeof(int32_t) * n);
  order[n] = static_cast<int32_t>(q.count);
  order[n + 1] = static_cast<int32_t>(q.vertex_count);
  return g_log.n;
}

}  // extern "C"
