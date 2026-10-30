// The animation runtime (zeff_anim_run, zeff_anim_events, zeff_anim_init's
// post-load pass) for tests/differential/test_anim_run.py: a mission's world
// and anim.zbd read, anim_load, then frames of the action lists with calls
// in between. The calls into code not ported are recorded (as the emulator
// side records the original's) and answer the same deterministic values.
// The dump is a list of 32-bit words the Python side builds the same way.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/sound.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zeffect/zeff_anim_save.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zsave/zsave.h"
#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_world.h"
#include "mech3/lab_view.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zvideo/zvid_format.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zweapon.h"

namespace {
using zcls::Node;
using zeff::AnimDef;

std::vector<uint32_t> g_trace;
uint32_t g_finds = 0, g_rays = 0;
std::vector<uint32_t> g_out;

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}
void tr(std::initializer_list<uint32_t> w) { g_trace.insert(g_trace.end(), w); }

int32_t nidx(const Node* n) { return n ? zeff::node_index(n) : -1; }

// Defs and their instances as ids: index, plus the place in the chain << 16.
std::map<const AnimDef*, int32_t> g_ids;
void build_ids() {
  g_ids.clear();
  zeff::AnimState& a = zeff::anim();
  for (int32_t i = 0; i < a.def_count(); ++i) {
    int32_t depth = 0;
    for (const AnimDef* d = &a.defs[i]; d; d = d->instance) g_ids[d] = i | (depth++ << 16);
  }
}
int32_t did(const AnimDef* d) {
  if (!d) return -1;
  auto it = g_ids.find(d);
  return it == g_ids.end() ? -2 : it->second;
}

int32_t puffer_id(const void* p) {
  if (!p) return -1;
  int32_t i = 0;
  for (const zmod::Puffer& q : zmod::puff_state().puffers) {
    if (&q == p) return i;
    ++i;
  }
  return -2;
}

int32_t tex_id(const zimg::TextureEntry* t) {
  if (!t) return -3;
  if (t == zimg::default_entry()) return -1;
  const zimg::TextureEntry* base = zimg::textures().entries;
  if (t >= base && t < base + zimg::kMaxTextures) return static_cast<int32_t>(t - base);
  return -2;
}

int32_t action_id(const void* a) {
  if (!a) return 0;
  if (a == reinterpret_cast<void*>(&zeff::anim_tick)) return 1;
  if (a == reinterpret_cast<void*>(&zeff::anim_reset_tick)) return 2;
  if (a == reinterpret_cast<void*>(&zeff::anim_wait_tick)) return 3;
  return 4;  // the puffer tick adapter
}

int32_t callback_id(const void* cb) {
  if (!cb) return 0;
  if (cb == reinterpret_cast<const void*>(&zeff::anim_on_collide)) return 1;
  if (cb == reinterpret_cast<const void*>(&zeff::anim_on_weapon_hit)) return 2;
  return 3;
}

void w(uint32_t v) { g_out.push_back(v); }
void wi(int32_t v) { g_out.push_back(static_cast<uint32_t>(v)); }
void wraw(const uint8_t* p, int n) {
  for (int o = 0; o + 4 <= n; o += 4) {
    uint32_t v;
    std::memcpy(&v, p + o, 4);
    w(v);
  }
}
// Raw words with some replaced by the given values.
void wraw_with(const uint8_t* p, int n, std::initializer_list<std::pair<int, uint32_t>> repl) {
  std::vector<uint8_t> c(p, p + n);
  for (auto& r : repl) std::memcpy(c.data() + r.first, &r.second, 4);
  wraw(c.data(), n);
}

int32_t seq_offset(const zeff::SeqDef& s) {
  const int32_t size = s.get<int32_t>(0x3c);
  if (s.events && s.cur >= s.events && s.cur <= s.events + (size > 0 ? size : 0)) return static_cast<int32_t>(s.cur - s.events);
  return -1;
}

void dump_seq(const zeff::SeqDef& s, bool blobs) {
  wraw_with(s.raw, 0x40, {{0x34, static_cast<uint32_t>(seq_offset(s))}, {0x38, 0}});
  if (blobs && s.get<int32_t>(0x3c) > 0) {
    std::vector<uint8_t> b(s.events, s.events + s.get<int32_t>(0x3c));
    b.resize((b.size() + 3) & ~size_t{3});
    wraw(b.data(), static_cast<int>(b.size()));
  }
}

void dump_def(const AnimDef& d, bool blobs) {
  std::vector<uint8_t> raw(d.raw, d.raw + 0x13c);
  std::memcpy(raw.data() + 0xc8, d.reset_seq.raw, 0x40);
  const auto put = [&](int off, uint32_t v) { std::memcpy(raw.data() + off, &v, 4); };
  put(0x40, static_cast<uint32_t>(nidx(d.node)));
  put(0x64, static_cast<uint32_t>(nidx(d.root)));
  put(0x68, static_cast<uint32_t>(nidx(d.object)));
  put(0x6c, d.callback != nullptr), put(0x70, 0);
  put(0x74, static_cast<uint32_t>(nidx(d.at_node)));
  put(0x84, static_cast<uint32_t>(nidx(d.at_node2)));
  put(0xc4, 0);
  put(0xfc, static_cast<uint32_t>(seq_offset(d.reset_seq)));
  put(0x100, 0);
  for (int off = 0x114; off <= 0x134; off += 4) put(off, 0);
  put(0x138, static_cast<uint32_t>(did(d.instance)));
  wraw(raw.data(), 0x13c);
  for (int i = 0; i < d.count(0x108); ++i) dump_seq(d.seqs[i], blobs);
  if (blobs && d.get<int32_t>(0x104) > 0) {
    std::vector<uint8_t> b(d.reset_events, d.reset_events + d.get<int32_t>(0x104));
    b.resize((b.size() + 3) & ~size_t{3});
    wraw(b.data(), static_cast<int>(b.size()));
  }
  for (int i = 0; i < d.count(0x109); ++i)
    wraw_with(d.objects[i].raw, 0x60, {{0x24, static_cast<uint32_t>(nidx(d.objects[i].node))}});
  for (int i = 0; i < d.count(0x10a); ++i)
    wraw_with(d.nodes[i].raw, 0x28, {{0x24, static_cast<uint32_t>(nidx(d.nodes[i].node))}});
  for (int i = 0; i < d.count(0x10b); ++i)
    wraw_with(d.lights[i].raw, 0x2c, {{0x24, static_cast<uint32_t>(nidx(static_cast<Node*>(d.lights[i].made)))}});
  for (int i = 0; i < d.count(0x10c); ++i)
    wraw_with(d.puffers[i].raw, 0x2c, {{0x24, static_cast<uint32_t>(puffer_id(d.puffers[i].made))}});
  for (int i = 0; i < d.count(0x10d); ++i)
    wraw_with(d.sounds[i].raw, 0x2c, {{0x24, static_cast<uint32_t>(nidx(static_cast<Node*>(d.sounds[i].made)))}});
  for (int i = 0; i < d.count(0x10e); ++i)
    wraw_with(d.static_sounds[i].raw, 0x24,
              {{0x20, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(d.static_sounds[i].sound))}});
  for (int i = 0; i < d.count(0x110); ++i)
    wraw_with(d.prereqs[i].raw, 0x30,
              {{0x28, static_cast<uint32_t>(did(d.prereqs[i].def))}, {0x2c, static_cast<uint32_t>(nidx(d.prereqs[i].node))}});
  for (int i = 0; i < d.count(0x112); ++i)
    wraw_with(d.anim_refs[i].raw, 0x48, {{0x44, static_cast<uint32_t>(did(d.anim_refs[i].ref))}});
}

void dump_puffer(const zmod::Puffer& p) {
  w(p.active), wi(p.number), w(p.timed_textures);
  wi(static_cast<int32_t>(p.textures.size()));
  for (auto& t : p.textures) w(bits(t.time)), wi(tex_id(t.texture));
  wi(static_cast<int32_t>(p.colors.size()));
  for (auto& c : p.colors) w(bits(c.r)), w(bits(c.g)), w(bits(c.b)), w(bits(c.a)), w(bits(c.time));
  wi(static_cast<int32_t>(p.growth.size()));
  for (auto& g : p.growth) w(bits(g.time)), w(bits(g.growth));
  w(p.by_distance);
  for (float v : {p.interval, p.inv_interval, p.accum, p.size_min, p.size_max, p.life_min, p.life_max, p.age_min,
                  p.age_max, p.deviation, p.friction, p.wind_factor})
    w(bits(v));
  wi(nidx(p.node));
  for (float v : p.prev) w(bits(v));
  w(p.has_prev);
  for (const float* v : {p.translate, p.local_velocity, p.world_velocity, p.min_random, p.max_random, p.accel})
    for (int i = 0; i < 3; ++i) w(bits(v[i]));
  for (float v : {p.near_start, p.near_end, p.near_inv, p.far_start, p.far_end, p.far_inv}) w(bits(v));
}

void dump_nodes_list(Node* const* list, int32_t count) {
  wi(list != nullptr);
  if (list)
    for (int32_t i = 0; i < count; ++i) wi(nidx(list[i]));
}

std::map<const void*, int32_t> g_records;
int32_t record_id(const void* r) {
  if (!r) return -1;
  auto it = g_records.find(r);
  if (it != g_records.end()) return it->second;
  const int32_t id = static_cast<int32_t>(g_records.size());
  g_records[r] = id;
  return id;
}

void dump_node(Node* n, bool full) {
  uint32_t owner;
  std::memcpy(&owner, n->raw + 0x40, 4);
  if (n->owner) owner = 0x80000000u | static_cast<uint32_t>(did(static_cast<AnimDef*>(n->owner)));
  wraw_with(n->raw, 0xd0,
            {{0x38, 0}, {0x3c, 0}, {0x40, owner}, {0x48, static_cast<uint32_t>(action_id(n->action))},
             {0x58, 0}, {0x60, 0}, {0xbc, static_cast<uint32_t>(record_id(n->damage))}, {0xcc, n->changed != nullptr}});
  w(n->link);
  dump_nodes_list(n->parents, n->parent_count());
  dump_nodes_list(n->children, n->child_count());
  if (zmod::Model* m = n->model) {
    wraw(m->header + 8, 4), wraw(m->header + 0x24, 4);
    const zmod::Material* mat = m->count(0x10) && m->polygons ? m->polygons[0].material : nullptr;
    if (mat && mat->cycle) wi(tex_id(mat->texture)), w(bits(mat->cycle->index));
    else wi(-9), wi(-9);
  } else {
    wi(-8), wi(-8), wi(-8), wi(-8);
  }
  if (!n->data) return;
  using namespace zcls;
  switch (n->cls()) {
    case 1: wraw_with(static_cast<Camera*>(n->data)->raw, 0x1e8, {{0, 0}, {4, 0}, {8, 0}, {0xc, 0}}); break;
    case 5: wraw(static_cast<Object3d*>(n->data)->raw, 0x90); break;
    case 9: {
      auto* l = static_cast<Light*>(n->data);
      wraw_with(l->raw, 0xd0, {{0xcc, 0}});
      dump_nodes_list(l->nodes, l->get<int32_t>(0xc8));
      break;
    }
    case 10: {
      auto* s = static_cast<Sound*>(n->data);
      wraw_with(s->raw, 0x94, {{0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s->sound))}, {4, 0}, {0x90, 0}});
      dump_nodes_list(s->nodes, s->get<int32_t>(0x8c));
      break;
    }
    case 2: {
      auto* wd = static_cast<World*>(n->data);
      wraw_with(wd->raw, 0xbc, {{0xc, 0}, {0x90, 0}, {0xa4, 0}, {0xa8, 0}, {0xb0, 0}, {0xb4, 0}});
      dump_nodes_list(wd->lights, wd->get<int32_t>(0xa0));
      dump_nodes_list(wd->sounds, wd->get<int32_t>(0xac));
      for (int32_t i = 0; i < wd->get<int32_t>(4); ++i) {
        int32_t at = -1;
        for (int32_t z = 0; at < 0 && z < wd->get<int32_t>(0x8c); ++z)
          for (int32_t x = 0; x < wd->get<int32_t>(0x88); ++x)
            if (reinterpret_cast<uint32_t*>(wd->rows[z][x].raw) == wd->area_pending[i]) at = z * 0x10000 + x;
        wi(at);
      }
      for (int32_t z = 0; full && z < wd->get<int32_t>(0x8c); ++z)
        for (int32_t x = 0; x < wd->get<int32_t>(0x88); ++x) {
          const Partition& p = wd->rows[z][x];
          w(p.get<uint32_t>(0));
          dump_nodes_list(p.nodes, p.get<int16_t>(0x3a));
        }
      break;
    }
    default: break;
  }
}

void puff_tick_action(Node*) { tr({8}); }

// The run's hooks: deterministic answers, every call recorded.
void install_hooks() {
  zeff::AnimHooks& h = zeff::anim_hooks();
  h.copy_tree = [](Node*) -> Node* { return nullptr; };
  h.create_puffer = []() -> void* { return zmod::puffer_create(); };
  h.find_sound = [](const char*) -> void* {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x70000000u + 0x10u * g_finds++));
  };
  h.effect_index = [](const char*) -> int32_t { return -1; };
  zcls::set_find_sound(h.find_sound);
  zeff::AnimRunHooks& r = zeff::anim_run_hooks();
  r = zeff::AnimRunHooks{};
  r.play_sound = [](void*, void* s, float v) { tr({1, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s)), bits(v)}); };
  r.play_sound_at = [](void*, void* s, const float p[3], float v) {
    tr({2, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s)), bits(p[0]), bits(p[1]), bits(p[2]), bits(v)});
  };
  r.ground = [](void*, Node*, float x, float y, float z, zeff::GroundHits* out) {
    tr({3, bits(x), bits(y), bits(z)});
    const auto ix = static_cast<int64_t>(std::floor(static_cast<double>(x) / 8.0));
    const auto iz = static_cast<int64_t>(std::floor(static_cast<double>(z) / 8.0));
    const int64_t h5 = (((ix * 7 + iz * 13) % 5) + 5) % 5;
    out->count = h5 == 4 ? 0 : static_cast<int32_t>(h5 % 2) + 1;
    out->hit[0].y = static_cast<float>(-h5) * 2.0f;
    out->hit[0].has_surface = h5 % 3 != 0;
    out->hit[0].surface = h5 % 3 == 1 ? 1 : 4;
    out->hit[1].y = out->hit[0].y + 6.0f;
    out->hit[1].has_surface = true;
    out->hit[1].surface = 2;
  };
  r.ray_down = [](void*, Node* n, const float* p, float height, int32_t* hit) -> int32_t {
    tr({4, static_cast<uint32_t>(nidx(n)), p != nullptr, p ? bits(p[0]) : 0, p ? bits(p[1]) : 0, p ? bits(p[2]) : 0,
        bits(height)});
    *hit = g_rays++ % 3 == 0;
    return 0;
  };
  r.effect = [](void*, AnimDef* d, uint8_t*) { tr({9, 3, static_cast<uint32_t>(did(d))}); };
  r.camera_state = [](void*, AnimDef* d, uint8_t*) { tr({9, 20, static_cast<uint32_t>(did(d))}); };
  r.camera_from_to = [](void*, AnimDef* d, zeff::SeqDef*, uint8_t*) -> int32_t {
    tr({9, 21, static_cast<uint32_t>(did(d))});
    return 2;
  };
  r.remove_child = [](void*, AnimDef* d, uint8_t*) { tr({9, 16, static_cast<uint32_t>(did(d))}); };
  r.detonate = [](void*, uint8_t* name, const float p[3]) {
    tr({10, fnv(reinterpret_cast<const char*>(name))});
    tr({11, bits(p[0]), bits(p[1]), bits(p[2])});
  };
  r.screen_tint = [](void*, uint16_t colour, double alpha) {
    uint64_t q;
    std::memcpy(&q, &alpha, sizeof q);
    tr({9, 36, colour, static_cast<uint32_t>(q), static_cast<uint32_t>(q >> 32)});
  };
  r.fbfx_wave = [](void*, AnimDef* d, zeff::SeqDef*, uint8_t*) -> int32_t {
    tr({9, 37, static_cast<uint32_t>(did(d))});
    return 2;
  };
  r.message = [](void*, uint8_t*) { tr({9, 38, 0}); };
  zcls::update_hooks().update_node = [](void*, Node* n) { tr({6, static_cast<uint32_t>(nidx(n))}); };
  zcls::update_hooks().frame_update = [](void*) { tr({7}); };
  zmod::PuffHooks& ph = zmod::puff_hooks();
  ph = zmod::PuffHooks{};
  ph.create_node = [](void*) -> Node* { return zcls::object3d_create(); };
  ph.set_priority = [](void*, Node* n, int32_t p) -> int32_t { return zcls::node_set_priority(n, p); };
  ph.set_callback = [](void*, Node* n, void (*tick)()) -> int32_t {
    return zcls::node_set_action(n, tick ? puff_tick_action : nullptr);
  };
}

void record_report(int level, const char*, int line, const char*) {
  tr({5, static_cast<uint32_t>(level), static_cast<uint32_t>(line)});
}
}  // namespace

extern "C" {

// Reads the world and sets the anim path; anim_load is a separate call.
int ar_open(const char* world, const char* anim_path) {
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
  zrndr::light_color() = zrndr::FogColor{};
  zrndr::software_format() = zrndr::SoftwareFormat{0xf800, 0x07e0, 8, 3, 3};
  zvid::clear_color() = 0;
  zcls::frames() = zcls::Frames{};
  zcls::list_walk() = zcls::ListWalk{};
  zmod::puff_state().puffers.clear();
  zmod::puff_state().particles.clear();
  zmod::puff_state().tick_node = nullptr;
  zmod::puff_state().wind = zmod::Wind{};
  zeff::anim_log() = zeff::AnimLog{};
  zwep::hit_state() = zwep::HitState{};
  zwep::hit_state().health_after = 0.0f;  // the original's zeroed global
  zmod::zones() = zmod::Zones{};
  g_trace.clear(), g_finds = 0, g_rays = 0, g_records.clear();
  zerr::set_sink(record_report);
  install_hooks();
  if (zcls::read_zbd(world) != 0) return -1;
  g_trace.clear();
  zeff::anim() = zeff::AnimState{};
  zeff::anim().fix_prerequisites = false;  // the exe's path (no shipped mission reaches it)
  zeff::anim().fix_instance_tree = false;  // the exe's path
  zeff::anim_allow_loading(1);
  zeff::anim_set_zbd_path(anim_path);
  zeff::anim_set_world(zcls::list_head(zcls::kWorlds)->node);
  ztimer::state().dt = 0.0f;
  return 0;
}

// 1: the node and frame updates recorded (events 6 and 7); 0: the port runs.
void ar_stub_update(int on) {
  zcls::update_hooks() = zcls::UpdateHooks{};
  if (!on) return;
  zcls::update_hooks().update_node = [](void*, Node* n) { tr({6, static_cast<uint32_t>(nidx(n))}); };
  zcls::update_hooks().frame_update = [](void*) { tr({7}); };
}

void ar_random(const float* table) { std::memcpy(zeff::anim().random, table, sizeof zeff::anim().random); }

int ar_load() {
  const int r = zeff::anim_load();
  build_ids();
  return r;
}

void ar_frame(float dt) {
  ztimer::state().dt = dt;
  zcls::run_action_lists();
  build_ids();
}

// Calls between frames (ids are def indices; node -1 for none).
void ar_start(int def, int node, float x, float y, float z) {
  zeff::anim_start(&zeff::anim().defs[def], node >= 0 ? zcls::node_at(node) : nullptr, x, y, z);
  build_ids();
}
void ar_start_at(int def, int node, const float* nine) {
  zeff::anim_start_at(&zeff::anim().defs[def], node >= 0 ? zcls::node_at(node) : nullptr, nine, nine + 3, nine + 6);
  build_ids();
}
void ar_restart(int def, int node) {
  zeff::anim_restart(&zeff::anim().defs[def], node >= 0 ? zcls::node_at(node) : nullptr);
  build_ids();
}
void ar_stop(int def) { zeff::anim_stop(&zeff::anim().defs[def]); build_ids(); }
void ar_invalidate(int def) { zeff::anim_invalidate(&zeff::anim().defs[def]); }
void ar_collide(int def, float damage) { zeff::anim_on_collide(&zeff::anim().defs[def], damage); build_ids(); }
void ar_hit(int def, uint32_t hit_flags, float damage) {
  zwep::WeaponDef weapon;
  weapon.flags = hit_flags;  // +0x6c
  zeff::anim_on_weapon_hit(&zeff::anim().defs[def], &weapon, nullptr, damage);
  build_ids();
}
void ar_player(float x, float y, float z) {
  const float p[3] = {x, y, z};
  zeff::anim_set_player(p);
}
void ar_zone(const uint8_t* zone) { zeff::anim_set_zone(zone); }

// The mission's end (0x0050ace0), and the anim registrants' save and load
// (test_anim_save.py): zSave made and the three added as zEffect's start
// adds them, `name` written or read in `dir` (kind 2).
// A node's state changed outside the animations (test_anim_save's played
// load): active or not, moved by (dx, dy, dz) from its translation.
void ar_perturb(int node, int active, float dx, float dy, float dz) {
  Node* n = zcls::node_at(node);
  zcls::node_set_active(n, active);
  float x, y, z;
  zcls::object3d_get_translation(n, &x, &y, &z);
  zcls::object3d_set_translation(n, x + dx, y + dy, z + dz);
}
// A def's (or its depth-th instance's) status set directly (test_anim_save's
// forced states).
void ar_set_status(int def, int depth, int value) {
  zeff::AnimDef* d = &zeff::anim().defs[def];
  for (int i = 0; i < depth && d; ++i) d = d->instance;
  if (d) d->raw[0x98] = static_cast<uint8_t>(value);
}
void ar_set_log_node(int record, int node) { zeff::anim_log().records[record].node = node; }
// test_lab_view.py: a world made and gridded (0x004f1bd0, 0x004f2500,
// 0x004f2530, 0x004f2560), its data and cells as words (pointers 0); the
// lab's frame step (0x00450690) on a new camera and object3d, the draw
// recorded.
int lv_world(float ox, float oz, float sx, float sz, float cx, float cz, int size_first, uint32_t* out, int len) {
  Node* w = zcls::world_create();
  if (size_first) zcls::world_set_size(w, sx, sz);
  zcls::world_set_origin(w, ox, oz);
  if (!size_first) zcls::world_set_size(w, sx, sz);
  zcls::world_make_grid(w, cx, cz);
  auto* d = static_cast<zcls::World*>(w->data);
  std::vector<uint32_t> o;
  auto words = [&](const uint8_t* p, int n) {
    for (int i = 0; i < n; i += 4) {
      uint32_t v;
      std::memcpy(&v, p + i, 4);
      o.push_back(v);
    }
  };
  words(d->raw, 0xbc);
  for (int32_t r = 0; r < d->get<int32_t>(0x8c); ++r)
    for (int32_t c = 0; c < d->get<int32_t>(0x88); ++c) words(d->rows[r][c].raw, 0x48);
  const int n = static_cast<int>(o.size());
  if (n <= len) std::memcpy(out, o.data(), o.size() * 4);
  return n;
}
namespace {
uint32_t g_drawn[4];
int lv_draw(Node* camera, Node* node, int32_t hw) {
  g_drawn[0] = static_cast<uint32_t>(camera != nullptr), g_drawn[1] = static_cast<uint32_t>(node != nullptr);
  g_drawn[2] = static_cast<uint32_t>(hw), g_drawn[3] = static_cast<uint32_t>(zmod::zones().mode);
  return 0;
}
}  // namespace
int lv_frame(const float* cam, const float* node_rot, float dt, uint32_t* out, int len) {
  mech3::LabView v;
  v.camera = zcls::camera_create();
  v.hardware = 1;
  Node* obj = zcls::object3d_create();
  zcls::object3d_set_rotation(obj, node_rot[0], node_rot[1], node_rot[2]);
  std::memset(g_drawn, 0xff, sizeof g_drawn);
  mech3::lab_view_frame(v, obj, cam, cam + 3, dt, lv_draw);
  std::vector<uint32_t> o;
  auto words = [&](const uint8_t* p, int n) {
    for (int i = 0; i < n; i += 4) {
      uint32_t x;
      std::memcpy(&x, p + i, 4);
      o.push_back(x);
    }
  };
  words(static_cast<zcls::Camera*>(v.camera->data)->raw, 0x1e8);
  words(static_cast<zcls::Object3d*>(obj->data)->raw, 0x90);
  for (uint32_t x : g_drawn) o.push_back(x);
  o.push_back(static_cast<uint32_t>(zmod::zones().mode));
  const int n = static_cast<int>(o.size());
  if (n <= len) std::memcpy(out, o.data(), o.size() * 4);
  return n;
}
// The armour section's highlight (0x004504d0) on a tree of named parts: the
// names a '\n'-separated list under one root, `first` then `section`
// highlighted (switching sections); each part's object3d data out.
int lv_highlight(const char* names, int first, int section, uint32_t* out, int len) {
  Node* root = zcls::object3d_create();
  zcls::node_set_name(root, "root");
  std::vector<Node*> parts;
  std::string all(names), one;
  for (size_t i = 0; i <= all.size(); ++i) {
    if (i < all.size() && all[i] != '\n') { one += all[i]; continue; }
    if (one.empty()) continue;
    Node* n = zcls::object3d_create();
    zcls::node_set_name(n, one.c_str());
    zcls::node_add_child(root, n);
    parts.push_back(n);
    one.clear();
  }
  mech3::lab_highlight(root, first);  // the section shown before
  mech3::lab_highlight(root, section);
  std::vector<uint32_t> o;
  for (Node* n : parts) {
    const uint8_t* d = static_cast<zcls::Object3d*>(n->data)->raw;
    for (int i = 0; i < 0x90; i += 4) {
      uint32_t x;
      std::memcpy(&x, d + i, 4);
      o.push_back(x);
    }
  }
  const int n = static_cast<int>(o.size());
  if (n <= len) std::memcpy(out, o.data(), o.size() * 4);
  return n;
}
void ar_stop_all(int keep) {
  zeff::anim_stop_all(keep);
  build_ids();
}
// zModel's sweeps (0x00537f20, 0x005392c0) and their state: each model
// slot's use count (+0x0c), polygon count (+0x10) and link (+0x5c), the
// pool's count and free head; each material's first word (alpha, flags),
// its links and the table's heads.
int ar_material_sweep(int step) { return zmod::material_sweep(step); }
void ar_material_sweep_reset() { zmod::material_sweep_reset(); }
int ar_models_free_unused() { return zmod::models_free_unused(); }
void ar_model_set_users(int i, int n) {
  zmod::ModelPool& p = zmod::models();
  if (i >= 0 && i < static_cast<int>(p.slots.size())) std::memcpy(p.slots[static_cast<size_t>(i)].header + 0xc, &n, 4);
}
int ar_pools(uint32_t* out, int len) {
  std::vector<uint32_t> o;
  zmod::ModelPool& p = zmod::models();
  o.push_back(static_cast<uint32_t>(p.size)), o.push_back(static_cast<uint32_t>(p.count)), o.push_back(static_cast<uint32_t>(p.free_head));
  for (int32_t i = 0; i < p.size && i < static_cast<int32_t>(p.slots.size()); ++i) {
    const zmod::Model& m = p.slots[static_cast<size_t>(i)];
    o.push_back(m.count(0xc)), o.push_back(m.count(0x10)), o.push_back(m.link);
  }
  zmod::MaterialTable& t = zmod::materials();
  o.push_back(static_cast<uint32_t>(t.capacity)), o.push_back(static_cast<uint32_t>(t.free_head)), o.push_back(static_cast<uint32_t>(t.used_head));
  for (int32_t i = 0; i < t.capacity; ++i) {
    const zmod::Material& m = t.at(i);
    o.push_back(static_cast<uint32_t>(m.alpha) | static_cast<uint32_t>(m.flags) << 8);
    o.push_back(static_cast<uint16_t>(m.prev) | static_cast<uint32_t>(static_cast<uint16_t>(m.next)) << 16);
  }
  const int n = static_cast<int>(o.size());
  if (n <= len) std::memcpy(out, o.data(), o.size() * 4);
  return n;
}
void ar_reset_all(int restart) {
  zeff::anim_reset_all(restart);
  build_ids();
}
int ar_save_game(const char* dir, const char* name, int load) {
  const zutl::Mounts kept = zutl::mounts();
  zutl::mounts().read = {dir};
  zutl::mounts().write = dir;
  if (!zsave::manager()) zsave::init();
  zeff::anim_register_savers();
  const bool ok = load ? zsave::load(name, 2) : zsave::save(name, 2);
  zutl::mounts() = kept;
  build_ids();
  return ok ? 1 : 0;
}
void ar_level(int level) { zeff::anim_set_level(level); }
void ar_drawn(int node) {
  Node* n = zcls::node_at(node);
  n->set_flags(n->flags() | 0x80000000u);
}

// The state as words: globals, defs (blobs and every node when `full`),
// puffers, the touched nodes, lists, the log and the hook trace since the
// last dump.
int ar_dump(int full, uint32_t* out, int cap) {
  g_out.clear();
  g_records.clear();
  zeff::AnimState& a = zeff::anim();
  std::vector<uint8_t> block(a.block.raw, a.block.raw + 0x44);
  std::memset(block.data() + 0x0c, 0, 4), std::memset(block.data() + 0x14, 0, 4);
  std::memset(block.data() + 0x18, 0, 4);
  wraw(block.data(), 0x44);
  w(a.stagger), wi(a.random_index), wi(a.stopped_elapsed), wi(a.level);
  wi(static_cast<int32_t>(reinterpret_cast<intptr_t>(zwep::hit_state().killed))), w(bits(zwep::hit_state().health_after));
  const int32_t count = a.def_count();
  wi(count);
  std::vector<Node*> touched;
  for (int32_t i = 0; i < count; ++i) {
    int32_t depth = 0;
    for (const AnimDef* d = &a.defs[i]; d; d = d->instance, ++depth) {
      wi(depth);
      dump_def(*d, full != 0);
      touched.push_back(d->node), touched.push_back(d->root), touched.push_back(d->object);
      for (int k = 0; k < d->count(0x109); ++k) touched.push_back(d->objects[k].node);
      for (int k = 0; k < d->count(0x10a); ++k) touched.push_back(d->nodes[k].node);
      for (int k = 0; k < d->count(0x10b); ++k) touched.push_back(static_cast<Node*>(d->lights[k].made));
      for (int k = 0; k < d->count(0x10d); ++k) touched.push_back(static_cast<Node*>(d->sounds[k].made));
    }
    wi(-1);
  }
  wi(static_cast<int32_t>(zmod::puff_state().puffers.size()));
  for (const zmod::Puffer& p : zmod::puff_state().puffers) dump_puffer(p);
  const zcls::NodePool& pool = zcls::nodes();
  wi(pool.capacity), wi(pool.live), wi(pool.free_head);
  if (full) {
    for (int32_t i = 0; i < pool.capacity; ++i) dump_node(zcls::node_at(i), true);
  } else {
    std::vector<int32_t> ids;
    for (Node* n : touched)
      if (n) ids.push_back(nidx(n));
    ids.push_back(nidx(zcls::list_head(zcls::kWorlds)->node));
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    wi(static_cast<int32_t>(ids.size()));
    for (int32_t i : ids) wi(i), dump_node(zcls::node_at(i), false);
  }
  for (int list = 0; list < 16; ++list) {
    if (!full && list == zcls::kAllNodes) continue;
    int32_t n = 0;
    for (zcls::ListItem* it = zcls::lists().list[list].head; it; it = it->next) ++n;
    wi(n);
    for (zcls::ListItem* it = zcls::lists().list[list].head; it; it = it->next) wi(nidx(it->node)), wi(it->removable);
    wi(zcls::lists().list[list].needs_purge);
  }
  // Damage records: for each record (first seen in pool order) its slots.
  wi(static_cast<int32_t>(g_records.size()));
  std::vector<const zwep::NodeHandlers*> recs(g_records.size());
  for (auto& kv : g_records) recs[kv.second] = static_cast<const zwep::NodeHandlers*>(kv.first);
  for (const zwep::NodeHandlers* r : recs)
    for (const auto& s : r->slot)
      wi(did(static_cast<const AnimDef*>(s.object))), wi(callback_id(reinterpret_cast<const void*>(s.fn)));
  const zeff::AnimLog& log = zeff::anim_log();
  wi(log.count);
  for (int32_t i = 0; i < log.count; ++i) {
    const zeff::AnimLogRecord& r = log.records[i];
    wi(r.type), w(r.serial);
    wraw(reinterpret_cast<const uint8_t*>(r.name), 0x20);
    wi(r.node);
    const int words = r.type == 1 ? 9 : r.type == 2 ? 3 : r.type == 3 ? 7 : r.type == 4 ? 8 : 0;
    for (int k = 0; k < words; ++k) w(r.args[k]);
  }
  wi(static_cast<int32_t>(g_trace.size()));
  g_out.insert(g_out.end(), g_trace.begin(), g_trace.end());
  g_trace.clear();
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), n * 4);
  return n;
}

}  // extern "C"

