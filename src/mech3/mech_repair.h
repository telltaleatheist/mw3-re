#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"

// Mech::repair 0x004154e0's calls out (mech_repair.cpp, docs/vehicles.md,
// The repair): the store it takes ammo and armour from, the player's view
// and the HUD it refreshes.
namespace mech3 {

struct Mech;
class Store;
struct ConfigItem;

class RepairHooks : public zgame::Unported {
 public:
  virtual Store* store();          // 0x00479430 on [0x0060861c]: the current game's (none: nothing taken)
  virtual bool viewed(Mech& m);    // [0x005f4c8c]+0x64 is the mech
  // The HUD (0x00430aa0 on +0x684), its weapon lines refreshed (0x00446520
  // on +0x6520) and its reset (0x0043be10).
  virtual void* hud(Mech& m);
  virtual void hud_weapons_refresh(void* hud);
  virtual void hud_reset(void* hud);
};
void set_repair_hooks(RepairHooks* h);  // null: the defaults
RepairHooks& repair_hooks();

// The store's calls (mech_repair_store.cpp, apart from mech.h): find_like
// 0x004714c0 and remove vt+0x14 (0x00471d20).
ConfigItem* repair_store_find(Store* s, const ConfigItem& like);
void repair_store_remove(Store* s, ConfigItem* item, int32_t n);

}  // namespace mech3
