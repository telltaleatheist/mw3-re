#pragma once
#include <cstdint>
#include <list>
#include <vector>

#include "gamez/zgame/unported.h"
#include "mech3/zai_target.h"

// The sensors (docs/ai.md section 15): each Core frame 0x004a8cd0 fills
// every vehicle's contact list (AiTarget +0xac) with what it can sense, in
// sight or not, nearest first, shares C3 contacts, keeps the targets picked
// and runs the view's radar ping. The original file is not known: these
// functions sit between turret.cpp and veh_basic.cpp (0x004a7c70..0x004ab4c0).
// The entity sort the mission start runs (0x004d5ca0) and the lance flag
// (0x004d5be0) are AiTarget's (after ZAiTarget.cpp) and live here too.
namespace mech3::ai {

using zgame::Handle;

// zEntity's list [0x00736558]: begin [0x0073655c], end [0x00736560]. zEntity
// is not ported; the list is the port's, filled by 0x005a7f50's callers.
std::vector<Handle>& entity_list();
// The allies after the player in the sorted list: [0x0060b550, 0x0060b548).
struct SortedAllies {
  size_t first = 0, end = 0;
};
SortedAllies& sorted_allies();
// [0x0060b570]: every vehicle senses every other, in range or not, and
// sees it. Only cleared (the launch, 0x00491547): a debug switch.
bool& see_all();

// One vehicle's frame (0x44 bytes, vtable 0x005c88ac; 0x004a7fd0).
struct SensorRecord {
  Handle self = 0;              // +0x04  the vehicle (its entity's vt+0x2c)
  AiTarget* t = nullptr;        // its AiTarget fields
  // The picks before the frame: the vehicle each led to (0x004e6fe0) and
  // the contact on the same vehicle found this frame.
  Contact* target = nullptr;    // +0x08
  Handle target_was = 0;        // +0x0c
  Contact* ally = nullptr;      // +0x10
  Handle ally_was = 0;          // +0x14
  Contact* enemy = nullptr;     // +0x18
  Handle enemy_was = 0;         // +0x1c
  Contact* saved = nullptr;     // +0x20
  Handle saved_was = 0;         // +0x24
  // +0x28: the vehicle's tracked contacts (AiTarget +0x88): where each is and
  // the vehicle it saw.
  struct Ref {
    Contact** slot;
    Handle was;
  };
  std::vector<Ref> refs;
  bool is_player = false;       // +0x38
  // +0x3c: bit 0 passive radar (+0xbc bit 2), 1 shut down (vt+0xa0 == 1),
  // 2 BAP, 3 a C3 master (the player only), 4 C3 (a slave, or an ally of
  // the player with any C3), 6 ECM, 7 a lancemate under orders.
  uint8_t flags = 0;
  int32_t sorted_by_seen = 0;   // +0x40  1 once the C3 merge sorted the contacts by what they saw
};

// What the sensors ask of the objects of modules not ported (vehicles,
// targets, zEntity, the HUD's ping). Virtual calls go through vcall(); the
// keys name the slot on the kind of object the original calls it on.
class SensorHooks : public zgame::Unported {
 public:
  virtual uint32_t vcall(Handle obj, uint32_t slot, std::initializer_list<zgame::Arg> args);
  // The AiTarget part of a vehicle or target (its fields, not a call).
  virtual AiTarget* fields(Handle x) { (void)x; return nullptr; }
  virtual uint32_t team(Handle t) { AiTarget* a = fields(t); return a ? a->team : 0; }  // vt+0x28's +0xc0
  virtual int32_t equipment(Handle mech, int32_t kind) { (void)mech, (void)kind; return 0; }  // +0x3ac + 4 kind
  virtual uint8_t active(Handle t) { (void)t; return 1; }   // a target's +0x08
  virtual void set_sensed(Handle e, uint8_t on) { (void)e, (void)on; }  // ZEntity +0x09
  // The world position of +0x14's node (the centroid) or of +0x04's
  // (0x005a85e0), refreshed first when stale (0x00448600: 0x0051e9d0,
  // 0x0051fde0, 0x004ed280(node, 1), 0x0051ea00).
  virtual void centroid(Handle x, float out[3]) { (void)x, out[0] = out[1] = out[2] = 0; }
  virtual void node_position(Handle x, float out[3]) { (void)x, out[0] = out[1] = out[2] = 0; }
  virtual void entity_refresh(Handle x) { unported("0x005a85c0", {x}); }  // vt+0x1c when it has a node
  virtual void contact_delete(Contact* c) { delete c; }
  // vt+0xc0 (AiTarget's 0x004d5e40; the mech's 0x0041d890 adds the HUD's lock).
  virtual void set_target(Handle x, Contact* c, bool keep) {
    if (AiTarget* a = fields(x)) a->set_target(c, keep);
  }
  // The targets.zrd objects ([0x0060b584..0x0060b588]), by handle.
  virtual std::vector<Handle> targets();
  virtual void set_sorted() {}  // [0x0060b56c] = 1
  // The view's ping (0x004adce0 on the object's +0xe8).
  virtual void ping(Handle s, const float pos[3]) {
    unported("0x004adce0", {s, zgame::bits(pos[0]), zgame::bits(pos[1]), zgame::bits(pos[2])});
  }
  // The brain's lancemate setting (+0xcd) and pilot (+0xd4).
  virtual uint8_t brain_lancemate(Handle b) { (void)b; return 0; }
  virtual const char* brain_pilot(Handle b) { (void)b; return nullptr; }
  virtual const char* message(int32_t id) {
    unported("0x00576040", {id});
    return nullptr;
  }
  virtual Handle player() { return unported("[0x0060861c]+0x14", {}); }
  virtual Handle view_target() { return unported("[0x005f4c8c]+0x64", {}); }
  virtual Handle stealth() { return unported("[0x00609a2c]", {}); }
};
void set_sensor_hooks(SensorHooks* h);
SensorHooks& sensor_hooks();

// Mech3.exe 1.2 @ 0x004a8cd0: the frame's sensing, from the Core frame.
void sensors_update();

// Mech3.exe 1.2 @ 0x004d5ca0: the lance flags, then the list sorted (the
// player first, the stealth entity last, allies before the rest, the
// player's lancemates first among them in the lance order), and the allies'
// range after the player. [0x0060b56c] set.
void entities_sort();
// Mech3.exe 1.2 @ 0x004d5be0: AiTarget +0xbc bit 4 from the brain's lancemate setting.
void lance_flag(Handle vehicle);
// Mech3.exe 1.2 @ 0x004d5dc0: the player's allies after the sort turn on him.
void allies_turn();

}  // namespace mech3::ai
