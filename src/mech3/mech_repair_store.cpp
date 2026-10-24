// Mech::repair's store calls (mech_repair.cpp), kept apart from mech.h:
// pilot.h's Handle is zgame's.
#include "mech3/mech_repair.h"
#include "mech3/pilot.h"

namespace mech3 {

// 0x00479430 on [0x0060861c]
Store* RepairHooks::store() { return current_pilot() ? current_pilot()->store() : nullptr; }
ConfigItem* repair_store_find(Store* s, const ConfigItem& like) { return s->find_like(&like); }
void repair_store_remove(Store* s, ConfigItem* item, int32_t n) { s->remove(item, n); }

}  // namespace mech3
