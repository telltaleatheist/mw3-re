#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "mech3/zrdraiv.h"
#include "upgrades/editor/form.h"

// The vehicle editor (EDIT VEHICLES 0x004040c0 and debug command 0x9d ->
// 0x004b86e0; docs/editor.md, The vehicle editor): the "Mech AI" dialog
// (template 165), a vehicle picked from the mission's and its AI settings
// edited on twelve pages, Apply putting them on its brain and in aiv.zrd's
// record cache. Original file unknown: it sits between weapon.cpp and
// zainetedit.cpp (0x004b7c80..0x004bcb70), with the tree view's window.
// FIX (A): the MFC dialog is a form (form.h) drawn by ImGui, its pages tabs.
namespace mech3 {

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
  // Pause's check box: 0x0058fa80 (the app deactivated) or 0x0058faa0.
  virtual void pause(bool) {}
};
void set_vehicle_editor_hooks(VehicleEditorHooks* h);  // null: the defaults
VehicleEditorHooks& vehicle_editor_hooks();

// The editor (0x988 bytes, 0x004b80f0; [0x0060b310]), as far as its shell
// keeps state; the pages' controls come with their sittings (S7d, S7e).
struct VehicleEditor {
  ai::AiParams params;          // +0x064  the settings shown (0x004d98c0), in the file's units
  ai::AivRecord made;           // +0x294  the record a load reads or makes (0x004e0680)
  std::vector<VehicleEditorItem> items;  // the combo 1239 (+0x140)
  int32_t selected = 0;         // +0x1c4
  int32_t pause = 0;            // +0x1c0  Pause's check box (1218)
  uint8_t pause_box = 0;        // the form's copy of it
  int32_t team_side = 0;        // +0x4dc  0 the player's ally, 1 enemy, 2 other (0x004bc260)
  int32_t keep[5] = {2, 2, 2, 2, 2};  // +0x974..+0x984: 2 "as the brain has it"
  int32_t tab = 0;              // the main tab's page
  EditorForm form;
};
VehicleEditor*& vehicle_editor();  // [0x0060b310]

void edit_vehicles(int32_t y);       // 0x004040c0 -> 0x004b86e0: made once and shown
void vehicle_editor_select(int32_t i);  // 0x004b8d70: the old one stored, the new one loaded
void vehicle_editor_apply();         // 0x004b8d40
void vehicle_editor_ok();            // 0x004b8d20: Apply, then closed
void vehicle_editor_refresh();       // 0x004b9f20 -> 0x004b9e40
void vehicle_editor_pause();         // 0x004bc3f0
void vehicle_editor_close();         // DestroyWindow -> 0x004b8b70 (PostNcDestroy)

}  // namespace mech3
