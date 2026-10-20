#pragma once

// The bounding sphere of eight box corners. Original file unknown: it sits
// between zClass\cls_util.c and Window.c, most likely cls_util.c.
namespace zcls {

// ECX corners, EDX centre, stack &radius: the centre of their axis-aligned
// box and its half-diagonal through the approximate square root.
void corners_sphere(const float corners[8][3], float centre[3], float* radius);  // 0x004f9c40

// ECX box (min xyz, max xyz), EDX centre, stack &radius: the box's centre
// and half-diagonal (the partitions' sphere after the area update).
void box_sphere(const float box[6], float centre[3], float* radius);  // 0x004f9bc0

// The engine's square root guess (bits / 2 + 0x1fc00000), up to ~6% off.
float approx_sqrt(float x);

}  // namespace zcls
