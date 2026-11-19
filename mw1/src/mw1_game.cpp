#include "mw1_game.h"
#include "mw1_fade.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include <functional>
#include "mech3/app_vehicle.h"
#include "mech3/action_menu.h"
#include "mech3/app_states.h"
#include "mech3/app.h"
#include "mech3/pause_menu.h"
#include "mech3/briefing.h"
#include "mech3/lab_screen.h"
#include "mech3/lab_filebox.h"
#include "mech3/lab_view.h"
#include "mech3/options.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/window.h"
#include "mech3/menu_widgets.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/mech_names.h"
#include "mech3/mech_objective.h"
#include "mech3/mech_config.h"
#include "mech3/mission.h"
#include "mech3/mods.h"
#include "mech3/options_menu.h"
#include "mech3/pilot.h"
#include "mech3/program_flow.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_params.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zutil/zutl_path.h"
#include "mw1_art.h"
#include "mw1_bar_screen.h"
#include "mw1_lab_fit.h"
#include "mw1_mech_icons.h"
#include "mw1_loading.h"
#include "mw1_mech_renders.h"
#include "platform/render3d.h"
#include "mw1_results.h"
#include "mech3/mech_salvage.h"
#include "mw1_event_screen.h"
#include "mw1_office.h"
#include "mw1_campaign.h"
#include "mw1_hub.h"
#include "mw1_market.h"
#include "mw1_market_screen.h"
#include "mw1_lab_pilot.h"
#include "mw1_menu.h"
#include "mw1_images.h"
#include "mw1_lab_skin.h"
#include "mw1_layouts.h"
#include "mw1_missions.h"
#include "mw1_pilots.h"
#include "mw1_repairs.h"
#include "mw1_scenery.h"
#include "mw1_travel.h"
#include "mw1_travel_map.h"
#include "mw1_travel_screen.h"
#include "mw1_save.h"
#include "mw1_story.h"
#include "mw1_ladder.h"

namespace mw1 {
namespace {

constexpr int32_t kBoxOk = 0, kBoxYesNo = 4, kAnswerYes = 6;
constexpr const char* kFontsFile = "mw1fonts.zrd";  // MW1's data folder
constexpr const char* kDialogFile = "mw1dialog.zrd", *kDialogName = "MW1MESSAGEBOX";  // every message box's layout
constexpr uint32_t kOptionsStateGlobal = 0x609868;  // PM's main menu's Options state (its OPTIONS button pushes it)  // the message box's styles and answer (Win32's)
constexpr uint32_t kGfxOptionsStateGlobal = 0x60986c, kControlsStateGlobal = 0x609870;  // its other pages' (MW1_SCREEN)
constexpr const char* kPilotName = "MW1 Company";  // the company's PM pilot (step 2)
constexpr float kNavNear = 1.0f;  // item 43: only a map's own point on the structure serves as its (a near "Able" says nothing)
constexpr float kReachRadius = 40.0f;  // metres from the structure's point: FIXES.md item 45, the worst row's closest stand (mw3c2's tower, 33 m) and a margin
constexpr int32_t kReachIa = 2;        // the map with the structures (c2's ia2)

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string join(const std::vector<std::string>& l, const char* sep) {
  std::string s;
  for (const std::string& x : l) s += (s.empty() ? "" : sep) + x;
  return s;
}
std::string title_case(std::string s) {
  if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
  return s;
}
// Is `name` one of the mission's targets.zrd entities? Read as the engine reads it (the mounted archives, the
// mission's first): at block_read the targets aren't made yet (entity_find answers none), and destroy_entity finds
// its entity by name later, at its checks; a name it never finds counts as destroyed at once, so it's asked here.
bool target_listed(const std::string& name) {
  zrdr::Node* root = zrdr::load("targets.zrd");
  if (!root) return false;
  zrdr::Node* top = root;
  if (top->type == zrdr::List && zrdr::list_count(top) == 2 && top->list[1].type == zrdr::List) top = &top->list[1];
  const bool there = zrdr::find_flat(top, name.c_str(), 1) != nullptr;
  zrdr::free_node(root);
  return there;
}

// The contract's objective as the offer and the briefing say it.
std::string objective_words(const Contract& k) {
  const std::string s = k.structure_words.empty() ? k.structure : k.structure_words;
  switch (k.objective) {
    case Objective::kReachStructure: return "reach " + s;
    case Objective::kAttackStructure: return "destroy " + s;
    case Objective::kDefendStructure: return "defend " + s;  // won by destroying the enemies (Instant Action's own objective)
    case Objective::kDestroyAll: break;
  }
  return "destroy all enemy mechs";
}
// The map as PM's Instant Action screen names it: the world (PM's c1..c4 by their messages, 0xe33.., an MW3 world by
// the name it was registered with) and the scenario of its archive (ia1 Battle, ia2 Attack, ia3 Defend; the
// scenarios' table, action_menu.h). The code ("c2 Instant Action 3") when either has no words.
std::string map_words(mech3::Engine* e, const std::string& world, int32_t ia) {
  const std::string code = world + " Instant Action " + std::to_string(ia);
  if (!e) return code;
  auto said = [](const char* t) { return t && *t ? std::string(t) : std::string(); };
  std::string name;
  if (world.size() == 2 && world[0] == 'c' && world[1] >= '1' && world[1] <= '4') name = said(e->message(0xe33 + (world[1] - '1')));
  for (const mech3::ModIaWorld& w : mech3::mod_ia_worlds())
    if (name.empty() && w.folder == world && !zutl::game_text(w.name, &name)) name.clear();
  std::string scenario;
  for (int32_t i = 1; i < mech3::kActionScenarioCount && scenario.empty(); ++i)
    if (mech3::kActionScenarios[i].mission == ia) scenario = said(e->message(mech3::kActionScenarios[i].name));
  return name.empty() || scenario.empty() ? code : name + ", " + scenario;
}

const char* ending_word(Outcome o) { return o == Outcome::kSuccess ? "Success" : o == Outcome::kFailure ? "Failure" : "Retreat"; }

// A wreck's condition (provisional, research/contracts.md): the share of its parts (ids 1..15) still standing.
int32_t condition_of(void* mech) {
  int32_t standing = 0, all = 0;
  return mech3::program_mech_parts_standing(mech, &standing, &all) && all ? standing * 100 / all : 0;
}

class Loop : public mech3::ProgramFlow, public mech3::ObjectivesExtension {
 public:
  Loop(uint32_t seed, const char* reach) : rng_(seed ? seed : std::random_device{}()) {
    if (reach && *reach) row_set_ = true, row_ = {"c2", kReachIa, kDesert, RowKind::kReach, reach, std::string("the ") + reach};
  }

  // ---- ProgramFlow ----
  // MW1's own menus: Pirate's Moon's main menu screen and its multiplayer state, never shown, are not built.
  bool own_menus() const override { return true; }
  // MW1's pictures and fonts, and the pause and the options in MW1's look (mw1_layouts.h: Pirate's Moon's screens
  // on MW1's layouts), before Pirate's Moon builds those screens.
  void menus_building(mech3::Engine& e) override {
    register_images();
    register_fonts();
    // FIXES.md item 23: the Company screen's mech pictures, PM's lab icons in amber (mw1_mech_icons.h).
    if (const int32_t n = make_mech_icons(zutl::mounts().write); n > 0) {
      bool there = false;
      for (zimg::Library* lib : zimg::library_list(1)) there = there || (lib && std::strstr(lib->name, kIconLibrary));
      if (!there) zimg::new_image_library(kIconLibrary, 0, false);
    } else {
      std::printf("mw1: no mech icons from Pirate's Moon's pictures: the Company screen's mech slots stay empty\n");
    }
    // FIXES.md item 14: the mech renders, registered when made under this stamp; else made by the pass this run (not
    // for development shots and checks: MW1_SCREEN's pages keep the icons).
    const bool current = renders_current(zutl::mounts().write, render_mechs());
    if (current && !renders_ready()) zimg::new_image_library(kRenderLibrary, 0, false);
    if (current) art::Store::get().add(hi_renders_path(zutl::mounts().write));  // the 2D's 4x set (B2b)
    set_renders_ready(current);
    // Current ones are looked at again once the frame is known (render_pass_frame): a bigger window than they were
    // made in makes them again, behind the same cover (renders_outgrown).
    renders_wanted_ = !zutl::mounts().write.empty() && !std::getenv("MW1_SCREEN");
    renders_made_height_ = current ? renders_frame_height(zutl::mounts().write) : 0;
    renders_current_ = current;
    for (const LayoutSwap& s : kLayoutSwaps)
      if (layout_there(s.file, s.dialog))
        ztwod::set_layout(s.game_file, s.game_dialog, s.file, s.dialog, std::string(s.dialog) + kWideSuffix);
    for (const auto& [game_font, font] : kLayoutFonts) ztwod::set_layout_font(game_font, font);
    // The lab's reskin (mw1_lab_skin.h): PM's layouts, our pictures and fonts while they are read.
    for (const std::vector<chrome::Reskin>* t : {&chrome::lab_pictures(), &chrome::filebox_pictures()})
      for (const chrome::Reskin& r : *t)
        ztwod::set_layout_image(r.game, chrome::reskin_name(r.game), t == &chrome::lab_pictures() ? chrome::kLabFile : chrome::kFileBoxFile);
    for (const char* f : kLabFonts) ztwod::set_layout_font(f, std::string("mw1_") + f, chrome::kLabFile);
    for (const auto& [game_font, font] : kFileBoxFonts) ztwod::set_layout_font(game_font, font, chrome::kFileBoxFile);
    // The lab's own words where Pirate's Moon's aren't MW1's (FIXES 3+5): its chassis list is the company's mechs
    // (the Mech Complex's YOUR MECHS), not MW3's mobile field base; PM's "Avaliable" spelt right.
    for (const auto& [key, words] : kLabLabels) ztwod::set_layout_label(key, words, chrome::kLabFile);
    for (const char* f : kPmCursorFiles) ztwod::set_layout_image(kPmCursor, chrome::kCursor, f);  // FIXES.md item 22
    auto* ge = dynamic_cast<mech3::GameEngine*>(&e);
    mech3::set_menus_still(true);  // FIXES.md item 9: PM's own screens' backgrounds held still too
    mech3::set_buttons_steady(true);  // FIXES.md item 25: PM's own HelpButtons hold their lamp, as ours (item 15)
    ztwod::set_layout_wide([ge] { return ge && ge->buffer_width() > 640; });
  }
  bool shot_driver_ = false;  // MW1_SCREEN=pause's (development)
  bool main_entered(mech3::Engine& e) override {
    auto& ge = static_cast<mech3::GameEngine&>(e);
    ge_ = &ge;
    if (!fade_hook_) {  // FIXES.md item 52: the fade on screens not MW1's
      fade_hook_ = true;
      auto before = ge.on_present;
      ge.on_present = [this, before](int f) {
        if (before) before(f);
        if (ge_) fade_present(*ge_);
      };
    }
    // The mission's end heard the frame it happens (latch_ending), after each frame shown. Put on here, as the main
    // state is entered, never from inside a frame's own on_present (replacing the function that is running frees it).
    if (!ending_hook_) {
      ending_hook_ = true;
      auto before = ge.on_present;
      ge.on_present = [this, before](int f) {
        if (before) before(f);
        if (deployed_ && launched_ && !ending_latched_ && ge_ && ge_->mission_ended()) latch_ending(*ge_);
        lab_look_frame();
        briefing_cursor_frame();
        if (std::getenv("MW1_FIT")) screen_fit_frame();
        render_pass_frame();
      };
    }
    // MW1_SCREEN=pause or pause_options (development: --shot pictures of the pause over a mission): the offer
    // accepted, DEPLOY, the pause pushed 90 frames into the mission (and its OPTIONS). MW1_SCREEN=loading: DEPLOY's
    // loading screen, held there. MW1_SCREEN=lab or lab_files: the Mech Complex's MECH LAB instead (and the lab's load
    // box); lab_3, lab_4 its pages (lab_1, empty, falls back to the chassis: item 26). MW1_SCREEN=results: the
    // mission won 600 frames in (every enemy the commander's, the left arm hit), by PM's Exit Mission: the results.
    if (const char* s = std::getenv("MW1_SCREEN");
        s && (std::string(s).rfind("pause", 0) == 0 || std::string(s).rfind("lab", 0) == 0 || std::string(s).rfind("loading", 0) == 0 ||
              std::string(s) == "results" || std::string(s).rfind("walk_", 0) == 0) &&
        !shot_driver_) {
      hold_for_shot_ = std::string(s).rfind("loading", 0) == 0;
      shot_driver_ = true;
      auto before = ge.on_present;
      ge.on_present = [this, before, page = std::string(s)](int f) {
        if (before) before(f);
        if (page == "results" || page.rfind("walk_", 0) == 0) name_state(f);
        pause_shot_frame(page);
      };
    }
    // Every message box in MW1's look (mw1dialog.zrd), Pirate's Moon's own boxes too (its quit question).
    if (layout_there(kDialogFile, kDialogName)) ge.set_message_box_layout(kDialogFile, kDialogName);
    // MW1's main menu over PM's (mw1_menu.h): at the start, and under the hub after a mission (the hub's QUIT comes
    // back to it). Without its layout, or for the tests' straight start, the hub as before.
    const bool with_menu = start_ != Start::kHub && menu(ge);
    if (with_menu) {
      show_menu();
      ge.push_state(menu_, 0);
    }
    if (!company_.stores && with_menu) {
      const char* screen = std::getenv("MW1_SCREEN");  // development: --shot pictures of the intro
      if (start_ == Start::kNewCampaign || (screen && std::string(screen).rfind("intro", 0) == 0)) {
        start_ = Start::kMenu;  // once: --new-company, as NEW CAMPAIGN without its question
        new_campaign(ge, false);
        return true;
      }
      // MW1_SCREEN=hub, market, travel or bar: the hub at once, over the menu (shots); else the menu waits.
      if (!screen || std::string(screen) == "menu") return true;
    }
    return enter_hub(ge);
  }

  // The hub entered: the company loaded or made when there's none, a mission's return settled into AFTER ACTION, the
  // hub pushed (or the message boxes without its layout). No offer here (FIXES.md item 7): contracts come from the
  // House office.
  bool enter_hub(mech3::GameEngine& ge) {
    if (!company_.stores) start_company();
    // Back from a contract's mission, the main menu stays MW1's even when the next offer is declined: taken over,
    // Instant Action's own return (its screen reopened) does not follow (program_flow.h).
    const bool returning = deployed_;
    loading_.reset();  // a load aborted (or PM's next round's): its loading screen gone
    if (deployed_ && !launched_) {  // ABORT at the briefing: the company as it was, the contract still taken (a replay's failure still pending)
      deployed_ = false;
      std::printf("mw1: aborted before the launch: nothing changed\n");
    } else if (deployed_) {
      // Back from the mission: AFTER ACTION, not settled until its OK (or REPLAY plays it again); saved now, so a
      // restart shows it again and it is settled once.
      deployed_ = false, launched_ = false;
      pending_ = std::make_unique<AfterAction>();
      pending_->outcome = ended_ && !failed_ ? Outcome::kSuccess : failed_ ? Outcome::kFailure : Outcome::kRetreat;
      pending_->wrecks = wrecks_;
      pending_->killers = killers_, pending_->seconds = seconds_;
      pending_->armor_left = armor_left_, pending_->structure_left = structure_left_;
      pending_->lance = fates_;
      roll_missing(pending_->lance, rng_);  // FIXES.md item 32: each downed lancemate KILLED or MISSING, once
      for (const MechBill& b : bills_) pending_->bills.push_back({b.who, b.mech, b.damage, b.destroyed});
      pending_->stayed = stayed_behind(flying_, fates_);  // the map's allied places ran out: they didn't fly
      pending_->salvage = std::move(salvage_);
      pending_->launch_stores = launch_stores_, pending_->launch_loadout = launch_loadout_;
      // MW1_RESULTS=retreat or lost (development: --shot pictures of AFTER ACTION's other endings, item 38): the
      // mission withdrawn from, or failed with the commander's mech destroyed.
      if (const char* r = std::getenv("MW1_RESULTS")) {
        const std::string w = r;
        if (w == "retreat") pending_->outcome = Outcome::kRetreat;
        if (w == "lost") {
          pending_->outcome = Outcome::kFailure;
          for (auto& b : pending_->bills)
            if (b.who == 0) b.destroyed = true;
        }
      }
      std::printf("mw1: back: %s, %zu wrecks: after action\n", ending_word(pending_->outcome), wrecks_.size());
      save();
    }
    if (hub(ge)) {
      // A mission back (or a restart with one pending): MW1's results screen over the hub (FIXES.md item 19), until
      // CONTINUE settles it; the hub under it without AFTER ACTION.
      // MW1_FIT (the screens' text guard) or MW1_LONG_NAME (its shots): the company on the longest-named world, every
      // page's worst case for the plaques and the status bar (the name only: the world itself stays).
      if (std::getenv("MW1_FIT") || std::getenv("MW1_LONG_NAME"))
        for (const Planet& pl : kPlanets)
          if (std::strlen(pl.name) > company_.planet.size()) company_.planet = pl.name;
      results_up_ = pending_ && results_screen(ge);
      show_hub();
      ge.push_state(hub_, 0);
      if (results_up_) {
        show_results();
        ge.push_state(results_, 0);
      }
      // FIXES.md item 52: the first screen after a mission (or the start) in from black. Not under the shots' and
      // tests' screens (MW1_SCREEN): their pictures are of the screen itself.
      if (fade::enabled() && !std::getenv("MW1_SCREEN")) fade::current().fade_in();
      // MW1_SCREEN=market (development: --shot pictures of a screen past the hub).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s).rfind("market", 0) == 0) {
        press(kHubMarket);
        if (std::string(s) == "market_lab") press_market(kMarketLab);  // the lab on the commander's mech, its PILOT line
        if (std::string(s) == "market_dealer") {  // the dealer's limit up (a later tier's roll; mw1screenfit's case)
          company_.market.limit = company_.market.sold + kDealerLimitBase, show_market();
        }
        // market_mixed: funds for the cheapest only (3,000,000 on the start world's stock), the rest dimmed (item 41).
        if (std::string(s) == "market_mixed") company_.funds = 3000000, show_market();
      }
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "travel") press(kHubTravel);
      // MW1_SCREEN=travel_pick: the map with a world chosen (MW1_TRAVEL_TO, a name; else the farthest of the first few).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "travel_pick") {
        press(kHubTravel);
        const char* to = std::getenv("MW1_TRAVEL_TO");
        const std::vector<int32_t> near = destinations(company_, -1);
        tchoice_.world = to ? find_by_name(to) : near.size() > 6 ? near[6] : -1;
        show_travel();
      }
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "bar") press(kHubBar);
      // bar_note_long (mw1screenfit's case): the bar with a note longer than any the game makes, the status bar's
      // last resort (the world's name gives way, then the words are cut).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "bar_note_long") {
        press(kHubBar);
        bchoice_.note = std::string(140, 'W');
        show_bar();
      }
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "bar_recruit") press(kHubBar), press_bar(kBarCrew);
      if (const char* s = std::getenv("MW1_SCREEN"); s && (std::string(s) == "bar_lance" || std::string(s) == "company")) press(kHubCompany);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "office") press(kHubOffice);
      // hub_clear: the messages read (OK), the scenery and the tiles alone; hub_contract_read: a contract taken at the
      // office, its message read, the status line saying it (item 16).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "hub_clear" && !pending_) press(kHubOk);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "hub_contract_read" && !pending_)
        press(kHubOffice), press_office(kOfficeRequest), press_office(kOfficeAccept), press(kHubOk);
      // office_offer: the office with an offer up; hub_contract: one accepted, the hub's DEPLOY; hub_options: OPTIONS.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "office_offer") press(kHubOffice), press_office(kOfficeRequest);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "hub_contract")
        press(kHubOffice), press_office(kOfficeRequest), press_office(kOfficeAccept);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "hub_options") press(kHubOptions);
      // credits: the credits over the hub, MW1_CREDITS_AT seconds in (default 12: MW1's first panels up).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "credits" && !pending_) {
        const char* at = std::getenv("MW1_CREDITS_AT");
        play_credits(at ? static_cast<float>(std::atof(at)) : 12.0f);
      }
      // hub_long, hub_long_next: the hub's messages longer than its box (made-up lines): the box at its tallest, page
      // 1 of 2; and after OK (NEXT), page 2 (FIXES.md item 42). hub_long_short: two lines, the box sized to them.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s).rfind("hub_long", 0) == 0 && !pending_) {
        const int count = std::string(s) == "hub_long_short" ? 2 : 9;
        for (int k = 1; k <= count; ++k) company_.notices.push_back("Test message line " + std::to_string(k) + ".");
        show_hub();
        if (std::string(s) == "hub_long_next") press(kHubOk);
      }
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "news") press(kHubBar), press_bar(kBarNews);
      // FIXES.md item 52's captures: the player's presses at set frames (as a click: faded), the hub's first at
      // MW1_FADE_AT (default 60), the next 40 frames on. fade_bar: BAR, then the Bar's BACK; fade_lab: MECH COMPLEX,
      // then MECH LAB (on the commander's mech).
      if (const char* s = std::getenv("MW1_SCREEN"); s && (std::string(s) == "fade_bar" || std::string(s) == "fade_lab")) {
        const bool lab = std::string(s) == "fade_lab";
        const int at = std::getenv("MW1_FADE_AT") ? std::atoi(std::getenv("MW1_FADE_AT")) : 60;
        press(kHubOk);
        auto before = ge.on_present;
        ge.on_present = [this, before, lab, at, start = -1](int f) mutable {
          if (before) before(f);
          if (start < 0) start = f;
          const int n = f - start;
          if (n == at) {
            std::printf("mw1: fade capture: frame %d, %s\n", f, lab ? "MECH COMPLEX" : "BAR");
            as_player([&] { press(lab ? kHubMarket : kHubBar); });
          }
          if (n == at + 40) {
            std::printf("mw1: fade capture: frame %d, %s\n", f, lab ? "MECH LAB" : "BACK");
            as_player([&] { lab ? press_market(kMarketLab) : press_bar(kBarBack); });
          }
        };
      }
      // bar_drink: ORDER DRINK at the Bar, its tip (or the quiet text) over the Bar (FIXES.md item 34);
      // message_market, message_office: a message (the quiet text) over the Mech Complex, the office.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "bar_drink") press(kHubBar), press_bar(kBarDrink);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "message_market") press(kHubMarket), message_over(kClassMechComplex);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "message_office") press(kHubOffice), message_over(kClassOffice);
      // MW1_SCREEN=options, options_gfx, options_controls, options_upgrades: PM's options pages on MW1's layouts.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s).rfind("options", 0) == 0) {
        const std::string page = s;
        const zgame::Handle st = page == "options_gfx" ? ge.peek32(kGfxOptionsStateGlobal) : page == "options_controls" ? ge.peek32(kControlsStateGlobal)
                                 : page == "options_upgrades" ? mech3::options_upgrades_state() : ge.peek32(kOptionsStateGlobal);
        if (st) ge.push_state(st, 0);
      }
      const char* sc = std::getenv("MW1_SCREEN");
      if (!sc || std::string(sc) == "hub" || std::string(sc) == "event") hub_events();
      return true;
    }
    // No hub layout (MW1's data missing): the message boxes, as before the hub: the office's offer put as a question.
    if (pending_) {
      ge.message_box_words(kBoxOk, "After action", join(after_action_lines(), "\n"));
      press(kHubOk);
    }
    if (!contract_.accepted) {
      if (!offered_) new_offer();
      if (!offered_) return returning;  // no House office here
      if (static_cast<int32_t>(ge.message_box_words(kBoxYesNo, "MechWarrior 1: Critical Hit", plain_line(join(offer_lines(true), "\n")) + "\n\nAccept and deploy?")) !=
          kAnswerYes)
        return returning;
      take_contract();
    }
    deploy(ge);
    return true;
  }

  // ---- MW1's main menu and the intro (mw1_menu.h) ----

  // MW1's image archive (mw1_images.h) beside PM's, once: the intro's backdrop and picture.
  // MW1's fonts (data/mw1fonts.zrd) beside the game's, once on each 2D host.
  static void register_fonts() {
    static ztwod::Host* added = nullptr;
    if (added == &ztwod::host()) return;
    added = &ztwod::host();
    ztwod::host().add_font_table(kFontsFile);
  }

  static void register_images() {
    for (zimg::Library* lib : zimg::library_list(1))
      if (lib && std::strstr(lib->name, kImageLibrary)) return;
    zimg::Library* lib = zimg::new_image_library(kImageLibrary, 0, false);
    // The screens' art beside it (mw1_art.h): mw1art.zbd where mw1image.zbd resolved to.
    if (!lib || !zimg::open(*lib)) return;
    std::string path = std::string(zimg::state().dir) + lib->name;
    const size_t at = path.rfind(kImageLibrary);
    if (at == std::string::npos) return;
    path.replace(at, std::strlen(kImageLibrary), art::kArtLibrary);
    if (!art::install(path, true)) std::printf("mw1: no art archive (%s): the screens lack their pictures\n", path.c_str());
  }

  // The menu's state on this engine, made once; false when MW1's layout is not there.
  bool menu(mech3::GameEngine& ge) {
    if (menu_engine_ == &ge && ge.menu_state_of(menu_) == menu_state_) return menu_ != 0;
    menu_engine_ = &ge, menu_ = 0, menu_state_ = nullptr, intro_ = 0, intro_state_ = nullptr;
    if (!layout_there(kMenuFile, kMenuDialog)) {
      std::printf("mw1: no menu layout (%s in MW1's data): the hub at the start\n", kMenuFile);
      return false;
    }
    menu_ = ge.adopt_state([this](zgame::Handle h) { return menu_state_ = new MenuState(h, [this](int32_t id) { as_player([&] { press_menu(id); }); }); });
    return menu_ != 0;
  }

  bool save_there() const {
    std::error_code ec;
    return !save_path_.empty() && std::filesystem::is_regular_file(save_path_, ec);
  }

  void show_menu() {
    if (!menu_state_) return;
    // CONTINUE with a company to go back to: the one in play, or the save.
    std::string line;
    if (company_.stores) line = title_case_name(company_.planet.c_str()) + ", " + thousands(company_.funds) + " C-bills";
    bool can_continue = company_.stores != nullptr;
    if (!company_.stores && save_there()) {  // the saved company, read to name it (the game loads it at CONTINUE)
      Company saved;
      Contract offer;
      AfterAction after;
      bool has_offer = false, has_after = false;
      std::string why;
      if (load_file(save_path_, saved, offer, &has_offer, &why, &after, &has_after) == Loaded::kLoaded) {
        line = title_case_name(saved.planet.c_str()) + ", " + thousands(saved.funds) + " C-bills";
        can_continue = true;
      } else {
        line = "The save doesn't read: a new campaign";  // CONTINUE would make a new company (start_company)
      }
    }
    menu_state_->show_menu(can_continue, line);
  }

  void press_menu(int32_t id) {
    std::printf("mw1: menu: %s\n", menu_button_name(id));
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    switch (id) {
      case kMenuNew: screen_change([this, ge] { new_campaign(*ge, true); }); return;
      case kMenuContinue:
        if (company_.stores) {  // the hub's QUIT came back here: the same company, the same offer
          screen_change([this, ge] {
            show_hub();
            ge->push_state(hub_, 0);
            hub_events();
          });
          return;
        }
        if (save_there()) screen_change([this, ge] { enter_hub(*ge); });
        return;
      case kMenuOptions:  // PM's own options, the state its main menu made (its OPTIONS button's, 0x00609868)
        if (const zgame::Handle options = ge->peek32(kOptionsStateGlobal)) ge->push_state(options, 0);
        return;
      case kMenuQuit: ge->confirm_quit(); return;
      default: return;  // Escape: nothing
    }
  }

  // NEW CAMPAIGN: over a company asks first, and keeps its save as mw1save.prev; then MW1's intro, then the new company
  // in the hub (intro_done). `ask` false: --new-company's (the flag is the asking).
  void new_campaign(mech3::GameEngine& ge, bool ask) {
    const bool over = company_.stores || save_there();
    if (ask && over &&
        static_cast<int32_t>(ge.message_box_words(kBoxYesNo, "MechWarrior 1: Critical Hit",
                                                  "Start a new campaign?\n\nThe company you have now is kept as " + std::string(kPrevSaveFile) +
                                                      ".")) != kAnswerYes)
      return;
    if (save_there()) {
      const std::filesystem::path prev = std::filesystem::path(save_path_).replace_filename(kPrevSaveFile);
      std::error_code ec;
      std::filesystem::rename(save_path_, prev, ec);
      std::printf("mw1: the company's save kept as %s%s\n", prev.string().c_str(), ec ? " (failed)" : "");
    }
    company_ = Company{}, contract_ = Contract{}, pending_.reset(), offered_ = false, deployed_ = false, launched_ = false;
    fresh_ = true;  // start_company makes one, loading nothing
    if (layout_there(kIntroFile, kIntroDialog) && menu_engine_ == &ge) {
      std::vector<std::string> lines = intro_narration();
      std::printf("mw1: the intro, %zu lines of narration\n", lines.size());
      intro_ = ge.adopt_state([this, lines](zgame::Handle h) {
        return intro_state_ = new IntroState(h, lines, [this] { intro_done(); });
      });
      if (intro_) {
        // MW1_SCREEN=intro_picture (development: a --shot of the picture without the narration's wait).
        if (const char* sc = std::getenv("MW1_SCREEN"); sc && std::string(sc) == "intro_picture")
          intro_state_->start_at(intro_length() - 3.0f);
        ge.push_state(intro_, 0);
        return;
      }
    }
    intro_done();
  }

  // The intro over (or skipped): the new company, its first offer, the hub over the menu.
  void intro_done() {
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    // Popped with 0 (FIXES.md item 35): the menu under it not redrawn and shown for a frame before the hub.
    if (intro_ && ge->menu_state_of(intro_) == intro_state_) ge->pop_state(0);
    enter_hub(*ge);
    fresh_ = false;
  }

  // ---- the hub (mw1_hub.h) ----

  // One MW1 screen to the next in one step (FIXES.md item 35): the app's Replace (0x00590250), the screen up leaving
  // and `next` entering, never the state under them (MW1's menu) resumed between: a pop then a push showed the menu
  // for a frame at every change. Pirate's Moon's way (pop, push) when `next` isn't a menu state.
  void swap_to(mech3::GameEngine& ge, zgame::Handle next) {
    mech3::MenuState* st = ge.menu_state_of(next);
    if (mech3::Mech3App* app = mech3::app_if_any(); st && app) {
      app->replace(st, 0);
      return;
    }
    ge.pop_state(1);
    ge.push_state(next, 0);
  }

  // FIXES.md item 52: a change of screen faded (out to black, the change, in), when the player's press asks for it.
  // A press from a screen's button (as_player) runs at once, but the screen changes it asks for (screen_change) are
  // kept, in order, and made together under black; a press from the code (the tests, the shots) changes at once.
  // A message in the bottom box isn't a change of screen: never kept, never faded.
  bool fade_hook_ = false;
  bool queuing_ = false;
  std::vector<std::function<void()>> queued_;
  void screen_change(std::function<void()> fn) {
    if (queuing_) queued_.push_back(std::move(fn));
    else fn();
  }
  void as_player(const std::function<void()>& press) {
    if (queuing_ || fade::current().busy()) return;  // during a fade: ignored
    queuing_ = true;
    press();
    queuing_ = false;
    if (queued_.empty()) return;
    std::vector<std::function<void()>> ops = std::move(queued_);
    queued_.clear();
    auto run = [ops] {
      for (const auto& f : ops) f();
    };
    if (fade::enabled() && ge_) fade::current().start(run);
    else run();
  }

  // The hub's state on this engine, made once (Engine::adopt_state); false when MW1's layout is not there.
  bool hub(mech3::GameEngine& ge) {
    if (hub_engine_ == &ge && ge.menu_state_of(hub_) == hub_state_) return hub_ != 0;
    hub_engine_ = &ge, ge_ = &ge, hub_ = 0, hub_state_ = nullptr;
    market_ = 0, market_state_ = nullptr;
    travel_ = 0, travel_state_ = nullptr;
    bar_ = 0, bar_state_ = nullptr;
    office_ = 0, office_state_ = nullptr;
    hub_options_ = 0, hub_options_state_ = nullptr;
    for (int32_t i = 0; i < kSides; ++i) events_[i] = 0, event_states_[i] = nullptr;
    event_state_ = nullptr, event_up_ = false;
    results_ = 0, results_state_ = nullptr, results_up_ = false;
    if (!layout_there(kHubFile, kHubDialog)) {
      std::printf("mw1: no hub layout (%s in MW1's data): the message boxes\n", kHubFile);
      return false;
    }
    hub_ = ge.adopt_state([this](zgame::Handle h) { return hub_state_ = new HubState(h, [this](int32_t id) { as_player([&] { press(id); }); }); });
    if (hub_state_) hub_state_->set_hover([this](int32_t id) { hub_hovered(id); });
    return hub_ != 0;
  }

  // A button of the hub (or the boxes' answers, without it).
  void press(int32_t id) {
    std::printf("mw1: hub: %s\n", hub_button_name(id));
    mech3::GameEngine* ge = ge_;
    // The box's next page first (FIXES.md item 42: six lines a page): OK is NEXT until the last.
    if (id == kHubOk && hub_page_ + 1 < hub_pages(hub_view_panel())) {
      ++hub_page_;
      show_hub();
      return;
    }
    hub_page_ = 0;
    company_.notices.clear();  // the hub's last news, shown once
    hub_hover_ = 0;            // left (or the button gone): the status line's own words again
    switch (id) {
      case kHubDeploy:
        // The contract taken at the office: the briefing (the hub off, then the main menu replaced by it).
        if (pending_ || !contract_.accepted || !ge) return;
        screen_change([this, ge] {
          ge->pop_state(0);  // the menu under it not redrawn (FIXES.md item 35): Load replaces it at once
          deploy(*ge);
        });
        return;
      case kHubCompany:
        // MW1's company screen (CREW_MENU): the crew grid in the hub's place (LEAVE brings the hub back).
        if (pending_ || !ge || !bar(*ge)) return;
        screen_change([this, ge] {
          bchoice_ = BarChoice{};
          bchoice_.page = BarPage::kLance, bchoice_.company = true;
          show_bar();
          swap_to(*ge, bar_);
        });
        return;
      case kHubOk: {
        if (!pending_) {  // the messages read (cleared above): the scenery again
          show_hub();
          return;
        }
        settle_pending();
        show_hub();
        hub_events();  // back at the hub (MW1's after a battle)
        return;
      }
      case kHubReplay:
        if (!pending_ || pending_->outcome != Outcome::kFailure || !ge) return;
        screen_change([this, ge] {
          ge->pop_state(0);  // as DEPLOY's
          replay(*ge);
        });
        return;
      case kHubOptions:
        // MW1's OPTIONS in the hub's place: SETTINGS (PM's options), MAIN MENU, BACK (Escape at the hub too).
        if (pending_ || !ge || !hub_options(*ge)) return;
        screen_change([this, ge] {
          hub_options_state_->show(scenery_image(company_.planet_number), upper(company_.planet));
          swap_to(*ge, hub_options_);
        });
        return;
      case kHubMarket:
        // The market in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !market(*ge)) return;
        screen_change([this, ge] {
          choice_ = MarketChoice{};
          show_market();
          swap_to(*ge, market_);
          story_events(kClassMechComplex);
        });
        return;
      case kHubTravel:
        // The travel screen in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !travel(*ge)) return;
        screen_change([this, ge] {
          tchoice_ = TravelChoice{};
          show_travel();
          swap_to(*ge, travel_);
        });
        return;
      case kHubBar:
        // The Bar in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !bar(*ge)) return;
        screen_change([this, ge] {
          bchoice_ = BarChoice{};
          show_bar();
          swap_to(*ge, bar_);
          story_events(kClassBarEntry);
        });
        return;
      case kHubOffice:
        // The House office in the hub's place (BACK brings the hub back): the offer to negotiate, or why there's none.
        // A place, as MW1's: it opens wherever the world has one (its events fire on entering).
        if (pending_ || contract_.accepted || !contracts_here(company_) || !ge || !office(*ge)) return;
        screen_change([this, ge] {
          ochoice_ = OfficeChoice{};
          show_office();
          swap_to(*ge, office_);
          story_events(kClassOffice);
        });
        return;
      default: return;  // the screens that come later
    }
    show_hub();
  }

  // ---- the House office (mw1_campaign.h's negotiate, mw1_office.h) ----

  bool office(mech3::GameEngine& ge) {
    if (office_ && ge.menu_state_of(office_) == office_state_) return true;
    if (!layout_there(kOfficeFile, kOfficeDialog)) {
      std::printf("mw1: no office layout (%s in MW1's data)\n", kOfficeFile);
      return false;
    }
    office_ = ge.adopt_state([this](zgame::Handle h) { return office_state_ = new OfficeState(h, [this](int32_t id) { as_player([&] { press_office(id); }); }); });
    return office_ != 0;
  }

  void press_office(int32_t id) {
    if (id < kOfficePayDown || id > kOfficeAdvanceUp) std::printf("mw1: office: %s\n", office_button_name(id));  // the arrows unlogged
    mech3::GameEngine* ge = ge_;
    switch (id) {
      case kOfficeSubmit: {
        if (!offered_ || pending_ || !ochoice_.ask.any()) return;
        const Answer a = negotiate(company_, contract_, ochoice_.ask, rng_);
        ochoice_.answer = answer_lines(a, company_, contract_);
        ochoice_.ask = Request{};  // MW1 clears the ask after each round
        std::printf("mw1: office: round %d (patience %d%s): %s; the offer %d thousand, salvage %d%%, advance %d%%\n", contract_.attempts,
                    company_.patience, never_breaks_off(company_.planet_house) ? ", never breaks off" : "",
                    a == Answer::kAgreed ? "agreed" : a == Answer::kCounter ? "a counter-offer" : "terminated", contract_.payment,
                    contract_.salvage_percent, contract_.advance_percent);
        if (a == Answer::kAgreed) {
          take_contract();
          back_to_hub();  // the hub with the contract and its DEPLOY
          return;
        }
        if (a == Answer::kTerminated) {
          offered_ = false;  // the world's offers withdrawn until the company leaves
          std::printf("mw1: office: negotiations terminated on %s: no contracts here until the company leaves%s\n", company_.planet.c_str(),
                      stranded(company_) ? "; stranded" : "");
        }
        save();
        break;
      }
      case kOfficeAccept:
        if (!offered_ || pending_) return;
        take_contract();
        back_to_hub();
        return;
      case kOfficeRequest:
      case kOfficeNext:
        // REQUEST_MISSION_OFFER and NEXT_MISSION_OFFER: an offer up (another in the last one's place), the asks and
        // the answer cleared; none where the House won't deal (it broke off, or refuses the company).
        if (pending_ || contract_.accepted || !earning_here(company_) || (id == kOfficeNext) != offered_) return;
        new_offer();
        ochoice_ = OfficeChoice{};
        break;
      case kOfficeBack:
        back_to_hub();
        return;
      default:
        if (!offered_) return;
        step_ask(contract_, ochoice_.ask, id);
        break;
    }
    show_office();
  }

  // The office's ACCEPT (or an agreed round): the contract taken, its advance paid (mw1_campaign.h's accept), saved.
  void take_contract() {
    accept(company_, contract_);
    offered_ = false;
    // Said at the hub once (item 16: a contract's state change is a message).
    company_.notices.push_back("Contract accepted with House " + std::string(kHouses[contract_.house].name) + ": " +
                               objective_words(contract_) + ".");
    company_.notices.push_back("Payment " + thousands(int64_t{contract_.payment} * 1000) + " C-bills, " +
                               std::to_string(contract_.advance_percent) + "% paid in advance.");
    company_.notices.push_back("DEPLOY from the hub when the lance is ready.");
    std::printf("mw1: office: contract accepted: %s on %sia%d, %d thousand, %d%% advance; funds %s\n", objective_words(contract_).c_str(),
                contract_.world.c_str(), contract_.ia, contract_.payment, contract_.advance_percent, thousands(company_.funds).c_str());
    save();
  }
  // No contract: the offer gone (a new world, a mission settled).
  void clear_contract() {
    offered_ = false;
    contract_ = Contract{};
    save();
  }
  // A screen's way back to the hub (it in the screen's place); the hub's events (MW1 runs them each time it draws it).
  void back_to_hub() {
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    show_hub();
    swap_to(*ge, hub_);
    hub_events();
  }

  // ---- the hub's OPTIONS (mw1_hub.h) ----

  bool hub_options(mech3::GameEngine& ge) {
    if (hub_options_ && ge.menu_state_of(hub_options_) == hub_options_state_) return true;
    if (!layout_there(kHubFile, kHubOptionsDialog)) return false;
    hub_options_ = ge.adopt_state([this](zgame::Handle h) { return hub_options_state_ = new HubOptionsState(h, [this](int32_t id) { as_player([&] { press_hub_options(id); }); }); });
    return hub_options_ != 0;
  }

  void press_hub_options(int32_t id) {
    std::printf("mw1: options: %s\n", hub_options_button_name(id));
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    switch (id) {
      case kHubOptSettings:  // PM's own options over these (its BACK comes back), the state PM's main menu made
        if (const zgame::Handle options = ge->peek32(kOptionsStateGlobal)) ge->push_state(options, 0);
        return;
      case kHubOptMainMenu:
        screen_change([this, ge] {
          ge->pop_state(1);  // MW1's menu, under these (Pirate's Moon's without it); CONTINUE brings the hub back
          show_menu();
        });
        return;
      case kHubOptBack: screen_change([this] { back_to_hub(); }); return;
      default: return;
    }
  }

  void show_office() {
    if (!office_state_) return;
    std::vector<std::string> closed;
    if (!earning_here(company_)) {
      closed = refuses(company_, company_.planet_house) ? refusal_words(company_.planet_house) : terminated_words();
      closed.push_back(refuses(company_, company_.planet_house) ? "No contracts from this House." : "No contracts here until you leave.");
    }
    office_state_->show(office_view(company_, contract_, ochoice_, offered_ ? offer_lines(false, false) : std::vector<std::string>{}, closed));
  }

  // ---- the Bar (mw1_pilots.h, mw1_bar_screen.h) ----

  bool bar(mech3::GameEngine& ge) {
    if (bar_ && ge.menu_state_of(bar_) == bar_state_) return true;
    if (!layout_there(kBarFile, kBarDialog)) {
      std::printf("mw1: no Bar layout (%s in MW1's data)\n", kBarFile);
      return false;
    }
    bar_ = ge.adopt_state([this](zgame::Handle h) { return bar_state_ = new BarState(h, [this](int32_t id) { as_player([&] { press_bar(id); }); }); });
    return bar_ != 0;
  }

  void press_bar(int32_t id) {
    std::printf("mw1: bar: %s\n", bar_button_name(id));
    mech3::GameEngine* ge = ge_;
    clamp_choice(company_, bchoice_);
    const bool recruit = bchoice_.recruit < static_cast<int32_t>(company_.recruits.size());
    const std::string who = recruit ? pilot_title(company_.recruits[static_cast<size_t>(bchoice_.recruit)]) : "";
    const std::string mate =
        bchoice_.mate < static_cast<int32_t>(company_.lance.size()) ? pilot_title(company_.lance[static_cast<size_t>(bchoice_.mate)].id) : "";
    bool changed = false;
    const int32_t weight = lance_weight(company_);
    switch (id) {
      case kBarHire: {
        const Hire h = hire(company_, bchoice_.recruit);
        bchoice_.note = h == Hire::kDone ? who + " joins the lance."
                        : h == Hire::kFull ? std::string(hire_words(h)) + " " + who + " won't wait: the offer is gone."
                                           : hire_words(h);
        if (h == Hire::kDone) bchoice_.mate = static_cast<int32_t>(company_.lance.size()) - 1;
        changed = h != Hire::kNoSuch;
        break;
      }
      case kBarDecline:
        changed = decline(company_, bchoice_.recruit);
        bchoice_.note = changed ? who + " moves on." : "Nobody chosen.";
        break;
      case kBarFire:
        changed = fire(company_, bchoice_.mate);
        bchoice_.note = changed ? mate + " leaves the company." : "Nobody chosen.";
        break;
      case kBarAssign: {
        changed = assign(company_, bchoice_.mate);
        const int32_t h = mech_of(company_, bchoice_.mate);
        bchoice_.note = !changed ? "Nobody chosen."
                        : h >= 0 ? mate + " takes the " + mech_title(company_.hangar[static_cast<size_t>(h)].mech) + "."
                                 : mate + " has no mech: they stay behind.";
        break;
      }
      case kBarDrink:
        // MW1's drink (MwOrderDrinkContinuation): a unit of time (the wages if a month begins), then the drink's events:
        // a tip at its price, or the quiet text.
        if (!story_running() || event_up_) return;
        advance_time(company_, 1, rng_);
        story_events(kClassBarDrink);
        return;
      case kBarNews:
        // MW1's NewsNet (MwOpenNewsNetContinuation): a unit of time, then the news live now, story by story.
        if (!story_running() || event_up_) return;
        advance_time(company_, 1, rng_);
        open_news();
        return;
      case kBarCrew: bchoice_.page = BarPage::kRecruits, bchoice_.note.clear(); break;
      case kBarLance: bchoice_.page = BarPage::kLance, bchoice_.note.clear(); break;
      case kBarDone:
        if (bchoice_.company) return back_to_hub();  // the hub's COMPANY: LEAVE
        bchoice_.page = BarPage::kMenu, bchoice_.note.clear();
        break;
      case kBarRecruitUp: --bchoice_.recruit; break;
      case kBarRecruitDown: ++bchoice_.recruit; break;
      case kBarLanceUp: --bchoice_.mate; break;
      case kBarLanceDown: ++bchoice_.mate; break;
      case kBarBack:
        if (bchoice_.company) return back_to_hub();  // Escape on the hub's COMPANY
        if (bchoice_.page != BarPage::kMenu) {  // Escape on a page: back to the Bar's menu
          bchoice_.page = BarPage::kMenu, bchoice_.note.clear();
          break;
        }
        if (!ge) return;
        screen_change([this] { back_to_hub(); });
        return;
      default: return;
    }
    if (changed) {
      std::printf("mw1: bar: %s\n", bchoice_.note.c_str());
      // The lance weighs on the offer (MW1's): when its weight changed, the offer is drawn again for it.
      if (lance_weight(company_) != weight && offered_ && !pending_ && !contract_.accepted) new_offer();
      save();
    }
    show_bar();
  }

  void show_bar() {
    clamp_choice(company_, bchoice_);
    if (bar_state_) bar_state_->show(bar_view(company_, bchoice_, story_running()));
  }

  // ---- travel (mw1_travel.h, mw1_travel_screen.h) ----

  bool travel(mech3::GameEngine& ge) {
    if (travel_ && ge.menu_state_of(travel_) == travel_state_) return true;
    if (!layout_there(kTravelFile, kTravelDialog)) {
      std::printf("mw1: no travel layout (%s in MW1's data)\n", kTravelFile);
      return false;
    }
    travel_ = ge.adopt_state([this](zgame::Handle h) { return travel_state_ = new TravelState(h, [this](int32_t id) { as_player([&] { press_travel(id); }); }); });
    if (travel_state_) {
      // The map hears the mouse (a world chosen by a click, named under the mouse) and the keys (the arrows, a name
      // typed, Enter).
      travel_state_->set_pointer(kMapLeft, kMapTop, kMapLeft + kMapWidth - 1, kMapTop + kMapHeight - 1,
                                 [this](int32_t x, int32_t y, bool click) { travel_point(x, y, click); });
      travel_state_->set_keys([this](uint32_t k) { travel_key(k); });
    }
    return travel_ != 0;
  }

  // The mouse on the map: a click chooses the world there; a move names the world under it.
  void travel_point(int32_t x, int32_t y, bool click) {
    const int32_t w = world_at(x, y);
    if (click) {
      if (w < 0) return;
      tchoice_.world = w, tchoice_.search = Search{}, tchoice_.note.clear();
    } else {
      if (w == tchoice_.hover) return;
      tchoice_.hover = w;
    }
    show_travel();
  }

  // The keys on the map (DirectInput codes): the arrows step to the nearest world that way; Enter is GO; letters
  // find a world by name, Backspace takes the last off.
  void travel_key(uint32_t code) {
    constexpr uint32_t kUp = 0xC8, kDown = 0xD0, kLeft = 0xCB, kRight = 0xCD, kEnter = 0x1C, kPadEnter = 0x9C, kBackspace = 0x0E;
    switch (code) {
      case kUp: step_choice(company_, tchoice_, Dir::kUp); break;
      case kDown: step_choice(company_, tchoice_, Dir::kDown); break;
      case kLeft: step_choice(company_, tchoice_, Dir::kLeft); break;
      case kRight: step_choice(company_, tchoice_, Dir::kRight); break;
      case kEnter:
      case kPadEnter: return press_travel(kTravelGo);
      case kBackspace:
        if (const int32_t w = type_backspace(tchoice_.search); w >= 0) tchoice_.world = w;
        break;
      default: {
        // The host's keyboard layout's letter; without one (a host with no layout, the tests'), the US key's.
        uint32_t ch = zin::key_char(code);
        if (ch < 0x20 || ch > 0x7e) ch = static_cast<uint8_t>(scan_letter(code));
        if (!ch) return;
        if (const int32_t w = type_letter(tchoice_.search, static_cast<char>(ch)); w >= 0) tchoice_.world = w;
        break;
      }
    }
    if (code != kBackspace && (code == kUp || code == kDown || code == kLeft || code == kRight)) tchoice_.search = Search{};
    tchoice_.note.clear();
    show_travel();
  }

  void press_travel(int32_t id) {
    std::printf("mw1: travel: %s\n", travel_button_name(id));
    mech3::GameEngine* ge = ge_;
    switch (id) {
      case kTravelGo: {
        const int32_t w = chosen_world(company_, tchoice_);
        const int64_t cost = w >= 0 ? travel_cost(company_, kPlanets[w]) : 0;
        const Trip t = mw1::travel(company_, w, rng_);
        if (t != Trip::kDone) {
          tchoice_.note = trip_words(t);
          std::printf("mw1: travel: %s\n", tchoice_.note.c_str());
          break;
        }
        // Arrived (081B:5F15): the new world's market (travel's); its offers at its House office; the hub.
        std::printf("mw1: travelled to %s for %s C-bills: %s, funds %s\n", company_.planet.c_str(), thousands(cost).c_str(),
                    date_words(company_.date).c_str(), thousands(company_.funds).c_str());
        clear_contract();
        if (!ge) return;
        screen_change([this] { back_to_hub(); });
        return;
      }
      case kTravelFilter: tchoice_.filter = next_filter(tchoice_.filter); break;
      case kTravelBack:
        if (!ge) return;
        screen_change([this] { back_to_hub(); });
        return;
      default: return;
    }
    show_travel();
  }

  void show_travel() {
    if (travel_state_) travel_state_->show(travel_view(company_, tchoice_));
  }

  // ---- the Mech Complex (mw1_market.h, mw1_market_screen.h) ----

  // The Mech Complex's state on the hub's engine, made once; false when MW1's layout for it is not there.
  bool market(mech3::GameEngine& ge) {
    if (market_ && ge.menu_state_of(market_) == market_state_) return true;
    if (!layout_there(kMarketFile, kMarketDialog)) {
      std::printf("mw1: no market layout (%s in MW1's data)\n", kMarketFile);
      return false;
    }
    market_ = ge.adopt_state([this](zgame::Handle h) { return market_state_ = new MarketState(h, [this](int32_t id) { as_player([&] { press_market(id); }); }); });
    if (market_state_) {
      market_state_->set_pointer(kMarketListArea[0], kMarketListArea[1], kMarketListArea[2], kMarketListArea[3],
                                 [this](int32_t x, int32_t y, bool click) { market_point(x, y, click); });
      market_state_->set_resumed([this] { lab_closed(); });
    }
    return market_ != 0;
  }

  void press_market(int32_t id) {
    std::printf("mw1: market: %s\n", market_button_name(id));
    mech3::GameEngine* ge = ge_;
    Deal d = Deal::kDone;
    std::string done;
    MarketChoice& m = choice_;
    switch (id) {
      case kMarketBuy: {
        const int32_t mech = m.stock < static_cast<int32_t>(company_.market.stock.size()) ? company_.market.stock[m.stock] : -1;
        const int64_t price = mech >= 0 ? buy_price(mech, company_.tier) : 0;
        if ((d = buy(company_, m.stock)) == Deal::kDone) {
          done = "Bought: " + mech_title(mech) + " for " + thousands(price) + " C-bills.";
          m.owned = owned_count(company_) - 1;  // the new one chosen in YOUR MECHS (the hangar's last)
        }
        break;
      }
      case kMarketSell: {
        const int32_t mech = owned_mech(company_, m.owned);
        if ((d = sell(company_, m.owned)) == Deal::kDone)
          done = "Sold: " + mech_title(mech) + " for " + thousands(sell_price(mech, company_.tier)) + " C-bills.";
        break;
      }
      case kMarketLab:
        if (ge) screen_change([this, ge] { open_lab(*ge); });
        return;
      case kMarketStockUp: --m.stock, m.own_shown = false; break;
      case kMarketStockDown: ++m.stock, m.own_shown = false; break;
      case kMarketOwnUp: --m.owned, m.own_shown = true; break;
      case kMarketOwnDown: ++m.owned, m.own_shown = true; break;
      case kMarketBack:
        if (!ge) return;
        screen_change([this] { back_to_hub(); });
        return;
      default: return;
    }
    if (id == kMarketBuy || id == kMarketSell) {
      m.note = d == Deal::kDone ? done : deal_words(d);
      std::printf("mw1: market: %s\n", m.note.c_str());
      if (d == Deal::kDone) save();
    }
    show_market();
  }

  // A click on a list's row chooses it: FOR SALE's or YOUR MECHS', as scrolled.
  void market_point(int32_t x, int32_t y, bool click) {
    bool own = false;
    int32_t row = 0;
    const bool wide = market_state_ && market_state_->screen() && market_state_->screen()->wide();
    if (!click || !market_row_at(x, y, wide, &own, &row)) return;
    row += own ? choice_.owned_top : choice_.stock_top;
    const int32_t count = own ? owned_count(company_) : static_cast<int32_t>(company_.market.stock.size());
    if (row < 0 || row >= count) return;
    (own ? choice_.owned : choice_.stock) = row;
    choice_.own_shown = own;
    choice_.note.clear();
    show_market();
  }

  void show_market() {
    clamp_choice(company_, choice_);
    if (market_state_) market_state_->show(market_view(company_, choice_));
  }

  // MECH LAB (FIXES.md item 8): Pirate's Moon's lab on the chosen mech, as the briefing's MECHLAB opens it, with a
  // copy of the company's stores. The company pilot's Instant Action slot carries the mech and its loadout; when the
  // lab's ACCEPT brings the Mech Complex back (lab_closed), the slot's loadout and the store become the company's.
  void open_lab(mech3::GameEngine& ge) {
    const int32_t i = choice_.owned, mech = owned_mech(company_, i);
    if (mech < 0) return;
    mech3::Pilot* p = company_pilot();
    mech3::current_pilot() = p;
    p->select_game(mech3::kInstantAction);
    mech3::ActionGame* g = p->action();
    while (g->count > 0) g->remove(0);
    g->slot.set_config(nullptr), g->slot.mech = mech;
    // A copy of its loadout; on its stock one (null), the stock loadout itself: the lab edits a whole mech, where a
    // slot with none shows it bare.
    const mech3::Config* loadout = owned_loadout(i) ? owned_loadout(i).get() : stock_loadout(mech);
    g->slot.set_config(loadout);
    g->store.clear();
    g->store.full = 0;
    g->store.add_all(*company_.stores);
    p->save();
    lab_owned_ = i, lab_who_ = pilot_now(company_, i);
    if (!lab_hook_) {  // the lab's PILOT line, put on its screen once it is up (each frame shown, until it is)
      lab_hook_ = true;
      auto before = ge.on_present;
      ge.on_present = [this, before](int f) {
        if (before) before(f);
        lab_frame();
      };
    }
    std::printf("mw1: market: the mech lab on the %s\n", mech3::mech_name(mech));
    ge.pilot_chosen(p);  // the lab's pilot is the engine's current one ([0x0060861c])
    ge.mech_lab_open(ge.store_handle(&g->store), 0);
  }
  // FIXES.md item 22: PM's briefing reads its layout's node itself (BriefingScreen's load_node, not Dialog::load), so
  // set_layout_image's swap for briefing.zrd never applies to it: its pointer made ours once it has loaded.
  void briefing_cursor_frame() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (!app || app->current() != &mech3::states().load) return;
    mech3::BriefingScreen* b = mech3::briefing_screen();
    if (!b || !b->loaded || b->cursor != &b->cursor_obj) return;
    zimg::Texture* ours = ztwod::host().image_get(chrome::kCursor);
    if (ours && b->cursor_obj.image != ours) b->cursor_obj.set_cursor_image(ours);
  }

  // The lab's places that PM's layout leaves crossing our frames in our type (FIXES.md item 17; the guard is
  // MW1_SCREEN=lab_fit, mw1_lab_fit.h). Once per lab screen (its ammunition tab's label the mark), then each frame
  // the engine's right-hand arrows past their values.
  void lab_look_frame() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (!ge_ || !app || app->current() != &mech3::states().lab) return;
    mech3::LabScreen* ls = ge_->lab_screen(mech3::states().lab.lab_screen_);
    if (!ls) return;
    mech3::LabUnassignedList& u = ls->unassigned;
    if (u.weapon.labels.empty() || u.ammo.labels.empty() || u.equip.labels.empty() || u.armor.labels.empty()) return;
    if (std::strcmp(u.ammo.labels[0]->text, kAmmoTab) != 0) lab_look_once(*ls);
    // FIXES.md item 26: no empty page. PM's layout has tabs for the chassis, criticals, groups and ACCEPT only; its
    // weapons, equipment and mission-data pages (1, 2, 5) have nothing on them: the chassis in their place.
    if (ls->page == 1 || ls->page == 2 || ls->page == 5) ls->switch_page(0);
    lab_slot_icons(*ls);
    // The chosen page's tray button lit (its ACTIVATE look): LabScreen::refresh, on every page switch and every
    // loadout change, gives the criticals and groups buttons their still look again (Control::refresh), and only the
    // pointer leaving one lit it again (HelpButton::set_blink).
    for (int32_t p : kLabTabPages)
      if (mech3::HelpButton* t = ls->tab(p); t && t->selected && !t->blinking && t->img_activate && t->image != t->img_activate)
        t->set_image(t->img_activate), t->dirty();
    // ENGINE SELECTION's rating and type: refresh puts the right arrow 50 past the value's start ("Extra Light" ran
    // into it); kArrowGap past its end when it is longer.
    mech3::LabEngineTable& et = ls->engine_table;
    if (!(et.flags & ztwod::kHidden) && et.rows.size() >= 3) {
      auto beside = [](ztwod::FontText& v, mech3::LabButton& right) {
        if (v.needs_render) v.render();
        const int32_t want = std::max(v.x + 50, v.x + v.width + kArrowGap);
        if (right.x != want) right.set_pos(want, right.y);
      };
      beside(et.rows[0]->value, et.faster);
      beside(et.rows[2]->value, et.type_right);
    }
  }
  // FIXES.md item 26: the lab's mech slots in MW1's amber (mw1_mech_icons.h): PM's MechButton takes its mech's icons
  // when the slot's mech changes (set_mech); ours in their place each time, the lit look for the chosen slot.
  void lab_slot_icons(mech3::LabScreen& ls) {
    if (slot_screen_ != &ls) slot_screen_ = &ls, slot_ours_.fill(nullptr);
    ztwod::Host& h = ztwod::host();
    for (int32_t i = 0; i < kLabSlots; ++i) {
      mech3::MechButton& b = ls.mechs[i];
      if (b.mech < 0 || b.mech >= mech3::mech_count() || !b.img_normal || b.img_normal == slot_ours_[static_cast<size_t>(i)]) continue;
      zimg::Texture* dim = h.image_get(slot_icon_image(b.mech, SlotLook::kDim).c_str());
      if (!dim) continue;  // no amber icons (no PM icons at the start): PM's stay
      h.image_release(b.img_normal), h.image_release(b.img_activate), h.image_release(b.img_disabled);
      b.img_normal = dim;
      b.img_activate = h.image_get(slot_icon_image(b.mech, SlotLook::kLit).c_str());
      b.img_disabled = h.image_get(slot_icon_image(b.mech, SlotLook::kOff).c_str());
      b.set_image(b.selected ? b.img_activate : b.enabled ? b.img_normal : b.img_disabled);
      b.dirty();
      slot_ours_[static_cast<size_t>(i)] = dim;
    }
  }
  static constexpr int32_t kLabSlots = 12;
  const mech3::LabScreen* slot_screen_ = nullptr;
  std::array<zimg::Texture*, kLabSlots> slot_ours_{};

  void lab_look_once(mech3::LabScreen& ls) {
    place_lab_tabs(ls);
    mech3::LabUnassignedList& u = ls.unassigned;
    // FIXES.md item 39: 'MECH LOAD-OUT's and AVAILABLE SUPPLIES' rows across their panels, kLabListMargin in from
    // each side (PM's 172-wide rows sat at one side), their columns spread with them; the headers over the columns.
    const int32_t layout_left = ge_->buffer_width() / 2 - 320;
    centre_lab_list(ls.assigned, chrome::kLabSideWhole.x0, chrome::kLabSideWhole.x1, layout_left, {});
    centre_lab_list(u, 420 + chrome::kLabSideWhole.x0, 420 + chrome::kLabSideWhole.x1, layout_left,
                    {&u.sort_item, &u.sort_qty, &u.sort_mass, &u.sort_crit});
    // FIXES.md item 40: WEAPON GROUPING's and UNGROUPED WEAPONS' rows inside their panels the same way (PM's tags
    // and the groups' bar ran over the left panel's frame), the tags the rows' width; and a weapon put in a group
    // goes after the group's others (the engine's join_order, ours: PM lists a group in the loadout's order).
    centre_weapon_list(ls.grouped, chrome::kLabSideWhole.x0, layout_left);
    centre_weapon_list(ls.ungrouped, 420 + chrome::kLabSideWhole.x0, layout_left);
    ls.ungrouped.join_order = true;
    // The supply tabs: PM's at fixed x, and "Armor" ran past the right panel's frame. "Ammo" for "Ammunition", and
    // the four laid left to right from the first, kTabGap apart (each tab is its labels: Control::hit).
    for (auto* v : {&u.ammo.labels, &u.ammo.rollover_labels, &u.ammo.activate_labels, &u.ammo.disable_labels})
      for (ztwod::Text* t : *v) t->set_text_plain(kAmmoTab);
    if (u.ammo.text) u.ammo.text->set_text_plain(kAmmoTab);  // the checked look's
    int32_t x = lab_list_left(u);  // from the list's left (item 39)
    for (mech3::LabSelect* t : {&u.weapon, &u.ammo, &u.equip, &u.armor}) {
      ztwod::Text* first = t->labels[0];
      if (first->needs_render) first->render();
      shift_labels(*t, x - first->x, 0);
      t->bounds.left += x - first->x, t->bounds.right += x - first->x;
      t->x += x - first->x;
      x += first->width + kTabGap;
    }
    // The lists' down arrows sat on the bottom tray, below the panels' frames: the lists kListUp shorter (their
    // last row's room) and the arrows with them; the supplies' short size (its equipment tab's) too.
    for (mech3::LabItemList* l : {static_cast<mech3::LabItemList*>(&ls.assigned), static_cast<mech3::LabItemList*>(&u)}) {
      l->size[1] -= kListUp;
      if (l->down) l->down->set_pos(l->down->get_x(), l->down->get_y() - kListUp);
      l->layout();
    }
    u.long_size[1] -= kListUp, u.short_size[1] -= kListUp;
    // FIXES.md item 40 (Owen's gap, item 39's review): 'MECH LOAD-OUT's header and rows a row higher, into the band
    // under its title's rule (where the supplies have their tabs), its down arrow where it was: a row more shown.
    {
      mech3::LabAssignedList& a = ls.assigned;
      const int32_t up = a.spacing[1] + 0xd;  // a row (LabItemList::layout's step)
      std::vector<ztwod::Text*> moved;
      for (auto* v : {&a.labels, &a.rollover_labels, &a.activate_labels, &a.disable_labels})
        for (ztwod::Text* t : *v)
          if (t->y < a.get_y() && t->y >= a.get_y() - kLabHeaderAbove && std::find(moved.begin(), moved.end(), t) == moved.end())
            moved.push_back(t), t->y -= up, t->dirty();
      const int32_t down_y = a.down ? a.down->get_y() : 0;
      a.size[1] += up;
      a.set_rows(a.size);
      a.set_pos(a.get_x(), a.get_y() - up);
      if (a.down) a.down->set_pos(a.down->get_x(), down_y);
      a.layout();
    }
    // ENGINE SELECTION's last row ran past the right lower panel: the table kEngineUp higher, its values
    // kEngineValueW from its labels (PM's 130) for room for the arrows inside the frame.
    mech3::LabEngineTable& et = ls.engine_table;
    et.width = kEngineValueW;
    shift_labels(et, 0, -kEngineUp);
    et.set_pos(et.x, et.y - kEngineUp);
    // MECH DATA's values and its internal structure's choice kDataLeft further left: the right arrow sat on the
    // centre panel's frame. And all of it kDataUp higher: the centre's box ends above the tray now (Owen).
    if (mech3::LabMechData* md = ls.mech_data) {
      md->width -= kDataLeft;
      shift_labels(*md, 0, -kDataUp);
      md->set_pos(md->x, md->y - kDataUp);
      ztwod::Text* value = nullptr;  // the choice's label (offset 170), the rightmost
      for (ztwod::Text* t : md->endo.labels)
        if (!value || t->x > value->x) value = t;
      if (value) value->x -= kDataLeft, value->dirty();
      md->endo.next_dx -= kDataLeft;
      // CycleButton::set_pos moves its first label and the arrows; the rest of its labels by hand.
      ztwod::Text* first = md->endo.labels.empty() ? nullptr : md->endo.labels[0];
      md->endo.set_pos(md->endo.x, md->endo.y - kDataUp);
      std::vector<ztwod::Text*> moved{first};
      for (auto* v : {&md->endo.labels, &md->endo.rollover_labels, &md->endo.activate_labels, &md->endo.disable_labels})
        for (ztwod::Text* l : *v)
          if (std::find(moved.begin(), moved.end(), l) == moved.end()) moved.push_back(l), l->y -= kDataUp, l->dirty();
    }
  }
  // FIXES.md item 29: the tray's four buttons (one picture size, mw1_chrome.h) at even places along it, not PM's (its
  // icons of four sizes, the groups one over the centre's frame); the chosen page's glow centred on its button; item
  // 37: each one's hover label centred above it.
  void place_lab_tabs(mech3::LabScreen& ls) {
    const int32_t left = ge_->buffer_width() / 2 - 320;  // PM's 640 layout, centred
    for (int32_t i = 0; i < 4; ++i) {
      mech3::HelpButton* t = ls.tab(kLabTabPages[i]);
      if (!t) continue;
      t->set_pos(left + lab_tab_x(i), kLabTabY);
      t->glow[0] = (kLabTabW - 127) / 2, t->glow[1] = (kLabTabH - 63) / 2;  // lab_icon_glow's 127x63
      if (t->selected) ls.lab_tab_glow(*t, true, &ls.icon_glow);
      t->dirty();
      // FIXES.md item 37: its HELP label centred above it, under the tray's line (PM's layout placed it for PM's icons).
      for (ztwod::Text* x : t->help) {
        if (x->needs_render) x->render();
        const int32_t middle = left + lab_tab_x(i) + kLabTabW / 2;
        x->set_pos(x->align == 1 ? middle : x->align == 2 ? middle + x->width / 2 : middle - x->width / 2, kLabHelpY);
        x->dirty();
      }
    }
  }
  // FIXES.md item 39: a lab list's rows inside its panel (x0..x1, the frame's outer edges, in the 640 layout) with
  // kLabListMargin each side: its row width (the engine's, ours: LabItemList::row_width, its columns in proportion),
  // its rows moved there, its arrows kept where they were from the rows' right end (inside the panel), its click
  // area across the rows; its header (its own labels on the row above, else the given buttons') over the columns, its
  // titles (its labels higher up) moved with the rows' left.
  static int32_t lab_list_left(mech3::LabItemList& l) { return l.get_x() + l.offset[0] + l.spacing[0]; }
  static void centre_lab_list(mech3::LabItemList& l, int32_t x0, int32_t x1, int32_t layout_left,
                              const std::vector<ztwod::Control*>& header_buttons) {
    const int32_t inner_l = layout_left + x0 + chrome::kLabFrame, inner_r = layout_left + x1 - chrome::kLabFrame;
    const int32_t old_x = lab_list_left(l), old_w = l.row_width;
    const int32_t want = inner_l + chrome::kLabListMargin, w = inner_r - inner_l - 2 * chrome::kLabListMargin;
    auto col = [&](int32_t at) { return want + (at - old_x) * w / old_w; };  // a place on the old row, on the new
    std::vector<std::pair<mech3::LabListArrow*, int32_t>> arrows;  // each arrow from the rows' right end
    for (mech3::LabListArrow* a : {l.up, l.down})
      if (a) arrows.push_back({a, a->get_x() - (old_x + old_w)});
    l.row_width = w;
    const int32_t dx = want - old_x;
    l.set_pos(l.get_x() + dx, l.get_y());
    for (auto& [a, from_end] : arrows) {
      const int32_t ax = std::min(want + w + from_end, inner_r - (a->image ? a->image->width : 0));
      a->set_pos(ax, a->get_y());
    }
    l.rect.left = want, l.rect.right = want + w;
    // The bar to the row's end (chrome::kLabWideBar: the panel's width less the margins, kLabListRowW).
    if (l.bar_image && w == chrome::kLabListRowW)
      if (zimg::Texture* t = ztwod::host().image_get(chrome::reskin_name(chrome::kLabWideBar).c_str())) l.bar_image->set_image(t);
    // The header: the list's own labels on the row above it, or the buttons' (each moved with its click area).
    std::vector<ztwod::Text*> moved;
    for (auto* v : {&l.labels, &l.rollover_labels, &l.activate_labels, &l.disable_labels})
      for (ztwod::Text* t : *v)
        if (t->y < l.get_y() && std::find(moved.begin(), moved.end(), t) == moved.end()) {
          moved.push_back(t);
          if (t->y >= l.get_y() - kLabHeaderAbove) t->x = col(t->x);  // the header's: over its column
          else t->x += dx;                                           // the title's, with the rows' left
          t->dirty();
        }
    for (ztwod::Control* b : header_buttons) {
      if (b->labels.empty()) continue;
      const int32_t d = col(b->labels[0]->x) - b->labels[0]->x;
      shift_labels(*b, d, 0);
      b->bounds.left += d, b->bounds.right += d, b->x += d;
    }
    l.layout();
  }
  static constexpr int32_t kLabHeaderAbove = 16, kLabMarginSlack = 3;
  // FIXES.md item 40: a weapon list's rows (their tags kTagLeft left of the row's text) kLabListMargin inside its
  // panel's frame (x0, the frame's outer left in the 640 layout), the tags kLabListRowW wide, the click area across
  // them; its own labels (the title) moved with the rows.
  static constexpr int32_t kTagLeft = 3;
  static void centre_weapon_list(mech3::LabWeaponList& l, int32_t x0, int32_t layout_left) {
    const int32_t old_left = l.get_x() + l.offset_x + l.spacing_x - kTagLeft;
    const int32_t want = layout_left + x0 + chrome::kLabFrame + chrome::kLabListMargin, dx = want - old_left;
    l.tag_width = chrome::kLabListRowW;
    l.set_pos(l.get_x() + dx, l.get_y());
    for (mech3::FileListArrow* a : {l.up, l.down})
      if (a) a->set_pos(a->get_x() + dx, a->get_y());
    l.rect.left += dx, l.rect.right = std::max(l.rect.right + dx, want + chrome::kLabListRowW);
    if (l.bar_image)
      if (zimg::Texture* t = ztwod::host().image_get(chrome::reskin_name(chrome::kLabWideBar).c_str())) l.bar_image->set_image(t);
    shift_labels(l, dx, 0);
    l.layout();
  }
  // A control's labels, every look's, moved once each.
  static void shift_labels(ztwod::Control& c, int32_t dx, int32_t dy) {
    std::vector<ztwod::Text*> moved;
    for (auto* v : {&c.labels, &c.rollover_labels, &c.activate_labels, &c.disable_labels})
      for (ztwod::Text* l : *v)
        if (std::find(moved.begin(), moved.end(), l) == moved.end()) moved.push_back(l), l->x += dx, l->y += dy, l->dirty();
    if (auto* cb = dynamic_cast<ztwod::CheckButton*>(&c); cb && cb->text &&
        std::find(moved.begin(), moved.end(), cb->text) == moved.end())
      cb->text->x += dx, cb->text->y += dy, cb->text->dirty();
  }
  static constexpr const char* kAmmoTab = "Ammo";
  static constexpr int32_t kTabGap = 8, kListUp = 12, kEngineUp = 9, kEngineValueW = 112, kDataLeft = 6, kDataUp = 8, kArrowGap = 3;

  // The lab's PILOT line and its funds on its screen (mw1_lab_pilot.h), once the lab from MECH LAB is up.
  void lab_frame() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (lab_owned_ < 0 || !ge_ || !app || app->current() != &mech3::states().lab) return;
    mech3::LabScreen* ls = ge_->lab_screen(mech3::states().lab.lab_screen_);
    if (!ls) return;
    if (!lab_funds_.attached()) {  // every page's (FIXES.md item 41); the lab spends nothing, so set once
      lab_funds_.attach(ls, ge_->buffer_width() / 2 - 320 + kFundsX, kFundsY);
      lab_funds_.show(thousands(company_.funds) + " C-BILLS");
    }
    if (lab_pilot_.attached()) {  // the chassis page's only (FIXES.md item 17): hidden on the others
      lab_pilot_.visible(ls->page == kLabChassisPage);
      return;
    }
    // A row in the lab's right-hand panel (MOBILE HQ STOCKPILE), under its list: one chassis here, the one in the
    // lab. The lab is PM's 640 layout centred on a wider canvas: the panel's right edge is the layout's.
    const int32_t layout_right = ge_->buffer_width() / 2 + 320;
    lab_pilot_.attach(ls, layout_right - kPilotFromRight, kPilotY, [this](int32_t by) { lab_pilot_step(by); });
    show_lab_pilot();
    lab_pilot_.visible(ls->page == kLabChassisPage);
  }
  void lab_pilot_step(int32_t by) {
    const std::vector<int32_t> can = pilot_choices(company_, lab_owned_);
    if (can.empty()) return;
    const auto at = std::find(can.begin(), can.end(), lab_who_);
    const int32_t n = static_cast<int32_t>(can.size()), i = at == can.end() ? 0 : static_cast<int32_t>(at - can.begin());
    lab_who_ = can[static_cast<size_t>(((i + by) % n + n) % n)];
    show_lab_pilot();
  }
  void show_lab_pilot() {
    const std::string name = lab_who_ == kNobody ? "spare" : lab_who_ == kCommander ? "YOU" : pilot_title(lab_who_);
    lab_pilot_.show(name, pilot_choices(company_, lab_owned_).size() > 1);
  }

  void lab_closed() {
    if (lab_owned_ < 0) return;
    if (fade::enabled()) fade::current().fade_in();  // FIXES.md item 52: the Mech Complex back in from black
    const int32_t i = lab_owned_;
    lab_owned_ = -1;
    lab_pilot_.forget();  // its screen gone with the lab
    lab_funds_.forget();
    mech3::Pilot* p = mech3::current_pilot();
    mech3::ActionGame* g = p ? p->action() : nullptr;
    if (!g || g->slot.mech != owned_mech(company_, i)) return;
    company_.stores->assign(g->store);
    // ACCEPT lets the lab's slots go, their mechs back into its store as items; the company's mechs are its hangar's,
    // never the stores' (VISION.md).
    for (int32_t r = 0; r < mech3::kMechCount; ++r)
      while (company_.stores->has_mech(r)) company_.stores->remove_mech(r);
    owned_loadout(i) = g->slot.config ? copy_config(*g->slot.config) : nullptr;
    const int32_t mech = g->slot.mech;
    const bool moved = lab_who_ != pilot_now(company_, i);
    set_pilot(company_, i, lab_who_);
    if (moved && lab_who_ == kCommander) choice_.owned = 0;  // the commander's now: owned 0
    choice_.note = "The " + mech_title(mech) + " out of the mech lab" +
                   (lab_who_ == kNobody ? ", a spare." : ", piloted by " + (lab_who_ == kCommander ? std::string("you") : pilot_title(lab_who_)) + ".");
    std::printf("mw1: market: back from the mech lab: %d items in the stores\n", company_.stores->count());
    save();
    show_market();
  }
  std::shared_ptr<mech3::Config>& owned_loadout(int32_t i) {
    return i == 0 ? company_.loadout : company_.hangar[static_cast<size_t>(i - 1)].loadout;
  }

  // ---- the results screen (mw1_results.h, FIXES.md item 19) ----

  bool results_screen(mech3::GameEngine& ge) {
    if (results_ && ge.menu_state_of(results_) == results_state_) return true;
    results_ = 0, results_state_ = nullptr;
    if (!layout_there(kResultsFile, kResultsDialog)) {
      std::printf("mw1: no results layout (%s in MW1's data): AFTER ACTION at the hub\n", kResultsFile);
      return false;
    }
    results_ = ge.adopt_state([this](zgame::Handle h) { return results_state_ = new ResultsState(h, [this](int32_t id) { as_player([&] { press_results(id); }); }); });
    if (results_state_) {
      results_state_->set_keys([this](uint32_t k) { results_key(k); });
      results_state_->set_wheel([this](int32_t d) { results_move(-d); });
    }
    return results_ != 0;
  }

  void show_results() {
    if (!results_state_ || !pending_) return;
    seen_results_ = results_panel();
    rchoice_ = ResultsChoice{};
    // MW1_RESULTS_SCROLL=N, MW1_RESULTS_DETAILS=1 (development: --shot pictures of the list scrolled, DETAILS).
    if (const char* n = std::getenv("MW1_RESULTS_SCROLL")) rchoice_.top = std::atoi(n);
    if (std::getenv("MW1_RESULTS_DETAILS")) rchoice_.details = true;
    clamp_choice(seen_results_, rchoice_);
    show_results_view();
  }

  // Who disabled a wreck, as the results say it.
  std::string killer_words(size_t wreck) const {
    if (!pending_ || wreck >= pending_->killers.size()) return "";
    const int32_t who = pending_->killers[wreck];
    return who == 0 ? "you" : who > 0 ? pilot_title(who) : "";
  }

  // The results as CONTINUE will settle them: the settlement made on copies (after_action_lines' way).
  ResultsPanel results_panel() const {
    ResultsPanel p;
    if (!pending_) return p;
    const AfterAction& a = *pending_;
    Company c = company_;
    Contract k = contract_;
    Rng r = rng_;
    const int64_t before = c.funds;
    const Settlement s = settle(c, k, a.outcome, a.wrecks, r);
    Repairs rep;
    if (a.outcome != Outcome::kRetreat) {
      after_mission(c, a.lance);
      rep = settle_repairs(c, as_bills(a.bills));
    }
    const bool success = a.outcome == Outcome::kSuccess;
    p.planet = upper(company_.planet);
    p.employer = "HOUSE " + upper(kHouses[contract_.house].name);
    p.outcome = success ? "VICTORY" : a.outcome == Outcome::kFailure ? "DEFEAT" : "RETREAT";
    p.scene = scenery_image(company_.planet_number);
    p.replay = a.outcome == Outcome::kFailure && !contract_.rung;  // a story battle: again on the next arrival
    p.house = company_.planet_house;
    // The picture (item 31): someone lost is a lancemate KILLED or MISSING, or the commander's mech destroyed.
    bool lost = false, destroyed = false;
    for (const auto& b : a.bills)
      if (b.who == 0) destroyed = b.destroyed;
    for (const LanceFate& f : a.lance) lost = lost || f.destroyed;
    p.ending = ending_of(a.outcome, lost || destroyed);
    // Under the picture: the world and the employer, the objective, how it went and how long it took.
    std::string objective = objective_words(contract_);
    if (!objective.empty()) objective[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(objective[0])));
    if (contract_.rung) p.employer = "THE STORY";
    p.where.push_back(title_case_name(company_.planet.c_str()) +
                      (contract_.rung == kDarkWingRung ? std::string(", against the Dark Wing")
                       : contract_.rung              ? std::string(", on the trail")
                                                     : ", for House " + std::string(kHouses[contract_.house].name)));
    if (!objective.empty()) p.where.push_back(objective);
    p.where.push_back(std::string(success ? "Accomplished" : a.outcome == Outcome::kFailure ? "Failed" : "Withdrew") + " in " +
                      time_words(a.seconds));
    // PAYMENT: the fee (any deduction), the salvage share, the repair bill, the total and the funds.
    const bool docked = s.payout_percent < 100 && success;
    if (contract_.rung) {
      p.pay.push_back("No fee: not a contract");
    } else {
      p.pay.push_back("Fee: " + thousands(s.fee) +
                      (!docked ? "" : s.payout_percent ? " (" + std::to_string(s.payout_percent) + "% paid)" : " (withheld)"));
      p.pay.push_back("Salvage share: " + thousands(s.salvage));
    }
    for (const std::string& l : rep.lines)
      if (l.rfind("Repair bill", 0) == 0) p.pay.push_back(l);
    const int64_t total = c.funds - before;
    p.pay.push_back("Total: " + std::string(total < 0 ? "-" : "+") + thousands(total < 0 ? -total : total) + "   Funds: " +
                    thousands(c.funds));
    // CREW STATUS: each mech that flew, who flew it, what's left of it (MW1's "JENNER 93 %"); a lancemate's fate.
    for (const auto& b : a.bills) {
      std::string l = mech_title(b.mech) + " (" + (b.who == 0 ? std::string("you") : pilot_title(b.who)) + "): ";
      if (b.destroyed) {
        l += "destroyed";
        for (const LanceFate& f : a.lance)
          if (f.id == b.who && f.destroyed) l += pilot_killed(f) ? ", KILLED" : ", MISSING";
      } else {
        l += std::to_string(std::clamp(100 - (b.damage * 100 + kWhole / 2) / kWhole, 0, 100)) + "%";
      }
      p.crew.push_back(l);
    }
    if (a.bills.empty())  // a mission saved before the bills: the fates alone
      for (const LanceFate& f : a.lance)
        p.crew.push_back(pilot_title(f.id) + (!f.destroyed ? ": came back" : pilot_killed(f) ? ": KILLED" : ": MISSING"));
    for (int32_t id : a.stayed) p.crew.push_back(pilot_title(id) + ": stayed behind");
    // SALVAGE TO STORES: every part kept on a win, each row its item's place (a keep list's, later: item 38).
    if (success && a.salvage && a.salvage->count()) {
      const mech3::MessageText msg = [](int32_t id) { return mech3::pilot_hooks().message(id); };
      int32_t i = 0;
      for (const mech3::ConfigItem* it : a.salvage->items)
        p.salvage.push_back({std::to_string(it->count) + " x " + mech3::item_name(*it, msg) + (it->kind() == 2 ? " ammo" : ""), i++});
    } else {
      p.salvage.push_back({success ? "Nothing salvaged" : "None: the field was lost"});
    }
    // DETAILS: each wreck, who disabled it, its value (the salvage share's base); your mech by location.
    p.kills_title = "ENEMY MECHS DESTROYED: " + std::to_string(a.wrecks.size());
    for (size_t i = 0; i < a.wrecks.size(); ++i) {
      const std::string by = killer_words(i);
      const int64_t v = int64_t{wreck_value(company_, a.wrecks[i])} * 1000;
      p.kills.push_back({mech_title(kMechKinds[a.wrecks[i].kind].pm_mech) + (by.empty() ? "" : " by " + by) + " " + thousands(v),
                         static_cast<int32_t>(i)});
    }
    if (p.kills.empty()) p.kills.push_back({"None"});
    p.mech_title = "YOUR " + upper(mech_title(company_.mech));
    p.mech = mech_lines(a.armor_left, a.structure_left, destroyed);
    // The message (item 31): the exe's outcome line, and the deduction's after a win paid short (MW1's
    // MwRandomUpTo(2) of the three, from the settlement's draws on).
    if (contract_.rung == kDarkWingRung) {  // MW1's: lost, its attack-failed line; won, the victory text after CONTINUE
      const std::string failed = dark_wing_failed();
      if (!success && !failed.empty()) p.message.push_back(failed);
    } else {
      p.message = outcome_message(company_.planet_house, a.outcome, false, deduction_of(a.outcome, s.payout_percent, r.upto(2)));
    }
    return p;
  }

  // The results' buttons other than CONTINUE and REPLAY, and the keys and wheel: DETAILS, the list's scroll.
  void results_move(int32_t by) {
    rchoice_.top += by;
    clamp_choice(seen_results_, rchoice_);
    show_results_view();
  }
  void results_key(uint32_t code) {
    constexpr uint32_t kUp = 0xC8, kDown = 0xD0, kPageUp = 0xC9, kPageDown = 0xD1;
    if (code == kUp) results_move(-1);
    else if (code == kDown) results_move(1);
    else if (code == kPageUp) results_move(-kListRows);
    else if (code == kPageDown) results_move(kListRows);
  }
  void show_results_view() {
    if (results_state_) results_state_->show(results_view(seen_results_, rchoice_, ge_ && ge_->buffer_width() > 640));
  }

  void press_results(int32_t id) {
    if (id == kResultsDetails) return void((rchoice_.details = !rchoice_.details, rchoice_.top = 0, show_results_view()));
    if (id == kResultsListUp || id == kResultsListDown) return results_move(id == kResultsListUp ? -1 : 1);
    std::printf("mw1: results: %s\n", id == kResultsReplay ? "REPLAY" : "CONTINUE");
    mech3::GameEngine* ge = ge_;
    if (!results_up_ || !pending_ || !ge) return;
    if (id == kResultsReplay) {
      if (pending_->outcome != Outcome::kFailure) return;
      screen_change([this, ge] {
        ge->pop_state(0);  // the results, the hub under it not redrawn (FIXES.md item 35)
        results_up_ = false;
        ge->pop_state(0);  // the hub, nor the menu
        replay(*ge);
      });
      return;
    }
    screen_change([this, ge] {
      ge->pop_state(1);
      results_up_ = false;
      company_.notices.clear();
      settle_pending();
      show_hub();
      hub_events();  // back at the hub (MW1's after a battle)
    });
  }

  // ---- the story's events (mw1_story.h, mw1_event_screen.h) ----

  // The panel's state for a side (Side), made once; false when its layout is not there.
  bool event_screen(mech3::GameEngine& ge, Side side) {
    const auto i = static_cast<size_t>(side);
    if (events_[i] && ge.menu_state_of(events_[i]) == event_states_[i]) return true;
    if (!layout_there(kEventFile, event_dialog(side))) {
      std::printf("mw1: no event layout (%s %s in MW1's data): no story\n", kEventFile, event_dialog(side));
      return false;
    }
    events_[i] = ge.adopt_state([this, side, i](zgame::Handle h) {
      return event_states_[i] = new EventState(h, side, [this](int32_t id) { as_player([&] { press_event(id); }); });
    });
    return events_[i] != 0;
  }

  // The panel up on ev_panel_.side's state (pushed, or the other side's swapped for it); false when it can't be.
  bool raise_event(mech3::GameEngine& ge) {
    const Side side = ev_panel_.side;
    if (event_up_ && event_side_ == side) return true;
    if (!event_screen(ge, side)) return false;
    if (event_up_) swap_to(ge, events_[static_cast<size_t>(side)]);  // one side's for the other, in one step (item 35)
    else ge.push_state(events_[static_cast<size_t>(side)], 0);
    event_state_ = event_states_[static_cast<size_t>(side)], event_side_ = side, event_up_ = true;
    return true;
  }

  // For the shots (MW1_SCREEN=message_*): the quiet text as a class's message over its screen; OK ends it.
  void message_over(int32_t cls) {
    if (!ge_ || !story_running() || event_up_) return;
    ev_ = start(cls);
    ev_.done = true;
    ev_shown_ = Shown{};
    ev_panel_ = EventPanel{};
    ev_panel_.planet = upper(company_.planet), ev_panel_.place = place_words(cls);
    place_over(cls, ev_panel_);
    set_pages(text_pages(kEventQuiet));
    if (raise_event(*ge_)) show_event();
  }

  // The screen a class's messages fire over (FIXES.md items 34, 42): its picture behind, the box at its foot.
  void place_over(int32_t cls, EventPanel& p) const {
    switch (cls) {
      case kClassBarEntry:
      case kClassBarDrink:
      case kClassNews: p.scene = kBarScene, p.side = Side::kBottom; return;
      case kClassMechComplex: p.scene = kMarketScene, p.side = Side::kBottom; return;
      case kClassOffice: p.scene = kOfficeScene, p.side = Side::kBottom; return;
      default: p.scene = scenery_image(company_.planet_number), p.side = Side::kBottom; return;  // the hub's
    }
  }

  // MW1's pages on the panel: as they are on a full page; on a screen's foot rewrapped to the box's width, 16:9's
  // (MW1EVENTBOTTOM_WIDE, on a canvas wider than 640) or 4:3's, and cut to its lines (boxed_pages).
  void set_pages(std::vector<Page> pages) {
    ev_panel_.wide = ge_ && ge_->buffer_width() > 640;
    ev_panel_.pages = ev_panel_.side == Side::kBottom ? boxed_pages(pages, ev_panel_.wide ? kBoxWideColumns : kBoxColumns)
                                                      : std::move(pages);
    if (ev_panel_.pages.empty()) ev_panel_.pages.push_back({});
  }

  bool story_running() const { return story_on_ && script().there() && company_.stores && ge_; }

  // At the hub (MW1 runs its events each time it draws it; not while AFTER ACTION waits): the news new since it last
  // said, under its panel (research/story.md Q8, provisional), then its events (class 0; an arrival's after travel).
  void hub_events() {
    if (!story_running() || pending_ || event_up_) return;
    const std::vector<Shown> news = fresh_news(company_, rng_);
    for (const Shown& n : news) {
      const std::string h = headline(company_, n);
      std::printf("mw1: story: news %#x\n", n.event);
      if (!h.empty()) company_.notices.push_back("NEWS: " + h);
    }
    if (!news.empty()) save();  // heard (and seen): not announced again after a quit
    show_hub();
    story_events(kClassArrival);
  }

  // A class's events fired in turn on the panel, each waiting for the player; none: nothing happens.
  void story_events(int32_t cls) {
    if (!story_running() || event_up_) return;
    ev_ = start(cls);
    next_event();
  }

  void next_event() {
    mech3::GameEngine* ge = ge_;
    Shown s;
    if (!ge || !next(company_, ev_, rng_, &s)) return events_done();
    std::printf("mw1: story: event %#x (class %d)%s%s\n", s.event, ev_.cls, s.captions.empty() ? "" : ", a choice", s.restart ? ", a death" : "");
    ev_shown_ = s;
    ev_panel_ = EventPanel{};
    ev_panel_.planet = upper(company_.planet);
    ev_panel_.place = place_words(ev_.cls);
    ev_panel_.scene = event_art(s.event);  // its own art: a full page, the art dimmed behind
    if (ev_panel_.scene.empty()) place_over(ev_.cls, ev_panel_);  // else over its screen's picture, at its foot
    set_pages(shown_pages(company_, s));
    if (s.restart) {
      // MW1 restarts the campaign here (PLAY AGAIN / QUIT); ours never does (FIXES.md item 33, Owen: "you cant die in
      // this one"): the page is shown and play goes on.
      std::printf("mw1: story: MW1 would restart here; play goes on (FIXES.md item 33)\n");
    } else {
      ev_panel_.captions = s.captions;
      if (s.event == kEventDarkWingAttack) ev_panel_.live = {kDarkWingBattle, true};  // ATTACK waits for its battle (Q5)
    }
    if (!raise_event(*ge)) return events_done();
    show_event();
  }

  void show_event() {
    if (event_state_) event_state_->show(event_view(ev_panel_));
  }

  // The class done: the panel off, the screen under it shown again (the funds may have changed), saved.
  void events_done() {
    mech3::GameEngine* ge = ge_;
    const bool was_up = event_up_;
    if (event_up_ && ge) ge->pop_state(1);
    event_up_ = false, event_state_ = nullptr, news_.clear();
    if (ev_.cls == kClassArrival) company_.story.arrived = false;  // MW1 clears it after the hub's events
    if (!was_up) return;
    save();
    show_hub(), show_bar(), show_market(), show_office();
    if (credits_next_) credits_next_ = false, play_credits();  // the victory's panel read: the credits (item 47)
  }

  // FIXES.md item 47: after the Dark Wing's victory, the credits (mw1_menu.h's CreditsState: MW1's, then ours) over
  // the victory's picture; then the hub, play going on. None without their layout or MW1's exe. `at`: seconds in
  // (development shots).
  void play_credits(float at = 0.0f) {
    mech3::GameEngine* ge = ge_;
    if (!ge || credits_up_ || !layout_there(kCreditsFile, kCreditsDialog)) return;
    std::vector<CreditPanel> roll = credits_roll();
    if (roll.empty()) {
      std::printf("mw1: no credits (MW1's exe doesn't read): play goes on\n");
      return;
    }
    const size_t ours = extra_credits().size();
    credits_ = ge->adopt_state([this, roll](zgame::Handle h) {
      return credits_state_ = new CreditsState(h, roll, kVictoryScene, [this] { credits_done(); });
    });
    if (!credits_) return;
    credits_state_->start_at(at);
    const std::vector<CreditLine> lines = credits_layout(roll);
    std::printf("mw1: the credits, %zu panels (%zu ours), %zu lines, %.0f s\n", roll.size(), ours, lines.size(), credits_length(lines));
    credits_up_ = true;
    ge->push_state(credits_, 0);
  }
  void credits_done() {
    if (!credits_up_) return;
    credits_up_ = false;
    if (ge_ && ge_->menu_state_of(credits_) == credits_state_) ge_->pop_state(1);
    std::printf("mw1: the credits over: play goes on\n");
    show_hub();
  }

  void press_event(int32_t id) {
    std::printf("mw1: event: %s\n", event_button_name(id));
    if (!event_up_) return;
    if (ev_panel_.news) return press_news(id);
    switch (id) {
      case kEventNext:
        if (!ev_panel_.last_page()) {
          ++ev_panel_.page;
          break;
        }
        if (ev_panel_.choosing()) return;  // a choice to make
        next_event();
        return;
      case kEventChoice1:
      case kEventChoice2:
      case kEventChoice3: {
        const int32_t i = id - kEventChoice1;
        if (!ev_panel_.choosing() || i >= static_cast<int32_t>(ev_panel_.captions.size())) return;
        if (static_cast<size_t>(i) < ev_panel_.live.size() && !ev_panel_.live[static_cast<size_t>(i)]) return;
        std::printf("mw1: story: chose %d of event %#x\n", i + 1, ev_shown_.event);
        if (ev_shown_.event == kEventDarkWingAttack && i == kAttackChoice && kDarkWingBattle) {
          events_done();
          return attack(kDarkWingRung);
        }
        choose(ev_, ev_shown_, i);
        next_event();
        return;
      }
      default: return;
    }
    show_event();
  }

  // FIXES.md item 47 (mw1_ladder.h): a story battle launched, its rung's force on a map of the world's climate that
  // holds it; no office, no pay. 0x72's ATTACK (MW1's MwPrepareEventSpecialCombatContinuation, ATTACK the first
  // caption): the Dark Wing's, rung 6.
  static constexpr int32_t kAttackChoice = 0;
  void attack(int32_t r) {
    if (!ge_ || pending_) return;
    if (contract_.accepted) std::printf("mw1: story battle over a taken contract (%s): it gives way\n", objective_words(contract_).c_str());
    contract_ = story_battle(company_, r, rng_);
    offered_ = false;
    std::printf("mw1: story battle %d: %zu enemies (%d t, aim x%g) on %sia%d (%s)\n", contract_.rung, contract_.enemies.size(),
                rung_tons(contract_.enemies), rung(contract_.rung).aim, contract_.world.c_str(), contract_.ia,
                climate_name(contract_.climate));
    save();
    press(kHubDeploy);
  }
  // After the Dark Wing's defeat (MW1's darkWingVictory): the name cleared (won, 0x72 and the arrest 0x75 seen), its
  // victory text from the exe on the panel over its picture, then the hub (MW1's CONTINUE; its QUIT dropped: play
  // goes on, as item 33).
  void dark_wing_won() {
    company_.story.dark_wing_won = true;
    company_.story.seen[static_cast<size_t>(kEventDarkWingAttack)] = 1;
    company_.story.seen[static_cast<size_t>(kEventPortArrest)] = 1;
    save();
    std::printf("mw1: story: the Dark Wing defeated: the name cleared\n");
    const std::string text = dark_wing_victory();
    if (!ge_ || !story_running() || event_up_ || text.empty()) return;
    ev_ = start(kClassArrival);
    ev_.done = true;
    ev_shown_ = Shown{};
    ev_panel_ = EventPanel{};
    ev_panel_.planet = upper(company_.planet), ev_panel_.place = "VICTORY";
    ev_panel_.scene = kVictoryScene;
    std::vector<Page> pages;
    Page pg;
    for (const std::string& l : wrap({text}, kMw1Columns)) {
      if (pg.lines.size() == kVictoryPageLines) pages.push_back(pg), pg = Page{};
      pg.lines.push_back(l);
    }
    if (!pg.lines.empty()) pages.push_back(pg);
    set_pages(pages);
    if (raise_event(*ge_)) credits_next_ = true, show_event();
  }
  static constexpr const char* kVictoryScene = "mw1_story_honor_restored";
  static constexpr size_t kVictoryPageLines = 12;

  // The plaque's second line: where the event fires.
  static std::string place_words(int32_t cls) {
    switch (cls) {
      case kClassBarEntry:
      case kClassBarDrink: return "THE BAR";
      case kClassNews: return "NEWS";
      case kClassMechComplex: return "THE MECH COMPLEX";
      case kClassOffice: return "THE HOUSE OFFICE";
      default: return "ARRIVAL";
    }
  }

  // The Bar's NEWS on the panel: each story live now (marked seen, as MW1's browsing marks it), PREV and NEXT through
  // their pages, BACK to the Bar. None: a line saying so.
  void open_news() {
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    news_ = live_news(company_, rng_);
    ev_ = start(kClassNews);
    ev_.done = true;
    std::printf("mw1: story: the news, %zu stories\n", news_.size());
    ev_panel_ = EventPanel{};
    ev_panel_.planet = upper(company_.planet), ev_panel_.place = place_words(kClassNews), ev_panel_.news = true;
    place_over(kClassNews, ev_panel_);  // the Bar's
    ev_panel_.stories = static_cast<int32_t>(news_.size());
    news_story(0, false);
    if (!raise_event(*ge)) return;
    show_event();
  }

  // Story `i` of the news on the panel, at its first page (its last: `at_end`).
  void news_story(int32_t i, bool at_end) {
    ev_panel_.story = i;
    if (i < static_cast<int32_t>(news_.size())) set_pages(shown_pages(company_, news_[static_cast<size_t>(i)]));
    else set_pages({{{news_.empty() ? "Nothing on the news." : ""}, false}});
    ev_panel_.page = at_end ? ev_panel_.pages.size() - 1 : 0;
  }

  void press_news(int32_t id) {
    switch (id) {
      case kEventNext:
        if (!ev_panel_.last_page()) ++ev_panel_.page;
        else if (ev_panel_.story + 1 < ev_panel_.stories) news_story(ev_panel_.story + 1, false);
        else return events_done();  // Escape at the end: back to the Bar
        break;
      case kEventPrev:
        if (ev_panel_.page > 0) --ev_panel_.page;
        else if (ev_panel_.story > 0) news_story(ev_panel_.story - 1, true);
        break;
      case kEventBack: return events_done();
      default: return;
    }
    show_event();
  }

  // The pending mission settled (CONTINUE on the results screen; OK at the hub without its layout): the pay, the
  // salvage on a win, the experience, casualties and repairs on a counted mission, the time and the world moved on.
  void settle_pending() {
    const Settlement s = settle(company_, contract_, pending_->outcome, pending_->wrecks, rng_);
    // The parts are kept only on a win (VISION.md, the endings): failed or quit, the mission's salvage is dropped.
    if (pending_->outcome == Outcome::kSuccess && pending_->salvage) company_.stores->add_all(*pending_->salvage);
    // A counted mission (won, or failed and accepted; a retreat isn't, nor a replayed failure): the lancemates'
    // experience, then their casualties (mw1_pilots.h).
    // Then the repairs (mw1_repairs.h): the bill, the commander's loss.
    if (pending_->outcome != Outcome::kRetreat) {
      after_mission(company_, pending_->lance);
      const Repairs r = settle_repairs(company_, as_bills(pending_->bills));
      for (const std::string& l : r.lines) company_.notices.push_back(l);
      std::printf("mw1: repairs: billed %s, paid %s, forgiven %s\n", thousands(r.billed).c_str(), thousands(r.paid).c_str(),
                  thousands(r.forgiven).c_str());
    }
    std::printf("mw1: settled: %s, %zu wrecks, salvage %s, fee %s, payout %d%%, funds %s, %d items in the stores\n",
                ending_word(pending_->outcome), pending_->wrecks.size(), thousands(s.salvage).c_str(), thousands(s.fee).c_str(),
                s.payout_percent, thousands(company_.funds).c_str(), company_.stores->count());
    const bool story_won = contract_.rung == kDarkWingRung && pending_->outcome == Outcome::kSuccess;
    const int32_t rung_fought = contract_.rung;
    pending_.reset();
    // The mission's time (STARTUP 350: the return, the world's weights, the deployment), then the refresh the
    // original makes on every return (STARTUP 081B:378). A story battle (MW1's Dark Wing): won, none (MW1 goes
    // straight to its victory); else SPECIAL_ASSAULT_DEPLOYMENT_UNITS, then the refresh.
    if (!story_won) {
      advance_time(company_, rung_fought ? kStoryBattleUnits : mission_units(company_), rng_);
      refresh_world(company_, rng_);
    }
    clear_contract();  // fought: the next from the office
    if (story_won) dark_wing_won();
  }
  static constexpr int32_t kStoryBattleUnits = 0x1C;  // SPECIAL_ASSAULT_DEPLOYMENT_UNITS

  // A failed contract played again, everything as it was at its first launch (VISION.md, the endings: REPLAY on a
  // failed mission's results); its advance stays paid, nothing is settled. The failure stays pending until the replay
  // launches (an ABORT on its way in brings the results back).
  void replay(mech3::GameEngine& ge) {
    if (pending_->launch_stores) company_.stores->assign(*pending_->launch_stores);
    company_.loadout = pending_->launch_loadout;
    launch_stores_ = pending_->launch_stores, launch_loadout_ = pending_->launch_loadout;
    deploy(ge);
  }
  void show_hub() {
    if (!hub_state_) return;
    HubView v;
    v.scenery = scenery_image(company_.planet_number);
    v.office = "mw1_tile_house_" + lower(kHouses[company_.planet_house].name);
    v.planet = upper(company_.planet);
    v.house = "HOUSE " + upper(kHouses[company_.planet_house].name);
    v.office_open = contracts_here(company_);
    v.date = date_words(company_.date);
    // The day of the month (a month is MW1's 60 units: two to a day) and the wages' day, the month's turn.
    const int32_t day = company_.date.units / 2 + 1, days = kUnitsPerMonth / 2;
    v.day = "DAY " + std::to_string(day) + " \xB7 PAYDAY IN " + std::to_string(days - day + 1);
    // The company's strip (Owen on item 16: the stores and the mechs are the COMPANY's and the Mech Complex's).
    v.labels = {"C-BILLS", "PAYROLL", "KILLS"};
    v.values = {thousands(company_.funds), company_.lance.empty() ? "NONE" : "-" + thousands(monthly_wages(company_)) + "/MO",
                std::to_string(company_.kills)};
    // The box only with something to say (FIXES.md items 16, 42, MW1's notifications box): AFTER ACTION, or the
    // company's messages since (payday, promotions, casualties, the news, a contract taken), until OK.
    v.panel = hub_view_panel();
    if (pending_ && !results_up_) {  // without the results screen's layout: the old AFTER ACTION panel
      v.title = "AFTER ACTION", v.after_action = true;
      v.replay = pending_->outcome == Outcome::kFailure;
    } else if (!company_.notices.empty()) {
      v.title = "MESSAGES", v.message = true;
    }
    if (hub_page_ >= hub_pages(v.panel)) hub_page_ = 0;
    v.page = hub_page_;
    v.contract = contract_.accepted && !pending_;
    v.status = pending_ ? "The mission's back: OK settles it." : hub_hover_ ? tile_words(hub_hover_) : hub_status();
    hub_state_->show(v);
  }

  // The hub's box's lines: AFTER ACTION's and the messages after it, or the messages; none: no box.
  std::vector<std::string> hub_view_panel() const {
    std::vector<std::string> p;
    if (pending_ && !results_up_) {
      p = after_action_lines();
      if (!company_.notices.empty()) p.push_back("");
    }
    for (const std::string& n : company_.notices) p.push_back(n);
    return p;
  }

  // The status line with nothing hovered: the contract taken, or why there's none here, or what to do. One line of
  // the status font: about 56 characters fit 4:3's.
  std::string hub_status() const {
    const std::string house = kHouses[company_.planet_house].name;
    if (contract_.accepted)
      return "Contract: " + objective_words(contract_) + " for " + kHouses[contract_.house].name + ". DEPLOY.";
    // Stranded (nothing to earn, no fare out): its own words, why and what to do (the STARPORT has the rest).
    const std::vector<std::string> stranded = stranded_lines(company_);
    if (!stranded.empty()) return "Stranded: no fare out, no contracts. See the STARPORT.";
    return !contracts_here(company_) ? "No House office here: TRAVEL to a world with one."
           : refuses(company_, company_.planet_house) ? "House " + house + " won't deal with you: TRAVEL on."
           : !earning_here(company_) ? "Negotiations here are over: TRAVEL on."
                                     : "Pick a service.";
  }
  // A tile's words on the status line while the mouse is over it (MW1's hub says them as the cursor moves).
  std::string tile_words(int32_t id) const {
    const std::string house = kHouses[company_.planet_house].name;
    switch (id) {
      case kHubCompany: return "COMPANY: your lance, its pilots and their mechs.";
      case kHubMarket: return "MECH COMPLEX: buy, sell and refit mechs.";
      case kHubOffice:
        return earning_here(company_) ? "HOUSE OFFICE: ask House " + house + " for a contract."
                                      : "HOUSE OFFICE: House " + house + " has nothing for you.";
      case kHubDeploy: return "DEPLOY: to the contract's battle.";
      case kHubTravel: return "STARPORT: travel to another world.";
      case kHubBar: return "BAR: a drink, the news, pilots looking for work.";
      case kHubOptions: return "OPTIONS: the game's settings and the main menu.";
      case kHubOk: return pending_ ? "OK settles the mission." : "OK clears the messages.";
      case kHubReplay: return "REPLAY: fight the mission again.";
      default: return hub_status();
    }
  }
  void hub_hovered(int32_t id) {
    if (id == hub_hover_) return;
    hub_hover_ = id;
    show_hub();
  }

  // The contract on offer, line by line (with the planet and the funds first for the message box, which has no plaque).
  std::vector<std::string> offer_lines(bool with_company = false, bool with_terms = true, const Contract* of = nullptr) const {
    const Contract& k = of ? *of : contract_;
    // The enemy as the player meets it (FIXES.md item 20): MW1's count by weight class, then the Pirate's Moon mechs
    // that stand in for MW1's kinds, by name (MW1's own kind names stay in the logs).
    // The key facts highlighted (item 20's colour, mw1_screen.h): the employer, the target, the objective, the map,
    // the enemy's count and class, the pay. A message box gets the lines plain.
    std::string enemies;
    for (int32_t w = 3; w >= 1; --w) {
      int32_t n = 0;
      for (int32_t e : k.enemies) n += kMechKinds[e].weight == w;
      if (n) enemies += std::string(enemies.empty() ? "" : ", ") + std::to_string(n) + (w == 3 ? " Heavy" : w == 2 ? " Medium" : " Light");
    }
    std::vector<std::pair<std::string, int32_t>> named;
    for (int32_t e : k.enemies) {
      const std::string t = mech_title(kMechKinds[e].pm_mech);
      auto it = std::find_if(named.begin(), named.end(), [&](const auto& p) { return p.first == t; });
      if (it == named.end()) named.push_back({t, 1});
      else ++it->second;
    }
    std::string names;
    for (const auto& [t, n] : named) names += std::string(names.empty() ? "" : ", ") + (n > 1 ? std::to_string(n) + " " : "") + t;
    enemies = highlighted(enemies);
    if (!names.empty()) enemies += " (" + names + ")";
    std::vector<std::string> l;
    if (with_company)
      l = {company_.planet + ", House " + kHouses[company_.planet_house].name, "Funds: " + thousands(company_.funds) + " C-bills", ""};
    l.push_back("Employer: " + highlighted("House " + std::string(kHouses[k.house].name)));
    if (k.target < kHouseCount) l.push_back("Against: " + highlighted("House " + std::string(kHouses[k.target].name)));
    const std::string title = mission_title(k.code);
    if (!title.empty()) l.push_back(title_case_name(title.c_str()));
    if (k.mission_planet >= 0)
      l.push_back("On " + title_case_name(kPlanets[k.mission_planet].name) + " (" + highlighted(climate_name(k.climate)) + ")");
    l.push_back("Objective: " + highlighted(objective_words(k)));
    // FIXES.md item 44 (c): the map's own climate, so a battle on another climate's map is no surprise ("desert, no
    // ice map yet" on an ice world while the table has none for it).
    std::string map = "Map: " + highlighted(map_words(ge_, k.world, k.ia));
    if (const int32_t c = map_climate(map_table(), k.world, k.ia); c >= 0) {
      map += " (" + highlighted(climate_name(c));
      // Item 46: a map of another climate is the planet's pool's choice; said only while the table has no map of
      // the planet's own climate at all (the ice's until item 44).
      const bool none_of_it = std::none_of(map_table().begin(), map_table().end(), [&](const MapRow& r) { return r.climate == k.climate; });
      if (k.mission_planet >= 0 && c != k.climate && none_of_it) map += std::string(", no ") + climate_name(k.climate) + " map yet";
      map += ")";
    }
    l.push_back(map);
    l.push_back("Enemy: " + enemies);
    if (!with_terms) return l;
    l.push_back("Payment: " + highlighted(thousands(int64_t{k.payment} * 1000)) + " C-bills");
    l.push_back("Advance " + std::to_string(k.advance_percent) + "%, salvage " + std::to_string(k.salvage_percent) + "%");
    return l;
  }

  // The mission back, as OK would settle it: the settlement on copies, with the same rolls OK will make.
  std::vector<std::string> after_action_lines() const {
    if (!pending_) return {};
    Company c = company_;
    Contract k = contract_;
    Rng r = rng_;
    const Settlement s = settle(c, k, pending_->outcome, pending_->wrecks, r);
    // The repairs as OK will settle them (a counted mission: won, or failed and accepted).
    Repairs rep;
    if (pending_->outcome != Outcome::kRetreat) {
      after_mission(c, pending_->lance);
      rep = settle_repairs(c, as_bills(pending_->bills));
    }
    std::vector<std::string> l = {pending_->outcome == Outcome::kSuccess   ? "Success"
                                  : pending_->outcome == Outcome::kFailure ? "Failure"
                                                                          : "Retreat: the contract broken",
                                  "Enemy mechs disabled: " + std::to_string(pending_->wrecks.size()),
                                  "Salvage: " + thousands(s.salvage) + " C-bills",
                                  "Payment: " + thousands(s.fee) + " C-bills (" + std::to_string(s.payout_percent) + "%)",
                                  };
    for (const std::string& x : rep.lines) l.push_back(x);
    l.push_back("Funds: " + thousands(c.funds) + " C-bills");
    for (const LanceFate& f : pending_->lance)
      l.push_back(pilot_title(f.id) + (!f.destroyed                                ? ": flew"
                                       : pending_->outcome == Outcome::kRetreat ? ": mech destroyed"
                                       : pilot_missing(f)                       ? ": MISSING"
                                                                                : ": KILLED"));
    for (int32_t id : pending_->stayed)
      l.push_back(pilot_title(id) + " stayed behind: no place on this map (" + std::to_string(pending_->lance.size()) + " allied)");
    if (pending_->outcome == Outcome::kFailure) l.push_back(""), l.push_back("REPLAY: play it again as it was");
    return l;
  }

  // A new contract on offer (the test overrides on it), saved.
  void new_offer() {
    offered_ = earning_here(company_);
    if (!offered_) {
      save();
      std::printf("mw1: %s on %s: no contracts\n",
                  !contracts_here(company_)                  ? "no House office"
                  : refuses(company_, company_.planet_house) ? "the House refuses the company"
                                                             : "negotiations broken off",
                  company_.planet.c_str());
      return;
    }
    contract_ = generate_contract(company_, 0, rng_);
    // The test overrides: a world's battle (destroy all there), or a row of the map table whatever was rolled.
    if (!world_.empty()) {
      contract_.world = world_, contract_.ia = world_ia_, contract_.objective = Objective::kDestroyAll;
      contract_.structure.clear(), contract_.structure_words.clear();
    }
    if (!enemies_.empty()) contract_.enemies = enemies_;  // a test's force (the ladder's map survey)
    if (row_set_) {
      contract_.world = row_.world, contract_.ia = row_.ia, contract_.objective = objective_for(row_.kind);
      contract_.structure = row_.node, contract_.structure_words = row_.words;
      // MW1_REACH's row: the map table's own (its world, archive and words) when it has a reach row on that node.
      for (const MapRow& r : map_table())
        if (row_.words == "the " + row_.node && r.kind == RowKind::kReach && r.node == row_.node)
          contract_.world = r.world, contract_.ia = r.ia, contract_.structure_words = r.words;
    }
    company_.last_map_world = contract_.world, company_.last_map_ia = contract_.ia;  // item 46: not drawn next time
    save();
    std::printf("mw1: offered: %s on %sia%d (MW1's %s, code %d, %s), %d thousand, %zu enemies; climate %s, the map's %s\n",
                objective_words(contract_).c_str(), contract_.world.c_str(), contract_.ia, category_name(contract_.category),
                contract_.code, mw1_objective_name(objective_of_code(contract_.code)), contract_.payment, contract_.enemies.size(),
                climate_name(contract_.climate), climate_name(map_climate(map_table(), contract_.world, contract_.ia)));
  }

  // FIXES.md item 18: DEPLOY goes straight into the mission. PM's briefing (the Load state's screen) is built and
  // loads the mission, but never drawn; MW1's loading screen is up instead, and the mission launches once loaded.
  bool skip_briefing() const override { return true; }
  // FIXES.md item 19: PM's debriefing never shown either; the mission ends on MW1's results screen (enter_hub).
  bool skip_debriefing() const override { return true; }
  LoadingNext loading_frame(mech3::Engine& e, float dt, bool loaded) override {
    if (!loading_ && layout_there(kLoadFile, kLoadDialog)) {
      loading_ = std::make_unique<screens::Mw1Screen>(e, mech3::states().load.addr(), kLoadFile, kLoadDialog,
                                                     std::vector<screens::ScreenButton>{}, nullptr);
      const LoadingPanel panel = loading_panel();
      loading_->show(loading_view(panel));
      ++seen_.loads, seen_.lines = loading_->shown(), seen_.scene = panel.scene, seen_.enemy = panel.enemy;
      // Behind it, the pictures the mission comes back to: the results' and the hub's backdrop (mw1_art.h).
      art::Store::get().preload({scenery_image(company_.planet_number)});
      std::printf("mw1: loading %sia%d: %s\n", contract_.world.c_str(), contract_.ia, objective_words(contract_).c_str());
    }
    if (loading_) loading_->update(dt);
    if (!loaded || hold_launch_ || hold_for_shot_) return LoadingNext::kWait;
    // A load MW1 didn't ask for (PM's own next round, its debriefing's CONTINUE): never played, back to the hub.
    if (!deployed_ || launched_) return LoadingNext::kAbort;
    return LoadingNext::kLaunch;
  }
  // The loading screen's facts: the battle's world (else the company's) and its scenery, the employer, the contract
  // as the office's offer said it (less what the plaque and the title say).
  LoadingPanel loading_panel() const {
    const char* screen = std::getenv("MW1_SCREEN");
    const Contract k = screen && std::string(screen) == "loading_long" ? longest_contract() : contract_;
    LoadingPanel p;
    const int32_t number = k.mission_planet >= 0 ? kPlanets[k.mission_planet].number : company_.planet_number;
    p.planet = upper(k.mission_planet >= 0 ? title_case_name(kPlanets[k.mission_planet].name) : company_.planet);
    p.employer = "HOUSE " + upper(kHouses[k.house].name);
    p.scene = scenery_image(number);
    const std::string title = mission_title(k.code);
    p.title = title.empty() ? "CONTRACT" : upper(title);
    const std::string title_line = title.empty() ? "" : title_case_name(title.c_str());
    for (const std::string& l : offer_lines(false, false, &k))
      if (l.rfind("Employer: ", 0) != 0 && l != title_line) p.lines.push_back(l);
    // The main enemy beside the terminal: the heaviest kind's PM mech (the first of the heaviest), its render once
    // the first run has made them, else its amber icon (both the Company screen's 96x128 slot).
    int32_t main = -1;
    for (int32_t e : k.enemies)
      if (e >= 0 && e < kKindCount && (main < 0 || kMechKinds[e].tons > kMechKinds[main].tons)) main = e;
    if (main >= 0) {
      const int32_t pm = kMechKinds[main].pm_mech;
      p.enemy = renders_ready() ? render_image(pm, RenderUse::kSlot) : mech_icon_image(pm);
    }
    return p;
  }

  // MW1_SCREEN=loading_long (mw1screenfit's fixed case for the loading terminal, the lead 2026-11-18): the taken
  // contract made the longest the terminal can be asked to say: the longest-named world, against the longest-named
  // other House, one of every kind, on the table's row whose map or objective line is the longest.
  Contract longest_contract() const {
    Contract k = contract_;
    for (int32_t i = 0; i < kPlanetCount; ++i)
      if (k.mission_planet < 0 || std::strlen(kPlanets[i].name) > std::strlen(kPlanets[k.mission_planet].name))
        k.mission_planet = i, k.climate = kPlanets[i].landscape;
    k.target = kHouseCount;
    for (int32_t h = 0; h < kHouseCount; ++h)
      if (h != k.house && (k.target == kHouseCount || std::strlen(kHouses[h].name) > std::strlen(kHouses[k.target].name))) k.target = h;
    k.enemies.clear();
    for (int32_t e = 0; e < kKindCount; ++e) k.enemies.push_back(e);
    size_t longest = 0;
    Contract best = k;
    for (const MapRow& r : map_table()) {
      Contract t = k;
      t.world = r.world, t.ia = r.ia, t.objective = objective_for(r.kind);
      t.structure = r.kind == RowKind::kAny ? "" : r.node, t.structure_words = r.kind == RowKind::kAny ? "" : r.words;
      for (const std::string& l : offer_lines(false, false, &t))  // the row's own lines (the rest are the same for each)
        if ((l.rfind("Map: ", 0) == 0 || l.rfind("Objective: ", 0) == 0) && plain_line(l).size() > longest)
          longest = plain_line(l).size(), best = t;
    }
    std::printf("mw1: loading_long: %s ia%d, its map or objective line %zu letters\n", best.world.c_str(), best.ia, longest);
    return best;
  }

  // The launch: the lab's choices become the company's (the stores and the loadout, from the briefing's copy). The
  // contract is accepted and its advance paid at its first launch only; a later launch of the same contract (a
  // replay; none reaches here yet, an IA REPLAY ends the mission as a failure) puts back what the first launch
  // left. Each launch starts the mission's salvage and wrecks afresh.
  void mission_launched(mech3::Engine&) override {
    loading_.reset();  // the loading screen's job done
    // FIXES.md item 43: the objective's nav point the selected one, so the HUD's nav and its arrow lead there.
    if (nav_point_) {
      float at[3];
      structure_point(mech3::mission_objective_hooks(), nav_node_, at);
      mech3::program_nav_move(nav_point_, at);
      std::printf("mw1: the objective's nav point selected: %s\n", mech3::program_nav_select(nav_point_) ? "yes" : "no");
    }
    if (const mech3::BriefingScreen* b = mech3::briefing_screen())  // launched from it: still there (tests)
      seen_.briefing_frames += b->frames_drawn, seen_.briefing_script = seen_.briefing_script || b->script.playing;
    mech3::Pilot* p = mech3::current_pilot();
    mech3::ActionGame* g = p ? p->action() : nullptr;
    if (!deployed_ || !g) return;
    if (!launched_ && !pending_) {  // the contract's first launch (taken at the office; an ABORT before brings it here again)
      company_.stores->assign(g->store);
      company_.loadout = g->slot.config ? copy_config(*g->slot.config) : nullptr;
      launch_stores_ = std::make_shared<mech3::Store>();
      launch_stores_->assign(*company_.stores);
      launch_loadout_ = company_.loadout;
      launched_ = true;
    } else {
      // A later launch of the same contract (a replay): what its first launch left.
      if (launch_stores_) company_.stores->assign(*launch_stores_);
      company_.loadout = launch_loadout_;
      launched_ = true;
      pending_.reset();  // the failure replayed: not settled, not shown again
    }
    wrecks_.clear(), killers_.clear(), armor_left_.clear(), structure_left_.clear(), seconds_ = -1;
    salvage_ = std::make_shared<mech3::Store>();
    ended_ = failed_ = ending_latched_ = false;
    std::printf("mw1: launched: %d items in the stores, the loadout %s\n", company_.stores->count(),
                company_.loadout ? "the lab's" : "stock");
  }

  // Instant Action's end (IA Dummy). Heard only when the mission is left without its own end (the pause screen's
  // QUIT): a mission that ended was latched the frame it did. PM's own way out of an ended mission (its debriefing's
  // CONTINUE, the next round's briefing, its EXIT) reaches IA Dummy only after the briefing has loaded the mission
  // again, its end flag cleared: sampled there, a win was a Retreat (Owen's win on mw3c2ia3, 2026-11-17).
  void mission_ending(mech3::Engine& e) override {
    if (!ending_latched_) latch_ending(e);
  }
  // The mission's outcome (and the lance's damage, for the repair bills) taken once per launch, never again until
  // the next launch.
  void latch_ending(mech3::Engine& e) {
    ending_latched_ = true;
    ended_ = e.mission_ended(), failed_ = e.mission_failed();
    if (deployed_) collect_bills();
    // The results screen's (FIXES.md item 19): the mission's time, the commander's mech by location.
    seconds_ = static_cast<int32_t>(mech3::missions().time);
    if (void* player = mech3::mech_mission_player()) location_left(bars_of(player), &armor_left_, &structure_left_);
    else armor_left_.clear(), structure_left_.clear();
    std::printf("mw1: the mission's end: %s\n", ended_ ? (failed_ ? "failed" : "won") : "left before its end");
  }

  // Each enemy mech the moment it dies (whoever killed it: MW1 pays for every disabled enemy): its wreck for the
  // pay, and its parts into the mission's salvage, never the mech itself (VISION: salvage gives parts), the
  // company's only if the mission is won.
  void mech_killed(void* victim, void* killer) override {
    if (deployed_ && victim == mech3::mech_mission_player()) commander_destroyed_ = true;
    if (!deployed_ || !salvage_ || victim == mech3::mech_mission_player()) return;
    if (const int32_t id = lancemate_of(victim)) {
      for (LanceFate& f : fates_)
        if (f.id == id) f.destroyed = true;
      std::printf("mw1: lancemate %s's mech destroyed\n", pilot_title(id).c_str());
      return;
    }
    mech3::LaunchedMech d;
    if (!mech3::describe_mech(victim, &d)) return;
    int32_t kind = -1;
    for (int32_t k : contract_.enemies)
      if (kMechKinds[k].pm_mech == d.roster) kind = k;
    if (kind < 0) return;  // not one of the contract's
    wrecks_.push_back({kind, condition_of(victim), damage_of(bars_of(victim))});
    killers_.push_back(whose(killer));  // the results' "by whom" (FIXES.md item 19)
    mech3::program_salvage_mech(victim, *salvage_, false);
    std::printf("mw1: an enemy %s disabled (%d%% standing)\n", kMechKinds[kind].name, wrecks_.back().condition);
  }

  // Instant Action's launch offers each ally (program_flow.h): slot 1.. in deploy()'s order, flying_'s. The lancemate's
  // name and their Gunnery's aim (kAimErrorFactor on the record's aim error); they launched.
  // FIXES.md item 47: a story battle's enemies aim as its rung says (mw1_ladder.h); a contract's as their records.
  void enemy_launching(int32_t slot, mech3::ai::AiParams& params) override {
    if (!deployed_ || !contract_.rung) return;
    const float error = params.get<float>(0xb8) * rung(contract_.rung).aim;
    std::printf("mw1: story battle %d: enemy in slot %d, aim error %g\n", contract_.rung, slot, error);
    enemy_aims_.push_back(error);
    params.put<float>(0xb8, error);
  }
  // FIXES.md item 48: an enemy past the map's own places is made beside one of them, on clear ground (player.cpp).
  bool extra_enemy_places() override { return deployed_; }
  void ally_launching(int32_t slot, mech3::ai::AiParams& params) override {
    if (!deployed_ || slot < 1 || slot > static_cast<int32_t>(flying_.size())) return;
    const int32_t id = flying_[static_cast<size_t>(slot - 1)];
    mech3::set_ai_pilot_name(params, pilot_title(id).c_str());
    const float error = params.get<float>(0xb8) * kAimErrorFactor[std::clamp(gunnery(company_, id), 0, 3)];
    launches_.push_back({id, params.get<float>(0xb8), error});
    params.put<float>(0xb8, error);
    bool known = false;
    for (const LanceFate& f : fates_) known = known || f.id == id;
    if (!known) fates_.push_back({id, false});
    std::printf("mw1: lancemate %s launched in slot %d, aim error %g\n", pilot_title(id).c_str(), slot, error);
  }

  // ---- repairs (mw1_repairs.h) ----

  static Bars bars_of(void* mech) {
    Bars b;
    int32_t type = -1, armor[11] = {};
    if (!mech || !mech3::mech_lance_armor(mech, &type, armor, b.armor_left)) return b;
    for (int k = 0; k < 11; ++k) b.armor_points[k] = static_cast<float>(armor[k]);
    mech3::mech_lance_structure(mech, b.structure_points, b.structure_left);  // none read: armor alone
    return b;
  }
  // Whose a mission mech is: 0 the commander's, a lancemate's pilot id, -1 none of the company's.
  int32_t whose(void* mech) const {
    if (mech && mech == mech3::mech_mission_player()) return 0;
    const int32_t id = lancemate_of(mech);
    return id ? id : -1;
  }
  // A field base mended one of the company's mechs: on its bill (VISION: the same rate as the end's).
  void mech_repaired(void* mech, const float armor[11], const float structure[11]) override {
    if (!deployed_) return;
    const int32_t who = whose(mech);
    if (who < 0) return;
    const int32_t m = mended(bars_of(mech), armor, structure);
    mended_[who] += m;
    std::printf("mw1: a field base mended %s (%d of %d)\n", who ? pilot_title(who).c_str() : "the commander", m, kWhole);
  }
  // The mission's end: each company mech's damage (with what field bases mended) and whether it was destroyed.
  void collect_bills() {
    bills_.clear();
    void* player = mech3::mech_mission_player();
    bills_.push_back({0, company_.mech, std::min(kWhole, damage_of(bars_of(player)) + mended_[0]), commander_destroyed_});
    int32_t n = 0;
    const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
    for (const LanceFate& f : fates_) {
      void* m = nullptr;
      for (int32_t k = 0; k < n && !m; ++k)
        if (lancemate_of(ai[k].mech) == f.id) m = ai[k].mech;
      int32_t mech = -1;
      for (size_t i = 0; i < company_.lance.size(); ++i)
        if (company_.lance[i].id == f.id)
          if (const int32_t h = mech_of(company_, static_cast<int32_t>(i)); h >= 0) mech = company_.hangar[static_cast<size_t>(h)].mech;
      if (mech < 0) continue;
      bills_.push_back({f.id, mech, std::min(kWhole, (m ? damage_of(bars_of(m)) : 0) + mended_[f.id]), f.destroyed});
    }
  }
  static std::vector<MechBill> as_bills(const std::vector<AfterAction::Bill>& v) {
    std::vector<MechBill> out;
    for (const AfterAction::Bill& b : v) out.push_back({b.who, b.mech, b.damage, b.destroyed});
    return out;
  }

  // The lancemate flying an AI mech: its brain's pilot is the name ally_launching gave.
  int32_t lancemate_of(void* mech) const {
    int32_t n = 0;
    const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
    for (int32_t k = 0; k < n; ++k) {
      if (ai[k].mech != mech) continue;
      auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[k].controls));
      if (!b || !b->params.pilot) return 0;
      for (const LanceFate& f : fates_)
        if (pilot_title(f.id) == b->params.pilot) return f.id;
    }
    return 0;
  }

  const Company* company() const { return company_.stores ? &company_ : nullptr; }
  const std::vector<AllyLaunch>& launches() const { return launches_; }
  const std::vector<float>& enemy_aims() const { return enemy_aims_; }
  const AfterAction* pending() const { return pending_.get(); }
  HubState* hub_state() const { return hub_state_; }
  MarketState* market_state() const { return market_state_; }
  const PilotChooser* lab_pilot() const { return lab_pilot_.attached() ? &lab_pilot_ : nullptr; }
  const LabFunds* lab_funds() const { return lab_funds_.attached() ? &lab_funds_ : nullptr; }
  BarState* bar_state() const { return bar_state_; }
  OfficeState* office_state() const { return office_state_; }
  HubOptionsState* hub_options_state() const { return hub_options_state_; }
  EventState* event_state() const { return event_up_ ? event_state_ : nullptr; }
  const LoadingSeen& loading_seen() const { return seen_; }
  ResultsState* results_state() const { return results_state_; }
  const ResultsPanel& results_shown() const { return seen_results_; }
  void set_hold_launch(bool on) { hold_launch_ = on; }
  void set_story(bool on) { story_on_ = on; }
  void offer_again() {  // tests: the offer drawn again after a change to the company, the hub shown
    new_offer();
    show_hub();
  }
  TravelState* travel_state() const { return travel_state_; }
  Company* company_mut() { return company_.stores ? &company_ : nullptr; }
  void market_changed() { show_market(); }
  void set_save(const std::string& path, bool fresh) { save_path_ = path, fresh_ = fresh; }
  void set_start(Start s) { start_ = s; }
  MenuState* menu_state() const { return menu_state_; }
  IntroState* intro_state() const { return intro_state_; }
  CreditsState* credits_state() const { return credits_up_ ? credits_state_ : nullptr; }
  void set_world(const std::string& world, int32_t ia) { world_ = world, world_ia_ = ia; }
  const Request* office_ask() const { return &ochoice_.ask; }
  const Contract* contract() const { return company_.stores && (offered_ || contract_.accepted || pending_) ? &contract_ : nullptr; }

  void set_row(const MapRow& r) { row_ = r, row_set_ = true; }
  void set_enemies(const std::vector<int32_t>& k) { enemies_ = k; }
  // What the mission's objectives became (tests, the map survey): "" none of ours (destroy all), else "reach",
  // "attack" or "defend"; `objective_fell_back_` when the contract's structure wasn't on the map.
  const std::string& objective_built() const { return objective_built_; }
  bool objective_fell_back() const { return objective_fell_back_; }

  // ---- ObjectivesExtension ----
  // The contract sets the goal (VISION, "Combat: maps"), the map's own primary replaced:
  //   reach: reaching the structure's node alone, as the data's reach_area builds one;
  //   attack: destroying the structure alone (destroy_entity), the kills still salvage;
  //   defend: Instant Action's own "destroy the enemies", the structure's destruction a fail (the first met fails
  //   the mission, whatever comes after).
  // A structure not on the map is never built in (destroy_entity would count it destroyed at once): the enemies to
  // destroy instead, said. The map survey (tests) is the gate; this is the net under it.
  void block_read(zrdr::Node*, mech3::Objectives& o) override {
    own_wins_ = false, objective_built_.clear(), objective_fell_back_ = false, nav_point_ = nullptr, nav_label_.clear();
    if (!deployed_ || contract_.objective == Objective::kDestroyAll) return;
    mech3::ObjectiveHooks& h = mech3::mission_objective_hooks();
    const char* name = contract_.structure.c_str();
    const std::string shown = contract_.structure_words.empty() ? contract_.structure : contract_.structure_words;
    auto fall_back = [&] {
      objective_fell_back_ = true;
      std::printf("mw1: no %s on %sia%d: destroy all instead\n", name, contract_.world.c_str(), contract_.ia);
    };
    switch (contract_.objective) {
      case Objective::kReachStructure: {
        const zgame::Handle node = h.node_find(name);
        if (!node) return fall_back();
        float point[3];
        h.default_point(point);
        const std::string desc = "Reach " + shown;
        auto r = std::make_unique<mech3::ReachAreaObjective>(desc.c_str(), node, point, kReachRadius);
        r->nav = place_nav(h, node);  // item 43: its place a nav point (the check then reads the point, at the node)
        o.primary = std::make_unique<mech3::ObjectiveGroup>(h.message(0x16d));
        o.primary->items.push_back(std::move(r));
        own_wins_ = true, objective_built_ = "reach";
        break;
      }
      case Objective::kAttackStructure: {
        if (!target_listed(contract_.structure)) return fall_back();
        const std::string desc = "Destroy " + shown;
        auto d = std::make_unique<mech3::DestroyObjective>(desc.c_str(), name, h.entity_find(name));
        d->nav = place_nav(h, h.node_find(name));
        o.primary = std::make_unique<mech3::ObjectiveGroup>(h.message(0x16d));
        o.primary->items.push_back(std::move(d));
        own_wins_ = true, objective_built_ = "attack";
        break;
      }
      case Objective::kDefendStructure: {
        if (!target_listed(contract_.structure)) return fall_back();
        const std::string desc = "Failed to protect " + shown;
        auto d = std::make_unique<mech3::DestroyObjective>(desc.c_str(), name, h.entity_find(name));
        d->nav = place_nav(h, h.node_find(name));
        o.primary.reset();  // Instant Action's "destroy the enemies" alone (add_enemy_group)
        if (!o.fail) o.fail = std::make_unique<mech3::ObjectiveGroup>(nullptr);
        o.fail->items.push_back(std::move(d));
        objective_built_ = "defend";
        break;
      }
      case Objective::kDestroyAll: break;
    }
  }
  // FIXES.md item 43: the contract's place as a nav point, as MW3's objectives name theirs ("nav"), watched (on the
  // map, the HUD's list, the arrow): at the structure's node, labelled with its words ("Pump complex"); a point of
  // the map's own on it (kNavNear) serves instead. At the launch it is put where the node is then (a vehicle's is
  // placed after the objectives are read) and selected, so the arrow leads there from the start. 0 without the node
  // or the mission's waynet.
  // FIXES.md item 45: where a structure is. Its node's place; a node that is only a group (no place of its own: the
  // world's origin, as c3's land bridge xslid01) is where its parts are, their middle (each part with a place, two
  // levels down).
  void structure_point(mech3::ObjectiveHooks& h, zgame::Handle node, float out[3]) const {
    h.node_position(node, out);
    if (out[0] != 0.0f || out[1] != 0.0f || out[2] != 0.0f || !ge_) return;
    auto* n = static_cast<zcls::Node*>(ge_->object_of(node));
    float sum[3] = {};
    int32_t parts = 0;
    std::function<void(zcls::Node*, int)> visit = [&](zcls::Node* c, int depth) {
      if (!c || depth > 2) return;
      float w[3] = {};
      if (depth > 0 && c->data && c->cls() == 5) zcls::node_world_position(c, w);
      if (w[0] != 0.0f || w[1] != 0.0f || w[2] != 0.0f) {
        for (int i = 0; i < 3; ++i) sum[i] += w[i];
        ++parts;
      }
      for (int32_t i = 0; i < c->child_count(); ++i) visit(c->children[i], depth + 1);
    };
    visit(n, 0);
    if (parts)
      for (int i = 0; i < 3; ++i) out[i] = sum[i] / static_cast<float>(parts);
  }
  bool structure_point_now(float out[3]) const {
    mech3::ObjectiveHooks& h = mech3::mission_objective_hooks();
    const zgame::Handle node = contract_.structure.empty() ? 0 : h.node_find(contract_.structure.c_str());
    if (!node) return false;
    structure_point(h, node, out);
    return true;
  }
  zgame::Handle place_nav(mech3::ObjectiveHooks& h, zgame::Handle node) {
    nav_label_.clear(), nav_point_ = nullptr, nav_node_ = 0;
    if (!node) return 0;
    float at[3];
    structure_point(h, node, at);
    std::string label = contract_.structure_words.empty() ? contract_.structure : contract_.structure_words;
    if (label.rfind("the ", 0) == 0) label = label.substr(4);
    if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    void* p = mech3::program_nav_add(label.c_str(), at, kNavNear);
    if (!p) return 0;
    const zgame::Handle nav = ge_ ? ge_->handle_of(p) : 0;
    if (!nav) return 0;
    h.nav_show(nav, 1);
    nav_point_ = p, nav_node_ = node;
    nav_label_ = mech3::program_waypoint_name(p) ? mech3::program_waypoint_name(p) : label;
    std::printf("mw1: the objective's nav point: %s at %.0f, %.0f\n", nav_label_.c_str(), at[0], at[2]);
    return nav;
  }
  // Destroy all and defend: Instant Action's own "destroy the enemies"; reach and attack win alone.
  bool add_enemy_group(const mech3::Objectives&) override { return !deployed_ || !own_wins_; }

 private:
  // The first hub: the saved company (and its offer) when there is one, else a new company. A save that doesn't
  // read is set aside (mw1save.bad) and said, and the game goes on with a new company. True: the offer is the saved.
  bool start_company() {
    bool has_offer = false;
    if (!save_path_.empty() && !fresh_) {
      std::string why;
      auto pending = std::make_unique<AfterAction>();
      bool has_pending = false;
      switch (load_file(save_path_, company_, contract_, &has_offer, &why, pending.get(), &has_pending)) {
        case Loaded::kLoaded:
          if (!company_.market.rolled) refresh_world(company_, rng_);  // a version 1 save: no market yet
          else if (company_.recruits.empty()) refresh_recruits(company_, rng_);  // before version 4: no Bar yet
          if (!company_.story.planets[0]) roll_story_planets_for_old_save(company_, rng_);  // before version 8: no story yet
          if (has_pending) pending_ = std::move(pending), launch_stores_ = pending_->launch_stores, launch_loadout_ = pending_->launch_loadout;
          std::printf("mw1: the company loaded from %s: %s C-bills, %d items in the stores, %d kills%s\n", save_path_.c_str(),
                      thousands(company_.funds).c_str(), company_.stores->count(), company_.kills,
                      has_pending ? ", a mission's after action" : has_offer ? ", its offer" : "");
          offered_ = has_offer && !contract_.accepted;  // an offer up at the office; one taken shows at the hub
          return has_offer;
        case Loaded::kBad:
          std::printf("mw1: the save %s doesn't read (%s): kept as mw1save.bad; a new company\n", save_path_.c_str(), why.c_str());
          break;
        case Loaded::kNone: break;
      }
    }
    company_ = new_company(rng_);
    // MW1_START_WORLD=<name> (development: --shot pictures of another House's world, FIXES.md item 50's liaisons): the
    // new company on that world (a name's beginning, as the travel map's search).
    if (const char* w = std::getenv("MW1_START_WORLD"))
      if (const int32_t i = find_by_name(w); i >= 0) set_planet(company_, i);
    refresh_world(company_, rng_);
    std::printf("mw1: a new company on %s, %s C-bills\n", company_.planet.c_str(), thousands(company_.funds).c_str());
    offered_ = false, contract_ = Contract{};
    save();  // saved at once (nothing on offer until the office is asked)
    return false;
  }

  // After each settlement and at each new offer (both here, at the hub); never mid-mission.
  void save() {
    if (save_path_.empty()) return;
    std::string why;
    if (!save_file(save_path_, company_, offered_ || contract_.accepted || pending_ ? &contract_ : nullptr, &why, pending_.get())) std::printf("mw1: the company not saved: %s\n", why.c_str());
  }

  static std::shared_ptr<mech3::Config> copy_config(const mech3::Config& c) {
    return std::shared_ptr<mech3::Config>(mech3::pilot_hooks().config_copy(c));
  }

  // The company's own PM pilot (step 2), in mw1's write dir and never PM's; made once, the player never picks it.
  static mech3::Pilot* company_pilot() {
    mech3::Pilot* p = mech3::find_pilot(kPilotName);
    return p ? p : mech3::create_pilot(kPilotName);
  }

  // MW1_SCREEN=pause's and lab's driver, each frame shown (main_entered).
  // MW1_SCREEN=walk_bar, walk_office_lab (FIXES.md item 35's captures, with --shot-series): the hub's screens one
  // after another, a step every kWalkStep frames from the hub's first: walk_bar the Bar and back; walk_office_lab
  // the office (an offer asked for, ACCEPT, back at the hub), the Mech Complex and its MECH LAB. Each frame's state
  // named on a change (name_state; MW1_SCREEN=results too: a mission's return).
  static constexpr int kWalkStep = 30;
  // The state each frame presented (`f`, the engine's count) shows, named when it changes.
  static void name_state(int f) {
    static zgame::State* last = nullptr;
    mech3::Mech3App* app = mech3::app_if_any();
    zgame::State* now = app ? app->current() : nullptr;
    if (now == last) return;
    last = now;
    auto* st = dynamic_cast<const mech3::State*>(now);
    std::printf("mw1: frame %d: %s\n", f, st ? st->name() : "none");
  }
  void walk_frame(const std::string& page, mech3::GameEngine& ge, zgame::State* now) {
    static int frame = 0, start = -1;
    ++frame;
    if (start < 0 && now == static_cast<zgame::State*>(ge.menu_state_of(hub_))) start = frame;
    if (start < 0 || (frame - start) % kWalkStep != 0) return;
    const int step = (frame - start) / kWalkStep;
    if (page == "walk_bar") {
      if (step == 1) press(kHubBar);
      if (step == 2) press_bar(kBarBack);
    } else if (page == "walk_office_lab") {
      if (step == 1) press(kHubOffice);
      if (step == 2) press_office(kOfficeRequest);
      if (step == 3) press_office(kOfficeAccept);  // back to the hub
      if (step == 4) press(kHubMarket);
      if (step == 5) press_market(kMarketLab);
    }
  }

  void pause_shot_frame(const std::string& page) {
    mech3::GameEngine* ge = ge_;
    mech3::Mech3App* app = mech3::app_if_any();
    if (!ge || !app) return;
    zgame::State* now = app->current();
    if (page.rfind("walk_", 0) == 0) return walk_frame(page, *ge, now);
    static zgame::State* last = nullptr;
    static int frames = 0;
    frames = now == last ? frames + 1 : 0;
    last = now;
    const bool lab = page.rfind("lab", 0) == 0;
    // The lab's pages: the Mech Complex's MECH LAB on the commander's mech (the briefing's is never shown).
    if (lab && now == static_cast<zgame::State*>(ge->menu_state_of(hub_)) && frames == 10 && lab_owned_ < 0) {
      static bool once = false;
      if (!once) once = true, press(kHubMarket), press_market(kMarketLab);
    }
    // The office's offer taken (MW1's way: the office, REQUEST, ACCEPT), then the hub's DEPLOY: the loading screen,
    // then the mission.
    if (!lab && now == static_cast<zgame::State*>(ge->menu_state_of(hub_)) && frames == 10 && !deployed_ && !pending_) {
      if (!contract_.accepted && earning_here(company_)) new_offer(), take_contract();
      if (contract_.accepted) press(kHubDeploy);
    }
    if (page == "lab_fit") return lab_fit_frame(*ge);
    if (page == "lab_render") return lab_render_frame(*ge, now);
    // lab_<page> (1 weapons, 3 criticals, 4 groups; lab_3e and lab_3a its supply tabs) or lab_files (the load box).
    if (now == &mech3::states().lab && frames == 20 && page != "lab") {
      static bool once = false;
      if (page == "lab_files" && !once) once = true, ge->config_import();
      else if (mech3::LabScreen* l = ge->lab_screen(mech3::states().lab.lab_screen_); l && page.size() >= 5) {
        l->switch_page(page[4] - '0');
        if (page.size() == 6 && page[4] == '3')  // lab_3e, lab_3a: the criticals page's equipment (ENGINE SELECTION) or armour tab
          l->unassigned.set_tab(3, page[5] == 'a'), l->unassigned.refill();
        // lab_4u (FIXES.md item 40): the groups page with weapons moved: the first and third out of their groups (the
        // ungrouped list), the fourth into the first group.
        if (page == "lab_4u") {
          const mech3::LabState& lab = mech3::states().lab;
          int32_t k = 0;
          for (mech3::Handle it : ge->lab_config_items(ge->lab_slot_cfg(ge->lab_slot(lab.pilot_, lab.slot_))))
            if (ge->lab_item_kind(it) == 1) {
              if (k == 0 || k == 2) ge->lab_set_item_group(it, -1);
              if (k == 3) ge->lab_set_item_group(it, 0);
              ++k;
            }
          l->refresh_groups();
        }
      }
    }
    // MW1_LAB_HOVER=0..3 (development: --shot pictures of a tray button's hover label, FIXES.md item 37): that
    // button's HELP labels shown, the others' hidden, each frame from the 25th.
    if (const char* h = std::getenv("MW1_LAB_HOVER"); h && lab && now == &mech3::states().lab && frames >= 25)
      if (mech3::LabScreen* l = ge->lab_screen(mech3::states().lab.lab_screen_))
        for (int32_t i = 0; i < 4; ++i)
          if (mech3::HelpButton* t = l->tab(kLabTabPages[i]))
            for (ztwod::Text* x : t->help) i == std::atoi(h) ? x->show_for(-1.0f) : x->hide();
    if (page == "results" && now == &mech3::states().core && frames == 600) {
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      for (int32_t i = 0; i < n; ++i) mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
      mech3::mech_hit_part(mech3::mech_mission_player(), 2, 40.0f);
      mech3::missions().ended = 1;
      mech3::missions().on_command(0x79);  // Exit Mission: the end screen, then (skip_debriefing) the results
      return;
    }
    if (page == "results") return;
    if (now == &mech3::states().core && frames == 90) app->push(app->state(3), 0);
    if (now == &mech3::states().pause && frames == 10 && page == "pause_options")
      if (auto* p = dynamic_cast<mech3::PauseScreen*>(ge->screen_of(mech3::states().pause.screen_))) p->on_button(mech3::PauseScreen::kOptions, 0, 0);
  }

  // MW1_SCREEN=lab_render (development, FIXES 14's test renders): the pass's stage (render_stage) showing
  // MW1_RENDER_MECH (a PM name, any case; else the lab's own) at MW1_RENDER_YAW degrees (0 the front).
  void lab_render_frame(mech3::GameEngine& ge, zgame::State* now) {
    if (now != &mech3::states().lab) return;
    static bool chosen = false;
    if (!chosen) {
      chosen = true;
      if (const char* n = std::getenv("MW1_RENDER_MECH"); n && mech3::mech_index(n) >= 0)
        mech3::states().lab.mech_ = mech3::mech_index(n);
    }
    const char* y = std::getenv("MW1_RENDER_YAW");
    render_stage(ge, y ? std::strtof(y, nullptr) : 0.0f);
  }

  // ---- FIXES.md item 14: the mech renders (mw1_mech_renders.h) ----
  // The lab made a render stage, each frame: its 2D hidden (the cover's apart), its pointer off, the camera moved by
  // kRenderCamera (once a lab), the 3D window a square in the render's middle (black round the mech: the lab clears
  // to black), and the shown mech held at `yaw` degrees (0 the front).
  void render_stage(mech3::GameEngine& ge, float yaw) { stage_2d(ge), stage_3d(ge, yaw); }
  void stage_2d(mech3::GameEngine& ge) {
    mech3::LabState& ls = mech3::states().lab;
    mech3::LabScreen* l = ge.lab_screen(ls.lab_screen_);
    if (!l) return;
    for (ztwod::Object2D* o = l->head; o; o = o->next)
      if (!(o->flags & ztwod::kHidden) && !is_cover(o)) o->hide(), stage_hidden_.push_back(o);
    if (l->cursor) stage_cursor_ = l->cursor, l->set_cursor(nullptr);
    if (stage_screen_ != l) {  // a lab just opened: its camera from the constants
      stage_screen_ = l;
      for (int i = 0; i < 3; ++i) ls.cam_pos_[i] += kRenderCamera[i];
    }
  }
  // True when the stage is set (a view and its mech): only then is the frame the pass reads back the mech alone.
  bool stage_3d(mech3::GameEngine& ge, float yaw) {
    mech3::LabState& ls = mech3::states().lab;
    mech3::LabView* v = ge.lab_view();
    zcls::Node* node = ls.model_ ? ge.lab_model_node(ls.model_) : nullptr;
    if (!v || !node) return false;
    const mech3::Rect& r = mech3::render();
    const int32_t side = std::min(r.w, r.h);
    zcls::window_set_size(v->window, side, side);
    zcls::window_set_origin(v->window, r.x + (r.w - side) / 2, r.y + (r.h - side) / 2);
    zcls::object3d_set_rotation(node, 0.0f, yaw * 0.017453292f, 0.0f);
    return true;
  }
  bool is_cover(const ztwod::Object2D* o) const {
    if (o == cover_plaque_.get() || o == cover_text_.get()) return o != nullptr;
    for (const auto& c : cover_cells_)
      if (o == c.get()) return true;
    return false;
  }
  // The pass (once a run, when the archive is missing or its stamp differs, and the renderer reads frames back):
  // settled on MW1's menu or hub, the lab opened over it on the first mech. The player sees only the cover (a black
  // screen, the plaque PREPARING THE MECH BAY and a cell lit per mech done): each mech is shown in frames the display
  // holds back (Render3D::hold_display), held at the front kRenderSettle frames, read back and made its pictures, then
  // one cover frame shows the progress. At the end the archive is written and registered and the lab ACCEPTed under a
  // held display, so the screen it opened over comes back straight from the cover. No pictures: the amber icons stay.
  static constexpr int32_t kRenderSettle = 4, kRenderGiveUp = 120;
  enum class PassStep { kCover, kRender, kLeaving };
  void hold_display(bool on) {
    if (platform::Render3D* r3 = ge_ ? ge_->platform_parts().render : nullptr) r3->hold_display(on);
  }
  // The cover's pieces in the lab's 2D layer, made once a lab (they outlive it: deleted once it has gone).
  void show_cover(mech3::GameEngine& ge, mech3::LabScreen& l, bool shown) {
    const int32_t n = static_cast<int32_t>(pass_.mechs.size());
    if (!cover_text_) {
      const int32_t w = ge.buffer_width(), x0 = (w - kPlaqueCoverW) / 2, y0 = 190;
      cover_plaque_ = std::make_unique<ztwod::Image>(0);
      cover_plaque_->set_image(kPlaqueCover);
      cover_plaque_->set_pos(x0, y0);
      l.add(cover_plaque_.get());
      cover_text_ = std::make_unique<ztwod::Text>();
      cover_text_->set_pos(w / 2, y0 + 15);
      cover_text_->set_font(ztwod::host().font_find("mw1_plaque"));
      cover_text_->set_text_plain("PREPARING THE MECH BAY");
      l.add(cover_text_.get());
      const int32_t bar = n * kProgressCellW + (n - 1) * kProgressGap;
      for (int32_t i = 0; i < n; ++i) {
        cover_cells_.push_back(std::make_unique<ztwod::Image>(0));
        cover_cells_.back()->set_pos((w - bar) / 2 + i * (kProgressCellW + kProgressGap), y0 + kPlaqueH + 16);
        l.add(cover_cells_.back().get());
      }
    }
    std::vector<ztwod::Object2D*> all = {cover_plaque_.get(), cover_text_.get()};
    for (size_t i = 0; i < cover_cells_.size(); ++i) {
      cover_cells_[i]->set_image(kProgressCell[i < pass_.at ? 1 : 0]);
      all.push_back(cover_cells_[i].get());
    }
    for (ztwod::Object2D* o : all) {
      if (shown) o->show_for(-1.0f);
      else o->hide();
      o->dirty();
    }
  }
  void drop_cover() {
    cover_cells_.clear(), cover_text_.reset(), cover_plaque_.reset();
    stage_hidden_.clear(), stage_cursor_ = nullptr, stage_screen_ = nullptr;
  }
  void render_pass_frame() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (!renders_wanted_ || !ge_ || !app) return;
    mech3::GameEngine& ge = *ge_;
    zgame::State* lab = &mech3::states().lab;
    if (!pass_.on) {
      const bool home = (menu_state_ && app->current() == static_cast<zgame::State*>(menu_state_)) ||
                        (hub_state_ && app->current() == static_cast<zgame::State*>(hub_state_));
      pass_.wait = home && !pending_ ? pass_.wait + 1 : 0;
      if (pass_.wait < 10) return;
      platform::Render3D* r3 = ge.platform_parts().render;
      std::vector<uint32_t> probe;
      int32_t pw = 0, ph = 0;
      if (!r3 || !r3->read_back(&probe, &pw, &ph)) return void(renders_wanted_ = false);
      if (renders_current_ && !renders_outgrown(renders_made_height_, ph)) return void(renders_wanted_ = false);
      if (renders_current_)
        std::printf("mw1: the mech pictures were made in a %d-row frame, this one is %d: made again\n", renders_made_height_, ph);
      pass_frame_height_ = ph;
      pass_ = RenderPass{};
      pass_.on = true, pass_.mechs = render_mechs();
      std::printf("mw1: rendering %zu mech pictures from the install's models (FIXES.md item 14)\n", pass_.mechs.size());
      mech3::Pilot* p = company_pilot();
      mech3::current_pilot() = p;
      p->select_game(mech3::kInstantAction);
      mech3::ActionGame* g = p->action();
      while (g->count > 0) g->remove(0);
      g->slot.set_config(nullptr), g->slot.mech = pass_.mechs.front();
      g->slot.set_config(stock_loadout(pass_.mechs.front()));
      g->store.clear();
      g->store.full = 0;
      hold_display(true);  // the lab's own first frames never show
      ge.pilot_chosen(p);
      ge.mech_lab_open(ge.store_handle(&g->store), 0);
      return;
    }
    if (app->current() != lab) {
      if (!pass_.opened && ++pass_.idle < kRenderGiveUp) return;
      if (!pass_.opened) std::printf("mw1: the mech lab never opened: no mech renders this run\n");
      hold_display(false);
      drop_cover();
      renders_wanted_ = false, pass_.on = false;
      return;
    }
    pass_.opened = true;
    mech3::LabState& ls = mech3::states().lab;
    mech3::LabScreen* l = ge.lab_screen(ls.lab_screen_);
    mech3::LabView* v = ge.lab_view();
    if (!l || !v) return;
    if (pass_.step == PassStep::kLeaving) {  // the cover's last frame shown: held from here, ACCEPT
      if (pending_accept_ == l) hold_display(true), pending_accept_ = nullptr, l->switch_page(6);
      return;
    }
    stage_2d(ge);
    if (pass_.step == PassStep::kCover) {  // the next frame the cover alone, shown
      show_cover(ge, *l, true);
      zcls::window_set_size(v->window, 0, 0);
      hold_display(false);
      if (pass_.at >= pass_.mechs.size()) {
        pass_.step = PassStep::kLeaving;
        finish_pass(ge, *l);
      } else {
        pass_.step = PassStep::kRender, pass_.wait = 0, pass_.idle = 0;
      }
      return;
    }
    // kRender: the mech alone in held frames, till it has shown kRenderSettle of them.
    const int32_t mech = pass_.mechs[pass_.at];
    if (pass_.wait == 0) {
      hold_display(true);
      show_cover(ge, *l, false);
      ls.mech_ = mech;
    }
    // Read back only frames drawn on a set stage, its 2D hidden from the frame before (stage_2d runs each frame):
    // FIXES 14's first pass read the lab's whole screen when the stage was not set.
    const bool staged = stage_3d(ge, 0.0f) && !stage_hidden_.empty() && !l->cursor;
    const int32_t entry = ls.model_index(mech);
    const bool shown = staged && entry >= 0 && static_cast<size_t>(entry) < ls.models_.size() && ls.model_ == ls.models_[static_cast<size_t>(entry)];
    pass_.wait = shown ? pass_.wait + 1 : std::max(pass_.wait, 1);
    if (!shown && ++pass_.idle > kRenderGiveUp) {
      std::printf("mw1: the %s never showed in the lab: no render of it\n", mech3::mech_name(mech));
      ++pass_.at, pass_.step = PassStep::kCover;
    } else if (shown && pass_.wait > kRenderSettle) {
      std::vector<uint32_t> px;
      int32_t w = 0, h = 0;
      if (ge.platform_parts().render->read_back(&px, &w, &h)) {
        std::vector<images::Image> made = renders_from_frame(mech, px.data(), w, h);
        if (made.empty())
          std::printf("mw1: the %s's frame was %s: no render of it\n", mech3::mech_name(mech),
                      frame_mech_alone(px.data(), w, h) ? "empty" : "not the mech alone on black");
        for (images::Image& i : made) pass_.made.push_back(std::move(i));
        if (!made.empty())
          for (HiRender& i : renders_hi_from_frame(mech, px.data(), w, h)) pass_.hi.push_back(std::move(i));
      }
      ++pass_.at, pass_.step = PassStep::kCover;
    }
  }
  // The archive written and registered; the lab ACCEPTed with the display held from the cover's last frame (the
  // screen it opened over comes back; render_pass_frame lets the display go once the lab has gone).
  void finish_pass(mech3::GameEngine& ge, mech3::LabScreen& l) {
    const std::string dir = zutl::mounts().write;
    // Every mech's pictures, or nothing written: no stamp, so the next start tries again (the amber icons till then).
    const bool whole = pass_.made.size() == pass_.mechs.size() * 2 && pass_.hi.size() == pass_.made.size();
    // Made again over current ones (a bigger frame): the archive's library holds its file open; closed for the write
    // (the same mechs, so the same table: its kept index still holds, and its next load opens the new file).
    if (whole && renders_ready())
      for (zimg::Library* lib : zimg::library_list(1))
        if (std::strncmp(lib->name, kRenderLibrary, std::strlen(kRenderLibrary)) == 0 && lib->name[std::strlen(kRenderLibrary)] == '.')
          zimg::close(*lib);
    if (whole && write_renders(dir, pass_.made, pass_.hi, pass_.mechs, pass_frame_height_)) {
      renders_current_ = true, renders_made_height_ = pass_frame_height_;
      if (!renders_ready()) zimg::new_image_library(kRenderLibrary, 0, false);
      art::Store::get().add(hi_renders_path(dir));
      set_renders_ready(true);
      std::printf("mw1: %zu mech pictures made into %s.zbd\n", pass_.made.size(), kRenderLibrary);
    } else {
      std::printf(whole ? "mw1: the mech pictures could not be written: the amber icons stay\n"
                        : "mw1: %zu of %zu mech pictures made: none kept, the next start tries again\n",
                  pass_.made.size(), pass_.mechs.size() * 2);
    }
    (void)ge;
    pending_accept_ = &l;
  }

  // A fit check's end (screen_fit_frame, lab_fit_frame): the program quits as closing its window does. Never _Exit:
  // stdout reaches the console and mw3.log through the run log's pipe and thread (sdlfe::RunLog), and an _Exit under
  // load ended the program before the thread had passed the report on ("exit 0 with no report", FIXES 3+5's flake,
  // the lead 2026-11-18); a quit lets the run log pass the last lines first. The report decides, not the exit (0).
  static void end_fit_run() {
    std::fflush(nullptr);
    mech3::Mech3App* app = mech3::app_if_any();
    if (app && app->message_pump()) app->message_pump()->post_quit();
    else std::_Exit(0);
  }

  // MW1_FIT=1 with MW1_SCREEN=<page> (FIXES 3+5's guard, tests/screen_fit.cmake): 40 frames into the MW1 screen the
  // page shows, each text of it inside the canvas (and clear of its arrows, mw1_lab_fit.h); printed by place and length
  // (MW1's words never), then the program ends (end_fit_run).
  void screen_fit_frame() {
    static bool reported = false;
    mech3::Mech3App* app = mech3::app_if_any();
    const char* page = std::getenv("MW1_SCREEN");
    if (reported || !ge_ || !app || !page) return;
    const std::string p = page;
    auto on = [&](auto* st) { return st && app->current() == static_cast<zgame::State*>(st) ? st->screen() : nullptr; };
    // A new company's hub opens on its story's first event, the news on the event panel: those count as the page's.
    Mw1Screen* sc = p == "results"     ? on(game_loop_results())
                    : p.rfind("loading", 0) == 0 ? (app->current() == &mech3::states().load ? loading_.get() : nullptr)
                    : p == "hub_options" ? on(game_loop_hub_options())
                    : p == "news" || p == "bar_drink" || p == "event" || (p.rfind("hub", 0) == 0 && on(game_loop_event()))
                        ? on(game_loop_event())
                    : p.rfind("hub", 0) == 0                ? on(game_loop_hub())
                    : p.rfind("market", 0) == 0             ? on(game_loop_market())
                    : p.rfind("travel", 0) == 0             ? on(game_loop_travel())
                    : p.rfind("office", 0) == 0             ? on(game_loop_office())
                    : p.rfind("bar", 0) == 0 || p == "company" ? on(game_loop_bar())
                                                                              : nullptr;
    static int frames = 0;
    if (!sc) return void(frames = 0);
    if (++frames < 40) return;
    const int32_t w = ge_->buffer_width(), h = ge_->buffer_height();
    int texts = 0, outside = 0;
    // The results and loading screens' texts each inside its own piece (their layouts' 640 x 480, centred; the
    // results' wide layout's from the left): the plaque, the picture's column, the status terminal (clear of the
    // list's arrows), the message's band (clear of the crest), the buttons; the loading terminal at the foot, LOADING in it (FIXES.md
    // item 42); the others inside the canvas.
    const bool boxed = p == "results" || p.rfind("loading", 0) == 0;
    const bool wide_results = p == "results" && sc->wide();
    const std::vector<FitBox> boxes =
        wide_results ? std::vector<FitBox>{{12, 226, 260, 340, "column"},     {272, 12, 803, 336, "status"},
                                           {12, 344, 733, 440, "message"},    {150, 446, 318, 476, "replay"},
                                           {342, 446, 510, 476, "continue"},  {534, 446, 702, 476, "details"}}
        : p == "results" ? std::vector<FitBox>{{12, 186, 212, 340, "column"},   {220, 12, 590, 336, "status"},
                                               {12, 344, 520, 440, "message"},  {52, 446, 220, 476, "replay"},
                                               {236, 446, 404, 476, "continue"}, {420, 446, 588, 476, "details"}}
        : p.rfind("loading", 0) == 0 ? std::vector<FitBox>{{170, 16, 470, 76, "plaque"}, {48, 300, 476, 464, "terminal"}}
                         : std::vector<FitBox>{{0, 0, w, h, "canvas"}};
    const std::vector<FitText> fits = fit_texts(*sc, boxed && !wide_results ? (w - 640) / 2 : 0, 0, boxes);
    for (const FitText& f : fits) {
      ++texts;
      if (f.fits) continue;
      ++outside;
      std::printf("mw1 screen fit: %s, %zu letters at %d..%d x %d..%d: OUTSIDE\n", page, f.text.size(), f.left, f.right, f.top, f.bottom);
    }
    // Text over text (FIXES 3+5: the status bar's world over its words): two texts' boxes crossing by more than
    // kSlack each way (a text's box is 3 wider than its letters, and a block's lines may touch by a pixel or two).
    constexpr int32_t kSlack = 3;
    int overlaps = 0;
    for (size_t i = 0; i < fits.size(); ++i)
      for (size_t j = i + 1; j < fits.size(); ++j) {
        const FitText &a = fits[i], &b = fits[j];
        const int32_t dx = std::min(a.right, b.right) - std::max(a.left, b.left);
        const int32_t dy = std::min(a.bottom, b.bottom) - std::max(a.top, b.top);
        if (dx <= kSlack || dy <= kSlack) continue;
        ++overlaps;
        std::printf("mw1 screen fit: %s, %zu letters at %d..%d x %d..%d over %zu letters at %d..%d x %d..%d: OVERLAP\n",
                    page, a.text.size(), a.left, a.right, a.top, a.bottom, b.text.size(), b.left, b.right, b.top, b.bottom);
      }
    std::printf("mw1 screen fit: done at %dx%d on %s, %d texts, %d outside, %d overlapping\n", w, h, page, texts, outside,
                overlaps);
    reported = true;
    end_fit_run();
  }

  // MW1_SCREEN=lab_fit (the lab's text guard, mw1_lab_fit.h): the lab's pages 0 (chassis), 1, 3 (each supply tab)
  // and 4, then its load box, each 20 frames after it shows; each text printed with whether it fits its panel; then the program
  // ends (end_fit_run).
  void lab_fit_frame(mech3::GameEngine& ge) {
    mech3::Mech3App* app = mech3::app_if_any();
    static int step = 0, wait = 0, texts = 0, outside = 0;
    auto report = [&](const char* where, ztwod::Layer& layer, int32_t ox, int32_t oy, const std::vector<FitBox>& panels) {
      for (const FitText& f : fit_texts(layer, ox, oy, panels)) {
        ++texts, outside += f.fits ? 0 : 1;
        std::printf("mw1 lab fit: %s \"%s\" %d..%d x %d..%d in %s: %s\n", where, f.text.c_str(), f.left, f.right, f.top,
                    f.bottom, f.panel ? f.panel : "none", f.fits ? "fits" : "OUTSIDE");
      }
    };
    if (ge.file_box_) {  // the load box's own frames (LabFileBox::run)
      if (++wait < 20) return;
      mech3::LabFileBox& box = *ge.file_box_;
      report("load box", box, ge.buffer_width() / 2 - 320, 0, load_box_panels());
      wait = 0, step = 100;
      box.answer(false);
      return;
    }
    if (step == 101) return;  // reported: the quit on its way
    if (step == 100) {
      std::printf("mw1 lab fit: done at %dx%d, %d texts, %d outside\n", ge.buffer_width(), ge.buffer_height(), texts, outside);
      step = 101;
      return end_fit_run();
    }
    if (!app || app->current() != &mech3::states().lab) return;
    mech3::LabScreen* l = ge.lab_screen(mech3::states().lab.lab_screen_);
    if (!l || ++wait < 20) return;
    wait = 0;
    // The pages, the criticals page with each supply tab: weapons, equipment (its ENGINE SELECTION table), armour.
    struct Step { int32_t page, tab; bool armor; const char* name; };
    static const Step kSteps[] = {{0, 0, false, "chassis"}, {3, 1, false, "criticals, weapons"},  // page 1 has no tab (item 26)
                                  {3, 2, false, "criticals, ammo"}, {3, 3, false, "criticals, equipment"},
                                  {3, 3, true, "criticals, armor"}, {4, 0, false, "groups"}};
    constexpr int kStepCount = static_cast<int>(sizeof kSteps / sizeof kSteps[0]);
    report(kSteps[step].name, *l, ge.buffer_width() / 2 - 320, 0, lab_fit_panels());
    // FIXES.md item 29: the tray's buttons, each shown one's hit area (its picture), the same size, apart, in the tray.
    std::vector<FitBox> tray;
    static const char* const kTabNames[4] = {"chassis", "criticals", "groups", "ACCEPT"};
    for (int32_t i = 0; i < 4; ++i)
      if (mech3::HelpButton* t = l->tab(kLabTabPages[i]); t && !(t->flags & ztwod::kHidden) && t->image) {
        const int32_t x = t->get_x() - (ge.buffer_width() / 2 - 320), y = t->get_y();
        tray.push_back({x, y, x + t->image->width, y + t->image->height, kTabNames[i]});
        std::printf("mw1 lab fit: %s the %s button %d..%d x %d..%d\n", kSteps[step].name, kTabNames[i], x, x + t->image->width, y,
                    y + t->image->height);
      }
    std::vector<std::string> wrong = tray_problems(tray, kLabTrayY);
    if (tray.size() != 4) wrong.push_back(std::to_string(tray.size()) + " of the 4 buttons shown");
    // FIXES.md item 37: each button's hover label (shown or not) centred over its own button, under the tray's line,
    // clear of the button, inside the layout, over no other label.
    std::vector<FitBox> labels;
    for (int32_t i = 0; i < 4; ++i) {
      mech3::HelpButton* t = l->tab(kLabTabPages[i]);
      if (!t || (t->flags & ztwod::kHidden) || !t->image) continue;
      const int32_t bx = t->get_x() - (ge.buffer_width() / 2 - 320), by = t->get_y(), middle = bx + t->image->width / 2;
      for (ztwod::Text* x : t->help) {
        if (!x->text[0]) continue;
        if (x->needs_render) x->render();
        int32_t lx = x->x - (ge.buffer_width() / 2 - 320);
        if (x->align == 1) lx -= x->width / 2;
        else if (x->align == 2) lx -= x->width;
        const FitBox b{lx, x->y, lx + x->width, x->y + x->text_height(), kTabNames[i]};
        std::printf("mw1 lab fit: %s the %s label \"%s\" %d..%d x %d..%d\n", kSteps[step].name, kTabNames[i], x->text, b.left,
                    b.right, b.top, b.bottom);
        const std::string n = std::string("the ") + kTabNames[i] + " label";
        if (std::abs((b.left + b.right) / 2 - middle) > 1) wrong.push_back(n + " isn't centred over its button");
        if (b.top <= kLabTrayY) wrong.push_back(n + " is on or above the tray's line");
        if (b.bottom > by) wrong.push_back(n + " overlaps its button");
        if (b.left < 0 || b.right > 640) wrong.push_back(n + " is outside the layout");
        for (const FitBox& o : labels)
          if (b.left < o.right && o.left < b.right && b.top < o.bottom && o.top < b.bottom) wrong.push_back(n + " overlaps another's");
        labels.push_back(b);
      }
    }
    if (labels.size() != tray.size()) wrong.push_back(std::to_string(labels.size()) + " labels for " + std::to_string(tray.size()) + " buttons");
    // FIXES.md item 39: 'MECH LOAD-OUT's and AVAILABLE SUPPLIES' rows centred in their panels (their left and right
    // margins within kLabMarginSlack of each other, neither on the frame), the supply tabs from the rows' left.
    int32_t margins = 0;
    if (l->page == kLabCriticalsPage) {
      const int32_t left = ge.buffer_width() / 2 - 320;
      struct ListIn { mech3::LabItemList* list; int32_t x0, x1; const char* name; };
      for (const ListIn& in : {ListIn{&l->assigned, chrome::kLabSideWhole.x0, chrome::kLabSideWhole.x1, "'MECH LOAD-OUT"},
                               ListIn{&l->unassigned, 420 + chrome::kLabSideWhole.x0, 420 + chrome::kLabSideWhole.x1, "AVAILABLE SUPPLIES"}}) {
        if (in.list->flags & ztwod::kHidden) continue;
        const int32_t rows_l = lab_list_left(*in.list) - left, rows_r = rows_l + in.list->row_width;
        const int32_t ml = rows_l - (in.x0 + chrome::kLabFrame), mr = (in.x1 - chrome::kLabFrame) - rows_r;
        std::printf("mw1 lab fit: %s %s rows %d..%d, margins %d and %d\n", kSteps[step].name, in.name, rows_l, rows_r, ml, mr);
        ++margins;
        if (std::abs(ml - mr) > kLabMarginSlack || ml < 2 || mr < 2) wrong.push_back(std::string(in.name) + "'s rows aren't centred in their panel");
      }
      mech3::LabUnassignedList& u = l->unassigned;
      if (!(u.flags & ztwod::kHidden) && !u.weapon.labels.empty()) {
        ++margins;
        if (std::abs(u.weapon.labels[0]->x - lab_list_left(u)) > 1) wrong.push_back("the supply tabs don't start at the rows' left");
      }
    }
    for (const std::string& w : wrong) std::printf("mw1 lab fit: %s the tray: %s: OUTSIDE\n", kSteps[step].name, w.c_str());
    texts += static_cast<int>(tray.size() + labels.size()) + margins, outside += static_cast<int>(wrong.size());
    if (++step < kStepCount) {
      if (l->page != kSteps[step].page) l->switch_page(kSteps[step].page);
      if (kSteps[step].tab) l->unassigned.set_tab(kSteps[step].tab, kSteps[step].armor), l->unassigned.refill();
    } else {
      ge.config_import();  // its frames run inside; then the end
    }
  }

  // The contract as the Action screen's LAUNCH sets a game up (action_menu.cpp): the company pilot's Instant Action
  // game, its map and mode, the enemy force, the company's mech and loadout, and as its store a copy of the
  // company's stores, which only the launch writes back. Then the Load state: PM's briefing built but never shown
  // (FIXES.md item 18), MW1's loading screen while the mission loads, then the launch.
  void deploy(mech3::GameEngine& ge) {
    mech3::Pilot* p = company_pilot();
    mech3::current_pilot() = p;
    p->select_game(mech3::kInstantAction);
    mech3::ActionGame* g = p->action();
    const int32_t world = mech3::mission_dir_index(contract_.world.c_str());
    while (g->count > 0) g->remove(0);
    g->set_mode(contract_.mode);
    g->field_bases = 0;
    if (g->slot.mech != company_.mech) g->slot.set_config(nullptr), g->slot.mech = company_.mech;
    g->slot.set_config(company_.loadout.get());  // a copy; null: the stock loadout
    // The lancemates with a mech, as allies first (line-up slots 1..), their mechs' saved loadouts; then the enemy.
    // Instant Action's line-up offers them (ally_launching) during the launch, before mission_launched.
    flying_.clear(), fates_.clear(), launches_.clear(), enemy_aims_.clear();
    bills_.clear(), mended_.clear(), commander_destroyed_ = false;
    for (size_t i = 0; i < company_.lance.size(); ++i)
      if (const int32_t h = mech_of(company_, static_cast<int32_t>(i)); h >= 0) {
        const OwnedMech& o = company_.hangar[static_cast<size_t>(h)];
        g->add(o.mech, 1, o.loadout.get(), 0);
        flying_.push_back(company_.lance[i].id);
      }
    for (int32_t k : contract_.enemies) g->add(kMechKinds[k].pm_mech, 0, nullptr, 0);
    g->x6c = 1, g->x70 = 0;
    g->set_world(world, contract_.ia);
    g->store.clear();
    g->store.full = 0;
    g->store.add_all(*company_.stores);
    p->save();
    deployed_ = true, launched_ = false;
    std::printf("mw1: deployed to %sia%d with the %s: loading\n", contract_.world.c_str(), contract_.ia,
                mech3::mech_name(company_.mech));
    ge.replace_state(6);  // the Load state: the mission loaded behind MW1's loading screen, then launched
  }

  Rng rng_;
  Company company_;
  Contract contract_;
  std::shared_ptr<mech3::Store> launch_stores_;   // the company's stores at the contract's first launch
  std::shared_ptr<mech3::Config> launch_loadout_;  // and its loadout
  std::vector<Wreck> wrecks_;
  std::vector<int32_t> flying_;    // the lancemates deployed, by ally slot (1..)
  std::vector<LanceFate> fates_;
  std::vector<AllyLaunch> launches_;  // tests: game_loop_ally_launches
  std::vector<float> enemy_aims_;     // a story battle's enemies' aim errors as launched (tests)
  std::vector<MechBill> bills_;       // this mission's company mechs (collect_bills)
  std::map<int32_t, int32_t> mended_;  // what field bases mended, by whose (0 the commander)
  bool commander_destroyed_ = false;   // those who launched this mission, and whether their mech was destroyed
  std::unique_ptr<AfterAction> pending_;  // a mission back, AFTER ACTION until OK (or REPLAY)
  bool offered_ = false;                  // a contract on offer at the House office (not yet accepted)
  zgame::Handle travel_ = 0;
  TravelState* travel_state_ = nullptr;
  TravelChoice tchoice_;
  // The hub on this engine (Engine::adopt_state): its handle and state, made once.
  mech3::GameEngine* menu_engine_ = nullptr;
  zgame::Handle menu_ = 0, intro_ = 0;
  MenuState* menu_state_ = nullptr;
  IntroState* intro_state_ = nullptr;
  zgame::Handle credits_ = 0;
  CreditsState* credits_state_ = nullptr;
  bool credits_next_ = false;  // the victory's panel up: the credits when it's read
  bool credits_up_ = false;
  Start start_ = Start::kMenu;
  mech3::GameEngine* hub_engine_ = nullptr;
  mech3::GameEngine* ge_ = nullptr;
  zgame::Handle hub_ = 0;
  HubState* hub_state_ = nullptr;
  int32_t hub_hover_ = 0;  // the hub's button under the mouse (its words on the status line), 0 none
  zgame::Handle bar_ = 0;
  BarState* bar_state_ = nullptr;
  BarChoice bchoice_;
  zgame::Handle office_ = 0;
  OfficeState* office_state_ = nullptr;
  zgame::Handle hub_options_ = 0;
  HubOptionsState* hub_options_state_ = nullptr;
  OfficeChoice ochoice_;
  zgame::Handle market_ = 0;
  MarketState* market_state_ = nullptr;
  MarketChoice choice_;
  int32_t lab_owned_ = -1;
  // FIXES.md item 14's render pass and its stage (render_pass_frame, render_stage).
  struct RenderPass {
    std::vector<int32_t> mechs;
    size_t at = 0;
    int32_t wait = 0, idle = 0;
    bool on = false, opened = false;
    PassStep step = PassStep::kCover;
    std::vector<images::Image> made;
    std::vector<HiRender> hi;  // the 4x set (research/hires-2d.md B2b)
  };
  RenderPass pass_;
  std::unique_ptr<ztwod::Image> cover_plaque_;
  std::unique_ptr<ztwod::Text> cover_text_;
  std::vector<std::unique_ptr<ztwod::Image>> cover_cells_;
  mech3::LabScreen* pending_accept_ = nullptr;
  bool renders_wanted_ = false;
  bool renders_current_ = false;      // the archive current at the start (its stamp)
  int32_t renders_made_height_ = 0;   // the frame it was made in (0: unknown)
  int32_t pass_frame_height_ = 0;     // this pass's frame
  std::vector<ztwod::Object2D*> stage_hidden_;
  ztwod::Object2D* stage_cursor_ = nullptr;
  mech3::LabScreen* stage_screen_ = nullptr;  // the owned mech in the Mech Complex's MECH LAB (-1 none open)
  int32_t lab_who_ = kNobody;  // its pilot as chosen in the lab (mw1_pilots.h)
  bool lab_hook_ = false;
  PilotChooser lab_pilot_;
  LabFunds lab_funds_;  // the funds on the lab's pages (FIXES.md item 41)
  // The PILOT line's place on the lab's screen: from the canvas's right edge, and down.
  static constexpr int32_t kLabChassisPage = 0;  // LabScreen::page: the chassis page (ICON_CHASSIS)
  static constexpr int32_t kLabCriticalsPage = 3;  // the criticals page: LOAD-OUT and the supplies (ICON_CRITICALS)
  std::shared_ptr<mech3::Store> salvage_;  // this mission's parts, the stores' on a win
  bool deployed_ = false, launched_ = false, ended_ = false, failed_ = false, own_wins_ = false;
  bool ending_latched_ = false, ending_hook_ = false;  // the outcome taken this launch; the per-frame watch put on
  void* nav_point_ = nullptr;
  zgame::Handle nav_node_ = 0;  // and its structure's node  // item 43: the objective's nav point (the mission's waynet's), null none
  std::string nav_label_;  // item 43: the objective's nav point's label (tests), "" none
  bool row_set_ = false;  // install_game_loop's reach, or set_game_loop_row's: every new contract on that row
  MapRow row_;
  std::vector<int32_t> enemies_;  // set_game_loop_enemies': every new contract's force (empty: its own)
  std::string objective_built_;
  bool objective_fell_back_ = false;
  std::string save_path_;  // mw1save.txt; empty: no save
  std::string world_;      // set_game_loop_world's: every new contract there
  int32_t world_ia_ = 1;
  bool fresh_ = false;     // --new-company: a new company over the save
  // The story's events (mw1_story.h) on the event panel (mw1_event_screen.h), over the screen they fire on.
  bool story_on_ = true;   // set_game_loop_story's (the tests that aren't about it: off)
  zgame::Handle events_[kSides] = {};       // by Side
  EventState* event_states_[kSides] = {};
  EventState* event_state_ = nullptr;  // the one up
  Side event_side_ = Side::kFull;
  bool event_up_ = false;  // the panel pushed
  zgame::Handle results_ = 0;
  ResultsState* results_state_ = nullptr;
  bool results_up_ = false;  // the results screen pushed (FIXES.md item 19)
  ResultsPanel seen_results_;  // the shown (the tests read it)
  ResultsChoice rchoice_;      // DETAILS shown, the list's scroll
  std::vector<int32_t> killers_;  // by wreck: who disabled it (AfterAction's killers)
  std::vector<int32_t> armor_left_, structure_left_;  // the commander's mech as the mission ended
  int32_t seconds_ = -1;  // the mission's time
  Dispatch ev_;            // the class firing
  Shown ev_shown_;         // the event on the panel
  EventPanel ev_panel_;
  size_t hub_page_ = 0;  // the hub's box's page (FIXES.md item 42)
  std::vector<Shown> news_;  // the Bar's NEWS: the stories live now
  // MW1's loading screen (mw1_loading.h) while a deployed mission loads, PM's briefing skipped (FIXES.md item 18).
  std::unique_ptr<screens::Mw1Screen> loading_;
  LoadingSeen seen_;  // tests
  bool hold_launch_ = false;   // tests: the Load state waits once loaded, the test launching or aborting it
  bool hold_for_shot_ = false;  // MW1_SCREEN=loading (development: --shot the loading screen)
};

}  // namespace

static std::unique_ptr<Loop>& the_loop() {
  static std::unique_ptr<Loop> loop;
  return loop;
}

void install_game_loop(uint32_t seed, const char* reach) {
  the_loop() = std::make_unique<Loop>(seed, reach);
  mech3::set_program_flow(the_loop().get());
  mech3::set_action_lineup_capacity(kLineupSlots);  // FIXES.md item 48: 16 past the commander (the exe's ten)
  mech3::set_objectives_extension(the_loop().get());
}

void uninstall_game_loop() {
  mech3::set_program_flow(nullptr);
  mech3::set_action_lineup_capacity(mech3::ActionGame::kLineupExe);
  mech3::set_objectives_extension(nullptr);
  the_loop().reset();
}

void set_game_loop_save(const std::string& path, bool new_company) {
  if (the_loop()) the_loop()->set_save(path, new_company);
}

void set_game_loop_start(Start s) {
  if (the_loop()) the_loop()->set_start(s);
}
MenuState* game_loop_menu() { return the_loop() ? the_loop()->menu_state() : nullptr; }
IntroState* game_loop_intro() { return the_loop() ? the_loop()->intro_state() : nullptr; }
CreditsState* game_loop_credits() { return the_loop() ? the_loop()->credits_state() : nullptr; }
void game_loop_press_menu(int32_t button) {
  if (the_loop()) the_loop()->press_menu(button);
}

void set_game_loop_world(const std::string& world, int32_t ia) {
  if (the_loop()) the_loop()->set_world(world, ia);
}
void set_game_loop_row(const MapRow& row) {
  if (the_loop()) the_loop()->set_row(row);
}
void set_game_loop_enemies(const std::vector<int32_t>& kinds) {
  if (the_loop()) the_loop()->set_enemies(kinds);
}
bool game_loop_structure_point(float out[3]) {
  return the_loop() && the_loop()->structure_point_now(out);
}
std::string game_loop_objective_built(bool* fell_back) {
  if (fell_back) *fell_back = the_loop() && the_loop()->objective_fell_back();
  return the_loop() ? the_loop()->objective_built() : std::string();
}

const AfterAction* game_loop_pending() { return the_loop() ? the_loop()->pending() : nullptr; }
HubState* game_loop_hub() { return the_loop() ? the_loop()->hub_state() : nullptr; }
ResultsState* game_loop_results() { return the_loop() ? the_loop()->results_state() : nullptr; }
const ResultsPanel* game_loop_results_shown() { return the_loop() ? &the_loop()->results_shown() : nullptr; }
HubOptionsState* game_loop_hub_options() { return the_loop() ? the_loop()->hub_options_state() : nullptr; }
void game_loop_press(int32_t button) {
  if (the_loop()) the_loop()->press(button);
}

BarState* game_loop_bar() { return the_loop() ? the_loop()->bar_state() : nullptr; }
OfficeState* game_loop_office() { return the_loop() ? the_loop()->office_state() : nullptr; }
EventState* game_loop_event() { return the_loop() ? the_loop()->event_state() : nullptr; }
LoadingSeen game_loop_loading_seen() { return the_loop() ? the_loop()->loading_seen() : LoadingSeen{}; }
void set_game_loop_hold_launch(bool on) {
  if (the_loop()) the_loop()->set_hold_launch(on);
}
void set_game_loop_story(bool on) {
  if (the_loop()) the_loop()->set_story(on);
}
void game_loop_offer_again() {
  if (the_loop()) the_loop()->offer_again();
}
MarketState* game_loop_market() { return the_loop() ? the_loop()->market_state() : nullptr; }
const PilotChooser* game_loop_lab_pilot() { return the_loop() ? the_loop()->lab_pilot() : nullptr; }
const LabFunds* game_loop_lab_funds() { return the_loop() ? the_loop()->lab_funds() : nullptr; }
TravelState* game_loop_travel() { return the_loop() ? the_loop()->travel_state() : nullptr; }
const std::vector<AllyLaunch>& game_loop_ally_launches() {
  static const std::vector<AllyLaunch> none;
  return the_loop() ? the_loop()->launches() : none;
}
const std::vector<float>& game_loop_enemy_aims() {
  static const std::vector<float> none;
  return the_loop() ? the_loop()->enemy_aims() : none;
}
Company* game_loop_company_for_tests() { return the_loop() ? the_loop()->company_mut() : nullptr; }
void game_loop_market_changed() {
  if (the_loop()) the_loop()->market_changed();
}

const Company* game_loop_company() { return the_loop() ? the_loop()->company() : nullptr; }
const Request* game_loop_office_ask() { return the_loop() ? the_loop()->office_ask() : nullptr; }
const Contract* game_loop_contract() { return the_loop() ? the_loop()->contract() : nullptr; }

}  // namespace mw1
