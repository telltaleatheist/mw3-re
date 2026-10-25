#pragma once
#include <cstdint>
#include <list>
#include <vector>

#include "gamez/zgame/unported.h"

// The animations' owner registry (docs/ai.md section 18): an animation whose
// callback (+0x6c) is the AI's 0x004d4b10 carries a record (+0x70) naming the
// entity that owns it and a valve, and the callback events it plays (event 35)
// reach them through the AI's message handler 0x004d4a20. The original file
// is not known: these sit after ZAiTarget.cpp's valves (0x004d3bf0..) and
// before zrdrainet.cpp, so likely ZAiTarget.cpp itself.
namespace mech3::ai {

using zgame::Handle;

// new(8): the owner (+0x00) and the valve (+0x04) set by the mission load's
// binder 0x004d4b30.
struct AnimOwnerRec {
  Handle owner = 0;
  Handle valve = 0;
};

// The list [0x0060b51c] (head [0x0060b520], count [0x0060b524]) of the
// registry [0x0060b518] (vtable 0x005ccd90), one of the mission listeners
// (0x004e78d0 from its constructor 0x004d48f0): emptied at the mission's end,
// freed with the world.
std::list<AnimOwnerRec*>& anim_owner_records();
// Its slot +0x10 0x004d4700 at the mission's end (0x004e7bc0): every record
// kept but emptied. The animations stop after (their callback hears 0).
void anim_owner_mission_end();
// Its slot +0x0c 0x004d4740 as the world goes (0x004e7b80): every record freed,
// before the animations are.
void anim_owner_free();

// What an animation's callback (+0x6c, 0x00508f20) is.
enum class AnimCallback : uint8_t { none, ours, other };  // other: not 0x004d4b10

struct AnimOwnerHooks : zgame::Unported {
  // zEffect's animation: its callback (0x00508f20), its argument (0x00508f30),
  // and both set to 0x004d4b10 and a record (0x00508f00).
  virtual AnimCallback anim_callback(Handle anim) { return AnimCallback(unported("0x00508f20", {anim}) & 0xff); }
  virtual AnimOwnerRec* anim_record(Handle anim) { (void)anim; return nullptr; }  // 0x00508f30
  virtual void anim_set_record(Handle anim, AnimOwnerRec* r) { (void)anim, (void)r; }  // 0x00508f00
  // The handler's calls: the owner's message (vt+0x74), the valve's set
  // (0x004d3c00), the inout tests (0x004c3bd0, ECX the animation).
  virtual void entity_message(Handle owner, Handle anim, int32_t msg) { unported("entity.vt+0x74", {owner, anim, msg}); }
  virtual void valve_set(Handle valve, int32_t v) { unported("0x004d3c00", {valve, v}); }
  virtual void inout_message(Handle anim, int32_t msg) { unported("0x004c3bd0", {anim, msg}); }
  // Message 0x11's (0x004d4980): the player's vehicle ([0x0060861c]+0x14),
  // the node "viaNode" (0x004f7fe0, list 6), its translation (0x004fc020) and
  // rotation (0x004fbe90), the vehicle's place (0x005a85e0), its placement
  // (vt+0xdc).
  virtual Handle player_vehicle() { return unported("[0x0060861c]+0x14", {}); }
  virtual Handle node_named(const char* name) { return unported("0x004f7fe0", {6, name}); }
  virtual void node_translation(Handle node, float out[3]) { (void)node, (void)out; }
  virtual void node_rotation(Handle node, float out[3]) { (void)node, (void)out; }
  virtual const float* entity_position(Handle e) { (void)e; return nullptr; }
  virtual void place(Handle v, const float pos[3], float yaw) {
    unported("vehicle.vt+0xdc", {v, zgame::bits(pos[0]), zgame::bits(pos[1]), zgame::bits(pos[2]), zgame::bits(yaw)});
  }
  // The binder's (0x004d4b30): the defs in turn (0x0050b570, from none), each
  // one's callback values in order (0x00508f40), its node (0x0050b5e0), a
  // node's entity (0x0040eee0), its parent when it has exactly one (+0x54 1,
  // +0x58[0]), the valve named as the node (0x004d3c40: found or made).
  virtual Handle anims_next(Handle after) { return unported("0x0050b570", {after}); }
  virtual void anim_callback_values(Handle anim, std::vector<int32_t>& out) { (void)anim, (void)out; }
  virtual Handle anim_node(Handle anim) { (void)anim; return 0; }
  virtual Handle node_entity(Handle node) { return unported("0x0040eee0", {node}); }
  virtual Handle node_parent(Handle node) { (void)node; return 0; }
  virtual Handle valve_of_node(Handle node) { return unported("0x004d3c40", {node}); }
};
void set_anim_owner_hooks(AnimOwnerHooks* h);
AnimOwnerHooks& anim_owner_hooks();

// 0x004d4f20 (ECX the owner, EDX the animation): the animation's record
// claimed for `owner` unless another holds it or the callback is not ours.
bool anim_owner_claim(Handle owner, Handle anim);
// 0x004d5100 -> 0x004d47e0: every record `owner` holds let go.
void anim_owner_release(Handle owner);
// 0x004d4b10 (ECX the animation, EDX its record) -> 0x004d4a20: what a
// callback event says.
void anim_owner_message(Handle anim, AnimOwnerRec* rec, int32_t msg);
// The binder's state on 0x004d4b30's stack (0x14 bytes, cleared by 0x004d46f0
// and 0x004d4e50): the def being bound, its record (or the temporary).
struct AnimBinding {
  bool valid = false;          // +0x00
  bool taken = false;          // +0x01  an event bound something
  Handle anim = 0;             // +0x04
  AnimOwnerRec* rec = nullptr; // +0x08
  AnimOwnerRec temp;           // +0x0c
};
// 0x004d4b30 (the mission load, after the vehicles' fixup): every def with
// callback events gets a record naming the node's entity (values 0, 0x38)
// and the valve (0x2a..0x31), or just one (0x39..0x3c).
void anim_owner_bind_all();
// 0x004d4c80 (ECX the def, EDX the binding, stack the value).
void anim_owner_bind_event(AnimBinding& b, Handle anim, int32_t value);
// 0x004d4e50: the binding's record kept and set on the def when an event
// bound something; the binding cleared.
bool anim_owner_bind_commit(AnimBinding& b);

// 0x004d4980: the player's vehicle moved by "viaNode" (`relative`, always 1
// from 0x004d4a20: the node's translation added to the vehicle's place).
void anim_owner_via_node(Handle vehicle, bool relative);

}  // namespace mech3::ai
