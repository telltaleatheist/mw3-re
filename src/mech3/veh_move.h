#pragma once
#include <cstdint>

#include "mech3/veh_basic_object.h"

// The vehicles' movement (docs/vehicles.md, The movement). Original file:
// these sit in 0x004a1000..0x004a3000, before mech3\veh_basic.cpp's named
// functions; named after the movement. The frame time and its inverse are
// [0x005e8110] / [0x005e8114] (the player tick's), through the hooks.
namespace mech3 {

// Mech3.exe 1.2 @ 0x004a28c0: while turning to the controls' heading (x760),
// a turn command cancels it; else a full turn a frame (turns[1] x dt) while
// the heading is further than that, then the yaw set to it and the turn 0.
void vehicle_turn_to_heading(BasicVehicle& v);
// Mech3.exe 1.2 @ 0x004a2890: zEntity's refresh, then the controls' up as
// the matrix's second row.
void vehicle_up_refresh(BasicVehicle& v);
// Mech3.exe 1.2 @ 0x004a2b10: the local speed from the throttle (rates[0],
// damped by rate_damping, held to |throttle| x top_speed) and the slide
// damped away; sliding, 0x004a2dd0 instead.
void vehicle_speed_step(BasicVehicle& v);
// Mech3.exe 1.2 @ 0x004a2dd0: sliding, the world velocity into the local
// one, the speed as above (held to top_speed), the side force applied, the
// slide ending when it turns over.
void vehicle_slide(BasicVehicle& v);
// Mech3.exe 1.2 @ 0x004a2c90: the side force, the slope's (x 60) and the
// turn's against the friction (friction[0] to start a slide, [1] in one).
float vehicle_side_force(BasicVehicle& v);

}  // namespace mech3
