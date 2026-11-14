// Probe for tests/differential/test_pm_camera_moved.py: Pirate's Moon's camera setters 0x00506ae0 / 0x00506cd0
// (the port's zcls::camera_set_rotation / camera_set_position with the table's camera_setters_mark_moved): a
// camera with children marks itself and everything below it moved (0x004ff6d0).
#include <cstdint>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zgame/zgame_table.h"
#include "pm/pm_game.h"

extern "C" {
// in: n nodes as (class, parent index, flags, +0x2c, object3d data word); node 0 is the camera (parent -1). which:
// 0 set rotation, 1 set position, 2 add rotation, 3 add position, 4 add a child (an object3d). out: each node's
// flags, +0x2c and data word after, then the added child's; returns the call's result.
int probe_pm_camera_moved(int n, const uint32_t* spec, int which, uint32_t* out) {
  zgame::set_game(pm::table());
  const zgame::Table& game = zgame::game();
  std::vector<zcls::Node*> nodes(static_cast<size_t>(n));
  std::vector<std::vector<zcls::Node*>> kids(static_cast<size_t>(n));
  zcls::Camera cam{};
  std::vector<zcls::Object3d> o3(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    const uint32_t* s = spec + 5 * i;
    zcls::Node* c = nodes[static_cast<size_t>(i)] = game.zcls_node_alloc();
    c->put<int32_t>(0x34, static_cast<int32_t>(s[0]));
    c->set_flags(s[2]);
    c->put<uint32_t>(0x2c, s[3]);
    if (s[0] == 1) c->data = &cam;
    if (s[0] == 5) o3[static_cast<size_t>(i)].put<uint32_t>(0, s[4]), c->data = &o3[static_cast<size_t>(i)];
    if (static_cast<int32_t>(s[1]) >= 0) kids[s[1]].push_back(c);
  }
  for (int i = 0; i < n; ++i) {
    const std::vector<zcls::Node*>& k = kids[static_cast<size_t>(i)];
    auto* arr = new zcls::Node*[k.size() + 1];  // node_link grows it with new[] / delete[]
    for (size_t j = 0; j < k.size(); ++j) arr[j] = k[j];
    nodes[static_cast<size_t>(i)]->children = arr;
    nodes[static_cast<size_t>(i)]->put<int32_t>(0x5c, static_cast<int32_t>(kids[static_cast<size_t>(i)].size()));
  }
  zcls::Node* added = game.zcls_node_alloc();
  zcls::Object3d added_data{};
  added_data.put<uint32_t>(0, 0);
  added->put<int32_t>(0x34, 5), added->data = &added_data;
  added->set_flags(0);
  int r = 0;
  switch (which) {
    case 0: r = zcls::camera_set_rotation(nodes[0], 0.1f, 0.2f, 0.3f); break;
    case 1: r = zcls::camera_set_position(nodes[0], 1.0f, 2.0f, 3.0f); break;
    case 2: r = zcls::camera_add_rotation(nodes[0], 0.1f, 0.2f, 0.3f); break;
    case 3: r = zcls::camera_add_position(nodes[0], 1.0f, 2.0f, 3.0f); break;
    default: r = zcls::camera_add_child(nodes[0], added); break;
  }
  for (int i = 0; i < n; ++i) {
    zcls::Node* c = nodes[static_cast<size_t>(i)];
    out[3 * i] = c->flags(), out[3 * i + 1] = c->get<uint32_t>(0x2c);
    out[3 * i + 2] = spec[5 * i] == 5 ? o3[static_cast<size_t>(i)].get<uint32_t>(0) : 0;
    delete[] c->children;
    c->children = nullptr, c->put<int32_t>(0x5c, 0), c->data = nullptr;
  }
  out[3 * n] = added->flags(), out[3 * n + 1] = added->get<uint32_t>(0x2c), out[3 * n + 2] = added_data.get<uint32_t>(0);
  delete[] added->parents;
  added->parents = nullptr, added->put<int32_t>(0x54, 0), added->data = nullptr;
  return r;
}
}  // extern "C"
