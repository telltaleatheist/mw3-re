#include "mech3/debriefing.h"

#include <cmath>
#include <cstdio>

#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mission.h"
#include "mech3/pilot.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

// ---- the engine's defaults (app_hooks.h) ----

bool Engine::mission_failed() { return missions().aborting != 0; }
bool Engine::mission_cheated() { return missions().cheats != 0; }
float Engine::mission_time() { return missions().time; }
void Engine::pilot_save_now() {
  if (Pilot* p = current_pilot()) p->save();
}
void Engine::campaign_reset_lancemates() {
  if (CampaignGame* c = current_pilot() ? current_pilot()->campaign() : nullptr) c->reset_lancemates(false);
}
void Engine::campaign_set_b0(uint8_t v) {
  if (CampaignGame* c = current_pilot() ? current_pilot()->campaign() : nullptr) c->b0 = v;
}

// ---- the mech lab's mech button ----

// Mech3.exe 1.2 @ 0x0044a770
MechButton::MechButton(Engine& e) : HelpButton(e, -1) {}

// Mech3.exe 1.2 @ 0x0044a840: the name joins the dialog, hidden.
bool MechButton::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  const bool r = HelpButton::read_control(node, dialog);
  dialog->add(&name);
  name.set_font(ztwod::host().font_find("lab_small_white"));
  show_name(false);
  return r;
}

// Mech3.exe 1.2 @ 0x0044aa70: the mech's name under the image.
void MechButton::show_name(bool on) {
  if (!on) {
    name.hide();
    return;
  }
  name.show_for(-1.0f);
  name.set_text_plain(e_.mech_name_of(mech));
  const int32_t h = image ? image->height : 0;
  name.set_pos(get_x(), get_y() + h);
}

// ---- the lance cards ----

// Mech3.exe 1.2 @ 0x0047af70
LanceCard::LanceCard(Engine& e) : MechButton(e) {}

// Mech3.exe 1.2 @ 0x0047b180
LanceCard::~LanceCard() {
  vehicle = 0;
  if (display) e_.lance_display_delete(display);
  display = 0;
}

// Mech3.exe 1.2 @ 0x0047b2b0: Object2D's update (no blink), then the line's
// and the display's.
void LanceCard::update(float dt) {
  if (flags & ztwod::kHidden) return;
  Object2D::update(dt);
  line.update(dt);
  if (display) e_.lance_display_update(display, dt);
}

// Mech3.exe 1.2 @ 0x0047b070: HTAL places the mech's display (pos from the
// card, height, width, space). FIX (bug): a missing key left the stack's
// value; here 0.
bool LanceCard::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  const bool r = MechButton::read_control(node, dialog);
  if (zrdr::Node* h = zrdr::find(node, "HTAL")) {
    int32_t pos[2] = {}, height = 0, width = 0, space = 0;
    zrdr::get_ivec2(h, "pos", pos);
    pos[0] += get_x();
    pos[1] += get_y();
    zrdr::get_int(h, "height", &height);
    zrdr::get_int(h, "width", &width);
    zrdr::get_int(h, "space", &space);
    display = e_.lance_display_new(pos[0], pos[1], height, width, space);
  }
  return r;
}

// Mech3.exe 1.2 @ 0x0047b210: the display shows the vehicle (6c).
void LanceCard::set_vehicle(Handle v) {
  vehicle = v;
  if (v && display) e_.lance_display_show(display, v);
}

// ---- the screen ----

namespace {
// The Win Movie State ([0x00608be8]): the campaign's end.
constexpr Handle kWinMovieState = 0x608be8;
// The IA Dummy State ([0x00608bd0]): Instant Action's tallies, then the main menu.
constexpr Handle kIaDummyState = 0x608bd0;

ztwod::Text* label(ztwod::Control& c, size_t i) { return i < c.labels.size() ? c.labels[i] : nullptr; }
ztwod::Text* help(HelpButton& b) { return b.help.empty() ? nullptr : b.help[0]; }
}  // namespace

// Mech3.exe 1.2 @ 0x0047b430. Item 6a: the texts and buttons; the results
// (0x0047baf0..0x0047c84b) are a hook (6b).
DebriefScreen::DebriefScreen(Engine& e, Handle state, int32_t campaign, int32_t mission, const uint8_t* w)
    : MenuScreen(e, state),
      continue_btn(e, -1),
      replay_btn(e, -1),
      abort_btn(e, -1),
      cards{LanceCard(e), LanceCard(e), LanceCard(e), LanceCard(e)},
      rank(e),
      salvage(e, 0, 5),
      won(w) {
  clean = e.mission_cheated() ? 0 : 1;
  const int32_t fast = e.exchange_fast_menu(0);
  if (load("dlgWin.zrd", "WINMENU", true)) {
    bind_primitive(&background, "WIN_BACKGROUND");
    abort_btn.id = kAbort;
    bind_button(&abort_btn, "ICON_ABORT");
    replay_btn.id = kReplay;
    bind_button(&replay_btn, "ICON_REPLAY");
    continue_btn.id = kContinue;
    bind_button(&continue_btn, "ICON_CONTINUE");
    static const char* const kCards[] = {"ICON_PLAYER0", "ICON_PLAYER1", "ICON_PLAYER2", "ICON_PLAYER3"};
    for (int i = 0; i < 4; ++i) bind_button(&cards[i], kCards[i]);
    bind_button(&title, "WIN_TITLE");
    bind_button(&salvage, "SALVAGE_LIST");
    bind_button(&rank, "ICON_RANK");
    unload();
  }
  e.set_fast_menu(fast);
  set_flags_all(0);
  set_active(1);
  const int32_t kind = e.game_kind();
  abort_btn.enabled = 1;
  abort_btn.refresh();
  continue_btn.enabled = 1;
  continue_btn.refresh();
  // The mission's time. FIX (bug): the seconds were the time less the
  // minutes only, so past an hour 01:01:3640 (fixes.md, as the HUD's timer).
  const float t = e.mission_time();
  const float m = t * 0.016666668f;
  const float hours = std::floor(m * 0.016666668f);
  const float minutes = std::floor(m - hours * 60.0f);
  const float seconds = zfix::options().bugs ? std::floor(t - hours * 3600.0f - minutes * 60.0f)
                                             : std::floor(t - minutes * 60.0f);
  char time[32];
  std::snprintf(time, sizeof time, "%02d:%02d:%02d", static_cast<int32_t>(hours), static_cast<int32_t>(minutes),
                static_cast<int32_t>(seconds));
  if (kind == kInstantAction) {
    const char* name = e.game_text(0xc);
    const std::string s = e.message_format_is(0xdc0, campaign, name);
    if (ztwod::Text* l = label(title, 2)) {
      if (!s.empty()) l->set_text("%s", s.c_str());
      else l->set_text_plain(e.game_text(0xc));
    }
    if (ztwod::Text* l = label(title, 3)) l->set_text_plain(time);
    if (ztwod::Text* h = help(abort_btn)) h->set_text_plain(e.message(0xdbf));
    if (ztwod::Text* l = label(title, 1)) l->set_message(0x176);
    if (ztwod::Text* h = help(continue_btn)) h->set_message(0x173);
    salvage.show_for(-1.0f);
    replay_btn.hide();
  } else if (kind == kCampaign) {
    if (ztwod::Text* l = label(title, 2)) l->set_text_id(0x177, campaign, mission);
    if (ztwod::Text* l = label(title, 3)) l->set_text_plain(time);
    const bool failed = e.mission_failed();
    if (ztwod::Text* l = label(title, 1)) l->set_message(failed ? 0x180 : 0x181);
    if (ztwod::Text* h = help(continue_btn)) h->set_message(clean && !failed ? 0x174 : 0x173);
    if (!failed) {
      salvage.show_for(-1.0f);
      salvage.store = e.mission_salvage_store();
      salvage.x218 = 0;
      salvage.refill();
      if (clean) replay_btn.show_for(-1.0f);
      else replay_btn.hide();
    } else {
      salvage.hide();
      replay_btn.hide();
    }
    if (*won)
      if (ztwod::Text* h = help(continue_btn)) h->set_message(0xfb8);
  }
  e.debrief_results(this);  // 0x0047baf0..0x0047c84b: the lance, the points, the score, the rank (6b)
}

// Mech3.exe 1.2 @ 0x0047c920
DebriefScreen::~DebriefScreen() {
  for (ztwod::Text*& t : texts) {
    remove(t);  // 0x0059fab0
    delete t;
    t = nullptr;
  }
  texts.clear();
}

// Mech3.exe 1.2 @ 0x0042efe0
void DebriefScreen::update(float dt) { Window::update(dt); }

// Mech3.exe 1.2 @ 0x0047d030. CONTINUE: a campaign won goes to its movie;
// a mission passed (and clean) to the next briefing; failed or cheated, the
// lancemates go back and the same mission is briefed again. ABORT: to the
// main menu, the pilot saved (Instant Action: a fade to the IA Dummy
// State). REPLAY: the mission's voice again.
void DebriefScreen::on_button(int32_t id, int32_t a, int32_t b) {
  (void)a, (void)b;
  Engine& e = engine();
  const bool no_cheat = !e.mission_cheated();
  const int32_t kind = e.game_kind();
  if (id == kReplay) {
    e.debrief_voice_play(state);
    return;
  }
  if (id == kAbort) {
    if (kind == kInstantAction) {
      e.fade_to_state(kIaDummyState, 0, 1, 2, 2.0f, 0, 0, 0);  // [0x005e7274]: 2 s
      return;
    }
    if (kind != kCampaign) return;
    if (e.mission_failed() || !no_cheat) e.campaign_reset_lancemates();
    e.pilot_save_now();
    e.mission_cleanup(1);
    e.replace_state(2);
    return;
  }
  if (id != kContinue) return;
  if (kind == kMechLab) {
    e.mission_cleanup(0);
  } else if (kind == kCampaign) {
    if (*won) {
      e.replace_state_at(kWinMovieState);
      return;
    }
    if (!e.mission_failed() && no_cheat) {
      e.mission_cleanup(0);
    } else {
      e.campaign_reset_lancemates();
      int32_t op = 0, mission = 0;
      e.game_campaign(&op, &mission);
      e.mission_cleanup(mission == 1 || (op == 4 && mission == 4) ? 1 : 0);
      e.anim_reset();
      e.campaign_set_b0(0);
    }
  } else if (kind == kInstantAction) {
    e.action_next_round();
    e.mission_cleanup(0);
    e.anim_reset();
  } else {
    return;
  }
  e.replace_state(6);
}

}  // namespace mech3
