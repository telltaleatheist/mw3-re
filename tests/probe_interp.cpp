// The script interpreter (gamez/zinterp, mech3/mech3_interp) for
// tests/differential/test_interp.py. A run is a small command list (the same
// text the test feeds the original): mounts, macros, settings, then
// `source` calls. Every engine call the commands make is recorded instead of
// run (fake nodes, models and textures stand in), as are the log, printf and
// zerr output and each line's progress; the state after is dumped. The text
// format matches the test's emulator side line for line.
#include <utime.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <cstddef>

#include "gamez/zclass/cls_data.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mech3_interp.h"

namespace {

using zcls::Node;
using zint::Engine;
using zmod::Model;

std::string g_out;
zint::Interp* g_in = nullptr;
int g_calls = 0;
std::set<std::string> g_fail;
int g_mkdir_errno = 0;
float g_ceiling = 150.0f;
bool g_world = false;

// Fake objects, in creation order, and their ids.
struct FakeNode {
  std::unique_ptr<Node> node;
  std::unique_ptr<zcls::Camera> cam;
  std::unique_ptr<zcls::Lod> lod;
};
std::vector<FakeNode> g_nodes;
std::vector<std::unique_ptr<Model>> g_models;  // made by model_new
std::vector<std::unique_ptr<zimg::TextureEntry>> g_textures;
std::map<const void*, std::string> g_ids;
std::map<std::string, Node*> g_by_name;
std::vector<const Model*> g_model_order;  // every fake model, for the dump

void put(const char* fmt, ...) {
  char buf[0x1000];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  g_out += buf;
}

std::string esc(const char* s) {
  if (!s) return "null";
  std::string r = "\"";
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
    if (*p >= 0x20 && *p < 0x7f && *p != '\\' && *p != '"' && *p != '|') {
      r += static_cast<char>(*p);
    } else {
      char b[8];
      std::snprintf(b, sizeof b, "\\x%02x", *p);
      r += b;
    }
  }
  return r + "\"";
}

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

// A real world (the integration runs): nodes by pool slot, models by pool
// index.
int32_t pool_node(const void* p) {
  const zcls::NodePool& pool = zcls::nodes();
  for (size_t b = 0; b < pool.blocks.size(); ++b) {
    const Node* base = pool.blocks[b].get();
    if (p >= base && p < base + pool.block_size)
      return static_cast<int32_t>(b) * pool.block_size + static_cast<int32_t>(static_cast<const Node*>(p) - base);
  }
  return -1;
}
int32_t pool_model(const void* p) {
  const std::vector<Model>& s = zmod::models().slots;
  if (s.empty() || p < s.data() || p >= s.data() + s.size()) return -1;
  return static_cast<int32_t>(static_cast<const Model*>(p) - s.data());
}

std::string id(const void* p) {
  if (!p) return "0";
  auto it = g_ids.find(p);
  if (it != g_ids.end()) return it->second;
  if (pool_node(p) >= 0) return "P" + std::to_string(pool_node(p));
  if (pool_model(p) >= 0) return "MP" + std::to_string(pool_model(p));
  return "?";
}

Model* new_model() {
  g_models.push_back(std::make_unique<Model>());
  Model* m = g_models.back().get();
  g_ids[m] = "M" + std::to_string(g_model_order.size());
  g_model_order.push_back(m);
  return m;
}

// A fake node; its name decides the rest: "clsN_" a class, "nomodel" no
// model, "nodata" no class data.
Node* new_node(const char* name, int32_t cls) {
  FakeNode f;
  f.node = std::make_unique<Node>();
  Node* n = f.node.get();
  std::memset(n->raw, 0, sizeof n->raw);
  if (name) {
    std::strncpy(reinterpret_cast<char*>(n->raw), name, 0x23);
    if (std::strncmp(name, "cls", 3) == 0) cls = std::atoi(name + 3);
  }
  n->put<int32_t>(0x34, cls);
  if (!name || !std::strstr(name, "nodata")) {
    if (cls == 1) {
      f.cam = std::make_unique<zcls::Camera>();
      std::memset(f.cam->raw, 0, sizeof f.cam->raw);
      n->data = f.cam.get();
    } else {
      f.lod = std::make_unique<zcls::Lod>();
      std::memset(f.lod->raw, 0, sizeof f.lod->raw);
      n->data = f.lod.get();
    }
  }
  g_ids[n] = "N" + std::to_string(g_nodes.size());
  g_nodes.push_back(std::move(f));
  if (!name || !std::strstr(name, "nomodel")) n->model = new_model();
  return n;
}

Node* find_fake(const char* name) {
  if (!name || std::strstr(name, "missing")) return nullptr;
  auto it = g_by_name.find(name);
  if (it != g_by_name.end()) return it->second;
  return g_by_name[name] = new_node(name, 0);
}

// The recorder, one line per engine call. Arguments by type: a pointer into
// the model being built is printed as its offset there.
const uint8_t* build_base() { return g_in && g_in->build ? g_in->build->raw : nullptr; }
const char* g_name_arg = nullptr;
int g_out_k = 0;

std::string build_off(const void* p) {
  const uint8_t* b = build_base();
  const uint8_t* q = static_cast<const uint8_t*>(p);
  if (b && q >= b && q < b + 0x1d8) {
    char s[16];
    std::snprintf(s, sizeof s, "B+%x", static_cast<unsigned>(q - b));
    return s;
  }
  return "";
}

void arg(Node* n) { put(" %s", id(n).c_str()); }
void arg(Model* m) { put(" %s", id(m).c_str()); }
void arg(zimg::TextureEntry* t) { put(" %s", id(t).c_str()); }
void arg(const char* s) {
  put(" %s", esc(s).c_str());
  if (!g_name_arg) g_name_arg = s;
}
void arg(float f) { put(" f:%08x", bits(f)); }
void arg(int v) { put(" %d", v); }
void arg(uint32_t v) { put(" %d", static_cast<int>(v)); }
void arg(const float* v) {
  const std::string b = build_off(v);
  if (!b.empty()) return put(" %s", b.c_str());
  put(" v:%08x,%08x,%08x", bits(v[0]), bits(v[1]), bits(v[2]));
}
void arg(float* o) {
  const std::string b = build_off(o);
  if (!b.empty()) return put(" %s", b.c_str());
  if (!o) return put(" null");
  *o = static_cast<float>(g_calls) + 0.25f * static_cast<float>(++g_out_k);
  put(" o");
}
void arg(void* p) {
  const std::string b = build_off(p);
  put(" %s", b.empty() ? "local" : b.c_str());
}
void arg(zint::Build* b) { put(" %s", b ? build_off(b).c_str() : "0"); }
void arg(zint::Action a) {
  put(" a:%s", !a ? "none" : a == zint::scroll_tick ? "tick" : a == zint::scroll_always_tick ? "always" : "?");
}

template <typename R> struct Ret;
template <> struct Ret<void> {
  static void make(const char*) {}
};
template <> struct Ret<int> {
  static int make(const char* name) {
    int r = 0;
    if (!std::strcmp(name, "list_count")) r = 3;
    else if (!std::strcmp(name, "polygon_material")) r = 42;
    else if (!std::strcmp(name, "pool_alive")) r = 1;
    else if (!std::strcmp(name, "make_dir")) r = g_mkdir_errno;
    if (g_fail.count(name)) r = 1;
    put(" -> %d", r);
    return r;
  }
};
template <> struct Ret<uint16_t> {
  static uint16_t make(const char*) {
    put(" -> %d", 0x1234);
    return 0x1234;
  }
};
template <> struct Ret<float> {
  static float make(const char*) {
    put(" -> f:%08x", bits(0.0125f));
    return 0.0125f;
  }
};
template <> struct Ret<Model*> {
  static Model* make(const char*) {
    Model* m = new_model();
    put(" -> %s", id(m).c_str());
    return m;
  }
};
template <> struct Ret<zimg::TextureEntry*> {
  static zimg::TextureEntry* make(const char*) {
    zimg::TextureEntry* t = nullptr;
    if (!g_name_arg || !std::strstr(g_name_arg, "missing")) {
      g_textures.push_back(std::make_unique<zimg::TextureEntry>());
      t = g_textures.back().get();
      g_ids[t] = "T" + std::to_string(g_textures.size() - 1);
    }
    put(" -> %s", id(t).c_str());
    return t;
  }
};
template <> struct Ret<Node*> {
  static Node* make(const char* name) {
    Node* n;
    if (!std::strcmp(name, "find_node") || !std::strcmp(name, "find_in_tree")) n = find_fake(g_name_arg);
    else if (!std::strcmp(name, "walk_prefix")) n = nullptr;
    else {
      int32_t cls = 0;
      if (!std::strcmp(name, "camera_create")) cls = 1;
      else if (!std::strcmp(name, "world_create")) cls = 2;
      else if (!std::strcmp(name, "window_create")) cls = 3;
      else if (!std::strcmp(name, "display_create")) cls = 4;
      else if (!std::strcmp(name, "object3d_create")) cls = 5;
      else if (!std::strcmp(name, "lod_create")) cls = 6;
      else if (!std::strcmp(name, "light_create")) cls = 9;
      else if (!std::strcmp(name, "seq_create")) cls = 7;
      n = new_node(nullptr, cls);
    }
    put(" -> %s", id(n).c_str());
    return n;
  }
};

template <typename R, typename... A>
R record(const char* name, A... a) {
  put("C %s", name);
  g_name_arg = nullptr;
  g_out_k = 0;
  (arg(a), ...);
  ++g_calls;
  return Ret<R>::make(name);
}

// Special cases: the node's model is written out, a name is written in.
int rec_get_model(Node* n, Model** out) {
  put("C get_model %s %s", id(n).c_str(), out ? "O" : "null");
  ++g_calls;
  int r = 0;
  if (!n) r = 5;
  else if (out) *out = n->model;
  put(" -> %d\n", r);
  return r;
}
int rec_set_name(Node* n, const char* name) {
  put("C set_name %s %s", id(n).c_str(), esc(name).c_str());
  ++g_calls;
  if (n && name) {
    const size_t len = std::strlen(name) < 34 ? std::strlen(name) : 34;
    std::memcpy(n->raw, name, len);
    n->raw[len] = 0;
  }
  put(" -> 0\n");
  return 0;
}
int rec_set_action(Node* n, zint::Action a) {
  put("C set_action %s", id(n).c_str());
  arg(a);
  ++g_calls;
  if (n) n->action = reinterpret_cast<void*>(a);
  put(" -> 0\n");
  return 0;
}
void rec_print(const char* text) { put("P %s\n", esc(text).c_str()); }

template <typename F> struct RetOf;
template <typename R, typename... P> struct RetOf<R (*)(P...)> { using type = R; };

template <typename R, typename... A>
R rec_line(const char* name, A... a) {
  if constexpr (std::is_void_v<R>) {
    record<R>(name, a...);
    put("\n");
  } else {
    R r = record<R>(name, a...);
    put("\n");
    return r;
  }
}

#define REC(m) e.m = [](auto... a) { return rec_line<RetOf<decltype(zint::Engine::m)>::type>(#m, a...); }

void install_recorders() {
  Engine& e = zint::engine();
  e = Engine{};
  REC(find_node); REC(find_in_tree); REC(walk_prefix); REC(list_count); REC(list_print); REC(node_alloc);
  e.set_name = rec_set_name;
  REC(set_active); REC(add_child); REC(delete_child); REC(delete_tree); REC(free_node); REC(free_world);
  e.get_model = rec_get_model;
  REC(set_model);
  e.set_action = rec_set_action;
  REC(set_can_modify); REC(set_overwrite); REC(set_action_priority);
  REC(set_altitude_surface); REC(set_intersect_surface); REC(set_intersect_bbox); REC(set_proximity);
  REC(set_landmark); REC(set_lighting); REC(pool_alive);
  REC(read_zbd); REC(write_prepare_a); REC(write_prepare_b); REC(write_zbd);
  REC(camera_create); REC(camera_set_rotate); REC(camera_get_translate); REC(camera_set_active);
  REC(camera_set_dynamic_lod); REC(camera_set_fov); REC(camera_set_horizon_xz); REC(camera_set_horizon);
  REC(camera_set_lod_multiplier); REC(camera_set_clip); REC(camera_get_clip); REC(camera_set_object_hse_test);
  REC(camera_set_window); REC(camera_set_world); REC(camera_set_translate);
  REC(display_create); REC(display_origin); REC(display_resolution); REC(display_set_clear_color);
  REC(window_create); REC(window_add_clear_vertex); REC(window_buffer); REC(window_close_clear_polygon);
  REC(window_origin); REC(window_resolution); REC(window_set_clear_polygon);
  REC(light_create); REC(light_set_ambient); REC(light_set_color); REC(light_set_diffuse); REC(light_set_directed);
  REC(light_set_directional); REC(light_set_orientation); REC(light_set_point); REC(light_set_ranges);
  REC(light_set_saturated); REC(light_set_static); REC(light_set_subdivide); REC(light_set_translate);
  REC(lod_create); REC(lod_add_child); REC(seq_create); REC(seq_add_child); REC(seq_set_active); REC(seq_set_loop);
  REC(seq_set_pause); REC(seq_set_repeat);
  REC(object3d_create); REC(object3d_add_child); REC(object3d_get_translate); REC(object3d_rotate);
  REC(object3d_scale); REC(object3d_set_opacity_is_set); REC(object3d_set_opacity); REC(object3d_translate);
  REC(world_create); REC(world_add_light); REC(world_extents); REC(world_origin); REC(world_partition_tolerance);
  REC(world_partition_max_dec); REC(world_partition); REC(world_set_fog_type); REC(world_set_fog_color);
  REC(world_get_fog_color); REC(world_set_fog_altitude); REC(world_set_fog_density); REC(world_set_fog_range);
  REC(world_get_fog_range); REC(world_set_virtual_map_size); REC(world_set_virtual_partition);
  REC(texture_add); REC(find_texture); REC(texture_directory); REC(pack_rgb); REC(matl_new);
  REC(material_array_size); REC(model3d_array_size); REC(model_new); REC(model_set_facade); REC(model_do_shadows);
  REC(polygon_material); REC(polygon_prepare); REC(polygon_add); REC(cycle_set_looping); REC(cycle_set_map);
  REC(cycle_set_on); REC(cycle_set_speed); REC(model_set_color); REC(model_set_priority);
  REC(model_set_show_backface); REC(model_set_morph); REC(model_set_scroll); REC(model_scroll_tick);
  REC(texture_world_base); REC(texture_world_per_meter); REC(palette_name); REC(palette_shading);
  REC(vertex_shading); REC(inverse_z_tolerance); REC(small_polygon_reject); REC(bfe_tolerance);
  REC(get_bfe_tolerance); REC(coplanar_tolerance); REC(colinear_tolerance); REC(write_texture_type);
  REC(write_texture_map); REC(perspective_delta_x); REC(perspective_adaptive); REC(perspective_inverse_z);
  REC(perspective_far_z); REC(clear_screen_buffer); REC(video_dither); REC(video_wireframe); REC(clouds_on);
  REC(clouds_period); REC(clouds_wavelength); REC(clouds_level); REC(clouds_heading); REC(make_shadows);
  REC(wind_max_speed); REC(wind_accel); REC(wind_ang_vel); REC(wind_static_velocity); REC(lens_flare_texture);
  REC(load_soils); REC(rdr_add_path); REC(rdr_set_path); REC(anim_set_zbd_path); REC(anim_debug_frame);
  REC(delete_file); REC(make_dir);
  e.print = rec_print;
}

void on_log(const char* fmt, va_list ap) {
  char buf[0x1000];
  // The exe passes no argument for this one's %s (zinterp_parse.cpp's FIX).
  if (!std::strcmp(fmt, "%s Failed: no current node")) std::snprintf(buf, sizeof buf, "<stray> Failed: no current node");
  else std::vsnprintf(buf, sizeof buf, fmt, ap);
  put("G %s\n", esc(buf).c_str());
}

// Pointers in zerr messages (%08x) are printed as the fake object's id.
void on_zerr(int level, const char*, int line, const char* message) {
  std::string m = message;
  for (size_t i = 0; (i = m.find("0x", i)) != std::string::npos; i += 2) {
    if (i + 10 > m.size()) break;
    const uint32_t v = static_cast<uint32_t>(std::strtoul(m.substr(i + 2, 8).c_str(), nullptr, 16));
    for (const auto& [p, name] : g_ids)
      if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)) == v && v) {
        m.replace(i + 2, 8, name);
        break;
      }
  }
  put("Z %x %x %s\n", level, line, esc(m.c_str()).c_str());
}

void on_line(zint::Interp& in) {
  put("L %u", in.argc);
  for (uint32_t i = 0; i < in.argc; ++i) put(" %s", esc(in.argv[i]).c_str());
  put("\n");
}
void on_dispatch(zint::Interp&) { put("D\n"); }
void on_done(zint::Interp& in, bool go) {
  put("X %d m%d e%d q%d d%u %s\n", go ? 1 : 0, in.misses, in.error, in.quit, in.skip_depth, id(in.current).c_str());
}

void dump(zint::Interp& in) {
  put("S macros");
  for (const zint::Macro& m : in.macros) put(" %s=%s", esc(m.name.c_str()).c_str(), esc(m.value.c_str()).c_str());
  put("\nS vars");
  for (const zint::Variable& v : in.vars) put(" %s:%d:%08x", v.name.c_str(), v.type, *static_cast<uint32_t*>(v.value));
  put("\nS depth %u includes %zu file %d binary %d loose %d current %s quit %d error %d\n", in.skip_depth,
      in.includes.size(), in.file ? 1 : 0, in.binary, in.loose_opens, id(in.current).c_str(), in.quit, in.error);
  const zint::Globals& g = zint::globals();
  put("S globals %s %s %s verbose %d persp %d\n", id(g.object_model).c_str(), id(g.cycle_model).c_str(),
      id(g.model_out).c_str(), g.verbose, zmod::draw_state().default_model);
  const mech3::InterpGlobals& m = mech3::interp_globals();
  put("S mw3 %08x %08x %08x ceiling %08x mech %s\n", bits(m.gravity), bits(m.inv_gravity), bits(m.damage_scale),
      bits(g_ceiling), in.mech ? "set" : "0");
  put("S build %s %s", id(in.build->texture).c_str(), id(in.build->model).c_str());
  for (int i = 0; i < 0x1d8; ++i) put(i % 4 ? "%02x" : " %02x", in.build->raw[i]);
  put("\nS scroll %s", id(zint::scroll_list().holder).c_str());
  for (Node* n : zint::scroll_list().nodes) put(" %s", id(n).c_str());
  put("\nS clutter");
  for (const std::string& s : zint::clutter_templates()) put(" %s", esc(s.c_str()).c_str());
  put("\n");
  for (const FakeNode& f : g_nodes) {
    put("S %s %s cls %d data", id(f.node.get()).c_str(), esc(f.node->name()).c_str(), f.node->cls());
    const uint8_t* d = f.cam ? f.cam->raw : f.lod ? f.lod->raw : nullptr;
    if (d)
      for (int i = 0; i < 0x50; ++i) put(i % 4 ? "%02x" : " %02x", d[i]);
    const void* a = f.node->action;
    put(" model %s action %s\n", id(f.node->model).c_str(),
        !a ? "none" : a == reinterpret_cast<void*>(zint::scroll_tick) ? "tick"
                    : a == reinterpret_cast<void*>(zint::scroll_always_tick) ? "always" : "?");
  }
  for (const Model* md : g_model_order) {
    put("S %s", id(md).c_str());
    for (int i = 0; i < 0x5c; ++i) put(i % 4 ? "%02x" : " %02x", md->header[i]);
    put("\n");
  }
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> v;
  std::istringstream is(s);
  for (std::string w; is >> w;) v.push_back(w);
  return v;
}

// The engine as the game starts with it: the ported functions.
const Engine& defaults() {
  static const Engine kDefaults = zint::engine();
  return kDefaults;
}

// Back to the ported functions, except the file and system calls and the
// world read (the world is read before the script).
void use_ported() {
  Engine& e = zint::engine();
  const Engine& d = defaults();
  using Fn = void (*)();
  const size_t n = sizeof(Engine) / sizeof(Fn);
  Fn* to = reinterpret_cast<Fn*>(&e);
  const Fn* from = reinterpret_cast<const Fn*>(&d);
  // LoadSoils (zmod::load_soils) stays recorded: it reads reader.zbd, not mounted here.
  const size_t keep[] = {offsetof(Engine, read_zbd), offsetof(Engine, delete_file), offsetof(Engine, make_dir),
                         offsetof(Engine, load_soils),
                         offsetof(Engine, print),
                         // Recorded too: the texture table, the lens flares' textures (they
                         // name the table's entries) and the model cycles (test_cycle.py).
                         offsetof(Engine, get_model), offsetof(Engine, texture_add),
                         offsetof(Engine, lens_flare_texture),
                         offsetof(Engine, cycle_set_on), offsetof(Engine, cycle_set_map),
                         offsetof(Engine, cycle_set_looping), offsetof(Engine, cycle_set_speed)};
  for (size_t i = 0; i < n; ++i) {
    bool k = false;
    for (size_t o : keep) k |= o == i * sizeof(Fn);
    if (from[i] && !k) to[i] = from[i];
  }
}

void dump_world() {
  put("S world live %d capacity %d anim %s\n", zcls::nodes().live, zcls::nodes().capacity,
      esc(zeff::anim().path).c_str());
  if (Node* h = zint::scroll_list().holder)
    put("S holder %s %s cls %d flags %08x\n", id(h).c_str(), esc(h->name()).c_str(), h->cls(), h->flags());
}

}  // namespace

extern "C" int probe_read_zbd(const char* path, int size, uint8_t* out, int cap);

extern "C" {

// Runs the command list in `config` and writes the trace; returns its length.
int probe_interp(const char* config, char* out, int cap) {
  defaults();
  g_out.clear();
  g_world = false;
  g_calls = 0;
  g_fail.clear();
  g_mkdir_errno = 0;
  g_ceiling = 150.0f;
  g_nodes.clear();
  g_models.clear();
  g_textures.clear();
  g_ids.clear();
  g_by_name.clear();
  g_model_order.clear();
  zutl::mounts() = zutl::Mounts{};
  zint::globals() = zint::Globals{};
  zint::globals().use_archive = 1;
  zint::scroll_list_init();
  zint::clutter_templates().clear();
  zmod::draw_state().default_model = 0;
  std::memset(zeff::anim().path, 0, sizeof zeff::anim().path);
  zrndr::device_fog().active = 0;
  mech3::interp_globals() = mech3::InterpGlobals{};
  install_recorders();
  zerr::set_sink(on_zerr);
  zint::observer() = zint::Observer{on_line, on_dispatch, on_done};

  zint::Interp in;
  g_in = &in;
  mech3::interp_create(in);
  mech3::interp_register_world_vars(in, &g_ceiling);
  in.log_fn = on_log;

  std::istringstream lines(config);
  for (std::string l; std::getline(lines, l);) {
    const std::vector<std::string> w = split(l);
    if (w.empty()) continue;
    const std::string& c = w[0];
    if (c == "world") {
      static std::vector<uint8_t> scratch(64 << 20);
      zerr::set_sink(nullptr);
      if (probe_read_zbd(w[1].c_str(), 0, scratch.data(), static_cast<int>(scratch.size())) < 0) return -1;
      zerr::set_sink(on_zerr);
      use_ported();
      g_world = true;
    } else if (c == "mount") zutl::mount(w[1]);
    else if (c == "archive") zint::globals().use_archive = std::stoi(w[1]);
    else if (c == "hardware") zrndr::device_fog().active = std::stoi(w[1]);
    else if (c == "console") in.console = static_cast<uint8_t>(std::stoi(w[1]));
    else if (c == "macro") zint::macro_set(in, w[1].c_str(), w.size() > 2 ? w[2].c_str() : "");
    else if (c == "fail") g_fail.insert(w[1]);
    else if (c == "mkdir_errno") g_mkdir_errno = std::stoi(w[1]);
    else if (c == "current") in.current = find_fake(w[1].c_str());
    else if (c == "mech") {
      static uint8_t mech[8];
      Node* n = find_fake(w[1].c_str());
      put("R %d\n", mech3::interp_set_mech(in, mech, n) ? 1 : 0);
    } else if (c == "source") {
      put("R %d\n", zint::source(in, w[1].c_str()) ? 1 : 0);
    } else if (c == "exec") {
      std::string text = l.substr(l.find("exec") + 5) + "\n";
      std::vector<char> buf(text.begin(), text.end());
      buf.resize(buf.size() + 0x400, 0);
      put("R %d\n", zint::exec_line(in, buf.data()) ? 1 : 0);
    } else if (c == "touch") {
      const std::string p = zutl::find(w[1].c_str());
      struct utimbuf t;
      t.actime = t.modtime = static_cast<time_t>(std::stoll(w[2]));
      if (!p.empty()) utime(p.c_str(), &t);
    } else if (c == "scrollclear") {
      zint::scroll_list_clear(std::stoi(w[1]) != 0);
    } else if (c == "scrolltick") {
      for (FakeNode& f : g_nodes)
        if (f.node->action == reinterpret_cast<void*>(zint::scroll_tick)) zint::scroll_tick(f.node.get());
      if (zint::scroll_list().holder) zint::scroll_always_tick(zint::scroll_list().holder);
    }
  }
  dump(in);
  if (g_world) dump_world();

  if (in.archive.f) std::fclose(in.archive.f);
  in.archive.f = nullptr;
  zint::destroy(in);
  g_in = nullptr;
  zerr::set_sink(nullptr);
  zint::observer() = zint::Observer{};
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n;
}

}  // extern "C"
