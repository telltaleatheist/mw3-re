#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "mech3/zrdraiv.h"
#include "upgrades/editor/ddx.h"
#include "upgrades/editor/form.h"

// The vehicle editor (EDIT VEHICLES 0x004040c0 and debug command 0x9d ->
// 0x004b86e0; docs/editor.md, The vehicle editor): the "Mech AI" dialog
// (template 165), a vehicle picked from the mission's and its AI settings
// edited on twelve pages, Apply putting them on its brain and in aiv.zrd's
// record cache. Original file unknown: it sits between weapon.cpp and
// zainetedit.cpp (0x004b7c80..0x004bcb70), with the tree view's window.
// FIX (A): the MFC dialog is a form (form.h) drawn by ImGui, its pages tabs,
// their controls kept as data and exchanged by MFC's rules (ddx.h).
namespace mech3 {

struct ConfigList;
struct NetEditor;
namespace ai {
class AiNet;
}

// One entry of the vehicle list (the combo 1239): its name (the list's text
// and the aiv record's key) and the vehicle (the item's data).
struct VehicleEditorItem {
  std::string name;
  void* vehicle = nullptr;
};

// What the editor asks of the game.
struct VehicleEditorHooks {
  virtual ~VehicleEditorHooks() = default;
  // 0x004bb290 / 0x004bb200: zEntity's list ([0x0073655c]) in order, each
  // mech (vt+0x2c, vt+0x40, then vt+0x50), and each other vehicle whose
  // controls (vt+0x140) answer vt+0x54.
  virtual std::vector<VehicleEditorItem> vehicles() { return {}; }
  // vt+0x140 -> vt+0x90: the vehicle's brain's settings, null without one.
  virtual ai::AiParams* brain_params(void*) { return nullptr; }
  // The vehicle's team masks (+0xc0 team, +0xc4 allies, +0xc8 enemies).
  virtual void set_masks(void*, uint32_t, uint32_t, uint32_t) {}
  // [0x0060861c]+0x14's +0xc4 / +0xc8: the player's allies and enemies (0 with no player).
  virtual uint32_t player_allies() { return 0; }
  virtual uint32_t player_enemies() { return 0; }
  // [0x0060861c]+0x14: whether there is a player (the team's radios need one).
  virtual bool has_player() { return player_allies() || player_enemies(); }
  // Pause's check box: 0x0058fa80 (the app deactivated) or 0x0058faa0.
  virtual void pause(bool) {}
  // The vehicle's mech's stock and saved loadouts (vt+0x50, +0x684, 0x00432940:
  // its definition's +0x20), null when it is no mech.
  virtual const ConfigList* configs(void*) { return nullptr; }
  // Send home (0x004bc960): the brain lets go of the net (the controls'
  // vt+0x64, 0x004e9100), the vehicle is put at the place (vt+0xdc), the brain
  // takes the net again (vt+0x04, 0x004e9380).
  virtual void send_home(void*, ai::AiNet*, const float[3], float) {}
};
void set_vehicle_editor_hooks(VehicleEditorHooks* h);  // null: the defaults
VehicleEditorHooks& vehicle_editor_hooks();

// The main tab's pages in order (0x004b8790): p1 0x4e8, p2 0x548, Attack
// 0x5b4, Retreat 0x7f4, p5 0x854, Fire 0x8b4, Valve 0x914. Each is a dialog
// of its own (its controls), exchanged by its DoDataExchange (vt+0x8c).
enum VehiclePage { kPageP1, kPageP2, kPageAttack, kPageRetreat, kPageP5, kPageFire, kPageValve, kPages };

// The editor (0x988 bytes, 0x004b80f0; [0x0060b310]); the pages from
// Attack on come with S7e.
struct VehicleEditor {
  ai::AiParams params;          // +0x064  the settings shown (0x004d98c0), in the file's units
  ai::AivRecord made;           // +0x294  the record a load reads or makes (0x004e0680)
  std::vector<VehicleEditorItem> items;  // the combo 1239 (+0x140)
  int32_t selected = 0;         // +0x1c4
  int32_t pause = 0;            // +0x1c0  Pause's check box (1218)
  NetEditor* shown = nullptr;   // +0x1c8  Show's editor on the home net (0x004043c0: on the frame's list)
  int32_t show = 0;             // +0x1cc  Show (1249)
  uint8_t show_inout = 0;       // +0x1d1  Show InOut (1282)
  int32_t team_side = 0;        // +0x4dc  0 the player's ally, 1 enemy, 2 other (0x004bc260)
  int32_t keep[5] = {2, 2, 2, 2, 2};  // +0x974..+0x984: 2 "as the brain has it"
  Dialog main;                  // the dialog's own controls: Pause 1218
  Dialog pages[kPages];         // each page's controls
  int32_t page = -1;            // +0x4e0  the main tab's page shown (-1 none yet)
  int32_t tab = 0;              // the form's tab (the tab control's selection)
  // p2's own (0x548 +0x60, +0x64, +0x68): the pilot's name as typed, and the
  // two flag bits as check boxes.
  std::string p2_pilot;
  int32_t p2_objective = 0, p2_shutdown = 0;
  EditorForm form;
  EditorForm message;           // a failed field's box (AfxMessageBox)
};
VehicleEditor*& vehicle_editor();  // [0x0060b310]

void edit_vehicles(int32_t y);       // 0x004040c0 -> 0x004b86e0: made once and shown
void vehicle_editor_select(int32_t i);  // 0x004b8d70: the old one stored, the new one loaded
void vehicle_editor_apply();         // 0x004b8d40
void vehicle_editor_ok();            // 0x004b8d20: Apply, then closed
void vehicle_editor_refresh();       // 0x004b9f20 -> 0x004b9e40
void vehicle_editor_pause();         // 0x004bc3f0
void vehicle_editor_close();         // DestroyWindow -> 0x004b8b70 (PostNcDestroy)
void vehicle_editor_tab(int32_t page);  // the tab's TCN_SELCHANGE 0x004b8770 -> 0x004b7c80
// The dialog's UpdateData (0x004b8680: the team round trip when showing,
// then the page shown); false when one of its fields failed.
bool vehicle_editor_update(bool save);
// One page's UpdateData on its own (its DoDataExchange).
bool vehicle_page_update(int32_t page, bool save);
// p2's handlers (its message map 0x005c9538).
void vehicle_editor_team_left();     // 1111 EN_KILLFOCUS 0x004b9270
void vehicle_editor_team_radio();    // 1197..1199 BN_CLICKED 0x004b9290
void vehicle_editor_home_net();      // 1238 CBN_SELCHANGE 0x004b9f30
void vehicle_editor_net_edit();      // 1250 0x004b9f50: window 181
void vehicle_editor_send_home();     // 1255 0x004b9f70 -> 0x004bc960
void vehicle_editor_config();        // 1273 CBN_SELCHANGE 0x004b9fb0
void vehicle_editor_chassis();       // 1274 CBN_SELCHANGE 0x004b9fd0
// Tests: when set, each page fill and gather (0x004bc4e0..0x004bc930) adds
// its address as it runs.
std::vector<uint32_t>*& vehicle_editor_trace();
std::vector<int32_t>*& vehicle_editor_ids();  // tests: each control a page exchanges, in order
// The editor made with its pages and lists, no vehicle loaded, not shown
// (edit_vehicles goes on from here; the probes start here).
VehicleEditor* vehicle_editor_create();

}  // namespace mech3
