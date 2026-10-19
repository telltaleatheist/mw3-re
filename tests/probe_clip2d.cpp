// Probes for test_clip2d.py (docs/render.md). Self-contained: no shared helpers.
#include <cstdint>
#include <cstring>

#include "gamez/zmodel/gmod_clip2d.h"

extern "C" {

int32_t probe_clip2d_slots() { return zmod::kClipVerts; }
int32_t probe_clip2d_shadow_size() { return sizeof(zmod::ShadowSets); }

// Loads the buffers (kClipVerts * 0x20 bytes each; the shadow sets as at
// 0x7489a8, 0xe20 bytes, and their partner 0xe00), runs clipper `which` (0 flat
// 0x529ce0, 1 lit 0x52aaf0, 2 textured 0x52bd10, 3 textured lit 0x52dc80,
// 4 0x52e920, 5 0x52fab0) and writes them back. Returns the clipper's result.
int32_t probe_clip2d(int32_t which, const void* rect, void* tl, void* work, void* tl_tmp,
                     void* work_tmp, void* shadow, void* shadow_tmp, int32_t* count) {
  zmod::Clip2D& b = zmod::clip2d();
  zmod::ScreenClip r;
  std::memcpy(&r, rect, sizeof r);
  std::memcpy(b.tl, tl, sizeof b.tl);
  std::memcpy(b.work, work, sizeof b.work);
  std::memcpy(b.tl_tmp, tl_tmp, sizeof b.tl_tmp);
  std::memcpy(b.work_tmp, work_tmp, sizeof b.work_tmp);
  std::memcpy(&b.shadow, shadow, sizeof b.shadow);
  std::memcpy(b.shadow_tmp, shadow_tmp, sizeof b.shadow_tmp);
  int32_t ret = 0;
  switch (which) {
    case 0: ret = zmod::clip2d_flat(r, count); break;
    case 1: ret = zmod::clip2d_lit(r, count); break;
    case 2: ret = zmod::clip2d_textured(r, count); break;
    case 3: ret = zmod::clip2d_textured_lit(r, count); break;
    case 4: ret = zmod::clip2d_textured_shadowed(r, count); break;
    case 5: ret = zmod::clip2d_textured_lit_shadowed(r, count); break;
  }
  std::memcpy(tl, b.tl, sizeof b.tl);
  std::memcpy(work, b.work, sizeof b.work);
  std::memcpy(tl_tmp, b.tl_tmp, sizeof b.tl_tmp);
  std::memcpy(work_tmp, b.work_tmp, sizeof b.work_tmp);
  std::memcpy(shadow, &b.shadow, sizeof b.shadow);
  std::memcpy(shadow_tmp, b.shadow_tmp, sizeof b.shadow_tmp);
  return ret;
}

}  // extern "C"
