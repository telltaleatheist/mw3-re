// Probes for test_flares.py (docs/render.md, Lens flares): the list 0x00567420,
// the pick 0x00567650, the texture setter 0x005676d0 and the draw 0x00567d30
// with its four squares 0x00567b90; the square 0x00567720 goes to the shared
// callback. Lights and textures are the emulator's addresses.
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zrender/zrndr_flare.h"
#include "gamez/zrender/zrndr_fog.h"
#include "probe_app.h"

namespace {
float bf(uint32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
uint32_t fb(float f) {
  uint32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
// Each light's +0x30 block (0x20 bytes) by the emulator's address of it.
std::map<uint32_t, std::vector<uint8_t>> g_lights;
std::map<const uint8_t*, uint32_t> g_light_of;
}  // namespace

extern "C" {

// hardware, near (bits), clip left, top, point right, bottom (bits), four textures.
void probe_flares_reset(int32_t hw, uint32_t near_bits, const uint32_t* rect, const uint32_t* textures) {
  zrndr::Flares& f = zrndr::flares();
  f = zrndr::Flares{};
  g_lights.clear();
  g_light_of.clear();
  zrndr::device_fog().active = hw;
  zmth::projection().near_z = bf(near_bits);
  zmod::DrawState& s = zmod::draw_state();
  s.clip.side_04 = rect[0], s.clip.side_08 = rect[1];
  s.point_rect[2] = bf(rect[2]), s.point_rect[3] = bf(rect[3]);
  for (int i = 0; i < 4; ++i)
    zrndr::flare_texture(i, reinterpret_cast<const zimg::TextureEntry*>(static_cast<uintptr_t>(textures[i])));
  zrndr::flare_hooks().sprite = [](const float* p, const zimg::TextureEntry* t, float sz, float a, int32_t c) {
    probe_app::call("0x00567720", {fb(p[0]), fb(p[1]), fb(p[2]), u(t), fb(sz), fb(a), c});
  };
}

// One light's +0x30 block: +0x0c on, +0x14 full, +0x18 gone.
void probe_flare_light(uint32_t addr, uint32_t on, uint32_t full, uint32_t gone) {
  std::vector<uint8_t>& b = g_lights[addr];
  b.assign(0x20, 0);
  std::memcpy(b.data() + 0x0c, &on, 4);
  std::memcpy(b.data() + 0x14, &full, 4);
  std::memcpy(b.data() + 0x18, &gone, 4);
  g_light_of[b.data()] = addr;
}

void probe_flare_add(const uint32_t* world, const uint32_t* screen, uint32_t color, uint32_t light) {
  const float w[3] = {bf(world[0]), bf(world[1]), bf(world[2])}, s[3] = {bf(screen[0]), bf(screen[1]), bf(screen[2])};
  zrndr::flare_add(w, s, color, light ? g_lights[light].data() : nullptr);
}

// The pick from `from`; out: each picked one's index in the list.
int32_t probe_flare_pick(int32_t from, int32_t* out) {
  const int32_t n = zrndr::flares_pick(from);
  for (int32_t i = 0; i < n; ++i) out[i] = static_cast<int32_t>(zrndr::flare_picked(i) - zrndr::flares().list);
  return n;
}

void probe_flare_draw(int32_t i) { zrndr::flare_draw(i); }

// The list's count, then each record's 8 words (the light as its address).
int32_t probe_flare_list(uint32_t* out, int32_t cap) {
  const zrndr::Flares& f = zrndr::flares();
  for (int32_t i = 0; i < f.count && i < cap; ++i) {
    const zrndr::Flare& r = f.list[i];
    uint32_t* o = out + 8 * i;
    for (int k = 0; k < 3; ++k) o[k] = fb(r.world[k]), o[3 + k] = fb(r.screen[k]);
    o[6] = r.color;
    auto it = g_light_of.find(r.light);
    o[7] = it == g_light_of.end() ? 0 : it->second;
  }
  return f.count;
}

}  // extern "C"
