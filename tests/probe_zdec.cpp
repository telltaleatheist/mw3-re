// zDEClient for tests/differential/test_zdec_init.py: declient.zrd read by the
// port with the textures, animations and file stubbed as the test stubs the
// original's (fake handles by id), then everything it set written out.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zclass/cls_node.h"
#include "gamez/zdeclient/zdec_init.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zreader/zreader.h"

namespace {
std::vector<uint8_t> g_out;
std::vector<std::string> g_known_textures, g_added, g_anims;
zimg::TextureEntry g_tex[512];
const uint8_t* g_file = nullptr;
size_t g_file_len = 0;

void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void f32(float v) {
  uint32_t b;
  std::memcpy(&b, &v, 4);
  u32(b);
}
void str(const char* s) {
  const size_t n = s ? std::strlen(s) : 0xffffffffu;
  u32(static_cast<uint32_t>(n));
  if (s) g_out.insert(g_out.end(), s, s + n);
}
// Textures: 1000 + the known name's index, 2000 + the added one's; -1 the default.
uint32_t tex_id(const zimg::TextureEntry* t) {
  if (!t) return 0;
  if (t == zimg::default_entry()) return 0xffffffffu;
  return static_cast<uint32_t>(t - g_tex);
}
uint32_t mat_id(const zmod::Material* m) {
  return m ? static_cast<uint32_t>(zmod::materials().index_of(m)) : 0xfffffffeu;
}

zimg::TextureEntry* find_texture(const char* name) {
  u32(0x71), str(name);
  for (size_t i = 0; i < g_known_textures.size(); ++i)
    if (g_known_textures[i] == name) return &g_tex[i];
  for (size_t i = 0; i < g_added.size(); ++i)
    if (g_added[i] == name) return &g_tex[256 + i];
  return nullptr;
}
zimg::TextureEntry* add_texture(const char* name) {
  u32(0x72), str(name);
  g_added.push_back(name);
  return &g_tex[256 + g_added.size() - 1];
}
zeff::AnimDef* anim_find(const char* name) {
  u32(0x73), str(name);
  if (!name) return nullptr;
  for (size_t i = 0; i < g_anims.size(); ++i)
    if (g_anims[i] == name) return reinterpret_cast<zeff::AnimDef*>(0x1000 + 16 * i);
  return nullptr;
}
int32_t load_textures() { return u32(0x74), 0; }
void register_restore() { u32(0x75); }
zrdr::Node* load(const char*) {
  if (!g_file_len) return nullptr;
  std::FILE* f = fmemopen(const_cast<uint8_t*>(g_file), g_file_len, "rb");
  auto* root = new zrdr::Node{};
  zrdr::read_node(f, root);
  std::fclose(f);
  return root;
}
void sink(int level, const char*, int line, const char*) { u32(0x70), u32(level), u32(line); }

void split(const char* s, std::vector<std::string>& out) {
  out.clear();
  for (const char* p = s; *p;) {
    const char* e = std::strchr(p, '\n');
    out.emplace_back(p, e ? e - p : std::strlen(p));
    if (!e) break;
    p = e + 1;
  }
}
}  // namespace

// Runs 0x00501ab0 on `file` (empty: no file). `textures` and `anims` are the
// names that exist, one a line. Returns the bytes written to out.
extern "C" int probe_zdec_init(const uint8_t* file, int len, const char* textures, const char* anims,
                               int restore, uint8_t* out, int cap) {
  g_out.clear(), g_added.clear();
  split(textures, g_known_textures), split(anims, g_anims);
  g_file = file, g_file_len = static_cast<size_t>(len);
  zmod::materials() = zmod::MaterialTable{};
  zdec::state() = zdec::State{};
  zdec::Hooks& h = zdec::hooks();
  h.load = load, h.find_texture = find_texture, h.add_texture = add_texture, h.anim_find = anim_find;
  h.load_textures = load_textures, h.register_restore = register_restore;
  zerr::set_sink(sink);
  zdec::set_mode(restore);
  static zcls::Node world{};
  world.data = reinterpret_cast<void*>(0x5000);
  const int32_t r = zdec::init(&world);
  zerr::set_sink(nullptr);

  const zdec::State& s = zdec::state();
  u32(0x7f), u32(static_cast<uint32_t>(r));
  const zwep::CraterOrder& c = s.crater;
  u32(c.x00), u32(c.points), u32(0), u32(c.x0c), f32(c.depth), f32(c.radius);
  const zwep::QuicksandOrder& q = s.quicksand;
  u32(q.x00), u32(q.points), u32(q.cycle ? 1u : 0u), u32(q.still ? 1u : 0u), u32(q.x10), f32(q.depth), f32(q.radius);
  u32(s.texture_count);
  for (int32_t i = 0; i < s.texture_count; ++i) {
    const zdec::CraterTexture& e = s.textures[i];
    u32(mat_id(e.ground)), u32(mat_id(e.crater));
    u32(e.anim ? static_cast<uint32_t>((reinterpret_cast<uintptr_t>(e.anim) - 0x1000) / 16) : 0xffffffffu);
  }
  u32(s.quicksand_on);
  if (s.quicksand_on) {
    u32(s.frame_count), f32(s.frame_rate), u32(mat_id(s.quicksand_cycle)), u32(mat_id(s.quicksand_still));
  }
  const zmod::MaterialTable& t = zmod::materials();
  u32(t.count);
  for (int32_t i = t.used_head; i >= 0; i = t.at(i).next) {
    const zmod::Material& m = t.at(i);
    u32(i), u32(m.alpha), u32(m.flags), u32(m.rgb), f32(m.r), f32(m.g), f32(m.b), u32(tex_id(m.texture));
    f32(m.unk_14[0]), f32(m.unk_14[1]), f32(m.unk_14[2]), u32(m.soil);
    if (m.flags & 4) {
      const zmod::Cycle* cy = m.cycle;
      u32(cy->looping), f32(cy->index), f32(cy->speed), u32(cy->texture_count), u32(cy->added);
      for (uint32_t k = 0; k < cy->texture_count; ++k) u32(tex_id(cy->textures[k]));
    }
  }
  if (static_cast<int>(g_out.size()) > cap) return -1;
  std::memcpy(out, g_out.data(), g_out.size());
  return static_cast<int>(g_out.size());
}
