#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "platform/force_feedback.h"

// FIX (A): IFORCE2.dll's effects (force_eff.ifr, Immersion's project file)
// played as SDL's rumble (docs/input.md, Force feedback). The file is read
// as IFORCE2 reads it: "ifpr", a count, then each record its size, name,
// type and key/value strings (Magnitude, Duration and Period in
// microseconds, a compound's ContainedObjects by their ID). An effect is
// found by its name with '_' for ' ' (the exe asks "impact_force"; its
// "i_fall" names nothing, as in the original). A part plays on the fast
// motor when periodic at 100 ms or less, else on the slow one, at
// Magnitude x gain, for its Duration (an infinite one until stopped). The
// conditions (Spring, Damper: forces on a stick's axes) have no rumble.
namespace sdlfe {

class SdlForceDevice : public platform::ForceDevice {
 public:
  // The joystick to rumble (SdlInput's), asked at each change.
  explicit SdlForceDevice(std::function<SDL_Joystick*()> joystick) : joystick_(std::move(joystick)) {}
  bool load(const std::string& path) override;
  std::vector<int32_t> create(const char* name) override;
  void set_gain(int32_t part, int32_t gain) override;
  void set_direction(int32_t part, int32_t dir) override;
  void start(int32_t part) override;
  void stop(int32_t part) override;

  struct Record {
    std::string name, type, id;
    std::map<std::string, std::string> values;
    std::vector<std::string> contains;  // a compound's parts by ID
  };
  static bool parse(const std::vector<uint8_t>& data, std::vector<Record>* out);  // false: not a project
  // What the rumble is set to (the tests' and the log's): 0..0xffff each, ms.
  struct Rumble {
    uint16_t low = 0, high = 0;
    uint32_t ms = 0;
  };
  Rumble last() const { return last_; }
  uint64_t (*now_ms)() = [] { return static_cast<uint64_t>(SDL_GetTicks()); };

 private:
  struct Part {
    bool fast = false, rumbles = false, infinite = false;
    int32_t magnitude = 0, gain = 10000;
    uint32_t ms = 0;
    uint64_t until = 0;  // 0: not playing
  };
  void apply();
  std::function<SDL_Joystick*()> joystick_;
  std::vector<Record> records_;
  std::vector<Part> parts_;
  Rumble last_;
};

}  // namespace sdlfe
