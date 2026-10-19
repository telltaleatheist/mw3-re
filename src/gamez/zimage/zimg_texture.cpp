#include "gamez/zimage/zimg_texture.h"

#include <cstring>

#include "gamez/zerror/zerr.h"

namespace zimg {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zImage\\zimg_texture.cpp";
TextureTable g_table;
}  // namespace

TextureTable& textures() { return g_table; }

// Mech3.exe 1.2 @ 0x00518080. No bounds check.
TextureEntry* texture_at(int index) { return index == -1 ? nullptr : &g_table.entries[index]; }

// Mech3.exe 1.2 @ 0x00518160. Quirk kept: a count of 0 returns without clearing
// the table count. On a short read the original keeps the bytes it got in the
// table; the port leaves the table as it was.
int read_texture_directory(int count, std::FILE* f) {
  if (count == 0) return 0;
  if (count > kMaxTextures) {
    zerr::report(0x100, kFile, 0x10d, "Too many textures for texture array size.");
    return -1;
  }
  static uint8_t buf[kMaxTextures * 0x28];
  if (std::fread(buf, static_cast<size_t>(count) * 0x28, 1, f) != 1) {
    zerr::report(0x200, kFile, 0x11b, "Error reading GameZ Texture directory data.");
    return -1;
  }
  for (int i = 0; i < count; ++i) {
    const uint8_t* raw = buf + i * 0x28;
    TextureEntry& e = g_table.entries[i];
    e.image = e.surface = nullptr;
    std::memcpy(e.name, raw + 8, 20);
    std::memcpy(&e.state, raw + 0x1c, 4);
    int32_t mip;
    std::memcpy(&mip, raw + 0x24, 4);
    e.group = g_table.group;
    e.mip = texture_at(mip);
  }
  g_table.count = count;
  return count;
}

}  // namespace zimg
