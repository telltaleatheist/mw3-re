#include "gamez/zdeclient/zdec_init.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "crt/msvc_rand.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zreader/zreader.h"

namespace zdec {
namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zDEClient\\zdec_init.cpp";
State g_state;
Hooks g_hooks;

zimg::TextureEntry* find_texture(const char* n) {
  return g_hooks.find_texture ? g_hooks.find_texture(n) : zimg::find_texture(n);
}
zimg::TextureEntry* add_texture(const char* n) {
  return g_hooks.add_texture ? g_hooks.add_texture(n) : zimg::add_texture(n);
}
zeff::AnimDef* anim_find(const char* n) { return g_hooks.anim_find ? g_hooks.anim_find(n) : zeff::anim_find(n); }

// Mech3.exe 1.2 @ 0x00501fa0. The texture's material: a loaded texture's
// existing one, else a new one; flag 8 either way. 1 when the texture had to
// be added (the caller then loads the new ones).
int material_for(zmod::Material** out, const char* name) {
  if (!name) return 0;
  int added = 0;
  zmod::Material* m = nullptr;
  zimg::TextureEntry* t = find_texture(name);
  if (t) {
    m = zmod::material_find_texture(t);
  } else {
    t = add_texture(name);
    added = 1;
  }
  if (!m) {
    zmod::Material made{};
    zmod::material_init(made);
    made.flags |= 1;
    made.texture = t;
    m = zmod::material_add(made);
  }
  if (m) m->flags |= 8;
  *out = m;
  return added;
}

const zrdr::Node& item(const zrdr::Node* n, int i) { return n->list[i]; }
}  // namespace

State& state() { return g_state; }
Hooks& hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x00501f90
void set_mode(uint32_t v) { g_state.restore_on = v; }

// Mech3.exe 1.2 @ 0x00502070
void set_world(zcls::Node* node) {
  if (!node) return;
  g_state.world_node = node;
  g_state.world = node->data;
}

// Mech3.exe 1.2 @ 0x00501ab0 (ECX the world node). Every key falls back to
// the built-in value; TEXTURE_PARMS is never read. docs/craters.md section 1.
int32_t init(zcls::Node* world) {
  State& s = g_state;
  if (!world) {
    zerr::report(0x200, kFile, 0x42, "Failed to DEClient: world node is NULL.");
    return -1;
  }
  set_world(world);
  // 0x004d72a0 (returns 0) is called here.
  zrdr::Node* root = g_hooks.load ? g_hooks.load("declient.zrd") : zrdr::load("declient.zrd");
  crt::msvc_srand(static_cast<uint32_t>(std::time(nullptr)));
  if (!root) zerr::report(0x100, kFile, 0x53, "Failed to read (declient.zrd), using defaults");

  zrdr::Node* crater = zrdr::find(root, "CRATER");
  zwep::CraterOrder& c = s.crater;
  c.x00 = 0x100c, c.points = 7, c.x0c = 0, c.depth = 4.0f, c.radius = 20.0f;
  zrdr::get_int(crater, "POINTS", &c.points);
  float slope = 0;
  std::memcpy(&slope, &c.x0c, 4);
  zrdr::get_float(crater, "SLOPE", &slope);
  std::memcpy(&c.x0c, &slope, 4);
  zrdr::get_float(crater, "DEPTH", &c.depth);
  zrdr::get_float(crater, "RADIUS", &c.radius);

  int loaded = 0;
  // The exe leaks the last mission's table.
  std::free(s.textures);
  s.texture_count = 1;
  s.textures = static_cast<CraterTexture*>(std::calloc(1, sizeof(CraterTexture)));
  if (material_for(&s.textures[0].crater, zrdr::get_string(crater, "DEFAULT_TEXTURE"))) loaded = 1;
  s.textures[0].anim = anim_find(zrdr::get_string(crater, "DEFAULT_ANIM"));
  if (zrdr::Node* pairs = zrdr::find(crater, "TEXTURE_ANIM")) {
    const int32_t n = zrdr::list_count(pairs);
    s.texture_count += (n - 1) / 2;
    s.textures = static_cast<CraterTexture*>(std::realloc(s.textures, s.texture_count * sizeof(CraterTexture)));
    for (int32_t k = 1; k < s.texture_count; ++k) s.textures[k] = CraterTexture{};  // the exe leaves them as realloc does
    CraterTexture* e = s.textures + 1;
    for (int32_t i = 1; i < n; i += 2, ++e) {
      const char* ground = item(pairs, i).s;
      if (material_for(&e->ground, ground)) loaded = 1;
      // A last key without a value finds the slot past the list (zReader's
      // spare, FIX (A) in fixes.md): no value.
      zrdr::Node* v = zrdr::find(pairs, ground);
      if (v && v->type == zrdr::List) {
        if (material_for(&e->crater, item(v, 1).s)) loaded = 1;
        e->anim = zrdr::list_count(v) > 2 ? anim_find(item(v, 2).s) : s.textures[0].anim;
      }
    }
  }

  zrdr::Node* qs = zrdr::find(root, "QUICK_SAND");
  if (!qs) {
    s.quicksand_on = 0;
  } else {
    int32_t frames = 1;
    if (zrdr::Node* tex = zrdr::find(qs, "DEFAULT_TEXTURE")) {
      std::memcpy(&s.frame_rate, &item(tex, 1).f, 4);
      frames = zrdr::list_count(tex) - 2;
      s.frame_count = frames;
      if (frames < 1) {
        s.frames = nullptr;
      } else {
        s.frames = static_cast<const char**>(std::malloc(frames * sizeof(char*)));
        for (int32_t i = 0; i < frames; ++i) s.frames[i] = item(tex, 2 + i).s;
      }
    } else {
      // FIX (bug): without DEFAULT_TEXTURE the exe takes one frame from the
      // last mission's (freed) name list, or crashes; untextured here.
      frames = 0;
    }
    zwep::QuicksandOrder& q = s.quicksand;
    q.x00 = 0x1008, q.points = 7, q.depth = 4.0f, q.radius = 20.0f;
    float qslope = 40.0f;
    zrdr::get_int(qs, "POINTS", &q.points);
    zrdr::get_float(qs, "SLOPE", &qslope);
    std::memcpy(&q.x08[2], &qslope, 4);
    zrdr::get_float(qs, "DEPTH", &q.depth);
    zrdr::get_float(qs, "RADIUS", &q.radius);
    s.quicksand_cycle = nullptr, s.quicksand_still = nullptr;
    zmod::Material m{};
    zmod::material_init(m);
    if (frames < 1) {
      m.flags &= ~1;
      zerr::report(0x100, kFile, 0xec, "Quick sand will NOT be textured");
    } else {
      s.quicksand_cycle = zmod::material_copy(m);
      s.quicksand_cycle->flags |= 1;
      if (frames > 1) {
        zmod::cycle_create(*s.quicksand_cycle, frames);
        zmod::cycle_set_speed(s.quicksand_cycle, s.frame_rate);
        zmod::cycle_set_loop(s.quicksand_cycle, 1);
        for (int32_t i = 0; i < frames; ++i) zmod::cycle_add(*s.quicksand_cycle, add_texture(s.frames[i]));
      } else {
        // FIX (bug): one frame: the exe adds the name past the list's end
        // (frames[1]) and drops the texture; the one frame, shown.
        s.quicksand_cycle->texture = add_texture(s.frames[0]);
      }
      loaded = 1;
    }
    zmod::material_set_soil(s.quicksand_cycle, 3);
    if (frames >= 1) {
      zmod::Material still{};
      zmod::material_init(still);
      s.quicksand_still = zmod::material_copy(still);
      s.quicksand_still->flags |= 1;
      s.quicksand_still->texture = find_texture(s.frames[0]);
      zmod::material_set_soil(s.quicksand_still, 0);
    }
    s.quicksand_on = 1;
  }
  if (loaded) {
    if (g_hooks.load_textures) g_hooks.load_textures();
    else zimg::load_group_0();
  }
  zrdr::free_node(root);
  s.frames = nullptr;  // FIX (A): the names lived in the freed tree; the exe keeps the dangling list
  if (s.restore_on && g_hooks.register_restore) g_hooks.register_restore();
  return 0;
}

// Mech3.exe 1.2 @ 0x004ff920
void crater_defaults(zwep::CraterOrder* out) { *out = g_state.crater; }

// Mech3.exe 1.2 @ 0x005020f0
void quicksand_defaults(zwep::QuicksandOrder* out) { *out = g_state.quicksand; }

}  // namespace zdec
