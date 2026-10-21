#include "mech3/mech_salvage.h"

#include "mech3/mech_config.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"

namespace mech3 {

namespace {
constexpr int32_t kElemental = 7;
constexpr int32_t kTag = 0x20;          // never salvaged
constexpr int32_t kSingleSink = 9, kDoubleSink = 10;
}  // namespace

// Mech3.exe 1.2 @ 0x00472da0. A weapon not made in its tech is taken in the
// other (Clan for anything but Clan). FIX (bug): a weapon without a def was
// a null read; here it is skipped.
bool add_weapons(Store& s, SalvageMech& m, int32_t tech, const SalvageCalls& c) {
  if (!m.has_manager) return false;
  for (const SalvageWeapon& w : m.weapons) {
    if (!w.present || w.type < 0 || w.type == kTag) continue;
    int32_t t = w.has_item ? w.item_tech : tech;
    if (!weapon_has_tech(w.type, t)) t = t != 1 ? 1 : 2;
    if (!weapon_has_tech(w.type, t)) continue;
    s.add_weapon(w.type, t);
    if (w.beam || w.bin < 0) continue;
    const int32_t left = m.bins[w.bin];
    if (left == 0 || left == -1) continue;
    s.add_ammo(w.type, left);
    m.bins[w.bin] = 0;
    if (c.empty_bin) c.empty_bin(m, w.bin);
  }
  return true;
}

// Mech3.exe 1.2 @ 0x00472f00
bool add_salvage(Store& s, SalvageMech& m, const SalvageCalls& c) {
  if (!m.has_data || m.type == kElemental) return false;
  bool mech = !m.torso_gone || m.head_gone;
  if (mech && (m.leg_gone[0] || m.leg_gone[1])) mech = (c.rand() & 1) != 0;
  if (mech) s.add_mech(m.type);
  add_weapons(s, m, m.tech, c);
  if (m.config) {
    for (const ConfigItem* it : m.config->items) {
      if (it->kind() != 3) continue;
      const int32_t part = it->location(-1);
      const int32_t kind = static_cast<const EquipmentItem*>(it)->equipment;
      if (kind == kSingleSink || kind == kDoubleSink) continue;  // the sinks below
      const int8_t st = part >= 0 && part < 16 ? m.parts[part] : 0;
      if (part == -1 || st != 2) s.add_copy(it, it->count);
    }
  }
  const int32_t extra = m.sinks - 10;  // the engine's ten stay with it
  if (extra > 0) s.add_equipment(kSingleSink + (m.doubles ? 1 : 0), m.sink_tech, extra);
  return true;
}

// Mech3.exe 1.2 @ 0x00472ec0
bool add_vehicle_salvage(Store& s, SalvageMech& m, const SalvageCalls& c) {
  if (!(c.rand() & 1)) return true;
  return add_weapons(s, m, 1, c);
}

// Mech3.exe 1.2 @ 0x0047ae10 (its CL 0 path)
void lance_salvage(Store& salvage, Store& own, std::vector<SalvageEntity>& list, uint32_t allies, bool team,
                   const SalvageCalls& c) {
  for (SalvageEntity& e : list) {
    if (!e.mech) continue;
    if (e.player) add_salvage(own, e.m, c);
    else if (team && (allies & e.team)) add_salvage(salvage, e.m, c);
  }
}

Store& mission_salvage() {
  static Store store(1);
  return store;
}

}  // namespace mech3
