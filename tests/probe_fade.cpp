// Probes for test_fade.py: the fade state (mech3/app_states, FadeState) with
// every call outside recorded.
#include <cstdint>
#include <cstring>

#include "gamez/zerror/zerr.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/app.h"
#include "mech3/app_states.h"
#include "probe_app.h"

namespace {
using probe_app::call;
using zgame::Arg;

struct Recording : mech3::Engine {
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  float frame_dt() override { return zgame::from_bits(peek32(0x72c52c)); }
};

// A state the fade moves on to: only its address matters.
struct Next : mech3::State {
  explicit Next(uint32_t addr) : mech3::State("Next", addr) {}
};

Recording* g_engine = nullptr;
mech3::Mech3App* g_app = nullptr;
Next g_next_a(0x6099a0), g_next_b(0x608bd0);

zgame::State* state_at(uint32_t addr) {
  if (addr == 0x609888) return &mech3::states().fade;
  if (addr == g_next_a.addr()) return &g_next_a;
  if (addr == g_next_b.addr()) return &g_next_b;
  return nullptr;
}
uint32_t state_addr(const zgame::State* s) {
  const auto* m = dynamic_cast<const mech3::State*>(s);
  return m ? m->addr() : 0;
}

}  // namespace

extern "C" {

// A fresh app and states, made unrecorded; the pixel format pack_bytes uses.
void probe_fade_reset(int32_t r_bits, int32_t g_bits, int32_t b_bits) {
  probe_app::Mute quiet;
  delete g_app;
  delete g_engine;
  mech3::reset_states();
  g_engine = new Recording;
  zerr::set_sink(nullptr);
  g_app = new mech3::Mech3App(g_engine);
  mech3::set_app(g_app);
  zvid::set_pixel_format(r_bits, g_bits, b_bits);
}

// Runs one ported function by its original address: ECX, EDX and the stack
// dwords as the original takes them; the result in *ret.
int32_t probe_fade_call(uint32_t addr, uint32_t ecx, uint32_t edx, const uint32_t* s, uint32_t* ret) {
  using namespace mech3;
  FadeState& f = states().fade;
  uint32_t r = 0;
  auto u8 = [](uint32_t v) { return static_cast<uint8_t>(v); };
  switch (addr) {
    case 0x48ee70:  // ECX arg, EDX direction; buffer, seconds, r, g, b
      fade_push(nullptr, ecx, static_cast<int32_t>(edx), static_cast<int32_t>(s[0]), zgame::from_bits(s[1]),
                u8(s[2]), u8(s[3]), u8(s[4]));
      break;
    case 0x48eea0:  // ECX next, EDX arg; direction, buffer, seconds, r, g, b
      fade_push(state_at(ecx), edx, static_cast<int32_t>(s[0]), static_cast<int32_t>(s[1]),
                zgame::from_bits(s[2]), u8(s[3]), u8(s[4]), u8(s[5]));
      break;
    case 0x48ef00:
      fade_replace(state_at(ecx), edx, static_cast<int32_t>(s[0]), static_cast<int32_t>(s[1]),
                   zgame::from_bits(s[2]), u8(s[3]), u8(s[4]), u8(s[5]));
      break;
    case 0x48ef60: f.on_queued(); break;
    case 0x48ef70: r = f.enter(); break;
    case 0x48ef90: r = f.update(); break;
    case 0x48f030: f.leave(); break;
    default: return 1;
  }
  *ret = r;
  return 0;
}

// The fade's fields +0x08..+0x34 (13 dwords; +0x20 the 16-bit colour, the
// next state as its address).
void probe_fade_fields(uint32_t* out) {
  const mech3::FadeState& f = mech3::states().fade;
  auto fb = [](float v) { return zgame::bits(v); };
  const uint32_t v[13] = {f.arg_,          static_cast<uint32_t>(f.mode_),      state_addr(f.next_),
                          f.cursor_,       f.image_,                            static_cast<uint32_t>(f.direction_),
                          f.colour_,       fb(f.seconds_),                      fb(f.t_),
                          fb(f.one_),      fb(f.rate_),                         static_cast<uint32_t>(f.buffer_),
                          0};
  std::memcpy(out, v, sizeof v);
}

// The app's queued state changes: (type, state address, arg) each.
int32_t probe_fade_queue(uint32_t* out, int32_t max) {
  int32_t n = 0;
  for (const auto& p : g_app->pending_list()) {
    if (n >= max) break;
    out[3 * n] = static_cast<uint32_t>(p.type);
    out[3 * n + 1] = state_addr(p.state);
    out[3 * n + 2] = p.arg;
    ++n;
  }
  return n;
}

}  // extern "C"
