#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"

// What the HUD's radar and TARGET box ask of a contact's object, by its class
// (docs/hud.md section 15, docs/ai.md section 15, Radar contacts): the slots
// of the entity vtables (found by their vt+0xd4) and the few functions they
// reach. Not one original file: each answer is the slot's function as
// written, the class picking which one (the port's stand-in for the vtable).
namespace mech3 {

// The classes, by their vtables.
enum class EntityClass : uint8_t {
  none,
  mech,        // 0x005bcef8
  net_mech,    // 0x005c3508 (the network's)
  vehicle,     // 0x005c88e8 (the basic vehicles), 0x005c8460 (the stealth object)
  field_base,  // 0x005c23d8 (0x004589d0's)
  turret,      // 0x005c8770
  target,      // 0x005ccb90
  trap,        // 0x005ccc80
  marker,      // 0x005cced8 (the command marker)
  plain,       // 0x005ccfe0 (a plain AiTarget)
  vehicle_base,  // 0x005c8d48, the vehicles' base class (0x004aeb00): no instance, each constructor puts its own
};

// The blip tables vt+0x3c reads (ally, enemy, other).
enum class BlipTable : uint8_t { none, mech, vehicle, turret, target };

struct EntityFacts : zgame::Unported {
  virtual EntityClass class_of(zgame::Handle o) { (void)o; return EntityClass::none; }
  // [0x00605618] mech, [0x0060b120] vehicle, [0x00609a38] turret, [0x0060b4f0] target.
  virtual zgame::Handle blip(BlipTable t, int32_t kind) { (void)t, (void)kind; return 0; }
  virtual uint32_t team(zgame::Handle o) { (void)o; return 0; }        // AiTarget +0xc0
  virtual float height(zgame::Handle o) { (void)o; return 0; }         // +0x68
  virtual uint8_t target_e8(zgame::Handle o) { (void)o; return 0; }    // a target's +0xe8 (its description's flags)
  virtual const char* target_name(zgame::Handle o) { (void)o; return nullptr; }  // a target's +0xec
  // 0x004aee70: the controls' (vt+0x140) brain's (vt+0x54) pilot's name.
  virtual const char* pilot_name(zgame::Handle o) { return reinterpret_cast<const char*>(uintptr_t{unported("0x004aee70", {o})}); }
  // 0x004709c0: the network object's name (+0x6fc, 0x00588a60).
  virtual const char* net_name(zgame::Handle o) { return reinterpret_cast<const char*>(uintptr_t{unported("0x004709c0", {o})}); }
  virtual zgame::Handle player_vehicle() { return unported("[0x0060861c]+0x14", {}); }
  virtual const char* player_name() { return nullptr; }         // [0x0060861c]+0x18
  // 0x005a85e0, zEntity3d's place; 0x004d84f0, the centroid node's
  // world translation (+0x14, its data +0x38: +0x84, refreshed first when its
  // flags have 0x20: 0x004ed280).
  virtual void place(zgame::Handle o, float out[3]) { (void)o, out[0] = out[1] = out[2] = 0; }
  virtual void target_centre(zgame::Handle o, float out[3]) { (void)o, out[0] = out[1] = out[2] = 0; }
};
void set_entity_facts(EntityFacts* f);
EntityFacts& entity_facts();

BlipTable blip_table_of(EntityClass c);
// vt+0x3c (0x0041ec60, 0x0045b970, 0x004a5fc0, 0x004d2d70; 0x00410510 none).
zgame::Handle entity_blip(zgame::Handle o, int32_t kind);
// vt+0x28 (0x004d2a70, itself) +0xc0.
uint32_t entity_team(zgame::Handle o);
// vt+0x50: itself for a mech (0x004d2a70), else none (0x0040bae0).
zgame::Handle entity_mech(zgame::Handle o);
// vt+0xa8: a target's 0x004d3350 (not its +0xe8 bit 0), else 0 (0x004d7290).
bool entity_hidden(zgame::Handle o);
// vt+0xa4: a mech's 0x0041df30 (the player's pilot name, else 0x004aee70, else
// ""), the network mech's 0x004709c0, a vehicle's or field base's 0x004aee70, a
// target's 0x004d3330 -> vt+0xc4 0x004d3320 (+0xec), a turret's, marker's or
// plain one's none (0x004d72a0).
const char* entity_name(zgame::Handle o);
// vt+0xbc: a target's or plain one's 0x004d84f0, else 0x004d7260 (0x005a85e0).
void entity_centre(zgame::Handle o, float out[3]);
// 0x004e6fe0: the contact's seen object's vt+0x2c (itself in every class).
zgame::Handle contact_vehicle_of(zgame::Handle seen);
// 0x004e7070: that object's +0x68, 5 with none.
float contact_size_of(zgame::Handle seen);

}  // namespace mech3
