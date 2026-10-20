#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// The entity manager at 0x00736558: the list of every entity and the update
// budget that lets only so many do a full update each frame (docs/movement.md,
// The frame). Original file unknown: the code follows zentity\zent_init.cpp.
// The list itself is the caller's (the program's mechs, mech3::ai::entity_list).

namespace zent {

struct Budget {
  bool updating = false;     // 0x0073657c  set while 0x005a8850 walks the list
  void* resume = nullptr;    // +0x10  where the next frame's walk starts
  void* current = nullptr;   // +0x14  the entity being updated
  int32_t left = 0;          // +0x18  full updates left this frame (-1: none counted)
  int32_t per_frame = 0x40;  // +0x1c  > 0 caps the full updates; <= 0 every entity (0x005a8750: 64)

  bool allows() const;       // 0x005a8910
  void spend();              // 0x005a8960

  // Mech3.exe 1.2 @ 0x005a8850: each entity's update (vt+0x34 when its +0x09
  // is set: `each` decides), from where the last frame stopped to the end and
  // round from the start; the next frame starts one further on. Not while a
  // walk runs. An empty list (never, in play: the player is on it) is left.
  template <typename T, typename F>
  void update_all(std::vector<T>& list, F&& each) {
    if (updating || list.empty()) return;
    updating = true;
    T* const data = list.data();
    const size_t n = list.size();
    auto at = [&](const void* p) {
      const T* t = static_cast<const T*>(p);
      return t >= data && t < data + n ? static_cast<size_t>(t - data) : 0;
    };
    const size_t start = at(resume);
    if (per_frame >= 0 || ++left >= 0) left = per_frame;
    for (size_t i = start; i < n; ++i) current = &data[i], each(data[i]);
    for (size_t i = 0; i < start; ++i) current = &data[i], each(data[i]);
    current = &data[start];  // where the second loop stops
    const size_t next = at(resume) + 1;
    resume = &data[next == n ? 0 : next];
    updating = false;
  }
};
Budget& budget();

}  // namespace zent
