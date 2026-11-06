// PM's models where MW3's code meets them: a node holds pm_model, not model.
#include <cstring>

#include "check.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zmodel/gmod_const.h"
#include "mech3/mech_build.h"
#include "mech3/veh_controls.h"
#include "pm/gamez/zmodel/gmod_const.h"
#include "pm/pm_game.h"

namespace {
pm::zmod::Model* pm_model(int32_t polys) {
  pm::zmod::Model* m = pm::zmod::model_alloc();
  std::memcpy(m->header + 0x10, &polys, 4);
  m->polygons = new pm::zmod::Polygon[static_cast<size_t>(polys)]{};
  return m;
}
}  // namespace

// ModelNew makes a model of MW3's type (none in PM's scripts); get_model gives a PM one. The setters go by pool.
TEST(pm_script_setters_go_by_pool) {
  zmod::Polygon mw3_polys[2] = {};
  zmod::Model mw3{};
  const uint32_t two = 2;
  std::memcpy(mw3.header + 0x10, &two, 4);
  mw3.polygons = mw3_polys;
  pm::zmod::Model* pm_one = pm_model(2);
  auto& e = zint::engine();
  e.model_set_priority(&mw3, 7);
  EXPECT(mw3_polys[1].priority == 7 && pm_one->polygons[1].priority == 0);
  e.model_set_priority(reinterpret_cast<zmod::Model*>(pm_one), 9);
  EXPECT(pm_one->polygons[1].priority == 9 && mw3_polys[1].priority == 7);
  mw3.polygons = nullptr;
  pm::zmod::model_release(pm_one);
}

// 0x004b0230 takes the marker's model off fpnt_c and frees it; the pool has the slot back.
TEST(pm_fpnt_c_marker_goes) {
  zcls::Node* node = zcls::object3d_create();
  pm::zmod::Model* m = pm_model(1);
  const int32_t count = pm::zmod::models().count;
  zcls::node_set_pm_model(node, m);
  mech3::VehControlsHooks h;
  EXPECT(h.node_model(node) == m);
  h.node_set_model(node, nullptr);
  h.model_release(m);
  EXPECT(!node->pm_model && !node->model);
  EXPECT(pm::zmod::models().count == count - 1);
  zcls::node_release(node);
}

// A shadow's priority (mech_part_build) goes to PM's model; a fade and the anim save go by the header.
TEST(pm_node_model_reads) {
  zcls::Node* node = zcls::object3d_create();
  pm::zmod::Model* m = pm_model(1);
  zcls::node_set_pm_model(node, m);
  mech3::MechBuildHooks h;
  EXPECT(h.node_model(node) == m);
  zgame::set_game(pm::table());
  EXPECT(zcls::node_model_header(node) == m->header);
  zgame::set_game(zgame::mw3_table());
  zcls::node_set_pm_model(node, nullptr);
  pm::zmod::model_release(m);
  zcls::node_release(node);
}
