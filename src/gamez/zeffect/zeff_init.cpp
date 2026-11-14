#include "gamez/zeffect/zeff_init.h"

#include <cstring>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zreader/zreader.h"

namespace zeff {

namespace {
EffectHooks g_default_hooks;
EffectHooks* g_hooks = &g_default_hooks;
bool g_loaded = false;                 // 0x0073f3c0
int32_t g_count = 0;                   // 0x0073f3c8
zrdr::Node* g_root = nullptr;          // 0x0073f3cc  the file, kept (the names point into it)
std::vector<Effect> g_table;           // 0x0073f3d4  calloc(count, 0x4c)
EffectHandle g_world = nullptr;        // 0x0073f3d8
EffectHandle g_camera = nullptr;       // 0x0073f3dc
std::list<Effect*>* g_free = nullptr;  // 0x0073f3d0  the copies back from their run (0x00555e50's list)
std::vector<std::unique_ptr<Effect>> g_copies;  // ours: every copy made (the exe's malloc), freed with the table
std::unordered_map<EffectHandle, Effect*> g_running;  // a copy's node's +0x40
int32_t g_spawned = 0;                 // 0x0073f3e4
int32_t g_made = 0;                    // 0x0073f3e0
int32_t g_reused = 0;                  // 0x0073f3e8

// Mech3.exe 1.2 @ 0x0050d610: the node's own model, else the first found under its children, depth first.
EffectHandle find_gfx(EffectHandle node) {
  EffectHooks& h = *g_hooks;
  if (EffectHandle m = h.node_model(node)) return m;
  for (int32_t i = 0; i < h.child_count(node); ++i)
    if (EffectHandle m = find_gfx(h.child(node, i))) return m;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x0050d880: a fresh copy of entry `index`: its node copied (0x004f9af0(node, 1, 1)) and its
// gfx found under the copy; none without the entry's node, the copy or its gfx (the exe leaks those).
Effect* copy_of(int32_t index) {
  if (index == -1) return nullptr;
  const Effect& e = g_table[size_t(index)];
  if (!e.node) return nullptr;
  auto c = std::make_unique<Effect>(e);
  c->flags |= 1;
  c->node = g_hooks->node_copy(e.node);
  if (!c->node) return nullptr;
  c->flags |= 2;
  c->gfx = find_gfx(c->node);
  if (!c->gfx) return nullptr;
  g_copies.push_back(std::move(c));
  return g_copies.back().get();
}

// Mech3.exe 1.2 @ 0x0050d820: a free copy of `index` taken back (0x00556120, the first of its index; 0x00555f70),
// else a new one.
Effect* get(int32_t index) {
  if (index == -1) return nullptr;
  if (g_free)
    for (auto it = g_free->begin(); it != g_free->end(); ++it)
      if ((*it)->index == index) {
        Effect* e = *it;
        g_free->erase(it);
        ++g_reused;
        return e;
      }
  ++g_made;
  return copy_of(index);
}

// Mech3.exe 1.2 @ 0x0050d7a0: the camera's place less `at`, squared, summed (x and y, then z), as a float.
float camera_distance2(const float* at) {
  float c[3];
  g_hooks->camera_position(g_camera, c);
  const float dx = c[0] - at[0], dy = c[1] - at[1], dz = c[2] - at[2];  // stored as floats, then squared in the x87
  return static_cast<float>((double{dx} * dx + double{dy} * dy) + double{dz} * dz);
}

// Mech3.exe 1.2 @ 0x0050d6a0: the copy's run set (grow 0.3 s at 6 a second, then shrink at 3.75 until 1 s, from
// 5), then, by its squared distance to the camera: under 56.25 nothing more (not placed or shown); under 1600 its
// size and growth scaled by the distance over 1600; then sized, placed, its cycle restarted and put in the world.
bool place(Effect& e, const float* at) {
  EffectHooks& h = *g_hooks;
  e.t = 0;
  e.scale = 5.0f, e.x40 = 1.4f, e.near2 = 56.25f, e.far2 = 1600.0f;
  e.grow_time = 0.3f, e.grow = 6.0f, e.x20 = 6.0f, e.x24 = 30.0f, e.shrink = 3.75f, e.x2c = 3.0f, e.x30 = 10.5f;
  e.life = 1.0f;
  const float d2 = camera_distance2(at);
  if (!(d2 >= e.near2)) {  // under, or unordered
    e.scale = 0, e.grow_time = 0, e.life = 0;
    return false;
  }
  if (!(d2 >= e.far2)) {
    const double f = double{d2} / e.far2;  // kept in the x87 for both products
    e.scale = static_cast<float>(f * e.scale);
    e.grow = static_cast<float>(f * e.grow);
  }
  h.set_scale(e.node, e.scale);
  h.set_translation(e.node, at);
  h.cycle_reset(e.gfx);
  h.add_child(g_world, e.node);
  ++g_spawned;
  return true;
}
}  // namespace

void set_effect_hooks(EffectHooks* h) { g_hooks = h ? h : &g_default_hooks; }
EffectHooks& effect_hooks() { return *g_hooks; }

zrdr::Node* EffectHooks::load(const char* file) { return zrdr::load(file); }

// Mech3.exe 1.2 @ 0x004ed530 on [0x0073f3dc]: the camera node's world place.
void EffectHooks::camera_position(EffectHandle cam, float out[3]) {
  unported("0x004ed530", {h(cam)});
  for (int i = 0; i < 3; ++i) out[i] = zgame::from_bits(unported("@camera", {i}));
}

// Mech3.exe 1.2 @ 0x0050d2c0 (ECX the world, EDX the camera, kept at 0x0073f3d8 / 0x0073f3dc). Once: the EFFECTS
// list's entries, each (the node's name first) "NAME", "MAPS" (its texture cycle's frames), "SPEED", "LOOPING":
// its node found (list 6), hidden and out of the queries, its gfx found under it, the gfx a facade (mode 1) with
// the cycle set; an entry whose node or gfx is missing reported and left at index -1. Then the textures loaded and
// the free list made.
int32_t effects_init(const char* file, EffectHandle world, EffectHandle camera) {
  if (g_loaded) return 0;
  EffectHooks& h = *g_hooks;
  zrdr::Node* root = g_root = h.load(file);
  if (!root) {
    if (h.error) h.error(file);
    return -1;
  }
  zrdr::Node* list = zrdr::find(root, "EFFECTS");
  g_count = list ? zrdr::list_count(list) - 1 : 0;  // FIX (A): no EFFECTS: none (the exe reads through 0)
  g_table.assign(size_t(g_count < 0 ? 0 : g_count), Effect{});
  g_world = world, g_camera = camera;
  for (int32_t i = 0; i < g_count; ++i) {
    zrdr::Node* entry = &list->list[i + 1];
    zrdr::Node* maps = zrdr::find(entry, "MAPS");
    Effect& e = g_table[size_t(i)];
    e.index = -1;
    e.node_name = entry->list[1].s;
    e.name = zrdr::get_string(entry, "NAME");
    e.flags = 0;
    e.node = h.node_named(e.node_name);
    if (!e.node) {
      h.report("%s(%d): Failed to find node (%s) for (%s)\n", e.node_name, e.name);
      continue;
    }
    EffectHandle gfx = find_gfx(e.node);
    if (!gfx) {
      h.report_gfx(e.node_name);
      continue;
    }
    h.set_flag8(e.node, 0);
    h.set_intersect(e.node, 0);
    h.set_active(e.node, 0);
    e.index = i;
    e.gfx = gfx;
    h.model_facade(gfx, 1);
    const int32_t frames = maps ? zrdr::list_count(maps) - 1 : 0;  // FIX (A): no MAPS: none (the exe reads 0)
    h.cycle_on(gfx, frames);
    float speed = 0;
    zrdr::get_float(entry, "SPEED", &speed);
    h.cycle_speed(gfx, speed);
    if (zrdr::Node* loop = zrdr::find(entry, "LOOPING")) h.cycle_looping(gfx, std::strcmp(loop->list[1].s, "ON") == 0);
    for (int32_t j = 1; j <= frames; ++j) h.cycle_map(gfx, h.add_texture(maps->list[j].s));
  }
  h.textures_load();
  g_free = new std::list<Effect*>;
  g_loaded = true;
  return 0;
}

int32_t effect_count() { return g_count; }
const Effect* effect_entry(int32_t i) { return i >= 0 && i < int32_t(g_table.size()) ? &g_table[size_t(i)] : nullptr; }
int32_t effects_spawned() { return g_spawned; }

// Mech3.exe 1.2 @ 0x0050d9e0: the first entry of that NAME (an entry without one passed: FIX (A), the exe compares
// with 0).
int32_t effect_find(const char* name) {
  for (int32_t i = 0; i < g_count; ++i)
    if (g_table[size_t(i)].name && std::strcmp(name, g_table[size_t(i)].name) == 0) return i;
  return -1;
}

// Mech3.exe 1.2 @ 0x0050d650: a free copy of the effect (0x0050d820) placed (0x0050d6a0), shown and given its tick
// (the node's +0x40 the copy, its action 0x0050d910). Only with a loaded table and an index.
void effect_spawn(int32_t index, const float* at) {
  EffectHooks& h = *g_hooks;
  if (h.spawn_asked) h.spawn_asked(index, at);
  if (!g_loaded || index == -1) return;
  static const float origin[3] = {0, 0, 0};
  Effect* e = get(index);
  if (!e) return;
  place(*e, at ? at : origin);
  h.set_active(e->node, 1);
  g_running[e->node] = e;
  h.set_action(e->node, true);
}

const Effect* effect_of(EffectHandle node) {
  auto it = g_running.find(node);
  return it == g_running.end() ? nullptr : it->second;
}

// Mech3.exe 1.2 @ 0x0050d910 (the node's action): for a node marked (+0x24 bit 2), its copy ages by the frame;
// growing until its grow time, then shrinking (not under 0.01) until its life is out; then off the node, back to
// the free list, the action off, hidden and out of the world when in it.
void effect_tick(EffectHandle node) {
  EffectHooks& h = *g_hooks;
  if (!(h.node_flags(node) & 4)) return;
  Effect* e = g_running.count(node) ? g_running[node] : nullptr;
  if (!e) return;  // FIX (A): the exe reads the copy through the node's +0x40 unchecked
  // The x87 keeps each sum and product wide: the age is compared before it is stored, the size before it is.
  const double t = double{h.dt()} + e->t;
  e->t = static_cast<float>(t);
  if (!(t >= e->grow_time)) {
    e->scale = static_cast<float>(double{e->grow} * h.dt() + e->scale);
    h.set_scale(node, e->scale);
    return;
  }
  if (!(t >= e->life)) {
    const double s = e->scale - double{e->shrink} * h.dt();
    e->scale = !(s >= 0.01) ? 0.01f : static_cast<float>(s);  // [0x005cdd00], a double; 0x3c23d70a stored
    h.set_scale(node, e->scale);
    return;
  }
  g_running.erase(node);
  if (g_free) g_free->push_back(e);  // 0x00555f30
  h.set_action(node, false);
  h.set_active(node, 0);
  if (h.node_parent(node)) h.remove_child(g_world, node);
}

// Mech3.exe 1.2 @ 0x0050d270: the table cleared, then zEffect's animation state started (0x00508fc0).
void effects_init() {
  g_loaded = false;
  g_count = 0;
  g_root = nullptr;
  g_table.clear();
  g_world = g_camera = nullptr;
  g_spawned = g_made = g_reused = 0;
  anim_init();
}

// Mech3.exe 1.2 @ 0x0050d570: the file and the table freed; each copy on the free list (0x00556070, from its head)
// its node destroyed when it made one (0x004f9050) and freed; the list freed; then 0x0050d270. Copies still running
// stay with their nodes in the exe; the port frees their memory too.
void effects_free() {
  if (g_root) zrdr::free_node(g_root);
  g_root = nullptr;
  g_table.clear();
  if (g_free) {
    for (Effect* e : *g_free)
      if (e->flags & 2) g_hooks->node_destroy(e->node);
    delete g_free;
    g_free = nullptr;
  }
  g_running.clear();
  g_copies.clear();
  effects_init();
}

// Mech3.exe 1.2 @ 0x0050d2b0, at the game's end.
void effects_shutdown() {
  effects_free();
  anim_shutdown();
}

}  // namespace zeff
