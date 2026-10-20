#include "mech3/menu_states.h"

#include "mech3/app.h"

namespace mech3 {

namespace {
const char* name_of(OptionsState::Kind k) {
  switch (k) {
    case OptionsState::kGame: return "Game Options State";
    case OptionsState::kGfxAudio: return "Gfx and Audio Options State";
    default: return "Controls Configure State";
  }
}
}  // namespace

// Mech3.exe 1.2 @ 0x0048eb80
MenuState::~MenuState() { clear_screen(); }

// Mech3.exe 1.2 @ 0x00438c80
void MenuState::escape() { a().pop(1); }

// Mech3.exe 1.2 @ 0x00438be0, 0x0043a640, 0x0042d9d0
OptionsState::OptionsState(Kind k, Handle addr) : MenuState(name_of(k), addr), kind(k) {
  static const uint32_t kScreens[] = {0x4379f0, 0x438f60, 0x42a590};  // new(0x1407c), (0x13f9c), (0x16c94)
  screen_ = e().construct(kScreens[k], {addr});
}

// Mech3.exe 1.2 @ 0x00438c90, 0x0042da50
bool OptionsState::enter() {
  Engine& en = e();
  if (kind == kGame) en.set_screen_mode(5);
  ShellState::enter();
  en.vcall(screen_, 0x10, {0});
  return true;
}

// Mech3.exe 1.2 @ 0x00429600
bool CreditsState::enter() {
  Engine& en = e();
  screen_ = en.construct(0x4289a0, {addr()});  // new(0x11ae0)
  en.credits_restart(screen_);
  ShellState::enter();
  return true;
}

// Mech3.exe 1.2 @ 0x00429670
void CreditsState::leave() {
  e().shell_clear(addr());  // 0x0048ee00
  ShellState::leave();
}

}  // namespace mech3
