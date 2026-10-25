#include "mech3/entity_bind.h"

#include <cstdint>

#include "gamez/zclass/cls_node.h"

namespace mech3 {

namespace {
uint32_t id_of(const zcls::Node* n) { return n->get<uint32_t>(0x28); }
bool bit(const zcls::Node* n, int b) { return (id_of(n) >> b & 1) != 0; }
// 0x0040e9d0: a vehicle class's node (+0x28 0xf000) its owner, a part's
// (low byte) the part's mech.
void* vehicle_class_owner(zcls::Node* n, const UnbindCalls& c) {
  if (id_of(n) & 0xf000) return n->owner;
  if (id_of(n) & 0xff) return c.part_mech ? c.part_mech(n) : nullptr;
  return nullptr;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0040eaa0
void entity_bind(zcls::Node* node, void* self, const EntityCasts& c) {
  if (c.turret) return zcls::node_tree_bind(node, c.turret, 0x20000, 0);  // 0x0040eb10
  if (c.vehicle_base) {                                                  // 0x0040eb30
    if (c.mech) return zcls::node_tree_bind(node, c.mech, 0x1000, 0);
    if (c.vehicle) return zcls::node_tree_bind(node, c.vehicle, 0x80000, 0);
    return zcls::node_tree_bind(node, c.vehicle_base, 0x40000, 0);
  }
  if (c.target_base) {  // 0x0040eb80
    if (c.trap) return zcls::node_tree_bind(node, c.trap, 0x400000, 0);
    if (c.target) return zcls::node_tree_bind(node, c.target, 0x200000, 0);
    return zcls::node_tree_bind(node, c.target_base, 0x100000, 0);
  }
  zcls::node_tree_bind(node, self, 0x10000, 0);
}

// Mech3.exe 1.2 @ 0x0040ed00
void entity_unbind(zcls::Node* node, void* self, const UnbindCalls& c) {
  if (!node) return;
  if (bit(node, 17) && node->owner) return zcls::node_tree_unbind(node, node->owner, 0x20000, 0);  // 0x0040e990
  // 0x0040e960: bit 18's owner (null or not), else a vehicle class's
  // (0x0040e9d0), else bit 19's (0x0040e9b0).
  void* o = nullptr;
  if (bit(node, 18)) o = node->owner;
  else if (!(o = vehicle_class_owner(node, c)) && bit(node, 19)) o = node->owner;
  if (o) {  // 0x0040ed80
    if (void* m = vehicle_class_owner(node, c)) return zcls::node_tree_unbind(node, m, 0x1000, 0);
    if (bit(node, 19) && node->owner) return zcls::node_tree_unbind(node, node->owner, 0x80000, 0);
    return zcls::node_tree_unbind(node, o, 0x40000, 0);
  }
  // 0x0040e8f0: bit 20's owner (null or not), else bit 21's (0x0040ee50),
  // else bit 22's (0x0040eeb0).
  o = nullptr;
  if (bit(node, 20)) o = node->owner;
  else if (bit(node, 21)) o = node->owner;
  if (!o && !bit(node, 20) && bit(node, 22)) o = node->owner;
  if (o) {  // 0x0040eca0
    if (void* t = c.trap_of ? c.trap_of(o) : nullptr) return zcls::node_tree_unbind(node, t, 0x400000, 0);
    if (void* t = c.target_of ? c.target_of(o) : nullptr) return zcls::node_tree_unbind(node, t, 0x200000, 0);
    return zcls::node_tree_unbind(node, o, 0x100000, 0);
  }
  zcls::node_tree_unbind(node, self, 0x10000, 0);  // 0x0040ec20
}

}  // namespace mech3
