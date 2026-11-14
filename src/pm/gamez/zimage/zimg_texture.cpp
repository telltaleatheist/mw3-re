#include "pm/gamez/zimage/zimg_texture.h"

#include "gamez/zimage/zimg_library.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zerror/zerr.h"

namespace pm::zimg {

namespace {
const char kFile[] = "C:\\zipper\\branchep1\\gamez\\zImage\\zimg_texture.cpp";
}  // namespace

// Pirate's Moon 1.0 @ 0x00531920. MW3's 0x00518160 kept the file's state and
// made a pointer of the mip index; PM's sets both.
int read_texture_directory(int count, std::FILE* f) {
  using ::zimg::kMaxTextures;
  if (count == 0) return 0;
  if (count > kMaxTextures) {
    zerr::report(0x100, kFile, 0x139, "Too many textures for texture array size.");
    return -1;
  }
  static uint8_t buf[kMaxTextures * 0x2c];
  if (std::fread(buf, static_cast<size_t>(count) * 0x2c, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x147, "Error reading GameZ Texture directory data.");
    return -1;
  }
  ::zimg::TextureTable& t = ::zimg::textures();
  t.count = count;
  for (int i = 0; i < count; ++i) {
    ::zimg::TextureEntry& e = t.entries[i];
    std::memcpy(e.name, buf + i * 0x2c + 0xc, 20);
    e.image = e.surface = nullptr;
    e.state = 2;
    e.group = t.group;
    e.mip = nullptr;
  }
  return count;
}

namespace {
::zimg::TextureEntry* g_light_map = nullptr;
}  // namespace
::zimg::TextureEntry* light_map() { return g_light_map; }
void set_light_map(::zimg::TextureEntry* texture) { g_light_map = texture; }
void set_additive(::zimg::TextureEntry* texture) {
  if (texture->image) static_cast<::zimg::Texture*>(texture->image)->stretch |= 4;
}

// Pirate's Moon 1.0 @ 0x00531870. The exe writes the image, surface and +0x08 pointers as they are, which the reader
// clears; 0 here.
int write_texture_directory(std::FILE* f) {
  const ::zimg::TextureTable& t = ::zimg::textures();
  const int32_t count = t.count;
  if (count == 0) return 0;
  std::vector<uint8_t> raw(static_cast<size_t>(count) * 0x2c);
  for (int32_t i = 0; i < count; ++i) {
    const ::zimg::TextureEntry& e = t.entries[i];
    uint8_t* r = raw.data() + static_cast<size_t>(i) * 0x2c;
    std::memcpy(r + 0xc, e.name, 20);
    std::memcpy(r + 0x20, &e.state, 4);
    std::memcpy(r + 0x24, &e.group, 4);
    const int32_t mip = ::zimg::texture_index(e.mip);  // 0x00531810
    std::memcpy(r + 0x28, &mip, 4);
  }
  if (std::fwrite(raw.data(), raw.size(), 1, f) != 1) {
    zerr::report(0x200, kFile, 0x11d, "Error writing texture directory.");
    return 0;
  }
  return count;
}

void textures_free() {
  g_light_map = nullptr;
  ::zimg::textures_free();
}

}  // namespace pm::zimg
