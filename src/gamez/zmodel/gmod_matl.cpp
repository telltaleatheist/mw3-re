#include "gamez/zmodel/gmod_matl.h"

#include <cstdlib>
#include <cstring>

#include "gamez/zerror/zerr.h"
#include "gamez/zvideo/zvid_format.h"

namespace zmod {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zModel\\gmod_matl.c";
constexpr int kBlock = 1000;  // 0x005edd50
MaterialTable g_table;

bool read4(std::FILE* f, void* out) { return std::fread(out, 4, 1, f) == 1; }

template <typename T>
T at(const uint8_t* p, int off) {
  T v;
  std::memcpy(&v, p + off, sizeof v);
  return v;
}
}  // namespace

MaterialTable& materials() { return g_table; }

// Mech3.exe 1.2 @ 0x005363d0. FIX (A): one vector instead of 0x2c-byte slots
// in malloc'd blocks; capacity is still rounded up to whole blocks.
int read_materials(std::FILE* f) {
  MaterialTable& t = g_table;
  int32_t size, count, free_head, used_head;
  if (!read4(f, &size) || !read4(f, &count) || !read4(f, &free_head) || !read4(f, &used_head)) {
    zerr::report(0x200, kFile, 0, "Error reading GameZ Material buffer data.");
    return -1;
  }
  t.capacity = size, t.count = count, t.free_head = free_head, t.used_head = used_head;
  if (size == 0) return 0;
  const int blocks = (size + kBlock - 1) / kBlock;
  t.slots.assign(static_cast<size_t>(blocks) * kBlock, Material{});
  for (int i = 0; i < size; ++i) {
    uint8_t raw[0x2c];
    if (std::fread(raw, 0x2c, 1, f) != 1) return -1;
    Material& m = t.slots[i];
    m.alpha = raw[0], m.flags = raw[1], m.rgb = at<uint16_t>(raw, 2);
    m.r = at<float>(raw, 4), m.g = at<float>(raw, 8), m.b = at<float>(raw, 0xc);
    m.texture_index = at<uint32_t>(raw, 0x10);
    for (int k = 0; k < 3; ++k) m.unk_14[k] = at<float>(raw, 0x14 + 4 * k);
    m.soil = at<uint32_t>(raw, 0x20);
    m.file_cycle = at<uint32_t>(raw, 0x24);
    m.prev = at<int16_t>(raw, 0x28), m.next = at<int16_t>(raw, 0x2a);
  }
  // The rest of a partial last block goes on the free list (never in the data).
  for (int i = blocks * kBlock - 1; i >= size; --i) {
    Material& m = t.slots[i];
    m.flags = 0x20;
    m.next = static_cast<int16_t>(t.free_head);
    m.prev = -1;
    if (t.free_head >= 0) t.slots[t.free_head].prev = static_cast<int16_t>(i);
    t.free_head = i;
  }
  t.capacity = blocks * kBlock;
  for (int i = t.used_head; i != -1; i = t.slots[i].next) {
    Material& m = t.slots[i];
    if (m.flags & 1) {
      m.texture = zimg::texture_at(static_cast<int32_t>(m.texture_index));
    } else {
      const float rgb[3] = {m.r, m.g, m.b};
      m.rgb = zvid::pack_rgb(rgb);
    }
    if (m.flags & 4) {
      auto* c = static_cast<Cycle*>(std::calloc(1, sizeof(Cycle)));
      uint8_t raw[0x1c];
      if (std::fread(raw, 0x1c, 1, f) != 1) return -1;
      c->looping = at<uint32_t>(raw, 0), c->frame = at<uint32_t>(raw, 4);
      c->index = at<float>(raw, 8), c->speed = at<float>(raw, 0xc);
      c->texture_count = at<uint32_t>(raw, 0x10), c->unk_14 = at<uint32_t>(raw, 0x14);
      c->textures = static_cast<zimg::TextureEntry**>(std::malloc(c->texture_count * sizeof(void*)));
      for (uint32_t k = 0; k < c->texture_count; ++k) {
        int32_t idx;
        if (!read4(f, &idx)) {
          zerr::report(0x200, kFile, 0x3c0, "Error reading GameZ Material cycle texture data.");
          return -1;
        }
        c->textures[k] = zimg::texture_at(idx);
      }
      m.cycle = c;
    }
  }
  return t.capacity;
}

}  // namespace zmod
