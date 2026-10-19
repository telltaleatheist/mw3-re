#pragma once
#include <cstdint>

// The entity manager at 0x00736558: the list of every entity and the update
// budget that lets only so many do a full update each frame (docs/movement.md,
// The frame). Original file unknown: the code follows zentity\zent_init.cpp.
// Only the budget is ported; its update loop (0x005a8850) is not.
namespace zent {

struct Budget {
  bool updating = false;     // 0x0073657c  set while 0x005a8850 walks the list
  void* resume = nullptr;    // +0x10  where the next frame's walk starts
  void* current = nullptr;   // +0x14  the entity being updated
  int32_t left = 0;          // +0x18  full updates left this frame (-1: none counted)
  int32_t per_frame = 0;     // +0x1c  > 0 caps the full updates; <= 0 every entity

  bool allows() const;       // 0x005a8910
  void spend();              // 0x005a8960
};
Budget& budget();

}  // namespace zent
