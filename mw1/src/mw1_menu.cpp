#include "mw1_menu.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "gamez/zinput/zinput.h"
#include "mech3/app_hooks.h"
#include "mw1_images.h"
#include "mw1_pilots.h"

namespace mw1::menu {
namespace {
// MW1's NEW, LOAD (the save: CONTINUE's id) and QUIT; OPTIONS (on the hub now) kept for its id, never shown.
const std::vector<ScreenButton> kMenuButtons = {{"NEW_CAMPAIGN", "NEW"}, {"CONTINUE", "LOAD"}, {"OPTIONS", "OPTIONS"}, {"QUIT", "QUIT"}};

// The narration's place and fonts (PM's fonts.zrd: Arial 16, grey then white), its lines 20 pixels apart.
constexpr int32_t kNarrationX = 165, kNarrationY = 132, kNarrationGap = 20;
constexpr const char* kGreyFont = "lab_large_grey";
constexpr const char* kWhiteFont = "lab_large_white";

float ticks(int32_t n) { return static_cast<float>(n) / kTicksPerSecond; }
}  // namespace

const char* menu_button_name(int32_t id) { return id >= 1 && id <= kMenuButtonCount ? kMenuButtons[id - 1].name : ""; }

// FIXES.md item 86 (waiting on Owen): MW1_MENU_TITLE=up or low (development) lays the menu out as one of the two
// choices for the title over the 'Mech's head (mw1menu.zrd's MW1MENUDIALOG_UP, _LOW); else as it is.
namespace {
const char* menu_dialog() {
  const char* t = std::getenv("MW1_MENU_TITLE");
  if (t && std::string(t) == "up") return "MW1MENUDIALOG_UP";
  if (t && std::string(t) == "low") return "MW1MENUDIALOG_LOW";
  return kMenuDialog;
}
}  // namespace

MenuState::MenuState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Menu State", kMenuFile, menu_dialog(), kMenuButtons, 0, std::move(pressed)) {}

void MenuState::show_menu(bool continue_on, const std::string& save_line) {
  ScreenView s;
  s.blocks = {at_place("TITLE", {"MECHWARRIOR"}), at_place("SUBTITLE", {"CRITICAL HIT"})};
  if (!save_line.empty()) s.blocks.push_back(at_place("SAVE", {save_line}));
  s.buttons.resize(kMenuButtonCount);
  s.buttons[kMenuContinue - 1].on = continue_on;
  s.buttons[kMenuOptions - 1].there = false;
  show(s);
}

std::vector<std::string> intro_narration() {
  const std::vector<uint8_t>& d = mw_main_data_segment();
  std::vector<std::string> lines;
  size_t at = kNarrationAt;
  for (int32_t i = 0; i < kNarrationLines; ++i) {
    std::string l;
    while (at < d.size() && d[at] && l.size() < 0x50) l += static_cast<char>(d[at++]);
    if (at >= d.size() || d[at] || l.empty()) return {};  // not MW1's exe as we know it
    ++at;
    lines.push_back(l);
  }
  return lines;
}

IntroPhase intro_phase(float s) {
  if (s < ticks(kFadeTicks)) return IntroPhase::kGrey;
  if (s < ticks(kFadeTicks + kQuietTicks + kNarrationTicks)) return IntroPhase::kWhite;
  if (s < intro_length()) return IntroPhase::kPicture;
  return IntroPhase::kDone;
}
float intro_length() { return ticks(kFadeTicks + kQuietTicks + kNarrationTicks + kPictureTicks); }

IntroState::IntroState(zgame::Handle h, std::vector<std::string> lines, std::function<void()> done)
    : Mw1State(h, "MW1 Intro State", kIntroFile, kIntroDialog, {}, 0, nullptr), lines_(std::move(lines)), done_(std::move(done)) {}

bool IntroState::enter() {
  // No narration (no MW1 exe to read it from): straight to the picture, not 25 s of an empty screen.
  const float picture = ticks(kFadeTicks + kQuietTicks + kNarrationTicks);
  seconds_ = lines_.empty() && start_ < picture ? picture : start_;
  frames_ = 0, finished_ = false, shown_ = IntroPhase::kDone;
  const zin::MouseInfo* m = zin::mouse_info();
  mouse_was_down_ = m && m->left;
  show_phase();
  const bool r = Mw1State::enter();
  return r;
}

void IntroState::show_phase() {
  const IntroPhase p = phase();
  if (p == shown_) return;
  shown_ = p;
  ScreenView s;
  if (p == IntroPhase::kGrey || p == IntroPhase::kWhite) {
    TextBlock b;
    b.x = kNarrationX, b.y = kNarrationY, b.gap = kNarrationGap, b.font = p == IntroPhase::kGrey ? kGreyFont : kWhiteFont;
    b.lines = lines_;
    s.blocks.push_back(b);
  } else {
    s.blocks.push_back({});  // the narration's lines hidden
  }
  s.panels = {p == IntroPhase::kPicture};  // PANEL: the picture
  show(s);
}

bool IntroState::update() {
  const bool r = Mw1State::update();
  if (finished_) return r;
  seconds_ += e().frame_dt();
  ++frames_;
  // Any key or a click skips (MW1's MwWaitTicksOrInputInterruption): a key this frame, the left button newly down.
  const zin::MouseInfo* m = zin::mouse_info();
  const bool down = m && m->left;
  const bool click = down && !mouse_was_down_;
  mouse_was_down_ = down;
  if (frames_ > 1 && (zin::frame_key() != 0 || click)) {
    std::printf("mw1: the intro skipped at %.1f s\n", seconds_);
    finish();
    return r;
  }
  if (phase() == IntroPhase::kDone) {
    finish();
    return r;
  }
  show_phase();
  return r;
}

void IntroState::finish() {
  if (finished_) return;
  finished_ = true;
  if (done_) done_();
}

// ---- The credits ----

std::vector<CreditPanel> mw1_credits() {
  const std::vector<uint8_t>& d = mw_main_data_segment();
  std::vector<CreditPanel> roll;
  for (const auto& panel : kCreditTexts) {
    CreditPanel p;
    for (uint16_t at : panel) {
      if (static_cast<size_t>(at) + 4 + 0x80 > d.size()) return {};
      CreditText t;
      t.x = d[at] | d[at + 1] << 8, t.y = d[at + 2] | d[at + 3] << 8;
      if (t.x >= 320 || t.y >= 200) return {};  // not MW1's exe as we know it
      t.lines.emplace_back();
      size_t i = at + 4;
      for (; i < static_cast<size_t>(at) + 4 + 0x80 && d[i]; ++i) {
        const uint8_t ch = d[i];
        if (ch == 0x0D) t.lines.emplace_back();
        else if (ch >= 0x20 && ch < 0x7f) t.lines.back() += static_cast<char>(ch);
        else return {};
      }
      if (d[i] || t.lines.front().empty()) return {};
      p.texts.push_back(std::move(t));
    }
    roll.push_back(std::move(p));
  }
  roll.back().headed = false;  // the trademark notice
  return roll;
}

std::vector<CreditPanel> extra_credits() { return {}; }

std::vector<CreditPanel> credits_roll() {
  std::vector<CreditPanel> roll = mw1_credits();
  for (CreditPanel& p : extra_credits()) roll.push_back(std::move(p));
  return roll;
}

std::vector<CreditLine> credits_layout(const std::vector<CreditPanel>& roll) {
  std::vector<CreditLine> out;
  int32_t top = 0;
  for (const CreditPanel& p : roll) {
    if (p.texts.empty()) continue;
    int32_t first = p.texts.front().y, foot = top;
    for (const CreditText& t : p.texts) first = std::min(first, t.y);
    for (const CreditText& t : p.texts) {
      size_t widest = 0;
      for (const std::string& l : t.lines) widest = std::max(widest, l.size());
      const int32_t x = widest > static_cast<size_t>(kCreditColumnChars) ? kCreditMiddleX : t.x < 160 ? kCreditLeftX : kCreditRightX;
      int32_t y = top + (t.y - first) * 2;
      for (size_t i = 0; i < t.lines.size(); ++i) {
        if (t.lines[i].empty()) {
          y += kCreditGap / 2;
          continue;
        }
        out.push_back({x, y, p.headed && i == 0, t.lines[i]});
        y += kCreditGap;
      }
      foot = std::max(foot, y);
    }
    top = foot + kCreditPanelGap;
  }
  return out;
}

int32_t credits_height(const std::vector<CreditLine>& lines) {
  int32_t h = 0;
  for (const CreditLine& l : lines) h = std::max(h, l.y + kCreditGap);
  return h;
}

float credits_length(const std::vector<CreditLine>& lines) {
  // From the first line entering at the window's foot to the last leaving at its top.
  return static_cast<float>(credits_height(lines) + kCreditsBottom - kCreditsTop) / kCreditsSpeed;
}

CreditsState::CreditsState(zgame::Handle h, std::vector<CreditPanel> roll, std::string art, std::function<void()> done)
    : Mw1State(h, "MW1 Credits State", kCreditsFile, kCreditsDialog, {}, 0, nullptr),
      lines_(credits_layout(roll)),
      art_(std::move(art)),
      done_(std::move(done)) {
  length_ = credits_length(lines_);
}

bool CreditsState::enter() {
  seconds_ = start_, frames_ = 0, finished_ = false, shown_at_ = -1;
  const zin::MouseInfo* m = zin::mouse_info();
  mouse_was_down_ = m && m->left;
  show_roll();
  return Mw1State::enter();
}

void CreditsState::show_roll() {
  // The roll's top this far below the window's foot, rising; a line shown while it's inside the window.
  const int32_t scrolled = static_cast<int32_t>(seconds_ * kCreditsSpeed);
  if (scrolled == shown_at_) return;
  shown_at_ = scrolled;
  ScreenView s;
  s.backdrop = art_;
  shown_count_ = 0;
  for (const CreditLine& l : lines_) {
    const int32_t y = kCreditsBottom + l.y - scrolled;
    if (y < kCreditsTop || y + kCreditGap > kCreditsBottom) continue;
    TextBlock b;
    b.x = l.x, b.y = y, b.font = l.head ? kCreditHeadFont : kCreditFont, b.lines = {l.text};
    s.blocks.push_back(std::move(b));
    ++shown_count_;
  }
  if (s.blocks.empty()) s.blocks.push_back({});
  show(s);
}

bool CreditsState::update() {
  const bool r = Mw1State::update();
  if (finished_) return r;
  seconds_ += e().frame_dt();
  ++frames_;
  const zin::MouseInfo* m = zin::mouse_info();
  const bool down = m && m->left;
  const bool click = down && !mouse_was_down_;
  mouse_was_down_ = down;
  if (frames_ > 1 && (zin::frame_key() != 0 || click)) {
    std::printf("mw1: the credits skipped at %.1f s\n", seconds_);
    finish();
    return r;
  }
  if (seconds_ >= length_) {
    finish();
    return r;
  }
  show_roll();
  return r;
}

void CreditsState::finish() {
  if (finished_) return;
  finished_ = true;
  if (done_) done_();
}

}  // namespace mw1::menu
