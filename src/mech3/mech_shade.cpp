#include "mech3/mech_shade.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

using zgame::Handle;

namespace {
ShadeGlobals g_shade;
ShadeHooks g_default_hooks;
ShadeHooks* g_hooks = &g_default_hooks;

constexpr float kLit = 1.0f;         // 0x005e9cdc
constexpr float kRate = 1.0f;        // 0x005e9ce0  levels a second
constexpr bool kSkipParts = true;    // 0x005e9ce4: the parts' own tests and fades never run
constexpr float kReach = -150.0f;    // 0x005e9ce8  along the light's direction: towards the sun
}  // namespace

ShadeGlobals& shade_globals() { return g_shade; }
void set_shade_hooks(ShadeHooks* h) { g_hooks = h ? h : &g_default_hooks; }
ShadeHooks& shade_hooks() { return *g_hooks; }

// Mech3.exe 1.2 @ 0x004e66a0: the level at the sun's diffuse (1 without a
// sun), the node's flags cleared.
void shade_level_reset(ShadeLevel& l, Handle node) {
  const float v = g_shade.sun ? g_hooks->sun_diffuse(g_shade.sun) : 1.0f;
  l.target = v;
  l.current = v;
  if (node) g_hooks->node_flags(node, 0);
}

// Mech3.exe 1.2 @ 0x004e6640 (and inline in 0x004e6480): the level a step
// towards its target, not past it.
void shade_level_fade(ShadeLevel& l, float dt) {
  if (l.current == l.target) return;
  const float step = kRate * dt;
  if (l.current > l.target) {
    l.current = l.current - step;
    if (l.current < l.target) l.current = l.target;
  } else {
    l.current = step + l.current;
    if (l.current > l.target) l.current = l.target;
  }
}

namespace {

// Mech3.exe 1.2 @ 0x004e6270: a ray from `at` 150 m towards the sun, the
// node itself left out: blocked, the target is 0, else 1. True when the
// level is not the sun's diffuse.
bool test(ShadeLevel& l, Handle node, const float at[3]) {
  bool shaded = false;
  if (node) {
    float dir[3];
    g_hooks->sun_direction(g_shade.sun, dir);
    const float far[3] = {dir[0] * kReach + at[0], dir[1] * kReach + at[1], dir[2] * kReach + at[2]};
    g_hooks->node_intersect(node, 0);
    g_hooks->query_mode(1);
    g_hooks->query_exclude(0x40000);
    uint8_t first = 0;
    const int32_t r = g_hooks->segment(at, far, &first);
    g_hooks->query_mode(0);
    g_hooks->node_intersect(node, 1);
    shaded = r == 0 && first != 0;
  }
  l.target = shaded ? 0.0f : kLit;
  return g_hooks->sun_diffuse(g_shade.sun) != l.current;
}

void set_flags(Handle node, uint8_t on) { g_hooks->node_flags(node, on); }  // 0x004e63c0

}  // namespace

// Mech3.exe 1.2 @ 0x004e61e0 (0x004e6250, 0x004e66a0(0)): the next turn of the round.
void shade_init(SunShade& s, Handle mech, Handle parts) {
  s.parts = parts;
  s.mech = mech;
  s.differs = 0;
  s.level = ShadeLevel{};
  shade_level_reset(s.level, 0);
  s.turn = g_shade.count++;
}

// Mech3.exe 1.2 @ 0x004e6480: a frame. The turn moves on once a frame; on
// this shade's turn its centroid's ray (0x004e6420) decides the target and
// whether its node is flagged; the level fades every frame.
void shade_update(SunShade& s, float dt) {
  if (!g_shade.sun) return;
  const int32_t frame = g_hooks->frame();
  if (g_shade.frame != frame) {
    if (++g_shade.turn == g_shade.count) g_shade.turn = 0;
    g_shade.frame = frame;
  }
  if (s.turn == g_shade.turn) {
    const Handle centroid = g_hooks->entity_centroid(s.mech);
    float at[3];
    g_hooks->node_world_cached(centroid, at);
    s.differs = test(s.level, g_hooks->entity_node(s.mech), at);
    // The parts' own rays (0x004e63f0) would follow; kSkipParts.
  }
  shade_level_fade(s.level, dt);
  set_flags(g_hooks->entity_node(s.mech), s.differs);
  static_assert(kSkipParts, "the parts' fades and flags (0x004e6640, 0x004e63c0) are not ported");
}

// Mech3.exe 1.2 @ 0x004e67c0: the mech's level and every part's back to
// the sun's, their nodes unflagged.
void shade_reset(SunShade& s) {
  shade_level_reset(s.level, g_hooks->entity_node(s.mech));
  if (!s.parts) return;
  const int32_t n = g_hooks->part_count(s);
  for (int32_t i = 0; i < n; ++i) {
    Handle node = 0;
    if (ShadeLevel* l = g_hooks->part(s, i, &node)) shade_level_reset(*l, node);
  }
}

// Mech3.exe 1.2 @ 0x004e6820: on with a sun, the scene walk's calls set and
// the round restarted; off, the calls cleared. Every mech's level reset.
uint8_t shade_enable(uint8_t on) {
  uint8_t r = on;
  if (on) {
    g_shade.sun = g_hooks->world_sun();
    r = g_shade.sun != 0;
  }
  if (r) {
    g_shade.on = 1;
    g_hooks->walk_callbacks(true);
    g_shade.frame = 0;
  } else {
    g_shade.on = 0;
    g_hooks->walk_callbacks(false);
    g_shade.frame = 0x7fffffff;
    g_shade.sun = 0;
  }
  const int32_t n = g_hooks->entity_count();
  for (int32_t i = 0; i < n; ++i)
    if (SunShade* s = g_hooks->entity_shade(i)) shade_reset(*s);
  return r;
}

void shade_apply() { shade_enable(g_shade.option); }  // 0x004e6900
void shade_off() { shade_enable(0); }                  // 0x004e6910

// Mech3.exe 1.2 @ 0x004e6920: kept; applied at once in a mission.
void shade_set_option(uint8_t on) {
  const float t = g_hooks->game_time();
  g_shade.option = on;
  if (t > 0.0f) g_shade.result = shade_enable(on);
}

// Mech3.exe 1.2 @ 0x004e6720: before a flagged node draws, the sun's
// diffuse is the node's owner's level (0x004e66f0); the light bake is off.
void shade_before(Handle node) {
  ShadeLevel* l = g_hooks->part_level(node);
  if (!l) l = g_hooks->mech_level(node);
  if (l) {
    l->saved = g_hooks->sun_diffuse(g_shade.sun);
    g_hooks->set_sun_diffuse(g_shade.sun, l->current);
  }
  g_hooks->static_lights(0);
  g_hooks->stamps_frozen(1);
}

// Mech3.exe 1.2 @ 0x004e6770: after it, the sun's own diffuse back (0x004e6710).
void shade_after(Handle node) {
  if (g_shade.sun) {
    ShadeLevel* l = g_hooks->part_level(node);
    if (!l) l = g_hooks->mech_level(node);
    if (l) g_hooks->set_sun_diffuse(g_shade.sun, l->saved);
  }
  g_hooks->static_lights(1);
  g_hooks->stamps_frozen(0);
}

}  // namespace mech3
