#include "mech3/menu_states.h"

#include "mech3/app.h"
#include "mech3/options_menu.h"

namespace mech3 {

namespace {
const char* name_of(OptionsState::Kind k) {
  switch (k) {
    case OptionsState::kGame: return "Game Options State";
    case OptionsState::kGfxAudio: return "Gfx and Audio Options State";
    case OptionsState::kUpgrades: return "Upgrades State";  // ours
    default: return "Controls Configure State";
  }
}
}  // namespace

// Mech3.exe 1.2 @ 0x0048eb80
MenuState::~MenuState() { clear_screen(); }

// Mech3.exe 1.2 @ 0x00438c80
void MenuState::escape() { e().pop_state(1); }

// Mech3.exe 1.2 @ 0x00438be0, 0x0043a640, 0x0042d9d0
OptionsState::OptionsState(Kind k, Handle addr) : MenuState(name_of(k), addr), kind(k) {
  static const uint32_t kScreens[] = {0x4379f0, 0x438f60, 0x42a590, kUpgradesScreenCtor};  // new(0x1407c), (0x13f9c), (0x16c94), ours
  screen_ = e().construct(kScreens[k], {addr});
}

// Mech3.exe 1.2 @ 0x00438c90, 0x0042da50, Pirate's Moon 1.0 @ 0x0043b7c0 (Gfx and Audio: the
// effects and music volumes kept for BACK, 0x0040b9a0 / 0x0040b9e0)
bool OptionsState::enter() {
  Engine& en = e();
  if (kind == kGame) en.set_screen_mode(en.menu_vmode());  // 5 (ours: wide_menus)
  ShellState::enter();
  if (kind == kGfxAudio) kept_effects = en.sound_frequency(), kept_music = en.cd_volume();
  en.vcall(screen_, 0x10, {0});
  return true;
}

// Pirate's Moon 1.0 @ 0x0043b790 (0x0040b980 / 0x0040b9b0, then the pop as 0x00438c80)
// Pirate's Moon 1.0 @ 0x004399e0 (Game Options, vt+0x28): the pop with 7, which no state below takes,
// so the one redraw of the screen under it is skipped (its animation redraws it anyway); MW3's 1.
void OptionsState::escape() {
  if (kind == kGame) return e().pop_state(7);
  if (kind == kGfxAudio) {
    Engine& en = e();
    en.set_sound_frequency(kept_effects);
    en.set_cd_volume(kept_music);
  }
  MenuState::escape();
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
