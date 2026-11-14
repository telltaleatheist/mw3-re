// Probe for tests/differential/test_pm_scene_children.py: Pirate's Moon's scene render 0x0050a550 (the port's
// zcls::render_world_children with the table's world_children_by_cells): which of the world's children it draws.
#include <cstdint>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zgame/zgame_table.h"
#include "pm/pm_game.h"

namespace {
std::vector<uint32_t> g_drawn;
}  // namespace

extern "C" {
// in: n children as (flags, x0, z0, x1, z1), the picked range (x0, z0, x1, z1). out: the drawn children's indices.
int probe_pm_scene_children(int n, const int32_t* kids, const int32_t* picked, uint32_t* out) {
  zgame::set_game(pm::table());
  const zgame::Table& game = zgame::game();
  zcls::Node* world = game.zcls_node_alloc();
  std::vector<zcls::Node*> nodes(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    zcls::Node* c = nodes[static_cast<size_t>(i)] = game.zcls_node_alloc();
    const int32_t* k = kids + 5 * i;
    c->put<int32_t>(0x34, 11);                         // a switch, inactive: drawn as nothing
    c->set_flags(static_cast<uint32_t>(k[0]) & ~4u);
    c->put<uint32_t>(0x2c, 0x20);                       // the pre hook logs it
    c->raw[0x30] = 0xff;
    c->put<int32_t>(0x4c, static_cast<int16_t>(k[1])), c->put<int32_t>(0x50, static_cast<int16_t>(k[2]));
    c->pm_virtual[0] = static_cast<int16_t>(k[3]), c->pm_virtual[1] = static_cast<int16_t>(k[4]);
    c->put<int32_t>(0x7c, i);
  }
  world->children = nodes.data();
  world->put<int32_t>(0x5c, n);
  zcls::SceneWalk& w = zcls::scene_walk();
  for (int i = 0; i < 4; ++i) w.picked[i] = picked[i];
  void (*const was)(zcls::Node*) = w.pre;
  g_drawn.clear();
  w.pre = [](zcls::Node* node) { g_drawn.push_back(static_cast<uint32_t>(node->get<int32_t>(0x7c))); };
  zcls::render_world_children(world);
  w.pre = was;
  world->children = nullptr;
  world->put<int32_t>(0x5c, 0);
  int k = 0;
  for (uint32_t v : g_drawn) out[k++] = v;
  return k;
}
}  // extern "C"
