#include "gamez/zweapon/zwep_damage.h"

#include <new>

namespace zwep {

namespace {
// 0x00583270 (ECX record, EDX old, stack owner, node, callback, slot)
void fill(DamageRecord* rec, DamageRecord* old, void* owner, zcls::Node* node, void* callback, int slot) {
  rec->slot[slot].owner = owner;
  rec->slot[slot].callback = callback;
  spread_record(node, rec, old);
}
}  // namespace

DamageRecord* record_of(const zcls::Node* node) { return static_cast<DamageRecord*>(node->damage); }

// Mech3.exe 1.2 @ 0x00583220. A null parent ends the walk early (not owned).
int owns_record(const zcls::Node* node, const DamageRecord* rec) {
  int32_t i = 0;
  for (; i < node->parent_count(); ++i) {
    const zcls::Node* p = node->parents[i];
    if (!p || record_of(p) == rec) break;
  }
  return i >= node->parent_count() ? 1 : 0;
}

// Mech3.exe 1.2 @ 0x005832a0. Children first, then the node.
void spread_record(zcls::Node* node, DamageRecord* rec, DamageRecord* old) {
  if (record_of(node) != old) return;
  for (int32_t i = 0; i < node->child_count(); ++i) spread_record(node->children[i], rec, old);
  node->damage = rec;
}

// Mech3.exe 1.2 @ 0x005832f0. FIX (A): new for calloc.
int register_damage(void* owner, zcls::Node* node, void* callback, int slot) {
  DamageRecord* rec = record_of(node);
  if (!rec) {
    auto* fresh = new (std::nothrow) DamageRecord{};
    if (!fresh) return 0;
    fill(fresh, nullptr, owner, node, callback, slot);
    return 1;
  }
  if (owns_record(node, rec)) {
    if (rec->slot[slot].callback) return 0;
    rec->slot[slot].owner = owner;
    rec->slot[slot].callback = callback;
    return 1;
  }
  // Inherited from a parent: the subtree gets a record of its own with only
  // this slot; the parent's other slot no longer reaches it.
  auto* fresh = new (std::nothrow) DamageRecord{};
  if (!fresh) return 0;
  fill(fresh, rec, owner, node, callback, slot);
  return 1;
}

// Mech3.exe 1.2 @ 0x005833b0. The proximity flag marks the node (0x40) for
// near misses.
int register_weapon_hit(void* owner, zcls::Node* node, int proximity, void* callback) {
  const int r = register_damage(owner, node, callback, kWeaponHit);
  if (r && proximity) zcls::node_set_proximity(node, 1);
  return r;
}

// Mech3.exe 1.2 @ 0x005833f0
int register_collide(void* owner, zcls::Node* node, void* callback) {
  return register_damage(owner, node, callback, kCollide);
}

}  // namespace zwep
