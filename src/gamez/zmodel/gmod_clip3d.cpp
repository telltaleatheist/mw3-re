#include "gamez/zmodel/gmod_clip3d.h"

// Semantics, quirks and the emulator check: docs/drafts/clip3d.md.
namespace zmod {

Clip3dBuffers& clip3d_buffers() {
  static Clip3dBuffers b;
  return b;
}

namespace {

// `prev + (cur - prev) * t` in the x87 order. Two statements so the compiler
// cannot fuse it into an FMA the original never did.
float lerp(float cur, float prev, float t) {
  float d = (cur - prev) * t;
  return d + prev;
}

// x87 `fcomp; test ah, 1` takes the branch on below or unordered, so a NaN z
// is never "at or beyond" a plane.
bool at_or_beyond(float z, float plane) { return z >= plane; }

// The shared start of every clipper. Returns 0 (drop), 1 (keep as is) or -1
// (clip against the near plane).
int pre_test(const ClipPlanes& clip, int32_t count, const float (*cam)[3]) {
  if (clip.mask & 0x20) {
    // The far plane only drops a polygon wholly at or beyond it; it never cuts.
    int i = 0;
    while (i < count && at_or_beyond(cam[i][2], clip.far_z)) ++i;
    if (i >= count) return 0;
  }
  if (!(clip.mask & 0x10)) return 1;
  int i = 0;
  while (i < count && at_or_beyond(cam[i][2], clip.near_z)) ++i;
  if (i >= count) return count >= 3;  // nothing in front of the near plane
  return -1;
}

void copy_cam_back(Clip3dBuffers& b, int out) {
  for (int i = 0; i < out; ++i)
    for (int k = 0; k < 3; ++k) b.cam[i][k] = b.cam_out[i][k];
}

// The cut point between vertex `prev` and `cur`; returns t.
float cut(Clip3dBuffers& b, float near_z, int cur, int prev, int out) {
  float zp = b.cam[prev][2];
  float t = (near_z - zp) / (b.cam[cur][2] - zp);
  b.cam_out[out][2] = near_z;
  b.cam_out[out][0] = lerp(b.cam[cur][0], b.cam[prev][0], t);
  b.cam_out[out][1] = lerp(b.cam[cur][1], b.cam[prev][1], t);
  return t;
}

void keep(Clip3dBuffers& b, int cur, int out) {
  for (int k = 0; k < 3; ++k) b.cam_out[out][k] = b.cam[cur][k];
}

// Shadow set s, vertex v: the sets sit 64 vertices apart.
int sv(int s, int v) { return Clip3dBuffers::kVerts * s + v; }

void cut_shadows(Clip3dBuffers& b, int cur, int prev, int out, float t) {
  for (int s = 0; s < b.shadow_count; ++s)
    for (int k = 0; k < 2; ++k)
      b.shadow_out[sv(s, out)][k] = lerp(b.shadow_uv[sv(s, cur)][k], b.shadow_uv[sv(s, prev)][k], t);
}

void keep_shadows(Clip3dBuffers& b, int cur, int out) {
  for (int s = 0; s < b.shadow_count; ++s)
    for (int k = 0; k < 2; ++k) b.shadow_out[sv(s, out)][k] = b.shadow_uv[sv(s, cur)][k];
}

void copy_shadows_back(Clip3dBuffers& b, int out) {
  for (int s = 0; s < b.shadow_count; ++s)
    for (int i = 0; i < out; ++i)
      for (int k = 0; k < 2; ++k) b.shadow_uv[sv(s, i)][k] = b.shadow_out[sv(s, i)][k];
}

}  // namespace

// Mech3.exe 1.2 @ 0x005275e0
bool clip3d_flat(const ClipPlanes& clip, int32_t& count) {
  Clip3dBuffers& b = clip3d_buffers();
  int r = pre_test(clip, count, b.cam);
  if (r >= 0) return r != 0;
  int out = 0;
  int prev = count - 1;
  for (int i = 0; i < count; ++i) {
    bool in = at_or_beyond(b.cam[i][2], clip.near_z);
    if (in != at_or_beyond(b.cam[prev][2], clip.near_z)) cut(b, clip.near_z, i, prev, out++);
    if (in) keep(b, i, out++);
    prev = i;
  }
  count = out;
  if (out < 3) return false;
  copy_cam_back(b, out);
  return true;
}

// Mech3.exe 1.2 @ 0x00527a30
bool clip3d_lit(const ClipPlanes& clip, int32_t& count) {
  Clip3dBuffers& b = clip3d_buffers();
  int r = pre_test(clip, count, b.cam);
  if (r >= 0) return r != 0;
  int out = 0;
  int prev = count - 1;
  for (int i = 0; i < count; ++i) {
    bool in = at_or_beyond(b.cam[i][2], clip.near_z);
    if (in != at_or_beyond(b.cam[prev][2], clip.near_z)) {
      float t = cut(b, clip.near_z, i, prev, out);
      b.rec_out[out][0] = lerp(b.rec[i][0], b.rec[prev][0], t);
      b.rec_out[out][1] = b.rec[prev][1];  // the point light is not interpolated
      b.rec_out[out][2] = lerp(b.rec[i][2], b.rec[prev][2], t);
      ++out;
    }
    if (in) {
      keep(b, i, out);
      for (int k = 0; k < 3; ++k) b.rec_out[out][k] = b.rec[i][k];
      ++out;
    }
    prev = i;
  }
  count = out;
  if (out < 3) return false;
  copy_cam_back(b, out);
  for (int i = 0; i < out; ++i)
    for (int k = 0; k < 3; ++k) b.rec[i][k] = b.rec_out[i][k];
  return true;
}

// Mech3.exe 1.2 @ 0x00527fd0
bool clip3d_textured(const ClipPlanes& clip, int32_t& count) {
  Clip3dBuffers& b = clip3d_buffers();
  int r = pre_test(clip, count, b.cam);
  if (r >= 0) return r != 0;
  int out = 0;
  int prev = count - 1;
  for (int i = 0; i < count; ++i) {
    bool in = at_or_beyond(b.cam[i][2], clip.near_z);
    if (in != at_or_beyond(b.cam[prev][2], clip.near_z)) {
      float t = cut(b, clip.near_z, i, prev, out);
      for (int k = 3; k < 6; ++k) b.rec_out[out][k] = lerp(b.rec[i][k], b.rec[prev][k], t);
      for (int k = 6; k < 8; ++k) b.tl_out[out][k] = lerp(b.tl[i][k], b.tl[prev][k], t);
      ++out;
    }
    if (in) {
      keep(b, i, out);
      for (int k = 3; k < 6; ++k) b.rec_out[out][k] = b.rec[i][k];
      for (int k = 6; k < 8; ++k) b.tl_out[out][k] = b.tl[i][k];
      ++out;
    }
    prev = i;
  }
  count = out;
  if (out < 3) return false;
  copy_cam_back(b, out);
  for (int i = 0; i < out; ++i) {
    for (int k = 3; k < 6; ++k) b.rec[i][k] = b.rec_out[i][k];
    for (int k = 6; k < 8; ++k) b.tl[i][k] = b.tl_out[i][k];
  }
  return true;
}

// Mech3.exe 1.2 @ 0x005286e0
bool clip3d_textured_lit(const ClipPlanes& clip, int32_t& count) {
  Clip3dBuffers& b = clip3d_buffers();
  int r = pre_test(clip, count, b.cam);
  if (r >= 0) return r != 0;
  int out = 0;
  int prev = count - 1;
  for (int i = 0; i < count; ++i) {
    bool in = at_or_beyond(b.cam[i][2], clip.near_z);
    if (in != at_or_beyond(b.cam[prev][2], clip.near_z)) {
      float t = cut(b, clip.near_z, i, prev, out);
      b.rec_out[out][0] = lerp(b.rec[i][0], b.rec[prev][0], t);
      b.rec_out[out][1] = b.rec[prev][1];  // as in 0x527a30
      b.rec_out[out][2] = lerp(b.rec[i][2], b.rec[prev][2], t);
      for (int k = 3; k < 6; ++k) b.rec_out[out][k] = lerp(b.rec[i][k], b.rec[prev][k], t);
      for (int k = 6; k < 8; ++k) b.tl_out[out][k] = lerp(b.tl[i][k], b.tl[prev][k], t);
      ++out;
    }
    if (in) {
      keep(b, i, out);
      for (int k = 0; k < 6; ++k) b.rec_out[out][k] = b.rec[i][k];
      for (int k = 6; k < 8; ++k) b.tl_out[out][k] = b.tl[i][k];
      ++out;
    }
    prev = i;
  }
  count = out;
  if (out < 3) return false;
  copy_cam_back(b, out);
  for (int i = 0; i < out; ++i) {
    for (int k = 0; k < 6; ++k) b.rec[i][k] = b.rec_out[i][k];
    for (int k = 6; k < 8; ++k) b.tl[i][k] = b.tl_out[i][k];
  }
  return true;
}

// Mech3.exe 1.2 @ 0x00528bb0
bool clip3d_textured_shadow(const ClipPlanes& clip, int32_t& count) {
  Clip3dBuffers& b = clip3d_buffers();
  int r = pre_test(clip, count, b.cam);
  if (r >= 0) return r != 0;
  int out = 0;
  int prev = count - 1;
  for (int i = 0; i < count; ++i) {
    bool in = at_or_beyond(b.cam[i][2], clip.near_z);
    if (in != at_or_beyond(b.cam[prev][2], clip.near_z)) {
      float t = cut(b, clip.near_z, i, prev, out);
      for (int k = 3; k < 6; ++k) b.rec_out[out][k] = lerp(b.rec[i][k], b.rec[prev][k], t);
      for (int k = 6; k < 8; ++k) b.tl_out[out][k] = lerp(b.tl[i][k], b.tl[prev][k], t);
      cut_shadows(b, i, prev, out, t);
      ++out;
    }
    if (in) {
      keep(b, i, out);
      keep_shadows(b, i, out);
      for (int k = 3; k < 6; ++k) b.rec_out[out][k] = b.rec[i][k];
      for (int k = 6; k < 8; ++k) b.tl_out[out][k] = b.tl[i][k];
      ++out;
    }
    prev = i;
  }
  count = out;
  if (out < 3) return false;
  copy_cam_back(b, out);
  for (int i = 0; i < out; ++i) {
    for (int k = 3; k < 6; ++k) b.rec[i][k] = b.rec_out[i][k];
    for (int k = 6; k < 8; ++k) b.tl[i][k] = b.tl_out[i][k];
  }
  copy_shadows_back(b, out);
  return true;
}

// Mech3.exe 1.2 @ 0x00529120
bool clip3d_textured_lit_shadow(const ClipPlanes& clip, int32_t& count) {
  Clip3dBuffers& b = clip3d_buffers();
  int r = pre_test(clip, count, b.cam);
  if (r >= 0) return r != 0;
  int out = 0;
  int prev = count - 1;
  for (int i = 0; i < count; ++i) {
    bool in = at_or_beyond(b.cam[i][2], clip.near_z);
    if (in != at_or_beyond(b.cam[prev][2], clip.near_z)) {
      float t = cut(b, clip.near_z, i, prev, out);
      b.rec_out[out][0] = lerp(b.rec[i][0], b.rec[prev][0], t);
      b.rec_out[out][1] = b.rec[prev][1];  // as in 0x527a30
      b.rec_out[out][2] = lerp(b.rec[i][2], b.rec[prev][2], t);
      for (int k = 3; k < 6; ++k) b.rec_out[out][k] = lerp(b.rec[i][k], b.rec[prev][k], t);
      for (int k = 6; k < 8; ++k) b.tl_out[out][k] = lerp(b.tl[i][k], b.tl[prev][k], t);
      cut_shadows(b, i, prev, out, t);
      ++out;
    }
    if (in) {
      keep(b, i, out);
      keep_shadows(b, i, out);
      for (int k = 0; k < 6; ++k) b.rec_out[out][k] = b.rec[i][k];
      for (int k = 6; k < 8; ++k) b.tl_out[out][k] = b.tl[i][k];
      ++out;
    }
    prev = i;
  }
  count = out;
  if (out < 3) return false;
  copy_cam_back(b, out);
  for (int i = 0; i < out; ++i) {
    for (int k = 0; k < 6; ++k) b.rec[i][k] = b.rec_out[i][k];
    for (int k = 6; k < 8; ++k) b.tl[i][k] = b.tl_out[i][k];
  }
  copy_shadows_back(b, out);
  return true;
}

}  // namespace zmod
