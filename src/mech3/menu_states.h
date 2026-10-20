#pragma once
#include <cstdint>

#include "mech3/app_states.h"

// The menu objects (docs/menus.md, The menu states): shell states the main
// menu makes on entering (0x00486990) and the pause screen makes for its
// Options (0x00492d70), pushed by their buttons and deleted when those
// screens leave. Unlike the static states they live on the heap; the address
// a hook names one by is its handle. Original files unknown: the options'
// with their screens (between mech3\mech_anim.cpp and mech3\mech_map.cpp),
// the credits' with theirs.
namespace mech3 {

// The shell base for the heap ones: the destructor (0x0048eb80) deletes the
// screen; Escape (+0x28, 0x00438c80) pops back (resume argument 1).
class MenuState : public ShellState {
 public:
  MenuState(const char* name, Handle addr) : ShellState(name, addr) {}
  ~MenuState() override;             // 0x0048eb80 (deleting 0x00438c60)
  void escape() override;            // 0x00438c80
};

// "Game Options State" (0x00438be0, vtable 0x005bf7a8), "Gfx and Audio Options
// State" (0x0043a640, vtable 0x005c0040) and "Controls Configure State"
// (0x0042d9d0, vtable 0x005be3d8): their screens are made with the state and
// read the options on every entry.
class OptionsState : public MenuState {
 public:
  enum Kind { kGame, kGfxAudio, kControls };
  OptionsState(Kind k, Handle addr);  // the screen: 0x004379f0, 0x00438f60, 0x0042a590
  // 0x00438c90 (Game: the menus' screen mode 5 first), 0x0042da50 (the others).
  bool enter() override;
  void resume(uint32_t arg) override { ShellState::resume(arg); }  // 0x0042da70
  Kind kind;
};

// "Credits State" (0x004295e0, vtable 0x005bd938): the screen made on entering
// and deleted on leaving.
class CreditsState : public MenuState {
 public:
  explicit CreditsState(Handle addr) : MenuState("Credits State", addr) {}
  bool enter() override;             // 0x00429600
  void leave() override;             // 0x00429670
};

}  // namespace mech3
