#include "mech3/mech_headlight.h"

#include <cstdint>
#include <cstring>

#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/light.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zrender/zrndr_fog.h"
#include "pm/gamez/zclass/light.h"

namespace mech3 {

namespace {
HeadlightHooks g_hooks;

void read_block(HeadlightConfig& c, zrdr::Node* b) {  // 0x004793c0, index 0
  if (!b) return;
  zrdr::Node* n;
  if ((n = zrdr::find(b, "offset"))) zrdr::get_vec3(n, c.light_offset);
  if ((n = zrdr::find(b, "color"))) zrdr::get_vec3(n, c.color);
  if ((n = zrdr::find(b, "range"))) zrdr::get_vec2(n, c.range);
  zrdr::get_float(b, "ambient", &c.ambient);
  zrdr::get_float(b, "diffuse", &c.diffuse);
  zrdr::get_byte_if(b, "subdivide", &c.subdivide);
}
}  // namespace

HeadlightHooks& headlight_hooks() { return g_hooks; }

// Pirate's Moon 1.0 @ 0x00479440: the `light0` block, the model's offset, and lodrange made the level's line:
// 1 at the near distance squared, 0 at the far.
void headlight_config_read(HeadlightConfig& c, zrdr::Node* block) {
  if (block) {
    read_block(c, zrdr::find(block, "light0"));
    if (zrdr::Node* n = zrdr::find(block, "model_offset")) zrdr::get_vec3(n, c.model_offset);
    float range[2] = {c.lod_scale, c.lod_base};
    if (zrdr::Node* n = zrdr::find(block, "lodrange"))
      if (zrdr::get_vec2(n, range)) c.lod_scale = range[0], c.lod_base = range[1];
  }
  const float far_sq = c.lod_base * c.lod_base;
  const float scale = -1.0f / (far_sq - c.lod_scale * c.lod_scale);  // 0x005deea8
  c.lod_scale = scale;
  c.lod_base = -(far_sq * scale);
}

// Pirate's Moon 1.0 @ 0x004795c0, with 0x00479780's light.
Headlight::Headlight(zcls::Node* parent_node, zcls::Node* beam_node, const HeadlightConfig* config, zcls::Node* world)
    : parent(parent_node), beam(beam_node), cfg(config) {
  node = zcls::object3d_create();
  zcls::node_set_name(node, "spotlight");
  fade_rate = 0.0f;
  light = zcls::light_create();
  zcls::node_set_name(light, "");
  set_active(true);
  zcls::node_set_name(light, "light0");
  zcls::node_set_active(light, 0);
  zcls::world_add_light(world, light);
  zcls::node_add_child(node, light);
  pm::zcls::light_set_position(light, cfg->light_offset[0], cfg->light_offset[1], cfg->light_offset[2]);
  pm::zcls::light_set_color(light, cfg->color[0], cfg->color[1], cfg->color[2]);
  pm::zcls::light_set_range(light, cfg->range[0], cfg->range[1]);
  pm::zcls::light_set_ambient(light, cfg->ambient);
  pm::zcls::light_set_diffuse(light, cfg->diffuse);
  pm::zcls::light_set_bit4(light);
  pm::zcls::light_set_ambient_only(light, 0);
  pm::zcls::light_set_diffuse_only(light, cfg->subdivide);
  turn(false, false);
  zcls::object3d_set_translation(beam, cfg->model_offset[0], cfg->model_offset[1], cfg->model_offset[2]);
  fade_rate = -5.0f;
}

// Pirate's Moon 1.0 @ 0x004796f0, with 0x004798d0(0): out of the parent, the light out of the world.
Headlight::~Headlight() {
  if (node->parent_count() == 1) zcls::node_remove_child(parent, node);
  parent = nullptr;
  if (g_hooks.world) zcls::world_remove_light(g_hooks.world(), light);
  zcls::tree_destroy(node);
  node = nullptr;
}

// Pirate's Moon 1.0 @ 0x00408900 (the class's vt+8).
bool Headlight::active() const { return node && ((node->flags() >> 2) & 1); }

// Pirate's Moon 1.0 @ 0x00479950
bool Headlight::set_active(bool active_on) {
  const int r = zcls::node_set_active(node, active_on);
  if (!active_on) {
    if (node->parent_count() == 1) zcls::node_remove_child(parent, node);
  } else if (node->parent_count() == 0) {
    zcls::node_add_child(parent, node);
  }
  return r == 0;
}

// Pirate's Moon 1.0 @ 0x004798d0 (its +0x18's vt+4 is the add under the parent, as set_active's; 0x00510300 is
// MW3's object3d_set_translation 0x004fc0b0, on the new parent).
void Headlight::reattach(zcls::Node* to, const float* offset) {
  if (node->parent_count() == 1) zcls::node_remove_child(parent, node);
  parent = to;
  if (!to || !active()) return;
  zcls::node_add_child(parent, node);
  static const float kNone[3] = {0, 0, 0};  // [0x0066f5e0]
  const float* o = offset ? offset : kNone;
  zcls::object3d_set_translation(parent, o[0], o[1], o[2]);
}

// Pirate's Moon 1.0 @ 0x004799b0
void Headlight::turn(bool turn_on, bool fading) {
  on = turn_on;
  target = turn_on ? 1.0f : 0.0f;
  if (!fading) set_level(target);
}

// Pirate's Moon 1.0 @ 0x00479a40: the beam in the tree while the level is not 0; the light scaled by level and lod.
void Headlight::set_level(float v) {
  const bool add = level == 0.0f && v != 0.0f;
  const bool drop = level != 0.0f && v == 0.0f;
  const float k = v * lod;
  level = v;
  const bool lit = level != 0.0f;
  if (add) zcls::node_add_child(node, beam);
  else if (drop) zcls::node_remove_child(node, beam);
  set_active(lit);
  zcls::node_set_active(light, k != 0.0f);
  pm::zcls::light_set_ambient(light, k * cfg->ambient);
  pm::zcls::light_set_diffuse(light, k * cfg->diffuse);
}

// Pirate's Moon 1.0 @ 0x004799f0: the level by the distance squared, only while on. The fade applies it.
void Headlight::set_lod(float distance_sq) {
  if (!on) return;
  lod = distance_sq * cfg->lod_scale + cfg->lod_base;
  if (lod > 1.0f) lod = 1.0f;
  else if (lod < 0.0f) lod = 0.0f;
}

// Pirate's Moon 1.0 @ 0x00479b50. e^(rate dt) by the exponent's bits: the truncated product of 2^23 / ln 2
// (0x005e00c8) added to 1.0's. Under 0.05 it is off; over 0.95, full.
void Headlight::fade(float dt) {
  if (dt == 0.0f || (!on && level == 0.0f)) return;
  float v = level;
  if (level != target) {
    const auto bits = static_cast<int32_t>(dt * fade_rate * 12102200.0f) + 0x3f800000;
    float e;
    std::memcpy(&e, &bits, 4);
    v = target - (target - level) * e;
    if (v < 0.05f) return set_level(0.0f);  // 0x005e01c8
    if (v > 0.95f) v = 1.0f;                // 0x005e5be8
  }
  set_level(v);
}

// Pirate's Moon 1.0 @ 0x00479c30
void Headlight::detach() {
  held_parent = nullptr;
  if (node->parent_count() == 1) {
    held_parent = node->parents[0];
    zcls::node_remove_child(held_parent, node);
  }
}

// Pirate's Moon 1.0 @ 0x00479c60
void Headlight::attach(float dt) {
  if (held_parent) {
    zcls::node_add_child(held_parent, node);
    held_parent = nullptr;
  }
  fade(dt);
}

void Headlight::show_beam(bool show) { zcls::node_set_active(beam, show); }

// Pirate's Moon 1.0 @ 0x00479510
Headlight* headlight_create(zcls::Node* parent, const HeadlightConfig* cfg) {
  if (!zrndr::device_fog().active || !g_hooks.load_beam || !g_hooks.world) return nullptr;
  zcls::Node* beam = g_hooks.load_beam();
  return new Headlight(parent, beam, cfg, g_hooks.world());
}

}  // namespace mech3
