#pragma once
#include <cstdint>

// The player's lance orders from his keys (docs/ai.md 17.6): the dispatcher
// 0x00469940 the commands reach through 0x00469930 (mech_command.cpp's
// lance_order). Original file unknown: the code sits with the player's
// command handler (0x00469xxx, mech_*.cpp).
namespace mech3::ai {
class Brain;
struct Contact;
}  // namespace mech3::ai

namespace mech3 {

struct Mech;
struct MechControls;

// What the dispatcher calls (unset: the lancemate's AI controls' brain, its
// Brain::lance_order); the tests stand in.
struct LanceCalls {
  ai::Brain* (*brain)(Mech& mate) = nullptr;                       // vt+0x140, then vt+0x54
  int32_t (*order)(ai::Brain& b, int32_t order, ai::Contact* c) = nullptr;  // 0x004ea000..0x004ea230
};
LanceCalls& lance_calls();

// Mech3.exe 1.2 @ 0x00469940 (ECX the controls, EDX the order): the orders
// 0, 1, 7, 8, 9, 12 to the selected lancemate on his contact on the player
// (0x00469400), 3, 4, 5, 6, 10, 11 on the player's target (0x00469650),
// through the lancemate's brain (ai::Brain::lance_order). The controls the
// order leaves (the same ones: the command mode, order 2, and the lance
// selection it keeps, item L4c, are not ported).
MechControls* lance_dispatch(MechControls* c, int32_t order);

}  // namespace mech3
