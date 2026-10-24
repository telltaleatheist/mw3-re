// The store side of probe_mech_repair.cpp (pilot.h's Handle is zgame's, so
// apart from mech.h).
#include <cstdint>

#include "mech3/mech_config.h"
#include "mech3/pilot.h"

namespace m3 = mech3;

m3::Store* rp_store_make() { return new m3::Store(0); }
void rp_store_free(m3::Store* s) { delete s; }

// kind 2 ammo (id the weapon), 3 equipment (id, tech), 1 a weapon (id, tech).
void rp_store_add(m3::Store* s, int32_t kind, int32_t id, int32_t tech, int32_t count, uint8_t b10) {
  m3::ConfigItem* it = kind == 2 ? static_cast<m3::ConfigItem*>(s->add_ammo(id, 1))
                       : kind == 3 ? static_cast<m3::ConfigItem*>(s->add_equipment(id, tech, 1))
                                   : static_cast<m3::ConfigItem*>(s->add_weapon(id, tech));
  if (!it) return;
  it->count = count;
  it->b10 = b10;
}

// The items: kind, id, tech (-1 none), count, b10 each; returns the words.
int rp_store_dump(m3::Store* s, uint32_t* out) {
  int k = 0;
  out[k++] = static_cast<uint32_t>(s->items.size());
  for (m3::ConfigItem* it : s->items) {
    out[k++] = static_cast<uint32_t>(it->kind());
    out[k++] = static_cast<uint32_t>(it->id());
    const auto* e = dynamic_cast<m3::EquipmentItem*>(it);
    const auto* w = dynamic_cast<m3::WeaponItem*>(it);
    out[k++] = e ? static_cast<uint32_t>(e->tech) : w ? static_cast<uint32_t>(w->tech) : 0xffffffffu;
    out[k++] = static_cast<uint32_t>(it->count);
    out[k++] = it->b10;
  }
  return k;
}
