#pragma once
#include <cstdint>

#include "mech3/menu_widgets.h"

// The main menu screen (docs/mainmenu.md): dialog.zrd's MAINMENU with its
// seven rippling buttons, the scrolling background and title, and the version
// line; the Main state (app_states) makes it and presses its buttons for the
// hotkeys. Original file unknown: between mech3\mech_waynet.cpp and
// mech3\mission.cpp (0x00485780..0x0048eb00), with the game states.
namespace mech3 {

// A main menu button (vtable 0x005c4f90, one subclass each: 0x005c4ef0 ..
// 0x005c4b30), 0x2a8 bytes: a rippling button with a line to its label
// (AUXBITMAP). Pressed, it pushes its menu's state; QUIT asks first.
class MainMenuButton : public SlideButton {
 public:
  enum Kind : int32_t { kInstantAction, kNewCampaign, kMultiplayer, kTraining, kOptions, kCredits, kQuit };
  MainMenuButton(Engine& e, Kind k);  // 0x00438d70(-1), the line hidden
  ~MainMenuButton() override = default;  // 0x0048e840 (0x0048e860); 0x00486840..0x00486900
  void update(float dt) override;    // +0x24  0x0048e810
  void on_left_up() override;        // +0x30  0x0048e980 .. 0x0048eb00
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x0048e6b0
  void set_baked(bool on) override;  // +0x90  0x0048e780

  ztwod::Image line{0};              // +0x1ec  AUXBITMAP
  Kind kind;                         // which subclass (its vtable in the original)
};

// The menu objects the Main state makes (0x00486990), the buttons' targets.
uint32_t menu_global(MainMenuButton::Kind k);

// The main menu (vtable 0x005c4b10), 0x12c1c bytes.
class MainMenu : public MenuScreen {
 public:
  MainMenu(Engine& e, Handle state);  // 0x00486260
  ~MainMenu() override;              // 0x00485780 (deleting 0x00485a60)
  void bake();                       // 0x00486ed0: FastMenu's still screen
  // The button at an offset of the object (+0x111ec + 0x2a8 n), or null.
  MainMenuButton* button_at(uint32_t offset);

  MainMenuButton buttons[7];         // +0x111ec  INSTANTACTION NEWCAMPAIGN MULTIPLAYER TRAINING OPTIONS CREDITS QUIT
  Marquee title;                     // +0x12484  MECH3TEXT
  ztwod::Image unused_image{0};      // +0x12544  never bound
  zrdr::Node* definition = nullptr;  // +0x12600  the dialog's tree when it loaded
  ztwod::Control unused_button;      // +0x12604  never bound
  ztwod::Text version;               // +0x12750  "MechWarrior 3 1.2", bottom right
};

MainMenu* main_menu();               // [0x00609854]

}  // namespace mech3
