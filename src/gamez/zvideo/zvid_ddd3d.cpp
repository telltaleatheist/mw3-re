#include "gamez/zvideo/zvid_ddd3d.h"

#include <cstdio>
#include <cstring>
#include <utility>

#include "gamez/zerror/zerr.h"

namespace zvid {

namespace {
TransparentQueue g_queue;
DeviceState g_state;
Device g_device;

constexpr const char* kFile = "C:\\zipper\\GameZMech3\\zVideo\\zvid_ddd3d.c";

// The sort key: the first vertex's sz bits as a signed int (negative floats
// order backwards among themselves; sz is never negative in play).
int32_t key(int32_t index) {
  int32_t k;
  std::memcpy(&k, &g_queue.polys[index].verts[0].sz, 4);
  return k;
}

void set_render_state(int32_t state, int32_t value) {
  if (g_device.set_render_state) g_device.set_render_state(g_device.ctx, state, value);
}

void set_texture_stage_state(int32_t type, int32_t value) {
  if (g_device.set_texture_stage_state) g_device.set_texture_stage_state(g_device.ctx, 0, type, value);
}
}  // namespace

TransparentQueue& transparent_queue() { return g_queue; }
DeviceState& device_state() { return g_state; }
Device& device() { return g_device; }

// Inlined at each transparent draw (lines 0x591 and 0x599 of zvid_ddd3d.c).
TLVertex* queue_transparent(int32_t primitive, int32_t count, const D3DTexture* texture) {
  TransparentQueue& q = g_queue;
  char msg[64];
  if (q.count >= TransparentQueue::kMaxPolys) {
    std::snprintf(msg, sizeof msg, "Not enough MAX_TRANSPARENT_POLYS: need %d", static_cast<int>(q.count + 1));
    zerr::report(0x400, kFile, 0x591, msg);
    return nullptr;
  }
  const uint32_t need = q.vertex_count + static_cast<uint32_t>(count);
  if (need > TransparentQueue::kMaxVerts) {
    std::snprintf(msg, sizeof msg, "Not enough MAX_TRANSPARENT_VERTICES: need %d", static_cast<int>(need));
    zerr::report(0x400, kFile, 0x599, msg);
    return nullptr;
  }
  TransparentPoly& p = q.polys[q.count++];
  p.primitive = primitive;
  p.count = count;
  p.texture = texture;
  p.verts = &q.verts[q.vertex_count];
  q.vertex_count = need;
  return p.verts;
}

// Mech3.exe 1.2 @ 0x0057dec0. Quicksort down to runs of 9, median of three
// (lo, mid, hi), then one insertion sort over the whole list; not stable.
void sort_transparent() {
  int32_t* order = g_queue.order;
  int32_t stack[300];  // 0x4b4 bytes of locals; the depth stays under 2 log2(6000)
  int32_t* sp = stack;
  int32_t lo = 0;
  int32_t hi = static_cast<int32_t>(g_queue.count) - 1;
  for (;;) {
    while (hi > lo + 8) {
      const int32_t mid = (hi + lo) >> 1;
      if (key(order[lo]) > key(order[mid])) std::swap(order[lo], order[mid]);
      if (key(order[lo]) > key(order[hi])) std::swap(order[lo], order[hi]);
      if (key(order[mid]) > key(order[hi])) std::swap(order[mid], order[hi]);
      const int32_t pivot = key(order[mid]);
      int32_t i = lo + 1;
      int32_t j = hi - 1;
      do {
        while (key(order[i]) < pivot) ++i;
        while (key(order[j]) > pivot) --j;
        if (i > j) break;
        std::swap(order[i], order[j]);
        ++i;
        --j;
      } while (i <= j);
      // Keep the smaller side, push the larger.
      if (j + i > hi + lo) {
        *sp++ = lo;
        *sp++ = j;
        lo = i;
      } else {
        *sp++ = i;
        *sp++ = hi;
        hi = j;
      }
    }
    if (sp <= stack) break;
    hi = *--sp;
    lo = *--sp;
  }
  for (int32_t k = 1; k < static_cast<int32_t>(g_queue.count); ++k) {
    const int32_t x = order[k];
    const int32_t kx = key(x);
    int32_t c = k;
    while (c > 0 && key(order[c - 1]) > kx) {
      order[c] = order[c - 1];
      --c;
    }
    order[c] = x;
  }
}

// Mech3.exe 1.2 @ 0x0057e110. FIX (A): the D3D calls go through device().
void flush_transparent() {
  TransparentQueue& q = g_queue;
  DeviceState& s = g_state;
  if (q.count == 0) return;
  if (s.shade_mode != 2) set_render_state(9, 2), s.shade_mode = 2;     // Gouraud
  if (s.alpha_blend != 1) set_render_state(27, 1), s.alpha_blend = 1;
  if (s.z_write != 0) set_render_state(14, 0), s.z_write = 0;
  for (uint32_t i = 0; i < q.count; ++i) q.order[i] = static_cast<int32_t>(q.count - 1 - i);
  sort_transparent();
  for (uint32_t n = 0; n < q.count; ++n) {
    const TransparentPoly& p = q.polys[q.order[n]];
    void* texture = nullptr;
    if (p.texture) {
      texture = p.texture->d3d;
      // Compares a texture type with the cached ALPHAOP: anything but 2
      // (never a type) sends MODULATE, so ALPHAOP is always MODULATE here.
      const int32_t type = p.verts[0].color > 0xff000000u ? 4 : p.texture->type;
      if (s.alpha_op != type) {
        s.alpha_op = type;
        set_texture_stage_state(4, type == 2 ? type : 4);
      }
      if (s.address_u != p.texture->address_u) {
        set_texture_stage_state(13, p.texture->address_u);
        s.address_u = p.texture->address_u;
      }
      if (s.address_v != p.texture->address_v) {
        set_texture_stage_state(14, p.texture->address_v);
        s.address_v = p.texture->address_v;
      }
    }
    if (g_device.set_texture) g_device.set_texture(g_device.ctx, 0, texture);
    if (g_device.draw_primitive) {
      const int32_t hr = g_device.draw_primitive(g_device.ctx, p.primitive, 0x1c4, p.verts, p.count, 0x18);
      if (hr != 0 && g_device.report) g_device.report(g_device.ctx, hr, 0xb6f);
    }
  }
  if (s.alpha_blend != 0) set_render_state(27, 0), s.alpha_blend = 0;
  if (s.z_write != 1) set_render_state(14, 1), s.z_write = 1;
  q.count = 0;
  q.vertex_count = 0;
}

}  // namespace zvid
