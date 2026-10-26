#pragma once
#include <cstdint>

#include "mech3/debriefing.h"
#include "mech3/menu_widgets.h"

// The salvage screen (docs/campaign.md section 10): the Salvage State's
// (new(0x166a8), 0x0047d3b0, its press 0x0047e490), dlgSalvage.zrd's
// SALVAGEDIALOG, opened by the briefing's SALVAGE (0x0047d2f0). Original
// file unknown: 0x0047d2f0..0x0047e520, after the debriefing's, named after
// its state. The player's store and the mission's salvage side by side: a
// choice in one list moves the item (or a share of it) to the other, within
// the field bases' tons and bays.
namespace mech3 {

class Store;
struct ConfigItem;

// Each side's list (vtable 0x005c3ca8): a choice goes to the screen's press
// with the list's id and the row.
class SalvageList : public StoreList {
 public:
  SalvageList(Engine& e, int32_t list_id) : StoreList(e, 0, 5) { id = list_id; }  // 0x0048d880(0, 5)
  void select(int32_t index) override;  // +0x98  0x0047da70
};

// The screen (vtable 0x005c3c88), 0x166a8 bytes.
class SalvageScreen : public MenuScreen {
 public:
  enum Button : int32_t { kAccept = 10, kPlayer = 11, kMission = 12 };

  SalvageScreen(Engine& e, Handle state);  // 0x0047d3b0
  ~SalvageScreen() override = default;  // 0x0047db60 (deleting 0x0047db10)
  void on_button(int32_t id, int32_t a, int32_t row) override;  // +0x14  0x0047e490
  void refill();                     // 0x0047dcf0: the cards, the lists, the tons

  HelpButton accept;                 // +0x111ec  ICON_ACCEPT (10)
  // +0x11368: a control (vtable 0x005bc230) the screen never binds or uses.
  LanceCard bays[6];                 // +0x114c0  ICON_PILOT, the player's mechs (0x0047db30)
  LanceCard lance[4];                // +0x13c80  ICON_PILOT, the lance's
  SalvageList player{engine(), kPlayer};    // +0x15700  PLAYER_SALVAGE
  SalvageList mission{engine(), kMission};  // +0x15dd0  MISSION_SALVAGE
  ztwod::Image back{0};              // +0x164a0  BACKGROUND (its own, not MenuScreen's +0x110b0)
  WinTitle text;                     // +0x1655c  SALVAGE_TEXT: [3] used, [4] capacity, [5] the bases
};

// 0x0047e110 (the lists' stores): from the player's store to the mission's,
// one, an ammo item's lab rounds, or a share of an equipment's count.
bool salvage_to_mission(Store* to, Store* from, ConfigItem* item);
// 0x0047e270: from the mission's to the player's, within `bases` field
// bases (300 t and 2 mechs each, beside what `to` holds).
bool salvage_to_player(Store* to, Store* from, ConfigItem* item, int32_t bases);

}  // namespace mech3
