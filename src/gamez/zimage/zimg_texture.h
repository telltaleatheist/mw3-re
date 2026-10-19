#pragma once
#include <cstdint>
#include <cstdio>

// The global texture table (zImage\zimg_texture.cpp), filled from a world's
// texture directory (docs/gamez.md) and by name lookups, and the loading that
// gives each entry its image and Direct3D texture record (0x00519080..,
// between zimg_texture.cpp and zimg_fonts.cpp; docs/drafts/bake-textures.md).
namespace zimg {

struct Texture;

struct TextureEntry {        // 0x28 bytes in the file
  void* image;               // +0x00  the zimg::Texture, set when the texture is loaded by name
  void* surface;             // +0x04  the zvid::D3DTexture record (0x0057b130), on Direct3D
  char name[20];             // +0x08
  int32_t state;             // +0x1c  0 free, 2 to load (every world entry), 3 to reload, 1 loaded
  int32_t group;             // +0x20
  TextureEntry* mip;         // +0x24  an index in the file, -1 for none; rebuilt by name at load
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

// --- Loading ---

// Calls the loading makes into code not ported, or ported elsewhere. Unset
// ones do nothing (and return null / false).
struct TextureHooks {
  Texture* (*fallback)(const char* name) = nullptr;  // [0x00638520], never set in 1.2
  // 0x00542710: the entry is one of the three at 0x0074a2b4 (gmod_const
  // region, the projected-light textures), kept as an image only.
  bool (*projected)(const TextureEntry* entry) = nullptr;
  // [0x0072f374] = 0x0057baa0: copy a new image into an existing record (state 3).
  void (*reload)(void* surface, Texture* image) = nullptr;
};
TextureHooks& texture_hooks();

struct TextureLoad {
  int32_t list = 0;          // 0x0063851c  the library list searched first (the last hit)
  int32_t memory = 0;        // 0x00638524  bytes given to Direct3D (2 per texel)
};
TextureLoad& texture_load();

Texture& default_image();    // 0x005ecbc0: 8x8 red/green checks, 565
TextureEntry* default_entry();  // 0x00518210: the entry naming default_image()

TextureEntry* find_texture(const char* name);  // 0x00518220: case-sensitive, in-use entries
void set_entry_name(const char* path, char* dst);  // 0x00519210: base name, extension cut (0x00519150)
// ECX name: the image from the current library list, else the other (which
// then becomes current); an alpha texture's transparent texels are zeroed.
Texture* fetch_image(const char* name);        // 0x00518ee0
void clear_transparent(Texture& t);            // 0x00518e50
void link_mips(TextureEntry& entry);           // 0x00519270: "<name>_1" -> _2, _3, ... by name
// ECX entry, stack free_pixels (EDX unused): image, mips, then on Direct3D the
// record through zvid::create_texture. Only entries in state 2 or 3.
void load_texture(TextureEntry* entry, int32_t free_pixels);  // 0x00518f70
void load_group(int32_t group);                // 0x00519080: ECX group, -1 for all
int32_t load_group_0();                        // 0x005190d0

// zRender (between zreader.cpp and zrndr_draw.c): the software renderer's
// span constants of an image. Kept here until that file is ported.
void software_prepare(Texture& t);             // 0x00557ed0

}  // namespace zimg
