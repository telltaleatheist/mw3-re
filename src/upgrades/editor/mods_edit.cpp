#include "upgrades/editor/mods_edit.h"

#include "mech3/app_hooks.h"
#include "mech3/app_keys.h"
#include <cstdio>
#include <cstdlib>

#include "mech3/game_editor.h"
#include "mech3/game_editor_net.h"
#include "mech3/mod_campaign.h"
#include "mech3/mods_menu.h"
#include "mech3/net_save.h"
#include "mech3/pilot.h"
#include "upgrades/editor/save_as.h"
#include "upgrades/mods/entry_run.h"

namespace mech3 {

ModsEditSession& mods_edit_session() {
  static ModsEditSession s;
  return s;
}

void mods_edit_begin(const std::string& mod, std::function<void()> finish, bool editor) {
  ModsEditSession& s = mods_edit_session();
  s = ModsEditSession{};
  s.active = true, s.mod = mod, s.finish = std::move(finish), s.editor = editor;
  mods_menu_reopen() = true;  // back to MODS at the main menu, however the mission is left
  ai::net_save_into_mod() = mod;
  // Inside a campaign entry the saves go into a copy of it (save_as.h): the entry's own mod when that is a copy
  // of ours, else none yet (the first save asks its name).
  if (!entry_run().id.empty() && !mods_menu_write_dir().empty()) {
    save_as_begin(mods_menu_write_dir(), entry_run().id);
    s.mod = ai::net_save_into_mod();
  }
}

namespace {
const ModCampaign* campaign_mission();
constexpr char kHintWords[] = "PAUSE: EDIT MISSION, or SHIFT+F12";
constexpr float kHintSeconds = 8.0f;
struct Hint {
  ztwod::Layer layer;
  ztwod::FontText text{"", 0, 0};
  float left = 0.0f;
  bool shown = false;
};
Hint* g_hint = nullptr;
void hint_frame() {
  if (!g_hint) return;
  Hint& h = *g_hint;
  const bool show = h.left > 0.0f && !editor_state().active;
  if (show != h.shown) {
    h.shown = show;
    if (show) h.text.show_for(-1.0f);
    else h.text.hide();
    h.layer.set_active(show ? 1 : 0);
  }
  if (!show) return;
  h.left -= editor_hooks().frame_dt();
  h.layer.update(editor_hooks().frame_dt());
}
}  // namespace

void mods_edit_mission_started() {
  delete g_hint;
  g_hint = nullptr;
  if (!mods_edit_session().active && !campaign_mission()) return;
  g_hint = new Hint;
  if (const ztwod::Handle f = editor_hooks().font("mech_string")) g_hint->text.set_font(f);
  g_hint->text.set_pos(5, 140);  // under the radar and the objective line, the editor panel's column
  g_hint->text.set_text("%s", kHintWords);
  g_hint->text.hide();
  g_hint->layer.add(&g_hint->text);
  g_hint->left = kHintSeconds;
}

const char* mods_edit_hint() { return g_hint && g_hint->shown ? kHintWords : nullptr; }

void mods_edit_frame() {
  hint_frame();
  ModsEditSession& s = mods_edit_session();
  // MW3_EDIT_SAVE_AT=N (checks only): at the editor's Nth frame net_01 is opened and saved, the Save as box
  // answered with the name it offers, and the editor left; ImGui's box cannot be pressed by a scripted click.
  static int frames = 0;
  if (const char* at = s.active && s.entered && s.editor ? std::getenv("MW3_EDIT_SAVE_AT") : nullptr; at && ++frames == std::atoi(at)) {
    save_as_ask_hook() = [](std::string* name) { return std::printf("mw3: Save as answered with \"%s\" (MW3_EDIT_SAVE_AT)\n", name->c_str()), true; };
    edit_ai_nets(0);
    NetDialog& d = net_dialog();
    for (size_t i = 0; i < d.items.size(); ++i)
      if (d.items[i].first == "net_01") d.selected = static_cast<int32_t>(i);
    net_dialog_close();
    net_editor_save(0);
    std::printf("mw3: editor: %s\n", editor_panel().rows[24]->text);
    save_as_ask_hook() = nullptr;
    net_editor_exit(0);
    editor_exit(0);
    return;
  }
  if (!s.active || s.entered || !s.editor) return;
  s.entered = true;
  if (!editor_state().active) editor_enter();
}

namespace {
// A campaign's mission played from MODS: the current pilot's game is the entered
// campaign's (ModCampaignGame), so the campaign was entered from MODS.
const ModCampaign* campaign_mission() {
  const ModCampaign* c = entered_campaign();
  const Pilot* p = current_pilot();
  return c && p && dynamic_cast<const ModCampaignGame*>(p->current) ? c : nullptr;
}
}  // namespace

bool editor_switch_edits() { return !editor_state().active; }

bool editor_reachable() { return mods_edit_session().active || campaign_mission() || app_key_calls().debug_camera; }

void editor_from_level() {
  if (!editor_reachable()) return;
  if (!mods_edit_session().active) {
    const ModCampaign* c = campaign_mission();
    ai::net_save_into_mod() = c ? c->mod : std::string();
    // Inside an entry: into a copy of it (save_as.h), never the campaign's own mod.
    if (!entry_run().id.empty() && !mods_menu_write_dir().empty() && save_as_session().entry.empty())
      save_as_begin(mods_menu_write_dir(), entry_run().id);
    else if (!save_as_session().entry.empty())
      ai::net_save_into_mod() = save_as_session().copy;
  }
  editor_key(kEditorCommand);
}

void mods_edit_end() {
  ModsEditSession& s = mods_edit_session();
  // The copy's session ends with the mission however the editor was reached (EDIT's, or Shift+F12 in a campaign).
  if (save_as_session().made) entry_run().choose = save_as_session().copy;  // MODS opens on the new copy
  if (!save_as_session().entry.empty()) save_as_end(), ai::net_save_into_mod().clear();
  if (!s.active) return;
  if (s.finish) s.finish();
  ai::net_save_into_mod().clear();
  s = ModsEditSession{};
}

}  // namespace mech3
