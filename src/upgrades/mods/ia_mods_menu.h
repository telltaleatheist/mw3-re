#pragma once
#include <functional>
#include <string>
#include <vector>

#include "upgrades/mods/ia_mods.h"
#include "upgrades/mods/mods_tick_list.h"

// Ours (docs/mods-campaigns-plan.md, Instant Action; Owen 2026-11-10):
// Instant Action's own MODS. A MODS tab on its screen (ACTIONDIALOG, our
// record edit; ActionScreen 0x00488bc0) opens this screen over it,
// IAMODSDIALOG on the MODS screen's look: the tick list of the entry `ia`,
// ALL OFF, ACCEPT and BACK. ACCEPT saves the ticks; changed and some on, the
// program starts again into the entry (--entry ia --entry-ia); none on inside
// the entry, or ALL OFF there, it starts again plain on Instant Action
// (--open-ia); nothing changed, nothing starts again. Bound only where the
// program turns it on (ia_mods_page): the probes run the original's dialog.
namespace mech3 {

constexpr uint32_t kIaModsScreenCtor = 0xf0000005;  // ours: not the exe's addresses
constexpr uint32_t kIaModsStateCtor = 0xf0000006;
bool& ia_mods_page();     // the program turns Instant Action's MODS tab on
Handle& ia_mods_state();  // its menu object, made at the first press, let go with the main menu's
// The program's start again: into the entry (its ticks mounted) or plain, on Instant Action. False with `why`.
// `pilot`: the one to pick there ("" none: the pilot screen).
using IaModsRestart = std::function<bool(bool into_entry, const std::string& pilot, std::string* why)>;
IaModsRestart& ia_mods_restart();

class IaModsScreen : public MenuScreen {
 public:
  enum Id : int32_t { kAccept = 1, kBack, kAllOff };
  IaModsScreen(Engine& e, Handle state);
  ~IaModsScreen() override;
  void on_button(int32_t id, int32_t a, int32_t b) override;
  void accept();
  void show_note();
  void tell(const std::string& text);

  ModsSlideButton accept_btn, back;
  ShellButton all_off;
  ModTickList ticks;
  ztwod::Image lines{0}, lines2{0};
  zimg::Texture* own_lines = nullptr;
  ztwod::Text title, line, note, note2;
  std::vector<std::string> before;  // the entry's ticks as the screen opened
  bool inside = false;              // the run is inside the entry
};

class IaModsState : public MenuState {
 public:
  explicit IaModsState(Handle addr) : MenuState("IA Mods State", addr) {}
  bool enter() override;
  void leave() override;
};

}  // namespace mech3
