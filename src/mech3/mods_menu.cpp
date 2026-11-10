#include "mech3/mods_menu.h"

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <cstdio>
#include <cstdlib>

#include "crt/msvc_string.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/mainmenu.h"
#include "mech3/mod_campaign.h"
#include "mech3/mission.h"
#include "mech3/mod_install.h"
#include "upgrades/mods/mod_report.h"
#include "upgrades/mods/mod_export.h"

namespace mech3 {

// ---- the tables ----

ModsTables mods_tables(const std::string& write_dir, const std::vector<std::string>& enabled,
                       const std::vector<std::string>& mounted) {
  ModsTables t;
  for (ModsListEntry& m : mods_list(write_dir, enabled)) {
    const ModInfo& i = m.info;
    for (const std::string& file : i.campaigns) {
      ModsCampaignRow r;
      r.mod = i.folder, r.file = file, r.author = i.author, r.version = i.version;
      r.mounted = std::find(mounted.begin(), mounted.end(), i.folder) != mounted.end();
      ModCampaign c;
      if (!i.error.empty()) r.error = i.error;
      else if (!campaign_load(i.path, i.folder, file, &c)) r.error = c.error;
      r.name = c.name.empty() ? file : c.name;
      t.campaigns.push_back(std::move(r));
    }
    // A campaign's mod is entered from its row; one that also adds mechs,
    // weapons or Instant Action worlds is a mod as well.
    if (i.campaigns.empty() || !i.mechs.empty() || !i.weapons.empty() || !i.ia_worlds.empty())
      t.mods.push_back(std::move(m));
  }
  return t;
}

std::string mods_row_text(const ModsListEntry& m) {
  const std::string& name = m.info.name.empty() ? m.info.folder : m.info.name;
  const bool flagged = !m.info.error.empty() || !m.conflicts_with.empty() || (m.on && !m.needs.empty());
  return std::string(m.on ? "[x] " : "[ ] ") + name + (flagged ? " !" : "");
}

std::string mods_row_note(const ModsListEntry& m, const std::vector<ModsListEntry>& all) {
  if (!m.info.error.empty()) return "Cannot be used: " + m.info.error;
  // Ours (mod_needs.h): what it needs first, an off mod's as what turning it on would warn of.
  std::string needs;
  for (const std::string& w : m.needs) needs += (needs.empty() ? "" : " ") + w;
  if (!needs.empty() && !m.on) needs = "Turned on: " + needs;
  const std::string rest = mods_row_note_shared(m, all);
  return needs.empty() ? rest : rest.empty() ? needs : needs + " " + rest;
}

// The rest of a row's note: what it shares, the animation files not read.
std::string mods_row_note_shared(const ModsListEntry& m, const std::vector<ModsListEntry>& all) {
  const std::string unread = m.anims_unread > 0 ? std::to_string(m.anims_unread) + (m.anims_unread == 1 ? " animation file" : " animation files") +
                                                      " Pirate's Moon does not read"
                                                : std::string();
  if (m.conflicts_with.empty()) return unread;
  std::string names;
  for (const std::string& f : m.conflicts_with) {
    std::string n = f;
    for (const ModsListEntry& o : all)
      if (o.info.folder == f && !o.info.name.empty()) n = o.info.name;
    names += (names.empty() ? "" : ", ") + n;
  }
  const size_t files = m.shadowed.size(), rows = m.rows_taken.size();
  std::string what;
  if (files) what = std::to_string(files) + (files == 1 ? " file" : " files");
  if (rows) what += (what.empty() ? "" : " and ") + std::to_string(rows) + (rows == 1 ? " mech/weapon" : " mechs/weapons");
  const size_t anims = m.anims_taken.size();
  if (anims) what += (what.empty() ? "" : " and ") + std::to_string(anims) + (anims == 1 ? " animation" : " animations");
  std::string bases;
  for (const std::string& w : m.anim_bases) bases += (bases.empty() ? "" : ", ") + w;
  std::string note;
  if (what.empty()) note = "Its animations only add to " + bases + ": " + names + " has the whole world's";
  else if (!bases.empty()) note = "Shares " + what + " with " + names + ", which wins; its animations only add to " + bases;
  else note = "Shares " + what + " with " + names + ", which wins";
  return unread.empty() ? note : note + "; " + unread;
}

std::string mods_campaign_row_text(const ModsCampaignRow& c) {
  return c.name + (c.mounted && c.error.empty() ? "" : " !");
}

std::string mods_campaign_details(const ModsCampaignRow& c) {
  std::string s = c.author.empty() ? std::string("unknown author") : c.author;
  if (!c.version.empty()) s += ", version " + c.version;
  return s;
}

void mods_toggle(std::vector<std::string>& enabled, const std::string& folder) {
  const auto it = std::find(enabled.begin(), enabled.end(), folder);
  if (it != enabled.end()) enabled.erase(it);
  else enabled.push_back(folder);
}

bool mods_move(std::vector<std::string>& enabled, const std::string& folder, int32_t by) {
  const auto it = std::find(enabled.begin(), enabled.end(), folder);
  if (it == enabled.end()) return false;
  const int64_t at = it - enabled.begin(), to = at + by;
  if (to < 0 || to >= static_cast<int64_t>(enabled.size())) return false;
  std::swap(enabled[static_cast<size_t>(at)], enabled[static_cast<size_t>(to)]);
  return true;
}

// ---- the screen ----

bool& mods_menu_page() {
  static bool on = false;
  return on;
}

Handle& mods_menu_state() {
  static Handle h = 0;
  return h;
}

std::string& mods_menu_write_dir() {
  static std::string d;
  return d;
}

std::vector<std::string>& mods_menu_mounted() {
  static std::vector<std::string> m;
  return m;
}

std::string& mods_menu_install() {
  static std::string d;
  return d;
}

ModsMenuPlatform& mods_menu_platform() {
  static ModsMenuPlatform p;
  return p;
}

ModsEditStart& mods_menu_edit_start() {
  static ModsEditStart f;
  return f;
}
ModsEditStart& mods_menu_test_start() {
  static ModsEditStart f;
  return f;
}
bool& mods_menu_reopen() {
  static bool on = false;
  return on;
}

std::vector<ModsEditRow> mods_edit_rows(const ModInfo& m) {
  std::vector<ModsEditRow> rows;
  for (const ModCampaign& c : mod_campaigns())
    if (c.mod == m.folder)
      for (const CampaignMission& k : c.missions) {
        bool seen = false;
        for (const ModsEditRow& r : rows) seen = seen || (r.world == k.world && r.number == k.number);
        if (!seen) rows.push_back({k.world, k.number});
      }
  if (!rows.empty()) return rows;
  namespace fs = std::filesystem;
  for (const std::string& w : m.worlds) {
    std::error_code ec;
    std::vector<int32_t> found;
    for (const auto& f : fs::directory_iterator(fs::path(m.path) / "zbd" / w, ec)) {
      const std::string n = f.path().filename().string();
      int32_t k = 0;
      char tail[8] = {};
      if (std::sscanf(n.c_str(), "readerm%d%7s", &k, tail) == 2 && std::string(tail) == ".zbd" && k > 0) found.push_back(k);
    }
    std::sort(found.begin(), found.end());
    for (int32_t k : found) rows.push_back({w, k});
  }
  return rows;
}

std::vector<ModsEditRow> mods_test_rows(const ModInfo& m, const std::string& install) {
  std::vector<ModsEditRow> rows = mods_edit_rows(m);
  const size_t own = rows.size();
  namespace fs = std::filesystem;
  for (int32_t w = 0; w < 9; ++w) {  // the game's nine worlds (0x00484c50); a mod's come after
    std::error_code ec;
    std::vector<int32_t> found;
    for (const auto& f : fs::directory_iterator(fs::path(install) / "zbd" / mission_dir(w), ec)) {
      const std::string n = f.path().filename().string();
      int32_t k = 0;
      char tail[8] = {};
      if (std::sscanf(n.c_str(), "readerm%d%7s", &k, tail) == 2 && std::string(tail) == ".zbd" && k > 0) found.push_back(k);
    }
    std::sort(found.begin(), found.end());
    for (int32_t k : found) {
      bool seen = false;
      for (const ModsEditRow& r : rows) seen = seen || (crt::msvc_stricmp(r.world.c_str(), mission_dir(w)) == 0 && r.number == k);
      if (!seen) rows.push_back({mission_dir(w), k});
    }
  }
  // Pirate's Moon: its missions are numbered across the worlds (c1 1, 2, 5;
  // c2 3, 4, 8; c3 6, 7; c4 9, 10), so the game's are listed in play order.
  std::stable_sort(rows.begin() + static_cast<std::ptrdiff_t>(own), rows.end(),
                   [](const ModsEditRow& a, const ModsEditRow& b) { return a.number < b.number; });
  return rows;
}

namespace {
struct Picked {
  std::mutex m;
  bool answered = false;
  std::string path;
};
Picked& picked() {
  static Picked p;
  return p;
}
}  // namespace

void mods_menu_picked(const std::string& path) {
  Picked& p = picked();
  std::lock_guard<std::mutex> g(p.m);
  p.answered = true, p.path = path;
}

void ModsList::select(int32_t index) {
  FileList::select(index);
  notify(index, 0);
}

namespace {
constexpr size_t kReportWidth = 34;  // REPORT's characters a row in the campaigns' box (main_darkgreen)
// The render target from zVideo's buffers (0x00558290), as the other menus'.
void target_buffers(Engine& e) {
  const Handle a = e.video_back_buffer();
  const Handle b = e.video_buffer_da0();
  const Handle r = e.video_buffer_d90();
  e.render_set_target2(e.video_target(), r, b, a);
}
// Ours (Owen, 2026-11-09): the pilot screen's lines (pilot_bg_lines, 619x445)
// with the pilots' box of six bars (24,113 to 256,437) taken out and a copy
// of its right box (the games' plain one, 378,33, 241x210) put in its place,
// the CAMPAIGNS list's box lined up with MODS'. The same size and place, so
// the wide layout stretches it as the pilot screen's. Null when it cannot.
zimg::Texture* campaign_lines(zimg::Texture* src) {
  constexpr int kBoxX = 378, kBoxY = 33, kBoxW = 241, kBoxH = 210, kToX = 24, kToY = 113, kClearW = 265;
  if (!src || !src->pixels || !src->alpha || src->palette_count || src->width < kBoxX + kBoxW ||
      src->height < kToY + kBoxH)
    return nullptr;
  const int w = src->width, h = src->height;
  zimg::Texture* dst = ztwod::host().image_new(w, h);
  if (!dst) return nullptr;
  dst->flags = (src->flags & ~(zimg::kAlphaOwned | zimg::kPaletteOwned)) | zimg::kPixelsOwned | zimg::kAlphaOwned;
  dst->alpha = static_cast<uint8_t*>(std::calloc(static_cast<size_t>(w) * h, 1));
  auto* in = static_cast<const uint16_t*>(src->pixels);
  auto* out = static_cast<uint16_t*>(dst->pixels);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const bool cleared = x < kClearW && y >= kToY;
      out[y * w + x] = cleared ? 0 : in[y * w + x];
      dst->alpha[y * w + x] = cleared ? 0 : src->alpha[y * w + x];
    }
  for (int y = 0; y < kBoxH; ++y)
    for (int x = 0; x < kBoxW; ++x) {
      out[(kToY + y) * w + kToX + x] = in[(kBoxY + y) * w + kBoxX + x];
      dst->alpha[(kToY + y) * w + kToX + x] = src->alpha[(kBoxY + y) * w + kBoxX + x];
    }
  return dst;
}

void put(ztwod::Text& t, const char* font, int32_t x, int32_t y) {
  t.set_font(ztwod::host().font_find(font));
  t.set_pos(x, y);
}
void say(ztwod::Text& t, const std::string& s) {
  t.set_text("%s", s.c_str());
  if (t.needs_render) t.render();
}
}  // namespace

int32_t ModsSlideButton::hit(int32_t px, int32_t py) {
  if (!get_rect()) return 0;  // off, or being read, as SlideButton's
  const zimg::Texture* img = stashed ? stashed : image;  // baked: the image it had
  if (img && px >= x && px < x + img->width && py >= y && py < y + img->height) {
    // circlebtn_small is 82x82, the ring and knob in its middle and a faint halo
    // (alpha under 64) filling the rest: the ring and knob are the button.
    if (!img->alpha) return 1;
    if (img->alpha[(py - y) * img->width + (px - x)] >= 64) return 1;
  }
  for (ztwod::Text* t : labels) {  // its label's own box, as Control::hit
    if (t->needs_render) t->render();
    const int32_t left = t->align == 1 ? t->get_x() - t->width / 2 : t->align == 2 ? t->get_x() - t->width : t->get_x();
    if (px >= left && px <= left + t->width && py >= t->get_y() && py <= t->get_y() + t->text_height()) return 1;
  }
  return 0;
}

ModsScreen::ModsScreen(Engine& e, Handle s)
    : MenuScreen(e, s),
      enter(e, kEnter),
      back(e, kBack),
      toggle(e, kToggle),
      up(e, kUp),
      down(e, kDown),
      editor(e, kEditor),
      test_btn(e, kTest),
      add_btn(e, kAdd),
      export_btn(e, kExport),
      add_folder_btn(e, kAddFolder),
      folder_btn(e, kOpenFolder),
      report_btn(e, kReport),
      campaign_list(e, kCampaigns),
      mod_list(e, kMods) {
  target_buffers(e);
  const bool moving = !e.fast_menu();
  // Made when MODS is pressed, past the main menu's read: dialog.zrd read again.
  if (load("dialog.zrd", "MODSDIALOG", moving)) {
    zimg::Texture* img = nullptr;
    uint8_t bg_moving = 0;
    e.main_menu_background(&img, &bg_moving);
    background.set_image(img);
    background.moving = bg_moving;
    if (!bg_moving) {
      background.set_pos(0, 0);
      background.src = &background.view;
    }
    add(&background);
    bind_primitive(&lines, "LINES");
    bind_primitive(&lines2, "LINES2");
    if ((own_lines = campaign_lines(lines2.image))) lines2.set_image(own_lines);
    bind_button(&enter, "ENTER");
    bind_button(&back, "BACK");
    if (!moving) {
      bake();
      remove(&background);
      remove(&lines);
      remove(&lines2);
      enter.set_baked(true);
      back.set_baked(true);
    }
    bind_button(&campaign_list, "CAMPAIGNLIST");
    bind_button(&mod_list, "MODLIST");
    bind_button(&toggle, "TOGGLE");
    bind_button(&up, "MOVEUP");
    bind_button(&down, "MOVEDOWN");
    bind_button(&editor, "EDITOR");
    bind_button(&test_btn, "TESTMOD");
    bind_button(&add_btn, "ADDMOD");
    bind_button(&export_btn, "EXPORTMOD");
    bind_button(&add_folder_btn, "ADDFOLDER");
    bind_button(&folder_btn, "MODSFOLDER");
    bind_button(&report_btn, "REPORTMOD");
    unload();
  }
  // The details in PILOT ENTRY's box, the notes under the MODS table.
  put(detail_label, "main_red", 71, 21);
  put(detail, "main_green", 81, 38);
  put(detail2, "ia_dark_grey", 81, 54);
  put(note, "ia_dark_grey", 384, 266);
  put(restart, "ia_dark_grey", 384, 279);
  say(detail_label, "D E T A I L S");
  add(&detail_label), add(&detail), add(&detail2), add(&note), add(&restart);
  set_flags_all(0);
  set_active(1);
  enabled = mods_enabled(mods_menu_write_dir());
  refill();
}

ModsScreen::~ModsScreen() {
  if (own_lines) {
    lines2.set_image(static_cast<zimg::Texture*>(nullptr));
    ztwod::host().image_release(own_lines);
  }
}

void ModsScreen::refill() {
  tables = mods_tables(mods_menu_write_dir(), enabled, mods_menu_mounted());
  campaign_list.clear_items();
  for (size_t i = 0; i < tables.campaigns.size(); ++i)
    campaign_list.add_item(mods_campaign_row_text(tables.campaigns[i]).c_str(), static_cast<int32_t>(i));
  const int32_t was = mod_list.selected;
  mod_list.clear_items();
  for (size_t i = 0; i < tables.mods.size(); ++i)
    mod_list.add_item(mods_row_text(tables.mods[i]).c_str(), static_cast<int32_t>(i));
  campaign_list.layout(), mod_list.layout();
  if (focus == kMods && was >= 0 && was < mod_list.item_count()) mod_list.FileList::select(was);
  else if (campaign_list.item_count() && campaign_list.selected < 0) campaign_list.FileList::select(0);
  say(restart, enabled == mods_menu_mounted() ? "" : "Mod changes take hold at the next start.");
  show_details();
}

void ModsScreen::show_details() {
  if (reporting) return;  // REPORT's lines stay until REPORT again
  std::string d, d2, n;
  if (focus == kCampaigns && campaign_list.selected >= 0 &&
      campaign_list.selected < static_cast<int32_t>(tables.campaigns.size())) {
    const ModsCampaignRow& c = tables.campaigns[static_cast<size_t>(campaign_list.selected)];
    d = c.name, d2 = mods_campaign_details(c);
    if (!c.error.empty()) n = "Cannot be played: " + c.error;
    else if (!c.mounted) n = "Its mod is off: ENTER turns it on for the next start.";
  } else if (focus == kMods && mod_list.selected >= 0 && mod_list.selected < static_cast<int32_t>(tables.mods.size())) {
    const ModsListEntry& m = tables.mods[static_cast<size_t>(mod_list.selected)];
    d = m.info.name.empty() ? m.info.folder : m.info.name;
    d2 = (m.info.author.empty() ? std::string("unknown author") : m.info.author) +
         (m.info.version.empty() ? "" : ", version " + m.info.version);
    n = mods_row_note(m, tables.mods);
  }
  say(detail, d);
  say(detail2, d2);
  say(note, n);
}

void ModsScreen::on_button(int32_t id, int32_t a, int32_t) {
  const std::string& dir = mods_menu_write_dir();
  auto chosen_mod = [&]() -> const ModsListEntry* {
    const int32_t i = mod_list.selected;
    return i >= 0 && i < static_cast<int32_t>(tables.mods.size()) ? &tables.mods[static_cast<size_t>(i)] : nullptr;
  };
  switch (id) {
    case kCampaigns:
    case kMods:
      focus = id;
      (void)a;
      show_details();
      return;
    case kToggle:
    case kUp:
    case kDown:
      if (const ModsListEntry* m = chosen_mod()) {
        const std::string folder = m->info.folder;
        const bool turning_on = id == kToggle && std::find(enabled.begin(), enabled.end(), folder) == enabled.end();
        if (id == kToggle) mods_toggle(enabled, folder);
        else mods_move(enabled, folder, id == kUp ? -1 : 1);
        mods_save_enabled(dir, enabled);
        refill();
        for (int32_t i = 0; i < mod_list.item_count(); ++i)
          if (tables.mods[static_cast<size_t>(i)].info.folder == folder) mod_list.FileList::select(i);
        focus = kMods;
        show_details();
        // Ours (mod_needs.h): turned on all the same, its warnings said.
        for (const ModsListEntry& e : tables.mods)
          if (turning_on && e.info.folder == folder && !e.needs.empty())
            tell(e.needs[0] + (e.needs.size() > 1 ? " (" + std::to_string(e.needs.size() - 1) + " more: REPORT)" : ""));
      }
      return;
    case kEnter:
      if (reporting) return;
      if (editing) edit_start();
      else enter_campaign();
      return;
    case kAdd:
    case kAddFolder:
      if (!mods_menu_platform().pick) {
        tell("Adding a mod is not possible here: copy it into the mods folder.");
        return;
      }
      tell(id == kAdd ? "Pick the mod's archive (.zip, .7z, .rar)." : "Pick the mod's folder.");
      export_folder.clear();
      mods_menu_platform().pick(id == kAddFolder);
      return;
    case kExport: {
      // The selected row's mod, a campaign's or a mod's.
      std::string folder;
      if (focus == kMods) {
        if (const ModsListEntry* m = chosen_mod()) folder = m->info.folder;
      } else if (campaign_list.selected >= 0 && campaign_list.selected < static_cast<int32_t>(tables.campaigns.size())) {
        folder = tables.campaigns[static_cast<size_t>(campaign_list.selected)].mod;
      }
      if (folder.empty()) {
        tell("Choose a mod or a campaign to export.");
        return;
      }
      if (!mods_menu_platform().save) {
        tell("Exporting is not possible here: zip the mod's folder in the mods folder.");
        return;
      }
      ModInfo info;
      info.folder = folder;
      for (const ModInfo& i : mods_found(dir))
        if (i.folder == folder) info = i;
      tell("Choose where to save " + (info.name.empty() ? folder : info.name) + ".");
      export_folder = folder;
      mods_menu_platform().save(mod_export_file_name(info));
      return;
    }
    case kOpenFolder: {
      std::error_code ec;
      const std::filesystem::path m = std::filesystem::path(dir) / "mods";
      std::filesystem::create_directories(m, ec);
      if (!mods_menu_platform().open_folder || !mods_menu_platform().open_folder(m.string()))
        tell("The mods folder: " + m.string());
      return;
    }
    case kEditor:
      if (reporting) report();
      edit();
      return;
    case kReport:
      if (editing) missions(testing);  // EDITOR's or TEST's list closed first
      report();
      return;
    case kTest:
      if (reporting) report();
      test();
      return;
    case kBack:
      if (reporting) {
        report();  // back to the tables first
        return;
      }
      if (editing) {
        missions(testing);  // back to the tables first
        return;
      }
      state_escape();
      return;
    default:
      return;
  }
}

// ENTER: the chosen campaign entered (its own pilots), MODS let go and the
// Campaign State pushed on the main menu, as its CAMPAIGN (the main menu
// shown again lets the campaign go: MainState::update); a campaign whose mod is not on in this
// run is never started half-mounted: its mod is turned on for the next start.
void ModsScreen::enter_campaign() {
  const int32_t i = campaign_list.selected;
  if (focus != kCampaigns || i < 0 || i >= static_cast<int32_t>(tables.campaigns.size())) {
    say(note, "Choose a custom campaign to enter.");
    return;
  }
  const ModsCampaignRow row = tables.campaigns[static_cast<size_t>(i)];
  if (!row.error.empty()) {
    say(note, "Cannot be played: " + row.error);
    return;
  }
  if (!row.mounted) {
    if (std::find(enabled.begin(), enabled.end(), row.mod) == enabled.end()) {
      mods_toggle(enabled, row.mod);
      mods_save_enabled(mods_menu_write_dir(), enabled);
      refill();
    }
    say(note, row.name + ": its mod is turned on; restart to play it.");
    return;
  }
  const ModCampaign* found = nullptr;
  for (const ModCampaign& c : mod_campaigns())
    if (c.mod == row.mod && c.file == row.file) found = &c;
  std::string why = "a mission's world is missing";
  if (!found || !campaign_enter(*found, &why)) {
    say(note, "Cannot be played: " + why);
    return;
  }
  // The Campaign State on the main menu, as from CAMPAIGN: once a pilot is
  // chosen it pops itself and replaces the state below with the mission's,
  // which must be the main menu's (nothing left under a mission).
  Engine& e = engine();
  e.pop_state(0);
  e.push_state(e.peek32(menu_global(MainMenuButton::kNewCampaign)), 0);
}

// REPORT (ours; modding.md, Plain errors on the MODS screen): the chosen
// mod's check and this start's mount as sentences in the campaigns' box
// (mod_report.h), wrapped to its width; REPORT or BACK goes back.
void ModsScreen::report() {
  if (reporting) {
    reporting = false;
    refill();
    return;
  }
  std::string folder, name;
  if (focus == kCampaigns && campaign_list.selected >= 0 &&
      campaign_list.selected < static_cast<int32_t>(tables.campaigns.size())) {
    const ModsCampaignRow& c = tables.campaigns[static_cast<size_t>(campaign_list.selected)];
    folder = c.mod, name = c.name;
  } else if (focus == kMods && mod_list.selected >= 0 && mod_list.selected < static_cast<int32_t>(tables.mods.size())) {
    const ModsListEntry& m = tables.mods[static_cast<size_t>(mod_list.selected)];
    folder = m.info.folder, name = m.info.name.empty() ? m.info.folder : m.info.name;
  }
  std::string path;
  for (const ModInfo& i : mods_found(mods_menu_write_dir()))
    if (i.folder == folder) path = i.path;
  if (path.empty()) {
    say(note, "Choose a mod or a custom campaign for its report.");
    return;
  }
  int refused = 0;
  std::vector<std::string> lines;
  std::vector<std::string> sentences;
  for (const ModsListEntry& e : tables.mods)
    if (e.info.folder == folder) sentences = e.needs;  // what it needs first (mod_needs.h)
  for (const std::string& s : mod_report(path, mods_menu_install(), folder, &refused)) sentences.push_back(s);
  for (const std::string& s : sentences)
    for (const std::string& l : mod_report_wrap(s, kReportWidth)) lines.push_back(l);
  reporting = true;
  campaign_list.clear_items();
  for (size_t i = 0; i < lines.size(); ++i) campaign_list.add_item(lines[i].c_str(), static_cast<int32_t>(i));
  campaign_list.layout();
  focus = kCampaigns;
  say(detail, "REPORT " + name);
  say(detail2, refused ? std::to_string(refused) + (refused == 1 ? " thing" : " things") + " the game would refuse" : "nothing refused");
  say(note, "REPORT or BACK goes back to the tables.");
  std::printf("mw3: mods: report on %s: %zu lines\n", folder.c_str(), lines.size());
}

// EDITOR (ours; editor.md, The way in from MODS): the selected campaign's
// mod, or the selected mod, its missions listed in CUSTOM CAMPAIGNS' box;
// ENTER edits the chosen one. A mod not on in this run is turned on for the
// next start instead, as ENTER does. Pressed again, back to the tables.
void ModsScreen::edit() { missions(false); }

// TEST (ours; modding.md, Quick testing): as EDITOR, with the game's own
// missions after the mod's; ENTER plays the chosen one with the mods on.
void ModsScreen::test() { missions(true); }

void ModsScreen::missions(bool test) {
  std::string folder, name;
  if (editing) {
    const bool same = testing == test;
    const std::string was = edit_mod;
    editing = false, testing = false, edit_rows.clear(), edit_mod.clear();
    refill();
    if (same) return;  // pressed again: back to the tables; the other: its list, of the same mod
    folder = was;
    for (const ModsListEntry& m : tables.mods)
      if (m.info.folder == was) name = m.info.name.empty() ? was : m.info.name;
    for (const ModsCampaignRow& c : tables.campaigns)
      if (name.empty() && c.mod == was) name = c.name;
    if (name.empty()) name = was;
  } else if (focus == kCampaigns && campaign_list.selected >= 0 &&
      campaign_list.selected < static_cast<int32_t>(tables.campaigns.size())) {
    const ModsCampaignRow& c = tables.campaigns[static_cast<size_t>(campaign_list.selected)];
    folder = c.mod, name = c.name;
  } else if (focus == kMods && mod_list.selected >= 0 && mod_list.selected < static_cast<int32_t>(tables.mods.size())) {
    const ModsListEntry& m = tables.mods[static_cast<size_t>(mod_list.selected)];
    folder = m.info.folder, name = m.info.name.empty() ? m.info.folder : m.info.name;
  }
  const char* verb = test ? "test" : "edit";
  if (folder.empty()) {
    say(note, std::string("Choose a mod or a custom campaign to ") + verb + ".");
    return;
  }
  const std::vector<std::string>& on = mods_menu_mounted();
  if (std::find(on.begin(), on.end(), folder) == on.end()) {
    if (std::find(enabled.begin(), enabled.end(), folder) == enabled.end()) {
      mods_toggle(enabled, folder);
      mods_save_enabled(mods_menu_write_dir(), enabled);
      refill();
    }
    say(note, name + ": its mod is turned on; restart to " + verb + " it.");
    return;
  }
  std::vector<ModsEditRow> rows;
  for (const ModsListEntry& m : mods_list(mods_menu_write_dir(), enabled))
    if (m.info.folder == folder) rows = test ? mods_test_rows(m.info, mods_menu_install()) : mods_edit_rows(m.info);
  if (rows.empty()) {
    say(note, name + " has no missions to edit (no campaign= lines, no world with a reader archive).");
    return;
  }
  editing = true, testing = test, edit_mod = folder, edit_rows = std::move(rows);
  campaign_list.clear_items();
  for (size_t i = 0; i < edit_rows.size(); ++i)
    campaign_list.add_item((edit_rows[i].world + " mission " + std::to_string(edit_rows[i].number)).c_str(), static_cast<int32_t>(i));
  campaign_list.layout();
  campaign_list.FileList::select(0);
  focus = kCampaigns;
  say(detail, (test ? "TEST " : "EDIT ") + name);
  say(detail2, "");
  tell(test ? "Choose a mission; ENTER plays it with the mods on. TEST again goes back."
            : "Choose a mission; ENTER opens it in the editor. EDITOR again goes back.");
}

void ModsScreen::edit_start() {
  const int32_t i = campaign_list.selected;
  if (i < 0 || i >= static_cast<int32_t>(edit_rows.size())) {
    say(note, testing ? "Choose a mission to play." : "Choose a mission to edit.");
    return;
  }
  const ModsEditRow r = edit_rows[static_cast<size_t>(i)];
  const ModsEditStart& start = testing ? mods_menu_test_start() : mods_menu_edit_start();
  std::string why = testing ? "the mission cannot be started here" : "the editor cannot be started here";
  if (!start || !start(edit_mod, r.world, r.number, &why)) say(note, (testing ? "Cannot play it: " : "Cannot edit it: ") + why);
}

void ModsScreen::update(float dt) {
  Window::update(dt);
  // The add runs on the update after "Adding ..." was drawn: it can take a
  // while (a big archive) and the screen would look frozen without a word.
  if (!adding.empty()) {
    std::string path;
    path.swap(adding);
    add_mod(path);
    return;
  }
  if (!exporting.empty()) {
    std::string zip;
    zip.swap(exporting);
    export_mod(zip);
    return;
  }
  std::string path;
  {
    Picked& p = picked();
    std::lock_guard<std::mutex> g(p.m);
    if (!p.answered) return;
    p.answered = false, path.swap(p.path);
  }
  if (!export_folder.empty()) {  // EXPORT's save box answered
    if (path.empty()) {
      export_folder.clear();
      tell("Not exported.");
      return;
    }
    if (std::filesystem::path(path).extension().empty()) path += ".zip";  // a save box that did not add it
    tell("Exporting to " + std::filesystem::path(path).filename().string() + "...");
    exporting = path;
    return;
  }
  if (path.empty()) return;
  std::filesystem::path shown(path);
  while (!shown.empty() && shown.filename().empty()) shown = shown.parent_path();  // a folder with a trailing '/'
  tell("Adding " + shown.filename().string() + "...");
  adding = path;
}

// Two lines under MODS (the note's and the restart's), the text wrapped at
// a word; what does not fit ends with "...".
void ModsScreen::tell(const std::string& text) {
  constexpr size_t kWidth = 58;
  std::string a = text, b;
  if (a.size() > kWidth) {
    size_t cut = a.rfind(' ', kWidth);
    if (cut == std::string::npos || cut < kWidth / 2) cut = kWidth;
    b = a.substr(cut + (a[cut] == ' ' ? 1 : 0));
    a.resize(cut);
    if (b.size() > kWidth) b = b.substr(0, kWidth - 3) + "...";
  }
  say(note, a);
  say(restart, b);
}

// ADD MOD's pick: prepared, the game asked when its files cannot tell, added
// (mod_install.h), turned on for the next start when it is more than a
// campaign, shown in its table and said.
void ModsScreen::add_mod(const std::string& path) {
  const ModAddDone d = mod_add_run(mods_menu_write_dir(), path, mods_menu_install(), mods_menu_platform().ask_game, &enabled);
  if (!d.ok) {
    tell(d.message);
    return;
  }
  refill();
  if (d.campaign_only) {
    for (int32_t k = 0; k < campaign_list.item_count(); ++k)
      if (tables.campaigns[static_cast<size_t>(k)].mod == d.folder) {
        campaign_list.FileList::select(k), focus = kCampaigns;
        break;
      }
    show_details();
    tell("Added " + d.name + ": ENTER on its campaign turns it on for the next start.");
  } else {
    for (int32_t k = 0; k < mod_list.item_count(); ++k)
      if (tables.mods[static_cast<size_t>(k)].info.folder == d.folder) {
        mod_list.FileList::select(k), focus = kMods;
        break;
      }
    show_details();
    tell(d.message);
  }
}

// EXPORT's answer: the mod chosen when the save box opened, packed into
// `zip` (upgrades/mods/mod_export.h) and said.
void ModsScreen::export_mod(const std::string& zip) {
  std::string folder;
  folder.swap(export_folder);
  const std::filesystem::path dir = std::filesystem::path(mods_menu_write_dir()) / "mods" / folder;
  tell(mod_export(dir.string(), zip, mods_menu_install(), true).message);  // checked first (modder step 8)
}

bool ModsState::enter() {
  Engine& en = e();
  screen_ = en.construct(kModsScreenCtor, {addr()});
  ShellState::enter();
  return true;
}

void ModsState::leave() {
  e().shell_clear(addr());
  ShellState::leave();
}

}  // namespace mech3
