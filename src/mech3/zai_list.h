#pragma once
#include <cstdint>
#include <list>

// Four static lists of the AI's (ctor 0x004d8540, vtable 0x005cd0d4, dtor
// 0x004d8590; between ZAiTarget.cpp and zrdrainet.cpp, file not named):
// [0x00604038] reset each Core frame (0x00480f5e), [0x00604048] and
// [0x00604078] at the mission load's "Post Vehicle Fixup", [0x00603f90]
// after it (0x00490f23..0x00490f7c). Nothing in the exe adds to them (their
// addresses appear only in those calls and the static ctors and dtors), so
// they stay empty in play. docs/ai.md section 19.
namespace mech3::ai {

// An entry: the reset calls its vt+0x04 (no arguments) once it is off the list.
struct ResetEntry {
  virtual ~ResetEntry() = default;
  virtual void reset() = 0;  // vt+0x04
};

struct ResetList {
  std::list<ResetEntry*> items;  // +0x08 the sentinel node, +0x0c the count
  // Mech3.exe 1.2 @ 0x004d8610: each entry from the front taken off, then told.
  void reset();
};

// The four, by the exe's address (0x00604038, 0x00604048, 0x00604078,
// 0x00603f90); null for another.
ResetList* reset_list(uint32_t address);

}  // namespace mech3::ai
