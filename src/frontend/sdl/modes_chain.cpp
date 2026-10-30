#include "frontend/sdl/modes_chain.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "mech3/action_menu.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_sensors.h"
#include "mech3/app_states.h"
#include "mech3/briefing.h"
#include "mech3/debriefing.h"
#include "mech3/mainmenu.h"
#include "mech3/mission.h"
#include "mech3/pause_menu.h"
#include "mech3/pilot.h"
#include "mech3/pilot_menu.h"

namespace sdlfe {
namespace {

const char* const kScenarios[] = {"None", "Battle", "Battle Waves", "Battle Progression", "Attack", "Defend"};
const char* const kWorlds[] = {"c1", "c2", "c3", "c4"};

struct Tally {
  int32_t player = 0, attack_wins = 0, attack_losses = 0, defend_wins = 0, defend_losses = 0, waves = 0;
};
Tally tally() {
  Tally t;
  mech3::Pilot* p = mech3::current_pilot();
  if (mech3::ActionGame* g = p ? p->action() : nullptr)
    t = {g->player_kills, g->attack_wins, g->attack_losses, g->defend_wins, g->defend_losses, g->waves};
  return t;
}
Tally g_before;

}  // namespace

// "all", or "<world>:<scenario>" (c2:4), or a world alone (c3).
ModesChain::ModesChain(mech3::GameEngine& e, mech3::Mech3App& app, std::string which) : e_(e), app_(app) {
  if (which != "all" && !which.empty()) {
    const int w = which.size() >= 2 && which[0] == 'c' ? which[1] - '1' : -1;
    if (w >= 0 && w <= 3) first_world_ = last_world_ = w;
    const size_t colon = which.find(':');
    if (colon != std::string::npos) first_scenario_ = last_scenario_ = std::atoi(which.c_str() + colon + 1);
  }
  world_ = first_world_, scenario_ = first_scenario_;
}

void ModesChain::frame() {
  if (done_) return;
  mech3::States& st = mech3::states();
  zgame::State* now = app_.current();
  if (now != last_) last_ = now, frames_in_state_ = 0;
  ++frames_in_state_;
  if (now == &st.main && mech3::main_menu() && frames_in_state_ == 30 && !pilot_done_) {
    const_cast<mech3::MainMenu*>(mech3::main_menu())->buttons[0].on_left_up();  // INSTANT ACTION
  } else if (auto* ps = dynamic_cast<mech3::PilotState*>(now)) {
    if (!ps->screen || pilot_done_ || frames_in_state_ < 20 || !app_.pending_list().empty()) return;
    ps->screen->new_bar.set_text("Actions");
    ps->screen->new_bar.on_return();
    ps->screen->on_button(0x18, 0, 0);  // ACCEPT
    pilot_done_ = true;
  } else if (auto* gs = dynamic_cast<mech3::GameMenuState*>(now); gs && gs->kind == mech3::GameMenuState::kAction) {
    if (frames_in_state_ == 20) action_screen();
  } else if (now == &st.load) {
    mech3::BriefingScreen* b = mech3::briefing_screen();
    if (b && b->loaded && frames_in_state_ > 10 && !accepted_) {
      accepted_ = true;
      b->on_button(mech3::BriefingScreen::kBtnLaunch, 0, 0);
    }
  } else if (now == &st.core) {
    if (frames_in_state_ == 1) core_frames_ = 0, kills_ = hits_ = 0, ended_logged_ = quitting_ = false;
    core();
  } else if (now == &st.pause) {
    auto* p = dynamic_cast<mech3::PauseScreen*>(e_.screen_of(st.pause.screen_));
    if (p && frames_in_state_ == 5) p->on_button(mech3::PauseScreen::kQuit, 0, 0);
  } else if (now == &st.end_win && frames_in_state_ == 60) {
    debriefing();
  }
}

// The case's choices on the Action screen, then ACCEPT. Back from a case's
// end (the IA Dummy State reopened it) the last case's tallies are checked.
void ModesChain::action_screen() {
  auto* gs = static_cast<mech3::GameMenuState*>(app_.current());
  auto* a = dynamic_cast<mech3::ActionScreen*>(e_.screen_of(gs->screen_));
  if (!a) return;
  if (!case_.empty()) {
    const Tally t = tally();
    const bool attack = scenario_ == 4, defend = scenario_ == 5;
    const bool counted = (attack && (win_round_ ? t.attack_wins == g_before.attack_wins + 1 : t.attack_losses == g_before.attack_losses + 1)) ||
                         (defend && (win_round_ ? t.defend_wins == g_before.defend_wins + 1 : t.defend_losses == g_before.defend_losses + 1)) ||
                         (!attack && !defend && t.attack_wins == g_before.attack_wins && t.defend_wins == g_before.defend_wins &&
                          t.attack_losses == g_before.attack_losses && t.defend_losses == g_before.defend_losses);
    std::printf("mw3: modes case %s ended: player kills %d -> %d, attack %d/%d, defend %d/%d, best wave %d: %s\n",
                case_.c_str(), g_before.player, t.player, t.attack_wins, t.attack_losses, t.defend_wins, t.defend_losses,
                t.waves, counted ? "tallied as the exe tallies" : "NOT tallied as expected");
    counted ? ++results_ok_ : ++results_bad_;
    next_case();
    if (done_) return;
  }
  // The world, the scenario (its step refills the enemies), a line-up.
  a->worlds.select(world_);
  a->scenarios.step(scenario_ - 1 - a->scenarios.index);  // its rows: Battle (0) .. Defend (4)
  const int lancemates = cases_ % 3;  // none, one, two
  for (int i = 0; i < lancemates; ++i) a->allies[2 + i].select(6 + i);
  if (scenario_ >= 4) a->allies[1].select(1);  // the field base, in Attack and Defend
  int enemies = 1;
  if (a->fixed)  // a custom line-up (+0x16f9c 1): up to four enemies; else the wave's
    for (int i = 1; i <= cases_ % 4; ++i) a->enemies[i].select(3 + i), ++enemies;
  win_round_ = cases_ % 2 == 0;  // the round ABORT tallies: won in even cases, lost in odd
  round_ = 1;
  case_ = std::string(kWorlds[world_]) + " " + kScenarios[scenario_];
  std::printf("mw3: modes case %s: world %d, scenario %d (%s), %d lancemate(s), %d enemy slot(s)%s; ends %s\n",
              case_.c_str(), a->worlds.selected_value(), a->scenarios.index, a->scenarios.labels.empty() ? "?" : a->scenarios.labels[0]->text,
              lancemates, enemies, a->fixed ? "" : " (the wave's)", win_round_ ? "won" : "lost");
  g_before = tally();
  accepted_ = false;
  a->on_button(4, 0, 0);  // ACCEPT
}

void ModesChain::next_case() {
  ++cases_;
  if (++scenario_ > last_scenario_) scenario_ = first_scenario_, ++world_;
  if (world_ > last_world_) {
    std::printf("mw3: modes done: %d cases, %d tallied as expected, %d not\n", cases_, results_ok_, results_bad_);
    done_ = true;
  }
}

// The round: won (shooting until the objectives end the mission; the wave
// modes two waves cleared, then QUIT) or lost (the pause's REPLAY at frame 90).
void ModesChain::core() {
  ++core_frames_;
  mech3::Missions& m = mech3::missions();
  mech3::Pilot* p = mech3::current_pilot();
  mech3::ActionGame* g = p ? p->action() : nullptr;
  const int mode = g ? g->mode : 0;
  const bool waves = mode == 1 || mode == 2;
  const bool win = round_ == 2 ? win_round_ : !win_round_;
  if (core_frames_ == 2) {
    waves_start_ = g ? g->x6c : 0;
    std::printf("mw3: modes %s round %d: world index %d loaded, the game's world %d mission %d mode %d wave %d\n",
                case_.c_str(), round_, m.current, g ? g->x74 : -1, g ? g->x78 : -1, mode, waves_start_);
  }
  if (m.ended) {
    if (!ended_logged_) {
      ended_logged_ = true;
      std::printf("mw3: modes %s round %d %s at core frame %d (%d kills, %d hits)\n", case_.c_str(), round_,
                  m.aborting ? "lost" : "won by its objectives", core_frames_, kills_, hits_);
      if (!m.aborting) m.on_command(0x71);  // Exit Mission
    }
    return;
  }
  if (quitting_) return;
  if (!win && core_frames_ == 90) {
    m.abort_now();  // the pause's REPLAY: lost at once (0x0048fa80)
    return;
  }
  if (core_frames_ > 2 && win && waves && g && g->x6c >= waves_start_ + 2) {  // the start read at frame 2
    std::printf("mw3: modes %s round %d: waves %d and %d cleared at core frame %d (%d kills); QUIT\n", case_.c_str(), round_,
                waves_start_, waves_start_ + 1, core_frames_, kills_);
    quitting_ = true;
    app_.push(app_.state(3), 0);  // Escape, as the Core State's update pushes it: the Pause State, whose QUIT leaves
    return;
  }
  if (core_frames_ == 20000) {
    std::printf("mw3: modes %s round %d NOT won by its objectives after 20000 frames: Exit Mission\n", case_.c_str(), round_);
    m.ended = 1;
    m.on_command(0x71);
    return;
  }
  if (core_frames_ < 60 || !win) return;
  static void* gun = nullptr;
  if (!gun) gun = mech3::chain_weapon();
  for (const mech3::ChainTarget& t : mech3::chain_hostiles()) {
    ++hits_;
    if (mech3::chain_hit(t.h, gun)) ++kills_;
  }
}

// Round 1's debriefing CONTINUEs (the next round), round 2's ABORTs.
void ModesChain::debriefing() {
  mech3::States& st = mech3::states();
  auto* d = dynamic_cast<mech3::DebriefScreen*>(e_.screen_of(st.end_win.screen_));
  if (!d) return;
  if (round_ == 1) {
    std::printf("mw3: modes %s debriefing round 1: score %d, CONTINUE (the next round)\n", case_.c_str(), d->score);
    round_ = 2, accepted_ = false;
    d->on_button(mech3::DebriefScreen::kContinue, 0, 0);
  } else {
    std::printf("mw3: modes %s debriefing round 2: score %d, ABORT\n", case_.c_str(), d->score);
    d->on_button(mech3::DebriefScreen::kAbort, 0, 0);
  }
}

}  // namespace sdlfe
