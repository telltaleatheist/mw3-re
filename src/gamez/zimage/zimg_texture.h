#pragma once
#include <cstdint>
#include <cstdio>

// The global texture table (zImage\zimg_texture.cpp), filled from a world's
// texture directory (docs/gamez.md) and by name lookups.
namespace zimg {

struct TextureEntry {        // 0x28 bytes in the file
  void* image;               // +0x00  set when the texture is loaded by name
  void* surface;             // +0x04
  char name[20];             // +0x08
  int32_t state;             // +0x1c  non-zero = in use
  int32_t group;             // +0x20
  TextureEntry* mip;         // +0x24  an index in the file, -1 for none
};

constexpr int kMaxTextures = 0x1000;

struct TextureTable {
  TextureEntry entries[kMaxTextures];  // 0x00610484
  int32_t count = 0;                   // 0x00610480
  int32_t group = 0;                   // 0x006384d4  current texture group
};
TextureTable& textures();

int read_texture_directory(int count, std::FILE* f);  // 0x00518160
TextureEntry* texture_at(int index);                  // 0x00518080

}  // namespace zimg
