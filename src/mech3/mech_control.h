#pragma once
#include <cstdint>

#include "mech3/controls_cfg.h"

// The shared part of a mech's controls (docs/controls.md): the commanded
// throttle, turn and jump, and their smoothing toward the targets
// (docs/movement.md, Throttle and turn). Original file unknown: the code sits
// between mech_config and mech_data (named here mech_control after the
// alphabetical link order). The player, AI and network classes derive from it.
namespace mech3 {

struct Mech;

// exp(x) the game's way (ftol(x * 2^23 / ln 2) + 0x3f800000 as float bits; 0x00426960 and others).
float fast_exp(float x);

struct MechControls {
  virtual ~MechControls() = default;
  uint32_t flags = 0;          // +0x08  bit 2 doubles the turn rate
  int32_t view = 0;            // +0x0c  the player's view state 0..3 (0x00426780)
  float throttle = 0;          // +0x10  -1..1, smoothed toward throttle_target
  float turn = 0;              // +0x14  smoothed toward turn_target
  float pitch = 0;             // +0x18
  float twist = 0;             // +0x1c
  float jump = 0;              // +0x20  0..1, smoothed toward jump_target
  float jj_strafe = 0;         // +0x24
  float jj_forward = 0;        // +0x28
  float axis_2c = 0, axis_30 = 0;  // +0x2c, +0x30  free-look pitch and yaw (the torso's look inputs)
  uint8_t fire = 0;            // +0x34  handed to the weapons each frame (IControls +0x10)
  int32_t wrapped = 0;         // +0x38  set: an order wrapper to take off (primary vt +0x04)
  bool joy_throttle_set = false;  // +0x3c  the joystick's throttle sets the target outright (the throttle keys clear it)
  bool jump_held = false;      // +0x3d  a jump set to hold (0x00426b20(v, 1)): the player's zero leaves it
  bool throttle_held = false;  // +0x3e  cleared by the throttle keys; set, the throttle does not return to zero
  bool throttle_returns = false;  // +0x3f  ThrottleConst off: with no throttle key the throttle eases to zero
  float joy_throttle = 0;      // +0x40  the joystick throttle's last reading
  float throttle_target = 0;   // +0x44  -1..1
  float turn_target = 0;       // +0x48
  float jump_target = 0;       // +0x4c
  bool reverse = false;        // +0x50  the last non-zero throttle command was < 0
  uint32_t last_pov = 1;       // +0x54  the joystick hat's last reading
  bool moves[2] = {true, true};  // +0x58, +0x59  either set: the gait or the actions run
  Mech* owner = nullptr;       // +0x70  (IControls +0x10)

  virtual int update(float dt);  // IControls +0x0c: 0x00426960 for the base and network class
  int smooth(float dt) { return MechControls::update(dt); }  // 0x00426960 whatever the class
  void set_throttle(float v);    // 0x00426b60 (IControls +0x20)
  void set_turn(float v) { turn_target = v; }  // 0x00426b10 (IControls +0x28)
  void set_jump(float v, bool hold = false) {  // 0x00426b20 (IControls +0x34: 0x00426b40, no hold)
    jump_target = v;
    jump_held = hold;
  }
  void zero();                   // 0x00426c80
  // 0x00426650: the control mode (+0x08). While the axis table is ready, bit
  // 4 puts the joystick's axes in it and bit 1 the mouse's.
  void set_mode(uint32_t mode);
};

// The player's controls (ctor 0x00427320, 0x160 bytes; controls.md): the
// keyboard, joystick and mouse through zInput's commands and devices.
struct PlayerControls : MechControls {
  float look_dx = 0, look_dy = 0;  // +0x138, +0x13c  the mouse look's motion (view 2)
  float look_x = 0, look_y = 0;    // +0x140, +0x144  its position
  int update(float dt) override;   // 0x00427d60
  // 0x00427870: the joystick and mouse axes into the frame's inputs.
  void read_axes(float dt, float* throttle, float* turn, float* pitch, float* twist, float* look_pitch,
                 float* look_yaw);
};

// The AI's controls (ctor 0x004d8750, 0x320 bytes): the AI brain at +0x5c
// (ai.md section 4, not ported) sets the targets and runs the smoothing.
struct AiControls : MechControls {
  int update(float dt) override;   // 0x004e96f0
};

// The axis table's four bindings (0x006040d0: throttle, twist, pitch, turn;
// controls_cfg.h), set by the controls file, the options and set_mode.
inline AxisBinding* axis_bindings() { return axis_table().axes; }

// What the controls call outside: zInput (unset: its port), the HUD, the
// pilot and the AI brain (unset: nothing, 0).
struct ControlsCalls {
  uint32_t (*command)(int cmd) = nullptr;        // 0x0051d300; unset: zin::command
  const void* (*joy_state)() = nullptr;          // 0x0051dce0 (a DIJOYSTATE2); unset: zin::joy_state
  const void* (*joy_config)() = nullptr;         // 0x0051da90 (six zin::JoyAxis); unset: zin::joy_config
  const void* (*mouse_info)() = nullptr;         // 0x0051b7b0 (a zin::MouseInfo); unset: zin::mouse_info
  bool (*joy_present)() = nullptr;               // 0x0051dc10; unset: zin::joy_present
  void (*run_command)(int cmd) = nullptr;        // 0x0051c440; unset: zin::run_callback
  void (*mouse_set)(float x, float y) = nullptr; // 0x0051b440; unset: zin::mouse_set
  int (*camera_view)() = nullptr;                // [0x005f4c8c] +0x58 +0x40
  bool (*mw2_jets)() = nullptr;                  // 0x0040d1e0: the MW2JJets option ([0x005f4d04] bit 7)
  void (*pilot_tick)(MechControls&) = nullptr;   // IControls +0xa4: 0x004d8990 (0x004d9290)
  uint8_t (*screen_busy)(void* screen) = nullptr;  // the HUD's +0x167c
  void (*zoom)(void* screen, bool in) = nullptr; // the HUD's +0x1508: vt +0x60(-1) held, vt +0x64 let go
  int (*ai_brain)(MechControls&, float dt) = nullptr;  // 0x004e9710 on +0x5c; unset: the smoothing alone
  void (*joy_use)() = nullptr;                   // 0x0051b2c0; unset: zin::joy_use
};
ControlsCalls& controls_calls();

}  // namespace mech3
