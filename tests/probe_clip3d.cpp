// Probes for test_clip3d.py (docs/drafts/clip3d.md). Self-contained: no shared helpers.
#include <cstdint>
#include <cstring>

#include "gamez/zmodel/gmod_clip3d.h"

extern "C" {

// Size in bytes of each buffer, in the order probe_clip3d takes them.
void probe_clip3d_sizes(int32_t* sizes) {
  zmod::Clip3dBuffers& b = zmod::clip3d_buffers();
  sizes[0] = sizeof b.cam;
  sizes[1] = sizeof b.rec;
  sizes[2] = sizeof b.tl;
  sizes[3] = sizeof b.tl_out;
  sizes[4] = sizeof b.cam_out;
  sizes[5] = sizeof b.rec_out;
  sizes[6] = sizeof b.shadow_uv;
  sizes[7] = sizeof b.shadow_out;
}

// which: 0 flat 0x5275e0, 1 lit 0x527a30, 2 textured 0x527fd0,
// 3 textured lit 0x5286e0, 4 textured shadow 0x528bb0, 5 textured lit shadow
// 0x529120 (with `shadows` UV sets, [0x7489a8]). clip: the 0x24-byte struct at 0x7402f8.
// The buffers are read in and written back; returns EAX, *count the new count.
int32_t probe_clip3d(int which, const uint8_t* clip, int32_t* count, uint8_t* cam, uint8_t* rec,
                     uint8_t* tl, uint8_t* tl_out, uint8_t* cam_out, uint8_t* rec_out,
                     uint8_t* shadow_uv, uint8_t* shadow_out, int32_t shadows) {
  zmod::Clip3dBuffers& b = zmod::clip3d_buffers();
  zmod::ClipPlanes planes;
  std::memcpy(&planes, clip, sizeof planes);
  std::memcpy(b.cam, cam, sizeof b.cam);
  std::memcpy(b.rec, rec, sizeof b.rec);
  std::memcpy(b.tl, tl, sizeof b.tl);
  std::memcpy(b.tl_out, tl_out, sizeof b.tl_out);
  std::memcpy(b.cam_out, cam_out, sizeof b.cam_out);
  std::memcpy(b.rec_out, rec_out, sizeof b.rec_out);
  std::memcpy(b.shadow_uv, shadow_uv, sizeof b.shadow_uv);
  std::memcpy(b.shadow_out, shadow_out, sizeof b.shadow_out);
  b.shadow_count = shadows;
  bool r = false;
  switch (which) {
    case 0: r = zmod::clip3d_flat(planes, *count); break;
    case 1: r = zmod::clip3d_lit(planes, *count); break;
    case 2: r = zmod::clip3d_textured(planes, *count); break;
    case 3: r = zmod::clip3d_textured_lit(planes, *count); break;
    case 4: r = zmod::clip3d_textured_shadow(planes, *count); break;
    case 5: r = zmod::clip3d_textured_lit_shadow(planes, *count); break;
    default: return -1;
  }
  std::memcpy(cam, b.cam, sizeof b.cam);
  std::memcpy(rec, b.rec, sizeof b.rec);
  std::memcpy(tl, b.tl, sizeof b.tl);
  std::memcpy(tl_out, b.tl_out, sizeof b.tl_out);
  std::memcpy(cam_out, b.cam_out, sizeof b.cam_out);
  std::memcpy(rec_out, b.rec_out, sizeof b.rec_out);
  std::memcpy(shadow_uv, b.shadow_uv, sizeof b.shadow_uv);
  std::memcpy(shadow_out, b.shadow_out, sizeof b.shadow_out);
  return r ? 1 : 0;
}

}  // extern "C"
