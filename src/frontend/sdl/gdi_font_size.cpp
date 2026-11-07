#include "frontend/sdl/gdi_font_size.h"

#include <cstdlib>
#include <cstring>

namespace sdlfe {

namespace {
int u16(const std::vector<uint8_t>& d, size_t o) { return o + 2 <= d.size() ? d[o] << 8 | d[o + 1] : 0; }
int s16(const std::vector<uint8_t>& d, size_t o) { return static_cast<int16_t>(u16(d, o)); }
uint32_t u32(const std::vector<uint8_t>& d, size_t o) {
  return o + 4 <= d.size() ? static_cast<uint32_t>(u16(d, o)) << 16 | static_cast<uint32_t>(u16(d, o + 2)) : 0;
}
// FreeType's FT_MulDiv: a * b / c rounded to nearest.
int mul_div(int a, int b, int c) {
  if (c == 0) return 0;
  const long long n = static_cast<long long>(a) * b;
  return static_cast<int>((n + (n >= 0 ? c / 2 : -c / 2)) / c);
}
}  // namespace

bool gdi_face_metrics(const std::vector<uint8_t>& d, GdiFaceMetrics* out) {
  *out = GdiFaceMetrics{};
  if (d.size() < 12) return false;
  const int n = u16(d, 4);
  size_t head = 0, hhea = 0, os2 = 0, vdmx = 0;
  for (int i = 0; i < n; ++i) {
    const size_t r = 12 + 16 * static_cast<size_t>(i);
    if (r + 16 > d.size()) return false;
    const size_t off = u32(d, r + 8);
    if (std::memcmp(&d[r], "head", 4) == 0) head = off;
    if (std::memcmp(&d[r], "hhea", 4) == 0) hhea = off;
    if (std::memcmp(&d[r], "OS/2", 4) == 0) os2 = off;
    if (std::memcmp(&d[r], "VDMX", 4) == 0) vdmx = off;
  }
  if (!head || !hhea) return false;
  out->upem = u16(d, head + 18);
  out->hhea_ascent = s16(d, hhea + 4);
  out->hhea_descent = s16(d, hhea + 6);
  if (os2) {
    out->win_ascent = u16(d, os2 + 74);
    out->win_descent = u16(d, os2 + 76);
  }
  if (vdmx) {
    // The ratio records (charset, x, y start, y end): the first for a 1:1
    // device (x 0 is any), then its group's records.
    const int ratios = u16(d, vdmx + 4);
    for (int i = 0; i < ratios; ++i) {
      const size_t r = vdmx + 6 + 4 * static_cast<size_t>(i);
      const int x = r + 3 < d.size() ? d[r + 1] : -1, y0 = r + 3 < d.size() ? d[r + 2] : -1,
                y1 = r + 3 < d.size() ? d[r + 3] : -1;
      if (!(x == 0 || (x == 1 && y0 <= 1 && 1 <= y1))) continue;
      const size_t g = vdmx + static_cast<size_t>(u16(d, vdmx + 6 + 4 * static_cast<size_t>(ratios) + 2 * i));
      const int recs = u16(d, g);
      for (int k = 0; k < recs; ++k) {
        const size_t e = g + 4 + 6 * static_cast<size_t>(k);
        out->vdmx.push_back({u16(d, e), s16(d, e + 2), s16(d, e + 4)});
      }
      break;
    }
  }
  return true;
}

GdiSize gdi_font_size(const GdiFaceMetrics& m, int32_t height) {
  GdiSize s;
  if (height == 0) height = 16;  // (Wine's default; the game always gives one)
  // VDMX: a cell height takes the record that matches it, else the one before
  // the first that is taller; a character height its own size's record.
  const GdiFaceMetrics::Vdmx* hit = nullptr;
  if (height > 0) {
    for (size_t i = 0; i < m.vdmx.size(); ++i) {
      const int cell = m.vdmx[i].y_max - m.vdmx[i].y_min;
      if (cell == height) {
        hit = &m.vdmx[i];
        break;
      }
      if (cell > height) {
        if (i > 0) hit = &m.vdmx[i - 1];
        break;
      }
    }
  } else {
    for (const auto& v : m.vdmx)
      if (v.ppem == -height) hit = &v;
  }
  if (hit) {
    s.ppem = hit->ppem;
    s.ascent = hit->y_max;
    s.descent = -hit->y_min;
    return s;
  }
  int units = m.win_ascent + m.win_descent;
  if (units == 0) units = m.hhea_ascent - m.hhea_descent;
  if (height > 0) {
    s.ppem = mul_div(m.upem, height, units);
    if (s.ppem > 1 && mul_div(units, s.ppem, m.upem) > height) --s.ppem;  // rounded over the height
  } else {
    s.ppem = -height;
  }
  if (s.ppem < 1) s.ppem = 1;
  const int a = m.win_ascent + m.win_descent ? m.win_ascent : m.hhea_ascent;
  const int dsc = m.win_ascent + m.win_descent ? m.win_descent : -m.hhea_descent;
  s.ascent = mul_div(a, s.ppem, m.upem);
  s.descent = mul_div(dsc, s.ppem, m.upem);
  return s;
}

// Arial's and Impact's VDMX to size 48, the 1:1 group's {size, yMax, yMin}: a
// stand-in has no such table, and these pick smaller sizes than the win metrics.
const GdiFaceMetrics::Vdmx kArialCells[] = {
    {8, 8, -2}, {9, 9, -3}, {10, 10, -3}, {11, 11, -3}, {12, 12, -3}, {13, 13, -3}, {14, 13, -3}, {15,
    14, -3}, {16, 15, -3}, {17, 15, -4}, {18, 17, -4}, {19, 18, -4}, {20, 19, -4}, {21, 19, -5}, {22,
    20, -5}, {23, 21, -5}, {24, 21, -6}, {25, 23, -5}, {26, 25, -6}, {27, 26, -6}, {28, 26, -6}, {29,
    27, -6}, {30, 28, -7}, {31, 28, -7}, {32, 29, -7}, {33, 31, -7}, {34, 32, -7}, {35, 32, -8}, {36,
    33, -8}, {37, 34, -8}, {38, 34, -9}, {39, 35, -9}, {40, 36, -9}, {41, 38, -9}, {42, 38, -9}, {43,
    39, -10}, {44, 40, -10}, {45, 40, -10}, {46, 42, -10}, {47, 43, -10}, {48, 45, -10},
};
const GdiFaceMetrics::Vdmx kImpactCells[] = {
    {8, 9, -2}, {9, 11, -2}, {10, 12, -2}, {11, 13, -2}, {12, 13, -3}, {13, 14, -3}, {14, 15, -3}, {15,
    16, -3}, {16, 17, -3}, {17, 17, -4}, {18, 18, -4}, {19, 19, -4}, {20, 21, -4}, {21, 22, -4}, {22,
    22, -5}, {23, 23, -5}, {24, 24, -5}, {25, 27, -5}, {26, 28, -5}, {27, 28, -6}, {28, 29, -6}, {29,
    30, -6}, {30, 31, -6}, {31, 32, -7}, {32, 32, -7}, {33, 34, -7}, {34, 35, -7}, {35, 36, -7}, {36,
    36, -8}, {37, 37, -8}, {38, 38, -8}, {39, 39, -8}, {40, 40, -8}, {41, 42, -9}, {42, 43, -9}, {43,
    44, -9}, {44, 45, -9}, {45, 47, -9}, {46, 47, -10}, {47, 49, -10}, {48, 50, -10},
};
void stand_in_metrics(const std::string& file, const std::string& found, GdiFaceMetrics* m) {
  std::string name;
  for (char c : found) name += c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
  if (name == file) return;
  m->upem = 2048;  // the three game faces' own OS/2 values
  m->win_ascent = file == "impact.ttf" ? 2066 : file == "lucon.ttf" ? 1616 : 1854;
  m->win_descent = file == "arial.ttf" ? 434 : 432;
  m->vdmx.clear();
  if (file == "arial.ttf") m->vdmx.assign(std::begin(kArialCells), std::end(kArialCells));
  if (file == "impact.ttf") m->vdmx.assign(std::begin(kImpactCells), std::end(kImpactCells));
}

}  // namespace sdlfe
