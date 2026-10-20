#include "gamez/zvideo/zvid_dd.h"

#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zrender/zrndr_span.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zvideo/zvid_ddd3d.h"

namespace zvid {

namespace {
Surfaces g_surfaces;
SurfaceHooks g_hooks;
int32_t g_depth = 0;
TextTarget g_text;
}  // namespace

Surfaces& surfaces() { return g_surfaces; }
SurfaceHooks& surface_hooks() { return g_hooks; }
int32_t& scene_depth() { return g_depth; }
TextTarget& text_target() { return g_text; }

// Mech3.exe 1.2 @ 0x00577590. The primary is locked only when
// [0x008026e4] allows it; a locked surface is left as it is.
int32_t lock_surface(Surface* s) {
  if (!g_surfaces.primary_lockable && s == &g_surfaces.primary) return 0;
  if (s->locked) return 0;
  Surface got = *s;
  const int32_t r = g_hooks.lock ? g_hooks.lock(g_hooks.ctx, &got) : 1;
  if (r == 0) {
    s->width = got.width;
    s->locked = 1;
    s->height = got.height;
    s->valid = 1;
    s->pitch = got.pitch;
    s->pixels = got.pixels;
  }
  return r;
}

// Mech3.exe 1.2 @ 0x00577600
int32_t unlock_surface(Surface* s) {
  if (!g_surfaces.primary_lockable && s == &g_surfaces.primary) return 0;
  if (s->locked == 0) return 0;
  s->locked = 0;
  return g_hooks.unlock ? g_hooks.unlock(g_hooks.ctx, s) : 0;
}

// Mech3.exe 1.2 @ 0x00579af0
int32_t scene_begin() {
  if (g_depth < 1) {
    begin_scene();
    ++g_depth;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00579b10
int32_t scene_end() {
  if (0 < g_depth) {
    if (g_depth < 2) end_scene();
    --g_depth;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x005a67a0 (ECX pixels, EDX width; height, pitch) -> 0x005a66d0.
void set_text_target(uint16_t* pixels, int32_t width, int32_t height, int32_t pitch) {
  g_text.pixels = pixels, g_text.width = width, g_text.height = height, g_text.pitch = pitch;
}

// Mech3.exe 1.2 @ 0x00576d00. 0x00557f90 sets the fillers' target and the
// blit's (the port's two views of 0x008007c8): pixels and pitch, no rect.
int32_t lock_3d() {
  Surface& b = g_surfaces.back;
  lock_surface(&b);
  auto* px = reinterpret_cast<uint8_t*>(b.pixels);
  zrndr::set_target(px, nullptr, 0, b.pitch);
  zrndr::target2d().base = px;
  zrndr::target2d().pitch = static_cast<uint32_t>(b.pitch);
  zrndr::set_finish_target(b.pixels, b.width, b.height, b.pitch);
  set_text_target(b.pixels, b.width, b.height, b.pitch);
  return 0;
}

// Mech3.exe 1.2 @ 0x00576d60
void unlock_3d() { unlock_surface(&g_surfaces.back); }

}  // namespace zvid
