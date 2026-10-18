#include "gamez/zrender/zrndr_span.h"

#include <cstring>

namespace zrndr {

namespace {
SpanState g_span;

// One PUSH: step the write pointer back and store the value little-endian, as x86
// does. The original's write pointer is ESP; here it is an ordinary pointer.
inline void push16(uint8_t*& p, uint16_t v) {
  p -= 2;
  std::memcpy(p, &v, 2);
}
inline void push32(uint8_t*& p, uint32_t v) {
  p -= 4;
  std::memcpy(p, &v, 4);
}
}  // namespace

SpanState& span_state() { return g_span; }

// FIX (A): the original points ESP at the span and fills it with PUSH. That only
// exists on x86, and it is fragile anywhere: whatever uses the stack while ESP
// points at the pixels (an exception, a signal, a debugger) writes into the
// picture or crashes. Same writes, same order, through a normal pointer.
void span_fill16(uint32_t color, uint32_t count) {
  uint8_t* p = g_span.dest + count * 2;    // ADD ESP, ECX twice
  const uint32_t pair = (color << 16) | color;
  uint32_t n = count;
  if (n & 1) push16(p, static_cast<uint16_t>(pair));  // PUSH AX
  n >>= 1;
  if (n & 1) push32(p, pair);              // PUSH EAX
  n >>= 1;
  while (n != 0) {                         // PUSH EAX; PUSH EAX; DEC ECX
    push32(p, pair);
    push32(p, pair);
    n -= 1;
  }
}

void span_textured16(uint32_t u, uint32_t v, int32_t count) {
  uint8_t* p = g_span.dest + static_cast<uint32_t>(count) * 2;
  uint32_t bytes = static_cast<uint32_t>(count) * 2;
  uint32_t mask = g_span.v_mask;
  // As in the original, count must be at least 1: SUB EDI, 2 / JNZ never stops at 0.
  do {
    const uint32_t col = (static_cast<uint32_t>(static_cast<int32_t>(u) >> 20)) & 0x3ff;  // SAR
    const uint32_t row = (mask & v) >> 10;
    u += g_span.du;
    v += g_span.dv;
    mask = g_span.v_mask;
    bytes -= 2;
    push16(p, g_span.texture[col + row]);  // PUSH WORD [EBP + EAX*2]
  } while (bytes != 0);
}

}  // namespace zrndr
