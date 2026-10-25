#pragma once
#include <cstdint>
#include <vector>

// File unknown: between zreader.cpp and zrndr_draw.c (0x00558290..0x00558610),
// named after zRender. The screen's colour tint (flashes and fades): 0x00558540
// keeps it for the software renderer's finish (0x00558610), or hands it to
// Direct3D's quad (0x0057dd80). docs/game-loop.md, The fade state;
// docs/render-tint.md.
namespace zrndr {

// The buffer the finish tints, set by 0x00558290.
struct FinishTarget {
  uint16_t* base = nullptr;        // 0x0072c1e4
  int32_t width = 0;               // 0x0072c1e8  pixels
  int32_t height = 0;              // 0x0072c1ec
  int32_t pitch = 0;               // 0x0072c1f0  bytes
  int32_t pitch_px = 0;            // 0x0072c1f4  pitch / 2
};
FinishTarget& finish_target();

// Mech3.exe 1.2 @ 0x00558290 (ECX pixels, EDX width; height, pitch).
void set_finish_target(uint16_t* base, int32_t width, int32_t height, int32_t pitch);

// What 0x00558540 keeps for the software finish.
struct TintState {
  int32_t pending = 0;             // 0x007fb2dc  set, never cleared by the finish
  int32_t left = 0;                // 0x007fb2e0
  int32_t top = 0;                 // 0x007fb2e4
  int32_t right = 0;               // 0x007fb2e8  inclusive: the screen's is width - 1
  int32_t bottom = 0;              // 0x007fb2ec  exclusive
  uint32_t colour = 0;             // 0x007fb2f0  16-bit
  double alpha = 0;                // 0x007fb2f8
};
TintState& tint_state();

// The finish's per-call constants, as the row routines read them.
struct FinishConstants {
  uint32_t keep = 0;               // 0x0072c1bc  (1 - alpha) * 32, truncated
  uint32_t red = 0;                // 0x0072c1fc  the colour's red share, both words
  uint32_t blue = 0;               // 0x0072c200
  uint32_t green = 0;              // 0x0072c204
  uint32_t pair_hi = 0;            // 0x0072c1d0  the scalar rows' added terms
  uint32_t pair_lo = 0;            // 0x0072c1b8
  int32_t row = 0;                 // 0x0072c1f8  which row routine: 0 0x005582c0, 1 0x00558320,
                                   //             2 0x00558380, 3 0x00558460
};
FinishConstants& finish_constants();

// The machine the finish picks its rows for: [[0x008008d8]] bit 2 (MMX), and
// the 2D format [0x0074a738] (5: RGB555, else RGB565).
struct FinishMachine {
  bool mmx = true;                 // every machine the rebuild runs on
  int32_t format = 6;
};
FinishMachine& finish_machine();

// What 0x00558540 calls in Direct3D.
struct TintHooks {
  void (*quad)(uint16_t colour, const int32_t* rect, double alpha) = nullptr;  // 0x0057dd80
};
TintHooks& tint_hooks();

// Mech3.exe 1.2 @ 0x00558540 (CX colour, EDX rect or null; the alpha a
// double): in Direct3D ([0x0072f348]) the quad, else kept for the finish.
void tint(uint16_t colour, const int32_t* rect, double alpha, bool direct3d);

// Mech3.exe 1.2 @ 0x00558610: the kept tint over the finish target's rows.
// Returns the last row routine's EAX as the original does ([0x0072c1f4]
// when rows ran, the kept keep factor when none did, 0 with no tint).
uint32_t finish();

// Mech3.exe 1.2 @ 0x005587b0: the kept tint dropped.
void tint_clear();

// The row routines (ECX row, EDX pixel count).
void tint_row_555(uint16_t* row, int32_t count);      // 0x005582c0
void tint_row_565(uint16_t* row, int32_t count);      // 0x00558320
void tint_row_555_mmx(uint16_t* row, int32_t count);  // 0x00558380
void tint_row_565_mmx(uint16_t* row, int32_t count);  // 0x00558460

// The noise the static draws from: 0x6400 bytes of CRT rand ([0x0072c1dc],
// its size [0x0072c1d8]), filled once by zRender's set-up 0x005581c0. FIX
// (A): one zeroed byte past the end (the fill's last pixel can read it).
std::vector<uint8_t>& noise_table();
// Mech3.exe 1.2 @ 0x005581c0: the noise (rand: the CRT's when null). Its
// other half, the ripple's scratch, is made as the ripple needs it.
void finish_init(int32_t (*rand)() = nullptr);
// Mech3.exe 1.2 @ 0x00558c10 (ECX rect or null, the level a double; DX, then
// the stack's two colours): green static over the finish target, the HUD's
// (docs/hud.md section 21). Each row starts at a random point of the noise;
// a pixel takes a colour by its noise byte's third (lo, mid, hi) when the
// next byte is under level x 256. No rect: the target less its last column
// and row.
void static_fill(const int32_t* rect, double level, uint16_t lo, uint16_t mid, uint16_t hi,
                 int32_t (*rand)() = nullptr);

}  // namespace zrndr
