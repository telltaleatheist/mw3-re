#pragma once
#include <cstdint>

#include "gamez/zclass/cls_node.h"

// The nodes' damage callbacks (docs/anim-postload.md, damage records). The
// original file is unknown: these sit between zvideo\zvid_ddd3d.c and
// zweapon\zwep_init.c, next to the weapon hit code that calls them
// (0x00583480, 0x005836b0, not ported), so the file is named for that.
namespace zwep {

// A node's +0xbc: who to call when a weapon hits it (slot 0) or something
// rams it (slot 1). One record is shared by a node and the descendants that
// had the same one.
struct DamageRecord {              // 0x10 bytes, calloc'd
  struct Slot {
    void* owner = nullptr;         // an animation def (zeff), passed back as ECX
    void* callback = nullptr;      // 0x00502e60 / 0x00502df0 for animations
  } slot[2];
};
enum DamageSlot { kWeaponHit = 0, kCollide = 1 };

DamageRecord* record_of(const zcls::Node* node);
// 1 when every parent exists and none has `rec`: the node holds its own.
int owns_record(const zcls::Node* node, const DamageRecord* rec);   // 0x00583220
// Gives `rec` to the node and the descendants still on `old`.
void spread_record(zcls::Node* node, DamageRecord* rec, DamageRecord* old);  // 0x005832a0
// 1 when set. A slot already taken on the node's own record is left (0); an
// inherited record is replaced by a new one holding only this slot.
int register_damage(void* owner, zcls::Node* node, void* callback, int slot);  // 0x005832f0
int register_weapon_hit(void* owner, zcls::Node* node, int proximity, void* callback);  // 0x005833b0
int register_collide(void* owner, zcls::Node* node, void* callback);  // 0x005833f0

// The weapon code's hit globals the callbacks set.
struct HitState {
  int32_t killed = 0;              // 0x009a1788  0x00583710 ECX
  void* last = nullptr;            // 0x009a1790  0x00583710 EDX (a hit record), when its +0x24 is set
  float fraction = 0.0f;           // 0x009a178c  0x00583730, health left over max (the hit code sets 1 first)
};
HitState& hit_state();
void set_killed(int32_t killed, void* hit);  // 0x00583710
void set_fraction(float f);                         // 0x00583730

}  // namespace zwep
