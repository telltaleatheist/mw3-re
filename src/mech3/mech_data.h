#pragma once
#include <cstdint>

#include "mech3/mech.h"

namespace zrdr {
struct Node;
}

// A mech type's animations, loaded once and copied to each mech of it
// (docs/movement.md, The gait table). Original file: mech3\mech_data.cpp
// (0x004308a0..0x00431a70), its getters just after. The object is the
// mech's +0x684, here `Pilot` (mech.h).
namespace mech3 {

struct MechDfn;

// The calls the loader makes into code not ported here. Unset: dfn is
// mech_dfn(roster); canned does nothing and returns false.
struct MechDataCalls {
  const MechDfn* (*dfn)(Pilot&) = nullptr;               // 0x004846a0 on +0x0c
  bool (*canned)(Pilot&, zrdr::Node* canned, int i) = nullptr;  // 0x00431750: the canned effects (+0x14c + 0x14 i)
};
MechDataCalls& mech_data_calls();

const char* gait_name(int i);     // 0x00431560: limp, limp, stand, step, walk, trot, run, ..., 12 stand
const char* action_name(int i);   // 0x004316e0: 1 fallf .. 8 collapser

// The engine's top speed (0x00433030 over 0x00433050 with the XL flag 1):
// rating / mass x 1.5 x 10.8 km/h, x 0.2778 to m/s.
float engine_speed(int32_t engine, float mass);
// The base speed of 0x00431350 (config 0's engine and mech, 0x00432950) and
// of 0x004329c0 (config 0's engine, the roster's mass).
float config_speed(Pilot& p, bool roster_mass);

bool load_gait(Pilot& p, zrdr::Node* motion, int i);   // 0x00431350
bool load_action_motion(Pilot& p, int i);              // 0x004315d0
void load_animations(Pilot& p);                        // 0x00431a70
MechAnim* gait_copy(const Pilot& p, int i);            // 0x00431fb0
MechAction* action_copy(const Pilot& p, int i);        // 0x00432030
// 0x004329c0: the gait table for a top speed, the file's pairs x base / max.
void scale_gaits(Pilot& p, float gait[13][2], float max_speed);

}  // namespace mech3
