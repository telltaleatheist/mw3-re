#include "mech3/zai_anim_owner.h"

#include <algorithm>

namespace mech3::ai {
namespace {
AnimOwnerHooks g_default;
AnimOwnerHooks* g_hooks = &g_default;
AnimOwnerHooks& H() { return *g_hooks; }
}  // namespace

void set_anim_owner_hooks(AnimOwnerHooks* h) { g_hooks = h ? h : &g_default; }
AnimOwnerHooks& anim_owner_hooks() { return *g_hooks; }

std::list<AnimOwnerRec*>& anim_owner_records() {
  static std::list<AnimOwnerRec*> records;  // [0x0060b51c]
  return records;
}

// Mech3.exe 1.2 @ 0x004d4700.
void anim_owner_mission_end() {
  for (AnimOwnerRec* r : anim_owner_records()) r->valve = 0, r->owner = 0;
}

// Mech3.exe 1.2 @ 0x004d4740.
void anim_owner_free() {
  for (AnimOwnerRec* r : anim_owner_records()) delete r;
  anim_owner_records().clear();
}

// Mech3.exe 1.2 @ 0x004d4f20. A record of our own callback is the
// animation's only while the list still holds it; else (none yet, or a
// stale one) a new one is made and listed. Another callback refuses.
namespace {
// The record the claim and the binder work on (0x004d4f20 and 0x004d4c80
// alike): `temp` with no callback or ours on a record no list holds; ours
// listed; none for another's.
AnimOwnerRec* record_for(Handle anim, AnimOwnerRec* temp) {
  AnimOwnerHooks& h = H();
  AnimOwnerRec* arg = h.anim_record(anim);  // 0x00508f30
  const AnimCallback cb = h.anim_callback(anim);
  if (cb == AnimCallback::none) return temp;
  if (cb != AnimCallback::ours) return nullptr;
  const auto& list = anim_owner_records();
  return arg && std::find(list.begin(), list.end(), arg) != list.end() ? arg : temp;
}
}  // namespace

bool anim_owner_claim(Handle owner, Handle anim) {
  AnimOwnerHooks& h = H();
  if (!anim) return false;  // the exe reads +0x70 of it; no caller passes none
  AnimOwnerRec temp;  // 0x004d46f0
  AnimOwnerRec* rec = record_for(anim, &temp);
  bool claimed = false;
  if (rec && (rec->owner == 0 || rec->owner == owner)) {
    rec->owner = owner;
    claimed = true;
  }
  if (!rec || !claimed) return false;
  if (rec == &temp) {
    rec = new AnimOwnerRec(temp);
    anim_owner_records().push_back(rec);  // 0x0045b800
  }
  h.anim_set_record(anim, rec);  // 0x00508f00 with 0x004d4b10
  return true;
}

// Mech3.exe 1.2 @ 0x004d4b30. The last def's binding is committed inline as
// 0x004d4e50 would (0x0058f2a0, 0x004d5110: the list's insert).
void anim_owner_bind_all() {
  AnimOwnerHooks& h = H();
  AnimBinding b;
  std::vector<int32_t> values;
  for (Handle a = h.anims_next(0); a; a = h.anims_next(a)) {
    values.clear();
    h.anim_callback_values(a, values);  // 0x00508f40 with 0x004d4c80
    for (int32_t v : values) anim_owner_bind_event(b, a, v);
  }
  anim_owner_bind_commit(b);
}

// Mech3.exe 1.2 @ 0x004d4c80. Another def first commits the one before. A
// valve message walks (value - 0x2a) / 2 parents up from the def's node, each
// the only parent; past the root (a node with none, or several) nothing.
void anim_owner_bind_event(AnimBinding& b, Handle anim, int32_t value) {
  AnimOwnerHooks& h = H();
  if (b.anim != anim) anim_owner_bind_commit(b);
  if (!anim) return;
  if (!b.valid) {
    b.anim = anim, b.valid = true, b.taken = false;
    b.rec = record_for(anim, &b.temp);
  }
  if (!b.rec) return;
  switch (value) {
    case 0:
    case 0x38: {
      const Handle e = h.node_entity(h.anim_node(b.anim));
      if (!e || (b.rec->owner && b.rec->owner != e)) return;
      b.taken = true;
      b.rec->owner = e;
      break;
    }
    case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f: case 0x30: case 0x31: {
      int32_t up = (value - 0x2a) >> 1;
      Handle node = h.anim_node(b.anim);
      // FIX (bug): the exe reads a missing node's parent count (no shipped def).
      while (up > 0 && node) {
        const Handle p = h.node_parent(node);
        if (!p) break;
        --up, node = p;
      }
      if (up != 0) return;
      const Handle valve = h.valve_of_node(node);
      if (!valve || (b.rec->valve && b.rec->valve != valve)) return;
      b.taken = true;
      b.rec->valve = valve;
      break;
    }
    case 0x39: case 0x3a: case 0x3b: case 0x3c:
      b.taken = true;
      break;
    default:
      break;
  }
}

// Mech3.exe 1.2 @ 0x004d4e50.
bool anim_owner_bind_commit(AnimBinding& b) {
  const bool ok = b.valid && b.anim && b.rec && b.taken;
  if (ok) {
    if (b.rec == &b.temp) {
      b.rec = new AnimOwnerRec(b.temp);
      anim_owner_records().push_back(b.rec);
    }
    H().anim_set_record(b.anim, b.rec);  // 0x00508f00 with 0x004d4b10
  }
  b = AnimBinding{};
  return ok;
}

// Mech3.exe 1.2 @ 0x004d5100 -> 0x004d47e0 (the registry [0x0060b518]).
// The exe also tests the valve against 0 and stores 0 there: nothing.
void anim_owner_release(Handle owner) {
  for (AnimOwnerRec* r : anim_owner_records())
    if (r->owner == owner) r->owner = 0;
}

// Mech3.exe 1.2 @ 0x004d4b10 -> 0x004d4a20. The owner hears every message
// (vt+0x74); the valve is read after it, as the exe does.
void anim_owner_message(Handle anim, AnimOwnerRec* rec, int32_t msg) {
  AnimOwnerHooks& h = H();
  if (!rec) return;
  if (rec->owner) h.entity_message(rec->owner, anim, msg);
  switch (msg) {
    case 0x11:
      anim_owner_via_node(h.player_vehicle(), true);
      break;
    case 0x2a: case 0x2c: case 0x2e: case 0x30:  // the four valve levels, off
    case 0x2b: case 0x2d: case 0x2f: case 0x31:  // and on
      if (rec->valve) h.valve_set(rec->valve, msg & 1);
      break;
    case 0x39: case 0x3a: case 0x3b: case 0x3c:
      h.inout_message(anim, msg);
      break;
    default:
      break;
  }
}

// Mech3.exe 1.2 @ 0x004d4980 (ECX the vehicle, EDX `relative`).
// FIX (bug): with no "viaNode" (no shipped world has one) the exe places the
// vehicle at its stack's leftovers; and with no vehicle it would crash.
void anim_owner_via_node(Handle vehicle, bool relative) {
  AnimOwnerHooks& h = H();
  const Handle node = h.node_named("viaNode");
  if (!node || !vehicle) return;
  float pos[3], rot[3];
  h.node_translation(node, pos);
  h.node_rotation(node, rot);
  if (relative) {
    const float* p = h.entity_position(vehicle);
    for (int i = 0; i < 3; ++i) pos[i] = p[i] + pos[i];
  }
  h.place(vehicle, pos, rot[1]);
}

}  // namespace mech3::ai
