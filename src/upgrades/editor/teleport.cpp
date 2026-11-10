#include "upgrades/editor/teleport.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/app_mech.h"
#include "mech3/app_vehicle.h"
#include "mech3/game_editor_net.h"
#include "mech3/mech_waynet.h"
#include "mech3/net_save.h"
#include "mech3/teleport.h"
#include "upgrades/editor/form.h"

namespace mech3 {

namespace {
namespace fs = std::filesystem;

// The game's side: the player, its move, the mission's nav points; the
// questions as the editor's own forms (an answer comes later, not at once).
struct Wired : TeleportHooks {
  bool player_place(float pos[3]) override {
    void* me = mech_mission_player();
    return me && mech_world_position(me, pos);
  }
  void move_player(const float pos[3], float yaw) override {
    if (void* me = mech_mission_player()) move_mech(me, pos, yaw);  // vt+0xdc
  }
  std::vector<TeleportSpot> navs() override {
    std::vector<TeleportSpot> out;
    auto* w = static_cast<Waynet*>(program_mission_waynet());
    for (int32_t i = 0; w && i < w->count(); ++i) {
      const auto* p = dynamic_cast<const WaynetPoint*>(w->elem(i));
      const char* label = p ? p->label_or_null() : nullptr;
      if (!label) continue;  // FIX (bug): the exe copies from null (teleport.h)
      TeleportSpot s;
      for (int k = 0; k < 3; ++k) s.pos[k] = p->pos[k];
      s.name = label;
      out.push_back(s);
    }
    return out;
  }
  int32_t message_box(int32_t style, int32_t caption, int32_t text, const std::string& arg) override;
};
Wired g_wired;

struct State {
  std::vector<TeleportSpot> spots;
  TeleportDialog dialog;
  std::string world;  // the folder of the archive location.zrd came from ("c1"), "" none
  EditorForm form, confirm;
  std::string note;
};
State g;
std::string g_saved;

void show_note(const std::string& t) {
  g.note = t;
  if (!g.form.fields.empty()) g.form.fields.back().label = t;
}

// The questions: Delete's (style 4) as a Yes / No form, the empty name's (0x30) on the form.
int32_t Wired::message_box(int32_t style, int32_t caption, int32_t text, const std::string& arg) {
  (void)caption;
  const std::string said = zutl::format_message(text, arg.c_str());
  if (style != 4) {
    show_note(said.empty() ? "Type a name for the place first" : said);
    return 1;
  }
  g.confirm = EditorForm{};
  g.confirm.title = "Teleport##confirm";
  g.confirm.modal = true;
  g.confirm.fields = {{said.empty() ? "Delete " + arg + "?" : said, FormField::kLabel}};
  g.confirm.buttons = {{"Yes", [] { form_hide(&g.confirm), g.dialog.remove_picked(); }}, {"No", [] { form_hide(&g.confirm); }}};
  form_show(&g.confirm);
  return 0;  // answered by the form
}

// The archive holding location.zrd: its folder's name (the world's).
std::string location_world() {
  for (zutl_zar::Archive* a : zrdr::state().archives)
    if (a && a->path && zutl_zar::find(*a, "location.zrd") >= 0) {
      std::string p = a->path;  // the game's path ("zbd\\c1\\reader.zbd") or the host's
      for (char& c : p) c = c == '\\' ? '/' : c;
      return fs::path(p).parent_path().filename().string();
    }
  return {};
}

// Cancel only closes, as Pirate's Moon's 0x004a8a40 has it (IDOK only);
// MW3's moved and saved on Cancel too.
void close() {
  form_hide(&g.confirm);
  form_hide(&g.form);
}

// 0x004a37a0 after the dialog: the player moved there; with the dialog's
// changed flag, the spots written back.
void ok() {
  float pos[3], yaw = 0;
  int32_t changed = 0;
  if (!teleport_dialog_ok(g.dialog, pos, &yaw, &changed)) return show_note(ddx_message().text);
  teleport_hooks().move_player(pos, yaw);
  std::string line = "moved to " + g.dialog.dlg.at(TeleportDialog::kList).text;
  if (changed) {
    // Ours: into the mod, as the editor's other saves, loose beside the
    // world's archive (read ahead of its entry). The exe wrote beside the
    // file it read and renamed the old one to location.bak without its
    // folder, into the working directory. FIX (bug): the .bak beside it.
    const std::string& mod = ai::net_save_into_mod();
    if (mod.empty() || g.world.empty() || zutl::mounts().write.empty()) {
      line += "; the places not saved (open the mission from MODS)";
    } else {
      const fs::path to = fs::path(zutl::mounts().write) / "mods" / mod / "zbd" / g.world / "location.zrd";
      std::error_code ec;
      fs::create_directories(to.parent_path(), ec);
      if (fs::exists(to, ec)) fs::rename(to, to.string() + ".bak", ec);
      std::ofstream(to, std::ios::binary | std::ios::trunc) << location_zrd_text(g.spots);
      g_saved = to.string();
      line += "; the places saved to mods\\" + mod + "\\zbd\\" + g.world + "\\location.zrd";
    }
  }
  close();
  editor_status(line);
}

void build() {
  TeleportDialog& d = g.dialog;
  g.form = EditorForm{};
  g.form.title = "Teleport";
  DlgControl& list = d.dlg.at(TeleportDialog::kList);
  FormField places{"place", FormField::kChoice};
  places.combo = &list.combo, places.i = &list.combo.cur, places.text = &list.text;
  places.changed = [] { g.dialog.pick(); };
  places.left = [] { g.dialog.left_list(); };
  g.form.fields.push_back(places);
  const auto edit = [&](const char* label, int32_t id) {
    FormField f{label, FormField::kText};
    f.text = &d.dlg.at(id).text;
    f.changed = [] { g.dialog.changed_text(); };
    return f;
  };
  g.form.fields.push_back(edit("x", TeleportDialog::kX));
  g.form.fields.push_back(edit("y", TeleportDialog::kY));
  g.form.fields.push_back(edit("z", TeleportDialog::kZ));
  g.form.fields.push_back(edit("yaw (deg.)", TeleportDialog::kYaw));
  g.form.fields.push_back({g.note, FormField::kLabel});  // last: what the dialog said
  g.form.buttons = {{"OK", ok}, {"Cancel", close}, {"Delete", [] { g.dialog.remove(); }}};
}
}  // namespace

// Ours: the TELEPORT row. 0x004a37a0 to its DoModal: the player's place,
// the list (Current Location, the nav points, location.zrd's), the dialog.
// FIX (bug): without a location.zrd the exe stops with an error box; here
// the list is the player's and the nav points (a mod's new world has none).
void teleport_open(int32_t) {
  if (teleport_hooks_default()) set_teleport_hooks(&g_wired);
  float pos[3] = {};
  if (!teleport_hooks().player_place(pos)) return editor_status("Teleport: no player to move");
  g = State{};
  zrdr::Node* location = zrdr::load("location.zrd");
  g.world = location ? location_world() : std::string();
  g.spots = teleport_spots(pos, location);
  if (location) zrdr::free_node(location);
  teleport_dialog_begin(g.dialog, &g.spots, pos, 0.0f);  // the exe passes yaw 0
  build();
  form_show(&g.form);
}

EditorForm& teleport_form() { return g.form; }
TeleportDialog& teleport_dialog() { return g.dialog; }
const std::string& teleport_last_saved() { return g_saved; }

}  // namespace mech3
