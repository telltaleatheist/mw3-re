#pragma once
// zEntity3d's node binding (0x0040e8e0..0x0040eeb0; docs/vehicles.md, The
// node binding): a game object's node tree is told which object it is (+0x28
// the kind bit, +0x40 the object), so a hit, a pick or a sight line on any of
// its nodes finds it again (0x0040e920). Original file unknown: in the exe's
// first module, between mech3\briefing.cpp and mech3\mech.cpp.

namespace zcls { struct Node; }

namespace mech3 {

// What an entity answers to the casts 0x0040eaa0 asks, each null when it is
// not that: the object found is the owner the nodes get.
struct EntityCasts {
  void* turret = nullptr;        // vt+0x48: bit 17 (0x0040eb10)
  void* vehicle_base = nullptr;  // vt+0x40; on it (0x0040eb30), else bit 18:
  void* mech = nullptr;          //   vt+0x50: 0x1000 (0x0040ec00)
  void* vehicle = nullptr;       //   vt+0xec: bit 19 (0x0040ebe0)
  void* target_base = nullptr;   // vt+0x60; on it (0x0040eb80), else bit 20:
  void* trap = nullptr;          //   vt+0xe4: bit 22 (0x0040ee70)
  void* target = nullptr;        //   vt+0xdc: bit 21 (0x0040ee10)
};
// Mech3.exe 1.2 @ 0x0040eaa0: the first cast that answers names the kind;
// none, `self` with bit 16.
void entity_bind(zcls::Node* node, void* self, const EntityCasts& casts);

// What 0x0040ed00 asks of the objects it finds on the node.
struct UnbindCalls {
  void* (*part_mech)(zcls::Node* node) = nullptr;  // 0x00460000: a part node's mech (+0x28 low byte)
  void* (*trap_of)(void* object) = nullptr;        // vt+0xe4
  void* (*target_of)(void* object) = nullptr;      // vt+0xdc
};
// Mech3.exe 1.2 @ 0x0040ed00: the binding taken off, by the node's own bits:
// the tree's nodes whose owner is the object found lose the kind bit and the
// owner (0x0040ec20); nothing found, `self` with bit 16.
void entity_unbind(zcls::Node* node, void* self, const UnbindCalls& calls);

}  // namespace mech3
