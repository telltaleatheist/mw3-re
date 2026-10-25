#include "mech3/zai_list.h"

namespace mech3::ai {

// Mech3.exe 1.2 @ 0x004d8610. The front is read again after each call, so
// an entry told may add or take others.
void ResetList::reset() {
  while (!items.empty()) {
    ResetEntry* e = items.front();
    items.pop_front();
    e->reset();
  }
}

ResetList* reset_list(uint32_t address) {
  static ResetList lists[4];  // 0x00604038, 0x00604048, 0x00604078, 0x00603f90
  switch (address) {
    case 0x00604038: return &lists[0];
    case 0x00604048: return &lists[1];
    case 0x00604078: return &lists[2];
    case 0x00603f90: return &lists[3];
    default: return nullptr;
  }
}

}  // namespace mech3::ai
