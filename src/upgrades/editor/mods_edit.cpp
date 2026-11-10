#include "upgrades/editor/mods_edit.h"

#include "mech3/app_hooks.h"
#include "mech3/app_keys.h"
#include "mech3/game_editor.h"
#include "mech3/mod_campaign.h"
#include "mech3/mods_menu.h"
#include "mech3/net_save.h"
#include "mech3/pilot.h"

namespace mech3 {

ModsEditSession& mods_edit_session() {
  static ModsEditSession s;
  return s;
}

void mods_edit_begin(const std::string& mod, std::function<void()> finish) {
  ModsEditSession& s = mods_edit_session();
  s = ModsEditSession{};
  s.active = true, s.mod = mod, s.finish = std::move(finish);
  ai::net_save_into_mod() = mod;
  editor_after_exit() = [] {
    if (mods_edit_session().active) mods_edit_session().leave = true;
  };
}

void mods_edit_frame() {
  ModsEditSession& s = mods_edit_session();
  if (!s.active || s.entered || s.leave) return;
  s.entered = true;
  if (!editor_state().active) editor_enter();
}

bool mods_edit_leave(Engine& en) {
  ModsEditSession& s = mods_edit_session();
  if (!s.active || !s.leave) return false;
  en.mission_cleanup(1);  // 0x00491780
  mods_edit_end();
  mods_menu_reopen() = true;
  en.replace_state(2);  // the main menu, which opens MODS again
  return true;
}

namespace {
// A custom campaign's mission: the current pilot's game is the entered
// campaign's (ModCampaignGame), so the campaign was entered from MODS.
const ModCampaign* campaign_mission() {
  const ModCampaign* c = entered_campaign();
  const Pilot* p = current_pilot();
  return c && p && dynamic_cast<const ModCampaignGame*>(p->current) ? c : nullptr;
}
}  // namespace

bool editor_reachable() { return mods_edit_session().active || campaign_mission() || app_key_calls().debug_camera; }

void editor_from_level() {
  if (!editor_reachable()) return;
  if (!mods_edit_session().active) {
    const ModCampaign* c = campaign_mission();
    ai::net_save_into_mod() = c ? c->mod : std::string();
  }
  editor_key(kEditorCommand);
}

void mods_edit_end() {
  ModsEditSession& s = mods_edit_session();
  if (!s.active) return;
  if (s.finish) s.finish();
  ai::net_save_into_mod().clear();
  editor_after_exit() = nullptr;
  s = ModsEditSession{};
}

}  // namespace mech3
