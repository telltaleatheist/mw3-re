#include "mech3/mods_menu.h"

#include <algorithm>
#include <cstdlib>

#include "gamez/zimage/zimg_library.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/mainmenu.h"
#include "mech3/mod_campaign.h"

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
  const bool flagged = !m.info.error.empty() || !m.conflicts_with.empty();
  return std::string(m.on ? "[x] " : "[ ] ") + name + (flagged ? " !" : "");
}

std::string mods_row_note(const ModsListEntry& m, const std::vector<ModsListEntry>& all) {
  if (!m.info.error.empty()) return "Cannot be used: " + m.info.error;
  if (m.conflicts_with.empty()) return "";
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
  return "Shares " + what + " with " + names + ", which wins";
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

ModsScreen::ModsScreen(Engine& e, Handle s)
    : MenuScreen(e, s),
      enter(e, kEnter),
      back(e, kBack),
      toggle(e, kToggle),
      up(e, kUp),
      down(e, kDown),
      editor(e, kEditor),
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
        if (id == kToggle) mods_toggle(enabled, folder);
        else mods_move(enabled, folder, id == kUp ? -1 : 1);
        mods_save_enabled(dir, enabled);
        refill();
        for (int32_t i = 0; i < mod_list.item_count(); ++i)
          if (tables.mods[static_cast<size_t>(i)].info.folder == folder) mod_list.FileList::select(i);
        focus = kMods;
        show_details();
      }
      return;
    case kEnter:
      enter_campaign();
      return;
    case kEditor:
      say(note, "The mission editor is not built yet.");
      return;
    case kBack:
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
