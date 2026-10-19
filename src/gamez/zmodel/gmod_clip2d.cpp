#include "gamez/zmodel/gmod_clip2d.h"

// The original rounds every step to float (Direct3D runs the FPU at 24 bits,
// docs/drafts/clip2d.md); no fused multiply-adds, so the steps stay the same.
#pragma STDC FP_CONTRACT OFF

namespace zmod {

namespace {

Clip2D g_clip;

// x87 FCOMP's C0: true when a < b or either is NaN.
bool below(float a, float b) { return !(a >= b); }

// The fields a clipper carries besides sx, sy, sz.
enum : unsigned { kUV = 1, kLight = 2, kColor = 4, kShadow = 8 };

using Sets = ShadowUV[kShadowSets][kShadowVerts];

// One side of the rect, Sutherland-Hodgman: each edge prev -> cur emits the
// crossing point when the two differ, then cur when inside.

template <unsigned F>
int pass(const TLVertex* stl, const VertexWork* swk, const Sets& ssh, TLVertex* dtl,
         VertexWork* dwk, Sets& dsh, int sets, int n, int axis, float edge, bool keep_below) {
  auto inside = [&](const TLVertex& v) {
    float c = axis == 0 ? v.sx : v.sy;
    return below(c, edge) == keep_below;
  };
  int out = 0;
  int prev = n - 1;
  for (int i = 0; i < n; prev = i, ++i) {
    const TLVertex& c = stl[i];
    const TLVertex& p = stl[prev];
    bool in = inside(c);
    if (in != inside(p)) {
      TLVertex& o = dtl[out];
      float t;
      if (axis == 0) {
        t = (edge - p.sx) / (c.sx - p.sx);
        o.sx = edge;
        o.sy = (c.sy - p.sy) * t + p.sy;
      } else {
        t = (edge - p.sy) / (c.sy - p.sy);
        o.sy = edge;
        o.sx = (c.sx - p.sx) * t + p.sx;
      }
      o.sz = (c.sz - p.sz) * t + p.sz;
      if (F != 0) {
        // Perspective-correct: sz is 1/z, so weight by sz and divide by the new one.
        float w = 1.0f / o.sz;
        float tw = t * w;
        float a = tw * c.sz;
        float b = (w - tw) * p.sz;
        if (F & kUV) {
          o.tu = p.tu * b + c.tu * a;
          o.tv = p.tv * b + c.tv * a;
        }
        for (int k = 0; k < 3; ++k) {
          if (F & kLight) dwk[out].light[k] = swk[prev].light[k] * b + swk[i].light[k] * a;
          if (F & kColor) dwk[out].color[k] = swk[prev].color[k] * b + swk[i].color[k] * a;
        }
        if (F & kShadow) {
          for (int k = 0; k < sets; ++k) {
            dsh[k][out].u = ssh[k][prev].u * b + ssh[k][i].u * a;
            dsh[k][out].v = ssh[k][prev].v * b + ssh[k][i].v * a;
          }
        }
      }
      ++out;
    }
    if (in) {
      TLVertex& o = dtl[out];
      o.sx = c.sx;
      o.sy = c.sy;
      o.sz = c.sz;
      if (F & kUV) {
        o.tu = c.tu;
        o.tv = c.tv;
      }
      for (int k = 0; k < 3; ++k) {
        if (F & kLight) dwk[out].light[k] = swk[i].light[k];
        if (F & kColor) dwk[out].color[k] = swk[i].color[k];
      }
      if (F & kShadow) {
        for (int k = 0; k < sets; ++k) dsh[k][out] = ssh[k][i];
      }
      ++out;
    }
  }
  return out;
}

template <unsigned F>
int32_t clip(const ScreenClip& r, int32_t* count) {
  Clip2D& b = g_clip;
  // The outcodes come from the vertices; the cull's plane mask only decides
  // whether the clipper is called. n <= 0 never reaches here (the original
  // would run off the buffer).
  unsigned code = 0;
  for (int i = *count - 1; i >= 0; --i) {
    const TLVertex& v = b.tl[i];
    if (below(v.sx, r.left)) code |= 1;
    else if (!below(v.sx, r.right)) code |= 2;
    if (below(v.sy, r.top)) code |= 4;
    else if (!below(v.sy, r.bottom)) code |= 8;
  }
  if (code == 0) return 1;

  // The original keeps one pointer per set in 7-slot stack arrays: more than
  // 7 would overrun them; the shadow code never sets more [unverified].
  const int sets = (F & kShadow) ? b.shadow.count : 0;
  bool in_tmp = false;  // which buffer pair holds the polygon now
  struct Side { unsigned bit; int axis; float edge; bool keep_below; };
  const Side sides[4] = {{1, 0, r.left, false}, {2, 0, r.right, true},
                         {4, 1, r.top, false}, {8, 1, r.bottom, true}};
  for (const Side& s : sides) {
    if (!(code & s.bit)) continue;
    int n = in_tmp ? pass<F>(b.tl_tmp, b.work_tmp, b.shadow_tmp, b.tl, b.work, b.shadow.uv, sets,
                             *count, s.axis, s.edge, s.keep_below)
                   : pass<F>(b.tl, b.work, b.shadow.uv, b.tl_tmp, b.work_tmp, b.shadow_tmp, sets,
                             *count, s.axis, s.edge, s.keep_below);
    *count = n;
    if (n < 3) return 0;
    in_tmp = !in_tmp;
  }
  if (in_tmp) {
    for (int i = *count - 1; i >= 0; --i) {
      TLVertex& d = b.tl[i];
      const TLVertex& s = b.tl_tmp[i];
      d.sx = s.sx;
      d.sy = s.sy;
      d.sz = s.sz;
      if (F & kUV) {
        d.tu = s.tu;
        d.tv = s.tv;
      }
      for (int k = 0; k < 3; ++k) {
        if (F & kLight) b.work[i].light[k] = b.work_tmp[i].light[k];
        if (F & kColor) b.work[i].color[k] = b.work_tmp[i].color[k];
      }
    }
    for (int k = 0; k < sets; ++k) {
      for (int i = *count - 1; i >= 0; --i) b.shadow.uv[k][i] = b.shadow_tmp[k][i];
    }
  }
  return 1;
}

}  // namespace

Clip2D& clip2d() { return g_clip; }

// Mech3.exe 1.2 @ 0x00529ce0. Untextured, unlit: sx, sy, sz only.
int32_t clip2d_flat(const ScreenClip& r, int32_t* count) { return clip<0>(r, count); }

// Mech3.exe 1.2 @ 0x0052aaf0. Untextured, lit or fogged: plus work +0..+8.
int32_t clip2d_lit(const ScreenClip& r, int32_t* count) { return clip<kLight>(r, count); }

// Mech3.exe 1.2 @ 0x0052bd10. Textured: plus tu, tv and the vertex colour
// (work +0xc..+0x14), not the light and fog.
int32_t clip2d_textured(const ScreenClip& r, int32_t* count) { return clip<kUV | kColor>(r, count); }

// Mech3.exe 1.2 @ 0x0052dc80. Textured, lit per vertex: everything.
int32_t clip2d_textured_lit(const ScreenClip& r, int32_t* count) {
  return clip<kUV | kLight | kColor>(r, count);
}

// Mech3.exe 1.2 @ 0x0052e920. 0x52bd10 plus the projected-shadow UV sets.
int32_t clip2d_textured_shadowed(const ScreenClip& r, int32_t* count) {
  return clip<kUV | kColor | kShadow>(r, count);
}

// Mech3.exe 1.2 @ 0x0052fab0. 0x52dc80 plus the projected-shadow UV sets.
int32_t clip2d_textured_lit_shadowed(const ScreenClip& r, int32_t* count) {
  return clip<kUV | kLight | kColor | kShadow>(r, count);
}

}  // namespace zmod
