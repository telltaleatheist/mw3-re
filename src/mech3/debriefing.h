#pragma once
#include <cstdint>
#include <vector>

#include "mech3/menu_widgets.h"

// The debriefing screen (docs/campaign.md section 3): the Win State's
// (new(0x13bf4), 0x0047b430, its press 0x0047d030), dlgWin.zrd's WINMENU.
// Original file unknown: 0x0047af70..0x0047d2c0, between the Win State
// (0x0047ab10..) and the Salvage State (0x0047d2f0..), named after them.
// Item 6a: the screen, its texts by the game and the result, the buttons and
// their press, the lance cards' and the salvage list's shells. The results
// (0x0047baf0..0x0047c84b with the rank 0x0047b2f0), the cards' mech display
// (0x0047b210, 0x0047e840) and the salvage list's refill (0x0048dbe0) are
// hooks (campaign.md section 7, items 6b, 6c).
namespace mech3 {

// The mech lab's mech button (0x0044a770, vtable 0x005c0d70): the help
// button with a mech's number and its name under the image.
class MechButton : public HelpButton {
 public:
  explicit MechButton(Engine& e);    // 0x0044a770
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x0044a840
  void show_name(bool on);           // 0x0044aa70

  int32_t mech = 0x13;               // +0x17c  0x13: none
  ztwod::Text name;                  // +0x180
};

// A lance member's card (0x0047af70, vtable 0x005c39b0), 0x6a0 bytes: its
// mech button with the mech's display (0x0047e840, HTAL's place) and a line.
class LanceCard : public MechButton {
 public:
  explicit LanceCard(Engine& e);     // 0x0047af70
  ~LanceCard() override;             // 0x0047b180: the display deleted
  void update(float dt) override;    // +0x24  0x0047b2b0: no blink; the line and the display
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x0047b070
  void on_left_up() override {}      // +0x30  0x0049bfd0: a card is not pressed
  void on_enter() override {}        // +0x3c  0x0049bfd0
  void on_leave() override {}        // +0x40  0x0049bfd0
  void set_vehicle(Handle v);        // 0x0047b210

  Handle vehicle = 0;                // +0x64c
  Handle display = 0;                // +0x650  (not ported: a hook's handle)
  ztwod::Line2D line{0, 0, 0, 0, 0};  // +0x654
};

// The rank's icon (vtable 0x005c3bd0) and the title (vtable 0x005c11c0):
// no rollover (+0x3c, +0x40 0x0049bfd0).
class RankIcon : public HelpButton {
 public:
  explicit RankIcon(Engine& e) : HelpButton(e, -1) {}
  void on_enter() override {}
  void on_leave() override {}
};
class WinTitle : public ztwod::Control {
 public:
  void on_enter() override {}
  void on_leave() override {}
};

// The screen (vtable 0x005c3b00), 0x13bf4 bytes.
class DebriefScreen : public MenuScreen {
 public:
  enum Button : int32_t { kContinue = 0, kReplay = 1, kAbort = 2 };

  // 0x0047b430: the Win State's address, the operation and mission shown,
  // and the Win State's +0x10 (the campaign is over).
  DebriefScreen(Engine& e, Handle state, int32_t campaign, int32_t mission, const uint8_t* won);
  ~DebriefScreen() override;         // 0x0047c920 (deleting 0x0047c8d0)
  void update(float dt) override;    // +0x00  0x0042efe0: the window's
  void on_button(int32_t id, int32_t a, int32_t b) override;  // +0x14  0x0047d030

  ztwod::Image background{0};        // +0x111ec  WIN_BACKGROUND
  HelpButton continue_btn, replay_btn, abort_btn;  // +0x112a8, +0x11424, +0x115a0 (0x0044c310)
  LanceCard cards[4];                // +0x1171c  ICON_PLAYER0..3
  RankIcon rank;                     // +0x1319c  ICON_RANK
  ztwod::Line2D lines[2]{{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}};  // +0x13318, +0x13364
  WinTitle title;                    // +0x133b0  WIN_TITLE
  StoreList salvage;                 // +0x134fc  SALVAGE_LIST (vtable 0x005c3b20: only its destructor its own)
  std::vector<ztwod::Text*> texts;   // +0x13bd0  the results' texts (6b; +0x13bcc its allocator byte)
  const uint8_t* won;                // +0x13bdc  the Win State's +0x10
  int32_t score = 0;                 // +0x13be0  the campaign's +0xc4 (6b)
  int32_t lance_scores[4] = {};      // +0x13be4  each lancemate's +0xb4 (6b)
  uint8_t clean = 0;                 // no cheat ([0x00609994] clear; a local of 0x0047b430)
};

}  // namespace mech3
