#include "mech3/mods_menu.h"

#include <fstream>

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
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/import_folder.h"
#include "upgrades/mods/screen_fit.h"
#include "upgrades/mods/mods_delete.h"
#include "upgrades/mods/entry_run.h"
#include "upgrades/mods/mod_report.h"
#include "upgrades/mods/mod_export.h"

namespace mech3 {

// ---- the tables ----

std::vector<ModsCampaignRow> mods_entry_rows(const std::string& write_dir) {
  std::vector<ModsCampaignRow> out;
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  const std::vector<ModInfo> mods = mods_found(write_dir);
  for (const CampaignEntry& e : all) {
    if (e.kind == CampaignEntry::kInstantAction) continue;  // Instant Action's mods are on its own screen
    ModsCampaignRow r;
    r.entry = e.id, r.name = e.name, r.mod = e.mod, r.file = e.campaign.empty() ? std::string() : e.campaign + ".txt";
    r.mounted = true;
    for (const ModInfo& m : mods)
      if (m.folder == e.mod) r.author = m.author, r.version = m.version;
    if (e.kind == CampaignEntry::kCopy)
      if (const CampaignEntry* b = find_entry(all, e.base)) r.copy_of = b->name;
    if (e.missing) {
      // Its name as our marker in its folder kept it ("<name>" (<id>): ...), since its mod.txt is gone.
      std::ifstream marker(std::filesystem::path(entry_dir(write_dir, e.id)) / kCampaignMarker);
      std::string line;
      std::getline(marker, line);
      const size_t q = line.find('"', 1);
      if (line.size() > 2 && line[0] == '"' && q != std::string::npos && q > 1) r.name = line.substr(1, q - 1);
      r.error = "its mod is not in the mods folder any more (its pilots and saves are kept: import it again)";
    }
    else if (!e.error.empty()) {
      r.error = e.error;
      if (!r.file.empty()) r.name = r.file;  // a campaign file that does not read has no name of its own: its file's
    }
    else
      for (const std::string& m : entry_missing_mods(write_dir, e))
        r.error += (r.error.empty() ? "these mods are not there any more (turn them off or import them again): " : ", ") + m;
    out.push_back(std::move(r));
  }
  return out;
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
  if (c.mod.empty() && !c.entry.empty()) return "the game's own campaign";
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
ModsPlay& mods_menu_play() {
  static ModsPlay p;
  return p;
}

ModsTestRestart& mods_menu_test_restart() {
  static ModsTestRestart r;
  return r;
}
ModsTestRestart& mods_menu_edit_restart() {
  static ModsTestRestart r;
  return r;
}
std::string& mods_menu_open_entry() {
  static std::string id;
  return id;
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
zimg::Texture* campaign_lines(zimg::Texture* src, bool left_box = true) {
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
  // Without the left box (Instant Action's MODS) the short line that joined it to the top box goes too.
  constexpr int kJoinX0 = 215, kJoinX1 = 235, kJoinY0 = 75;
  for (int y = kJoinY0; !left_box && y < kToY; ++y)
    for (int x = kJoinX0; x < kJoinX1 && x < w; ++x) out[y * w + x] = 0, dst->alpha[y * w + x] = 0;
  for (int y = 0; left_box && y < kBoxH; ++y)
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

void mods_screen_target(Engine& e) { target_buffers(e); }
void mods_text_put(ztwod::Text& t, const char* font, int32_t x, int32_t y) { put(t, font, x, y); }
void mods_text_say(ztwod::Text& t, const std::string& s) { say(t, s); }
zimg::Texture* mods_screen_lines(zimg::Texture* src, bool left_box) { return campaign_lines(src, left_box); }

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
      editor(e, kEditor),
      test_btn(e, kTest),
      add_btn(e, kAdd),
      export_btn(e, kExport),
      add_folder_btn(e, kAddFolder),
      folder_btn(e, kOpenFolder),
      report_btn(e, kReport),
      delete_btn(e, kDelete),
      reset_btn(e, kDelete),
      campaign_list(e, kCampaigns),
      ticks(e),
      mod_list(ticks.list),
      enabled(ticks.ticks) {
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
    bind_button(&ticks.list, "MODLIST");
    bind_button(&ticks.toggle, "TOGGLE");
    bind_button(&ticks.up, "MOVEUP");
    bind_button(&ticks.down, "MOVEDOWN");
    bind_button(&editor, "EDITOR");
    bind_button(&test_btn, "TESTMOD");
    bind_button(&add_btn, "ADDMOD");
    bind_button(&export_btn, "EXPORTMOD");
    bind_button(&add_folder_btn, "ADDFOLDER");
    bind_button(&folder_btn, "MODSFOLDER");
    bind_button(&report_btn, "REPORTMOD");
    bind_button(&delete_btn, "DELETEROW");
    bind_button(&reset_btn, "RESETROW");
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
  entry = official_entries().front().id;
  if (!mods_menu_open_entry().empty()) entry = mods_menu_open_entry();  // the campaign just left (--mods-entry)
  refill();
  if (entry_run().screen == EntryRun::kTest) open_test_mode();
}

ModsScreen::~ModsScreen() {
  if (own_lines) {
    lines2.set_image(static_cast<zimg::Texture*>(nullptr));
    ztwod::host().image_release(own_lines);
  }
}

// The left box's heading follows what it shows (its list's label, spaced as the dialog's).
void ModsScreen::heading(const char* words) {
  if (campaign_list.labels.empty() || !campaign_list.labels[0]) return;
  std::string spaced;
  for (const char* c = words; *c; ++c) spaced += *c, spaced += c[1] ? " " : "";
  say(*campaign_list.labels[0], spaced);
}

void ModsScreen::refill() {
  heading("CAMPAIGNS");
  tables.campaigns = mods_entry_rows(mods_menu_write_dir());
  campaign_list.clear_items();
  int32_t at = 0;
  for (size_t i = 0; i < tables.campaigns.size(); ++i) {
    campaign_list.add_item(mods_campaign_row_text(tables.campaigns[i]).c_str(), static_cast<int32_t>(i));
    if (tables.campaigns[i].entry == entry) at = static_cast<int32_t>(i);
  }
  campaign_list.layout();
  if (campaign_list.item_count()) campaign_list.FileList::select(at);
  choose_entry();
}

const ModsCampaignRow* ModsScreen::chosen_row() const {
  const int32_t i = campaign_list.selected;
  return !reporting && !editing && i >= 0 && i < static_cast<int32_t>(tables.campaigns.size()) ? &tables.campaigns[static_cast<size_t>(i)] : nullptr;
}

// The campaign chosen at the left: MODS shows its own mods (grey, always on)
// and every asset mod with that campaign's ticks.
void ModsScreen::choose_entry() {
  const std::string& dir = mods_menu_write_dir();
  if (const ModsCampaignRow* row = chosen_row()) entry = row->entry;
  const std::vector<CampaignEntry> all = campaign_entries(dir);
  const CampaignEntry* e = find_entry(all, entry);
  std::vector<std::string> locked;
  if (e) entry_mount_list(dir, *e, &locked);
  ticks.fill(dir, e ? entry_ticks(dir, entry) : std::vector<std::string>{}, locked, true);
  focus = kCampaigns;  // the fill chose a row of MODS: the details stay the campaign's
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
    else if (delete_plan_for_entry(mods_menu_write_dir(), c.entry, entry_run().id).by_hand)
      n = "PLAY starts it. Put in the mods folder by hand: remove its folder yourself to delete it.";
    else n = (c.copy_of.empty() ? std::string() : "A copy of " + c.copy_of + ". ") + "PLAY starts it with the mods ticked here.";
  } else if (focus == kMods && mod_list.selected >= 0 && mod_list.selected < static_cast<int32_t>(ticks.rows.size())) {
    const ModsListEntry& m = ticks.rows[static_cast<size_t>(mod_list.selected)];
    d = m.info.name.empty() ? m.info.folder : m.info.name;
    d2 = (m.info.author.empty() ? std::string("unknown author") : m.info.author) +
         (m.info.version.empty() ? "" : ", version " + m.info.version);
    n = ticks.note();
    // One the game did not add has no DELETE tab: said, so the player knows how it goes (playtest M12).
    if (n.empty() && delete_plan_for_mod(mods_menu_write_dir(), m.info.folder).by_hand)
      n = "Put in the mods folder by hand: remove its folder yourself to delete it.";
  }
  say(detail, d);
  say(detail2, d2);
  show_delete_tab();
  // A campaign's note takes both lines (a reason can be long); a mod's is one, over whose mods these are.
  if (focus == kCampaigns) {
    tell(n);
  } else {
    const ModsCampaignRow* row = chosen_row();
    // A long one takes both lines; else the second says whose ticks these are.
    const auto lines = mods_note_lines(n, [this](const std::string& t) { return mods_text_fits(t, &note, kModsNotePx, kModsNoteChars); });
    say(note, lines.first);
    say(restart, !lines.second.empty() ? lines.second
                 : row ? mods_text_fit("Ticks for: " + row->name, &note, kModsNotePx, kModsNoteChars) : std::string());
  }
}

void ModsScreen::on_button(int32_t id, int32_t a, int32_t) {
  const std::string& dir = mods_menu_write_dir();
  if (test_mode) {  // the entry's test list: a mission chosen, played, or the entry left
    if (id == kEnter) edit_start();
    else if (id == kBack) state_escape();
    else if (id != kCampaigns && id != kMods && id != ModTickList::kList) tell("This is the test of one campaign: PLAY plays a mission, BACK leaves it.");
    return;
  }
  auto chosen_mod = [&]() -> const ModsListEntry* {
    const int32_t i = mod_list.selected;
    return i >= 0 && i < static_cast<int32_t>(ticks.rows.size()) ? &ticks.rows[static_cast<size_t>(i)] : nullptr;
  };
  switch (id) {
    case kCampaigns:
      focus = id;
      (void)a;
      if (!reporting && !editing) choose_entry();
      else show_details();
      return;
    case kMods:
    case ModTickList::kList:
      focus = kMods;
      show_details();
      return;
    case kToggle:
    case kUp:
    case kDown:
    case ModTickList::kToggle:
    case ModTickList::kUp:
    case ModTickList::kDown: {
      // The tick list's own: the chosen campaign's mods.txt written at once.
      const int32_t tid = id == kToggle ? ModTickList::kToggle : id == kUp ? ModTickList::kUp : id == kDown ? ModTickList::kDown : id;
      bool changed = false;
      ticks.on_button(tid, &changed);
      if (changed) {
        const std::vector<CampaignEntry> all = campaign_entries(dir);
        const CampaignEntry* e = find_entry(all, entry);
        if (!e || !entry_save_ticks(dir, *e, ticks.ticks)) tell("The mods could not be saved (the settings folder cannot be written).");
        // A missing mod unticked, or one ticked: the campaign's own row may change its "!".
        const std::vector<ModsCampaignRow> rows = mods_entry_rows(dir);
        if (!reporting && !editing && rows.size() == tables.campaigns.size()) {
          tables.campaigns = rows;
          const int32_t sel = campaign_list.selected;
          campaign_list.clear_items();
          for (size_t i = 0; i < rows.size(); ++i) campaign_list.add_item(mods_campaign_row_text(rows[i]).c_str(), static_cast<int32_t>(i));
          campaign_list.layout();
          campaign_list.FileList::select(sel);
        }
      }
      focus = kMods;
      show_details();
      if (!ticks.warned.empty()) tell(ticks.warned);
      return;
    }
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
      // With a report up, the folder its text file was saved in (ours: mod_report.h).
      if (reporting && !report_file.empty()) {
        const std::string r = mod_reports_dir(dir);
        if (!mods_menu_platform().open_folder || !mods_menu_platform().open_folder(r)) tell("The reports folder: " + r);
        return;
      }
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
    case kDelete:
      delete_row();
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

// The plan for what is chosen: the mod at the right when MODS has the focus (never a campaign's own mod from
// there: its row is at the left), else the campaign at the left.
namespace {
DeletePlan plan_for(ModsScreen& s) {
  DeletePlan p;
  if (s.test_mode || s.reporting || s.editing) return p.refused = "Close the list first.", p;
  if (s.focus == ModsScreen::kMods) {
    const ModsListEntry* m = s.ticks.chosen();
    if (!m) return p.refused = "Choose a mod or a campaign.", p;
    if (mod_is_campaign(m->info)) return p.refused = "It is a campaign's own: choose the campaign at the left to delete it.", p;
    return delete_plan_for_mod(mods_menu_write_dir(), m->info.folder);
  }
  const ModsCampaignRow* row = s.chosen_row();
  if (!row) return p.refused = "Choose a mod or a campaign.", p;
  return delete_plan_for_entry(mods_menu_write_dir(), row->entry, entry_run().id);
}
}  // namespace

void ModsScreen::show_delete_tab() {
  const std::string label = plan_for(*this).label;
  tab_shown = label == "DELETE" ? 1 : label == "RESET" ? 2 : 0;
  if (label == "DELETE") delete_btn.show_for(-1.0f);
  else delete_btn.hide();
  if (label == "RESET") reset_btn.show_for(-1.0f);
  else reset_btn.hide();
}

// DELETE or RESET: the game's own confirm saying what goes, then the one remover (mods_delete.h).
void ModsScreen::delete_row() {
  // One question a click. DELETE and RESET are two tabs in one place: after a delete the game's own campaign
  // is chosen and RESET is shown where DELETE was, and the click that was just answered reached it too, so
  // "Reset MechWarrior 3 to the original?" came up straight after the delete's OK (seen in a scripted run,
  // 2026-11-11). The next frame's update lets the tab be pressed again.
  if (delete_asked) return;
  delete_asked = true;
  const DeletePlan plan = plan_for(*this);
  if (plan.kind == DeletePlan::kNone) {
    tell(plan.refused);
    return;
  }
  const bool reset = plan.kind == DeletePlan::kReset;
  std::printf("mw3: mods: asks: %s\n", plan.question.c_str());
  const int answer = engine().message_box_words(1, reset ? "Reset" : "Delete", plan.question);
  delete_asked = true;  // the box's own frames ran this screen's update
  if (answer != 1) {
    tell(reset ? "Not reset." : "Not deleted.");
    return;
  }
  std::string why;
  const bool ok = delete_run(mods_menu_write_dir(), plan, &why);
  std::printf("mw3: mods: %s %s%s%s: %s\n", reset ? "reset" : "delete", plan.mod.empty() ? "" : ("mods/" + plan.mod + " ").c_str(),
              plan.entry.empty() ? "" : "campaigns/", plan.entry.c_str(), ok ? "done" : why.c_str());
  entry = official_entries().front().id;  // the game's own campaign chosen after it
  focus = kCampaigns;
  refill();
  tell(ok ? (reset ? "Reset." : "Deleted.") : (reset ? "Not reset: " : "Not deleted: ") + why);
}

// ENTER: the chosen campaign entered (its own pilots), MODS let go and the
// Campaign State pushed on the main menu, as its CAMPAIGN (the main menu
// shown again lets the campaign go: MainState::update); a campaign whose mod is not on in this
// run is never started half-mounted: its mod is turned on for the next start.
void ModsScreen::enter_campaign() {
  const ModsCampaignRow* row = chosen_row();
  if (!row) {
    say(note, "Choose a campaign to play.");
    return;
  }
  const std::string name = row->name, id = row->entry;
  std::string why = row->error;
  if (why.empty() && !mods_menu_play()) why = "the game cannot start again from here";
  if (!why.empty() || !mods_menu_play()(id, &why)) tell(name + " cannot be played: " + why);
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
  } else if (focus == kMods && mod_list.selected >= 0 && mod_list.selected < static_cast<int32_t>(ticks.rows.size())) {
    const ModsListEntry& m = ticks.rows[static_cast<size_t>(mod_list.selected)];
    folder = m.info.folder, name = m.info.name.empty() ? m.info.folder : m.info.name;
  }
  std::string path;
  for (const ModInfo& i : mods_found(mods_menu_write_dir()))
    if (i.folder == folder) path = i.path;
  if (path.empty()) {
    // The game's own campaign has no mod to report on; its mods are at the right.
    tell(focus == kCampaigns && chosen_row() && chosen_row()->mod.empty()
             ? name + " is the game's own campaign: choose one of its mods at the right for a report."
             : "Choose a mod or a campaign for its report.");
    return;
  }
  int refused = 0;
  std::vector<std::string> lines;
  std::vector<std::string> sentences;
  for (const ModsListEntry& e : ticks.rows)
    if (e.info.folder == folder) sentences = e.needs;  // what it needs first (mod_needs.h)
  // A copy: what it changed from the campaign it was saved from, first (mod_report.h, mod_copy_report).
  for (const std::string& s : mod_copy_report(mods_menu_write_dir(), mods_menu_install(), folder)) sentences.push_back(s);
  for (const std::string& s : mod_report(path, mods_menu_install(), folder, &refused)) sentences.push_back(s);
  // The whole report as a text file of ours too, for the mod's maker to read outside the game.
  std::string why;
  report_file.clear();
  int warnings = 0;
  for (const std::string& s : sentences) warnings += s.rfind("Wrong: ", 0) == 0;
  std::vector<std::string> text = {"Report on " + name + "."};  // the file's first line; the screen has it in its heading
  text.insert(text.end(), sentences.begin(), sentences.end());
  if (mod_report_write(mods_menu_write_dir(), folder, text, &report_file, &why))
    sentences.push_back(""), sentences.push_back("This report is saved as " + report_file + " (MODS FOLDER opens its folder).");
  else
    sentences.push_back(""), sentences.push_back("This report could not be saved as a file: " + why + ".");
  for (const std::string& s : sentences) {
    if (s.empty()) lines.push_back("");
    for (const std::string& l : mods_text_wrap(s, &detail, kModsRowPx, kModsRowChars)) lines.push_back(l);  // the box's real width
  }
  reporting = true;
  campaign_list.clear_items();
  for (size_t i = 0; i < lines.size(); ++i) campaign_list.add_item(lines[i].c_str(), static_cast<int32_t>(i));
  campaign_list.layout();
  focus = kCampaigns;
  heading("REPORT");
  say(detail, mods_text_fit("REPORT " + name, &detail, kModsDetailPx, kModsDetailChars));
  say(detail2, refused ? "the mod is refused"
               : warnings ? std::to_string(warnings) + (warnings == 1 ? " warning" : " warnings") + ", nothing refused" : "nothing refused");
  say(note, report_file.empty() ? "BACK: the list." : "BACK: the list. MODS FOLDER: the saved report.");
  say(restart, "");
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
  if (test_mode) return;  // the test list is the screen there
  std::string folder, name, entry_id;
  bool own = false;
  if (editing) {
    const bool same = testing == test;
    editing = false, testing = false, edit_rows.clear(), edit_mod.clear();
    refill();
    if (same) return;  // pressed again: back to the tables; the other: its list, of the same campaign
  }
  // The chosen campaign's missions; with a row of MODS in focus, that mod's own first (it must be on for the campaign).
  const ModsCampaignRow* row = chosen_row();
  if (!row) {
    say(note, std::string("Choose a campaign to ") + (test ? "test." : "edit."));
    return;
  }
  entry_id = row->entry, folder = row->mod, name = row->name, own = row->mod.empty();
  if (!row->error.empty()) {
    tell(name + " cannot be played: " + row->error);
    return;
  }
  const ModsListEntry* picked = focus == kMods ? ticks.chosen() : nullptr;
  if (picked && !picked->on) {
    tell("Tick " + (picked->info.name.empty() ? picked->info.folder : picked->info.name) + " for " + name + " to " + (test ? "test" : "edit") + " it there.");
    return;
  }
  std::vector<ModsEditRow> rows;
  auto add = [&](const std::vector<ModsEditRow>& more) {
    for (const ModsEditRow& r : more) {
      bool seen = false;
      for (const ModsEditRow& have : rows) seen = seen || (crt::msvc_stricmp(have.world.c_str(), r.world.c_str()) == 0 && have.number == r.number);
      if (!seen) rows.push_back(r);
    }
  };
  const std::vector<ModInfo> found = mods_found(mods_menu_write_dir());
  auto info_of = [&](const std::string& f) {
    for (const ModInfo& i : found)
      if (i.folder == f) return i;
    return ModInfo{};
  };
  // EDITOR's list is TEST's: any mission the campaign plays can be opened; its saves make a copy (save_as.h).
  if (picked) add(mods_edit_rows(picked->info));
  if (!own) add(mods_edit_rows(info_of(folder)));
  add(mods_test_rows(own ? ModInfo{} : info_of(folder), mods_menu_install()));
  if (rows.empty()) {
    say(note, name + " has no missions to " + (test ? "test" : "edit") + " (no campaign= lines, no world with a reader archive).");
    return;
  }
  editing = true, testing = test, edit_mod = folder, edit_rows = std::move(rows);
  campaign_list.clear_items();
  for (size_t i = 0; i < edit_rows.size(); ++i)
    campaign_list.add_item((edit_rows[i].world + " mission " + std::to_string(edit_rows[i].number)).c_str(), static_cast<int32_t>(i));
  campaign_list.layout();
  campaign_list.FileList::select(0);
  focus = kCampaigns;
  heading(test ? "TEST" : "EDITOR");  // short: "TEST: MISSIONS" spaced out runs over the frame's join
  say(detail, mods_text_fit((test ? "TEST " : "EDIT ") + name, &detail, kModsDetailPx, kModsDetailChars));
  say(detail2, "");
  tell(test ? "Choose a mission; PLAY starts the game again in " + name + " and plays it. TEST again goes back."
            : "Choose a mission; PLAY starts the game again in " + name + " with it open in the editor. Saving there makes a copy.");
}

// Inside an entry started for TEST: the screen is its test list alone.
void ModsScreen::open_test_mode() {
  const EntryRun& run = entry_run();
  entry = run.id;
  std::string name = run.id;
  for (const ModsCampaignRow& c : tables.campaigns)
    if (c.entry == run.id) name = c.name;
  std::vector<ModsEditRow> rows;
  const std::vector<ModInfo> found = mods_found(mods_menu_write_dir());
  ModInfo info;
  for (const ModInfo& i : found)
    if (i.folder == run.mod) info = i;
  // Every mounted mod's own missions, then the campaign's, then the game's, each once.
  auto add = [&](const std::vector<ModsEditRow>& more) {
    for (const ModsEditRow& r : more) {
      bool seen = false;
      for (const ModsEditRow& have : rows) seen = seen || (crt::msvc_stricmp(have.world.c_str(), r.world.c_str()) == 0 && have.number == r.number);
      if (!seen) rows.push_back(r);
    }
  };
  for (const std::string& m : mods_menu_mounted())
    for (const ModInfo& i : found)
      if (i.folder == m) add(mods_edit_rows(i));
  add(mods_test_rows(info, mods_menu_install()));
  test_mode = true, editing = true, testing = true, edit_mod = run.mod, edit_rows = std::move(rows);
  campaign_list.clear_items();
  int32_t at = 0;
  for (size_t i = 0; i < edit_rows.size(); ++i) {
    campaign_list.add_item((edit_rows[i].world + " mission " + std::to_string(edit_rows[i].number)).c_str(), static_cast<int32_t>(i));
    if (crt::msvc_stricmp(edit_rows[i].world.c_str(), run.world.c_str()) == 0 && edit_rows[i].number == run.number) at = static_cast<int32_t>(i);
  }
  campaign_list.layout();
  if (campaign_list.item_count()) campaign_list.FileList::select(at);
  focus = kCampaigns;
  heading("TEST");
  say(detail, mods_text_fit("TEST in " + name, &detail, kModsDetailPx, kModsDetailChars));
  say(detail2, std::to_string(mods_menu_mounted().size()) + (mods_menu_mounted().size() == 1 ? " mod on" : " mods on"));
  // A mission that did not load says why here (the load's own words), and the list stays.
  std::string said = run.said;
  if (said.empty() && mech3::missions().failed)
    said = "The mission could not be loaded: " + (mech3::missions().failed_why.empty() ? std::string("its files are incomplete") : mech3::missions().failed_why) + ".";
  tell(!said.empty() ? said : "Choose a mission; PLAY plays it at once. BACK leaves " + name + ".");
}

void ModsScreen::edit_start() {
  const int32_t i = campaign_list.selected;
  if (i < 0 || i >= static_cast<int32_t>(edit_rows.size())) {
    say(note, testing ? "Choose a mission to play." : "Choose a mission to edit.");
    return;
  }
  const ModsEditRow r = edit_rows[static_cast<size_t>(i)];
  // On a plain run TEST starts the program again into the campaign; inside it (the test mode) the mission is
  // played in place, the mechs' definitions read again (upgrades/mods/mod_test.h).
  if (!test_mode) {
    std::string why = "the game cannot start again from here";
    const ModsTestRestart& go = testing ? mods_menu_test_restart() : mods_menu_edit_restart();
    if (!go || !go(entry, r.world, r.number, &why)) tell(std::string(testing ? "Cannot play it: " : "Cannot edit it: ") + why);
    return;
  }
  const ModsEditStart& start = testing ? mods_menu_test_start() : mods_menu_edit_start();
  std::string why = testing ? "the mission cannot be started here" : "the editor cannot be started here";
  if (!start || !start(edit_mod, r.world, r.number, &why)) tell((testing ? "Cannot play it: " : "Cannot edit it: ") + why);
}

void ModsScreen::update(float dt) {
  Window::update(dt);
  delete_asked = false;
  // The two tabs share a place: the one not wanted stays hidden (a tab hidden while its own press was still
  // being shown came back, and RESET and DELETE were drawn over each other after a delete).
  if (tab_shown != 1) delete_btn.hide();
  if (tab_shown != 2) reset_btn.hide();
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
  const auto lines = mods_note_lines(text, [this](const std::string& t) { return mods_text_fits(t, &note, kModsNotePx, kModsNoteChars); });
  say(note, lines.first);
  say(restart, lines.second);
}

// ADD MOD's pick: prepared, the game asked when its files cannot tell, added
// (mod_install.h), turned on for the next start when it is more than a
// campaign, shown in its table and said.
void ModsScreen::add_mod(const std::string& path) {
  // Added unticked: nothing changes for any campaign until the player ticks it.
  // A whole modded game folder: what differs from the player's install becomes the mod (upgrades/mods/import_folder.h).
  std::string took;
  const ModAddDone d = import_folder_run(mods_menu_write_dir(), path, mods_menu_install(), mods_menu_platform().ask_game, nullptr, &took);
  if (!d.ok) {
    tell(d.message);
    return;
  }
  refill();
  // A campaign (its own campaign= lines, or one that plays the game's own over its files): its row in CAMPAIGNS
  // chosen; an asset mod: its row in MODS, unticked.
  bool listed = false;
  for (int32_t k = 0; k < campaign_list.item_count() && !listed; ++k)
    if (tables.campaigns[static_cast<size_t>(k)].mod == d.folder) {
      campaign_list.FileList::select(k);
      choose_entry();
      listed = true;
    }
  for (int32_t k = 0; k < mod_list.item_count() && !listed; ++k)
    if (ticks.rows[static_cast<size_t>(k)].info.folder == d.folder) {
      mod_list.FileList::select(k), focus = kMods;
      listed = true;
    }
  show_details();
  // The outcome first, with the warnings' count and REPORT; then what to do next (screen_fit.h). An import from
  // an install or installer says all it took in the log (import_folder_run); the screen has no room for it.
  if (!took.empty()) std::printf("mw3: mods: %s\n", d.message.c_str());
  const auto lines = mods_import_lines(d, took, [this](const std::string& t) { return mods_text_fits(t, &note, kModsNotePx, kModsNoteChars); });
  say(note, lines.first);
  say(restart, lines.second);
  std::printf("mw3: mods: on the screen: %s | %s\n", note.text, restart.text);  // what the two lines hold now
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
