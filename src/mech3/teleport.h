#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "upgrades/editor/ddx.h"

namespace zrdr { struct Node; }

// The developers' teleport (docs/editor.md, Teleport). Original file unknown:
// the command 0x004a37a0 sits between player.cpp and turret.cpp, dialog 132
// (0x00404630..0x00404ab0) between the start and briefing.cpp. Command 0x95
// in app_key's switch (0x0040dba0) runs it, but no key ever calls it in the
// release (0x0040db60 registers none); ours opens it from the editor's menu.
// Pirate's Moon: the command 0x004a8450 (0x9d in the switch 0x0040ba30, no
// key in 0x0040b9f0 either) and dialog 132 at 0x004a8a40..0x004a9000; what
// changed is marked "Pirate's Moon" below (editor.md, Pirate's Moon's teleport).
// It lists places: the player's own ("Current Location"), every vehicle of
// the mission by name, and the mission's location.zrd (each world's
// reader.zbd ships one with the developers' spots); the dialog moves the
// player to the one picked or typed, and may write the list back.
namespace mech3 {

// The 0x50-byte record: its place, its yaw in degrees, its name.
struct TeleportSpot {
  float pos[3] = {};      // +0x00
  float yaw = 0;          // +0x0c  degrees
  std::string name;       // +0x10  up to 59 characters
  uint8_t unsaved = 0;    // +0x4c  1: not written back (Current Location, or deleted)
  uint8_t kept = 1;       // +0x4d  Pirate's Moon: 0 for a nav point, never written back
};

struct TeleportHooks {
  virtual ~TeleportHooks() = default;
  // The player's vehicle (the world's +100): its place (0x005a85e0 on its node).
  virtual bool player_place(float pos[3]) { (void)pos; return false; }
  // Its vt+0xdc: put at `pos`, facing `yaw` (radians).
  virtual void move_player(const float pos[3], float yaw) { (void)pos, (void)yaw; }
  // The mission's nav points ([0x00609980], the nav waynet: vt+0x10 the
  // count, 0x0058b060 each, vt+0x24 its label, vt+0x18 its place); yaw 0.
  // FIX (bug): one with no label (vt+0x24 null) is left out; MW3's copies
  // from null, Pirate's Moon's ([0x0062daf0]) lists it with its name unset.
  virtual std::vector<TeleportSpot> navs() { return {}; }
  // 0x004ac150: the game's message box (`arg` into the text's %s); 6 for Yes.
  virtual int32_t message_box(int32_t style, int32_t caption, int32_t text, const std::string& arg) {
    (void)style, (void)caption, (void)text, (void)arg;
    return 0;
  }
};
TeleportHooks& teleport_hooks();
void set_teleport_hooks(TeleportHooks* h);  // null: the defaults
bool teleport_hooks_default();

// 0x004a37a0's list: "Current Location" at `player` (unsaved), the nav points,
// then `location`'s entries (zrdr's tree of location.zrd: name ((x y z) yaw)
// pairs; null for none).
std::vector<TeleportSpot> teleport_spots(const float player[3], const zrdr::Node* location);
// The save's text (0x004a37a0's fprintf): its header with the build's date
// and time (PM's "Nov 14 1999", "14:36:03", whenever it is saved), then every
// spot but the unsaved ones and the nav points as "name" ( ( x y z ) yaw ),
// %8.1f each.
std::string location_zrd_text(const std::vector<TeleportSpot>& spots);

// Dialog 132 (0x00404630: CDialog +0x60 the spots, +0x64 changed, +0x68 the
// list, +0xa8..+0xb4 x, y, z, yaw).
struct TeleportDialog {
  static constexpr int32_t kList = 1073, kX = 1009, kY = 1010, kZ = 1011, kYaw = 1012, kDelete = 3;
  Dialog dlg;
  std::vector<TeleportSpot>* spots = nullptr;  // +0x60
  // +0x64: write the list back on OK. Set by any edit's EN_CHANGE, which
  // Windows sends for the dialog's own SetWindowText too: OnInitDialog's
  // UpdateData(FALSE) fills the empty fields, so in effect every OK writes.
  int32_t changed = 0;
  float x = 0, y = 0, z = 0, yaw = 0;          // +0xa8..+0xb4, yaw in degrees

  TeleportDialog();                            // 0x00404630: the list sorted (CBS_SORT), editable (CBS_DROPDOWN)
  void exchange(DataExchange& dx);             // 0x00404720
  bool update(bool save);                      // CWnd::UpdateData with exchange
  void init();                                 // 0x004047b0
  void pick();                                 // 0x004049b0: CBN_SELCHANGE
  void remove();                               // 0x00404a20: Delete, asked first
  void remove_picked();                        // 0x00404a20 after Yes
  void left_list();                            // 0x00404860: CBN_KILLFOCUS, a typed name added
  void changed_text();                         // 0x00404ab0: CBN_EDITCHANGE, EN_CHANGE
};

// 0x0040dfe0 split at its DoModal: the dialog over `spots`, its fields at
// `pos` and `yaw` (radians: shown in degrees); and OK's end, the fields back
// (`pos`, `yaw` in radians) and the changed flag.
void teleport_dialog_begin(TeleportDialog& d, std::vector<TeleportSpot>* spots, const float pos[3], float yaw);
bool teleport_dialog_ok(TeleportDialog& d, float pos[3], float* yaw, int32_t* changed);

}  // namespace mech3
