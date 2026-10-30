// Probes for test_decal.py (docs/damage-decals.md): the cookie 0x00542730 on
// the test's images, and the damage decal 0x00474d90 with 0x00474cd0 on
// skins the test describes (the cookie and the device reload recorded).
#include <cstdint>
#include <cstring>
#include <vector>

#include "crt/msvc_rand.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_hit.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zrender/zrndr_blit.h"
#include "mech3/mech_skin.h"

namespace {
zimg::Texture make_image(int16_t w, int16_t h, void* pixels, uint8_t* alpha, uint16_t* palette) {
  zimg::Texture t{};
  t.width = w, t.height = h, t.pixels = pixels, t.alpha = alpha, t.palette = palette;
  return t;
}
std::vector<uint32_t> g_calls;  // the decal's: 1 cookie (target index, damage entry index, hole), 2 reload (target)
}  // namespace

extern "C" {

// The cookie: the target image (w, h, pixels: written back), the damage image
// (its size, pixels, whether palettered), the hole (size, alpha), the UV
// (in and out) and the format. Returns the result (0, 1).
int probe_cookie(int16_t tw, int16_t th, uint16_t* tpix, int16_t fw, int16_t fh, const uint16_t* fpix, int32_t fpal,
                 int16_t hw, int16_t hh, const uint8_t* halpha, float* uv, int32_t format) {
  zimg::Texture target = make_image(tw, th, tpix, nullptr, nullptr);
  static uint16_t palette[1] = {};
  zimg::Texture from = make_image(fw, fh, const_cast<uint16_t*>(fpix), nullptr, fpal ? palette : nullptr);
  zimg::Texture hole = make_image(hw, hh, nullptr, const_cast<uint8_t*>(halpha), nullptr);
  zimg::TextureEntry entry{};
  entry.image = &target;
  zmod::uv_capture().uv[0] = uv[0], zmod::uv_capture().uv[1] = uv[1];
  zrndr::target2d().format = format;
  const bool r = zmod::cookie(&entry, &from, &hole);
  uv[0] = zmod::uv_capture().uv[0], uv[1] = zmod::uv_capture().uv[1];
  return r ? 1 : 0;
}

// The decal: `n` skin entries, each with its texture entry (0: none), cached
// flag, damage link (index, -1 none) and the damage's texture (0: none); the
// material's texture is entry `hit` (-1: a texture none has); `holes` holes;
// the damage skin linked or not; the seed. out: the calls, count first.
int probe_decal(int32_t n, const int32_t* has_tex, const int32_t* cached, const int32_t* damage, const int32_t* damage_tex,
                int32_t hit, int32_t holes, int32_t linked, uint32_t seed, uint32_t* out) {
  using namespace mech3;
  static zimg::TextureEntry texes[16];
  static zimg::Texture images[16], hole_images[8];
  static uint16_t pixels[16][4];
  std::vector<SkinEntry*> made;
  Skin skin("s", 0);
  for (int32_t i = 0; i < n; ++i) {
    auto* e = new SkinEntry("e", "e");
    texes[i] = zimg::TextureEntry{};
    images[i] = make_image(2, 2, pixels[i], nullptr, nullptr);
    texes[i].image = &images[i];
    texes[i].surface = (i & 1) ? &texes[i] : nullptr;  // a device record on odd ones
    e->cached = static_cast<uint8_t>(cached[i]);
    e->texture = has_tex[i] ? (cached[i] ? static_cast<void*>(&images[i]) : static_cast<void*>(&texes[i])) : nullptr;
    made.push_back(e);
    skin.entries.push_back(e);
  }
  for (int32_t i = 0; i < n; ++i)
    if (damage[i] >= 0) {
      made[i]->damage = made[damage[i]];
      if (!damage_tex[i]) made[damage[i]]->texture = nullptr;
    }
  skin_globals().damage_mode = 0;  // the set made without fetching the real holes
  SkinSet set(0);
  Skin* dmg = new Skin("damage", 0);
  dmg->loaded = 1;
  set.damage = dmg;
  skin_globals().damage_mode = static_cast<uint8_t>(linked);
  skin_globals().holes.clear();
  for (int32_t i = 0; i < holes; ++i) skin_globals().holes.push_back(&hole_images[i]);
  zmod::Material m{};
  // hit >= 100: the material's texture pointer is image hit-100's (a cached
  // entry's own; the lookup must skip cached ones).
  m.texture = hit >= 100 ? reinterpret_cast<zimg::TextureEntry*>(&images[hit - 100])
              : hit >= 0 ? &texes[hit] : reinterpret_cast<zimg::TextureEntry*>(&hole_images[7]);
  crt::rand_fixed_seed().reset();
  crt::msvc_srand(seed);
  g_calls.clear();
  // The cookie and the reload recorded (test hooks; the decal's own logic runs).
  skin_globals().cookie = [](zimg::TextureEntry* t, const zimg::Texture* f, const zimg::Texture* h) {
    int32_t ti = -1, fi = -1, hi = -1;
    for (int k = 0; k < 16; ++k) {
      if (t == &texes[k]) ti = k;
      if (f == &images[k]) fi = k;
    }
    for (int k = 0; k < 8; ++k)
      if (h == &hole_images[k]) hi = k;
    g_calls.insert(g_calls.end(), {1u, static_cast<uint32_t>(ti), static_cast<uint32_t>(fi), static_cast<uint32_t>(hi)});
    return true;
  };
  zimg::texture_hooks().reload = [](void* s, zimg::Texture*) {
    int32_t si = -1;
    for (int k = 0; k < 16; ++k)
      if (s == &texes[k]) si = k;
    g_calls.insert(g_calls.end(), {2u, static_cast<uint32_t>(si)});
  };
  set.damage_decal(&skin, &m);
  skin_globals().cookie = nullptr;
  zimg::texture_hooks().reload = nullptr;
  skin.entries.clear();
  for (SkinEntry* e : made) e->texture = nullptr, e->damage = nullptr, delete e;
  set.damage = nullptr;
  delete dmg;
  skin_globals().holes.clear();
  out[0] = static_cast<uint32_t>(g_calls.size());
  for (size_t i = 0; i < g_calls.size(); ++i) out[1 + i] = g_calls[i];
  return 0;
}

}  // extern "C"
