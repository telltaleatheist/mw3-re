// Probes for test_scenewalk.py (docs/drafts/scenewalk.md): a world read by
// the port's loaders, the camera set with the ported setters, the world
// render 0x4f7030 run with a recording model draw and recording hooks.
// Self-contained: no shared helpers.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"

namespace {

std::vector<uint8_t> g_out;
std::vector<zmod::ShadowCaster> g_casters;
int32_t g_cell_filter = 0;

void put(const void* p, size_t n) {
  const auto* b = static_cast<const uint8_t*>(p);
  g_out.insert(g_out.end(), b, b + n);
}
void u32(uint32_t v) { put(&v, 4); }
void f32(float v) { put(&v, 4); }

int32_t index_of(const zcls::Node* n) {
  const zcls::NodePool& p = zcls::nodes();
  for (size_t b = 0; b < p.blocks.size(); ++b) {
    const zcls::Node* base = p.blocks[b].get();
    if (n >= base && n < base + p.block_size) return static_cast<int32_t>(b * p.block_size + (n - base));
  }
  return -1;
}

// Event 1: a model draw ([0x74a2e0]): node, mask, the top of the stack and
// the render state the walk set around it.
int32_t record_draw(zcls::Node* node, uint32_t mask) {
  u32(1), u32(static_cast<uint32_t>(index_of(node))), u32(mask);
  put(zmth::matrix_stack().matrix().m, 48);
  const zmod::DrawState& ds = zmod::draw_state();
  f32(ds.opacity), u32(ds.overlay), u32(ds.shadows);
  u32(zmod::lighting().tint_on);
  u32(static_cast<uint32_t>(zcls::scene_walk().morph));
  return 0;
}

// The hidden-surface stand-in both sides use: hidden on a coarse grid of
// the centre, so a rounding difference cannot flip it.
int32_t hse(void*, const float c[3], float r) {
  const int32_t gx = static_cast<int32_t>(std::floor(c[0] / 37.0f));
  const int32_t gz = static_cast<int32_t>(std::floor(c[2] / 37.0f));
  const int32_t v = ((gx * 7 + gz * 13) % 5 + 5) % 5 != 0;
  u32(2), u32(static_cast<uint32_t>(gx)), u32(static_cast<uint32_t>(gz)), f32(r), u32(v);
  return v;
}

void frame_update(void*) { u32(3); }
int32_t zone_probe(void*, zcls::Node*, const float*, uint32_t*) {
  u32(4);
  return 0;
}
void collect(void*) {
  zmod::ShadowCasters& c = zmod::shadow_casters();
  c.all_count = 0;
  for (zmod::ShadowCaster& s : g_casters) c.all[c.all_count++] = &s;
  u32(5);
}
void directions(void*) { u32(6); }
// The casters over a cell: those whose node index has the parity of the
// cell rect's first value (floored), so lists differ from cell to cell.
void cell_casters(void*, const float* rect) {
  zmod::ShadowCasters& c = zmod::shadow_casters();
  const int32_t k = static_cast<int32_t>(std::floor(rect[0] / 100.0f));
  c.cell_count = 0;
  for (zmod::ShadowCaster& s : g_casters)
    if (((index_of(s.node) + k) & 1) == g_cell_filter) c.cell[c.cell_count++] = &s;
  u32(7);
}

zcls::Node* camera_node() { return zcls::list_head(zcls::kCameras)->node; }

}  // namespace

extern "C" {

// Reads the world like 0x4ff520 and resets the walk's state.
int sw_load(const char* path) {
  zimg::textures().count = 0;
  zimg::textures().group = 3;
  zmod::materials() = zmod::MaterialTable{};
  zmod::models() = zmod::ModelPool{};
  zvid::set_pixel_format(5, 6, 5);
  zcls::nodes() = zcls::NodePool{};
  zcls::nodes().free_head = -1;
  zcls::lists() = zcls::Lists{};
  zmod::fog() = zmod::Fog{};
  zrndr::fog_color() = zrndr::FogColor{};
  zrndr::software_format() = zrndr::SoftwareFormat{0xf800, 0x07e0, 8, 3, 3};
  zerr::set_sink(nullptr);
  const int r = zcls::read_zbd(path);
  zcls::scene_walk() = zcls::SceneWalk{};
  zmod::walk_state() = zmod::WalkState{};
  zmod::walk_state().draw = record_draw;
  zmod::zones() = zmod::Zones{};
  zmod::shadow_casters() = zmod::ShadowCasters{};
  zmod::draw_state() = zmod::DrawState{};
  zmod::lighting().tint_on = 0;
  zrndr::device_fog().active = 1;
  zmth::reset_stack();
  zcls::world_render_hooks() = zcls::WorldRenderHooks{};
  zcls::world_render_hooks().zone_probe = zone_probe;
  zcls::update_hooks() = zcls::UpdateHooks{};
  zcls::update_hooks().frame_update = frame_update;
  zmod::cull_hooks() = zmod::CullHooks{};
  zmod::cull_hooks().hidden_surface = hse;
  zmod::cull_hooks().collect_casters = collect;
  zmod::cull_hooks().caster_directions = directions;
  zmod::cull_hooks().cell_casters = cell_casters;
  g_casters.clear();
  return r;
}

int sw_camera() { return index_of(camera_node()); }
int sw_find(const char* name) { return index_of(zcls::list_find_by_name(zcls::kAllNodes, name)); }

int sw_set_horizon(int index) { return zcls::camera_set_horizon(camera_node(), index < 0 ? nullptr : zcls::node_at(index)); }

int sw_set_camera(float px, float py, float pz, float rx, float ry, float rz) {
  zcls::Node* c = camera_node();
  const int a = zcls::camera_set_rotation(c, rx, ry, rz);
  const int b = zcls::camera_set_position(c, px, py, pz);
  return a | (b << 4);
}

int sw_set_fov(float h, float v) { return zcls::camera_set_fov(camera_node(), h, v); }
int sw_set_clip(float n, float f) { return zcls::camera_set_clip(camera_node(), n, f); }

// Camera data dword at off (zones +0x1e0 / +0x1e4, flags +0x10).
void sw_camera_put(int off, uint32_t v) { static_cast<zcls::Camera*>(camera_node()->data)->put(off, v); }

// The globals a run varies: frame counter, frame step, HSE test, dynamic
// LoD, shadows, zone mode, the world's partition mode (+0x50).
void sw_globals(uint32_t frame, float dt, int hse_on, int lod_dynamic, int shadows, int zone_mode,
                int virtual_cells, int cell_filter) {
  zcls::frames().frame = frame;
  ztimer::state().dt = dt;
  zcls::scene_walk().hidden_surface = hse_on;
  zcls::scene_walk().lod_dynamic = lod_dynamic;
  zmod::draw_state().shadows = shadows;
  zmod::zones().mode = zone_mode;
  g_cell_filter = cell_filter;
  zcls::Node* world = zcls::camera_world(camera_node());
  static_cast<zcls::World*>(world->data)->put<int32_t>(0x50, virtual_cells);
}

void sw_casters(const int* nodes, int n) {
  g_casters.clear();
  for (int i = 0; i < n; ++i) g_casters.push_back(zmod::ShadowCaster{zcls::node_at(nodes[i])});
}

// One frame: the trace of draws and hooks, then the camera's data and the
// view; returns the byte count.
int sw_render(uint8_t* out, int cap) {
  g_out.clear();
  zcls::render_world(camera_node(), 0);
  u32(0xffffffff);
  put(static_cast<zcls::Camera*>(camera_node()->data)->raw, 0x1e8);
  put(zmth::view().view.m, 48);
  const zcls::SceneWalk& w = zcls::scene_walk();
  for (const zcls::ScreenRing& r : w.rings) {
    u32(static_cast<uint32_t>(r.count));
    for (int32_t i = 0; i < r.count; ++i) {
      const zcls::ScreenRing::Cell& c = r.cell[i];
      u32(static_cast<uint32_t>(c.x)), u32(static_cast<uint32_t>(c.z)), u32(static_cast<uint32_t>(c.wrapped));
      u32(c.mask);
    }
  }
  if (static_cast<int>(g_out.size()) > cap) return -1;
  std::memcpy(out, g_out.data(), g_out.size());
  return static_cast<int>(g_out.size());
}

// A node's sphere and flags after a frame.
int sw_node(int index, uint8_t* out) {
  zcls::Node* n = zcls::node_at(index);
  std::memcpy(out, n->raw + 0x24, 4);
  std::memcpy(out + 4, n->raw + 0x64, 16);
  return 20;
}

// 0x4ebdb0 and 0x4f9c40 on a given node with a given top of the stack.
int sw_box_sphere(int index, const float* top, int top_identity, float* out) {
  zcls::Node* n = zcls::node_at(index);
  zmth::Mat34 m;
  std::memcpy(m.m, top, 48);
  zmth::reset_stack();
  zmth::push(&m);
  zmth::matrix_stack().is_identity() = top_identity;
  float corners[8][3];
  const int r = zcls::node_box_corners(n, corners);
  zmth::pop();
  std::memcpy(out, corners, sizeof corners);
  zcls::corners_sphere(corners, out + 24, out + 27);
  return r;
}

// 0x4f5a90 on given points (count + 1 slots).
int sw_hull(float* points, int count) {
  return zcls::view_hull(reinterpret_cast<float(*)[3]>(points), count);
}

int sw_set_lod_scale(float f) { return zcls::camera_set_lod_scale(camera_node(), f); }

// Node flags &= ~clear (both sides of a test change the data the same way).
void sw_clear_flags(int index, uint32_t clear) {
  zcls::Node* n = zcls::node_at(index);
  n->set_flags(n->flags() & ~clear);
}

}  // extern "C"
