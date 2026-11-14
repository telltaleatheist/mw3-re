#ifndef MECH3_DIR_LIMITS_PM_H
#define MECH3_DIR_LIMITS_PM_H

// Pirate's Moon's direction limits (the exe's own report names the class CZValidateDir): one record for a
// vehicle's gun, a turret's turn and a mech firepoint's aim, where MechWarrior 3 had four turn-limit classes
// (turret.h's TurnLimits) and the vehicles' pitch-only TurretLimits. 0x48 bytes, as 18 floats:
//   [0..3]   the yaw band from 0 up: its middle's direction (-sin, 0, -cos) and the cosine of its half width
//            (FLT_MAX in [3] when there is none, -1 when it is half a turn or more): 0x004a6d40;
//   [4..7]   the band from 0 down, the same way;
//   [8..10], [11..13]  the edges the clamp falls back on outside both bands (+0x20, +0x2c): nothing in the exe
//            writes them, so they stay 0 here;
//   [14..17] the pitch: the sines of its bottom and top, then their cosines (0x004a6e30).
// A direction is within when its pitch is and, unless it is all but straight up or down, either yaw band
// holds it (0x004a7000). docs/pm-tests.md, The direction limits.

namespace mech3::pm_dir {

constexpr int kSize = 18;

void unset(float* c);                                              // 0x004a6ec0: no band, no pitch (never within)
void set(float* c, float pitch_lo, float pitch_hi, float yaw_lo, float yaw_hi);  // 0x004a6ee0
void set_radians(float* c, float pitch_lo, float pitch_hi, float yaw_lo, float yaw_hi);  // 0x004a7390
void set_turn(float* c, float pitch, float yaw);                  // 0x004a7420: -pitch..pitch, -yaw..yaw
bool within(const float* c, const float d[3]);                    // 0x004a70c0 -> 0x004a7000
bool clamp(const float* c, const float d[3], float out[3]);       // 0x004a71f0 with 0x004a7170

// The setter's report when the pitch is outside -pi/2..pi/2 (0x00406cb0); none by default.
extern void (*report)(int level, int line, const char* text);

}  // namespace mech3::pm_dir

#endif
