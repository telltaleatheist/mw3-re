#include "upgrades/mods/entry_run.h"

namespace mech3 {

EntryRun& entry_run() {
  static EntryRun r;
  return r;
}

}  // namespace mech3
