#include "gamez/zimage/zimg_library.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "crt/msvc_string.h"

namespace zimg {

namespace {
State g_state;
std::vector<Library*> g_libraries;  // 0x006384f4, count 0x006384f8

bool exists(const char* name) {
  if (g_state.file_exists) return g_state.file_exists(name);
  std::string path = std::string(g_state.dir) + name;
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (f) std::fclose(f);
  return f != nullptr;
}

// 16-bit RGB565 to the RGB555 a 15-bit display wants: green loses its low bit.
void to_555(uint16_t* p, int count) {
  for (int i = 0; i < count; ++i) p[i] = ((p[i] >> 1) & 0x7fe0) | (p[i] & 0x1f);
}

// Mech3.exe 1.2 @ 0x00519740: the software renderer's shaded copies, one set
// per light table. Without light tables (hardware rendering) it is a no-op.
uint16_t* shade(uint16_t* palette) { return palette; }

int32_t read_size(const Texture& t, int bytes) {  // 0x00517f70
  return t.palette_count != 0 ? t.pixel_count : bytes * t.pixel_count;
}
}  // namespace

State& state() { return g_state; }

// Mech3.exe 1.2 @ 0x005182a0
Library* new_library(const char* name, int32_t variant, bool r_prefix) {
  auto* lib = new Library{};
  lib->variant = variant;
  lib->r_prefix = r_prefix;
  std::snprintf(lib->name, sizeof lib->name, "%s", name);
  lib->needs_resolve = true;
  g_libraries.push_back(lib);
  return lib;
}

// Mech3.exe 1.2 @ 0x00518a30. Tries <name>.zbd for variant 0, else
// <name><variant>.zbd and down to 0; a missing plain name also tries 8 down.
void resolve_name(Library& lib) {
  if (!lib.needs_resolve) return;
  char base[120];
  std::snprintf(base, sizeof base, "%s", lib.name);
  lib.needs_resolve = false;
  const char* plain = lib.r_prefix ? "r%s.%s" : "%s.%s";
  const char* numbered = lib.r_prefix ? "r%s%d.%s" : "%s%d.%s";
  int n = lib.variant;
  if (n == 0) {
    std::snprintf(lib.name, sizeof lib.name, plain, base, "zbd");
    if (exists(lib.name)) return;
    n = 8;
  }
  bool found = false;
  do {
    if (n < 0) break;
    if (n == 0) std::snprintf(lib.name, sizeof lib.name, plain, base, "zbd");
    else std::snprintf(lib.name, sizeof lib.name, numbered, base, n, "zbd");
    found = exists(lib.name);
    --n;
  } while (!found);
  if (!found) lib.missing = true;
}

// Mech3.exe 1.2 @ 0x005187c0
bool open(Library& lib) {
  if (!g_state.enabled) return false;
  if (lib.needs_resolve) resolve_name(lib);
  if (lib.file) return true;
  if (lib.missing) return false;
  // FIX (A): host path under State::dir instead of the zutil search path.
  lib.file = std::fopen((std::string(g_state.dir) + lib.name).c_str(), "rb");
  if (!lib.file) return false;
  if (lib.entries) return true;
  if (std::fread(&lib.header, 0x18, 1, lib.file) == 1 && lib.header.has_entries == 1) {
    lib.entries = static_cast<Entry*>(std::malloc(lib.header.texture_count * sizeof(Entry)));
    if (std::fread(lib.entries, sizeof(Entry), lib.header.texture_count, lib.file) ==
        static_cast<size_t>(lib.header.texture_count)) {
      lib.palette_base = static_cast<int32_t>(g_state.palettes.size());
      if (lib.header.palette_count < 1) return true;
      const size_t first = g_state.palettes.size();
      g_state.palettes.resize(first + lib.header.palette_count, nullptr);
      for (size_t i = first; i < g_state.palettes.size(); ++i) {
        g_state.palettes[i] = static_cast<uint16_t*>(std::malloc(0x200));
        if (std::fread(g_state.palettes[i], 2, 0x100, lib.file) != 0x100) {
          // Quirk kept: the palette slots stay counted after a failed read.
          std::fclose(lib.file);
          lib.file = nullptr;
          std::free(lib.entries);
          lib.entries = nullptr;
          return false;
        }
        if (g_state.display_format == 5) to_555(g_state.palettes[i], 0x100);
        g_state.palettes[i] = shade(g_state.palettes[i]);
      }
      return true;
    }
    std::fclose(lib.file);
    lib.file = nullptr;
    std::free(lib.entries);
    lib.entries = nullptr;
    return false;
  }
  std::fclose(lib.file);
  lib.file = nullptr;
  return false;
}

// Mech3.exe 1.2 @ 0x00518db0
Texture* load(Library& lib, const char* name) {
  if (!open(lib) || !lib.entries) return nullptr;
  for (int i = 0; i < lib.header.texture_count; ++i) {
    const Entry& e = lib.entries[i];
    if (crt::msvc_stricmp(e.name, name) != 0) continue;
    std::fseek(lib.file, static_cast<long>(e.offset), SEEK_SET);
    Texture* t = read_texture(lib.file);
    if (e.palette == -1) return t;
    // The original writes through t unchecked; a failed read would crash there.
    if (t) t->palette = g_state.palettes[lib.palette_base + e.palette];
    return t;
  }
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00517c20
int bytes_per_pixel(const Texture& t) { return (t.flags & kTwoBytes) ? 2 : 1; }

// Mech3.exe 1.2 @ 0x00517d80
int read_info(std::FILE* f, Texture& t) {
  if (!f) return -1;
  uint8_t info[16];
  std::fread(info, 0x10, 1, f);
  int16_t w, h, palette_count, stretch;
  std::memcpy(&w, info + 4, 2);
  std::memcpy(&h, info + 6, 2);
  std::memcpy(&palette_count, info + 12, 2);
  std::memcpy(&stretch, info + 14, 2);
  t.width = w;  // 0x00517c90
  t.height = h;
  t.width_again = w;
  t.pixel_count = static_cast<int32_t>(w) * h;
  t.flags = info[0];   // 0x00517c60
  t.unk_08 = info[8];  // 0x00517c30
  t.palette_count = palette_count;
  t.stretch = stretch;
  return 0;
}

// Mech3.exe 1.2 @ 0x00517df0. Asking for more bytes per pixel than the texture
// has returns 0 without reading anything; fewer is an error.
int read_data(std::FILE* f, Texture& t, int bytes) {
  const int own = bytes_per_pixel(t);
  if (bytes == 0) bytes = own;
  if (bytes != own) return bytes <= own ? -1 : 0;
  const int32_t size = read_size(t, bytes);
  if (std::fread(t.pixels, 1, size, f) != static_cast<size_t>(size)) return -1;
  if (t.flags & kFullAlpha) {
    t.alpha = static_cast<uint8_t*>(std::malloc(t.pixel_count));
    const size_t n = std::fread(t.alpha, 1, t.pixel_count, f);
    t.flags |= kAlphaOwned;
    if (n != static_cast<size_t>(t.pixel_count)) return -1;
  }
  if (!(t.flags & kGlobalPalette) && t.palette_count != 0) {
    const int32_t psize = bytes * t.palette_count;
    t.palette = static_cast<uint16_t*>(std::malloc(psize));
    const size_t n = std::fread(t.palette, 1, psize, f);
    t.flags |= kPaletteOwned;
    if (n != static_cast<size_t>(psize)) return -1;
  }
  if (bytes == 2 && !(t.flags & kGlobalPalette)) {
    if (g_state.display_format == 5) {
      if (t.palette_count == 0) to_555(static_cast<uint16_t*>(t.pixels), t.pixel_count);
      else to_555(t.palette, t.palette_count);
    }
    if (t.palette_count != 0) t.palette = shade(t.palette);
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00517f80. Quirk kept: a failed pixel read still returns
// the texture.
Texture* read_texture(std::FILE* f) {
  auto* t = static_cast<Texture*>(std::calloc(1, sizeof(Texture)));  // 0x00517c10
  if (read_info(f, *t) != 0) return nullptr;
  t->pixels = std::malloc(read_size(*t, bytes_per_pixel(*t)));     // 0x00517c40
  read_data(f, *t, 0);
  t->flags |= kPixelsOwned;
  return t;
}

// Mech3.exe 1.2 @ 0x00517cc0 (with 0x00517d00). The device texture at +0x30 is
// the renderer's and is not ported.
void free_texture(Texture* t) {
  if (!t) return;
  if (t->pixels && (t->flags & kPixelsOwned)) std::free(t->pixels);
  if (t->alpha && (t->flags & kAlphaOwned)) std::free(t->alpha);
  if (t->palette && (t->flags & kPaletteOwned) && !(t->flags & kGlobalPalette)) std::free(t->palette);
  std::free(t);
}

}  // namespace zimg
