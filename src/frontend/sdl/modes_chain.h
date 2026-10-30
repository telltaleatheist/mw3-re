#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace mech3 {
class GameEngine;
class Mech3App;
class Objective;
}
namespace zgame {
class State;
}

// Instant Action played through as a player plays it (ours, not original
// code; docs/campaign.md section 15): `--action-chain all` takes a new pilot
// from the main menu's INSTANT ACTION to the Action screen and, for each
// world and scenario it offers, chooses them (and a line-up), ACCEPTs and
// LAUNCHes; the enemies are shot as the campaign chain shoots them until the
// mission's objectives end it (the wave modes: two waves cleared, then the
// pause menu's QUIT). The debriefing's CONTINUE plays the next round (the
// animations reset, 0x0050ad60), lost or won the other way round, and its
// ABORT ends the case through the IA Dummy State back to the Action screen.
namespace sdlfe {

class ModesChain {
 public:
  ModesChain(mech3::GameEngine& e, mech3::Mech3App& app, std::string which);
  void frame();  // each frame shown
  bool done() const { return done_; }
  std::function<void(int dik, bool down)> key;  // a key pressed as the keyboard does (mw3_main's)

 private:
  void action_screen();
  void core();
  void debriefing();
  void next_case();
  // Training (`training`): t1's four missions from the pilot screen's
  // TRAINING, each lesson's objectives met as a player meets them.
  void training_core();
  void lesson_act();
  void act_on(const mech3::Objective* o);
  void press(const char* command, int hold = 2);  // its first key, down now and up `hold` frames later
  void press_key(int code, int hold = 2);
  bool training_ = false;
  int lesson_ = -2, lesson_at_ = 0, act_ = 0, missions_done_ = 0, stuck_ = 0;
  std::vector<std::pair<int, int>> releases_;  // (frame, dik)
  int frames_ = 0;

  mech3::GameEngine& e_;
  mech3::Mech3App& app_;
  bool done_ = false;
  const zgame::State* last_ = nullptr;
  int frames_in_state_ = 0, core_frames_ = 0;
  // The case: world 0..3, scenario row 1..5; the round 1 or 2; whether this
  // round is to be won.
  int world_ = 0, scenario_ = 1, round_ = 1, cases_ = 0, first_world_ = 0, last_world_ = 3;
  int first_scenario_ = 1, last_scenario_ = 5;
  bool win_round_ = true, pilot_done_ = false, accepted_ = false, ended_logged_ = false, quitting_ = false;
  int kills_ = 0, hits_ = 0, waves_start_ = 0;
  int results_ok_ = 0, results_bad_ = 0;
  std::string case_;
};

}  // namespace sdlfe
