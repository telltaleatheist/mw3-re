// Pirate's Moon's texture fetch 0x005327d0 never clears an alpha texture's transparent texels (MW3's 0x00518ee0 does,
// through clear_transparent 0x00518e50). No shipped PM texture has colour under alpha 0 (318 alpha textures over the
// mech, c1 and image libraries checked), so it shows only in a mod's: here rmechtex.zbd copied with one transparent
// texel coloured, which the fetch must keep.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"

TEST(pm_fetch_keeps_transparent_colours) {
  if (!std::filesystem::exists(MW3PM_GAME_DIR)) return;
  const std::string src = std::string(MW3PM_GAME_DIR) + "/zbd/rmechtex.zbd";
  std::ifstream in(src, std::ios::binary);
  std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  EXPECT(d.size() > 0x18);
  auto u32 = [&](size_t at) { uint32_t v; std::memcpy(&v, &d[at], 4); return v; };
  auto i16 = [&](size_t at) { int16_t v; std::memcpy(&v, &d[at], 2); return v; };
  const uint32_t count = u32(12);
  std::string name;
  int32_t texel = -1;
  for (uint32_t i = 0; i < count && texel < 0; ++i) {
    const size_t e = 0x18 + i * 0x28;
    const uint32_t off = u32(e + 32);
    const uint8_t flags = d[off];
    const int32_t w = i16(off + 4), h = i16(off + 6), pal = i16(off + 12);
    if (!(flags & zimg::kTwoBytes) || !(flags & zimg::kAlpha) || !(flags & zimg::kFullAlpha) || pal != 0) continue;
    const size_t px = static_cast<size_t>(w) * h, alpha = off + 16 + px * 2;
    for (size_t k = 0; k < px; ++k)
      if (d[alpha + k] == 0) {
        name = reinterpret_cast<const char*>(&d[e]);
        texel = static_cast<int32_t>(k);
        d[off + 16 + k * 2] = 0x34, d[off + 16 + k * 2 + 1] = 0x12;  // a colour under alpha 0
        break;
      }
  }
  EXPECT(texel >= 0);
  const std::string dir = std::string(MW3_TEST_TMP_ROOT) + "/pm_fetch/";
  std::filesystem::create_directories(dir);
  std::ofstream(dir + "rmechtex.zbd", std::ios::binary).write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
  zimg::State& st = zimg::state();
  const char* dir_was = st.dir;
  const int32_t format_was = st.display_format;
  st.dir = dir.c_str(), st.display_format = 6;  // RGB565: the file's colours as they are
  zimg::free_all(0);
  zimg::new_library("mechtex", 0, true);
  zimg::Texture* f = texel >= 0 ? zimg::fetch_image(name.c_str()) : nullptr;
  std::printf("  %s: texel %d under alpha 0 reads %#x after the fetch\n", name.c_str(), texel,
              f && f->pixels ? static_cast<const uint16_t*>(f->pixels)[texel] : 0u);
  EXPECT(f && f->alpha && f->alpha[texel] == 0 && static_cast<const uint16_t*>(f->pixels)[texel] == 0x1234);
  zimg::free_all(0);
  st.dir = dir_was, st.display_format = format_was;
  std::filesystem::remove(dir + "rmechtex.zbd");
  std::filesystem::remove(dir);  // ours, now empty
}
