#include "frontend/sdl/campaign_chain.h"

#include <cstdio>

#include "frontend/sdl/sdl_render3d.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/briefing.h"
#include "mech3/debriefing.h"
#include "mech3/lab_screen.h"
#include "mech3/mainmenu.h"
#include "mech3/menu_states.h"
#include "mech3/mission.h"
#include "mech3/pilot.h"
#include "mech3/pilot_menu.h"
#include "mech3/salvage_screen.h"

namespace sdlfe {

namespace {
// A store as "mechs/units in items": its mechs and all its units.
std::string store_line(const mech3::Store& st) {
  int32_t mechs = 0, units = 0;
  for (const mech3::ConfigItem* it : st.items) mechs += it->kind() == 0 ? it->count : 0, units += it->count;
  char buf[64];
  std::snprintf(buf, sizeof buf, "%d mechs %d units in %zu", mechs, units, st.items.size());
  return buf;
}
}  // namespace

std::string CampaignChain::mission_name() const {
  const mech3::Pilot* p = mech3::current_pilot();
  const auto* c = p ? dynamic_cast<const mech3::CampaignGame*>(p->current) : nullptr;
  if (!c) return "?";
  char buf[32];
  std::snprintf(buf, sizeof buf, "op %d mission %d", c->operation(), c->mission_number());
  return buf;
}

void CampaignChain::shoot(const char* what) {
  if (!render_ || o_.dir.empty()) return;
  char path[512];
  for (;;) {  // the next free number: a resumed run adds to the first run's shots
    std::snprintf(path, sizeof path, "%s/chain_%02d_%s.bmp", o_.dir.c_str(), shots_, what);
    bool taken = false;
    for (const char* w : {"briefing", "debriefing"}) {
      char p2[512];
      std::snprintf(p2, sizeof p2, "%s/chain_%02d_%s.bmp", o_.dir.c_str(), shots_, w);
      if (std::FILE* f = std::fopen(p2, "rb")) std::fclose(f), taken = true;
    }
    if (!taken) break;
    ++shots_;
  }
  std::snprintf(path, sizeof path, "%s/chain_%02d_%s.bmp", o_.dir.c_str(), shots_++, what);
  shot_path_ = path;  // capture_next keeps the pointer until the frame is shown
  render_->capture_next(shot_path_.c_str());
  std::printf("mw3: chain shot %s (%s)\n", path, mission_name().c_str());
}

// The briefing: loaded, shot, the lab opened (when lit) and accepted, the
// salvage screen's last mission row moved over (when lit), then LAUNCH.
void CampaignChain::briefing() {
  mech3::BriefingScreen* b = mech3::briefing_screen();
  if (!b || !b->loaded) return;
  if (wait_ > 0) {
    --wait_;
    return;
  }
  mech3::Pilot* p = mech3::current_pilot();
  auto* c = p ? dynamic_cast<mech3::CampaignGame*>(p->current) : nullptr;
  switch (brief_step_) {
    case 0:
      if (c) {
        std::string lance;
        for (const mech3::GameSlot* l : c->lancemates) lance += " " + std::to_string(l ? l->mech : -2);
        std::printf("mw3: chain briefing %s: store {%s}, salvage {%s}, slot mech %d, lancemates [%s ] (x9c %d), bases %d, wins %d\n",
                    mission_name().c_str(), store_line(c->store).c_str(), store_line(c->store3c).c_str(), c->slot.mech,
                    lance.c_str(), c->x9c, c->a8, wins_);
      }
      shoot("briefing");
      brief_step_ = 1, wait_ = 5;
      return;
    case 1:
      if (wins_ >= o_.wins) {  // the shot written; the save was the debriefing's
        std::printf("mw3: chain done at %s after %d wins\n", mission_name().c_str(), wins_);
        done_ = true;
        return;
      }
      brief_step_ = 2;
      if (b->lab_btn.enabled) {
        std::printf("mw3: chain MECHLAB\n");
        b->on_button(mech3::BriefingScreen::kBtnMechLab, 0, 0);
        return;
      }
      [[fallthrough]];
    case 2:
      brief_step_ = 3;
      if (b->salvage_btn.enabled) {
        std::printf("mw3: chain SALVAGE\n");
        b->on_button(mech3::BriefingScreen::kBtnSalvage, 0, 0);
        return;
      }
      [[fallthrough]];
    default:
      brief_step_ = 4;
      launched_ = mission_name();
      std::printf("mw3: chain LAUNCH %s\n", launched_.c_str());
      b->on_button(mech3::BriefingScreen::kBtnLaunch, 0, 0);
  }
}

void CampaignChain::frame() {
  if (done_) return;
  mech3::States& st = mech3::states();
  const zgame::State* now = app_.current();
  if (now != last_state_) last_state_ = now, frames_in_state_ = 0;
  ++frames_in_state_;
  if (now == &st.main && mech3::main_menu() && frames_in_state_ == 30) {
    if (wins_ > 0) {  // back at the main menu after the campaign's end
      std::printf("mw3: chain main menu after %d wins\n", wins_);
      done_ = true;
      return;
    }
    mech3::main_menu()->buttons[1].on_left_up();  // NEW CAMPAIGN
  } else if (auto* ps = dynamic_cast<mech3::PilotState*>(app_.current())) {
    if (!ps->screen || pilot_done_ || frames_in_state_ < 20 || !app_.pending_list().empty()) return;
    if (o_.resume) {
      // The write dir's pilot; the screen then chooses its newest game
      // (0x004882b0: the last of the list), as a player who only picks the pilot.
      ps->screen->pilots.select(0);
      for (int32_t i = 0; i < ps->screen->games.item_count(); ++i)
        std::printf("mw3: chain game %d: %s\n", i, ps->screen->games.items[static_cast<size_t>(i)]->text);
      if (mech3::Pilot* p = ps->screen->chosen)
        if (!p->campaigns.empty())
          if (auto* g = dynamic_cast<mech3::CampaignGame*>(p->campaigns.back()))
            std::printf("mw3: chain newest game, loaded: store {%s}, salvage {%s}, lancemates %zu\n",
                        store_line(g->store).c_str(), store_line(g->store3c).c_str(), g->lancemates.size());
    } else {
      ps->screen->new_bar.set_text("Chain");
      ps->screen->new_bar.on_return();
    }
    std::printf("mw3: chain pilot %s\n", o_.resume ? "resumed" : "new");
    ps->screen->on_button(0x18, 0, 0);  // ACCEPT
    pilot_done_ = true;
  } else if (now == &st.load) {
    if (frames_in_state_ == 1 && brief_step_ >= 4) {  // a new briefing
      brief_step_ = 0;
      mech3::Pilot* p = mech3::current_pilot();
      if (auto* c = p ? dynamic_cast<mech3::CampaignGame*>(p->current) : nullptr) {
        std::string lance;
        for (const mech3::GameSlot* l : c->lancemates)
          lance += " " + std::to_string(l ? l->mech : -2) + (l && l->config ? "+cfg" : "");
        std::printf("mw3: chain load %s (before the briefing's lance): salvage {%s}, lancemates [%s ]\n",
                    mission_name().c_str(), store_line(c->store3c).c_str(), lance.c_str());
      }
    }
    if (frames_in_state_ == 1 && brief_step_ < 4 && brief_step_ > 0) wait_ = 10;  // back from the lab or salvage
    briefing();
  } else if (now == &st.lab && frames_in_state_ == 60) {
    const mech3::LabState& lab = st.lab;
    if (mech3::LabScreen* ls = e_.lab_screen(lab.lab_screen_)) {
      std::printf("mw3: chain lab: %d slots, ACCEPT\n", lab.slots_);
      ls->on_button(6, 0, 0);  // ACCEPT
    }
  } else if (now == &st.briefing_salvage && frames_in_state_ == 30) {
    auto* sv = dynamic_cast<mech3::SalvageScreen*>(e_.screen_of(st.briefing_salvage.screen_));
    if (sv) {
      const int32_t rows = sv->mission.item_count();
      if (rows > 1) sv->mission.select(rows - 1);
      std::printf("mw3: chain salvage: %d mission rows, one moved; ACCEPT\n", rows);
      sv->on_button(mech3::SalvageScreen::kAccept, 0, 0);
    }
  } else if (now == &st.core) {
    if (frames_in_state_ == 1) core_frames_ = 0;
    if (++core_frames_ == 60) {  // won, as --win: Exit Mission with the objectives met
      std::printf("mw3: chain win %s\n", launched_.c_str());
      mech3::missions().ended = 1;
      mech3::missions().on_command(0x71);
    }
  } else if (now == &st.end_win) {
    if (frames_in_state_ == 60) shoot("debriefing");
    if (frames_in_state_ == 70) {
      auto* d = dynamic_cast<mech3::DebriefScreen*>(e_.screen_of(st.end_win.screen_));
      if (d) {
        ++wins_;
        std::printf("mw3: chain debriefing after %s: score %d, CONTINUE (wins %d; the game now at %s)\n",
                    launched_.c_str(), d->score, wins_, mission_name().c_str());
        d->on_button(mech3::DebriefScreen::kContinue, 0, 0);
        brief_step_ = 4;  // the next Load state is a new briefing
      }
    }
  } else if (dynamic_cast<const mech3::CreditsState*>(now) && wins_ > 0 && frames_in_state_ == 30) {  // the campaign's end
    std::printf("mw3: chain credits after %d wins\n", wins_);
    done_ = true;
  } else if (frames_in_state_ == 3000) {
    const auto* ms = dynamic_cast<const mech3::State*>(now);
    std::printf("mw3: chain stuck in %s\n", ms ? ms->name() : "?");
    done_ = true;
  }
}

}  // namespace sdlfe
