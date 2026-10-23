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

// The track's ground following (2c2; docs/vehicles.md, The suspension).
// Mech3.exe 1.2 @ 0x004a23f0: the supports' world places (the motion's
// support[8..], from support[0..]), the ground under each (one query from
// 500 up, the vehicle's node left out), each one's ground height (the pick,
// step 4, water not ground) into heights[], its slope against the world
// velocity into slopes[], the highest water surface into *top, a count by
// the pick's water flag into counts[100]; support 0's polygon gives the zone.
// False when a support has no ground under it.
bool vehicle_support_ground(BasicVehicle& v, float* heights, float* top, float* slopes, int32_t* counts);
// Mech3.exe 1.2 @ 0x004a2820: a support's height *y and vertical speed *vs
// a frame on (gravity g): falling while above ground (*air), landing at the
// ground with `landing` for its speed, else following it.
void vehicle_support_step(float* y, float* vs, float ground, int32_t* air, float g, float landing);
// Mech3.exe 1.2 @ 0x004a1760 (TRACK): the top speeds (slowed by quicksand
// or lava under two supports, which the pick never reports: never), the
// supports stepped, the pitch and roll rates from three of them (or, all in
// the air, back toward level), the plane through the three for the up, the
// position on it, the matrix, pitch and roll; in the air both held to 30
// degrees. 0 when a support has no ground.
int32_t vehicle_suspension(BasicVehicle& v);
// Mech3.exe 1.2 @ 0x004abf70: pos's height on the plane through `at` with normal n.
void height_on_plane(float pos[3], const float at[3], const float n[3]);
// Mech3.exe 1.2 @ 0x004a3e40: the forward (+0x18) tilted onto the up.
void vehicle_forward_from_up(BasicVehicle& v);
// Mech3.exe 1.2 @ 0x004a3ed0: the matrix from forward and up: (forward x up,
// up, -forward, the position).
void vehicle_matrix_build(BasicVehicle& v);

}  // namespace mech3
