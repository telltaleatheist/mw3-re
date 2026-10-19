#pragma once
#include <cstdint>

#include "gamez/zgame/zgameapp.h"
#include "mech3/app_hooks.h"

// The game states the app's stack runs (docs/game-loop.md). The static objects
// at 0x00608e70..0x00608f58 are picked by Mech3App::state(n) (0x00482970).
// Attract, Intro, Exit, Pre Lab, Core and the key list sit next to the app
// (original file unknown, between mech3\mech_waynet.cpp and mech3\mission.cpp);
// the menu states share a base, the "shell" state (0x0048eb10, vtable
// 0x005c6038), whose screens are the 2D front end's.
namespace mech3 {

class Mech3App;

class State : public zgame::State {
 public:
  State(const char* name, uint32_t addr) : name_(name), addr_(addr) {}
  const char* name() const { return name_; }  // +0x04, _strdup'd in the original
  // The static object's address in the original: how hooks name the state.
  uint32_t addr() const { return addr_; }

 protected:
  Engine& e();
  Mech3App& a();

 private:
  const char* name_;
  uint32_t addr_;
};

// 0 "Attract State" (0x00608e90, vtable 0x005c4078): the ATTRACT movie, shown
// when the main menu sits idle for 60 seconds.
class AttractState : public State {
 public:
  AttractState() : State("Attract State", 0x608e90) {}
  bool enter() override;              // 0x00482330
  bool update() override;             // 0x00482430
  void leave() override;              // 0x00482470
  uint32_t message(uint32_t, uint32_t) override { return 1; }  // 0x004c93c0
  Handle movie_ = 0;                  // +0x08
};

// 1 "Intro State" (0x00608f58, vtable 0x005c40c8): INTRO, then the SPLASH logos.
class IntroState : public State {
 public:
  IntroState() : State("Intro State", 0x608f58) {}
  bool enter() override;              // 0x00482070
  bool update() override;             // 0x00482170
  void leave() override;              // 0x004822d0
  uint32_t message(uint32_t, uint32_t) override { return 1; }  // 0x004c93c0
  Handle intro_ = 0;                  // +0x08
  Handle splash_ = 0;                 // +0x0c
};

// 4 "Exit State" (0x00608ef8, vtable 0x005c4178): stops the game, then quits.
class ExitState : public State {
 public:
  ExitState() : State("Exit State", 0x608ef8) {}
  bool enter() override;              // 0x00482060
  bool update() override { return true; }  // 0x00499270
};

// 7 "Pre Lab State" (0x00608f38, vtable 0x005c4120): the debug menu's Mech Lab.
class PreLabState : public State {
 public:
  PreLabState() : State("Pre Lab State", 0x608f38) {}
  bool enter() override;              // 0x00480b30
  bool update() override;             // 0x00480b60
  void leave() override {}            // 0x005ac5c0
};

// 5 "Core State" (0x00608ec0, vtable 0x005c4040): the mission. Its frame picks
// one of three drawing paths (the objects at vtables 0x005c4068/6c/70).
class CoreState : public State {
 public:
  enum Renderer : int { kNone = -1, kSoftware = 0, kDoubled = 1, kHardware = 2 };
  CoreState() : State("Core State", 0x608ec0) {}
  void on_activate(uint32_t active) override;  // 0x00481280
  bool enter() override;              // 0x00480c70
  bool update() override;             // 0x00481f70
  void leave() override;              // 0x00481fc0
  void suspend(uint32_t arg) override;  // 0x00481250
  void resume(uint32_t arg) override;   // 0x00481170

  bool frame();                       // 0x00481e70
  void pick_renderer();               // 0x00480bf0
  void start_mission();               // 0x00480df0
  bool draw_software(Handle veh, bool flag);  // 0x00481290
  bool draw_doubled(Handle veh, bool flag);   // 0x00481a10
  bool draw_hardware(Handle veh, bool flag);  // 0x00481790

  Handle window_rect_ = 0;            // +0x08
  Handle display_rect_ = 0;           // +0x0c
  Handle render_rect_ = 0;            // +0x10
  float elapsed_ = 0;                 // +0x14
  uint8_t paused_ = 0;                // +0x18
  uint8_t active_ = 0;                // +0x19
  int renderer_ = kNone;              // +0x1c
  Handle message_box_ = 0;            // +0x20
};

// 10, unnamed (0x00608ea0, vtable 0x005c40a0): a list of the 0xa7 input
// commands and their keys, likely a debug screen.
class KeyListState : public State {
 public:
  KeyListState() : State("", 0x608ea0) {}
  bool enter() override;              // 0x00482570
  bool update() override;             // 0x00482520
  void leave() override;              // 0x00482550
  Handle list_ = 0;                   // +0x18
};

// The menu states' base (0x0048eb10): a 2D screen at +0x0c.
class ShellState : public State {
 public:
  ShellState(const char* name, uint32_t addr) : State(name, addr) {}
  bool enter() override;              // 0x0048ebf0
  bool update() override;             // 0x0048ed30
  void leave() override;              // 0x0048ec40
  void suspend(uint32_t arg) override;  // 0x0048ec80
  void resume(uint32_t arg) override;   // 0x0048ecc0
  virtual void escape() {}            // +0x28, command 1 (Escape)
  void refresh_target();              // 0x0048edc0

  uint32_t cursor_ = 0;               // +0x08  the cursor state before entering
  Handle screen_ = 0;                 // +0x0c
};

// 2 "Main State" (0x00608f00, vtable 0x005c5030): the main menu.
class MainState : public ShellState {
 public:
  MainState() : ShellState("Main State", 0x608f00) {}
  bool enter() override;              // 0x00486990
  bool update() override;             // 0x00486d40
  void leave() override;              // 0x00486c50
  void suspend(uint32_t arg) override;  // 0x0048e400
  void resume(uint32_t arg) override;   // 0x00486e80
  void escape() override;             // 0x00486ec0
  void reset_idle() { idle_ = 60.0f; }  // 0x00486980
  uint32_t save_pilots_ = 0;          // +0x10  (never set by this file)
  float idle_ = 0;                    // +0x14  seconds left before the attract movie
};

// 3 "Pause State" (0x00608f48, vtable 0x005c40f0).
class PauseState : public ShellState {
 public:
  PauseState() : ShellState("Pause State", 0x608f48) {}
  bool enter() override;              // 0x00493040
  void leave() override;              // 0x004931e0
  void escape() override;             // 0x00493250
};

// 6 "Load State" (0x00608f18, vtable 0x005c4148): the briefing (briefing.cpp).
class LoadState : public ShellState {
 public:
  LoadState() : ShellState("Load State", 0x608f18) {}
  bool enter() override;              // 0x004091e0
  bool update() override;             // 0x00409440
  void leave() override;              // 0x004093e0
  void suspend(uint32_t arg) override;  // 0x004092c0
  void resume(uint32_t arg) override;   // 0x004092e0
  void escape() override {}           // 0x0049bfd0
  uint8_t build_ = 0;                 // +0x10
  Handle movie_ = 0;                  // +0x18
};

// 9 "Win State" (0x00608e70, vtable 0x005c3980): the debriefing.
class WinState : public ShellState {
 public:
  WinState() : ShellState("Win State", 0x608e70) {}
  bool enter() override;              // 0x0047ac20
  bool update() override;             // 0x0047aed0
  void leave() override;              // 0x0047af20
  void escape() override {}           // 0x0049bfd0
  uint8_t campaign_won_ = 0;          // +0x10
  Handle movie_ = 0;                  // +0x14
  char voice_[32] = {};               // +0x18  CString "c%dm%dwin"; +0x1c its sound
                                      // (set by 0x0047abe0, so kept in the original's memory)
};

// 8 "Salvage State" (0x00608ee8, vtable 0x005c3ee8).
class SalvageState : public ShellState {
 public:
  SalvageState() : ShellState("Salvage State", 0x608ee8) {}
  bool enter() override;              // 0x0047d310
  void leave() override;              // 0x0047d390
  void escape() override {}           // 0x0049bfd0
};

struct States {
  AttractState attract;
  IntroState intro;
  MainState main;
  PauseState pause;
  ExitState exit;
  CoreState core;
  LoadState load;
  PreLabState pre_lab;
  SalvageState salvage;
  WinState win;
  KeyListState key_list;
};
States& states();
void reset_states();

// Mech3.exe 1.2 @ 0x004814e0: the Direct3D frame's first call, the mechs'
// projected shadows (docs/render-shadows.md).
void update_shadows(Engine& en);

}  // namespace mech3
