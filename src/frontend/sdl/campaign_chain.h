#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mech3 {
class GameEngine;
class Mech3App;
}  // namespace mech3

// --campaign-chain (ours, for checking the campaign as a player plays it;
// program.md): a pilot driven through the real states, screen by screen, by
// the presses a player makes. The main menu's NEW CAMPAIGN, a new pilot (or,
// resuming, the write dir's first pilot and its newest game), each briefing
// with the lab opened and accepted and SALVAGE's first mission row moved
// over, LAUNCH, the mission won by Exit Mission (as --win), the debriefing
// and CONTINUE, then the next briefing. Each briefing and debriefing is shot
// to DIR/chain_<n>_<what>.bmp, each step printed as "mw3: chain ...". It
// ends at the briefing after `wins` missions won (the game saved at each
// debriefing), or after the campaign's end: the win movie, the main menu
// pressing its credits, CANCEL there, and the main menu's NEW CAMPAIGN
// reaching the pilot screen again. Every state entered is printed.
namespace sdlfe {

class SdlRender3D;

struct ChainOptions {
  bool resume = false;  // the existing pilot, not a new one
  int wins = 4;         // the missions to win
  bool kills = false;   // won by shooting every enemy (kills:N), not Exit Mission at once
  std::string dir;      // where the shots go
};

class CampaignChain {
 public:
  CampaignChain(mech3::GameEngine& e, mech3::Mech3App& app, SdlRender3D* render, ChainOptions o)
      : e_(e), app_(app), render_(render), o_(std::move(o)) {}
  ~CampaignChain();
  void frame();  // each frame shown
  bool done() const { return done_; }

 private:
  void briefing();
  void shoot(const char* what);
  std::string mission_name() const;
  mech3::GameEngine& e_;
  mech3::Mech3App& app_;
  SdlRender3D* render_;
  ChainOptions o_;
  bool done_ = false;
  int wins_ = 0, shots_ = 0, frames_in_state_ = 0, core_frames_ = 0;
  const void* last_state_ = nullptr;
  // This briefing's steps: 0 loading, 1 shot, 2 lab done, 3 salvage done, 4 launched.
  int brief_step_ = 0, wait_ = 0;
  bool pilot_done_ = false;
  // The campaign's end: every state the app enters, printed as it is entered
  // (one that lasts a frame too), the credits entered, then the main menu.
  void on_state(const void* s);
  bool hooked_ = false, after_credits_ = false;
  int frames_ = 0, credits_entered_ = 0;
  const void* logged_state_ = nullptr;
  void shoot_hostiles();  // kills: each Core frame's hits
  int hits_ = 0, kills_ = 0, idle_frames_ = 0;
  // The mission's salvage by where it came from: "kind/id" -> units.
  using Counts = std::map<std::string, int32_t>;
  Counts salvage_seen_, salvage_kills_, salvage_other_;
  void debrief_salvage();   // kills: the debriefing's list against them
  void salvage_weapons();   // kills: SALVAGE's weapon rows moved over
  void lab_fit();           // kills: a salvaged weapon fitted in the lab
  std::vector<int32_t> moved_weapons_;  // the weapons SALVAGE moved to the store
  int32_t fitted_weapon_ = -1, fitted_count_ = 0;  // the lab's fit, checked in the next mission
  int killed_units_ = 0, mismatched_units_ = 0;
  std::string launched_;  // the mission LAUNCH started
  std::string shot_path_;  // the shot asked for, alive until it is written
};

}  // namespace sdlfe
