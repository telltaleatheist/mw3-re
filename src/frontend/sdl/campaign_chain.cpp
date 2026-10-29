#include "frontend/sdl/campaign_chain.h"

#include <algorithm>
#include <cstdio>
#include <functional>

#include "frontend/sdl/sdl_render3d.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/app_sensors.h"
#include "mech3/app_states.h"
#include "mech3/briefing.h"
#include "mech3/credits.h"
#include "mech3/debriefing.h"
#include "mech3/lab_screen.h"
#include "mech3/mainmenu.h"
#include "mech3/mech_objective.h"
#include "mech3/mech_salvage.h"
#include "mech3/zai_valve.h"
#include "gamez/zclass/cls_node.h"
#include "mech3/menu_states.h"
#include "mech3/mission.h"
#include "mech3/pilot.h"
#include "mech3/pilot_menu.h"
#include "mech3/salvage_screen.h"

namespace sdlfe {

namespace {
// A store as "mechs/units in items": its mechs and all its units.
using Counts = std::map<std::string, int32_t>;
Counts counts(const mech3::Store* st) {
  Counts out;
  if (st)
    for (const mech3::ConfigItem* it : st->items) out[std::to_string(it->kind()) + "/" + std::to_string(it->id())] += it->count;
  return out;
}
// b - a, the keys that changed.
Counts minus(const Counts& b, const Counts& a) {
  Counts out;
  for (const auto& [k, v] : b)
    if (const auto it = a.find(k); v != (it == a.end() ? 0 : it->second)) out[k] = v - (it == a.end() ? 0 : it->second);
  for (const auto& [k, v] : a)
    if (!b.count(k)) out[k] = -v;
  return out;
}
void add(Counts& to, const Counts& d) {
  for (const auto& [k, v] : d)
    if ((to[k] += v) == 0) to.erase(k);
}
std::string line(const Counts& c) {
  std::string s;
  for (const auto& [k, v] : c) s += (s.empty() ? "" : " ") + k + "x" + std::to_string(v);
  return s;
}

std::string store_line(const mech3::Store& st) {
  int32_t mechs = 0, units = 0;
  for (const mech3::ConfigItem* it : st.items) mechs += it->kind() == 0 ? it->count : 0, units += it->count;
  char buf[64];
  std::snprintf(buf, sizeof buf, "%d mechs %d units in %zu", mechs, units, st.items.size());
  return buf;
}
}  // namespace

namespace {
// One objective and what it names, indented by its depth.
void print_objective(mech3::GameEngine& e, const mech3::Objective& o, int depth) {
  static const char* const kKinds[] = {"group", "destroy", "node_change", "reach_area", "valve_change"};
  const char* kind = o.kind < 5 ? kKinds[o.kind] : "training";
  std::string what;
  if (auto* d = dynamic_cast<const mech3::DestroyObjective*>(&o)) what = d->name ? *d->name : "?";
  if (auto* r = dynamic_cast<const mech3::ReachAreaObjective*>(&o)) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "(%.0f %.0f %.0f) r %.0f%s", r->point[0], r->point[1], r->point[2], r->radius,
                  r->node ? " node" : "");
    what = buf;
  }
  if (auto* v = dynamic_cast<const mech3::ValveChangeObjective*>(&o)) {
    const auto* valve = static_cast<const mech3::ai::Valve*>(e.object_of(v->watch));
    what = std::string(valve && valve->name ? valve->name : "?") + " to " + std::to_string(v->target) + " (now " +
           std::to_string(valve ? valve->get() : 0) + ")";
  }
  if (auto* n = dynamic_cast<const mech3::NodeChangeObjective*>(&o)) {
    const auto* node = static_cast<const zcls::Node*>(e.object_of(n->node));
    what = std::string(node ? node->name() : "?") + (n->active ? " on" : " off");
  }
  if (auto* g = dynamic_cast<const mech3::ObjectiveGroup*>(&o); g && g->goal_count)
    what += "(" + std::to_string(g->goal_count) + " of " + std::to_string(g->items.size()) + (g->simultaneous ? ", together" : "") + ")";
  std::printf("mw3: chain objective %*s%s %s%s%s\n", depth * 2, "", kind, what.c_str(), o.done ? " done" : "",
              o.salvage ? " +salvage" : "");
  if (auto* g = dynamic_cast<const mech3::ObjectiveGroup*>(&o))
    for (const auto& c : g->items) print_objective(e, *c, depth + 1);
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
      // With kills, SALVAGE comes first, so the lab can fit what it moved.
      if (o_.kills ? b->salvage_btn.enabled : b->lab_btn.enabled) {
        std::printf("mw3: chain %s\n", o_.kills ? "SALVAGE" : "MECHLAB");
        b->on_button(o_.kills ? mech3::BriefingScreen::kBtnSalvage : mech3::BriefingScreen::kBtnMechLab, 0, 0);
        return;
      }
      [[fallthrough]];
    case 2:
      brief_step_ = 3;
      if (o_.kills ? b->lab_btn.enabled : b->salvage_btn.enabled) {
        std::printf("mw3: chain %s\n", o_.kills ? "MECHLAB" : "SALVAGE");
        b->on_button(o_.kills ? mech3::BriefingScreen::kBtnMechLab : mech3::BriefingScreen::kBtnSalvage, 0, 0);
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

CampaignChain::~CampaignChain() {
  if (hooked_) app_.on_state_change = nullptr;
}

void CampaignChain::on_state(const void* s) {
  if (s == logged_state_) return;
  logged_state_ = s;
  const auto* ms = static_cast<const mech3::State*>(s);
  if (dynamic_cast<const mech3::CreditsState*>(ms)) ++credits_entered_;
  std::printf("mw3: chain state %s (frame %d, credits flag %d)\n", ms ? ms->name() : "none", frames_,
              app_.reopen_credits());
}

// Every enemy the player could shoot, hit once a frame by a shot of the
// player's through zWeapon's impact (mech3::chain_hit) until it dies; once
// the mission is won (its objectives met), Exit Mission. The mission's
// salvage is followed: what each kill added, and what came otherwise (the
// objectives' salvage).
void CampaignChain::shoot_hostiles() {
  if (core_frames_ == 60) salvage_seen_.clear(), salvage_kills_.clear(), salvage_other_.clear();
  {
    const Counts now = counts(&mech3::mission_salvage());
    add(salvage_other_, minus(now, salvage_seen_));
    salvage_seen_ = now;
  }
  if (mech3::missions().ended) {
    if (!mech3::missions().aborting) {
      std::printf("mw3: chain won %s by %d kills (%d hits, core frame %d): Exit Mission\n", launched_.c_str(), kills_,
                  hits_, core_frames_);
      mech3::missions().on_command(0x71);
    }
    return;
  }
  // An objective nothing the chain does can meet (c1m3's two gdbldg nodes,
  // campaign.md section 14): after 1500 frames, the win as --win gives it.
  if (core_frames_ == 1500) {
    std::printf("mw3: chain %s not won by its kills (%d) after 1500 frames: won as --win\n", launched_.c_str(), kills_);
    mech3::missions().ended = 1;
    mech3::missions().on_command(0x71);
    return;
  }
  static void* gun = nullptr;
  if (!gun) gun = mech3::chain_weapon();
  const std::vector<mech3::ChainTarget> hostiles = mech3::chain_hostiles();
  if (core_frames_ == 60) {
    std::printf("mw3: chain %zu enemies in %s:", hostiles.size(), launched_.c_str());
    for (const mech3::ChainTarget& t : hostiles) std::printf(" %s %s;", t.kind, t.name.c_str());
    std::printf("\n");
    hits_ = kills_ = idle_frames_ = 0;
  }
  // The buildings the objectives want down (node_change to off: zAnim's
  // scene objects, not on zEntity's list) are shot at too.
  std::vector<const mech3::NodeChangeObjective*> buildings;
  {
    const mech3::Objectives& ob = mech3::mission_objectives();
    std::function<void(const mech3::Objective&)> find = [&](const mech3::Objective& o) {
      if (o.done) return;
      if (auto* n = dynamic_cast<const mech3::NodeChangeObjective*>(&o); n && !n->active) buildings.push_back(n);
      if (auto* g = dynamic_cast<const mech3::ObjectiveGroup*>(&o))
        for (const auto& c : g->items) find(*c);
    };
    for (const mech3::ObjectiveGroup* g : {ob.primary.get(), ob.secondary.get(), ob.final.get()})
      if (g) find(*g);
  }
  int shot_buildings = 0;
  // What a destroyed target's fall waits for goes first (c1m3's legs). The
  // player goes up to the first of these: their animations run only within
  // range of the player (EXECUTION_BY_RANGE, anim-run.md).
  const std::vector<void*> first = mech3::chain_prerequisite_nodes();
  if (hostiles.empty() || !first.empty()) {
    void* next = !first.empty() ? first.front() : !buildings.empty() ? e_.object_of(buildings.front()->node) : nullptr;
    if (next) mech3::chain_player_near(next, 100.0f);
  }
  for (void* n : first)
    if (mech3::chain_hit_node(n, gun)) ++hits_, ++shot_buildings;
  if (!first.empty() && core_frames_ % 300 == 60) {
    std::printf("mw3: chain %zu nodes a building's fall waits for:", first.size());
    for (void* n : first) std::printf(" %s;", static_cast<const zcls::Node*>(n)->name());
    std::printf("\n");
  }
  for (const mech3::NodeChangeObjective* b : buildings)
    if (mech3::chain_hit_node(e_.object_of(b->node), gun)) ++hits_, ++shot_buildings;
  if (core_frames_ % 300 == 60 && !buildings.empty()) {
    std::printf("mw3: chain %zu buildings the objectives want down, %d can be shot:", buildings.size(), shot_buildings);
    for (const mech3::NodeChangeObjective* b : buildings) {
      const auto* n = static_cast<const zcls::Node*>(e_.object_of(b->node));
      std::printf(" %s;", n ? n->name() : "?");
    }
    std::printf("\n");
  }
  // No one left to shoot and the mission not over: a valve the primary or
  // final objectives still wait for is a place, its region (reg_<valve>),
  // else the mobile field base ("return to the MFB"), the regions' own trip.
  if (hostiles.empty() && shot_buildings == 0) {
    if (++idle_frames_ % 90 == 60) {
      std::string valve;
      const mech3::Objectives& ob = mech3::mission_objectives();
      std::function<void(const mech3::Objective&)> find = [&](const mech3::Objective& o) {
        if (!valve.empty() || o.done) return;
        if (auto* v = dynamic_cast<const mech3::ValveChangeObjective*>(&o)) {
          const auto* x = static_cast<const mech3::ai::Valve*>(e_.object_of(v->watch));
          if (x && x->name) valve = x->name;
        }
        if (auto* g = dynamic_cast<const mech3::ObjectiveGroup*>(&o))
          for (const auto& c : g->items) find(*c);
      };
      for (const mech3::ObjectiveGroup* g : {ob.primary.get(), ob.final.get()})
        if (g) find(*g);
      const std::string region = "reg_" + valve;
      const char* where = "nowhere";
      if (!valve.empty() && mech3::chain_player_to_region(region.c_str())) where = region.c_str();
      else if (mech3::chain_player_to("mobilehq_01", 40.0f)) where = "by mobilehq_01";
      std::printf("mw3: chain no enemies left in %s (core frame %d), waiting for valve %s: the player set %s\n",
                  launched_.c_str(), core_frames_, valve.empty() ? "none" : valve.c_str(), where);
    }
  } else {
    idle_frames_ = 0;
  }
  salvage_seen_ = counts(&mech3::mission_salvage());  // the buildings' hits' own: the other's next frame
  for (const mech3::ChainTarget& t : hostiles) {
    ++hits_;
    std::vector<int32_t> carried = mech3::chain_weapon_types(t.h);
    if (mech3::chain_hit(t.h, gun)) {
      ++kills_;
      const Counts now = counts(&mech3::mission_salvage());
      const Counts got = minus(now, salvage_seen_);
      add(salvage_kills_, got);
      salvage_seen_ = now;
      // Its salvage's weapons are the ones it carried, but TAG (32), which
      // comes in no tech; a vehicle's all or none (a coin, 0x00472ec0).
      std::vector<int32_t> salvaged;
      for (const auto& [k, v] : got)
        if (k.rfind("1/", 0) == 0)
          for (int32_t i = 0; i < v; ++i) salvaged.push_back(std::stoi(k.substr(2)));
      carried.erase(std::remove(carried.begin(), carried.end(), 32), carried.end());
      std::sort(carried.begin(), carried.end());
      std::sort(salvaged.begin(), salvaged.end());
      const bool vehicle = std::string(t.kind) == "vehicle";
      const bool same = salvaged == carried || (vehicle && salvaged.empty());
      if (std::string(t.kind) == "mech" || vehicle) {
        ++killed_units_;
        if (!same) ++mismatched_units_;
      }
      std::printf("mw3: chain killed %s %s (core frame %d): salvage {%s}%s\n", t.kind, t.name.c_str(), core_frames_,
                  line(got).c_str(), std::string(t.kind) == "mech" || vehicle ? (same ? ", its weapons" : ", NOT its weapons") : "");
    }
  }
  // A hit that does not kill leaves the salvage alone; anything else this
  // frame is the next frame's "other".
  salvage_seen_ = counts(&mech3::mission_salvage());
}

// The debriefing's SALVAGE_LIST is the mission's store, and it holds what
// the kills added, the objectives' salvage, and the lance's mechs the win
// puts in (0x0047ae10, mechs only): nothing else.
void CampaignChain::debrief_salvage() {
  auto* d = dynamic_cast<mech3::DebriefScreen*>(e_.screen_of(mech3::states().end_win.screen_));
  if (!d) return;
  const mech3::Store* shown = e_.store_of(d->salvage.store);
  const Counts list = counts(shown);
  Counts rest = minus(list, salvage_kills_);
  rest = minus(rest, salvage_other_);
  const bool ok = shown == &mech3::mission_salvage() && !salvage_kills_.empty() && mismatched_units_ == 0;
  std::printf("mw3: chain debriefing salvage {%s} (%d rows, the mission's store %d): kills {%s} (%d units, %d not "
              "their weapons), objectives {%s}, the win's lance {%s}: %s\n",
              line(list).c_str(), d->salvage.item_count(), shown == &mech3::mission_salvage() ? 1 : 0,
              line(salvage_kills_).c_str(), killed_units_, mismatched_units_, line(salvage_other_).c_str(),
              line(rest).c_str(), ok ? "matches" : "differs");
  killed_units_ = mismatched_units_ = 0;
}

// The salvage screen: every weapon row of the mission's list moved to the
// player's (within the bases' tons), as a player taking the guns.
void CampaignChain::salvage_weapons() {
  auto* sv = dynamic_cast<mech3::SalvageScreen*>(e_.screen_of(mech3::states().briefing_salvage.screen_));
  if (!sv) return;
  const Counts store0 = counts(e_.store_of(sv->player.store)), salv0 = counts(e_.store_of(sv->mission.store));
  std::vector<const mech3::ConfigItem*> stuck;  // the rows the bases have no room for
  for (int guard = 0; guard < 64; ++guard) {
    int32_t row = -1;
    const mech3::ConfigItem* item = nullptr;
    for (int32_t i = 1; i < sv->mission.item_count() && row < 0; ++i) {
      item = e_.item_of(static_cast<zgame::Handle>(sv->mission.items[static_cast<size_t>(i)]->value));
      if (item && item->kind() == 1 && std::find(stuck.begin(), stuck.end(), item) == stuck.end()) row = i;
    }
    if (row < 0) break;
    const Counts before = counts(e_.store_of(sv->mission.store));
    sv->on_button(mech3::SalvageScreen::kMission, 0, row);
    if (counts(e_.store_of(sv->mission.store)) == before) stuck.push_back(item);
  }
  const Counts got = minus(counts(e_.store_of(sv->player.store)), store0);
  const Counts left = minus(salv0, counts(e_.store_of(sv->mission.store)));
  moved_weapons_.clear();
  for (const auto& [k, v] : got)
    if (k.rfind("1/", 0) == 0 && v > 0) moved_weapons_.push_back(std::stoi(k.substr(2)));
  std::printf("mw3: chain salvage weapons moved: the store +{%s}, the salvage -{%s}: %s\n", line(got).c_str(),
              line(left).c_str(), got == left && !moved_weapons_.empty() ? "moved" : "not moved");
}

// The lab's criticals page, its weapons tab: a weapon SALVAGE moved put on
// the shown mech, the first part it fits (ASSIGNED_LIST's part,
// UNASSIGNED_LIST's row).
void CampaignChain::lab_fit() {
  const mech3::LabState& lab = mech3::states().lab;
  mech3::LabScreen* ls = e_.lab_screen(lab.lab_screen_);
  if (!ls || moved_weapons_.empty()) return;
  mech3::Config* cfg = e_.lab_config_of(e_.lab_slot_cfg(e_.lab_slot(lab.pilot_, lab.slot_)));
  if (!cfg) return;
  auto carried = [&](int32_t id) {
    int32_t n = 0;
    for (const mech3::ConfigItem* it : cfg->items) n += it->kind() == 1 && it->id() == id ? it->count : 0;
    return n;
  };
  static const int32_t kParts[] = {2, 3, 7, 8, 1, 6};  // the arms, the side torsos, the centre, the head
  auto fit = [&](const char* after) {
    for (int32_t part : kParts) {
      ls->assigned.choose(part);
      ls->unassigned.refill();
      for (int32_t i = 0; i < ls->unassigned.item_count(); ++i) {
        const mech3::ConfigItem* it = e_.item_of(ls->unassigned.object_at(i));
        if (!it || it->kind() != 1 || !ls->unassigned.can_xa8(i)) continue;
        const int32_t id = it->id();
        if (std::find(moved_weapons_.begin(), moved_weapons_.end(), id) == moved_weapons_.end()) continue;
        const int32_t had = carried(id);
        ls->unassigned.select(i);
        if (carried(id) > had) {
          std::printf("mw3: chain lab: salvaged weapon %d put on part %d%s (%d carried now)\n", id, part, after, carried(id));
          fitted_weapon_ = id, fitted_count_ = carried(id);
          return true;
        }
      }
    }
    return false;
  };
  fitted_weapon_ = -1;
  if (fit("")) return;
  // The mech full: one of its own weapons back to the store first, part by
  // part, as a player making room (ASSIGNED_LIST's item row, 0x00455fb0).
  for (int32_t part : kParts) {
    ls->assigned.choose(part);
    for (int32_t r = 0; r < ls->assigned.item_count(); ++r) {
      if (!ls->assigned.is_item_row(r)) continue;
      const mech3::ConfigItem* it = e_.item_of(ls->assigned.item_at_row(r));
      if (!it || it->kind() != 1) continue;
      const int32_t id = it->id(), had = carried(id);
      ls->assigned.select(r);
      if (carried(id) >= had) continue;
      const std::string after = ", after weapon " + std::to_string(id) + " came off part " + std::to_string(part);
      if (fit(after.c_str())) return;
      break;
    }
  }
  std::printf("mw3: chain lab: no salvaged weapon fits the mech\n");
}

void CampaignChain::frame() {
  if (done_) return;
  if (!hooked_) {
    hooked_ = true;
    app_.on_state_change = [this](const zgame::State* s) { on_state(s); };
    on_state(app_.current());
  }
  ++frames_;
  mech3::States& st = mech3::states();
  const zgame::State* now = app_.current();
  if (now != last_state_) last_state_ = now, frames_in_state_ = 0;
  ++frames_in_state_;
  if (now == &st.main && mech3::main_menu() && frames_in_state_ == 30) {
    if (wins_ > 0 && !after_credits_) {  // the main menu's enter should have pressed CREDITS
      std::printf("mw3: chain main menu after %d wins without the credits\n", wins_);
      done_ = true;
      return;
    }
    if (after_credits_) std::printf("mw3: chain main menu after the credits: NEW CAMPAIGN\n");
    mech3::main_menu()->buttons[1].on_left_up();  // NEW CAMPAIGN
  } else if (auto* ps = dynamic_cast<mech3::PilotState*>(app_.current())) {
    if (after_credits_) {  // the main menu works after the campaign's end
      std::printf("mw3: chain pilot screen again after the credits (credits entered %d)\n", credits_entered_);
      done_ = true;
      return;
    }
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
  } else if (now == &st.lab && o_.kills && frames_in_state_ == 60) {
    if (mech3::LabScreen* ls = e_.lab_screen(st.lab.lab_screen_)) {
      ls->on_button(3, 0, 0);     // ICON_CRITICALS: ASSIGNED_LIST and UNASSIGNED_LIST
      ls->on_button(0x19, 0, 0);  // SELECT_WEAPON
    }
  } else if (now == &st.lab && o_.kills && frames_in_state_ == 75) {
    lab_fit();
  } else if (now == &st.lab && frames_in_state_ == (o_.kills ? 90 : 60)) {
    const mech3::LabState& lab = st.lab;
    if (mech3::LabScreen* ls = e_.lab_screen(lab.lab_screen_)) {
      std::printf("mw3: chain lab: %d slots, ACCEPT\n", lab.slots_);
      ls->on_button(6, 0, 0);  // ACCEPT
    }
  } else if (now == &st.briefing_salvage && frames_in_state_ == 30) {
    auto* sv = dynamic_cast<mech3::SalvageScreen*>(e_.screen_of(st.briefing_salvage.screen_));
    if (sv) {
      const int32_t rows = sv->mission.item_count();
      if (o_.kills) salvage_weapons();
      else if (rows > 1) sv->mission.select(rows - 1);
      std::printf("mw3: chain salvage: %d mission rows, %s; ACCEPT\n", rows, o_.kills ? "the weapons moved" : "one moved");
      sv->on_button(mech3::SalvageScreen::kAccept, 0, 0);
    }
  } else if (now == &st.core) {
    if (frames_in_state_ == 1) core_frames_ = 0;
    if (core_frames_ == 30 || (o_.kills && core_frames_ % 600 == 0 && !mech3::missions().ended)) {
      const mech3::Objectives& ob = mech3::mission_objectives();
      const char* names[] = {"primary", "secondary", "final", "fail"};
      const mech3::ObjectiveGroup* groups[] = {ob.primary.get(), ob.secondary.get(), ob.final.get(), ob.fail.get()};
      for (int i = 0; i < 4; ++i)
        if (groups[i]) std::printf("mw3: chain objectives %s %s:\n", launched_.c_str(), names[i]), print_objective(e_, *groups[i], 1);
    }
    if (o_.kills) {
      if (++core_frames_ == 30 && fitted_weapon_ >= 0) {  // the lab's fit, in the mission
        int32_t n = 0;
        for (int32_t t : mech3::salvage_weapon_types(mech3::mech_mission_player())) n += t == fitted_weapon_;
        std::printf("mw3: chain the launched mech carries weapon %d x%d (the lab fitted %d): %s\n", fitted_weapon_, n,
                    fitted_count_, n == fitted_count_ ? "fitted" : "not fitted");
        fitted_weapon_ = -1;
      }
      if (core_frames_ >= 60) shoot_hostiles();
    } else if (++core_frames_ == 60) {  // won, as --win: Exit Mission with the objectives met
      std::printf("mw3: chain win %s\n", launched_.c_str());
      mech3::missions().ended = 1;
      mech3::missions().on_command(0x71);
    }
  } else if (now == &st.end_win) {
    if (frames_in_state_ == 60) {
      shoot("debriefing");
      if (o_.kills) debrief_salvage();
    }
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
  } else if (auto* cs = dynamic_cast<const mech3::CreditsState*>(now);
             cs && wins_ > 0 && frames_in_state_ == 120) {  // the campaign's end
    auto* scr = dynamic_cast<mech3::CreditsScreen*>(e_.screen_of(cs->screen_));
    std::printf("mw3: chain credits after %d wins (entered %d): CANCEL\n", wins_, credits_entered_);
    if (scr) scr->cancel.on_left_up();
    after_credits_ = true;
  } else if (frames_in_state_ == 3000) {
    const auto* ms = dynamic_cast<const mech3::State*>(now);
    std::printf("mw3: chain stuck in %s\n", ms ? ms->name() : "?");
    done_ = true;
  }
}

}  // namespace sdlfe
