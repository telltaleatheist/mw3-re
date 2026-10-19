#pragma once
#include <cstdint>
#include <string>

// What the rebuilt code needs from the machine it runs on. These replace the Win32,
// DirectX, MCI and WinMM calls the original makes; each front end (SDL, Godot,
// tests) supplies an implementation. Keep this small: add a call only when ported
// code needs it, and name the original API it stands in for.
namespace platform {

// Stands in for GetTickCount(): milliseconds, but counted from when the game
// started, not from when the machine booted (see gamez/zutil/ztimer.cpp).
class Clock {
 public:
  virtual ~Clock() = default;
  virtual uint32_t ticks_ms() = 0;
};

// Stands in for the CD drive behind MCI "cdaudio". Plays one audio file at a time
// and reports when it reaches the end, as MCI's MM_MCINOTIFY did.
class MusicOutput {
 public:
  virtual ~MusicOutput() = default;
  virtual bool play(const std::string& path) = 0;  // false if it cannot be played
  virtual void stop() = 0;
  virtual void pause() = 0;
  virtual void resume() = 0;
  virtual void set_volume(uint16_t left, uint16_t right) = 0;  // auxSetVolume range
};

// Stands in for the CD's table of contents: is there a file at this path?
class Files {
 public:
  virtual ~Files() = default;
  virtual bool exists(const std::string& path) = 0;
};

// Stands in for the MessageBoxA the engine shows before ExitProcess when it
// must abort (0x004eada0): tell the player, the caller then quits.
class Alert {
 public:
  virtual ~Alert() = default;
  virtual void abort_message(const std::string& message) = 0;
};

}  // namespace platform
