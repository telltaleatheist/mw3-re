#pragma once
#include <cstdint>

#include "gamez/zmath/zmth_matrix.h"

// A world point on the screen (docs/hud.md section 14). Original file
// unknown: 0x0053cf10 sits between gmod_const.c and gmod_cookie.cpp;
// named after what it does.
namespace zmod {

// Mech3.exe 1.2 @ 0x0053cf10: `world` through the view (identity: as it is)
// and projected; the point clamped to the point rect (0x0074031c), the
// outcode its sides: 1 left, 2 right, 4 top (above), 8 bottom. Behind the
// near plane: 8, y at the bottom and x spread across the screen from
// -5000..5000. FIX (bug): there the original ORs in the low bits of the
// input point's address (an unset local with the identity), returned as 8.
uint32_t project_point(const zmth::Vec3* world, zmth::Vec3* out);

}  // namespace zmod
