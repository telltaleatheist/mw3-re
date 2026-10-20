// zTwoD's software blit into the locked 16-bit buffer (docs/twod.md, Drawing).
// Original file unknown: between zreader.cpp and zrndr_draw.c.
#include "gamez/zrender/zrndr_blit.h"

#include "gamez/zimage/zimg_library.h"

namespace zrndr {

namespace {
Target2D g_target;

// 565: red and blue by alpha/256 of the difference, green rounded to an odd
// count of its units when it falls (the code keeps bit 5 for the sign).
uint16_t blend565(uint16_t d16, uint16_t s16, uint32_t a) {
  const int32_t d = static_cast<int16_t>(d16), s = static_cast<int16_t>(s16);
  const int32_t ia = static_cast<int32_t>(a);
  const uint32_t r = static_cast<uint32_t>((((s & 0xf800) - (d & 0xf800)) * ia) >> 8) & 0xfffff800u;
  uint32_t g = static_cast<uint32_t>((((s & 0x7e0) - (d & 0x7e0)) * ia) >> 8);
  g = (g & ~0xffu) | (g & 0xe0u);
  g = (g & 0x80000000u) ? (g | 0x20u) : (g & ~0x20u);
  const int32_t d2 = d + static_cast<int32_t>(r);
  const int32_t b = (((s & 0x1f) - (d2 & 0x1f)) * ia) >> 8;
  return static_cast<uint16_t>(d2 + (b + static_cast<int32_t>(g)));
}

uint16_t blend555(uint16_t d16, uint16_t s16, uint32_t a) {
  const int32_t d = static_cast<int16_t>(d16), s = static_cast<int16_t>(s16);
  const int32_t ia = static_cast<int32_t>(a);
  const uint32_t r = static_cast<uint32_t>((((s & 0x7c00) - (d & 0x7c00)) * ia) >> 8) & 0xfffffc00u;
  const uint16_t dr = static_cast<uint16_t>(d16 + r);
  const uint32_t g = static_cast<uint32_t>((((s & 0x3e0) - (d & 0x3e0)) * ia) >> 8) & ~0x1fu;
  const int32_t b = (((s & 0x1f) - (d & 0x1f)) * ia) >> 8;
  return static_cast<uint16_t>(static_cast<uint32_t>(b) + g + dr);
}
}  // namespace

Target2D& target2d() { return g_target; }

// Mech3.exe 1.2 @ 0x0055b3c0
void blit16(const zimg::Texture* img, int32_t x, int32_t y, uint16_t key, const BlitRect* src) {
  // FIX (B): a source rectangle reaching past the image is cut at its edges;
  // the original reads past the pixels. The menus pass the screen's size as
  // the rectangle of their 1280x960 background, so at taller or wider screens
  // (the pause screen's Options above 960 lines) it read past it
  // (docs/menus.md, The background at other sizes; fixes.md).
  BlitRect cut;
  if (src) {
    cut = *src;
    if (cut.left < 0) {
      x -= cut.left;
      cut.left = 0;
    }
    if (cut.top < 0) {
      y -= cut.top;
      cut.top = 0;
    }
    if (cut.right > img->width) cut.right = img->width;
    if (cut.bottom > img->height) cut.bottom = img->height;
    src = &cut;
  }
  int32_t w, h;
  if (!src) {
    w = img->width;
    h = img->height;
  } else {
    w = src->right - src->left;
    h = src->bottom - src->top;
  }
  if (w < 0 || w > 0x800 || h < 0 || h > 0x800) return;
  const Target2D& t = g_target;
  uint32_t x2 = static_cast<uint32_t>(w + x - 1);
  const int32_t y_end = h + y;
  uint32_t y2 = static_cast<uint32_t>(y_end - 1);
  if (w + x <= 0 || x >= static_cast<int32_t>(t.width) || y_end <= 0 || y >= static_cast<int32_t>(t.height)) return;
  const int32_t left = x < 0 ? 0 : x;
  if (x2 >= t.width) x2 = t.width - 1;
  const int32_t top = y < 0 ? 0 : y;
  // FIX (bug): the original keeps a bottom of exactly height (jbe), one row
  // past the buffer; clipped at its last row here (docs/fixes.md).
  if (y2 >= t.height) y2 = t.height - 1;
  const int32_t width = static_cast<int32_t>(x2) - left + 1;
  const int32_t rows = static_cast<int32_t>(y2) - top + 1;
  const int32_t pitch = static_cast<int32_t>(t.pitch >> 1);
  const int32_t stride = img->width_again;
  int32_t col, row;
  if (!src) {
    col = left - x;
    row = top - y;
  } else {
    col = left - x + src->left;
    row = src->top + top - y;
  }
  uint16_t* dst = reinterpret_cast<uint16_t*>(t.base) + (pitch * top + left);
  const bool full = left == 0 && x2 == t.width - 1 && pitch == stride;
  const bool keyed = (img->flags & zimg::kAlpha) != 0;
  const int32_t lo = t.format == 6 ? 3 : 7;  // alphas at or below are skipped

  if (!img->palette) {
    const uint16_t* s = static_cast<const uint16_t*>(img->pixels) + (col + row * stride);
    if (img->alpha) {
      const uint8_t* a = img->alpha + col + row * stride;
      for (int32_t r = 0; r < rows; ++r, dst += pitch, s += stride, a += stride) {
        for (int32_t c = 0; c < width; ++c) {
          const uint32_t al = a[c];
          if (al <= static_cast<uint32_t>(lo)) continue;
          if (al >= 0xfc) dst[c] = s[c];
          else dst[c] = t.format == 6 ? blend565(dst[c], s[c], al) : blend555(dst[c], s[c], al);
        }
      }
      return;
    }
    if (keyed) {
      for (int32_t r = 0; r < rows; ++r, dst += pitch, s += stride)
        for (int32_t c = 0; c < width; ++c)
          if (s[c] != key) dst[c] = s[c];
      return;
    }
    if (full) {  // one copy by dwords: an odd last pixel is left out (as written)
      const int32_t n = (rows * width) >> 1;
      for (int32_t i = 0; i < 2 * n; ++i) dst[i] = s[i];
      return;
    }
    for (int32_t r = 0; r < rows; ++r, dst += pitch, s += stride)
      for (int32_t c = 0; c < width; ++c) dst[c] = s[c];
    return;
  }

  // FIX (bug): with a zero width (a source rectangle with right == left) the
  // original's palette loops count from 0 past 2^32 and fault; nothing is drawn.
  const uint16_t* pal = img->palette;
  const uint8_t* s = static_cast<const uint8_t*>(img->pixels) + col + row * stride;
  if (img->alpha) {
    const uint8_t* a = img->alpha + col + row * stride;
    for (int32_t r = 0; r < rows; ++r, dst += pitch, s += stride, a += stride) {
      for (int32_t c = 0; c < width; ++c) {
        const uint32_t al = a[c];
        if (al <= static_cast<uint32_t>(lo)) continue;
        if (al >= 0xfc) dst[c] = pal[s[c]];
        else dst[c] = t.format == 6 ? blend565(dst[c], pal[s[c]], al) : blend555(dst[c], pal[s[c]], al);
      }
    }
    return;
  }
  if (keyed) {
    for (int32_t r = 0; r < rows; ++r, dst += pitch, s += stride)
      for (int32_t c = 0; c < width; ++c)
        if (s[c] != key) dst[c] = pal[s[c]];
    return;
  }
  if (full) {
    for (int32_t i = 0; i < rows * width; ++i) dst[i] = pal[s[i]];
    return;
  }
  for (int32_t r = 0; r < rows; ++r, dst += pitch, s += stride)
    for (int32_t c = 0; c < width; ++c) dst[c] = pal[s[c]];
}

}  // namespace zrndr
