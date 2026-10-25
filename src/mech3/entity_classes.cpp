#include "mech3/entity_classes.h"

namespace mech3 {
namespace {
EntityFacts g_default;
EntityFacts* g_facts = &g_default;
EntityFacts& F() { return *g_facts; }
bool is_target(EntityClass c) { return c == EntityClass::target || c == EntityClass::trap; }
bool is_mech(EntityClass c) { return c == EntityClass::mech || c == EntityClass::net_mech; }
}  // namespace

void set_entity_facts(EntityFacts* f) { g_facts = f ? f : &g_default; }
EntityFacts& entity_facts() { return *g_facts; }

BlipTable blip_table_of(EntityClass c) {
  switch (c) {
    case EntityClass::mech:
    case EntityClass::net_mech: return BlipTable::mech;       // 0x0041ec60
    case EntityClass::vehicle: return BlipTable::vehicle;     // 0x0045b970
    case EntityClass::turret: return BlipTable::turret;       // 0x004a5fc0
    case EntityClass::target:
    case EntityClass::trap: return BlipTable::target;         // 0x004d2d70
    default: return BlipTable::none;                          // 0x00410510
  }
}

// Mech3.exe 1.2 @ 0x0041ec60, 0x0045b970, 0x004a5fc0, 0x004d2d70 (each
// [table + 4 kind]), 0x00410510 (0).
zgame::Handle entity_blip(zgame::Handle o, int32_t kind) {
  const BlipTable t = blip_table_of(F().class_of(o));
  return t == BlipTable::none ? 0 : F().blip(t, kind);
}

// Mech3.exe 1.2 @ 0x004d2a70 (vt+0x28) and the team at +0xc0.
uint32_t entity_team(zgame::Handle o) { return F().team(o); }

// Mech3.exe 1.2 @ 0x004d2a70 / 0x0040bae0 (vt+0x50).
zgame::Handle entity_mech(zgame::Handle o) { return is_mech(F().class_of(o)) ? o : 0; }

// Mech3.exe 1.2 @ 0x004d3350 / 0x004d7290 (vt+0xa8).
bool entity_hidden(zgame::Handle o) { return is_target(F().class_of(o)) && !(F().target_e8(o) & 1); }

// Mech3.exe 1.2 @ 0x0041df30, 0x004709c0, 0x004aee70, 0x004d3330 -> 0x004d3320,
// 0x004d72a0 (vt+0xa4).
const char* entity_name(zgame::Handle o) {
  EntityFacts& f = F();
  switch (f.class_of(o)) {
    case EntityClass::mech: {
      const char* n = o == f.player_vehicle() ? f.player_name() : f.pilot_name(o);
      return n ? n : "";  // [0x005f4c90]
    }
    case EntityClass::net_mech: return f.net_name(o);
    case EntityClass::vehicle:
    case EntityClass::field_base: return f.pilot_name(o);
    case EntityClass::target:
    case EntityClass::trap: return f.target_name(o);
    default: return nullptr;
  }
}

// Mech3.exe 1.2 @ 0x004d84f0 / 0x004d7260 (vt+0xbc).
void entity_centre(zgame::Handle o, float out[3]) {
  const EntityClass c = F().class_of(o);
  if (is_target(c) || c == EntityClass::plain) return F().target_centre(o, out);
  F().place(o, out);
}

// Mech3.exe 1.2 @ 0x004e6fe0. With no object the exe answers what EAX held.
zgame::Handle contact_vehicle_of(zgame::Handle seen) { return seen; }

// Mech3.exe 1.2 @ 0x004e7070 ([0x005cd5f4] 5 with none).
float contact_size_of(zgame::Handle seen) { return seen ? F().height(seen) : 5.0f; }

}  // namespace mech3
