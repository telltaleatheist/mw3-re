#pragma once
#include <cstdint>
#include <vector>

// GDI's choice of a TrueType font's size and its ascent and descent for a
// CreateFont height (docs/twod.md section 6, Text widths): a positive
// height is the cell, found in the font's VDMX table (the largest size whose
// yMax - yMin fits) or else scaled by the win ascent and descent; a negative
// one is the size itself. As Wine's load_VDMX and calc_ppem_for_height,
// which follow Windows.
namespace sdlfe {

struct GdiFaceMetrics {
  int upem = 2048;
  int win_ascent = 0, win_descent = 0;  // OS/2's usWinAscent, usWinDescent
  int hhea_ascent = 0, hhea_descent = 0;
  struct Vdmx {
    int ppem, y_max, y_min;
  };
  std::vector<Vdmx> vdmx;  // the 1:1 group, in size order (none: no table)
};

struct GdiSize {
  int ppem = 0;
  int ascent = 0, descent = 0;  // tmAscent, tmDescent (tmHeight their sum)
};

// The tables read from a TrueType file's bytes; false when it is not one.
bool gdi_face_metrics(const std::vector<uint8_t>& file, GdiFaceMetrics* out);
GdiSize gdi_font_size(const GdiFaceMetrics& m, int32_t height);

}  // namespace sdlfe
