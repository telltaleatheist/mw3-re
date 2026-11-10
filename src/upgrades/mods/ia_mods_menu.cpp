#include "upgrades/mods/ia_mods_menu.h"

#include <algorithm>

#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/entry_run.h"

namespace mech3 {

bool& ia_mods_page() {
  static bool on = false;
  return on;
}
Handle& ia_mods_state() {
  static Handle h = 0;
  return h;
}
IaModsRestart& ia_mods_restart() {
  static IaModsRestart r;
  return r;
}

namespace {
bool inside_entry() { return entry_run().screen == EntryRun::kInstantAction && entry_run().id == kIaEntry; }
}  // namespace

std::string ia_mods_line(const std::string& write_dir, int ticked) {
  const int n = ticked >= 0 ? ticked : inside_entry() ? static_cast<int>(entry_ticks(write_dir, kIaEntry).size()) : 0;
  return n ? "MODS: " + std::to_string(n) + " on" : "MODS: off";
}

IaModsStep ia_mods_step(const std::vector<std::string>& before, const std::vector<std::string>& now, bool inside) {
  if (now.empty()) return inside ? IaModsStep::kPlain : IaModsStep::kBack;
  return now == before && inside ? IaModsStep::kBack : IaModsStep::kIntoEntry;
}

IaModsScreen::IaModsScreen(Engine& e, Handle s)
    : MenuScreen(e, s), accept_btn(e, kAccept), back(e, kBack), all_off(e, kAllOff), ticks(e) {
  mods_screen_target(e);
  const bool moving = !e.fast_menu();
  if (load("dialog.zrd", "IAMODSDIALOG", moving)) {
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
    if ((own_lines = mods_screen_lines(lines2.image, false))) lines2.set_image(own_lines);  // no box at the left
    bind_button(&accept_btn, "ACCEPT");
    bind_button(&back, "BACK");
    if (!moving) {
      bake();
      remove(&background);
      remove(&lines);
      remove(&lines2);
      accept_btn.set_baked(true);
      back.set_baked(true);
    }
    bind_button(&ticks.list, "MODLIST");
    bind_button(&ticks.toggle, "TOGGLE");
    bind_button(&ticks.up, "MOVEUP");
    bind_button(&ticks.down, "MOVEDOWN");
    bind_button(&all_off, "ALLOFF");
    unload();
  }
  mods_text_put(title, "main_red", 71, 21);
  mods_text_put(line, "main_green", 81, 38);
  mods_text_put(note, "ia_dark_grey", 384, 266);
  mods_text_put(note2, "ia_dark_grey", 384, 279);
  add(&title), add(&line), add(&note), add(&note2);
  set_flags_all(0);
  set_active(1);
  inside = inside_entry();
  before = entry_ticks(mods_menu_write_dir(), kIaEntry);
  ticks.fill(mods_menu_write_dir(), before);
  mods_text_say(title, "I N S T A N T   A C T I O N");
  show_note();
}

IaModsScreen::~IaModsScreen() {
  if (own_lines) {
    lines2.set_image(static_cast<zimg::Texture*>(nullptr));
    ztwod::host().image_release(own_lines);
  }
}

// Two lines under the list, the text wrapped at the box's width.
void IaModsScreen::tell(const std::string& text) {
  constexpr size_t kWidth = 58;
  std::string a = text, b;
  if (a.size() > kWidth) {
    size_t cut = a.rfind(' ', kWidth);
    if (cut == std::string::npos || cut < kWidth / 2) cut = kWidth;
    b = a.substr(cut + (a[cut] == ' ' ? 1 : 0));
    a.resize(cut);
    if (b.size() > kWidth) b = b.substr(0, kWidth - 3) + "...";
  }
  mods_text_say(note, a);
  mods_text_say(note2, b);
}

// The line follows the ticks as they stand on the screen; the note is the
// chosen row's (why it cannot be used, what it needs or shares), else what
// ACCEPT would do.
void IaModsScreen::show_note() {
  mods_text_say(line, ia_mods_line(mods_menu_write_dir(), static_cast<int>(ticks.ticks.size())));
  const std::string n = ticks.note();
  const std::vector<std::string> running = inside ? before : std::vector<std::string>{};
  tell(!n.empty() ? n
       : ticks.rows.empty() ? "No mods yet: IMPORT them on the main menu's MODS screen."
       : ticks.ticks != running ? "ACCEPT starts Instant Action again with these."
       : inside ? "These mods are on now." : "Tick the mods to play Instant Action with, then ACCEPT.");
}

void IaModsScreen::on_button(int32_t id, int32_t, int32_t) {
  if (ticks.on_button(id)) {
    show_note();
    if (!ticks.warned.empty()) tell(ticks.warned);
  } else if (id == kAllOff) {
    ticks.all_off();
    accept();
  } else if (id == kAccept) {
    accept();
  } else if (id == kBack) {
    engine().pop_state(0);
  }
  dirty_all();
}

void IaModsScreen::accept() {
  const std::string& dir = mods_menu_write_dir();
  const std::vector<CampaignEntry> all = campaign_entries(dir);
  const CampaignEntry* e = find_entry(all, kIaEntry);
  if (!e || !entry_save_ticks(dir, *e, ticks.ticks)) {
    tell("The mods could not be saved (the settings folder cannot be written).");
    return;
  }
  const IaModsStep step = ia_mods_step(before, ticks.ticks, inside);
  if (step == IaModsStep::kBack) {
    engine().pop_state(0);
    return;
  }
  std::string why;
  if (step == IaModsStep::kIntoEntry)
    for (const std::string& m : entry_missing_mods(dir, *e))
      why += (why.empty() ? "these mods are not there any more (turn them off or import them again): " : ", ") + m;
  if (why.empty() && !ia_mods_restart()) why = "the game cannot start again from here";
  if (!why.empty() || !ia_mods_restart()(step == IaModsStep::kIntoEntry, &why)) tell("Not started: " + why);
}

bool IaModsState::enter() {
  Engine& en = e();
  screen_ = en.construct(kIaModsScreenCtor, {addr()});
  ShellState::enter();
  return true;
}

void IaModsState::leave() {
  e().shell_clear(addr());
  ShellState::leave();
}

}  // namespace mech3
