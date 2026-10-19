#include "mech3/mech_anim.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

// Mech3.exe 1.2 @ 0x0041ee80. Keeps the phase when the rate changes.
void MechAnim::set_speed(float speed) {
  const float f = mult * speed;
  if (f != 0.0f) {
    const float rt = rate * t;
    t = rt / f;
  }
  rate = f;
}

// Mech3.exe 1.2 @ 0x0041ef60
void MechAnim::advance(float dt) { t = t + dt; }

// Mech3.exe 1.2 @ 0x0041fce0. -1 is the start.
void MechAnim::seek(int event) { t = event == -1 ? 0.0f : rate * events[event]; }

// Mech3.exe 1.2 @ 0x0041ef00: the same fraction of the cycle as `from`.
void MechAnim::copy_phase(const MechAnim& from) {
  const float f = rate * from.loop;
  if (f != 0.0f) t = from.rate * from.t * loop / f;
}

// Mech3.exe 1.2 @ 0x00420f80
void MechAction::advance(float dt) {
  if (done) return;
  float v = t + (reversed ? -dt : dt);
  done = true;  // 0x00409980: clamp, true when it clamped
  if (v > loop) v = loop;
  else if (v < 0.0f) v = 0.0f;
  else done = false;
  t = v;
}

// Mech3.exe 1.2 @ 0x00420f40
void MechAction::reset() {
  t = 0.0f;
  done = false;
  reversed = false;
}

// Mech3.exe 1.2 @ 0x00421000
bool MechAction::started() const { return reversed || t > 0.0f; }

// Mech3.exe 1.2 @ 0x00421060
void MechAction::stop() {
  reversed = true;
  done = false;
}

// Mech3.exe 1.2 @ 0x0041ef90
void MechAnim::set_stride_speed(float s) {
  stride_speed = s;
  if (stride > 1.0f) {
    const float a = stride_speed * loop;
    mult = a / stride;
  } else {
    mult = 1.0f;
  }
}

}  // namespace mech3
