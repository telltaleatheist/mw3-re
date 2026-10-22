#include "gamez/zentity/zent_init.h"

#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/object3d.h"

namespace zent {

namespace {
// 0x004eb0d0 (by name in the tree), else 0x004fb740, 0x004eb300 and
// 0x004eb9b0; one that cannot be added is deleted (0x004eaea0).
zcls::Node* find_or_make(zcls::Node* node, const char* name) {
  if (zcls::Node* f = zcls::node_find_in_tree(node, name)) return f;
  zcls::Node* made = zcls::object3d_create();
  if (!made) return nullptr;
  zcls::node_set_name(made, name);
  if (zcls::node_add_child(node, made) != 0) {
    zcls::node_delete(made);
    return nullptr;
  }
  return made;
}
}  // namespace

// Mech3.exe 1.2 @ 0x005a8390 (the nodes; the matrix +0x0c, the name
// 0x005a84c0 and the refresh 0x005a85c0 are the caller's).
void marker_nodes(zcls::Node* node, const char* centroid, const char* target, zcls::Node** centroid_out,
                  zcls::Node** target_out) {
  *centroid_out = *target_out = nullptr;
  if (!node) return;
  *centroid_out = find_or_make(node, centroid ? centroid : "centroid");
  *target_out = find_or_make(node, target ? target : "target");
}

}  // namespace zent
