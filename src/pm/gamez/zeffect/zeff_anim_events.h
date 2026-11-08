#pragma once
#include <cstdint>

namespace zeff { struct AnimDef; }

// C:\zipper\branchep1\gamez\zEffect (the handlers before zeff_anim_run.c, as MW3's): Pirate's Moon's own anim events,
// run in place of MW3's through zgame::Table. Their records are PM's (each sized by its header, as MW3's).
namespace pm::zeff {

// Pirate's Moon 1.0 @ 0x0051b910, event 4 (LIGHT_STATE). Against MW3's 0x00503390: a record to +0x98 with the
// light map (0x2000, +0x48) and the colour of the ambient (0x800, +0x84..+0x8c) and own colours (0x1000, +0x50) added,
// the place, orientation, range, colour, ambient and diffuse moved to +0x54.., and PM's setters (the static bit 0x200,
// the mixed colours).
int32_t ev_light_state(::zeff::AnimDef& def, uint8_t* e);

}  // namespace pm::zeff
