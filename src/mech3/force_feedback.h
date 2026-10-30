#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "platform/force_feedback.h"

// The player's force feedback (the object at [0x00608620], 0x34 bytes; made
// by the app's start 0x00480570 when zInput's joystick has force feedback,
// 0x0051ddb0). Original file unknown: 0x00433090..0x00433c80, between
// mech_data.cpp and mech_map.cpp. The effects are force_eff.ifr's, by name;
// IFORCE2 is platform::ForceDevice (FIX (A)). docs/input.md, Force feedback.
namespace mech3 {

// One named effect (0x158 bytes, 0x004339a0): its DirectInput parts.
class FfEffect {
 public:
  FfEffect(const char* name, platform::ForceDevice* dev);  // 0x004339a0
  bool start();                // 0x00433b70: every part, Start(1, 0)
  bool stop();                 // 0x00433bc0: Stop, on the first part as many times as there are parts (kept)
  bool set_direction(int32_t degrees);  // 0x00433c00: 0..360
  bool set_gain(int32_t gain);          // 0x00433c50: 0..100
  bool made() const { return !parts.empty(); }
  platform::ForceDevice* dev = nullptr;
  std::vector<int32_t> parts;  // +0x04 the array, +0x00 its count
};

class ForceFeedback {
 public:
  explicit ForceFeedback(platform::ForceDevice* dev);  // 0x00433090
  ~ForceFeedback();                                    // 0x004333f0
  void stop_all();                                     // 0x00433530
  void jolt(const float n[3], float amount);           // 0x004335d0: a collision, Spin_Move at amount x 80
  void weapon(int32_t kind);                           // 0x00433630: a shot, Recoil or laser_buzz by the weapon
  void hit(int32_t weapon, const float dir[3], float force);  // 0x00433720: impact_force from the hit's side
  void footstep(float speed);                          // 0x004337c0: footfall at speed x 100
  void landing(float jolt);                            // 0x00433800: impact_land
  void fall();                                         // 0x00433840: i_fall, 100, ahead
  void jets(bool on);                                  // 0x00433870: jets1
  void spring(bool on);                                // 0x004338a0: Spring (the mission's start)
  void dead_spring(bool on);                           // 0x004338d0
  void dead_dance(bool on);                            // 0x00433900
  void water(bool on);                                 // 0x00433930: wading

  // +0x00 the project (0x00433960: IFLoadProjectFile), then the effects.
  bool project = false;
  std::unique_ptr<FfEffect> recoil, laser_buzz, spin_move, impact_force, footfall, jets1, spring_fx, dead_spring_fx,
      dead_dance_fx, water_fx, impact_land, i_fall;  // +0x04 .. +0x30
};

// The game's calls to it on, to `f` (null: off): app_force.cpp.
void program_force_feedback(ForceFeedback* f);

}  // namespace mech3
