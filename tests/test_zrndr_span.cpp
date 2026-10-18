#include <cstring>
#include <vector>

#include "check.h"
#include "gamez/zrender/zrndr_span.h"

TEST(span_fill16_fills_exactly_count_pixels) {
  for (uint32_t count = 1; count <= 13; ++count) {
    std::vector<uint16_t> row(20, 0xaaaa);
    zrndr::span_state().dest = reinterpret_cast<uint8_t*>(&row[2]);
    zrndr::span_fill16(0x1234, count);
    for (uint32_t i = 0; i < row.size(); ++i) {
      const bool inside = i >= 2 && i < 2 + count;
      EXPECT(row[i] == (inside ? 0x1234 : 0xaaaa));
    }
  }
}

TEST(span_textured16_steps_u_and_v) {
  std::vector<uint16_t> tex(1024 * 4);
  for (size_t i = 0; i < tex.size(); ++i) tex[i] = static_cast<uint16_t>(i);
  std::vector<uint16_t> row(8, 0);
  zrndr::SpanState& s = zrndr::span_state();
  s.dest = reinterpret_cast<uint8_t*>(row.data());
  s.texture = tex.data();
  s.du = 1u << 20;          // one texel per pixel in u
  s.dv = 0;
  s.v_mask = 0xfffffc00;    // row bits
  zrndr::span_textured16(5u << 20, 1u << 20, 4);  // start at u=5, row offset 1024
  // Written right to left: the first texel computed lands at the right end.
  EXPECT(row[3] == 1024 + 5);
  EXPECT(row[2] == 1024 + 6);
  EXPECT(row[1] == 1024 + 7);
  EXPECT(row[0] == 1024 + 8);
  EXPECT(row[4] == 0);
}
