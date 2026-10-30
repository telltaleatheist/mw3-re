#pragma once
#include <cstdint>
#include <string>
#include <vector>

// FIX (A): what the game asks Immersion's IFORCE2.dll for (docs/input.md,
// Force feedback): the project file (IFLoadProjectFile), an effect's
// DirectInput effects by name (IFCreateEffects: a compound gives one per
// part), and each part's gain, direction, start and stop (IDirectInputEffect
// SetParameters with DIEP_GAIN / DIEP_DIRECTION, Start(1, 0), Stop). A front
// end plays them on what it has (the SDL one: the gamepad's or stick's rumble).
namespace platform {

class ForceDevice {
 public:
  virtual ~ForceDevice() = default;
  virtual bool load(const std::string& path) = 0;            // false: the project not read
  virtual std::vector<int32_t> create(const char* name) = 0;  // the parts; none: no such effect
  virtual void set_gain(int32_t part, int32_t gain) = 0;      // 0..10000
  virtual void set_direction(int32_t part, int32_t dir) = 0;  // hundredths of a degree, 0..36000
  virtual void start(int32_t part) = 0;
  virtual void stop(int32_t part) = 0;
};

}  // namespace platform
