// Probes for test_shadow_image.py (docs/render-shadows.md): the projected-shadow
// casters and their images (zmod shadow_*, zcls shadow_object3d / shadow_lod,
// zrndr fill_polygon), their textures through zvid into a recording
// platform::Render3D, and the per-mech upkeep mech3::update_shadows with every
// call outside recorded (probe_app's callback).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_mechlib.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zrender/zrndr_span.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/app_hooks.h"
#include "mech3/app_states.h"
#include "probe_app.h"

namespace {

// The texture log, int32 words: 1 create: w, h, type, then w * h RGBA dwords;
// 2 update: texture id, w, h, RGBA; 3 release: texture id.
std::vector<int32_t> g_log;
int32_t g_handles = 0;

class Recorder : public platform::Render3D {
 public:
  int32_t create_texture(int w, int h, const uint8_t* rgba, uint8_t type) override {
    g_log.insert(g_log.end(), {1, w, h, type});
    put(rgba, w * h);
    return g_handles++;
  }
  void release_texture(int32_t handle) override { g_log.insert(g_log.end(), {3, handle + 1}); }
  void update_texture(int32_t handle, int w, int h, const uint8_t* rgba) override {
    g_log.insert(g_log.end(), {2, handle + 1, w, h});
    put(rgba, w * h);
  }
  void set_viewport(int, int, int, int) override {}
  void clear(bool, uint8_t, uint8_t, uint8_t) override {}
  void begin_scene() override {}
  void end_scene() override {}
  void draw(platform::Primitive, const platform::TLVertex*, int, const platform::DrawState&) override {}
  void set_fog_color(uint8_t, uint8_t, uint8_t) override {}
  void present() override {}

 private:
  static void put(const uint8_t* rgba, int n) {
    for (int i = 0; i < n; ++i) {
      int32_t v;
      std::memcpy(&v, rgba + 4 * i, 4);
      g_log.push_back(v);
    }
  }
};
Recorder g_recorder;

std::vector<int32_t> g_reports;  // zerr: level, line
void zerr_sink(int level, const char*, int line, const char*) { g_reports.insert(g_reports.end(), {level, line}); }

std::vector<zmod::ShadowCaster*> g_ids;  // caster id - 1
zmth::Mat34 g_base;
zcls::Node* g_root = nullptr;

// Live casters by id; a freed one (a stale pointer) is -1.
int32_t id_of(const zmod::ShadowCaster* c) {
  if (!c) return 0;
  for (size_t i = 0; i < g_ids.size(); ++i)
    if (g_ids[i] == c) return static_cast<int32_t>(i + 1);
  return -1;
}

void put_i32(std::vector<uint8_t>& b, int32_t v) {
  const auto* p = reinterpret_cast<const uint8_t*>(&v);
  b.insert(b.end(), p, p + 4);
}
void put_f(std::vector<uint8_t>& b, const float* f, int n) {
  const auto* p = reinterpret_cast<const uint8_t*>(f);
  b.insert(b.end(), p, p + 4 * n);
}
int finish(const std::vector<uint8_t>& b, uint8_t* out, int cap) {
  if (static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}

int32_t node_index(const zcls::Node* n) {
  if (!n) return -1;
  const zcls::NodePool& p = zcls::nodes();
  for (size_t b = 0; b < p.blocks.size(); ++b) {
    const zcls::Node* base = p.blocks[b].get();
    if (n >= base && n < base + p.block_size) return static_cast<int32_t>(b * p.block_size + (n - base));
  }
  return -2;
}

uint8_t* model_header(zcls::Node* n) {
  if (n->model) return n->model->header;
  if (n->lib_model) return n->lib_model->header;
  return nullptr;
}

}  // namespace

extern "C" {

// A fresh state: 565, a device, the stack with an identity base, no casters.
void probe_si_init() {
  zvid::set_pixel_format(5, 6, 5);
  zvid::set_render3d(&g_recorder);
  zvid::display() = zvid::Display{};
  zvid::default_texture() = nullptr;
  zrndr::device_fog().active = 1;
  zrndr::soft_target() = zrndr::SoftTarget{};
  zmod::shadow_state() = zmod::ShadowState{};
  zmod::caster_list() = nullptr;
  zmod::set_shadows(0);
  zmth::reset_stack();
  zmth::push(&g_base);
  zmth::load_identity();
  zerr::set_sink(zerr_sink);
  g_ids.clear();
  g_log.clear();
  g_reports.clear();
  g_handles = 0;
}

// The mechlib tree at `offset`, read into a fresh node pool; the root's index.
int probe_si_tree(const char* path, long offset) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  std::fseek(f, offset, SEEK_SET);
  zcls::nodes() = zcls::NodePool{};
  zcls::lists() = zcls::Lists{};
  zcls::frames() = zcls::Frames{};
  zcls::TreeReader lib;
  g_root = zcls::read_tree(lib, f, nullptr);
  std::fclose(f);
  return node_index(g_root);
}

// kind 0 node bytes, 1 class data, 2 model header (world or mechlib model).
int probe_si_bytes(int write, int kind, int index, int off, uint8_t* data, int len) {
  zcls::Node* n = zcls::node_at(index);
  if (!n) return -1;
  if (kind == 3) {  // polygon `off`'s flags
    uint32_t* f = n->lib_model ? &n->lib_model->polygons[off].flags : n->model ? &n->model->polygons[off].flags : nullptr;
    if (!f || len != 4) return -1;
    if (write) std::memcpy(f, data, 4);
    else std::memcpy(data, f, 4);
    return 4;
  }
  uint8_t* p = kind == 0 ? n->raw : kind == 1 ? static_cast<uint8_t*>(n->data) : model_header(n);
  if (!p) return -1;
  if (write) std::memcpy(p + off, data, len);
  else std::memcpy(data, p + off, len);
  return len;
}

// The walk's LoD morph ([0x0060bc20], [0x0060bac8]) and the image precision.
void probe_si_state(int morph, float factor, int precision) {
  zcls::scene_walk().morph = morph;
  zcls::scene_walk().morph_factor = factor;
  zmod::shadow_state().precision = precision ? zrndr::X87::k53 : zrndr::X87::k24;
}

// One call: 0 init, 1 new (its id), 2 size(w, h), 3 node(index), 4 enable,
// 5 colour, 6 position(f), 7 draw(f light, ground f[3]), 8 upload,
// 9 release texture, 10 delete, 11 pack(r, g, b).
int probe_si_op(int op, int id, int a, int b, const float* f) {
  zmod::ShadowCaster* c = id > 0 && id <= static_cast<int>(g_ids.size()) ? g_ids[id - 1] : nullptr;
  switch (op) {
    case 0: zmod::shadows_init(); return 0;
    case 1:
      g_ids.push_back(zmod::shadow_new());
      return static_cast<int>(g_ids.size());
    case 2: zmod::shadow_set_size(c, a, b); return 0;
    case 3: zmod::shadow_set_node(c, a < 0 ? nullptr : zcls::node_at(a)); return 0;
    case 4: zmod::shadow_enable(c, a); return 0;
    case 5: zmod::shadow_set_color(c, static_cast<uint32_t>(a)); return 0;
    case 6: zmod::shadow_set_position(c, f); return 0;
    case 7: return zmod::shadow_draw(c, f, f[3]);
    case 8: zmod::shadow_upload(c); return 0;
    case 9: zmod::shadow_release_texture(c); return 0;
    case 10:
      zmod::shadow_delete(c);
      g_ids[id - 1] = nullptr;
      return 0;
    case 11: return zvid::pack_bytes(static_cast<uint8_t>(id), static_cast<uint8_t>(a), static_cast<uint8_t>(b));
  }
  return -1;
}

// The casters from the head, the module's globals, the soft target, the
// stack top; textures by creation order (texture_ids, filled as they appear).
int probe_si_dump(uint8_t* out, int cap) {
  static std::map<const void*, int32_t> textures;
  if (g_ids.empty()) textures.clear();
  std::vector<uint8_t> b;
  const zmod::ShadowState& s = zmod::shadow_state();
  put_i32(b, id_of(zmod::caster_list()));
  put_i32(b, static_cast<int32_t>(s.black)), put_i32(b, static_cast<int32_t>(s.white));
  put_i32(b, id_of(s.current)), put_i32(b, zmod::shadows_on());
  const zrndr::SoftTarget& t = zrndr::soft_target();
  put_i32(b, t.width), put_i32(b, t.height), put_i32(b, t.pitch), put_i32(b, t.bytes);
  for (int32_t r : t.rect) put_i32(b, r);
  put_i32(b, t.winding);
  put_i32(b, zmth::matrix_stack().top);
  int guard = 0;
  for (const zmod::ShadowCaster* c = zmod::caster_list(); c && guard < 64; c = c->next, ++guard) {
    put_i32(b, id_of(c)), put_i32(b, node_index(c->node));
    put_f(b, c->position, 3), put_f(b, c->ground_at, 2);
    put_i32(b, static_cast<int32_t>(c->color));
    put_f(b, &c->ground_y, 1), put_f(b, c->slope, 2), put_f(b, &c->slope_len, 1);
    put_f(b, c->box_min, 3), put_f(b, c->box_max, 3), put_f(b, c->scale, 2);
    put_i32(b, c->enabled);
    const auto* img = static_cast<const zimg::Texture*>(c->image);
    put_i32(b, img != nullptr);
    if (img) {
      put_i32(b, img->pixel_count), put_i32(b, img->width), put_i32(b, img->height);
      put_i32(b, img->flags), put_i32(b, img->width_again), put_i32(b, img->alpha != nullptr);
      put_i32(b, img->pixels != nullptr);
      if (img->pixels) {
        const auto* p = static_cast<const uint8_t*>(img->pixels);
        b.insert(b.end(), p, p + 2 * img->width * img->height);
      }
    }
    const auto* tex = static_cast<const zvid::D3DTexture*>(c->texture);
    if (!tex) {
      put_i32(b, 0);
    } else if (tex == zvid::default_texture()) {
      put_i32(b, -2);
    } else {
      auto it = textures.find(tex);
      if (it == textures.end()) it = textures.emplace(tex, static_cast<int32_t>(textures.size() + 1)).first;
      put_i32(b, it->second), put_i32(b, tex->type), put_i32(b, tex->address_u), put_i32(b, tex->address_v);
    }
    put_i32(b, id_of(c->prev)), put_i32(b, id_of(c->next));
  }
  return finish(b, out, cap);
}

// Every node of the tree: flags, class data (object3d 0x90, LoD 0x50), the
// model header's flags and morph factor.
int probe_si_nodes(const int32_t* indices, int count, uint8_t* out, int cap) {
  std::vector<uint8_t> b;
  for (int k = 0; k < count; ++k) {
    const int32_t i = indices[k];
    zcls::Node* n = zcls::node_at(i);
    if (!n) return -1;
    put_i32(b, i), put_i32(b, static_cast<int32_t>(n->flags()));
    if (n->cls() == 5 && n->data) {
      const auto* d = static_cast<const uint8_t*>(n->data);
      b.insert(b.end(), d, d + 0x90);
    } else if (n->cls() == 6 && n->data) {
      const auto* d = static_cast<const uint8_t*>(n->data);
      b.insert(b.end(), d, d + 0x50);
    }
    if (const uint8_t* h = model_header(n)) {
      b.insert(b.end(), h + 8, h + 12);
      b.insert(b.end(), h + 0x24, h + 0x28);
    }
  }
  return finish(b, out, cap);
}

int probe_si_log(int32_t* out, int cap) {
  const int n = static_cast<int>(g_log.size());
  if (n > cap) return -1;
  std::memcpy(out, g_log.data(), sizeof(int32_t) * n);
  g_log.clear();
  return n;
}

int probe_si_reports(int32_t* out, int cap) {
  const int n = static_cast<int>(g_reports.size());
  if (n > cap) return -1;
  std::memcpy(out, g_reports.data(), sizeof(int32_t) * n);
  g_reports.clear();
  return n;
}

// The polygon filler alone into a w x h 16-bit image (pixels in/out).
void probe_si_fill(uint16_t* pixels, int w, int h, const float* verts, int count, uint32_t color, int winding,
                   int precision) {
  const int32_t rect[4] = {0, 0, w - 1, h - 1};
  zrndr::set_target(reinterpret_cast<uint8_t*>(pixels), rect, 0x10, w * 2);
  zrndr::soft_target().winding = winding;
  zrndr::fill_polygon(verts, count, color, precision ? zrndr::X87::k53 : zrndr::X87::k24);
}

}  // extern "C"

// --- The per-mech upkeep 0x004814e0 ---
namespace {
using probe_app::call;
using zgame::Arg;
using zgame::Handle;

struct Upkeep : mech3::Engine {
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  Handle shadow_new() override { return call("0x00542c20", {}); }
  void shadow_set_size(Handle c, int32_t w, int32_t h) override { call("0x00542d30", {c, w, h}); }
  void shadow_set_node(Handle c, Handle node) override { call("0x00542dd0", {c, node}); }
  void shadow_enable(Handle c, int32_t on) override { call("0x00542e60", {c, on}); }
  void shadow_set_color(Handle c, uint32_t color) override { call("0x00542e00", {c, color}); }
  void shadow_set_position(Handle c, Handle pos) override { call("0x00542de0", {c, pos}); }
  int32_t shadow_draw(Handle c, Handle light, float ground) override {
    return static_cast<int32_t>(call("0x00542e70", {c, light, zgame::bits(ground)}));
  }
  void shadow_upload(Handle c) override { call("0x00542e10", {c}); }
  void shadow_release_texture(Handle c) override { call("0x00542e40", {c}); }
  void shadow_delete(Handle c) override { call("0x00542ca0", {c}); }
  float clouds_at(float x, float z) override {
    return zgame::from_bits(call("0x00543ac0", {zgame::bits(x), zgame::bits(z)}));
  }
  int32_t model_global_light() override { return static_cast<int32_t>(call("0x00526a80", {})); }
};
}  // namespace

extern "C" {
void probe_si_upkeep() {
  zvid::set_pixel_format(5, 6, 5);
  Upkeep e;
  mech3::update_shadows(e);
}
}  // extern "C"
