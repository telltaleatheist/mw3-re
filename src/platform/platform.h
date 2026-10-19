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

// Stands in for the Windows message queue zGameApp::Run (0x0058fd70) drives:
// PeekMessageA(PM_NOREMOVE), CWinThread::PumpMessage, MsgWaitForMultipleObjects,
// PostQuitMessage and SetThreadPriority (docs/game-loop.md).
class MessagePump {
 public:
  virtual ~MessagePump() = default;
  virtual bool peek() = 0;               // an event is waiting
  virtual bool pump() = 0;               // handle one event; false once quit was posted
  virtual void wait(uint32_t ms) = 0;    // sleep until an event arrives or ms pass
  virtual void post_quit() = 0;          // make a later pump() return false
  virtual void raise_priority() {}       // THREAD_PRIORITY_HIGHEST for the game thread
};

// Stands in for the registry keys the game reads and writes
// (HKLM/HKCU\SOFTWARE\MicroProse\MechWarrior 3\1.0, docs/startup.md). A value is
// raw bytes; read() fails when the value is missing or its size differs.
class Settings {
 public:
  virtual ~Settings() = default;
  virtual bool read(const std::string& name, bool per_user, void* data, uint32_t size) = 0;
  virtual void write(const std::string& name, bool per_user, const void* data, uint32_t size) = 0;
};

}  // namespace platform
