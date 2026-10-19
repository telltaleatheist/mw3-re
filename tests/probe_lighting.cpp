// Probes for test_lighting.py (docs/drafts/lighting.md). Self-contained: no shared helpers.
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_fogcalc.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zvideo/zvid_format.h"

namespace {

// Synthetic lights: data blobs and nodes whose only field read is the flags.
std::vector<std::unique_ptr<zcls::Light>> g_data;
std::vector<std::unique_ptr<zcls::Node>> g_nodes;
std::vector<zcls::Light*> g_data_ptrs;
std::vector<zcls::Node*> g_node_ptrs;
std::vector<int32_t> g_reports;
float g_rec[128][8];

void record(int level, const char*, int line, const char*) {
  g_reports.push_back(level);
  g_reports.push_back(line);
}

// Each hook call: kind (1 point colour, 2 send mixed fog, 3 mix fog, 4 device fog send), rgb.
struct Log {
  float* out = nullptr;
  int32_t cap = 0, n = 0;
  void add(int kind, const float* rgb) {
    if (n < cap) {
      float* r = out + 4 * n;
      r[0] = static_cast<float>(kind);
      for (int k = 0; k < 3; ++k) r[1 + k] = rgb ? rgb[k] : 0.0f;
    }
    ++n;
  }
} g_log;

void hook_point(const float rgb[3]) { g_log.add(1, rgb); }
void hook_send() { g_log.add(2, nullptr); }
void hook_mix(float rgb[3]) { g_log.add(3, rgb); }
void hook_device() { g_log.add(4, zrndr::device_fog().sent); }

void set_hooks(float* log, int32_t cap) {
  zmod::LightHooks& h = zmod::light_hooks();
  h.point_color = hook_point, h.send_mix_fog = hook_send, h.mix_fog = hook_mix;
  zrndr::device_fog().send = hook_device;
  g_log = Log{log, cap, 0};
}

int32_t index_of(const zcls::Light* d) {
  for (size_t i = 0; i < g_data_ptrs.size(); ++i)
    if (g_data_ptrs[i] == d) return static_cast<int32_t>(i);
  return -1;
}

// entries: per list entry (data index, node index, full, reach).
void dump_entries(int32_t* out) {
  const zmod::LightList& L = zmod::light_list();
  for (int32_t i = 0; i < L.count; ++i) {
    const zmod::LightEntry& e = L.list[i];
    int32_t node = -1;
    for (size_t k = 0; k < g_node_ptrs.size(); ++k)
      if (g_node_ptrs[k] == e.node) node = static_cast<int32_t>(k);
    out[4 * i] = index_of(e.data), out[4 * i + 1] = node, out[4 * i + 2] = e.full, out[4 * i + 3] = e.reach;
  }
}

}  // namespace

extern "C" {

// Makes n lights from datas (n * 0xd0 bytes) and node flags, and builds the
// list. hdr: any, count, directed, report count; sw: the 16 floats at 0x7497c8.
void probe_light_setup(const uint8_t* datas, const uint32_t* node_flags, int32_t n, int32_t d3d,
                       const float* fog_color, int32_t* hdr, int32_t* entries, float* sw) {
  g_data.clear(), g_nodes.clear(), g_data_ptrs.clear(), g_node_ptrs.clear(), g_reports.clear();
  for (int32_t i = 0; i < n; ++i) {
    auto d = std::make_unique<zcls::Light>();
    std::memcpy(d->raw, datas + 0xd0 * i, 0xd0);
    auto node = std::make_unique<zcls::Node>();
    std::memset(node->raw, 0, sizeof node->raw);
    node->set_flags(node_flags[i]);
    g_data_ptrs.push_back(d.get()), g_node_ptrs.push_back(node.get());
    g_data.push_back(std::move(d)), g_nodes.push_back(std::move(node));
  }
  zrndr::device_fog().active = d3d;
  std::memcpy(zmod::fog().color, fog_color, 12);
  // The entries keep their full/reach from the last model, as the original does.
  zerr::set_sink(record);
  zmod::build_light_list(g_node_ptrs.data(), g_data_ptrs.data(), n);
  zerr::set_sink(nullptr);
  const zmod::LightList& L = zmod::light_list();
  hdr[0] = L.any, hdr[1] = L.count, hdr[2] = L.directed, hdr[3] = static_cast<int32_t>(g_reports.size() / 2);
  dump_entries(entries);
  std::memcpy(sw, L.sw_color, 16 * 4);
}

// 0x543d90 on the current list. rec: 128 x 8 floats in and out; entries out;
// *directed the list's directed index after.
int32_t probe_light_reaching(const float* centre, float radius, int32_t single, int32_t d3d, float* rec,
                             int32_t* entries, int32_t* directed) {
  std::memcpy(g_rec, rec, sizeof g_rec);
  zmod::lighting().rec = g_rec;
  zmod::lighting().single = single;
  zrndr::device_fog().active = d3d;
  const int32_t r = zmod::lights_reaching(centre, radius);
  std::memcpy(rec, g_rec, sizeof g_rec);
  dump_entries(entries);
  *directed = zmod::light_list().directed;
  return r;
}

// 0x53daa0. matrix/identity: the stack top; fog: 0x74a22c (0x38 bytes) and the
// fog's camera-to-world matrix; tint: rgb, weight.
uint32_t probe_model_needs(uint32_t flags, const float* centre, float radius, const float* matrix, int32_t identity,
                           const uint8_t* fog, const float* camera, uint32_t mask, int32_t tint_on, const float* tint,
                           int32_t single, int32_t d3d, float* rec, int32_t* entries, float* log, int32_t cap,
                           int32_t* logged) {
  static zmth::Mat34 top;
  std::memcpy(top.m, matrix, 48);
  zmth::matrix_stack() = zmth::MatrixStack{};
  zmth::push(&top);
  zmth::matrix_stack().is_identity() = identity;
  std::memcpy(&zmod::fog(), fog, sizeof(zmod::Fog));
  std::memcpy(zmod::fog_calc().camera_to_world, camera, 48);
  zmod::Lighting& G = zmod::lighting();
  G.mask_base = mask, G.mask = &G.mask_base;
  G.tint_on = tint_on;
  std::memcpy(G.tint, tint, 12);
  G.tint_weight = tint[3];
  G.single = single;
  zrndr::device_fog().active = d3d;
  std::memcpy(g_rec, rec, sizeof g_rec);
  G.rec = g_rec;
  set_hooks(log, cap);
  const uint32_t r = zmod::model_needs(flags, centre, radius);
  std::memcpy(rec, g_rec, sizeof g_rec);
  dump_entries(entries);
  *logged = g_log.n;
  zmth::matrix_stack() = zmth::MatrixStack{};
  return r;
}

// 0x545720. io: draw flags, vary flags in and out. state floats: clouds (on as
// float, speed, scale, base, amplitude, sin, cos, u, v), camera matrix (12),
// camera offset (3), tint (3), weight, stale (3), device colour (3), sent (3).
int32_t probe_light_shade(const float* baked, float* normal, int32_t count, uint32_t* io, int32_t shadowed,
                          uint32_t needs, const float* cam, int32_t vertex_normals, const float* normals,
                          float* rec, float* state, float* log, int32_t cap, int32_t* logged) {
  static float cam_buf[64][3], nrm_buf[64][3];
  std::memcpy(cam_buf, cam, 12 * count);
  std::memcpy(nrm_buf, normals, 12 * count);
  std::memcpy(g_rec, rec, 32 * count);
  zmod::Lighting& G = zmod::lighting();
  G.cam = cam_buf, G.normals = nrm_buf, G.vertex_normals = vertex_normals, G.rec = g_rec;
  zmod::Clouds& c = zmod::clouds();
  c.on = static_cast<int32_t>(state[0]);
  c.speed = state[1], c.scale = state[2], c.base = state[3], c.amplitude = state[4];
  c.heading_sin = state[5], c.heading_cos = state[6], c.u = state[7], c.v = state[8];
  std::memcpy(zmth::view().camera.m, state + 9, 48);
  std::memcpy(G.camera_offset, state + 21, 12);
  std::memcpy(G.tint, state + 24, 12);
  G.tint_weight = state[27];
  std::memcpy(G.stale, state + 28, 12);
  zrndr::DeviceFog& df = zrndr::device_fog();
  std::memcpy(df.color, state + 31, 12);
  std::memcpy(df.sent, state + 34, 12);
  set_hooks(log, cap);
  const int32_t r = zmod::shade_polygon(baked, normal, count, &io[0], &io[1], shadowed, needs);
  std::memcpy(rec, g_rec, 32 * count);
  std::memcpy(state + 28, G.stale, 12);
  std::memcpy(state + 31, df.color, 12);
  std::memcpy(state + 34, df.sent, 12);
  *logged = g_log.n;
  return r;
}

// Reads a world (0x4ff520), sets the view, then world_update_lights and
// world_build_lights(none). out: per world light its data bytes +0x00..+0xc8,
// then the list header (any, count, directed) and entries (light index).
int32_t probe_light_world(const char* path, const float* view, int32_t none, int32_t d3d, uint8_t* out, int32_t cap) {
  zimg::textures().count = 0;
  zimg::textures().group = 3;
  zmod::materials() = zmod::MaterialTable{};
  zmod::models() = zmod::ModelPool{};
  zvid::set_pixel_format(5, 6, 5);
  zcls::nodes() = zcls::NodePool{};
  zcls::lists() = zcls::Lists{};
  zmod::fog() = zmod::Fog{};
  zrndr::fog_color() = zrndr::FogColor{};
  zrndr::software_format() = zrndr::SoftwareFormat{0xf800, 0x07e0, 8, 3, 3};
  zvid::clear_color() = 0;
  zerr::set_sink(record);
  if (zcls::read_zbd(path) != 0) return -1;
  zerr::set_sink(nullptr);
  zcls::Node* world = nullptr;  // the first live world node in the pool
  for (int32_t i = 0; i < zcls::nodes().capacity && !world; ++i) {
    zcls::Node* nd = zcls::node_at(i);
    if (nd && (nd->link & zcls::kNodeLive) && nd->cls() == 2) world = nd;
  }
  if (!world) return -2;
  std::memcpy(zmth::view().view.m, view, 48);
  zmth::matrix_stack() = zmth::MatrixStack{};
  static zmth::Mat34 base;
  zmth::push(&base);
  zmth::load_identity();
  zmod::light_hooks() = zmod::LightHooks{};
  zrndr::device_fog().active = d3d;
  zmod::world_update_lights(world);
  zmod::world_build_lights(world, none);
  auto* w = static_cast<zcls::World*>(world->data);
  const int32_t n = w->get<int32_t>(0xa0);
  std::vector<uint8_t> b;
  auto put = [&](const void* p, size_t k) { b.insert(b.end(), static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + k); };
  put(&n, 4);
  for (int32_t i = 0; i < n; ++i) put(static_cast<zcls::Light*>(w->light_data[i])->raw, 0xc8);
  const zmod::LightList& L = zmod::light_list();
  put(&L.any, 4), put(&L.count, 4), put(&L.directed, 4);
  for (int32_t i = 0; i < L.count; ++i) {
    int32_t k = -1;
    for (int32_t j = 0; j < n; ++j)
      if (w->light_data[j] == L.list[i].data && w->lights[j] == L.list[i].node) k = j;
    put(&k, 4);
  }
  put(L.sw_color, 64);
  if (static_cast<int32_t>(b.size()) > cap) return -3;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int32_t>(b.size());
}

// Cloud setters and the sampler: op 0 set_level(a, b), 1 set_heading(a),
// 2 set_period(a), 3 set_wavelength(a), 4 sample(a, b). state: the 9 floats.
float probe_clouds(int32_t op, float a, float b, float* state) {
  zmod::Clouds& c = zmod::clouds();
  c.on = static_cast<int32_t>(state[0]);
  c.speed = state[1], c.scale = state[2], c.base = state[3], c.amplitude = state[4];
  c.heading_sin = state[5], c.heading_cos = state[6], c.u = state[7], c.v = state[8];
  float r = 0.0f;
  switch (op) {
    case 0: zmod::clouds_set_level(a, b); break;
    case 1: zmod::clouds_set_heading(a); break;
    case 2: zmod::clouds_set_period(a); break;
    case 3: zmod::clouds_set_wavelength(a); break;
    case 4: r = zmod::clouds_at(a, b); break;
    default: break;
  }
  const float s[9] = {static_cast<float>(c.on), c.speed, c.scale, c.base, c.amplitude,
                      c.heading_sin, c.heading_cos, c.u, c.v};
  std::memcpy(state, s, sizeof s);
  return r;
}

}  // extern "C"

#include "gamez/zclass/light.h"

extern "C" {

// test_light_setters.py: one setter on a light node whose data is `data`
// (0xd0 bytes); which 0 orientation, 1 ambient, 2 diffuse, 3 get orientation.
// Writes the data back, then the frame stamp and the result (and the read
// values for 3) to out.
int probe_light_setter(int which, uint8_t* data, int null_data, float a, float b, float c, uint32_t frame,
                       uint8_t* out) {
  static zcls::Node node;
  static zcls::Light light;
  std::memcpy(light.raw, data, sizeof light.raw);
  node.data = null_data ? nullptr : &light;
  zcls::frames().frame = frame;
  zcls::frames().update_frame = 0;
  float r[3] = {0, 0, 0};
  int res = 0;
  if (which == 0) res = zcls::light_set_orientation(&node, a, b, c);
  if (which == 1) res = zcls::light_set_ambient(&node, a);
  if (which == 2) res = zcls::light_set_diffuse(&node, a);
  if (which == 3) res = zcls::light_get_orientation(&node, &r[0], &r[1], &r[2]);
  std::memcpy(data, light.raw, sizeof light.raw);
  std::memcpy(out, &zcls::frames().update_frame, 4);
  std::memcpy(out + 4, &res, 4);
  std::memcpy(out + 8, r, 12);
  return 0;
}

}  // extern "C"
