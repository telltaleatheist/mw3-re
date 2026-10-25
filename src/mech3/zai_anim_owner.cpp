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
bool anim_owner_claim(Handle owner, Handle anim) {
  AnimOwnerHooks& h = H();
  if (!anim) return false;  // the exe reads +0x70 of it; no caller passes none
  AnimOwnerRec temp;  // 0x004d46f0
  AnimOwnerRec* rec = nullptr;
  AnimOwnerRec* arg = h.anim_record(anim);  // 0x00508f30
  const AnimCallback cb = h.anim_callback(anim);
  if (cb == AnimCallback::none) {
    rec = &temp;
  } else if (cb == AnimCallback::ours) {
    const auto& list = anim_owner_records();
    rec = arg && std::find(list.begin(), list.end(), arg) != list.end() ? arg : &temp;
  }
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
