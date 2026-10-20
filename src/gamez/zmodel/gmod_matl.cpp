#include "gamez/zmodel/gmod_matl.h"

#include <cstdlib>
#include <cctype>
#include <cstring>

#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
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

// Mech3.exe 1.2 @ 0x00536070. No upper bound check.
Material* take_material(int32_t index) {
  if (index < 0) return nullptr;
  Material& m = g_table.slots[index];
  m.flags |= 0x10;
  return &m;
}

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

namespace {
std::vector<std::string> g_soils = {"default", "water", "seafloor", "quicksand", "lava", "fire"};

// The name test 0x00535f20 and 0x00537cb0 share: `name` starts with the soil
// and ends there or at a space.
bool soil_matches(const char* name, const std::string& soil) {
  const size_t n = soil.size();
  if (std::strlen(name) < n) return false;
  const unsigned char c = static_cast<unsigned char>(name[n]);
  if (c && !std::isspace(c)) return false;
  for (size_t i = 0; i < n; ++i)
    if (std::tolower(static_cast<unsigned char>(name[i])) != std::tolower(static_cast<unsigned char>(soil[i])))
      return false;
  return true;
}
}  // namespace

std::vector<std::string>& soil_names() { return g_soils; }

// Mech3.exe 1.2 @ 0x00535f20
int32_t soil_index(const char* name) {
  if (!name) return -1;
  for (size_t i = 0; i < g_soils.size(); ++i)
    if (soil_matches(name, g_soils[i])) return static_cast<int32_t>(i);
  return -1;
}

// Mech3.exe 1.2 @ 0x00537cb0: the names in the file's first list not known
// yet are added, up to 100 in all. The items are taken as strings unchecked.
int load_soils(const char* file) {
  if (!file) return 0;
  zrdr::Node* root = zrdr::load(file);
  if (!root) return 0;
  zrdr::Node* list = root->list[1].list;
  if (list) {
    const int32_t n = zrdr::list_count(&root->list[1]);
    for (int32_t i = 1; i < n; ++i) {
      const char* name = list[i].s;
      if (name && soil_index(name) != -1) continue;
      if (g_soils.size() >= 100) break;
      g_soils.emplace_back(name ? name : "");
    }
  }
  zrdr::free_node(root);
  return 0;
}

// Mech3.exe 1.2 @ 0x00537270
int reset_soils() {
  g_soils.resize(6);
  return 0;
}

}  // namespace zmod
