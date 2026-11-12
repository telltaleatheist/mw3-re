#pragma once
#include <cstdint>

namespace zeff { struct AnimDef; struct SeqDef; }

// C:\zipper\branchep1\gamez\zEffect (the handlers before zeff_anim_run.c, as MW3's): Pirate's Moon's own anim events,
// run in place of MW3's through zgame::Table. Their records are PM's (each sized by its header, as MW3's).
namespace pm::zeff {

// Pirate's Moon 1.0 @ 0x0051b910, event 4 (LIGHT_STATE). Against MW3's 0x00503390: a record to +0x98 with the
// light map (0x2000, +0x48) and the colour of the ambient (0x800, +0x84..+0x8c) and own colours (0x1000, +0x50) added,
// the place, orientation, range, colour, ambient and diffuse moved to +0x54.., and PM's setters (the static bit 0x200,
// the mixed colours).
int32_t ev_light_state(::zeff::AnimDef& def, uint8_t* e);

// Pirate's Moon 1.0 @ 0x0051c060, event 5 (LIGHT_ANIMATION): MW3's 0x00503910 step for step, through PM's own range
// and colour setters (0x0050edd0, 0x0050f460; its getters 0x0050eef0, 0x0050f3f0 read the same fields as MW3's).
int32_t ev_light_animation(::zeff::AnimDef& def, ::zeff::SeqDef& s, uint8_t* e);

// Pirate's Moon 1.0 @ 0x0051f900, event 25 (STOP_ANIMATION). Against MW3's 0x00506d60, two cached indices (shorts):
// +0x2c into the def's own anim refs, which it searches first by either of a ref's names (the last match kept: the
// loop does not stop), stopping the def or instance that ref started; +0x2e into the defs, searched only when no ref is named so (index 0 never stopped, as
// MW3's). PM's stop 0x0051f830 is MW3's 0x00506ca0 on its offsets.
int32_t ev_stop_animation(::zeff::AnimDef& def, uint8_t* e);

// Pirate's Moon 1.0 @ 0x0051e2c0, event 12 (OBJECT_MOTION_SI_SCRIPT). Against MW3's 0x00505b00, a 0x24-byte record:
// node +0x0c, and +0x10 the SI script (the anim file's, AnimState::pm_scripts) whose data holds the key frames, its size
// +0x14 their end and its +0x0c the cubic flag (MW3: the frames after the event, its +0x14). Played by 0x0051df10 and
// 0x0051deb0 (MW3's 0x00505710 and 0x005056b0 on the script).
int32_t ev_object_motion_script(::zeff::AnimDef& def, ::zeff::SeqDef& s, uint8_t* e);

}  // namespace pm::zeff
