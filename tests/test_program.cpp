#include <set>
// The program's pieces that run without a window (docs/program.md): the
// message DLL, the mounts and write dir, the music lookup, and the start-up
// through InitInstance and the original main loop to the main menu's state.
#include <cmath>
#include <algorithm>
#include <map>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zsound/zsnd_cd.h"
#include "gamez/zutil/zutl_msg.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_reg.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_init.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_hud.h"
#include "mech3/hud.h"
#include "mech3/message_panel.h"
#include "mech3/app_mech.h"
#include "mech3/mech_objective.h"
#include "mech3/zai_node.h"
#include "mech3/zai_target.h"
#include "mech3/zai_valve.h"
#include "mech3/cockpit_sound.h"
#include "mech3/app_sensors.h"
#include "mech3/app_states.h"
#include "mech3/lab_screen.h"
#include "mech3/app_view.h"
#include "mech3/controls_menu.h"
#include "mech3/mainmenu.h"
#include "mech3/mech_control.h"
#include "mech3/mission.h"
#include "mech3/mission_world.h"
#include "mech3/options.h"
#include "mech3/mech_shade.h"
#include "mech3/pause_menu.h"
#include "mech3/pilot.h"
#include "mech3/briefing.h"
#include "gamez/zutil/zfix.h"
#include "mech3/debriefing.h"
#include "mech3/pilot_menu.h"
#include "gamez/ztwod/twod.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/player.h"
#include "mech3/sensors.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_attack.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_target.h"
#include "mech3/zrdrvalve.h"
#include "platform/reg_file.h"
#include "platform/silent_sound.h"

namespace {

// 0x005bcd88, the double 0x4161c0 subtracts from the ground's height
// (test_place.py checks the placement against the exe in the emulator).
constexpr double kPlaceSink = -0.01;

// The highest ground under (x, z) from top down, water (a material of soil
// 1) left out, through the multi-probe query 0x4ee2f0 (test_di),
// independent of the placement's own query; NaN when none.
float ground_under(float x, float top, float z) {
  zcls::Node* w = mech3::world_hooks().world;
  if (!w) return std::nanf("");
  float probe[3] = {x, top, z};
  static zcls::GroundHits hits;
  zcls::ground_query(w, probe, 1, top, &hits);
  float best = std::nanf("");
  for (int i = 0; i < hits.count; ++i) {
    if (hits.hit[i].material && hits.hit[i].material->soil == 1) continue;
    if (!(hits.hit[i].height <= best)) best = hits.hit[i].height;
  }
  return best;
}

bool have_game() { return std::filesystem::exists(std::string(MW3_GAME_DIR) + "/Mech3Msg.dll"); }

std::string temp_dir(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  std::filesystem::remove_all(d, ec);
  std::filesystem::create_directories(d, ec);
  return d;
}
void touch(const std::string& path) {
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (f) std::fputs("x", f), std::fclose(f);
}

struct SavedMounts {
  zutl::Mounts saved = zutl::mounts();
  ~SavedMounts() { zutl::mounts() = saved; }
};

}  // namespace

// A new process starts with zSound not yet initialised. The exe's shutdown
// (0x00570510) never clears `initialized` (0x0072c2c0) and its init returns
// early while it is set, so a second app in one process, as each mw3tests
// case makes, would get no sound queue. zEffect needs nothing: the app's
// start (0x0050d270) and end (0x0050d2b0) free it.
// The texture libraries likewise: the unload frees them (0x00518b70), the
// start (0x005184e0) does not touch them.
void reset_process_state() {
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
  mech3::damage_set() = mech3::DamageSet{};  // the HUD's silhouettes, loaded once a process (0x0043afc0)
}

TEST(msgdll_reads_the_message_table_and_zlocgetid) {
  if (!have_game()) return;
  zutl::MessageDll dll;
  EXPECT(zutl::read_message_dll(std::string(MW3_GAME_DIR) + "/Mech3Msg.dll", dll));
  EXPECT(dll.text.count(7) == 1 && dll.text[7] == "MechWarrior 3");
  EXPECT(dll.text[0x14d].find("Cannot read required data") == 0);
  EXPECT(dll.ids.size() > 1000);
  EXPECT(dll.ids["MSG_SKIN_PRIMARY"] == 0xf10 && dll.ids["MSG_SKIN_VARIANTH"] == 0xf17);
  zutl::install_messages(&dll);
  EXPECT(std::string(zutl::localize("MSG_SKIN_PRIMARY")) == "GUNMETAL");
  EXPECT(std::string(zutl::localize("damage")) == "damage");  // not a key: the name back
  zutl::install_messages(nullptr);
}

TEST(msgdll_rejects_a_file_that_is_not_a_dll) {
  const std::string d = temp_dir("msgdll");
  touch(d + "/fake.dll");
  zutl::MessageDll dll;
  EXPECT(!zutl::read_message_dll(d + "/fake.dll", dll));
  EXPECT(!zutl::read_message_dll(d + "/missing.dll", dll));
}

TEST(mounts_write_dir_first_then_mounts_in_order) {
  SavedMounts keep;
  const std::string d = temp_dir("mounts");
  touch(d + "/install/configs/a.mw3");
  touch(d + "/install/zbd/only.zbd");
  touch(d + "/override/configs/a.mw3");
  touch(d + "/saves/configs/a.mw3");
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(d + "/install");
  EXPECT(zutl::find("configs\\a.mw3") == d + "/install/configs/a.mw3");
  zutl::mount_front(d + "/override");
  EXPECT(zutl::find("configs\\a.mw3") == d + "/override/configs/a.mw3");
  EXPECT(zutl::find("zbd\\only.zbd") == d + "/install/zbd/only.zbd");
  zutl::set_write_dir(d + "/saves");
  EXPECT(zutl::find("configs\\a.mw3") == d + "/saves/configs/a.mw3");
  EXPECT(zutl::write_path("configs\\b.mw3") == d + "/saves/configs/b.mw3");
  EXPECT(zutl::find("configs\\none.mw3").empty());
}

TEST(music_tracks_found_through_the_mounts) {
  SavedMounts keep;
  const std::string d = temp_dir("music");
  touch(d + "/install/music/track03.ogg");
  touch(d + "/saves/music/track02.flac");
  touch(d + "/saves/music/track03.wav");
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(d + "/install");
  zutl::set_write_dir(d + "/saves");
  mech3::MountFiles files;
  zsnd_cd::set_backend(&files, nullptr, "music");
  EXPECT(zsnd_cd::track_path(2) == "music/track02.flac");
  EXPECT(zsnd_cd::track_path(3) == "music/track03.ogg");  // .ogg is asked for first
  EXPECT(zutl::find(zsnd_cd::track_path(3).c_str()) == d + "/install/music/track03.ogg");
  EXPECT(zsnd_cd::track_path(4).empty() && zsnd_cd::track_path(1).empty());
  zsnd_cd::close();
  EXPECT(zsnd_cd::open({}));
  EXPECT(zsnd_cd::state().num_tracks == 3);  // the highest track is the disc's count
  zsnd_cd::close();
  zsnd_cd::set_backend(nullptr, nullptr, "music");
}

namespace {
// What the program gives the engine, without a window.
struct FakeWindow : platform::Window {
  std::string title;
  bool shown = false;
  uint32_t create(const std::string& t, int, int) override { return title = t, 0x1234; }
  void set_title(const std::string& t) override { title = t; }
  void show() override { shown = true; }
  void client_size(int* w, int* h) override { *w = 640, *h = 480; }
};
struct FakeAlert : platform::Alert {
  std::vector<std::string> shown;
  void abort_message(const std::string& m) override { shown.push_back(m); }
};
struct FakeClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 16; }
};
struct FakeScreen : platform::Screen2D {
  int frames = 0;
  void present_16(const uint16_t*, int w, int h, int, bool) override { frames += w == 640 && h == 480; }
};
// Quits after `passes` waits for a message: the loop runs that many passes.
struct CountingPump : platform::MessagePump {
  int passes = 0, limit = 200;
  bool quit = false;
  std::function<void()> on_quit;  // unset: the mission's cleanup
  std::function<bool()> stop;     // set: quits as soon as it says so
  bool peek() override { return ++passes > limit || (stop && stop()); }
  // The quit: a mission still up is cleaned up first, as a debriefing would,
  // while zSound is still there (ExitInstance shuts it down next; a running
  // animation's sounds would be stopped after it).
  bool pump() override {
    if (on_quit) on_quit();
    else mech3::missions().cleanup(false);
    return false;
  }
  void wait(uint32_t) override {}
  void post_quit() override { quit = true; }
};
}  // namespace

TEST(program_starts_like_the_game_to_the_main_state) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("program_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    EXPECT(alert.shown.empty());
    EXPECT(window.title == "MechWarrior 3 1.2");  // message 7 and the version
    EXPECT(window.shown);
    EXPECT(app.fmv() == 0);
    EXPECT(engine.video_mode() == 1);              // Direct3D: the GPU is the 3D card
    EXPECT(mech3::options().hw_card_flag == 1 && mech3::options().hw_card_dev == 0);
    EXPECT(zsnd::find("menu_select") == nullptr);  // sets load later, by the screens
    CountingPump pump;
    app.run(pump);
    EXPECT(app.current() == &mech3::states().main);  // Intro -> SPLASH -> Main
    EXPECT(engine.unported_counts().count("0x00598cc0") == 1);  // the movies are hooked
    // The main menu's screen is ported (mainmenu.md): MAINMENU loaded, its
    // buttons bound, the version line set; the menus it opens are hooks.
    EXPECT(engine.unported_counts().count("0x00486260") == 0);
    const mech3::MainMenu* menu = mech3::main_menu();
    EXPECT(menu != nullptr);
    if (menu) {
      EXPECT(menu->buttons[0].base_x == 20 && menu->buttons[0].base_y == 170);  // INSTANTACTION
      EXPECT(menu->buttons[6].base_x == 432 && menu->buttons[6].base_y == 395);  // QUIT
      EXPECT(menu->buttons[0].max_radius == 200 && menu->buttons[0].wavelength == 12.0f);
      EXPECT(std::string(menu->version.text) == "MechWarrior 3 1.2");
      EXPECT(menu->head != nullptr && menu->active == 1);
    }
    EXPECT(engine.unported_counts().count("0x00488bc0") == 1);  // the Action State's screen
    // The pilot screen (menus.md, The pilot screen) is made with its state:
    // PILOTDIALOG of the main menu's tree, no pilots yet; a name typed makes one.
    auto* ps = dynamic_cast<mech3::PilotState*>(engine.menu_state_of(engine.peek32(0x609858)));
    EXPECT(ps != nullptr && ps->screen != nullptr);
    if (ps && ps->screen) {
      mech3::PilotScreen& s = *ps->screen;
      EXPECT(s.pilots.dialog == &s && s.pilots.size == 6 && s.pilots.rows.size() == 6);
      EXPECT(s.games.dialog == &s && s.games.size == 14 && s.new_bar.editor.size == 0x10);
      EXPECT(s.rename_bar.dialog == &s && (s.rename_bar.flags & ztwod::kHidden) != 0);
      EXPECT(s.pilots.item_count() == 0 && s.chosen == nullptr);
      s.new_bar.set_text("Ace");
      s.new_bar.on_return();
      EXPECT(s.pilots.item_count() == 1 && s.chosen != nullptr && s.chosen == mech3::find_pilot("Ace"));
      EXPECT(zutl::exists("pilots\\Ace.mw3"));
      // The save after a mission (campaign.md 4, the Win State's 0x00478f10(1))
      // on the chosen pilot: Op 1 Mission 2's game filed and current, zSave's
      // world save still a hook.
      mech3::Pilot* ace = mech3::find_pilot("Ace");
      mech3::Pilot* was = mech3::current_pilot();
      mech3::current_pilot() = ace;
      ace->select_game(mech3::kCampaign);
      const int32_t n = ace->campaign_count();
      engine.pilot_record_mission(engine.pilot(), 1);
      mech3::CampaignGame* next = ace->campaign();
      EXPECT(ace->campaign_count() == n + 1 && next && next->operation() == 1 && next->mission_number() == 2);
      EXPECT(next && next->b0 == 1 && next->store.count() == ace->campaigns[0]->store.count());
      EXPECT(engine.unported_counts().count("0x005a8a70") == 1);
      mech3::current_pilot() = was;
    }
    // The controls screen (menus.md section 6) is made with its state: the
    // shipped dialog, group 0's commands, the seven groups, the axis popup.
    EXPECT(engine.unported_counts().count("0x0042a590") == 0);
    const mech3::Handle cs = engine.state_screen(engine.peek32(0x609870));
    auto* ctl = dynamic_cast<mech3::ControlsScreen*>(engine.screen_of(cs));
    EXPECT(ctl != nullptr);
    if (ctl) {
      EXPECT(ctl->group_cycle.count() == 7 && ctl->group_cycle.index == 0);
      EXPECT(!ctl->commands.strings.empty() && ctl->commands.first == 0);
      EXPECT(ctl->key_a.strings.size() == ctl->commands.strings.size());
      EXPECT(ctl->commands.dialog == ctl && ctl->assign_axis.dialog == ctl);
      EXPECT(ctl->axis_popup.popup != nullptr && ctl->axis_popup.popup->sources.count() == 9);
      EXPECT(engine.popup_state_of(cs + 0x16760) == &ctl->axis_popup);
      EXPECT(engine.popup_state_of(cs + 0x1677c) == &ctl->load_save_popup);
      EXPECT(engine.state_screen(cs + 0x16760) == ctl->axis_popup.screen_);
      // The load/save popup: its screen adopted, the bar and the list bound.
      mech3::LoadSavePopupScreen* ls = ctl->load_save_popup.popup;
      EXPECT(ls != nullptr && engine.state_screen(cs + 0x1677c) == ctl->load_save_popup.screen_);
      if (ls) {
        EXPECT(ls->bar.dialog == ls && ls->files.dialog == ls && ls->bar.editor.size == 0x14);
        EXPECT(ls->files.size == 7 && ls->files.rows.size() == 7 && ls->files.down != nullptr);
      }
    }
    EXPECT(screen.frames > 0);                       // the shell flips its 2D frame
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
}

// The campaign from the menus (campaign.md section 7, item 5): the main
// menu's NEW CAMPAIGN, a new pilot accepted, the Load state's ported
// briefing (brief_c11: its script, its map, the buttons once the load ends),
// LAUNCH, and c1 mission 1's Core frames with the pilot's own mech.
TEST(campaign_menus_to_c1m1_through_the_briefing_headless) {
  if (!have_game()) return;
  // Twice: with zfix bugs on (the rebuild) and off around the win (the exe's
  // b4 copy; off for the whole run the exe's own crashes come back).
  for (const bool fix : {true, false}) {
  const char* pilot_name = fix ? "Recruit" : "Recruit2";  // (the pilots stay between tests: new names)
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("campaign_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    int stage = 0, core_frames = 0, briefing_frames = 0;
    size_t steps = 0, objects = 0;
    std::string section_first;
    bool launch_enabled = false, lab_disabled = false, playing = false, debrief_seen = false, continue_help = false;
    int32_t second_number = 0;
    mech3::Handle debrief_handle = 0;
    bool debrief_freed = false, player_card = false, rank_text = false, player_armor = false;
    int32_t debrief_score = -1, mechs_before = 0;
    size_t debrief_texts = 0;
    size_t second_steps = 0;
    bool lab_up = false, lab_left = false;
    int32_t lab_mechs = -99;
    mech3::LaunchedMech mech;
    engine.on_present = [&](int) {
      if (stage == 0 && app.current() == &mech3::states().main && mech3::main_menu()) {
        const_cast<mech3::MainMenu*>(mech3::main_menu())->buttons[1].on_left_up();  // NEW CAMPAIGN
        stage = 1;
      } else if (stage == 1) {
        auto* ps = dynamic_cast<mech3::PilotState*>(app.current());
        if (!ps || !ps->screen) return;
        ps->screen->new_bar.set_text(pilot_name);  // (the pilots stay between tests: a new name)
        ps->screen->new_bar.on_return();
        ps->screen->on_button(0x18, 0, 0);  // ACCEPT
        stage = 2;
      } else if (stage == 2) {
        mech3::BriefingScreen* b = mech3::briefing_screen();
        if (!b || app.current() != &mech3::states().load) return;
        if (++briefing_frames == 1) {
          playing = b->script.playing != 0;  // the Load state started it
          steps = b->script.steps.size();
          objects = b->objects.size();
        }
        if (!b->loaded) return;
        launch_enabled = b->launch_btn.enabled != 0;
        lab_disabled = b->lab_btn.enabled == 0;  // Op 1 Mission 1
        b->on_button(mech3::BriefingScreen::kBtnLaunch, 0, 0);
        stage = 3;
      } else if (stage == 3 && app.current() == &mech3::states().core) {
        if (++core_frames == 30) {
          mech3::launched_mech(&mech);
          if (mech3::CampaignGame* cg = mech3::current_pilot() ? mech3::current_pilot()->campaign() : nullptr)
          {
            cg->c4 = 123456, cg->b4[1] = 7;  // a score and a lancemate's kills, carried through
            // A lancemate's slot (no mech: c1m1 has no lancemate on a card), so
            // the Win State copies two cards' numbers: card 1 has no record line.
            cg->lancemates.push_back(new mech3::GameSlot(-1));
            cg->x9c = 2;
            for (const mech3::ConfigItem* it : cg->store.items) mechs_before += it->kind() == 0 ? it->count : 0;
          }
          // Won (campaign.md section 1): Exit Mission brings the end screen's
          // fade to the debriefing's Win State.
          mech3::missions().ended = 1;
          zfix::options().bugs = fix;  // the Win State's copy of the scores
          mech3::missions().on_command(0x71);
          stage = 4;
        }
      } else if (stage == 4 && app.current() == &mech3::states().end_win) {
        auto* d = dynamic_cast<mech3::DebriefScreen*>(engine.screen_of(mech3::states().end_win.screen_));
        if (!d) return;
        zfix::options().bugs = true;  // (the Win State has copied them)
        debrief_seen = true;
        continue_help = !d->continue_btn.help.empty() && d->continue_btn.help[0]->text[0] != 0;
        debrief_handle = mech3::states().end_win.screen_;
        debrief_score = d->score;
        debrief_texts = d->texts.size();
        player_card = d->cards[0].vehicle != 0 && mech3::debrief_lance_count() >= 1;
        // The card's armor display (6c), from the ported mech's loadout.
        player_armor = d->cards[0].display && d->cards[0].display->most > 0;
        rank_text = !d->rank.labels.empty() && d->rank.labels[0]->text[0] != 0;
        d->on_button(mech3::DebriefScreen::kContinue, 0, 0);
        stage = 5;
      } else if (stage == 5 && app.current() == &mech3::states().load) {
        mech3::BriefingScreen* b = mech3::briefing_screen();
        if (!b || !b->loaded) return;
        second_number = engine.launch().number;
        debrief_freed = engine.screen_of(debrief_handle) == nullptr && mech3::states().end_win.screen_ == 0;
        second_steps = b->script.steps.size();
        b->on_button(mech3::BriefingScreen::kBtnMechLab, 0, 0);  // the Lab State (mechlab.md item 1)
        stage = 6;
      } else if (stage == 6 && app.current() == &mech3::states().lab) {
        const mech3::LabState& lab = mech3::states().lab;
        mech3::LabScreen* ls = engine.lab_screen(lab.lab_screen_);
        // The real screen (mechlab.md step 2): slot 0 shown, its mech put in
        // the lab's store, the chassis page up, the lance's buttons bound.
        lab_up = lab.pilot_ == engine.pilot() && lab.store_ != 0 && lab.slots_ >= 1 && lab.slot_ == 0 && ls &&
                 ls->page == 0 && ls->tabs[6].id == 6 && ls->mechs[0].id == 8 && ls->mechs[0].mech == 3;
        auto mechs_in = [&](const mech3::Store* st) {
          int32_t n = 0;
          for (const mech3::ConfigItem* it : st->items) n += it->kind() == 0 ? it->count : 0;
          return n;
        };
        if (mech3::Store* st = engine.store_of(lab.store_)) lab_mechs = mechs_in(st);
        if (ls) ls->on_button(6, 0, 0);  // ACCEPT
        if (mech3::Store* st = engine.store_of(lab.store_)) lab_mechs -= mechs_in(st);
        stage = 7;
      } else if (stage == 7 && app.current() == &mech3::states().load) {
        lab_left = mech3::states().lab.pilot_ == 0 && mech3::states().lab.store_ == 0;
        stage = 8;
      }
    };
    CountingPump pump;
    pump.limit = 1500;
    app.run(pump);
    EXPECT(stage == 8);
    EXPECT(lab_up && lab_left);  // MECHLAB opened the ported Lab State and screen, ACCEPT came back
    EXPECT(lab_mechs == 1);      // ACCEPT took the shown slot's mech back out of the lab's store
    EXPECT(engine.unported_counts().count("0x0044b920") == 0);  // the screen is the port
    EXPECT(engine.unported_counts().count("0x00448830") == 0);
    EXPECT(debrief_seen);   // the ported debriefing
    EXPECT(continue_help);  // its texts (Mech3Msg.dll's)
    EXPECT(debrief_freed);  // the Win State's leave freed it (shell_clear on 0x006099a0)
    // The results: the campaign's score (nothing scored in 30 frames), the
    // player's card, the lines, the rank (123456: rank 1).
    EXPECT(debrief_score == 123456 && debrief_texts > 4 && player_card && rank_text);
    EXPECT(player_armor);
    EXPECT(second_number == 2 && second_steps > 2);  // c1 mission 2's briefing, its script
    EXPECT(briefing_frames > 0 && steps > 2 && objects > 0 && playing);  // the voice, the markers, the map
    EXPECT(launch_enabled && lab_disabled);
    EXPECT(core_frames >= 30 && mech.parts > 0);
    mech3::Pilot* rec = mech3::find_pilot(pilot_name);
    mech3::CampaignGame* c = rec ? rec->campaign() : nullptr;
    EXPECT(c && c->operation() == 1 && c->mission_number() == 2);  // the Win State's save made it current
    EXPECT(c && c->c4 == 123456);  // the Win State's copy of the score, into the ported campaign
    // A card without a record line: the exe writes its 0 over the record;
    // FIX (bug), behind bugs, it keeps the lancemate's career kills.
    EXPECT(c && c->b4[1] == (fix ? 7 : 0));
    // 0x0047ae10's won path: the player's mech (whole) into the campaign's
    // +0x64 store, which the next mission's game adds to its own store.
    int32_t mechs_after = 0;
    if (c)
      for (const mech3::ConfigItem* it : c->store.items) mechs_after += it->kind() == 0 ? it->count : 0;
    // ... and mission 2's briefing fits the lance to it: the player's slot
    // (mech 3 from the campaign's start, 0x004733b0) takes it back out.
    EXPECT(mechs_after == mechs_before);
    EXPECT(c && c->slot.mech == 3 && c->slot.config);
    EXPECT(engine.peek32(0x60861c) == engine.handle_of(rec));  // the chosen pilot, not a stand-in
    // A game whose Load state is not ported (Instant Action): no mission
    // left over from the campaign, back to the main menu.
    if (rec) {
      rec->select_game(mech3::kInstantAction);
      engine.load_state_enter();
      EXPECT(!engine.launch().active);
      rec->select_game(mech3::kCampaign);
    }
    EXPECT(mech3::missions().current == 0);  // c1's world
    EXPECT(engine.unported_counts().count("0x00405230") == 0);  // the briefing is the port
    EXPECT(engine.unported_counts().count("0x0047b430") == 0);  // and the debriefing
    EXPECT(engine.unported_counts().count("0x00479440") == 0);  // the Win State's campaign is the launch's
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
  }
  zfix::options().bugs = true;
}

// The debug launch of c1 mission 1 without a window (docs/mission-start.md):
// the mission load reaches the mission start (0x0049e810), which reads
// player.zrd's gravities and aiv.zrd's vehicles, finds each one's compiled
// net and makes its AI controller (docs/ai.md, The AI controls). The AI mech
// (owens_01, an Owens on net 4) is built on its controller and powered down
// (p12's flag); the other vehicles are not ported (each constructor counted
// once), so their controllers are made and deleted. The weapons load
// (0x00583880) reads weapons.zrd and makes the weapon_tick object.
TEST(mission_start_reads_c1m1_vehicles_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_start_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    mech3::missions().start();
    EXPECT(!mech3::missions().failed);
    const mech3::PlayerState& s = mech3::player_state();
    EXPECT(s.gravity == 40.0f && s.water_gravity == 9.0f && s.quicksand_gravity == 3.0f);  // player.zrd
    EXPECT(s.tick != 0);
    const auto* tick = static_cast<const zcls::Node*>(engine.object_of(s.tick));
    EXPECT(tick && tick->action && tick->get<int32_t>(0x44) == 2);  // the player's tick, priority 2
    // weapons.zrd's definitions, and the weapon_tick object (priority 3).
    EXPECT(zwep::state().defs.size() > 30 && zwep::find_def("ERPPC") != nullptr);
    const auto* wt = static_cast<const zcls::Node*>(zwep::state().tick);
    EXPECT(wt && wt->action && wt->get<int32_t>(0x44) == 3);
    const auto& n = engine.unported_counts();
    auto count = [&](const char* k) { return n.count(k) ? n.at(k) : 0u; };
    EXPECT(count("0x004d8750") == 0);  // the AI controllers are the port's
    EXPECT(count("0x004e71c0") == 0);
    EXPECT(count("0x004589d0") == 3);  // mobilehq_01..03, over net 2
    EXPECT(count("0x004ac320") == 4);  // ttruck_01, ttruck_02, apc_01, apc_02
    EXPECT(count("0x004ac860") == 1);  // the stealth entity's block (the vehicles' controllers are made)
    EXPECT(count("0x00414150") == 0);  // the mech is the port's
    EXPECT(count("0x004161c0") == 0);  // its placement is the port's
    // owens_01: the AI mech, its controller and brain.
    int32_t made = 0;
    const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
    EXPECT(made == 1);
    if (made == 1) {
      EXPECT(std::string(ai[0].name) == "owens_01");
      mech3::LaunchedMech m;
      EXPECT(mech3::describe_mech(ai[0].mech, &m));
      EXPECT(m.roster == 11 && m.parts == 11 && m.weapons > 0);  // an Owens (its model has no hands)
      EXPECT(m.team == 4);                                          // team 2: mask 4
      EXPECT(m.powering_down);                                      // p12 ("owens" 0 1)
      // On the ground at its aiv.zrd place (-1767.7 3 -1229, yaw 30 degrees):
      // the terrain there is at 3, the mech 0.01 above it, in the world.
      EXPECT(m.in_world);
      EXPECT(m.pos[0] == -1767.7f && m.pos[2] == -1229.0f);
      EXPECT(m.pos[1] == static_cast<float>(static_cast<double>(ground_under(m.pos[0], 103.0f, m.pos[2])) - kPlaceSink));
      EXPECT(std::fabs(m.yaw_sin - std::sin(0.52359879f / 2)) < 1e-6f);
      if (zcls::Node* w = mech3::world_hooks().world) {
        float probe[3] = {m.pos[0], 103.0f, m.pos[2]};
        static zcls::GroundHits hits;
        zcls::ground_query(w, probe, 1, 103.0f, &hits);
        EXPECT(hits.count >= 1 && hits.hit[0].height == 3.0f);
      }
      const auto* c = static_cast<const mech3::AiControls*>(ai[0].controls);
      EXPECT(c->owner == ai[0].mech);  // the mech's constructor set it
      EXPECT(c->view == 2 && c->moves[1] && c->throttle_returns);   // 0x004265a0
      const mech3::ai::Brain* b = c->brain.get();
      EXPECT(b != nullptr);
      if (b) {
        EXPECT(b->params.kind() == 4 && b->params.team() == 2);    // the record copied
        EXPECT(b->params.mech() == 11 && b->params.skin() == 7);
        EXPECT(b->params.vlist && !b->params.vlist->empty());
        EXPECT(b->params.get<float>(0x1c) == 1000.0f * 1000.0f);   // params' first, squared
        EXPECT(b->orders.size() == 1);                              // one order, on net 4
        if (b->orders.size() == 1) {
          const mech3::ai::Order& o = b->orders.back();
          const auto* link = dynamic_cast<const mech3::ai::NetLink*>(o.net.get());
          EXPECT(o.owner == b && link && dynamic_cast<const mech3::ai::SharedState*>(o.state.get()));
          EXPECT(link && link->net() && link->net()->get_id() == 4 && !link->builtin);
          EXPECT(o.net->order == &o && o.state->order == &o);
        }
        EXPECT(b->order.owner == b);
        EXPECT(b->firing.brain == b && b->firing.aim && b->firing.phase == 8);
        EXPECT(b->radio == nullptr);  // owens_01 has no sound_tag
        EXPECT(b->step.next == mech3::ai::null_step());
        EXPECT(b->steer.near_dist == 20.0f && b->steer.x38 > 0.7071f && b->steer.x38 < 0.7072f);
      }
    }
    // valves.zrd (docs/ai.md section 13): c1 mission 1's triggers; bramp's
    // four records (wake owens_01, two delayed updates, a radio line), and
    // setting it wakes the Owens and starts the two timers.
    EXPECT(count("0x004e2120") == 0);
    mech3::ai::Valve* bramp = mech3::ai::valve_find("bramp");
    mech3::ai::Composite* c = bramp && bramp->chain ? bramp->chain->as_composite() : nullptr;
    EXPECT(c && c->items.size() == 4);
    if (c && c->items.size() == 4) {
      EXPECT(static_cast<mech3::ai::ValveAction*>(c->items.front())->kind == mech3::ai::ValveAction::Shutdown);
      EXPECT(static_cast<mech3::ai::ValveAction*>(c->items.back())->kind == mech3::ai::ValveAction::SoundOnce);
    }
    const mech3::ai::Valve* any = mech3::ai::valve_find("inside_c1m1");
    EXPECT(any != nullptr);
    if (bramp && made == 1) {
      bramp->set(1);
      mech3::LaunchedMech m;
      EXPECT(mech3::describe_mech(ai[0].mech, &m) && !m.powering_down);  // the trigger woke it
      const mech3::ai::Composite* timers =
          mech3::ai::valve_timers() ? mech3::ai::valve_timers()->as_composite() : nullptr;
      EXPECT(timers && timers->items.size() == 2);
    }
    // targets.zrd (docs/ai.md section 14): the satellite dish, 35 health,
    // its triggers comcntr_hit below 35 and comcntr_down at 0.
    {
      const auto& targets = mech3::ai::target_list();
      const mech3::ai::CZAiTarget* dish = nullptr;
      for (const mech3::ai::CZAiTarget* t : targets)
        if (t->node && std::string(static_cast<const zcls::Node*>(engine.object_of(t->node))->name()) == "satdish")
          dish = t;
      EXPECT(dish != nullptr);
      if (dish) {
        EXPECT(dish->desc.health == 35.0f && dish->desc.max_health == 35.0f && dish->desc.destroy_anim != 0);
        EXPECT(dish->desc.triggers && dish->desc.triggers->size() == 2);
        EXPECT(dish->team == 4);  // team 2
        mech3::ai::CZAiTarget* d = const_cast<mech3::ai::CZAiTarget*>(dish);
        mech3::ai::Valve* hit = mech3::ai::valve_find("comcntr_hit");
        mech3::ai::Valve* down = mech3::ai::valve_find("comcntr_down");
        EXPECT(hit && down && hit->get() == 0 && down->get() == 0);
        d->on_collide(10.0f);
        EXPECT(hit && hit->get() == 1 && down && down->get() == 0 && !d->destroyed);
        d->on_collide(25.0f);
        EXPECT(down && down->get() == 1 && d->destroyed && d->desc.health == 0.0f);
      }
    }
    mech3::missions().cleanup(false);  // 0x00491780: the valves and targets end
    EXPECT(mech3::ai::target_list().empty());
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
}

// The debug launch of c1 mission 1 without a window (docs/mech-build.md
// section 9): after the load the briefing's Launch (0x00491150) builds the
// player's mech, a Mad Cat in its first configuration, from mechlib.zbd's
// model, the definition and the motions: every part on its node, its
// weapons mounted, its armor, ten heat sinks, its height from the model,
// the skis and the AMS, on the player's team (aiv.zrd's player record).
TEST(mech_build_launches_the_player_c1m1_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mech_build_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, mission 1, the Mad Cat
    engine.set_launch(l);
    engine.missions_start(0x6098d0);  // the load, then the launch (our debug addition)
    EXPECT(!mech3::missions().failed);
    mech3::LaunchedMech m;
    EXPECT(mech3::launched_mech(&m));
    EXPECT(m.roster == 9);
    EXPECT(m.parts == 13);    // hip, torso, head, the arms and hands, thighs, calves and feet
    EXPECT(m.weapons > 0);    // its first loadout's weapons on their firepoints
    EXPECT(m.sinks >= 10);
    EXPECT(m.toes > 0);
    EXPECT(m.height > 5.0f && m.height < 20.0f);  // the model's box, not the default 12
    EXPECT(m.max_speed > 10.0f);  // the engine's top speed
    EXPECT(m.torso_armor_scale > 2.77f && m.torso_armor_scale < 2.78f);  // 36 points in the centre torso: 100 / 36
    EXPECT(m.team == 2);      // team 1 (aiv.zrd's player record): mask 2
    EXPECT(m.skis && m.ams);
    EXPECT(m.motions);        // motion.zbd's walk, and the step-walk blend
    mech3::missions().cleanup(false);  // 0x00491780, as the debrief would
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
}

namespace {
// Observers on the action lists, around the two ticks (docs/action-lists.md):
// each records its place and what the ticks have done so far.
struct TickWatch {
  const mech3::GameEngine* engine = nullptr;
  std::vector<std::string> ran;
  uint32_t player_ticks() const { return mech3::player_tick_count(); }  // player_tick's runs (0x004a1160)
  void add(const char* what) { ran.push_back(std::string(what) + ":" + std::to_string(player_ticks())); }
};
TickWatch g_watch;
zcls::Node* watcher(int32_t priority, zcls::NodeAction fn, bool back) {
  zcls::Node* n = zcls::object3d_create();
  zcls::node_set_priority(n, priority);
  back ? zcls::node_set_action_back(n, fn) : zcls::node_set_action(n, fn);
  return n;
}
}  // namespace

// The debug launch of c1 mission 1 through the Load and Core states without a
// window (docs/action-lists.md): each Core frame runs zClass's action lists
// (0x004f7d80), so the player's tick (priority 2, 0x004a1160) and weapon_tick
// (priority 3, zwep::tick) run once a frame, in that order, between watchers
// put before and after each on their lists. The earlier tests' missions are
// gone: their stop took zClass down (0x004f8f40), so list 2 holds this
// mission's one tick object.
TEST(mission_c1m1_frames_run_the_ticks_in_order_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_frames_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    g_watch = TickWatch{};
    g_watch.engine = &engine;
    int scenes = 0;
    uint32_t ticks_at_start = 0, tick_objects = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;  // headless: no 3D scene is drawn
      if (scenes++ == 0) {  // the mission runs: the watchers join the lists
        ticks_at_start = g_watch.player_ticks();
        const auto* tick = static_cast<const zcls::Node*>(engine.object_of(mech3::player_state().tick));
        for (zcls::ListItem* it = zcls::lists().list[2].head; it; it = it->next)
          tick_objects += tick && it->node && it->node->action == tick->action;
        watcher(1, [](zcls::Node*) { g_watch.add("1"); }, false);
        watcher(2, [](zcls::Node*) { g_watch.add("2<"); }, false);
        watcher(2, [](zcls::Node*) { g_watch.add("2>"); }, true);
        watcher(3, [](zcls::Node*) {
          g_watch.add("3<");
          zwep::run_state().zone = 0x55, zwep::run_state().tick_zone = 0;  // the weapon tick copies it
        }, false);
        watcher(3, [](zcls::Node*) { g_watch.add(zwep::run_state().tick_zone == 0x55 ? "3>" : "3>no-tick"); }, true);
        watcher(4, [](zcls::Node*) { g_watch.add("4"); }, false);
      }
    };
    CountingPump pump;
    pump.limit = 400;
    app.run(pump);
    EXPECT(ticks_at_start > 0);  // the player's tick ran before the watchers came
    EXPECT(tick_objects == 1);
    // Every frame since: 1, then the player's tick between 2< and 2>, the
    // weapon tick between 3< and 3>, then 4.
    const size_t frames = g_watch.ran.size() / 6;
    EXPECT(frames >= 3);
    EXPECT(g_watch.ran.size() == frames * 6);
    for (size_t f = 0; f < frames && f * 6 + 5 < g_watch.ran.size(); ++f) {
      const std::string t = std::to_string(ticks_at_start + f * tick_objects);
      const std::string t1 = std::to_string(ticks_at_start + (f + 1) * tick_objects);
      const std::string* r = &g_watch.ran[f * 6];
      EXPECT(r[0] == "1:" + t && r[1] == "2<:" + t && r[2] == "2>:" + t1);
      EXPECT(r[3] == "3<:" + t1 && r[4] == "3>:" + t1 && r[5] == "4:" + t1);
    }
    mech3::missions().cleanup(false);  // 0x00491780, as the debrief would
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
}

// c1 mission 1's Direct3D frames keep the mechs' projected shadows (0x004814e0,
// docs/render-shadows.md): the player's mech a 64x64 caster drawn along the
// sun onto its centre ski's ground, every other mech one only within 100 m.
TEST(mission_c1m1_mechs_cast_shadows_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_shadows_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0, mechs = 0, wrong = 0;
      bool sun = false, player = false, player_drawn = false, others_32 = true;
      bool cockpit = false, cockpit_parented = false;
      int view_id = -1;
    } seen;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      seen.sun = mech3::mission_sun() != nullptr;
      void* player = mech3::mech_mission_player();
      float at[3] = {};
      if (!player || !mech3::mech_world_position(player, at)) return;
      if (auto* c = static_cast<zcls::Node*>(mech3::mech_cockpit_node(player))) {
        seen.cockpit = true;
        seen.cockpit_parented = c->parent_count() > 0;
      }
      seen.view_id = mech3::program_view_mode_id();
      for (const auto h : mech3::ai::entity_list()) {
        void* m = engine.object_of(h);
        if (!mech3::entity_is_mech(m)) continue;
        ++seen.mechs;
        const auto* c = static_cast<const zmod::ShadowCaster*>(mech3::mech_shadow(m));
        float pos[3] = {};
        mech3::mech_world_position(m, pos);
        const float dx = at[0] - pos[0], dy = at[1] - pos[1], dz = at[2] - pos[2];
        const bool near = m == player || dx * dx + dy * dy + dz * dz < 10000.0f;
        if ((c != nullptr) != near) ++seen.wrong;
        if (!c) continue;
        const auto* img = static_cast<const zimg::Texture*>(c->image);
        if (!img || !c->enabled) {
          ++seen.wrong;
          continue;
        }
        if (m != player) {
          seen.others_32 = seen.others_32 && img->width == 32 && img->height == 32;
          continue;
        }
        seen.player = img->width == 64 && img->height == 64 && c->node;
        // Something of the mech drawn: pixels other than the corner's.
        const auto* px = static_cast<const uint16_t*>(img->pixels);
        int shaded = 0;
        for (int i = 0; px && i < 64 * 64; ++i) shaded += px[i] != px[0];
        seen.player_drawn = shaded > 100;
      }
    };
    CountingPump pump;
    pump.limit = 200;
    app.run(pump);
    EXPECT(seen.frames > 10);
    EXPECT(seen.sun);
    EXPECT(seen.mechs > 0);
    EXPECT(seen.wrong == 0);
    EXPECT(seen.player);
    EXPECT(seen.player_drawn);
    EXPECT(seen.others_32);
    // Playtest #4: in the cockpit view (camera id 1) the type's cockpit model
    // (0x00430ad0) hangs under the torso, and the frame's cockpit passes ran
    // (0x004f73f0, 0x004eb170) on the program's objects instead of the hooks.
    EXPECT(seen.cockpit && seen.cockpit_parented);
    EXPECT(seen.view_id == 1);
    EXPECT(engine.unported_counts().count("0x004f73f0") == 0);
    EXPECT(engine.unported_counts().count("0x004eb170") == 0);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  EXPECT(zmod::caster_list() == nullptr);  // every caster freed with its mech
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
}

// c1 mission 1 through the Load and Core states (docs/mission-frame.md): each
// Core frame runs the missions' frame (0x0048f470), which advances the game
// time [0x0060991c] by the frame's step and ticks the valves' timers. bramp,
// set on the first frame, wakes the Owens and starts its two delayed
// updates: truck_go after 18 s and c1m1dialogue4_play after 22 s, each set
// on the first frame its time is past. The cleanup (0x00491780) ends the
// valves and puts the game time back to 0.
namespace {
struct FastClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 60; }
};
}  // namespace

TEST(mission_c1m1_frames_fire_bramps_timers_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_timers_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FastClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    using mech3::ai::valve_find;
    struct Seen {
      int frames = 0;
      float start = -1, last = 0;          // the game time at bramp's set, and the frame before
      float truck = -1, dialogue = -1;     // the game time on the frame each was set
      float truck_before = -1, dialogue_before = -1;
      bool time_steps = true;              // each frame's time is the last plus the frame's step
      bool woke = false;
    } seen;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      const float now = mech3::missions().time;
      if (seen.frames > 0 && now != seen.last + engine.frame_dt()) seen.time_steps = false;
      if (seen.frames == 0) {
        if (mech3::ai::Valve* b = valve_find("bramp")) {
          b->set(1);
          seen.start = mech3::mech_game_time();
          int32_t made = 0;
          const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
          mech3::LaunchedMech m;
          seen.woke = made == 1 && mech3::describe_mech(ai[0].mech, &m) && !m.powering_down;
        }
      }
      const mech3::ai::Valve* t = valve_find("truck_go");
      const mech3::ai::Valve* d = valve_find("c1m1dialogue4_play");
      if (t && t->get() == 1 && seen.truck < 0) seen.truck = now, seen.truck_before = seen.last;
      if (d && d->get() == 1 && seen.dialogue < 0) seen.dialogue = now, seen.dialogue_before = seen.last;
      seen.last = now;
      ++seen.frames;
    };
    CountingPump pump;
    pump.limit = 1200;
    // The cleanup, checked at the quit (before ExitInstance): the valves end,
    // the game time goes back to 0.
    bool loaded = false, cleaned = false, cleared = false, timers_done = false, time_copy = false;
    pump.on_quit = [&] {
      time_copy = mech3::mech_game_time() == mech3::missions().time;  // the ports' copy of [0x0060991c]
      timers_done = mech3::ai::valve_timers() == nullptr;               // both done, the list gone
      loaded = mech3::missions().loaded == 1;
      cleaned = mech3::missions().cleanup(false);
      cleared = valve_find("bramp") == nullptr && mech3::missions().time == 0.0f;
    };
    app.run(pump);
    EXPECT(seen.frames > 200);
    EXPECT(seen.start >= 0 && seen.woke);
    EXPECT(seen.time_steps);
    EXPECT(time_copy);
    // Each fired on the frame its time was first past, not before.
    EXPECT(seen.truck > seen.start + 18.0f && !(seen.truck_before > seen.start + 18.0f));
    EXPECT(seen.dialogue > seen.start + 22.0f && !(seen.dialogue_before > seen.start + 22.0f));
    EXPECT(timers_done);
    EXPECT(loaded && cleaned && cleared);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// c1 mission 1 through the Load and Core states (docs/ai.md section 15): the
// launch puts the player's Mad Cat and the Owens on zEntity's list, the
// mission start sorts it (0x004d5ca0: the player first), and each Core frame
// the sensors (0x004a8cd0) fill the contact lists, nearest first: the
// player's holds the Owens and the targets.zrd objects within its 1000 m,
// the Owens' holds the player; the target keys pick from them.
// The program's Direct3D frame (render-frame.md): the locked back buffer is
// the 16-bit layer filled with the key, laid over the frame at the unlock
// (FIX (A)); the fade's colour through the ports, the Direct3D quad or the
// software finish (whose MMX rows leave a 640-wide screen's last four
// columns); the stills' handles reused.
TEST(frame_lock_lays_the_layer_and_the_tint_goes_through_the_ports) {
  struct LayerScreen : platform::Screen2D {
    int overlays = 0;
    uint16_t key = 0, first = 0, second = 0;
    void present_16(const uint16_t*, int, int, int, bool) override {}
    void overlay_16(const uint16_t* px, int, int, int, bool, uint16_t k, const uint8_t*, const uint16_t*) override {
      ++overlays, key = k, first = px[0], second = px[1];
    }
  };
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  LayerScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  const zvid::PixelFormat keep = zvid::pixel_format();
  zvid::set_pixel_format(5, 6, 5);
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    engine.poke32(0x72f348, 1);
    engine.video_begin_3d();
    EXPECT(zrndr::target2d().base == reinterpret_cast<uint8_t*>(engine.buffer16()));
    EXPECT(engine.buffer16()[0] == mech3::GameEngine::kLayerKey && engine.buffer16()[1] == mech3::GameEngine::kLayerKey);
    engine.buffer16()[0] = 0x1234;
    engine.video_end_3d();
    EXPECT(screen.overlays == 1 && screen.key == mech3::GameEngine::kLayerKey && screen.first == 0x1234 &&
           screen.second == mech3::GameEngine::kLayerKey);
    engine.screen_tint(0, 0, 0.5);  // Direct3D: the quad over the locked size, half black
    const zvid::TintQuad& q = zvid::tint_quad();
    EXPECT(q.pending == 1 && q.verts[2].sx == 640.0f && q.verts[2].sy == 480.0f && q.verts[0].diffuse == 0x7f000000u);
    engine.video_draw_3d();
    EXPECT(q.pending == 0);

    engine.poke32(0x72f348, 0);  // software: kept, then the finish
    engine.video_lock();
    std::fill(engine.buffer16(), engine.buffer16() + 640 * 480, uint16_t{0xffff});
    engine.screen_tint(0, 0, 0.5);
    engine.render_finish();
    EXPECT(engine.buffer16()[0] == 0x7bef && engine.buffer16()[635] == 0x7bef && engine.buffer16()[636] == 0xffff &&
           engine.buffer16()[640 * 479 + 1] == 0x7bef);
    zrndr::tint_state() = zrndr::TintState{};

    const mech3::Handle g1 = engine.video_grab(0), g2 = engine.video_grab(2);
    EXPECT(g1 && g2 && g1 != g2);
    engine.image_free(g1);
    EXPECT(engine.video_grab(0) == g1);  // freed: its handle comes back, nothing leaks
  }
  zvid::pixel_format() = keep;
}

// The player's view (docs/view.md): after the launch the view is on the
// Mad Cat in the options' camera, the cockpit (attach, id 1). Driven from the
// keyboard as mission_c1m1_player_walks_and_turns_headless, every Core frame
// camera1 sits at the torso node plus the cockpit's offset (0, 2.45, -1.3
// turned with the mech): walking, the camera goes along; turning, the offset
// turns with it.
TEST(mission_c1m1_camera_follows_the_cockpit_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_view_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0, followed = 0;
    bool cockpit = false, on_player = false, near = true;
    float first[3] = {}, walked[3] = {}, off_walked[3] = {}, off_turned[3] = {};
    uint32_t up = 0, left = 0;
    auto dist = [](const float* a, const float* b) {
      return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      float cam[3], torso[3];
      const bool have = mech3::program_view_positions(cam, torso);
      if (frames >= 2 && frames <= 360) {
        if (!have || dist(cam, torso) > 4.0f) near = false;
        else ++followed;
      }
      if (frames == 1) {
        up = zin::key1_of(0x18) & 0xff, left = zin::key1_of(2) & 0xff;  // throttle up, turn left
      } else if (frames == 2) {
        cockpit = mech3::program_view_mode_id() == 1;
        on_player = mech3::program_view_target() == engine.object_of(engine.field32(engine.pilot(), 0x14));
        std::memcpy(first, cam, sizeof first);
      } else if (frames == 3) {
        keys.key(up, true);
      } else if (frames == 300) {
        keys.key(up, false);
        std::memcpy(walked, cam, sizeof walked);
        for (int i = 0; i < 3; ++i) off_walked[i] = cam[i] - torso[i];
        keys.key(left, true);
      } else if (frames == 360) {
        keys.key(left, false);
        for (int i = 0; i < 3; ++i) off_turned[i] = cam[i] - torso[i];
      }
    };
    CountingPump pump;
    pump.limit = 900;
    app.run(pump);
    EXPECT(frames >= 360);
    EXPECT(cockpit && on_player);
    EXPECT(near && followed == 359);
    EXPECT(dist(walked, first) > 20.0f);  // walked, and the camera with it
    // Turned: the offset's level part turned with the mech.
    const float a0 = std::atan2(off_walked[0], off_walked[2]), a1 = std::atan2(off_turned[0], off_turned[2]);
    EXPECT(std::fabs(a1 - a0) > 0.05f);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

TEST(mission_c1m1_sensors_fill_the_contacts_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_sensors_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0;
      size_t entities = 0;
      bool player_first = false, sorted = false;
      int32_t player_n = 0, owens_n = 0;
      bool player_sees_owens = false, owens_hidden = false, owens_sees_player = false, nearest_first = true;
      bool owens_in_sight = false, picked = false, kept = false;
      bool player_up = false, player_placed = false, far_sensed = false, near_distance = false;
    } seen;
    mech3::Handle player = 0, owens = 0;
    int fade_frames = 0;  // the mission start's fade in (0x0048eea0) runs first
    engine.on_present = [&](int) {
      if (app.current() == &mech3::states().fade && seen.frames == 0) ++fade_frames;
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {  // as launched: 1433 m apart, beyond the player's 1000 m
        player = engine.field32(engine.pilot(), 0x14);
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        owens = made == 1 ? engine.handle_of(ai[0].mech) : 0;
        const std::vector<mech3::Handle>& list = mech3::ai::entity_list();
        seen.entities = list.size();
        seen.player_first = !list.empty() && list[0] == player;
        seen.sorted = mech3::player_get_60b56c() == 1;
        mech3::SensedContact c[64];
        const int32_t n = mech3::sensed_contacts(engine.object_of(player), c, 64);
        for (int32_t i = 0; i < n && i < 64; ++i) seen.far_sensed |= c[i].seen == owens;
        // The spawn left the player powered down (0x004782f0); the fade's end,
        // Core's resume(7), powered it up (0x0041cc30).
        mech3::LaunchedMech pm;
        seen.player_up = mech3::describe_mech(engine.object_of(player), &pm) && !pm.powering_down;
        // aiv y -35.9: the pick's reach (2y + 10) takes nothing, so the lowest
        // polygon, the only one there (mission-start.md section 11), before
        // the spawn's one update step lets the skis settle it.
        const float* at = mech3::spawn_position();
        seen.player_placed = pm.in_world && at[0] == -3200.9f && at[2] == -1225.1f &&
                             at[1] == static_cast<float>(static_cast<double>(ground_under(at[0], 64.1f, at[2])) - kPlaceSink) &&
                             std::fabs(pm.pos[1] - at[1]) < 0.05f;
        // Walked up: moved (vtable +0xdc) to the ground 300 m short of the Owens.
        const float near[3] = {-2067.7f, -35.79f, -1229.0f};
        mech3::move_mech(engine.object_of(player), near, 0.0f);
      } else if (seen.frames == 4) {  // the Owens powered down: sensed, not in sight
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        mech3::SensedContact c[64];
        seen.player_n = mech3::sensed_contacts(engine.object_of(player), c, 64);
        for (int32_t i = 0; i < seen.player_n && i < 64; ++i) {
          if (i > 0 && c[i].distance < c[i - 1].distance) seen.nearest_first = false;
          if (c[i].seen == owens) {
            seen.player_sees_owens = true, seen.owens_hidden = !c[i].in_sight;
            seen.near_distance = c[i].distance > 290.0f && c[i].distance < 310.0f;
          }
        }
        if (made == 1) {
          seen.owens_n = mech3::sensed_contacts(ai[0].mech, c, 64);
          for (int32_t i = 0; i < seen.owens_n && i < 64; ++i) seen.owens_sees_player |= c[i].seen == player;
        }
        if (mech3::ai::Valve* b = mech3::ai::valve_find("bramp")) b->set(1);  // wakes the Owens
      } else if (seen.frames == 6) {  // awake: in sight, and Ctrl+E (0x004d6a80) picks it
        mech3::SensedContact c[64];
        const int32_t n = mech3::sensed_contacts(engine.object_of(player), c, 64);
        for (int32_t i = 0; i < n && i < 64; ++i) seen.owens_in_sight |= c[i].seen == owens && c[i].in_sight;
        if (mech3::ai::AiTarget* t = mech3::ai::sensor_hooks().fields(player)) {
          t->nearest_enemy(0);
          const int32_t k = mech3::selected_contact(engine.object_of(player));
          seen.picked = k >= 0 && k < n && c[k].seen == owens;
        }
      } else if (seen.frames == 7) {  // the pick holds over a frame: the sensors find it again
        const int32_t k = mech3::selected_contact(engine.object_of(player));
        mech3::SensedContact c[64];
        const int32_t n = mech3::sensed_contacts(engine.object_of(player), c, 64);
        seen.kept = k >= 0 && k < n && c[k].seen == owens;
      }
    };
    CountingPump pump;
    pump.limit = 400;
    app.run(pump);
    EXPECT(fade_frames > 1);
    EXPECT(seen.frames >= 7);
    EXPECT(seen.entities == 2 && seen.player_first && seen.sorted);
    EXPECT(!seen.far_sensed);
    EXPECT(seen.player_sees_owens && seen.owens_hidden && seen.owens_sees_player && seen.nearest_first);
    EXPECT(seen.near_distance);
    EXPECT(seen.owens_in_sight && seen.picked && seen.kept);
    EXPECT(seen.player_up);
    EXPECT(seen.player_placed);
    EXPECT(engine.unported_counts().count("0x004782f0") == 0);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// The registry as a file (docs/settings.md): a regedit export reads as the
// registry would, values the game does not know survive a save, and a saved
// InGameVMode of 10 (1600x1200, not offered by the menu) comes back.
TEST(settings_file_reads_an_export_and_keeps_what_the_game_saved) {
  const std::string dir = temp_dir("settings");
  const std::string path = dir + "/settings.reg";
  const char text[] =
      "Windows Registry Editor Version 5.00\r\n\r\n"
      "[HKEY_CURRENT_USER\\Software\\MicroProse\\MechWarrior 3\\1.0]\r\n"
      "\"InGameVMode\"=dword:0000000a\r\n"
      "\"MouseAccelX\"=hex:00,00,\\\r\n  00,40\r\n"
      "\"Somebody's\"=\"a string\"\r\n"
      "\"Kept\"=dword:12345678\r\n"
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\MicroProse\\MechWarrior 3\\1.0]\r\n"
      "\"InstallOptions\"=dword:00000101\r\n";
  {  // as regedit writes it: UTF-16LE with a byte-order mark
    std::FILE* f = std::fopen(path.c_str(), "wb");
    std::fputc(0xff, f), std::fputc(0xfe, f);
    for (const char* p = text; *p; ++p) std::fputc(*p, f), std::fputc(0, f);
    std::fclose(f);
  }
  platform::RegFile reg(path);
  const std::string key = "SOFTWARE\\MicroProse\\MechWarrior 3\\1.0";
  EXPECT(reg.open(key, true, false));
  uint32_t size = 0, v = 0;
  EXPECT(reg.query(true, "ingamevmode", &size) && size == 4);
  EXPECT(reg.read(true, "InGameVMode", &v, &size) && v == 10);
  float f = 0;
  size = 4;
  EXPECT(reg.read(true, "MouseAccelX", &f, &size) && f == 2.0f);
  EXPECT(!reg.query(true, "Somebody's", &size));  // a string value: not the game's
  EXPECT(reg.query(false, "InstallOptions", &size) && size == 4);
  reg.close(true);

  zreg::Machine m;
  m.cpu_mhz = 2000;
  zreg::init("MicroProse", "MechWarrior 3", "1.0", m, 0x101);
  zreg::set_settings(&reg);
  mech3::options() = mech3::Options{};
  EXPECT(mech3::load_options(nullptr) == 1);
  EXPECT(mech3::options().in_game_vmode == 10);
  EXPECT(mech3::options().mouse_accel[0] == 2.0f);
  mech3::options().sound_frequency = 0.25f;
  EXPECT(zreg::write_all() == 1);

  platform::RegFile again(path);
  EXPECT(again.open(key, true, false));
  EXPECT(again.query(true, "Kept", &size));
  size = 4;
  EXPECT(again.read(true, "SoundVolume", &f, &size) && f == 0.25f);
  EXPECT(again.read(true, "InGameVMode", &v, &size) && v == 10);
  zreg::set_settings(nullptr);
  zreg::shutdown();
  mech3::options() = mech3::Options{};
}

// c1 mission 1 driven from the keyboard (docs/movement.md): the spawn's step
// and Core's power-up (0x0041cc30 from resume(7)) leave the Mad Cat powered
// up; the throttle-up key, held through zInput, walks it forward along the
// terrain (its feet on the ground), and a turn key turns it.
TEST(mission_c1m1_player_walks_and_turns_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_drive_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0;
      bool powered_up = false;
      float off_ground = 0;
      mech3::LaunchedMech start, walked, turned;
      bool feet = true;
      // The mission animations that run only near the player (flag 2): their
      // clocks (+0xa8) at frame 3 and 300, and the player the tests measure from.
      std::vector<std::pair<int32_t, float>> ranged;
      int32_t ranged_moved = 0;
      bool player_set = false;
      float player_gap = 1e9f;
    } seen;
    void* player = nullptr;
    uint32_t up = 0, left = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 1) {
        player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        up = zin::key1_of(0x18) & 0xff, left = zin::key1_of(2) & 0xff;  // throttle up, turn left
        mech3::describe_mech(player, &seen.start);
        const zeff::AnimState& a = zeff::anim();
        for (int32_t i = 1; i < a.def_count(); ++i)
          if (a.defs[i].get<uint32_t>(0x94) & 2) seen.ranged.push_back({i, a.defs[i].get<float>(0xa8)});
      } else if (seen.frames == 3) {
        // Powered up: the spawn's +0xde cleared by Core's resume(7) through
        // the port (0x0041cc30 never logged).
        mech3::LaunchedMech m;
        seen.powered_up = mech3::describe_mech(player, &m) && !m.powered_down && !m.powering_down &&
                          engine.unported_counts().count("0x0041cc30") == 0;
        seen.feet = m.feet_down;
        keys.key(up, true);
      } else if (seen.frames > 3 && seen.frames < 300) {
        // Both skis down every frame, the body on the ground under it.
        mech3::LaunchedMech m;
        if (!mech3::describe_mech(player, &m) || !m.feet_down) seen.feet = false;
        const float g = ground_under(m.pos[0], m.pos[1] + 20.0f, m.pos[2]);
        seen.off_ground = std::max(seen.off_ground, std::fabs(m.pos[1] - g));
      } else if (seen.frames == 300) {
        const zeff::AnimState& a = zeff::anim();
        for (const auto& [i, t] : seen.ranged) seen.ranged_moved += a.defs[i].get<float>(0xa8) != t;
        seen.player_set = a.block.get<int32_t>(0x24) == 1;
        mech3::LaunchedMech now;
        mech3::describe_mech(player, &now);
        float d2 = 0;
        for (int k = 0; k < 3; ++k) {
          const float d = a.block.get<float>(0x2c + 4 * k) - now.pos[k];
          d2 += d * d;
        }
        seen.player_gap = std::sqrt(d2);
        keys.key(up, false);
        mech3::describe_mech(player, &seen.walked);
        keys.key(left, true);
      } else if (seen.frames == 360) {
        keys.key(left, false);
        mech3::describe_mech(player, &seen.turned);
      }
    };
    CountingPump pump;
    pump.limit = 900;
    mech3::mech_parts_fx_log().clear();  // capped at 4,096 lines for the whole run
    app.run(pump);
    EXPECT(seen.frames >= 360);
    EXPECT(mech3::spawn_updates() > 0);  // the spawn's step (0x004782f0) ran the frame
    EXPECT(seen.powered_up);
    const float dx = seen.walked.pos[0] - seen.start.pos[0], dz = seen.walked.pos[2] - seen.start.pos[2];
    const float d = std::sqrt(dx * dx + dz * dz);
    EXPECT(d > 20.0f);  // walked
    // Along its heading: forward is -z turned by the yaw (a quaternion about y).
    const float yaw = 2.0f * std::asin(seen.start.yaw_sin);
    EXPECT(d > 0 && (dx * -std::sin(yaw) + dz * -std::cos(yaw)) / d > 0.95f);
    EXPECT(seen.feet);
    EXPECT(seen.off_ground < 0.1f);  // 0.024 m at most on c1m1's shore
    EXPECT(std::fabs(seen.turned.yaw_sin - seen.walked.yaw_sin) > 0.05f);
    // The feet's outside calls (PartCalls, docs/movement-ground.md): each
    // plant plays the soil's step sound, on dry ground leaves a print, in
    // the shallows starts the wake.
    std::map<std::string, int> fx;
    for (const std::string& e : mech3::mech_parts_fx_log()) ++fx[e.substr(0, e.find(' '))];
    // The pilot's update (0x004783a0) gives zEffect the player each frame, so
    // the animations that run only near the player tick (they froze before).
    EXPECT(seen.player_set && seen.player_gap < 1.0f);
    EXPECT(!seen.ranged.empty() && seen.ranged_moved > 0);
    // The mission's start animation (mission-load.md) ran to its end (status 3).
    const zeff::AnimDef* start = zeff::anim_find("start_single_player_m1");
    EXPECT(start && start->raw[0x98] == 3);
    EXPECT(fx["step"] > 8);  // 13: ten seconds at a walk
    EXPECT(fx["print"] > 0 && fx["wake"] > 0);  // 2 and 6 on c1m1's shore
    EXPECT(fx["pilot"] > 0);  // the engine and walk loops (Mech::movement_sounds)
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// c1 mission 1's guns (docs/weapons.md, In the program): the Mad Cat, moved
// 80 m west of the powered-down Owens and turned to face it, holds the fire
// key; its weapons fire through the manager into zWeapon, the shots (beams
// and projectiles) cross the Owens through the shot queries, its damage
// handler (0x004161a0) takes the hits into Mech::TakeDamage, and its armor
// drops; firing heats the Mad Cat.
TEST(mission_c1m1_player_fires_on_the_owens_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_fire_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0;
      mech3::LaunchedMech owens_before, owens_after, player_before, player_peak;
      int32_t flying_max = 0, hot_group = -1, hits_before = 0, hits_after = 0;
      bool hot_fired = false;
      void* attacker = nullptr;
    } seen;
    void* player = nullptr;
    void* player_node = nullptr;
    void* owens = nullptr;
    uint32_t fire = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        player_node = mech3::mech_node(player);  // read now: the cleanup at the quit frees the mech
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        owens = made == 1 ? ai[0].mech : nullptr;
        fire = zin::key1_of(0x3d) & 0xff;  // CMD_FIRE
        // 80 m west of the Owens, on the ground, facing east (+x: yaw -90).
        mech3::LaunchedMech o;
        mech3::describe_mech(owens, &o);
        const float x = o.pos[0] - 80.0f, z = o.pos[2];
        const float at[3] = {x, ground_under(x, o.pos[1] + 50.0f, z), z};
        mech3::move_mech(player, at, -1.5707964f);
        mech3::describe_mech(owens, &seen.owens_before);
        mech3::describe_mech(player, &seen.player_before);
        seen.hits_before = mech3::mech_hits_taken(nullptr);
      } else if (seen.frames == 6) {
        keys.key(fire, true);
      } else if (seen.frames == 120) {
        // The first group (the machine guns) makes no heat: one that does.
        seen.hot_group = mech3::mech_select_hot_group(player);
        seen.player_before = seen.player_peak;  // the heat before the hot group
      } else if (seen.frames > 6 && seen.frames < 200) {
        // The weapons aim where the HUD's reticle ray ends (0x0041cd40).
        zwep::PoolStats ps;
        zwep::pool_stats(&ps);
        if (ps.flying > seen.flying_max) seen.flying_max = ps.flying;
        mech3::LaunchedMech m;
        if (mech3::describe_mech(player, &m) && m.heat > seen.player_peak.heat) seen.player_peak = m;
        if (seen.frames > 120 && mech3::mech_group_reloading(player, seen.hot_group)) seen.hot_fired = true;
      } else if (seen.frames == 200) {
        keys.key(fire, false);
        mech3::describe_mech(owens, &seen.owens_after);
        seen.hits_after = mech3::mech_hits_taken(&seen.attacker);
      }
    };
    CountingPump pump;
    pump.limit = 700;
    // The cleanup at the quit kills every live shot (0x00584fe0).
    int live_before = 0, live_after = 0;
    auto live = [] {
      int k = 0;
      for (zwep::WeaponDef& d : zwep::state().defs)
        for (zwep::Projectile* q = d.active; q; q = q->next) ++k;
      return k;
    };
    pump.on_quit = [&] {
      // One shot in flight, as a weapon's spawn leaves it (pool_take, then
      // onto its definition's active list).
      if (zwep::WeaponDef* d = zwep::find_def("LRM10"))
        if (zwep::Projectile* q = zwep::pool_take(d)) q->next = d->active, d->active = q;
      live_before = live();
      mech3::missions().cleanup(false);
      live_after = live();
    };
    app.run(pump);
    EXPECT(live_before > 0 && live_after == 0);
    EXPECT(seen.frames >= 200);
    EXPECT(seen.flying_max > 0);                                // shots in the air
    EXPECT(seen.owens_after.armor < seen.owens_before.armor);  // hit
    EXPECT(seen.hits_after > seen.hits_before);                // through the Owens' damage handler
    EXPECT(seen.attacker == player_node);                      // by the Mad Cat's shots
    EXPECT(seen.hot_group >= 0 && seen.hot_fired);             // the selected group fired
    EXPECT(seen.player_peak.heat > seen.player_before.heat);   // firing heats
    // What the shots showed and played (docs/weapon-effects.md): the machine
    // guns' muzzle flash and looping fire sound, their ricochet and the hit
    // on a mech, the laser's muzzle and impact.
    const std::set<std::string> fx(mech3::weapon_fx_log().begin(), mech3::weapon_fx_log().end());
    EXPECT(fx.count("anim mac_muz.flt") && fx.count("sound wep_mgun_fire"));     // on fire
    EXPECT(fx.count("anim impact_mgun_mech") && fx.count("sound wep_ricochet"));  // on the hit
    EXPECT(fx.count("anim laser_blue_muz.flt") && fx.count("anim laser_impact_blue.flt"));
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// A survey, not a check (run with MW3_SURVEY=1): c1 mission 1 played for
// 600 Core frames, the player walking forward and firing, then every
// unported call the run reached with its count (the c1m1 backlog,
// docs/worklogs/game.md).
TEST(mission_c1m1_unported_survey_headless) {
  if (!have_game() || !std::getenv("MW3_SURVEY")) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_survey_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    uint32_t up = 0, fire = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (frames == 3) {
        up = zin::key1_of(0x18) & 0xff, fire = zin::key1_of(0x3d) & 0xff;
        keys.key(up, true);
      } else if (frames == 40) {
        keys.key(up, false);
        keys.key(fire, true);
      } else if (frames == 400) {
        keys.key(fire, false);
      }
    };
    CountingPump pump;
    pump.limit = 900;
    mech3::mech_parts_fx_log().clear();  // capped at 4,096 lines for the whole run
    app.run(pump);
    std::vector<std::pair<uint32_t, std::string>> list;
    for (const auto& [k, n] : engine.unported_counts()) list.push_back({n, k});
    std::sort(list.rbegin(), list.rend());
    std::map<std::string, int> parts;
    for (const std::string& e : mech3::mech_parts_fx_log()) ++parts[e];
    for (const auto& [k, n] : parts) std::printf("  parts %5d %s\n", n, k.c_str());
    std::printf("survey: %d core frames, %zu unported calls\n", frames, list.size());
    for (const auto& [n, k] : list) std::printf("  %8u %s\n", n, k.c_str());
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The left mouse button fires (CMD_TRIGGER1), with the mouse as DirectInput
// has it: exclusive and foreground, so refused while the window is not in
// front. The game starts unfocused, the window then gains the focus (the
// frame's OnActivate), and a held left button sets the weapons going.
TEST(mission_c1m1_left_button_fires_after_focus_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_fire_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    bool focused = false, mouse_acquired = false, mouse_lost = false, left = false;
    uint32_t coop[3] = {};
    int acquires = 0, refusals = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard || d == kMouse; }
    void close(Device) override {}
    void set_cooperative_level(Device d, uint32_t f) override { coop[d] = f; }
    int32_t acquire(Device d) override {
      if (d != kMouse) return 0;
      ++acquires;
      if ((coop[d] & 5) == 5 && !focused) return ++refusals, kNotAcquired;  // DISCL_EXCLUSIVE | FOREGROUND
      mouse_acquired = true, mouse_lost = false;
      return 0;
    }
    int32_t unacquire(Device d) override {
      if (d == kMouse) mouse_acquired = false;
      return 0;
    }
    void focus(bool on) {
      focused = on;
      if (!on && mouse_acquired && (coop[kMouse] & 5) == 5) mouse_acquired = false, mouse_lost = true;
    }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState* m) override {
      if (!mouse_acquired) return mouse_lost ? kInputLost : kNotAcquired;
      *m = platform::MouseState{};
      m->buttons[0] = left ? 0x80 : 0;
      return 0;
    }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0;
      mech3::LaunchedMech owens_before, owens_after, player_before, player_peak;
      int32_t flying_max = 0, hot_group = -1, hits_before = 0, hits_after = 0;
      bool hot_fired = false;
      void* attacker = nullptr;
    } seen;
    void* player = nullptr;
    void* player_node = nullptr;
    void* owens = nullptr;
    uint32_t fire = 0;
    // The message loop: 40 passes after the first frame the window comes to
    // the front (SdlPump's focus event: the frame's OnActivate, the device's focus).
    struct FocusPump : CountingPump {
      std::function<void()> front;
      int since = -1;
      bool peek() override {
        if (since >= 0 && ++since == 40 && front) front();
        return CountingPump::peek();
      }
    } pump;
    pump.limit = 900;
    pump.front = [&] {
      app.on_window_activate(1);
      keys.focus(true);
    };
    bool unfocused = false;
    engine.on_present = [&](int) {
      if (!unfocused) {  // the game starts behind another window (the terminal)
        unfocused = true;
        keys.focus(false);
        app.on_window_activate(0);
        pump.since = 0;
      }
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        player_node = mech3::mech_node(player);  // read now: the cleanup at the quit frees the mech
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        owens = made == 1 ? ai[0].mech : nullptr;
        fire = zin::key1_of(0x3d) & 0xff;  // CMD_FIRE
        // 80 m west of the Owens, on the ground, facing east (+x: yaw -90).
        mech3::LaunchedMech o;
        mech3::describe_mech(owens, &o);
        const float x = o.pos[0] - 80.0f, z = o.pos[2];
        const float at[3] = {x, ground_under(x, o.pos[1] + 50.0f, z), z};
        mech3::move_mech(player, at, -1.5707964f);
        mech3::describe_mech(owens, &seen.owens_before);
        mech3::describe_mech(player, &seen.player_before);
        seen.hits_before = mech3::mech_hits_taken(nullptr);
      } else if (seen.frames == 10) {
        keys.left = true;           // and the player clicks
      } else if (seen.frames == 120) {
        // The first group (the machine guns) makes no heat: one that does.
        seen.hot_group = mech3::mech_select_hot_group(player);
        seen.player_before = seen.player_peak;  // the heat before the hot group
      } else if (seen.frames > 6 && seen.frames < 200) {
        // The HUD's reticle on the Owens' middle (0x0041cd40 is not ported).
        mech3::LaunchedMech o;
        mech3::describe_mech(owens, &o);
        const float at[3] = {o.pos[0], o.pos[1] + o.height * 0.5f, o.pos[2]};
        mech3::mech_aim_at(player, at);
        zwep::PoolStats ps;
        zwep::pool_stats(&ps);
        if (ps.flying > seen.flying_max) seen.flying_max = ps.flying;
        mech3::LaunchedMech m;
        if (mech3::describe_mech(player, &m) && m.heat > seen.player_peak.heat) seen.player_peak = m;
        if (seen.frames > 120 && mech3::mech_group_reloading(player, seen.hot_group)) seen.hot_fired = true;
      } else if (seen.frames == 200) {
        keys.left = false;
        mech3::describe_mech(owens, &seen.owens_after);
        seen.hits_after = mech3::mech_hits_taken(&seen.attacker);
      }
    };
    app.run(pump);
    std::printf("  mouse: %d acquires, %d refused; trigger on the left button: %u\n", keys.acquires, keys.refusals,
                zin::mouse_of(0x3d));
    EXPECT(seen.frames >= 200);
    EXPECT(seen.flying_max > 0);                                // shots in the air
    EXPECT(seen.owens_after.armor < seen.owens_before.armor);  // hit
    EXPECT(seen.hits_after > seen.hits_before);                // through the Owens' damage handler
    EXPECT(seen.attacker == player_node);                      // by the Mad Cat's shots
    EXPECT(seen.hot_group >= 0 && seen.hot_fired);             // the selected group fired
    EXPECT(seen.player_peak.heat > seen.player_before.heat);   // firing heats
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// c1 mission 1's keys through the mech's command handler (0x0041be80,
// docs/controls.md, The mech's commands): F2 selects the second weapon
// group; the power key powers the Mad Cat down, and again back up; the jump
// key, held, fires the jump jets.
TEST(mission_c1m1_player_commands_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_commands_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::player_controls_instance() = nullptr;  // nor the player's controls (leaked): its keys registered again
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    l.mech = 17;                  // the Thor: its first configuration has jump jets
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0, down_at = 0, up_at = 0;
      mech3::LaunchedMech start, f2, jump_start;
      float climb = 0;
      int32_t jets_firing = 0;
      bool powering = false, stayed_down = true, flame = false, flame_after = false;
    } seen;
    void* player = nullptr;
    uint32_t f2 = 0, power = 0, jump = 0;
    auto tap = [&](uint32_t k) { keys.key(k, true), keys.key(k, false); };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      mech3::LaunchedMech m;
      if (seen.frames > 3) mech3::describe_mech(player, &m);
      if (seen.frames == 3) {
        player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        f2 = zin::key1_of(0x2d) & 0xff, power = zin::key1_of(0x3f) & 0xff, jump = zin::key1_of(0xc) & 0xff;
        mech3::describe_mech(player, &seen.start);
        tap(f2);
      } else if (seen.frames == 6) {
        seen.f2 = m;
        tap(power);
      } else if (seen.frames > 6 && seen.frames < 300) {
        seen.powering |= m.powering_down;
        if (!seen.down_at && m.powered_down) seen.down_at = seen.frames;
        if (seen.down_at && seen.frames < seen.down_at + 30 && !m.powered_down) seen.stayed_down = false;
        if (seen.down_at && seen.frames == seen.down_at + 30) tap(power);
        if (seen.down_at && !seen.up_at && !m.powering_down) seen.up_at = seen.frames;
      } else if (seen.frames == 300) {
        seen.jump_start = m;
        keys.key(jump, true);
      } else if (seen.frames > 300 && seen.frames < 360) {
        seen.climb = std::max(seen.climb, m.pos[1] - seen.jump_start.pos[1]);
        seen.jets_firing = std::max(seen.jets_firing, m.jets_firing);
        seen.flame |= m.jet_flame;  // the jets' flames (0x00432830 on the jet node)
      } else if (seen.frames == 360) {
        keys.key(jump, false);
        std::printf("commands: group %d -> %d, down at %d, up at %d, jets %d firing %d climb %.2f\n", seen.start.group,
                    seen.f2.group, seen.down_at, seen.up_at, m.jets, seen.jets_firing, seen.climb);
      } else if (seen.frames == 400) {
        seen.flame_after = m.jet_flame;
      }
    };
    CountingPump pump;
    pump.limit = 800;
    app.run(pump);
    EXPECT(seen.frames >= 360);
    EXPECT(seen.start.group == 0 && seen.f2.group == 1);  // F2: the second group
    EXPECT(seen.powering && seen.down_at > 0);            // the power key: down
    EXPECT(seen.stayed_down && seen.up_at > seen.down_at + 30);  // until pressed again, then up
    EXPECT(seen.jets_firing > 0 && seen.climb > 1.0f);    // the jump jets lift it
    EXPECT(seen.flame && !seen.flame_after);              // their flames burn, then are put out
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// c1 mission 1's Owens through the entity manager's frame (0x004a1140,
// 0x005a8850; docs/ai.md, The AI mechs' frames): woken by bramp it powers up
// and stands on its feet; held still, the player walking into it is blocked
// by it as a mech (the head-on probe's mech response), not as a wall.
TEST(mission_c1m1_owens_runs_its_frames_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_ai_frames_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::player_controls_instance() = nullptr;  // nor the player's controls (leaked): its keys registered again
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0;
      mech3::LaunchedMech asleep, woke, owens_end, far, near;
      float off_ground = 0, closest = 1e9f;
      bool feet = true, mech_block = false, wall_block = false;
      bool ai_on = false, orders_ok = true, node_order = false;
      int32_t fire_phase = -1, look_phase = -1, group_volleys = 0, firing = 0, shooting = 0;
      int32_t attacked = 0, home_done = 0, home_orders = 0, own_on = 0, let_go = 0;
      bool dodged = false, alongside = false, backed = false;
      float gw_from[2] = {}, gw_closest = 1e9f, let_go_at = 0, gw_moved = 0, gw_close = 0, gw_range = 0;
      float home_way[3] = {}, home_start = -1.0f, home_last = 0, home_range = 0;
      float chase_from = 0, chase_to = 0, fight_min = 0;
      float fire_range = 0, fire_max = 0;
      mech3::ai::Brain* brain = nullptr;
      std::map<std::string, uint32_t> counts0, counts1;
    } seen;
    void* player = nullptr;
    void* owens = nullptr;
    uint32_t up = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      mech3::LaunchedMech o, m;
      if (seen.frames > 3) mech3::describe_mech(owens, &o), mech3::describe_mech(player, &m);
      if (seen.frames == 3) {
        player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        owens = made == 1 ? ai[0].mech : nullptr;
        up = zin::key1_of(0x18) & 0xff;  // throttle up
        mech3::describe_mech(owens, &seen.asleep);
        if (mech3::ai::Valve* b = mech3::ai::valve_find("bramp")) b->set(1);  // wakes the Owens
        mech3::describe_mech(owens, &seen.woke);
        seen.ai_on = mech3::ai::ai_switch();
        seen.brain = made == 1 ? static_cast<mech3::AiControls*>(ai[0].controls)->brain.get() : nullptr;
        // Woken at once: not hit lately (the record's zero counts as a hit in
        // the game's first second, 0x004aed40; play's bramp comes later).
        if (made == 1) static_cast<mech3::AiControls*>(ai[0].controls)->hit.last = -10.0f;
        seen.counts0 = engine.unported_counts();
      } else if (seen.frames > 3 && seen.frames < 200) {
        // The Owens held still from frame 150 (it walks its net, ai.md
        // 17.4a): its speed share (brain +0xec, which bounds the node
        // state's throttle) 0, stopped by frame 200.
        if (seen.frames == 150 && seen.brain) seen.brain->xec = 0.0f;
        // Its brain's frame each tick (ai.md 17.1), its orders never empty.
        if (seen.brain && seen.brain->orders.empty()) seen.orders_ok = false;
        // Its frames run, standing on the ground.
        if (!o.feet_down) seen.feet = false;
        const float g = ground_under(o.pos[0], o.pos[1] + 20.0f, o.pos[2]);
        seen.off_ground = std::max(seen.off_ground, std::fabs(o.pos[1] - g));
      } else if (seen.frames == 200) {
        seen.far = o;  // 1.4 km from the player: no detail, the low-detail model
        seen.counts1 = engine.unported_counts();
        if (seen.brain) seen.fire_phase = seen.brain->firing.phase, seen.look_phase = seen.brain->firing.look.phase;
        if (seen.brain) seen.fire_range = seen.brain->firing.range, seen.fire_max = seen.brain->params.get<float>(0x30);
        if (seen.brain)
          for (const mech3::ai::Order& r : seen.brain->orders)
            if (r.state && r.state->from && r.state->from->kind == mech3::ai::StepKind::Node) seen.node_order = true;
        seen.owens_end = o;
        // 30 m west of the Owens, facing east (+x), walking at it. Its own
        // order off (+0x80) for the bump: the station keeping, checked after
        // the way home, would have it give way.
        if (seen.brain) seen.brain->params.put<int32_t>(0x80, 0);
        const float x = o.pos[0] - 30.0f, z = o.pos[2];
        const float at[3] = {x, ground_under(x, o.pos[1] + 50.0f, z), z};
        mech3::move_mech(player, at, -1.5707964f);
        keys.key(up, true);
      } else if (seen.frames > 200 && seen.frames < 500) {
        if (seen.brain) {
          const auto& f = seen.brain->firing;
          if (f.plan && f.plan->kind == 2) ++seen.group_volleys;
          if (f.phase == 4) ++seen.firing;
          for (int32_t g = 0; g < 5; ++g)
            if (mech3::mech_group_reloading(owens, g)) {
              ++seen.shooting;
              break;
            }
        }
        if (seen.frames == 450) seen.near = o;  // walking at it, 30 m off
        seen.mech_block |= m.blocked[1];
        seen.wall_block |= m.blocked[3];
        const float dx = m.pos[0] - o.pos[0], dz = m.pos[2] - o.pos[2];
        seen.closest = std::min(seen.closest, std::sqrt(dx * dx + dz * dz));
      } else if (seen.frames == 500) {
        keys.key(up, false);
        seen.owens_end.attacker = o.attacker;
        // The attack (docs/ai.md 17.2): the Owens' engaging is off in c1m1
        // (its +0x3c) until the mission's valves say; on here.
        // The Owens' mode (+0x40) is 0 (no behaviour for it): 1 here, as an
        // aiv record can give; the player 160 m east of it.
        if (seen.brain) {
          seen.brain->params.put<int32_t>(0x3c, 1);
          seen.brain->params.put<int32_t>(0x40, 1);
          seen.home_range = seen.brain->params.get<float>(0x28);
          seen.fight_min = seen.brain->params.get<float>(0x2c);
          const float x = o.pos[0] + 160.0f, z = o.pos[2];
          const float at[3] = {x, ground_under(x, o.pos[1] + 120.0f, z), z};
          mech3::move_mech(player, at, -1.5707964f);
        }
      } else if (seen.frames > 500 && seen.brain) {
        auto& br = *seen.brain;
        // The attack's orders under the fight: end, rearm, the way home.
        const size_t n = br.orders.size();
        const float pdx = m.pos[0] - o.pos[0], pdz = m.pos[2] - o.pos[2];
        const float pd = std::sqrt(pdx * pdx + pdz * pdz);
        const bool under = n >= 4 && dynamic_cast<mech3::ai::AttackReturnState*>(br.orders[n - 2].state.get()) &&
                           dynamic_cast<mech3::ai::AttackRearmState*>(br.orders[n - 3].state.get()) &&
                           dynamic_cast<mech3::ai::AttackEndState*>(br.orders[n - 4].state.get());
        // The fight (0x004d1d10): the Owens closes on the player, then the
        // order takes the behaviour for its mode (the vehicle net's slot 2).
        if (!seen.chase_from && under && dynamic_cast<mech3::ai::FightState*>(br.orders[n - 1].state.get()))
          seen.chase_from = pd;
        const auto* top = n ? br.orders[n - 1].state.get() : nullptr;
        if (!seen.attacked && under && top && !dynamic_cast<const mech3::ai::FightState*>(top) && top->from &&
            top->from->kind == mech3::ai::StepKind::VehMode1) {
          seen.chase_to = pd;
          seen.attacked = seen.frames;
          br.pop();  // the mode's behaviour (its state not ported yet) ended: the way home resumes
          // Its waypoint taken (the net's step nearest the Owens, within
          // its +0x28 here), the Owens is put 250 m east of it, facing it.
          for (const auto& ord : br.orders)
            if (auto* r = dynamic_cast<const mech3::ai::AttackReturnState*>(ord.state.get())) {
              const float x = r->way[0] + 250.0f, z = r->way[2];
              const float at[3] = {x, ground_under(x, o.pos[1] + 120.0f, z), z};
              mech3::move_mech(owens, at, 1.5707964f);
            }
        }
        if (seen.attacked && !seen.home_done) {
          const mech3::ai::AttackReturnState* home = nullptr;
          for (const auto& ord : br.orders)
            if (auto* r = dynamic_cast<const mech3::ai::AttackReturnState*>(ord.state.get())) home = r;
          if (home) {
            seen.home_way[0] = home->way[0], seen.home_way[1] = home->way[1], seen.home_way[2] = home->way[2];
            const float dx = o.pos[0] - home->way[0], dz = o.pos[2] - home->way[2];
            const float d = std::sqrt(dx * dx + dz * dz);
            seen.home_start = std::max(seen.home_start, d);  // the farthest, after the move
            seen.home_last = d;
          } else {
            seen.home_done = seen.frames;
            seen.home_orders = static_cast<int32_t>(br.orders.size());
            // The station keeping (0x004cc3e0): engaging off and the own
            // order on (+0x80 1) again; the player 30 m west, walking at it.
            br.params.put<int32_t>(0x3c, 0);
            br.params.put<int32_t>(0x80, 1);
            const float x = o.pos[0] - 30.0f, z = o.pos[2];
            const float at[3] = {x, ground_under(x, o.pos[1] + 80.0f, z), z};
            mech3::move_mech(player, at, -1.5707964f);
            keys.key(up, true);
            seen.gw_from[0] = o.pos[0], seen.gw_from[1] = o.pos[2];
          }
        } else if (seen.home_done && seen.frames > seen.home_done + 1) {
          // The own order on the player (0x004e81a0, within its +0x74, 40 m)
          // keeping station: it swerves while the player closes, runs
          // alongside inside +0x78 x 0.9, lets go past sqrt(+0x74) x 1.1.
          const auto* st = dynamic_cast<const mech3::ai::StationState*>(br.order.state.get());
          if (st) {
            if (!seen.own_on) seen.own_on = seen.frames;
            seen.dodged |= st->count == 8;
            seen.alongside |= st->phase == 3;
            seen.backed |= st->phase == 1;
            seen.gw_close = st->close, seen.gw_range = st->range;
            seen.gw_closest = std::min(seen.gw_closest, pd);
          } else if (seen.own_on && !seen.let_go) {
            seen.let_go = seen.frames;
            seen.let_go_at = pd;
            const float mx = o.pos[0] - seen.gw_from[0], mz = o.pos[2] - seen.gw_from[1];
            seen.gw_moved = std::sqrt(mx * mx + mz * mz);
          }
          if (seen.frames == seen.home_done + 450) keys.key(up, false);
        }
      }
    };
    CountingPump pump;
    pump.limit = 2400;  // the chase (80 m), the way home (250 m), giving way
    app.run(pump);
    EXPECT(seen.frames >= 500);
    // The level of detail (0x0041e870): 1.4 km off, no detail bits and the
    // low-detail model; 30 m off with the player's throttle up, 0x1c (no
    // sounds: the frame's budget is spent while the player moves) and the
    // full model back.
    EXPECT(seen.far.detail == 0 && seen.far.lod == 1);
    EXPECT(seen.near.detail == 0x1c && seen.near.lod == 0);
    EXPECT(seen.asleep.powered_down && !seen.woke.powering_down);  // bramp powers it up
    EXPECT(seen.owens_end.alive - seen.woke.alive > 3.0f);   // a full update each frame since
    EXPECT(seen.feet && seen.off_ground < 0.1f);             // standing on the ground
    EXPECT(seen.mech_block && !seen.wall_block);             // the player blocked by it as a mech
    EXPECT(seen.closest > 3.0f);                             // not walked through
    EXPECT(seen.owens_end.attacker == player);               // the bump hit it (0x00423a20)
    // The brain's frame ran on every tick since, the AI switch on, the
    // orders kept.
    EXPECT(seen.ai_on && seen.brain && seen.orders_ok);
    // Awake (the player in sight) each tick. The firing
    // (ai.md 17.7) picks a plan each tick: the player, 1.4 km off, is past
    // its range, so none, and its sensor range widened to aiv +0x30; it
    // rests in phase 5 with its look-around going.
    EXPECT(seen.fire_phase == 5 && seen.look_phase >= 1);
    EXPECT(seen.fire_max > 0 && seen.fire_range == seen.fire_max);
    // 30 m off, the plan pick takes the player and the volley (0x004b1860)
    // a weapon group; the line clear (0x004d5980), the firing holds the
    // trigger (phase 4) and the group's lasers fire (a beam running, or
    // recharging after).
    EXPECT(seen.group_volleys > 100 && seen.firing > 100 && seen.shooting > 0);
    // The shared behaviour's first run (0x004cbeb0) pushed the step of net 4
    // nearest the Owens on its order: a node step.
    EXPECT(seen.node_order);
    if (std::getenv("MW3_HOME_LOG"))
      std::fprintf(stderr, "GIVE WAY on %d dodged %d alongside %d backed %d closest %g close %g range %g let go %d at %g moved %g\n",
                   seen.own_on, seen.dodged, seen.alongside, seen.backed, seen.gw_closest, seen.gw_close, seen.gw_range,
                   seen.let_go, seen.let_go_at, seen.gw_moved);
    // The player walked at it head on: it swerves, then backs off facing
    // the player (phase 1; beside it, it runs alongside instead), and lets go
    // past its +0x28 (44 m).
    EXPECT(seen.own_on > seen.home_done && seen.dodged && (seen.backed || seen.alongside));
    EXPECT(seen.gw_closest > 15.0f && seen.gw_moved > 10.0f);
    EXPECT(seen.let_go > seen.own_on && seen.let_go_at > seen.gw_range - 1.0f && seen.let_go_at < seen.gw_range + 2.0f);
    // Engaging on, the Owens attacks (0x004e9ca0): the end, rearm and way
    // home orders under the fight. The fight closes from 160 m to between
    // +0x2c (60 m, nearer it backs off) and +0x28 (80 m), then takes the
    // mode's behaviour. That ended and the Owens 250 m off,
    // the way home (0x004cae90) drives it back to its net's nearest step and
    // ends within its +0x28 (80 m), the attack's orders gone with it (end,
    // rearm resumed).
    if (std::getenv("MW3_HOME_LOG"))
      std::fprintf(stderr, "HOME chase %g -> %g at %d; done %d orders %d farthest %g last %g range %g\n",
                   seen.chase_from, seen.chase_to, seen.attacked, seen.home_done, seen.home_orders, seen.home_start,
                   seen.home_last, seen.home_range);
    EXPECT(seen.chase_from > 150.0f && seen.chase_to <= seen.home_range + 1.0f && seen.chase_to >= seen.fight_min - 1.0f);
    EXPECT(seen.attacked > 0 && seen.home_done > seen.attacked);
    EXPECT(seen.home_start > seen.home_range && seen.home_last < seen.home_range + 2.0f);
    EXPECT(seen.home_orders <= 2);
    mech3::missions().cleanup(false);
    EXPECT(!mech3::ai::ai_switch());                         // the cleanup turns it off
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// c4b missions 5 and 6 failed by leaving them (docs/ai.md section 8,
// fixes.md, behind bugs): the player moved out of the area at core frame 5,
// his own vlist trips the fail objective's valve (m6's regions read as
// ..._c4bm7, m5's valve as outside_c4m5). The exe's reading of the same
// records (no region, the other valve) is test_ai_readers.py's; a bugs-off
// launch here would meet the HUD's unset block (hud.cpp) on the second.
TEST(missions_c4b_m5_m6_fail_when_left_headless) {
  struct Case {
    int32_t number;
    float out[2];  // x, z past the mission's outside region
    const char* valve;
  };
  const Case cases[] = {{5, {2020.0f, 500.0f}, "outside_c4m5"}, {6, {1375.0f, -500.0f}, "outside_c4m7"}};
  for (const Case& cs : cases) {
    {

      if (!have_game()) return;
      SavedMounts keep;
      zutl::mounts() = zutl::Mounts{};
      zutl::mount(MW3_GAME_DIR);
      zutl::set_write_dir(temp_dir("mission_c4b_out_write"));
      mech3::reset_states();
      reset_process_state();  // as a new process: zSound not yet initialised
      mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
      mech3::mech_types_forget();             // and no mech type cached
      mech3::player_controls_instance() = nullptr;  // nor the player's controls (leaked): its keys registered again
      mech3::options() = mech3::Options{};
      struct Keys : platform::Input {
        std::vector<platform::KeyEvent> queue;
        uint32_t t = 0;
        void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
        bool open(Device d) override { return d == kKeyboard; }
        void close(Device) override {}
        int32_t acquire(Device) override { return 0; }
        int32_t unacquire(Device) override { return 0; }
        int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
          const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
          std::copy(queue.begin(), queue.begin() + n, out);
          queue.erase(queue.begin(), queue.begin() + n);
          *count = n;
          return 0;
        }
        int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
        int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
      } keys;
      FakeWindow window;
      FakeAlert alert;
      FakeClock clock;
      FakeScreen screen;
      platform::SilentSoundOutput sound;
      mech3::MountFiles files;
      mech3::Platform p;
      p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
      p.input = &keys;
      {
        mech3::GameEngine engine(p, "mech3.exe -nofmv");
        engine.quiet = true;
        mech3::Mech3App app(&engine);
        mech3::set_app(&app);
        EXPECT(app.init_instance());
        mech3::GameEngine::Launch l;
        l.index = 4;  // c4b
        l.number = cs.number;
        engine.set_launch(l);
        app.post_intro_ = &mech3::states().load;
        int frames = 0, ended_at = -1;
        bool early = false, fail_done = false;
        engine.on_present = [&](int) {
          if (app.current() != &mech3::states().core) return;
          ++frames;
          if (frames == 5) {
            early = mech3::missions().ended;  // not failed where it starts
            const float at[3] = {cs.out[0], ground_under(cs.out[0], 2000.0f, cs.out[1]), cs.out[1]};
            mech3::move_mech(mech3::mech_mission_player(), at, 0.0f);
          }
          if (mech3::missions().ended && ended_at < 0) {
            ended_at = frames;
            const mech3::Objectives& o = mech3::mission_objectives();
            fail_done = o.fail && o.fail->done;
          }
        };
        CountingPump pump;
        pump.limit = 150;
        pump.stop = [&] { return ended_at >= 0; };
        app.run(pump);
        if (std::getenv("MW3_SURVEY"))
          std::printf("c4b m%d: frames %d ended at %d, the fail group (%s) done %d\n", cs.number, frames, ended_at,
                      cs.valve, fail_done);
        EXPECT(frames >= 5 && !early);
        EXPECT(ended_at > 5 && ended_at < 10 && fail_done);
        mech3::missions().cleanup(false);
        app.stop_game();
        app.exit_instance();
        mech3::set_app(nullptr);
      }
      zrdr::close_archives(true);
      mech3::reset_states();
      reset_process_state();
      mech3::options() = mech3::Options{};
    }
  }
}

// The lance's orders end to end (docs/ai.md 17.6, item L5): in c1 mission 4
// the player selects Dominic, targets an enemy 400 m from him (he fires at
// it from there already) and gives the lance key 0x54 ("only an enemy");
// Dominic follows it (the follow state on its own net) and closes in,
// firing. Then the player's hits on him for over 3 s turn him on the
// player (0x004d5dc0): his enemies gain the player's team and his allies
// lose it.
TEST(mission_c1m4_dominic_takes_the_order_and_the_lance_turns_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_lance_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::player_controls_instance() = nullptr;  // nor the player's controls (leaked): its keys registered again
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1 mission 4: Dominic with the player
    l.number = 4;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      mech3::ai::Brain* brain = nullptr;
      void* dominic = nullptr;
      void* enemy = nullptr;
      void* player = nullptr;
      mech3::ai::AiTarget *dt = nullptr, *pt = nullptr;
      zgame::Handle dv = 0, ev = 0, pv = 0;
      int frames = 0, ordered_at = -1, hit_from = -1;
      bool lancemate = false, order_taken = false, follows_enemy = false;
      int fired_before = 0, fired_after = 0, on_player = 0, phases[9] = {}, on_enemy = 0;
      float d0 = 0, closest = 1e9f;
      uint32_t enemies_before = 0, enemies_after = 0, allies_after = 0;
    } seen;
    auto dist = [](void* a, void* b) {
      mech3::LaunchedMech x, y;
      mech3::describe_mech(a, &x), mech3::describe_mech(b, &y);
      const float dx = x.pos[0] - y.pos[0], dz = x.pos[2] - y.pos[2];
      return std::sqrt(dx * dx + dz * dz);
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        for (int32_t i = 0; i < made; ++i) {
          const std::string n = ai[i].name ? ai[i].name : "";
          if (n == "Dominic_Paine") {
            seen.dominic = ai[i].mech;
            seen.brain = static_cast<mech3::AiControls*>(ai[i].controls)->brain.get();
          } else if (!seen.enemy && n.rfind("blackhawk", 0) == 0) {
            seen.enemy = ai[i].mech;
          }
        }
        seen.player = mech3::mech_mission_player();
        if (!seen.dominic || !seen.enemy || !seen.brain || !seen.player) return;
        seen.dv = engine.handle_of(seen.dominic), seen.ev = engine.handle_of(seen.enemy);
        seen.pv = engine.handle_of(seen.player);
        seen.dt = mech3::ai::brain_hooks().targetable(seen.dv), seen.pt = mech3::ai::brain_hooks().targetable(seen.pv);
        if (!seen.dt || !seen.pt) return;
        // The enemy (powered down) 400 m from Dominic, on a clear line (the
        // ground blocks others; MW3_LANCE_AT / MW3_LANCE_DIR move it).
        mech3::LaunchedMech d;
        mech3::describe_mech(seen.dominic, &d);
        const float ang = std::getenv("MW3_LANCE_DIR") ? std::strtof(std::getenv("MW3_LANCE_DIR"), nullptr) : 1.5707964f;
        const float far = std::getenv("MW3_LANCE_AT") ? std::strtof(std::getenv("MW3_LANCE_AT"), nullptr) : 400.0f;
        const float x = d.pos[0] + far * std::cos(ang), z = d.pos[2] + far * std::sin(ang);
        const float at[3] = {x, ground_under(x, d.pos[1] + 100.0f, z), z};
        mech3::move_mech(seen.enemy, at, 1.5707964f);
        if (mech3::ai::Valve* w = mech3::ai::valve_find("dominic_wakes")) w->set(1);
        return;
      }
      if (!seen.brain) return;
      mech3::ai::Brain& b = *seen.brain;
      const bool firing = b.firing.phase == 4 || b.firing.phase == 7;
      if (seen.frames == 60) {
        // The player selects Dominic (his saved target) and targets the enemy,
        // then gives order 10 (the lance key 0x54: "only an enemy", 0x004ea390).
        seen.lancemate = (seen.dt->xbc & 0x10) != 0;
        seen.d0 = dist(seen.dominic, seen.enemy);
        // The player's contacts on Dominic and on the enemy (his sensors list
        // neither now), as his target keys would leave them.
        static mech3::ai::Contact on_dominic, on_enemy;
        on_dominic.owner = on_enemy.owner = seen.pv;
        on_dominic.seen = on_dominic.entity = seen.dv;
        on_enemy.seen = on_enemy.entity = seen.ev;
        seen.pt->saved_target = &on_dominic;
        seen.pt->target = &on_enemy;
        mech3::player_command(0x54);  // 0x00426cb0: the controls, then the viewed mech's keys
        seen.pt->saved_target = seen.pt->target = nullptr;  // the test's, gone before the sensors see them
        auto* fo = dynamic_cast<mech3::ai::FollowState*>(b.current().state.get());
        auto* vp = dynamic_cast<mech3::ai::VehiclePart*>(b.current().net.get());
        seen.order_taken = fo && vp && vp->target && vp->target->vehicle == seen.ev;
        seen.ordered_at = seen.frames;
      } else if (seen.frames < 60) {
        if (firing) ++seen.fired_before;
      } else if (seen.frames < 1200) {
        if (firing) ++seen.fired_after;
        if (b.firing.phase >= 0 && b.firing.phase < 9) ++seen.phases[b.firing.phase];
        const mech3::ai::AiTarget* dtt = mech3::ai::brain_hooks().targetable(seen.dv);
        if (dtt && dtt->target && mech3::ai::brain_hooks().contact_vehicle(dtt->target) == seen.ev) ++seen.on_enemy;
        seen.closest = std::min(seen.closest, dist(seen.dominic, seen.enemy));
        auto* fo = dynamic_cast<mech3::ai::FollowState*>(b.current().state.get());
        if (fo) seen.follows_enemy = true;
      } else if (seen.frames == 1200) {
        seen.enemies_before = seen.dt->enemies;
        // The enemy taken 3 km away: his shots at it (their splash on him,
        // another attacker, would start the run of hits again) end.
        mech3::LaunchedMech e;
        mech3::describe_mech(seen.enemy, &e);
        const float far_x = e.pos[0] + 3000.0f, far_z = e.pos[2];
        const float away[3] = {far_x, ground_under(far_x, e.pos[1] + 500.0f, far_z), far_z};
        mech3::move_mech(seen.enemy, away, 0.0f);
        seen.hit_from = seen.frames;
      } else if (seen.frames < 1600) {
        // The player's hits on Dominic, one a frame for 400 frames (about 6 s):
        // the run of hits passes 3 s.
        mech3::mech_controls(seen.dominic)->on_hit(seen.player);
      } else if (seen.frames == 1600) {
        seen.enemies_after = seen.dt->enemies;
        seen.allies_after = seen.dt->allies;
        if (std::getenv("MW3_LANCE_LOG")) {
          const mech3::ai::HitRecord& r = mech3::mech_controls(seen.dominic)->hit;
          std::fprintf(stderr, "HITS last %g start %g by %x (player %x) run %g\n", r.last, r.run_start, r.by, seen.pv,
                       r.run());
        }
        // The player 30 m from Dominic, nearer than the enemy: his next pick.
        mech3::LaunchedMech d;
        mech3::describe_mech(seen.dominic, &d);
        const float x = d.pos[0] - 30.0f, z = d.pos[2];
        const float at[3] = {x, ground_under(x, d.pos[1] + 100.0f, z), z};
        mech3::move_mech(seen.player, at, 1.5707964f);
      } else {
        const mech3::ai::AiTarget* t = mech3::ai::brain_hooks().targetable(seen.dv);
        if (t && t->target && mech3::ai::brain_hooks().contact_vehicle(t->target) == seen.pv) ++seen.on_player;
      }
    };
    CountingPump pump;
    pump.limit = 2300;
    app.run(pump);
    if (std::getenv("MW3_LANCE_LOG"))
      std::fprintf(stderr,
                   "ORDER lancemate %d taken %d follows %d fired before %d after %d d0 %g closest %g | enemies %x -> %x,"
                   " on the player %d, frames %d\n",
                   seen.lancemate, seen.order_taken, seen.follows_enemy, seen.fired_before, seen.fired_after, seen.d0,
                   seen.closest, seen.enemies_before, seen.enemies_after, seen.on_player, seen.frames);
    if (std::getenv("MW3_LANCE_LOG"))
      std::fprintf(stderr, "PHASES %d %d %d %d %d %d %d %d %d on the enemy %d\n", seen.phases[0], seen.phases[1],
                   seen.phases[2], seen.phases[3], seen.phases[4], seen.phases[5], seen.phases[6], seen.phases[7],
                   seen.phases[8], seen.on_enemy);
    EXPECT(seen.frames >= 1750 && seen.brain);
    EXPECT(seen.lancemate && seen.order_taken && seen.follows_enemy);
    EXPECT(seen.fired_after > 300 && seen.closest < seen.d0 * 0.5f);
    EXPECT(seen.pt && !(seen.enemies_before & seen.pt->team) && (seen.enemies_after & seen.pt->team));
    // Allies no more (his next pick may take the player: logged; when it
    // comes depends on the walk, whose last bits vary from run to run).
    EXPECT(!(seen.allies_after & seen.pt->team));
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The player's allies fight only what comes in reach (docs/ai.md 17.8b): in
// c1 mission 4 Dominic, woken, with an enemy put 60 m in front of him and no
// order given, takes it as his target and fires at it while he follows the
// player (his own order), never closing in.
TEST(mission_c1m4_dominic_shoots_a_near_enemy_unordered_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_lance_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::player_controls_instance() = nullptr;  // nor the player's controls (leaked): its keys registered again
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 4: Dominic with the player
    l.number = 4;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      mech3::ai::Brain* brain = nullptr;
      void* dominic = nullptr;
      void* enemy = nullptr;
      zgame::Handle dv = 0, ev = 0;
      std::string enemy_name;
      int frames = 0, engage = 0, targeted = 0, on_enemy = 0, aiming = 0, firing = 0, attacks = 0;
      int down = 0, contacts = 0, enemy_contacts = 0, phase = -1, follows = 0, follows_player = 0;
      float closest = 1e9f;
      bool ai_on = false;
      float xe8 = 0, start[3] = {}, moved = 0;
    } seen;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        for (int32_t i = 0; i < made; ++i) {
          const std::string n = ai[i].name ? ai[i].name : "";
          if (n == "Dominic_Paine") {
            seen.dominic = ai[i].mech;
            seen.brain = static_cast<mech3::AiControls*>(ai[i].controls)->brain.get();
          } else if (!seen.enemy && n.rfind("blackhawk", 0) == 0) {
            seen.enemy = ai[i].mech, seen.enemy_name = n;
          }
        }
        if (!seen.dominic || !seen.enemy || !seen.brain) return;
        seen.dv = engine.handle_of(seen.dominic), seen.ev = engine.handle_of(seen.enemy);
        // The enemy (powered down) put 60 m in front of Dominic, nobody
        // fighting, no order given (MW3_LANCE_AT=150: the ground blocks the
        // line from there, he aims but never fires).
        mech3::LaunchedMech d;
        mech3::describe_mech(seen.dominic, &d);
        std::copy(d.pos, d.pos + 3, seen.start);
        const float x = d.pos[0] + (std::getenv("MW3_LANCE_AT") ? std::strtof(std::getenv("MW3_LANCE_AT"), nullptr) : 60.0f), z = d.pos[2];
        const float at[3] = {x, ground_under(x, d.pos[1] + 100.0f, z), z};
        mech3::move_mech(seen.enemy, at, 1.5707964f);
        if (mech3::ai::Valve* w = mech3::ai::valve_find("dominic_wakes")) w->set(1);  // he starts powered down
        seen.xe8 = seen.brain->xe8;
      } else if (seen.frames > 3 && seen.brain) {
        mech3::ai::Brain& b = *seen.brain;
        if (b.params.get<int32_t>(0x3c)) ++seen.engage;
        if (b.xe8 != seen.xe8) ++seen.attacks, seen.xe8 = b.xe8;
        const mech3::ai::AiTarget* t = mech3::ai::brain_hooks().targetable(seen.dv);
        if (t && t->target) {
          ++seen.targeted;
          if (mech3::ai::brain_hooks().contact_vehicle(t->target) == seen.ev) ++seen.on_enemy;
        }
        if (mech3::ai::brain_hooks().powered_down(seen.dv) == 1) ++seen.down;
        seen.ai_on = mech3::ai::ai_switch();
        seen.phase = b.firing.phase;
        if (t) {
          seen.contacts = std::max(seen.contacts, static_cast<int>(t->contacts.size()));
          for (const mech3::ai::Contact* c : t->contacts)
            if (mech3::ai::brain_hooks().contact_vehicle(c) == seen.ev) ++seen.enemy_contacts;
        }
        if (auto* fo = dynamic_cast<mech3::ai::FollowState*>(b.current().state.get())) {
          ++seen.follows;
          auto* vp = dynamic_cast<mech3::ai::VehiclePart*>(b.current().net.get());
          if (vp && vp->target && vp->target->vehicle == engine.handle_of(mech3::mech_mission_player())) ++seen.follows_player;
          (void)fo;
        }
        {
          mech3::LaunchedMech en;
          mech3::describe_mech(seen.enemy, &en);
          mech3::LaunchedMech dm;
          mech3::describe_mech(seen.dominic, &dm);
          const float ex = en.pos[0] - dm.pos[0], ez = en.pos[2] - dm.pos[2];
          seen.closest = std::min(seen.closest, std::sqrt(ex * ex + ez * ez));
        }
        if (b.firing.phase == 2 || b.firing.phase == 3) ++seen.aiming;
        if (b.firing.phase == 4 || b.firing.phase == 7) ++seen.firing;
        mech3::LaunchedMech d;
        mech3::describe_mech(seen.dominic, &d);
        const float dx = d.pos[0] - seen.start[0], dz = d.pos[2] - seen.start[2];
        seen.moved = std::max(seen.moved, std::sqrt(dx * dx + dz * dz));
      }
    };
    CountingPump pump;
    pump.limit = 900;  // 30 s
    app.run(pump);
    EXPECT(seen.frames >= 600 && seen.brain && seen.enemy);
    if (std::getenv("MW3_LANCE_LOG"))
      std::fprintf(stderr,
                   "DOMINIC vs %s: engage %d attacks %d targeted %d on it %d aiming %d firing %d moved %g"
                   " | down %d ai %d contacts %d its %d phase %d follows %d (the player %d) closest %g\n",
                   seen.enemy_name.c_str(), seen.engage, seen.attacks, seen.targeted, seen.on_enemy,
                   seen.aiming, seen.firing, seen.moved, seen.down, seen.ai_on, seen.contacts, seen.enemy_contacts, seen.phase, seen.follows,
                   seen.follows_player, seen.closest);
    // The firing picked the enemy itself (0x004b6790) and fired at it, with
    // engaging off all along and no attack: his order the whole time the
    // follow on the player (0x004d1980), never a step nearer the enemy
    // (ai.md 17.8b).
    if (!std::getenv("MW3_LANCE_AT")) {
      EXPECT(seen.down == 0 && seen.on_enemy > 600 && seen.firing > 300);
      EXPECT(seen.engage == 0 && seen.attacks == 0);
      EXPECT(seen.follows_player > 600 && seen.closest > 59.0f);
    }
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// c1 mission 1's Owens walking its net (docs/ai.md 17.4a): woken by bramp,
// its shared order puts it on the node of net 4 nearest it, where it
// stands, so it is there at once and goes on by the node's one link: a
// speed step (its share 0.7), a valve step, then the next node 245 m off.
// That node's state (0x004ced40) turns and throttles the Owens' own controls
// along the steering segment; it walks there and, within its stopping
// distance, goes on by one of that node's links: a scan step (0x004cbca0),
// then its post, where it waits for the valve firefly_retreat. MW3_WALK_LOG=1
// prints the walk.
TEST(mission_c1m1_owens_walks_to_its_node_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_ai_walk_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  const bool log = std::getenv("MW3_WALK_LOG") != nullptr;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    // Each node it is sent to: how far it was at first, the nearest it came,
    // the best it faced it (forward against the way there), whether the
    // order went on from it.
    struct Leg {
      const mech3::ai::Behaviour* node = nullptr;
      float start = -1, nearest = 1e9f, face = -2, slow = 0;
      bool went_on = false;
    };
    struct Seen {
      int frames = 0;
      std::vector<Leg> legs;
      float max_throttle = 0, max_turn = 0, moved = 0, start_pos[3] = {};
      bool feet = true, started = false;
      int32_t last_phase = -1;
      std::string waits_on;  // the valve the last node's one link waits for
    } seen;
    void* owens = nullptr;
    mech3::AiControls* ctl = nullptr;
    mech3::ai::Brain* brain = nullptr;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        owens = made == 1 ? ai[0].mech : nullptr;
        ctl = made == 1 ? static_cast<mech3::AiControls*>(ai[0].controls) : nullptr;
        brain = ctl ? ctl->brain.get() : nullptr;
        // Woken 0.1 s in, where play's bramp comes much later: the hit record
        // starts at 0, so in the game's first second every pilot counts as hit
        // lately (0x004aed40, as the exe); its last hit set long ago instead.
        if (ctl) ctl->hit.last = -10.0f;
        if (mech3::ai::Valve* b = mech3::ai::valve_find("bramp")) b->set(1);  // wakes the Owens
        return;
      }
      if (seen.frames < 4 || !brain || brain->orders.empty()) return;
      mech3::LaunchedMech o;
      mech3::describe_mech(owens, &o);
      if (!seen.started) {
        seen.started = true;
        for (int i = 0; i < 3; ++i) seen.start_pos[i] = o.pos[i];
      }
      if (!o.feet_down) seen.feet = false;
      const mech3::ai::BehaviourState* st = brain->current().state.get();
      auto* ns = dynamic_cast<const mech3::ai::NodeState*>(st);
      if (!seen.legs.empty() && (!ns || ns->from != seen.legs.back().node)) seen.legs.back().went_on = true;
      if (ns && (seen.legs.empty() || seen.legs.back().node != ns->from)) seen.legs.push_back(Leg{ns->from});
      if (ns) {
        Leg& g = seen.legs.back();
        const float* q = ns->from->position();
        float f[3];
        mech3::mech_forward(owens, f);
        const float dx = q[0] - o.pos[0], dz = q[2] - o.pos[2];
        const float d = std::sqrt(dx * dx + dz * dz);
        if (g.start < 0) g.start = d;
        g.nearest = std::min(g.nearest, d);
        g.slow = ns->slow;
        if (d > 1.0f) g.face = std::max(g.face, (f[0] * dx + f[2] * dz) / d);
      }
      seen.max_throttle = std::max(seen.max_throttle, ctl->throttle);
      seen.max_turn = std::max(seen.max_turn, std::fabs(ctl->turn));
      const float mx = o.pos[0] - seen.start_pos[0], mz = o.pos[2] - seen.start_pos[2];
      seen.moved = std::max(seen.moved, std::sqrt(mx * mx + mz * mz));
      if (ns) seen.last_phase = ns->phase;
      if (ns && ns->from->link_count() == 1 && ns->from->link(0)->kind == mech3::ai::StepKind::Valve) {
        auto* vs = static_cast<const mech3::ai::ValveStep*>(ns->from->link(0));
        seen.waits_on = vs->valve && vs->valve->name ? vs->valve->name : "";
      }
      if (log && seen.frames % 50 == 0)
        std::fprintf(stderr, "walk %4d pos %8.2f %8.2f thr %6.3f/%6.3f turn %6.3f/%6.3f state %#x legs %zu\n", seen.frames,
                     o.pos[0], o.pos[2], ctl->throttle, ctl->throttle_target, ctl->turn, ctl->turn_target,
                     st && st->from ? static_cast<unsigned>(st->from->kind) : 0u, seen.legs.size());
    };
    CountingPump pump;
    pump.limit = 1500;
    app.run(pump);
    if (log)
      for (const Leg& g : seen.legs)
        std::fprintf(stderr, "leg: %.2f m off, nearest %.2f (stopping %.2f), faced %.3f, on %d\n", g.start, g.nearest,
                     g.slow, g.face, g.went_on);
    EXPECT(seen.frames > 1000);
    EXPECT(seen.feet);
    // The node it stands on, then the one 245 m off.
    EXPECT(seen.legs.size() >= 2);
    if (seen.legs.size() >= 2) {
      EXPECT(seen.legs[0].start < 1.0f && seen.legs[0].went_on);  // there already: on at once
      const Leg& g = seen.legs[1];
      EXPECT(g.start > 200.0f);                    // the next node, far off
      EXPECT(g.face > 0.99f);                      // turned to face it
      // Walked there: on by its link once within its stopping distance (its
      // along and side from the carrot at the node, 0x004cf140).
      EXPECT(g.went_on && g.nearest <= g.slow && g.slow > 1.0f && g.slow < 30.0f);
    }
    // Past the scan step (0x004cbca0) to its post 119 m on, where it stands:
    // the one way on waits for the valve firefly_retreat (0x004cf140's 5 s
    // asks, phase 2).
    EXPECT(seen.legs.size() == 3);
    if (seen.legs.size() >= 3) {
      const Leg& g = seen.legs[2];
      EXPECT(g.start > 100.0f && g.face > 0.99f && g.nearest <= g.slow && !g.went_on);
      EXPECT(seen.last_phase == 2 && seen.waits_on == "firefly_retreat");
    }
    EXPECT(seen.moved > 200.0f);
    EXPECT(seen.max_turn > 0.05f);                                     // its own controls turned
    EXPECT(seen.max_throttle > 0.65f && seen.max_throttle <= 0.7f);    // and throttled to the speed step's 0.7
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// Playtest #12 (docs/keybindings.md): in c1 mission 1, each command's default
// key (0x00411150) pressed in turn, its modifiers first. A command fires when
// its callback runs (the mech's 0x00426cb0, the app's 0x0040dba0, the
// mission's 0x0048f440) or, with none, when zInput shows it held (the
// movement keys the player's controls read). Ctrl+Q (Exit Mission) and
// Alt+F4 end the run, so they are only checked bound. MW3_KEYS_OUT names a
// file for the audit's table: command, key, callback, fired, what was
// unported in the press.
namespace {
zin::CommandCallback g_key_cb[0x100];
int g_key_ran[0x100];
void key_count(int32_t cmd) {
  ++g_key_ran[cmd & 0xff];
  if (g_key_cb[cmd & 0xff]) g_key_cb[cmd & 0xff](cmd);
}
}  // namespace

TEST(mission_c1m1_default_keys_fire_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_keys_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  struct Row {
    int32_t cmd = 0;
    uint32_t key = 0;
    bool callback = false, pressed = false, held = false;
    int ran = 0;
    std::string unported;
  };
  std::vector<Row> rows;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    size_t next = 0;
    int phase = 0;  // 0 press, 1 look, 2 release
    std::map<std::string, uint32_t> before, idle;
    std::set<std::string> noise;
    int view_before = -1, view_after_x = -1;
    mech3::LaunchedMech enter_before, enter_after;
    // Playtest #14..#17: the look keys' look, torso mode's view state, the
    // fire mode's cockpit sound, the op points without multiplayer's stations.
    int look_after[6] = {-9, -9, -9, -9, -9, -9};
    int tmode_before = -9, tmode_after = -9;
    bool mode_played = false, mode_sounds = false;
    // zSound is closed headless (no device): the cockpit sound's play is
    // recorded instead (0x004e6d80 on the table's object).
    static std::vector<zsnd::Object*> g_cockpit_played;
    g_cockpit_played.clear();
    mech3::cockpit_sound_calls().play = [](zsnd::Object* s, float) -> zsnd::VoiceBase* {
      g_cockpit_played.push_back(s);
      return nullptr;
    };
    float throttles[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    auto mods = [](uint32_t key, std::vector<uint32_t>* out) {
      if (key & 0x100) out->push_back(0x38);  // Alt
      if (key & 0x200) out->push_back(0x1d);  // Ctrl
      if (key & 0x400) out->push_back(0x2a);  // Shift
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (frames == 10) {  // the mission is up: every command's bindings and callbacks
        zin::CmdMap* m = zin::current_map();
        for (int32_t c = 1; c < m->count && c < 0x100; ++c) {
          const uint32_t k = static_cast<uint32_t>(zin::key_of(c));
          Row r;
          r.cmd = c, r.key = k, r.callback = m->callbacks[c] != nullptr;
          if (r.callback) g_key_cb[c] = m->callbacks[c], m->callbacks[c] = key_count;
          if (k || r.callback) rows.push_back(r);
        }
      }
      if (frames == 14) idle = engine.unported_counts();
      if (frames == 19)  // what is unported every frame anyway
        for (const auto& [k, n] : engine.unported_counts())
          if (!idle.count(k) || idle[k] != n) noise.insert(k);
      if (frames < 20 || next >= rows.size()) return;
      Row& r = rows[next];
      std::vector<uint32_t> held;
      mods(r.key, &held);
      const uint32_t dik = r.key & 0xff;
      if (!r.key || r.cmd == 0x71 || r.cmd == 1) {  // no key, or one that ends the run
        ++next;
        return;
      }
      if (phase == 0) {
        before = engine.unported_counts();
        if (r.cmd == 0x61) view_before = mech3::program_view_mode_id();
        if (r.cmd == 0x27) mech3::describe_mech(mech3::mech_mission_player(), &enter_before);
        if (r.cmd == 0x16) tmode_before = mech3::mech_controls_view(mech3::mech_mission_player());
        for (uint32_t k : held) keys.key(k, true);
        keys.key(dik, true);
        r.pressed = true;
        phase = 1;
      } else if (phase == 1) {
        r.held = zin::command(r.cmd) != 0;
        keys.key(dik, false);
        for (uint32_t k : held) keys.key(k, false);
        phase = 2;
      } else {
        r.ran = g_key_ran[r.cmd];
        if (r.cmd == 0x61) view_after_x = mech3::program_view_mode_id();
        if (r.cmd == 0x27) mech3::describe_mech(mech3::mech_mission_player(), &enter_after);
        if (r.cmd >= 0x62 && r.cmd <= 0x67) look_after[r.cmd - 0x62] = mech3::program_view_look_state();
        if (r.cmd == 0x16) tmode_after = mech3::mech_controls_view(mech3::mech_mission_player());
        if (r.cmd == 0x28)
          for (int s = 0x14; s <= 0x16; ++s) {
            zsnd::Object* o = mech3::cockpit_sounds().sound[s];
            mode_sounds |= o != nullptr;
            mode_played |= o && std::find(g_cockpit_played.begin(), g_cockpit_played.end(), o) != g_cockpit_played.end();
          }
        if (r.cmd >= 0x1b && r.cmd <= 0x24) throttles[r.cmd - 0x1b] = mech3::mech_throttle_target(mech3::mech_mission_player());
        for (const auto& [k, n] : engine.unported_counts()) {
          auto it = before.find(k);
          if ((it == before.end() || it->second != n) && !noise.count(k))
            r.unported += (r.unported.empty() ? "" : " ") + k;
        }
        phase = 0;
        ++next;
      }
    };
    CountingPump pump;
    pump.limit = 2000;
    pump.stop = [&] { return frames > 20 && next >= rows.size(); };
    app.run(pump);
    std::string missing;
    for (const Row& r : rows) {
      if (!r.pressed) continue;
      if (!(r.ran > 0 || r.held)) missing += (missing.empty() ? "" : " ") + std::to_string(r.cmd);
    }
    if (const char* out = std::getenv("MW3_KEYS_OUT")) {
      if (FILE* f = std::fopen(out, "w")) {
        char name[128];
        for (const Row& r : rows) {
          zin::command_name(r.cmd, name, sizeof name);
          char kn[64];
          zin::key_name(r.key, kn, sizeof kn);
          std::fprintf(f, "%#x\t%s\t%s\t%d\t%d\t%d\t%d\t%s\n", r.cmd, name, r.key ? kn : "-", r.callback, r.pressed,
                       r.ran, r.held, r.unported.c_str());
        }
        std::fclose(f);
      }
    }
    if (!missing.empty()) std::fprintf(stderr, "keys that fired nothing: %s\n", missing.c_str());
    EXPECT(rows.size() > 90);
    bool exit_bound = false;  // Ctrl+Q: bound by the mission's start (0x0048f400), acts once it is over
    for (const Row& r : rows) exit_bound |= r.cmd == 0x71 && r.callback;
    EXPECT(exit_bound);
    EXPECT(missing.empty());
    EXPECT(view_before == 1 && view_after_x == 2);
    // 1..0: the throttle (0x004274d0); 2 is 0.2 though labelled 10%.
    const float want[10] = {0.0f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    for (int i = 0; i < 10; ++i) EXPECT(std::fabs(throttles[i] - want[i]) < 1e-6f);
    // Look left, right, up, down, ahead, back: the view's mode takes 0, 1, 4, 5, 2, 3 (vt+0x10).
    const int want_look[6] = {0, 1, 4, 5, 2, 3};
    for (int i = 0; i < 6; ++i) EXPECT(look_after[i] == want_look[i]);
    mech3::cockpit_sound_calls() = mech3::CockpitSoundCalls{};
    // Ctrl+keypad 0: into torso mode from 0 / 1, out of it from 2 / 3 (0x00426780).
    EXPECT(tmode_before >= 0 && tmode_after >= 0 && (tmode_before >= 2) != (tmode_after >= 2));
    EXPECT(mode_sounds && mode_played);  // \: the new mode's cockpit sound (0x0041e770)
    // Enter: the weapon after the current one in the HUD's list, its group selected.
    EXPECT(enter_before.weapon && enter_after.weapon && enter_after.weapon != enter_before.weapon);
    if (std::getenv("MW3_SURVEY"))
      std::fprintf(stderr, "enter: group %d -> %d, weapon %p -> %p\n", enter_before.group, enter_after.group,
                   enter_before.weapon, enter_after.weapon);  // in the cockpit (CameraMode 1); X: the zipper cam (0x0040aed0)
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  for (auto& cb : g_key_cb) cb = nullptr;
  for (int& n : g_key_ran) n = 0;
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
}

// Playtest #9 (docs/playtest.md): c1 mission 1's player walking at full
// throttle when the heat passes 1.5. "Shutting down" asks the mech to power
// down (0x0041cc30), whose Stop (the controls' vt+0x4c, 0x00426c10) zeroes the
// throttle, so the mech slows to a stand and the power-down finishes
// (0x0041cac0: standing, state 1); powered down it cools three times as fast
// and powers up again at 0.4 (docs/heat.md). The heat is held at 1.6 until
// then: cooled to 0.9 first, the shutdown is called off (as in the exe).
TEST(mission_c1m1_heat_shutdown_stops_the_mech_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_heat_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0, asked = 0, down = 0, up = 0;
      float walked = 0, moved_down = -1;
      float at_down[3] = {};
    } seen;
    float last[3] = {};
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      void* m = mech3::mech_mission_player();
      mech3::LaunchedMech d;
      if (!m || !mech3::describe_mech(m, &d)) return;
      if (seen.frames == 5) mech3::mech_hold_throttle(m, 1.0f);
      if (seen.frames == 60) seen.walked = std::hypot(d.pos[0] - last[0], d.pos[2] - last[2]);
      if (seen.frames >= 60 && !seen.down) mech3::mech_set_heat(m, 1.6f);  // held there, as by firing, until down
      if (seen.frames > 60 && !seen.asked && d.powering_down) seen.asked = seen.frames;
      if (seen.asked && !seen.down && d.powered_down) {
        seen.down = seen.frames;
        std::copy(d.pos, d.pos + 3, seen.at_down);
      }
      if (seen.down && seen.frames == seen.down + 30)
        seen.moved_down = std::hypot(d.pos[0] - seen.at_down[0], d.pos[2] - seen.at_down[2]);
      if (seen.down && !seen.up && !d.powering_down) seen.up = seen.frames;
      std::copy(d.pos, d.pos + 3, last);
    };
    CountingPump pump;
    pump.limit = 3000;
    pump.stop = [&] { return seen.up != 0; };
    app.run(pump);
    if (std::getenv("MW3_SURVEY"))
      std::fprintf(stderr, "heat: walked %.3f/frame, asked %d, down %d, moved %.3f in 30 frames down, up %d\n",
                   seen.walked, seen.asked, seen.down, seen.moved_down, seen.up);
    EXPECT(seen.walked > 0.01f);         // walking before
    EXPECT(seen.asked > 60);             // "shutting down" asked for the power-down
    EXPECT(seen.down > seen.asked);      // and it finished: the mech came to a stand
    EXPECT(seen.down && seen.down - seen.asked < 600);
    EXPECT(seen.moved_down >= 0.0f && seen.moved_down < 0.01f);  // still while down
    EXPECT(seen.up > seen.down);         // cooled and powered up again
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
}

// Playtest #19: the Mad Cat's cockpit (mech_madcat_hud.flt) holds three
// cracks and three dings, active in the file. Its explosion animation is
// moved onto the cockpit and reset when the cockpit is made (0x00430c05, the
// node in EDX), which hides them; a hit on the head runs it there
// (0x00460bff), showing one (docs/cockpit-damage.md).
TEST(mission_c1m1_cockpit_cracks_on_a_head_hit_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_cracks_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0, cockpit = 0, shown_before = -1, shown_after = 0, window_before = -1, hit = 0;
    } seen;
    static const char* const kMarks[] = {"crack01", "crack02", "crack03", "ding01", "ding02", "ding03"};
    static const char* const kWindow[] = {"hcrack01", "hcrack02"};
    const auto active = [](zcls::Node* root, const char* const* names, int n) {
      int on = 0;
      for (int i = 0; i < n; ++i)
        if (zcls::Node* k = zcls::node_find_in_tree(root, names[i])) on += (k->flags() >> 2) & 1;
      return on;
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      void* m = mech3::mech_mission_player();
      auto* c = m ? static_cast<zcls::Node*>(mech3::mech_cockpit_node(m)) : nullptr;
      if (!c) return;
      ++seen.cockpit;
      if (seen.frames == 20) {
        seen.shown_before = active(c, kMarks, 6);
        seen.window_before = active(c, kWindow, 2);
        seen.hit = mech3::mech_hit_part(m, 6, 1.0f);
      }
      if (seen.frames > 20) seen.shown_after = std::max(seen.shown_after, active(c, kMarks, 6));
    };
    CountingPump pump;
    pump.limit = 400;
    pump.stop = [&] { return seen.frames >= 80; };
    app.run(pump);
    if (std::getenv("MW3_SURVEY"))
      std::fprintf(stderr, "cracks: cockpit %d frames, shown %d before the hit, window %d, hit %d, %d after\n",
                   seen.cockpit, seen.shown_before, seen.window_before, seen.hit, seen.shown_after);
    EXPECT(seen.cockpit > 20);
    EXPECT(seen.shown_before == 0);   // none at the start
    EXPECT(seen.window_before == 2);  // the whole panes they replace shown
    EXPECT(seen.hit);
    EXPECT(seen.shown_after > 0);     // the head hit shows one
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
}

// Playtest #19: the Owens' cockpit has animated screens (cpit_ania..c), each
// cycled by its support\cpit_cycle<A..C>.gw at the launch (0x00491150):
// CycleTextureSetOn makes the first material cycled, CycleTextureSetMap fills
// its slots, the launch loads the textures. Before the interp's model calls
// were wired every one reported "no GFX data" and stayed still.
TEST(mission_c1m1_owens_cockpit_screens_cycle_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_screens_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    l.mech = 11;                  // the Owens
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0, screens = 0, cycled = 0, slots = 0, loaded = 0, named = 0;
      float index0[3] = {-1, -1, -1};
      int moved = 0;
    } seen;
    static const char* const kScreens[] = {"cpit_ania", "cpit_anib", "cpit_anic"};
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      void* m = mech3::mech_mission_player();
      auto* c = m ? static_cast<zcls::Node*>(mech3::mech_cockpit_node(m)) : nullptr;
      if (!c) return;
      for (int i = 0; i < 3; ++i) {
        zcls::Node* n = zcls::node_find_in_tree(c, kScreens[i]);
        zmod::Model* model = n ? n->model : nullptr;
        zmod::Material* mat = model && model->polygons ? model->polygons[0].material : nullptr;
        zmod::Cycle* cy = mat && (mat->flags & 4) ? mat->cycle : nullptr;
        if (seen.frames == 5) {
          seen.screens += n != nullptr;
          if (!cy) continue;
          ++seen.cycled;
          for (uint32_t k = 0; k < cy->texture_count; ++k) {
            const zimg::TextureEntry* t = cy->textures[k];
            ++seen.slots;
            seen.loaded += t && t->image != nullptr;
            seen.named += t && std::strncmp(t->name, "cpit_ani_", 9) == 0;
          }
          seen.index0[i] = cy->index;
          if (std::getenv("MW3_SURVEY"))
            std::fprintf(stderr, "screen %s: count %u added %u speed %.1f looping %u index %.2f frame %u\n", kScreens[i],
                         cy->texture_count, cy->added, cy->speed, cy->looping, cy->index, cy->frame);
        }
        if (seen.frames > 5 && cy && seen.index0[i] >= 0 && cy->index != seen.index0[i]) {
          ++seen.moved;
          seen.index0[i] = -1;  // counted once
        }
      }
    };
    CountingPump pump;
    pump.limit = 400;
    pump.stop = [&] { return seen.frames >= 60; };
    app.run(pump);
    if (std::getenv("MW3_SURVEY"))
      std::fprintf(stderr, "screens: %d found, %d cycled, %d slots, %d loaded, %d named cpit_ani_, %d moved\n",
                   seen.screens, seen.cycled, seen.slots, seen.loaded, seen.named, seen.moved);
    EXPECT(seen.screens > 0);
    EXPECT(seen.cycled == seen.screens);
    EXPECT(seen.slots > 0 && seen.loaded == seen.slots && seen.named == seen.slots);
    EXPECT(seen.moved > 0);  // a drawn one runs on (0x005376c0 steps a cycle only when drawn)
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
}

// The ceiling's crush (0x00423600) in the program: the collider's box test
// (0x004236f5) asks the box of the node crossed overhead (0x004ec9c0). The
// program had left CollideCalls::hit_box unwired, so a mech walking under a
// boxed object took no damage and did not duck. Here a boxed node of c1's
// world, the smallest, stands for the ceiling the player's mech meets on foot.
TEST(mission_c1m1_ceiling_crush_uses_the_box_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_crush_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0, boxed = 0;
      bool ran = false;
      zcls::Node* unboxed = nullptr;
      float plain[7] = {};
      float before[7] = {}, after[7] = {}, volume = 0;
    } seen;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      if (++seen.frames != 20) return;
      void* m = mech3::mech_mission_player();
      // The smallest boxed node (flag 0x100, box at +0x74, read here from the
      // node's bytes) of the world, and one without a box.
      zcls::Node* pick = nullptr;
      for (int32_t i = 0; i < zcls::nodes().capacity; ++i) {
        zcls::Node* n = zcls::node_at(i);
        if (!n || !n->name()[0]) continue;
        if (!(n->flags() & 0x100)) {
          if (!seen.unboxed) seen.unboxed = n;
          continue;
        }
        const float* b = reinterpret_cast<const float*>(n->raw + 0x74);
        const float v = (b[4] - b[1]) * (b[5] - b[2]) * (b[3] - b[0]);
        if (!(v > 1.0f)) continue;
        ++seen.boxed;
        if (!pick || v < seen.volume) pick = n, seen.volume = v;
      }
      if (!pick) return;
      float none[7];
      mech3::mech_ceiling_hit(m, nullptr, none);  // no node: no box, nothing happens
      std::copy(none, none + 7, seen.before);
      if (seen.unboxed) mech3::mech_ceiling_hit(m, seen.unboxed, seen.plain);
      seen.ran = mech3::mech_ceiling_hit(m, pick, seen.after);
    };
    CountingPump pump;
    pump.limit = 400;
    pump.stop = [&] { return seen.frames >= 21; };
    app.run(pump);
    if (std::getenv("MW3_SURVEY"))
      std::fprintf(stderr, "crush: %d boxed nodes, smallest volume %.2f; head %.3f -> %.3f, torso %.3f -> %.3f, crouch %.3f\n",
                   seen.boxed, seen.volume, seen.before[0], seen.after[0], seen.before[1], seen.after[1],
                   seen.after[4]);
    EXPECT(seen.ran && seen.boxed > 0);
    EXPECT(seen.before[5] == 0.0f && seen.after[5] == 1.0f);  // the program gives the box (0x004ec9c0)
    EXPECT(seen.unboxed && seen.plain[5] == 0.0f);             // and none for a node without one
    EXPECT(seen.after[0] < seen.before[0]);                   // the head takes 0.1 of it
    for (int s = 1; s <= 3; ++s) EXPECT(seen.after[s] < seen.before[s]);  // the torso's three sections
    EXPECT(seen.after[4] < 0.0f);                              // and the mech ducks (0x004622a0)
    // By the box's volume, v = volume / 180 x 0.25 (0x005e24e8): the hip
    // drops v (at most 1) times the crouch depth.
    const float v = std::min(1.0f, seen.volume / 180.0f * 0.25f);
    if (std::getenv("MW3_SURVEY"))
      std::fprintf(stderr, "crush: duck %.6g, want %.6g\n", seen.after[4], -v * seen.after[6]);
    EXPECT(std::fabs(seen.after[4] + v * seen.after[6]) <= 1e-5f * std::max(1.0f, seen.after[6]));
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
}

// The view's ground and water check (0x0040af30) in the program: the ground
// under the camera (0x004edab0) and the water in it (0x004abd90). An outside
// camera put under the ground is lifted half a metre above it; one put under
// a water surface gives the world the water's fog (0x0040b0e0), and back above
// it the air's. Before, the program answered "no ground" and "no water".
TEST(mission_c1m1_view_ground_and_water_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_ground_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, mission 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0;
      bool found = false, water = false, under = false, above_under = true;
      float mech_y = 0, buried = 0, lifted = 0, water_at[4] = {};
      float markers[4] = {};
      bool markers_ok = false;
    } seen;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      // The first frame: the markers placed at the build (0x005a8500), the
      // target 0.65 of the mech's height up, the centroid half.
      if (++seen.frames == 1) seen.markers_ok = mech3::mech_marker_heights(mech3::mech_mission_player(), seen.markers);
      if (seen.frames != 20) return;
      void* m = mech3::mech_mission_player();
      float at[3] = {};
      if (!mech3::mech_world_position(m, at)) return;
      mech3::program_view_set_camera(0);  // tether: an outside camera
      seen.mech_y = at[1];
      seen.buried = at[1] - 5.0f;
      bool under = false;
      seen.found = mech3::program_view_ground_probe(at[0], seen.buried, at[2], &seen.lifted, &under);
      if (mech3::program_view_find_water(at[0], at[2], 600.0f, seen.water_at)) {
        seen.water = true;
        float y;
        mech3::program_view_ground_probe(seen.water_at[0], seen.water_at[1] - 1.0f, seen.water_at[2], &y, &seen.under);
        mech3::program_view_ground_probe(seen.water_at[0], seen.water_at[1] + 3.0f, seen.water_at[2], &y,
                                         &seen.above_under);
      }
    };
    CountingPump pump;
    pump.limit = 400;
    pump.stop = [&] { return seen.frames >= 21; };
    app.run(pump);
    if (std::getenv("MW3_SURVEY"))
      std::fprintf(stderr, "ground: mech y %.2f, buried at %.2f, lifted to %.2f (found %d); water %d at (%.1f, %.2f, %.1f) "
                   "ground %.2f, under %d, above it under %d\n", seen.mech_y, seen.buried, seen.lifted, seen.found,
                   seen.water, seen.water_at[0], seen.water_at[1], seen.water_at[2], seen.water_at[3], seen.under,
                   seen.above_under);
    EXPECT(seen.found);                                              // 0x004edab0 finds the ground
    EXPECT(seen.lifted > seen.buried + 4.0f);                         // lifted out of it
    EXPECT(std::fabs(seen.lifted - seen.mech_y - 0.5f) < 1.0f);       // to about half a metre above
    EXPECT(seen.water);                                               // c1 has water
    EXPECT(seen.under && !seen.above_under);                          // its fog only below the surface
    EXPECT(engine.unported_counts().count("0x004edab0") == 0);
    if (std::getenv("MW3_SURVEY"))
      std::fprintf(stderr, "markers: node %.3f target %.3f centroid %.3f height %.3f\n", seen.markers[0], seen.markers[1],
                   seen.markers[2], seen.markers[3]);
    EXPECT(seen.markers_ok && seen.markers[3] > 1.0f);
    EXPECT(std::fabs(seen.markers[1] - seen.markers[0] - 0.65f * seen.markers[3]) < 0.01f);
    EXPECT(std::fabs(seen.markers[2] - seen.markers[0] - 0.5f * seen.markers[3]) < 0.01f);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
}

// The skins on the mech's model (0x00475780 at the type's load, 0x00475a80
// at the build, 0x004757f0 at the launch with the slot's skin): launched in
// its second skin (variant B), the Mad Cat's model and low-detail copy wear
// that skin's textures (`mcat01_b` for the primary's `mcat01`), loaded, and
// the mech's own skin (+0x3e8) is it. Before, every hook was unported and
// every mech wore its model's own textures (the primary skin's).
TEST(mission_c1m1_mech_wears_its_skin_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_skin_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, mission 1, the Mad Cat
    l.skin = 1;                   // its second skin
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    mech3::MechTextures t;
    int frames = 0;
    bool got = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || ++frames != 5) return;
      got = mech3::mech_textures(mech3::mech_mission_player(), &t);
    };
    CountingPump pump;
    pump.limit = 300;
    pump.stop = [&] { return frames >= 6; };
    app.run(pump);
    auto count_b = [](const std::vector<std::string>& v) {
      int n = 0;
      for (const std::string& s : v) n += s.size() > 2 && s.compare(s.size() - 2, 2, "_b") == 0;
      return n;
    };
    const int b = count_b(t.names), lod_b = count_b(t.lod);
    if (std::getenv("MW3_SURVEY")) {
      std::fprintf(stderr, "skin: %s, %zu textured polygons (%d _b, %d loaded), lod %zu (%d _b)\n", t.skin.c_str(),
                   t.names.size(), b, t.loaded, t.lod.size(), lod_b);
      for (size_t k = 0; k < t.names.size() && k < 8; ++k) std::fprintf(stderr, "  %s\n", t.names[k].c_str());
    }
    EXPECT(got && !t.names.empty());
    EXPECT(b > static_cast<int>(t.names.size()) / 2);       // the variant's textures on the model
    EXPECT(t.lod.empty() || lod_b > static_cast<int>(t.lod.size()) / 2);  // and its low-detail copy
    EXPECT(t.loaded == static_cast<int>(t.names.size() + t.lod.size()));  // every one with its image
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
}

// c1 mission 1's Owens destroyed (its centre torso, Destroy(0, 1)) once
// woken and standing (docs/damage.md, The death): dead, the slow death fall
// queued, the node renamed "deadmech", the pilot's eject seat out (a copy of
// its node, 0x00432830 from 0x0041d070), the mission's player untouched; the
// fall plays out over the next frames and the copies go with the mission.
TEST(mission_c1m1_owens_dies_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_death_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::player_controls_instance() = nullptr;  // nor the player's controls (leaked): its keys registered again
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0;
      mech3::LaunchedMech woke, killed, later, player, armed, armless;
      int32_t ejected = 0, salvage0 = 0, salvage1 = 0, listed = 0;
      bool stranger_skipped = false;  // a non-mech entity on zEntity's list
      bool lost = false;
    } seen;
    void* owens = nullptr;
    mech3::Handle stranger = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        owens = made == 1 ? ai[0].mech : nullptr;
        if (mech3::ai::Valve* b = mech3::ai::valve_find("bramp")) b->set(1);  // wakes the Owens
        // Both mechs are zEntity's, which the kill credit looks killers up in.
        void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        for (const auto h : mech3::ai::entity_list())
          seen.listed += engine.object_of(h) == owens || engine.object_of(h) == player;
      } else if (seen.frames == 20) {
        // An entity that is not a mech (a vehicle's place, here a bare block)
        // joins zEntity's list: every mech path must pass it by.
        static char stranger_object[0x200] = {};  // zeroed: any field read as a mech's would be 0
        stranger = engine.handle_of(stranger_object);
        mech3::ai::entity_list().push_back(stranger);
      } else if (seen.frames == 40 && owens) {
        mech3::LaunchedMech sm;
        seen.stranger_skipped = engine.object_of(stranger) != nullptr && !mech3::entity_is_mech(engine.object_of(stranger)) &&
                                !mech3::describe_mech(engine.object_of(stranger), &sm) &&
                                engine.unported_counts().count("entity vt+0x34") != 0;
        mech3::describe_mech(owens, &seen.armed);
        mech3::mech_destroy_part(owens, 2);  // the left arm flies off (0x00467100)
        mech3::describe_mech(owens, &seen.armless);
      } else if (seen.frames == 60 && owens) {
        mech3::describe_mech(owens, &seen.woke);
        seen.salvage0 = mech3::mission_salvage_items();
        mech3::mech_kill(owens, engine.object_of(engine.field32(engine.pilot(), 0x14)));  // the player's kill
        seen.salvage1 = mech3::mission_salvage_items();
        mech3::describe_mech(owens, &seen.killed);
        seen.ejected = mech3::canned_nodes_count();
      } else if (seen.frames == 420 && owens) {
        mech3::describe_mech(owens, &seen.later);
        void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        mech3::describe_mech(player, &seen.player);
        seen.lost = mech3::missions().aborting != 0;
      }
    };
    CountingPump pump;
    pump.limit = 900;
    app.run(pump);
    EXPECT(seen.frames >= 420);
    EXPECT(seen.listed == 2);
    EXPECT(seen.stranger_skipped);
    EXPECT(seen.woke.anim_state != 10 && !seen.woke.deadmech);
    // The left arm flew off: its node in the world, a stub in its place (the
    // Owens has no hand nodes).
    EXPECT(seen.armless.debris == 1 && seen.armless.debris_in_world);
    EXPECT(seen.armed.debris == 0 && seen.armless.parts == seen.armed.parts);  // the stub keeps the arm's place
    EXPECT(seen.killed.anim_state == 10 && seen.killed.deadmech);
    EXPECT(seen.killed.action == 7 || seen.killed.action == 8);  // standing still: a slow fall
    EXPECT(seen.ejected == 1);
    // The fall played out (about 4.4 s), then the dead mech's updates stop.
    EXPECT(seen.later.anim_state == 10 && seen.later.action == 0 && seen.later.alive - seen.killed.alive > 3.0f);
    // Then on the dead mechs' list (0x0041e350), whose tick (0x0041e3f0) keeps
    // it on the ground; not before its fall ended.
    EXPECT(!seen.killed.dead_listed && seen.later.dead_listed && seen.later.feet_down);
    EXPECT(seen.player.anim_state != 10 && !seen.lost);
    // The kill credit (0x0041b420): one mech for the player, the Owens' weight
    // class 1 is 400 points, and in the campaign it goes to the salvage.
    EXPECT(seen.player.kills[0] == 1 && seen.player.kills[3] == 400);
    EXPECT(seen.salvage1 > seen.salvage0);
    mech3::missions().cleanup(false);
    EXPECT(mech3::canned_nodes_count() == 0);  // 0x00431800 at the cleanup
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// c1m1 on the real mech (hud.md sections 12, 13): the player's HUD lists the
// Mad Cat's weapons, one shown line each with the weapon's DESC, the current
// group bracketed; then, with the Owens in the player's target slot and a
// group that locks, the lock goes through acquiring, waiting and locked:
// the reticle (centred, the cockpit view) over TARGET's triangle around the
// Owens straight ahead (TARGET's tracking 0x00440b00, nothing forced).
TEST(mission_c1m1_hud_weapon_lines_and_lock_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_hud_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {  // the pitch key (as the aim test)
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0, weapons = 0, lines = 0, names_ok = 0, bracketed = 0, lock_group = -1;
      int first[4] = {0, 0, 0, 0};  // the first frame of lock states 1..3
      bool target_set = false;
      int prev = 0, waiting = 0;
      int ai_named = 0;  // AI mechs whose brain gives a name
      int radar_frames = 0, blip_frames = 0, blip_off = -1, blips_locked = 0, blips_asleep = 0;  // the radar (hud.md section 15)
      float blip_d = 0.0f, blip_want = 0.0f;
      mech3::HudReport gauges;  // frame 10's (hud.md section 17)
      int panel_made = 0, panel_up = 0, panel_down = 0, posted = 0;  // the message panel (hud.md section 18)
    } seen;
    void* player = nullptr;
    void* owens = nullptr;
    mech3::HudReport r;
    uint32_t pitch = 0, pitch_up = 0;
    bool pitching = false, raising = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (mech3::MessagePanel* mp = mech3::message_panel()) {
        // a message held a second from frame 12 once the panel is idle (the
        // objectives' own lines come first: c1m1's emplacements, never built,
        // complete at once): up to its place, then down and hidden
        if (seen.frames >= 12 && !seen.posted && mp->y == mp->off_y && (mp->flags & ztwod::kHidden)) {
          seen.panel_made = 1;
          seen.posted = seen.frames;
          mech3::message_panel_add_for(1.0f, "%s", "Nav Alpha reached");
        }
        if (seen.posted && seen.frames > seen.posted && mp->y == mp->on_y && !seen.panel_up) seen.panel_up = seen.frames;
        if (seen.panel_up && (mp->flags & ztwod::kHidden) && !seen.panel_down) seen.panel_down = seen.frames;
      }
      if (seen.frames == 3) {
        player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        owens = made == 1 ? ai[0].mech : nullptr;
        mech3::LaunchedMech o;  // 150 m west of the Owens, facing it
        mech3::describe_mech(owens, &o);
        const float x = o.pos[0] - 150.0f, z = o.pos[2];
        const float at[3] = {x, ground_under(x, o.pos[1] + 50.0f, z), z};
        mech3::move_mech(player, at, -1.5707964f);
        pitch = zin::key1_of(5) & 0xff;  // CMD_PITCH_DOWN
        pitch_up = zin::key1_of(4) & 0xff;  // CMD_PITCH_UP
        mech3::program_hud_report(&r);  // the blip count from here
      } else if (seen.frames > 3 && seen.frames < 10 && mech3::program_hud_report(&r)) {
        // the throttle opened for a few frames, so the gauges move (hud.md section 17)
        if (seen.frames == 4) keys.key(zin::key1_of(0x18) & 0xff, true);
        if (seen.frames == 8) keys.key(zin::key1_of(0x18) & 0xff, false);
        seen.blips_asleep += r.blips;  // the Owens powered down: not in sight, no blip
        if (seen.frames == 9)
          if (mech3::ai::Valve* b = mech3::ai::valve_find("bramp")) b->set(1);  // wakes the Owens
      } else if (seen.frames == 10 && mech3::program_hud_report(&r)) {
        seen.weapons = r.weapons, seen.lines = r.lines, seen.names_ok = r.named, seen.bracketed = r.bracketed;
        seen.gauges = r;
        // A mech's own name (0x0041df30): the AI's from its brain's record (0x004aee70).
        int32_t n = 0;
        const mech3::AiMechMade* made = mech3::mission_ai_mechs(&n);
        for (int32_t i = 0; i < n; ++i) {
          const char* nm = mech3::mech_brain_name(made[i].mech);
          if (nm && *nm) ++seen.ai_named;
        }
        keys.key(zin::key1_of(0x1b) & 0xff, true);  // THROTTLE1: back to 0% for the lock
        keys.key(zin::key1_of(0x1b) & 0xff, false);
      } else if (seen.frames > 10 && seen.frames < 20 && mech3::program_hud_report(&r)) {
        // The Owens awake 150 m ahead: one blip a frame, 150 m x radius / range from the centre.
        seen.radar_frames += r.radar_shown && r.radar_r > 0;
        seen.blip_frames += r.blips == 1;
        seen.blip_d = std::hypot(static_cast<float>(r.blip_x - r.radar_x), static_cast<float>(r.blip_y - r.radar_y));
        seen.blip_want = 150.0f * static_cast<float>(r.radar_r) / r.sensor_range;
        seen.blip_off = std::abs(r.blip_x - r.radar_x);
      } else if (seen.frames == 20) {
        seen.lock_group = mech3::program_hud_lock_on(owens, &seen.target_set);
      } else if (seen.frames > 20 && mech3::program_hud_report(&r)) {
        if (seen.frames > 21) seen.blips_locked += r.blips;  // the target is a triangle instead (frame 20's blip came first)
        if (r.lock >= 1 && r.lock <= 3 && !seen.first[r.lock]) seen.first[r.lock] = seen.frames;
        if (r.lock == 2 && seen.prev != 2 && !seen.first[3]) seen.waiting = seen.frames;  // the last wait before the lock
        seen.prev = r.lock;
        // The torso pitched until the reticle lies inside TARGET's triangle
        // (the Owens projects below it), kept in its middle band. In the
        // player's torso mode (+0x84 = 1, 0x00426780) the pitch holds where
        // it is left and drifts on a little as the controls ease off, so
        // the keys steer both ways.
        const float h = static_cast<float>(r.base_y - r.apex_y);
        const bool high = r.ry < r.apex_y + 0.35f * h, low = r.ry > r.apex_y + 0.65f * h;
        if (high != pitching) keys.key(pitch, high), pitching = high;
        if (low != raising) keys.key(pitch_up, low), raising = low;
        // The Owens walks: the player turned toward it while it is off centre
        // (yaw -pi/2 faces +x; the turn resets the torso, so not while pitching).
        if (std::abs(r.shape_x - static_cast<float>(r.rx)) > 4.0f && !pitching) {
          mech3::LaunchedMech o, me;
          mech3::describe_mech(owens, &o);
          mech3::describe_mech(player, &me);
          mech3::move_mech(player, me.pos, std::atan2(-(o.pos[0] - me.pos[0]), -(o.pos[2] - me.pos[2])));
        }
      }
    };
    CountingPump pump;
    pump.limit = 900;
    app.run(pump);
    mech3::program_hud_force_over(false);
    std::printf("  %d weapons, %d lines shown (%d named), bracket %d; group %d, target %d, lock 1/2/3 at %d/%d/%d"
                " (the last wait from %d)\n",
                seen.weapons, seen.lines, seen.names_ok, seen.bracketed, seen.lock_group, seen.target_set,
                seen.first[1], seen.first[2], seen.first[3], seen.waiting);
    std::printf("  radar shown %d/9 frames, %d blips asleep, the Owens's blip %d/9 awake, %.1f px out (%.1f wanted, %d across),"
                " %d blips once targeted\n", seen.radar_frames, seen.blips_asleep, seen.blip_frames, seen.blip_d,
                seen.blip_want, seen.blip_off, seen.blips_locked);
    EXPECT(seen.weapons > 0);
    EXPECT(seen.lines == seen.weapons);    // one shown line a weapon
    EXPECT(seen.names_ok == seen.lines);   // named by the weapon's DESC
    EXPECT(seen.bracketed);                // the current group's bracket
    EXPECT(seen.lock_group >= 0 && seen.target_set);
    EXPECT(seen.first[1] > 0 && seen.first[2] > seen.first[1] && seen.first[3] > seen.first[2]);
    // 0.6 s waiting, on the fake clock's 1/60 s frames: about 36 frames from 2 to 3
    EXPECT(seen.first[3] - seen.waiting >= 33 && seen.first[3] - seen.waiting <= 39);
    // The MFD's armor display (mode 3, the ported LanceDisplay): laid out for
    // the player's mech when the HUD was given it (0x004423c0 → 0x0047eba0).
    EXPECT(seen.gauges.mfd_armor_most > 0 && seen.gauges.mfd_armor_w > 0);
    EXPECT(seen.ai_named > 0);
    EXPECT(seen.radar_frames == 9 && seen.blips_asleep == 0 && seen.blip_frames >= 8);
    EXPECT(std::abs(seen.blip_d - seen.blip_want) <= 1.5f && seen.blip_off <= 1);  // straight ahead
    EXPECT(seen.blips_locked == 0);
    const mech3::HudReport& g = seen.gauges;
    std::printf("  equipment %d lines (%d wanted), speed %.1f km/h, throttle bar %d up, fuel %d, heat unlit %d\n",
                g.equip_lines, g.equip_wanted, g.speed_kmh, g.throttle_up, g.fuel_shown, g.heat_unlit);
    std::printf("  message panel made %d, up at frame %d, down and hidden at %d\n", seen.panel_made, seen.panel_up,
                seen.panel_down);
    EXPECT(seen.panel_made && seen.panel_up > 12 && seen.panel_down > seen.panel_up + 50);
    EXPECT(g.equip_lines == g.equip_wanted);  // a line for each part, by kind (none: hidden)
    EXPECT(g.throttle_up > 0 && g.speed_kmh > 0.0f);  // the bar up from its bottom, the speed in km/h
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// The aim from the HUD (0x0041ce60, 0x00444350, 0x0041cd40): the look
// centred, the reticle's ray through the screen's middle finds the Owens 80 m
// ahead, and the weapons aimed there hit it with no other help.
TEST(mission_c1m1_player_aims_with_the_reticle_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_fire_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    // The HUD's silhouettes as distinct stand-ins (headless runs load no 2D
    // images), set before the HUD is made, so TARGET's picks can be told apart.
    static zimg::Texture stand_in[8][5];
    mech3::DamageSet& ds = mech3::damage_set();
    for (int i = 0; i < 8; ++i)
      for (int k = 0; k < 5; ++k) ds.parts[i].image[k] = &stand_in[i][k];
    ds.loaded = true;
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      int frames = 0;
      mech3::LaunchedMech owens_before, owens_after, player_before, player_peak;
      int32_t flying_max = 0, hot_group = -1;
      int aims = 0, hits = 0;
      float nearest = 1e9f;
      int32_t reticle[2] = {-1, -1};
      int locked = 0;
      bool pitching = false;
      mech3::LaunchedMech owens_at_lock;
      bool targeted = false;  // TARGET on the Owens, showing its damage (hud.md section 16)
      int damaged_at_start = -1, damaged_max = 0, damage_mode = 0, first_damaged = 0;
      float lowest = 2.0f;
    } seen;
    void* player = nullptr;
    void* owens = nullptr;
    uint32_t fire = 0, pitch = 0, pitch_up = 0;
    bool raising = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        owens = made == 1 ? ai[0].mech : nullptr;
        fire = zin::key1_of(0x3d) & 0xff;  // CMD_FIRE
        pitch = zin::key1_of(5) & 0xff;    // CMD_PITCH_DOWN
        pitch_up = zin::key1_of(4) & 0xff;  // CMD_PITCH_UP
        // 80 m west of the Owens, on the ground, facing east (+x: yaw -90).
        mech3::LaunchedMech o;
        mech3::describe_mech(owens, &o);
        const float x = o.pos[0] - 80.0f, z = o.pos[2];
        const float at[3] = {x, ground_under(x, o.pos[1] + 50.0f, z), z};
        mech3::move_mech(player, at, -1.5707964f);
        mech3::describe_mech(owens, &seen.owens_before);
        mech3::describe_mech(player, &seen.player_before);
      } else if (seen.frames == 6) {
        keys.key(fire, true);
        seen.targeted = mech3::program_hud_target(owens, 2);
      } else if (seen.frames == 120) {
        // The first group (the machine guns) makes no heat: one that does.
        seen.hot_group = mech3::mech_select_hot_group(player);
      } else if (seen.frames > 6 && seen.frames < 400) {
        // The torso pitched down (CMD_PITCH_DOWN) until the reticle's ray
        // ends on the Owens, then kept there (in the player's torso mode the
        // pitch holds and drifts on as the controls ease off: steered back
        // up or down by where the ray ends); where the ray ends.
        mech3::LaunchedMech o;
        mech3::describe_mech(owens, &o);
        float p3[3];
        bool hit = false;
        if (mech3::program_hud_aim(seen.reticle, p3, &hit)) {
          ++seen.aims;
          seen.hits += hit;
          const float dx = p3[0] - o.pos[0], dz = p3[2] - o.pos[2];
          const float d = std::sqrt(dx * dx + dz * dz);
          if (d < seen.nearest) seen.nearest = d;
          if (seen.frames > 30) {
            mech3::LaunchedMech me;
            mech3::describe_mech(player, &me);
            const float far = std::hypot(p3[0] - me.pos[0], p3[2] - me.pos[2]);
            const float to = std::hypot(o.pos[0] - me.pos[0], o.pos[2] - me.pos[2]);
            const bool on = hit && d < 8.0f;
            if (on && !seen.locked) {
              seen.locked = seen.frames;
              mech3::describe_mech(owens, &seen.owens_at_lock);
            }
            const bool down = !on && (!seen.locked || far > to), up = !on && seen.locked && far <= to;
            if (down != seen.pitching) keys.key(pitch, down), seen.pitching = down;
            if (up != raising) keys.key(pitch_up, up), raising = up;
          }
        }
        zwep::PoolStats ps;
        zwep::pool_stats(&ps);
        if (ps.flying > seen.flying_max) seen.flying_max = ps.flying;
        mech3::LaunchedMech m;
        if (mech3::describe_mech(player, &m) && m.heat > seen.player_peak.heat) seen.player_peak = m;
        mech3::HudReport hr;
        if (mech3::program_hud_report(&hr)) {
          if (seen.damaged_at_start < 0) seen.damaged_at_start = hr.target_damaged;
          if (hr.target_damaged > seen.damaged_max) seen.damaged_max = hr.target_damaged;
          if (hr.target_damaged > seen.damaged_at_start && !seen.first_damaged) seen.first_damaged = seen.frames;
          seen.damage_mode = hr.target_mode;
          seen.lowest = std::min(seen.lowest, hr.target_internal);
        }
      } else if (seen.frames == 400) {
        keys.key(fire, false);
        mech3::describe_mech(owens, &seen.owens_after);
      }
    };
    CountingPump pump;
    pump.limit = 900;
    app.run(pump);
    std::printf("  aims %d, hits %d, nearest %.1f m, locked at frame %d, Owens armor %.3f -> %.3f -> %.3f\n", seen.aims,
                seen.hits, seen.nearest, seen.locked, seen.owens_before.armor, seen.owens_at_lock.armor,
                seen.owens_after.armor);
    EXPECT(seen.frames >= 400);
    EXPECT(seen.aims > 300);                                    // the HUD aimed each frame
    EXPECT(seen.locked > 0 && seen.locked < 300);               // its ray found the Owens
    EXPECT(seen.flying_max > 0);                                // shots in the air
    EXPECT(seen.owens_after.armor < seen.owens_at_lock.armor);  // hit there, by the reticle's aim alone
    std::printf("  TARGET on the Owens %d (mode %d): parts showing damage %d at the start, up to %d (first at frame %d), lowest internal %.3f\n",
                seen.targeted, seen.damage_mode, seen.damaged_at_start, seen.damaged_max, seen.first_damaged, seen.lowest);
    EXPECT(seen.targeted && seen.damage_mode == 2);
    EXPECT(seen.damaged_at_start == 0 && seen.damaged_max > 0);  // the silhouette follows the hits
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::options() = mech3::Options{};
}

// Three missions in one process (docs/mission-load.md, The unload): c1
// mission 1, c1 mission 2 on the same world, then c2 mission 1, whose load
// unloads c1's world (0x00490480); each builds its mech and runs, with no
// report of nodes left over and c1's texture libraries gone.
TEST(missions_c1m1_c1m2_then_c2m1_in_one_process_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_unload_write"));
  mech3::reset_states();
  reset_process_state();  // as a new process: zSound not yet initialised
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    // Every report of a node with two parents or of a garbage node (the
    // signs of the last mission's animations left over).
    static int s_bad = 0;
    s_bad = 0;
    // c1 mission 1, c1 mission 2 (the same world: kept), c2 mission 1 (another
    // world: c1's unloaded first, 0x00490480), 60 frames each.
    struct Leg {
      int32_t index, number;
      int frames = 0;
      int32_t world = -1;
      const void* anims = nullptr;
      std::string anim_path;  // and what was read: the address may come round again
      int32_t anim_defs = 0;
      bool c1_mounted = false;
      size_t libraries = 0;
      mech3::LaunchedMech mech;
      int32_t alive = 0, left = -1;  // mechs up, then after the cleanup
      bool player_ok = false;          // the mission's player is the pilot's own mech
      bool player_cleared = false;     // and gone after the cleanup (0x004782c0(0))
      size_t voices = 0, voices_left = 99;  // playing before and after the cleanup
      bool tick_freed = false;              // the player's tick object released
    };
    auto playing = [] {  // the voices zSound's snapshot (0x0056eb10) finds playing
      zsnd::Snapshot* s = zsnd::snapshot();
      const size_t n = s->size() - 1;  // the master volume's record first
      zsnd::free_snapshot(s);
      return n;
    };
    Leg legs[3] = {{0, 1}, {0, 2}, {1, 1}};
    int leg = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || leg > 2) return;
      Leg& g = legs[leg];
      if (++g.frames == 1) {
        if (leg == 0)
          zerr::set_sink([](int, const char*, int, const char* m) {
            if (std::strstr(m, "Multiple parents") || std::strstr(m, "Unrecognized node")) ++s_bad;
          });
        g.world = mech3::missions().current;
        g.anims = zeff::anim().defs;
        g.anim_path = zeff::anim().path;
        g.anim_defs = zeff::anim().def_count();
        g.libraries = zimg::library_list(0).size();
        for (const std::string& m : zutl::mounts().read) g.c1_mounted |= m.find("/zbd/c1") != std::string::npos;
      }
      if (g.frames == 60) {
        mech3::launched_mech(&g.mech);
        g.alive = mech3::mechs_alive();
        // A voice nothing else owns (the radio's com_failed), which only the
        // cleanup's snapshot stop reaches.
        if (zsnd::Object* o = zsnd::find("com_failed")) zsnd::play(o, 1.0f);
        g.voices = playing();
        g.player_ok = mech3::mission_player_mech() != nullptr &&
                      mech3::mission_player_mech() == engine.object_of(engine.field32(engine.pilot(), 0x14));
        if (++leg > 2) return;
        // The mission over: its cleanup, then the next through the Load state.
        mech3::missions().cleanup(false);
        g.left = mech3::mechs_alive();
        g.voices_left = playing();
        g.tick_freed = mech3::player_state().tick == 0;  // 0x0049dc40's [0x00609a00]
        g.player_cleared = mech3::mission_player_mech() == nullptr && engine.field32(engine.pilot(), 0x14) == 0;
        mech3::GameEngine::Launch next;
        next.index = legs[leg].index, next.number = legs[leg].number;
        engine.set_launch(next);
        app.replace(&mech3::states().load, 0);
      }
    };
    CountingPump pump;
    pump.limit = 1800;
    app.run(pump);
    for (const Leg& g : legs) EXPECT(g.frames >= 60 && g.mech.parts > 0);  // each built its mech and ran
    // The cleanup deletes every mech (the pilot's, then zEntity's list):
    // none carried into the next mission, none left at the end.
    for (int i = 0; i < 2; ++i) EXPECT(legs[i].alive > 0 && legs[i].left == 0 && legs[i].player_cleared);
    for (const Leg& g : legs) EXPECT(g.player_ok);  // never a stale or an AI mech
    // The cleanup's sounds (0x00570410, 0x0056eb10, 0x0056f0a0, 0x0056f190):
    // what played (the engines, the start animation's) is stopped.
    for (int i = 0; i < 2; ++i) EXPECT(legs[i].voices > 0 && legs[i].voices_left == 0 && legs[i].tick_freed);
    EXPECT(legs[2].alive > 0 && mech3::mechs_alive() == 0);
    EXPECT(legs[0].world == 0 && legs[1].world == 0 && legs[2].world == 1);
    EXPECT(legs[1].anims == legs[0].anims && legs[1].anim_path == legs[0].anim_path);  // the same world keeps them
    // c2 read its own (0x0050b380 freed c1's). Not by address: the allocator
    // may hand c1's freed block to c2's table (it did in the full run).
    EXPECT(legs[2].anim_path != legs[0].anim_path && legs[2].anim_defs != legs[0].anim_defs);
    EXPECT(legs[0].c1_mounted && !legs[2].c1_mounted);  // c2's files, not c1's
    EXPECT(legs[2].libraries == legs[0].libraries);     // c1's texture libraries gone (0x00518b70)
    EXPECT(s_bad == 0);                               // nothing of c1 left in c2
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}


void place_survey_begin(const char* points);
void place_survey_leg(int leg, const char* world);
int place_survey_end(std::FILE* f, const char* const* leg_names, int* moved, int* points, int* point_moves);

// Every campaign mission's mech placements (0x004161c0) with the original
// step (y + 10) and the fixed one (10): where each lands either way, and the
// polygons under it. MW3_PLACE_REPORT=<file> writes the list;
// MW3_PLACE_POINTS=<file> adds every aiv.zrd record's place in every reader
// archive (campaign, instant action, multiplayer), from
// tools/analysis/aiv_points.py (docs/mission-start.md section 11).
TEST(placements_on_every_campaign_mission_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("placements_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  static const char* const kDir[] = {"c1", "c2", "c3", "c4", "c4b", "t1"};
  struct Leg {
    int32_t index, number;
    int frames = 0;
  };
  std::vector<Leg> legs;
  const int first[] = {1, 1, 1, 1, 4, 1}, last[] = {4, 4, 6, 3, 6, 4};
  for (int d = 0; d < 6; ++d)
    for (int n = first[d]; n <= last[d]; ++n) legs.push_back({d, n});
  std::vector<std::string> names;
  for (const Leg& g : legs) names.push_back(std::string(kDir[g.index]) + " m" + std::to_string(g.number));
  std::vector<const char*> cnames;
  for (const std::string& n : names) cnames.push_back(n.c_str());
  place_survey_begin(std::getenv("MW3_PLACE_POINTS"));
  place_survey_leg(0, kDir[legs[0].index]);
  size_t leg = 0;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;
    l.index = legs[0].index, l.number = legs[0].number;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || leg >= legs.size()) return;
      if (++legs[leg].frames < 2) return;
      if (++leg >= legs.size()) return;
      place_survey_leg(static_cast<int>(leg), kDir[legs[leg].index]);
      mech3::missions().cleanup(false);
      mech3::GameEngine::Launch next;
      next.index = legs[leg].index, next.number = legs[leg].number;
      engine.set_launch(next);
      app.replace(&mech3::states().load, 0);
    };
    CountingPump pump;
    pump.limit = 6000;
    app.run(pump);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  int moved = 0, points = 0, point_moves = 0;
  const char* report = std::getenv("MW3_PLACE_REPORT");
  std::FILE* f = report ? std::fopen(report, "w") : nullptr;
  const int placed = place_survey_end(f, cnames.data(), &moved, &points, &point_moves);
  if (f) std::fclose(f);
  for (const Leg& g : legs) EXPECT(g.frames >= 2);  // every mission reached play
  EXPECT(placed > static_cast<int>(legs.size()));   // the player and the AI mechs placed
  EXPECT(moved == 0);  // the step fix moves no campaign mech (Owen's condition)
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// Playtest #2 (docs/playtest.md): the game's own message box (0x004ac150,
// dlgMessage.zrd's MESSAGEBOX) answers the pilot screen's "Ok to delete
// pilot?", run in its own loop with frames presented. A zTwoD host that
// forwards the box's loop to the engine, as the SDL one does.
namespace {
struct BoxHost : ztwod::Host {
  mech3::GameEngine* e = nullptr;
  void input_poll(uint8_t a) override {
    e->pump_events();
    e->input_poll(a);
  }
  void timer_update() override { e->timer_update(); }
  float frame_dt() override { return e->frame_dt(); }
  void video_lock() override { e->video_lock(); }
  void video_unlock() override {}
  void video_flip(const ztwod::Rect*, const ztwod::Rect*, int32_t, int32_t) override { e->video_flip(0, 0, 0, 0); }
  uint32_t show_cursor(uint32_t on) override { return e->video_show_cursor(on); }
};
}  // namespace

TEST(playtest_delete_pilot_through_the_games_message_box_headless) {
  if (!have_game()) return;
  for (const bool yes : {true, false}) {
    SavedMounts keep;
    zutl::mounts() = zutl::Mounts{};
    zutl::mount(MW3_GAME_DIR);
    zutl::set_write_dir(temp_dir("msgbox_write"));
    mech3::reset_states();
    reset_process_state();
    mech3::options() = mech3::Options{};
    FakeWindow window;
    FakeAlert alert;
    FakeClock clock;
    FakeScreen screen;
    platform::SilentSoundOutput sound;
    mech3::MountFiles files;
    mech3::Platform p;
    p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
    {
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      BoxHost host;
      host.e = &engine;
      ztwod::set_host(&host);
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      const char* name = yes ? "DoomedYes" : "DoomedNo";
      int stage = 0, box_frames = 0;
      bool asked = false;
      engine.on_present = [&](int) {
        if (ztwod::MessageBox* box = engine.message_box_open()) {  // inside the box's own loop
          asked = true;
          if (++box_frames == 3) box->buttons[yes ? 0 : 1].on_left_up();  // OK or CANCEL
          return;
        }
        if (stage == 0 && app.current() == &mech3::states().main && mech3::main_menu()) {
          const_cast<mech3::MainMenu*>(mech3::main_menu())->buttons[1].on_left_up();  // NEW CAMPAIGN
          stage = 1;
        } else if (stage == 1) {
          auto* ps = dynamic_cast<mech3::PilotState*>(app.current());
          if (!ps || !ps->screen) return;
          ps->screen->new_bar.set_text(name);
          ps->screen->new_bar.on_return();
          stage = 2;
        } else if (stage == 2) {
          auto* ps = dynamic_cast<mech3::PilotState*>(app.current());
          if (!ps || !ps->screen || !ps->screen->chosen) return;
          ps->screen->on_button(0x19, 0, 0);  // DELETE: the box runs here
          stage = 3;
        }
      };
      CountingPump pump;
      pump.limit = 400;
      app.run(pump);
      EXPECT(stage == 3 && asked && box_frames >= 3);
      EXPECT((mech3::find_pilot(name) == nullptr) == yes);  // OK deletes, CANCEL keeps
      EXPECT(alert.shown.empty());                          // no native alert
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
      ztwod::set_host(nullptr);
    }
    zrdr::close_archives(true);
    mech3::reset_states();
    reset_process_state();
    mech3::options() = mech3::Options{};
  }
}

// Playtest #3 (docs/playtest.md): Escape in a mission pushes the Pause State,
// whose screen (0x00492750) is made over a grab of the frame shown; RESUME
// (0x00492d70) goes back through the state's +0x28, and so does Escape on the
// pause screen. The frame behind is the 3D one read back (FIX (A)), not the 2D
// layer's key colour (the purple Owen saw).
TEST(playtest_escape_pauses_the_mission_and_resume_returns_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("pause_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    void set_cooperative_level(Device, uint32_t) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  // A GPU whose frame reads back as one colour.
  struct OneColour : platform::Render3D {
    int32_t create_texture(int, int, const uint8_t*, uint8_t) override { return 1; }
    void release_texture(int32_t) override {}
    void update_texture(int32_t, int, int, const uint8_t*) override {}
    void set_viewport(int, int, int, int) override {}
    void clear(bool, uint8_t, uint8_t, uint8_t) override {}
    void begin_scene() override {}
    void end_scene() override {}
    void draw(platform::Primitive, const platform::TLVertex*, int, const platform::DrawState&) override {}
    void set_fog_color(uint8_t, uint8_t, uint8_t) override {}
    void present() override {}
    bool read_back(std::vector<uint32_t>* argb, int* w, int* h) override {
      argb->assign(320 * 240, 0xff3366ccu);
      *w = 320, *h = 240;
      return true;
    }
  } gpu;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  p.render = &gpu;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    struct GrabHost : BoxHost {
      bool shown = false;  // the grab found the 3D frame
      zimg::Texture* screen_grab(int32_t) override {
        std::vector<uint16_t> v;
        shown = e->shown_frame(&v) && !v.empty() && v[0] == zvid::pack_bytes(0x33, 0x66, 0xcc);
        return nullptr;
      }
    } host;
    host.e = &engine;
    ztwod::set_host(&host);
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, number 1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    mech3::States& st = mech3::states();
    int core = 0, paused = 0, pauses = 0, resumes = 0, stage = 0;
    bool bound = false, grabbed = false, sounds = false;
    zgame::State* last = nullptr;
    engine.on_present = [&](int) {
      zgame::State* now = app.current();
      if (now == &st.pause && last != &st.pause) ++pauses, paused = 0;
      if (now == &st.core && last == &st.pause) ++resumes, core = 0;
      last = now;
      if (now == &st.core) {
        ++core;
        if (core == 5 && stage < 2) keys.key(1, true);  // Escape
        if (core == 6 && stage < 2) keys.key(1, false);
      } else if (now == &st.pause) {
        ++paused;
        auto* ps = dynamic_cast<mech3::PauseScreen*>(engine.screen_of(st.pause.screen_));
        if (!ps) return;
        if (paused == 1) {
          bound = ps->resume.owner && ps->quit.owner && ps->options.owner;
          grabbed = host.shown;
          sounds = ps->sounds != 0;
        }
        if (stage == 0 && paused == 4) {
          ps->on_button(mech3::PauseScreen::kResume, 0, 0);  // RESUME
          stage = 1;
        } else if (stage == 1 && paused == 4) {
          keys.key(1, true);  // Escape on the pause screen
        } else if (stage == 1 && paused == 5) {
          keys.key(1, false);
          stage = 2;
        }
      }
    };
    CountingPump pump;
    pump.limit = 900;
    pump.stop = [&] { return stage == 2 && resumes == 2 && core >= 5; };
    app.run(pump);
    std::printf("  pauses %d, resumes %d, bound %d, grab of the 3D frame %d\n", pauses, resumes, bound, grabbed);
    EXPECT(pauses == 2 && resumes == 2);  // Escape pauses; RESUME, then Escape, return
    EXPECT(bound);                          // dlgPause.zrd's buttons
    EXPECT(grabbed);                        // the backdrop is the frame shown
    EXPECT(sounds);                         // zSound's snapshot taken (0x0056eb10)
    EXPECT(app.current() == &st.core);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
    ztwod::set_host(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// Playtest #10 (docs/playtest.md, hud.md, Images the data lacks): the HUD
// asks for 52 images; the 40 the game ships load, and the 12 the original
// asks for too are in no shipped library (MW3 1.2 or Pirate's Moon), so they
// stay null as in the original (0x005179d0 returns 0): a destroyed part's
// black silhouette and the arrows to a target that is neither ally nor enemy.
TEST(playtest_hud_images_the_data_lacks) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(std::string(MW3_GAME_DIR) + "/zbd");
  zimg::state() = zimg::State{};
  zimg::state().display_format = 6;
  for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
  zimg::register_image_library(1);
  const char* const parts[] = {"head", "ltorso", "rtorso", "ctorso", "larm", "rarm", "lleg", "rleg"};
  const char* const dirs[] = {"left", "right", "up", "down"};
  int found = 0, missing = 0;
  auto check = [&](const std::string& n, bool shipped) {
    const bool got = zimg::fetch_image(n.c_str()) != nullptr;
    if (got != shipped) std::printf("  %s: %s\n", n.c_str(), got ? "found" : "missing");
    EXPECT(got == shipped);
    (got ? found : missing) += 1;
  };
  for (const char* p : parts) {
    for (const char* c : {"blue", "grn", "yel", "red"}) check(std::string("dmg_") + p + "_" + c, true);
    check(std::string("dmg_") + p + "_blk", false);
  }
  for (const char* d : dirs) {
    check(std::string("hud_enarrow_") + d, true);
    check(std::string("hud_allyarrow_") + d, true);
    check(std::string("hud_othererrow_") + d, false);
  }
  EXPECT(found == 40 && missing == 12);
  for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
  zimg::state() = zimg::State{};
}

// Playtest #20, #13: the HUD keys in a c1m1 mission. Keypad 0 locks the zoom
// window open with one click (it used to click every frame and show
// nothing), again closes it; Z held shows it, let go hides it, one click each
// way; R the large radar, D the MFD's next mode, F12 the objectives, Alt+R
// at 1000 m to passive (amber) and back to 500 m (green), H the HUD off.
// The unported census (docs/playtest.md, "Unported in c1m1"): c1m1 played
// through the program for MW3_CENSUS_SECONDS of game time (default 180) in the
// Thor (it has jump jets): walking, turning, targeting and firing each group,
// jumping, zooming, every look, the HUD keys, torso mode, the camera, then
// Ctrl+Q at the end. Every unported address the engine logs, with its count,
// goes to MW3_CENSUS_OUT (tab separated, the most frequent first). Runs only
// with MW3_CENSUS=1: it takes a while.
TEST(census_c1m1_unported_headless) {
  if (!have_game() || !std::getenv("MW3_CENSUS")) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("census_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  const int seconds = std::getenv("MW3_CENSUS_SECONDS") ? std::atoi(std::getenv("MW3_CENSUS_SECONDS")) : 180;
  const int total = seconds * 60;  // FakeClock: 16 ms a frame, about 60 a second
  std::map<std::string, uint32_t> counts;
  int frames = 0, quit_at = 0;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, mission 1
    l.mech = 17;                  // the Thor: jump jets
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    auto press = [&](int32_t cmd, bool down) {
      const uint32_t k = zin::key1_of(cmd);
      if (!k) return;
      const uint32_t mods[3][2] = {{zin::kAlt, 0x38}, {zin::kCtrl, 0x1d}, {zin::kShift, 0x2a}};
      for (auto& m : mods)
        if (k & m[0] && down) keys.key(m[1], true);
      keys.key(k & 0xff, down);
      for (auto& m : mods)
        if (k & m[0] && !down) keys.key(m[1], false);
    };
    auto tap = [&](int32_t cmd) { press(cmd, true), press(cmd, false); };
    // One 20 s cycle (1200 frames): at frame f of the cycle, (cmd, held frames; 0 a tap).
    struct Step { int at; int32_t cmd; int hold; };
    static const Step kCycle[] = {
        {0, 0x24, 0},                                   // full throttle
        {30, 0x02, 90}, {150, 0x03, 90},                // turn left, then right
        {260, 0x46, 0},                                 // target the next enemy
        {270, 0x2c, 0}, {275, 0x3d, 40}, {320, 0x2d, 0}, {325, 0x3d, 40}, {370, 0x2e, 0}, {375, 0x3d, 40},
        {420, 0x2f, 0}, {425, 0x3d, 40}, {470, 0x30, 0}, {475, 0x3d, 40},  // each group fired
        {520, 0x28, 0}, {530, 0x39, 0},                 // the fire mode, an alpha strike
        {560, 0x0c, 70},                                // a jump
        {660, 0x14, 40}, {710, 0x15, 0}, {740, 0x15, 0},  // the zoom, its lock on and off
        {760, 0x62, 0}, {770, 0x63, 0}, {780, 0x64, 0}, {790, 0x65, 0}, {800, 0x67, 0}, {810, 0x66, 0},  // looks
        {820, 0x16, 0}, {840, 0x16, 0}, {850, 0x17, 0}, {870, 0x17, 0},  // torso mode, free look
        {880, 0x68, 0}, {890, 0x68, 0}, {900, 0x74, 0}, {910, 0x75, 0}, {920, 0x69, 0}, {930, 0x69, 0},
        {940, 0x43, 0}, {960, 0x43, 0}, {970, 0x6b, 0}, {990, 0x6b, 0}, {1000, 0x48, 0}, {1010, 0x49, 0},
        {1020, 0x07, 0}, {1030, 0x06, 0}, {1040, 0x59, 0}, {1050, 0x3a, 0},  // centring, a part, the AMS
        {1060, 0x61, 0}, {1100, 0x61, 0}, {1140, 0x61, 0},  // the camera round to the cockpit
        {1180, 0x1b, 0}};                               // stop
    std::vector<std::pair<int, int32_t>> releases;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      for (auto it = releases.begin(); it != releases.end();) {
        if (it->first == frames) press(it->second, false), it = releases.erase(it);
        else ++it;
      }
      if (frames < total) {
        const int c = frames % 1200;
        for (const Step& s : kCycle) {
          if (s.at != c) continue;
          if (s.hold) press(s.cmd, true), releases.push_back({frames + s.hold, s.cmd});
          else tap(s.cmd);
        }
      } else if (!quit_at) {
        quit_at = frames;
        tap(0x71);  // Ctrl+Q: the mission's quit box (0x0048dfe0)
      } else if (frames == quit_at + 30) {
        keys.key(0x1c, true), keys.key(0x1c, false);  // Enter on the box
      }
    };
    CountingPump pump;
    pump.limit = total + 2000;
    pump.stop = [&] { return quit_at && frames > quit_at + 120; };
    app.run(pump);
    counts = engine.unported_counts();
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  std::vector<std::pair<uint32_t, std::string>> rows;
  for (const auto& [k, n] : counts) rows.push_back({n, k});
  std::sort(rows.rbegin(), rows.rend());
  if (const char* out = std::getenv("MW3_CENSUS_OUT")) {
    if (FILE* f = std::fopen(out, "w")) {
      std::fprintf(f, "# c1m1, %d frames in the Core state (%d s asked), quit at %d\n", frames, seconds, quit_at);
      for (const auto& [n, k] : rows) std::fprintf(f, "%u\t%s\n", n, k.c_str());
      std::fclose(f);
    }
  }
  std::fprintf(stderr, "census: %d frames, %zu distinct unported, quit at %d\n", frames, rows.size(), quit_at);
  EXPECT(frames >= total);
}

TEST(playtest_hud_keys_zoom_radar_mfd_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("hud_keys_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    // A bound key with its modifiers (zinput.h: 0x100 Alt, 0x200 Ctrl, 0x400 Shift) down then up.
    auto press = [&](int32_t cmd, bool down) {
      const uint32_t k = zin::key1_of(cmd);
      const uint32_t mods[3][2] = {{zin::kAlt, 0x38}, {zin::kCtrl, 0x1d}, {zin::kShift, 0x2a}};
      for (auto& m : mods)
        if (k & m[0] && down) keys.key(m[1], true);
      keys.key(k & 0xff, down);
      for (auto& m : mods)
        if (k & m[0] && !down) keys.key(m[1], false);
    };
    auto tap = [&](int32_t cmd) { press(cmd, true), press(cmd, false); };
    struct At {
      mech3::HudReport r;
      int clicks = 0;
    };
    std::map<int, At> at;
    int zoom_clicks = 0, radar_clicks = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (frames == 3) mech3::program_count_sounds(0x17, &zoom_clicks);  // the zoom's click
      if (frames == 81) {  // then radar_up's
        mech3::program_count_sounds(0, nullptr);
        mech3::program_count_sounds(0x10, &radar_clicks);
      }
      static const std::map<int, int32_t> taps = {{12, 0x15}, {42, 0x15}, {82, 0x68}, {92, 0x74}, {102, 0x43},
                                                  {112, 0x6b}, {122, 0x69}, {142, 0x69}};
      if (auto it = taps.find(frames); it != taps.end()) tap(it->second);
      if (frames == 52) press(0x14, true);
      if (frames == 72) press(0x14, false);
      if (frames % 10 == 0 && frames <= 160) {
        mech3::program_hud_report(&at[frames].r);
        at[frames].clicks = frames <= 80 ? zoom_clicks : radar_clicks;
      }
    };
    CountingPump pump;
    pump.limit = 400;
    app.run(pump);
    mech3::program_count_sounds(0, nullptr);
    EXPECT(frames >= 160);
    EXPECT(engine.unported_counts().count("0x0048dfe0") == 0);  // playtest #21: the mission's quit box made
    auto& a = at;
    std::printf("hud keys: clicks %d %d %d %d %d %d %d, mfd %d -> %d, radar %.0f -> %.0f -> %.0f\n", a[10].clicks,
                a[20].clicks, a[40].clicks, a[50].clicks, a[60].clicks, a[70].clicks, a[80].clicks, a[90].r.mfd_mode,
                a[100].r.mfd_mode, a[120].r.sensor_range, a[130].r.sensor_range, a[150].r.sensor_range);
    EXPECT(!a[10].r.zoom_shown && a[10].clicks == 0);                       // no click while nothing happens
    EXPECT(a[20].r.zoom_shown && a[20].r.zoom_locked && a[20].clicks == 1);  // keypad 0: open, one click
    EXPECT(a[40].r.zoom_shown && a[40].clicks == 1);                        // stays open, no more clicks
    EXPECT(!a[50].r.zoom_shown && !a[50].r.zoom_locked && a[50].clicks == 2);  // keypad 0 again: closed
    EXPECT(a[60].r.zoom_shown && a[70].r.zoom_shown && a[70].clicks == 3);  // Z held: open, one click
    EXPECT(!a[80].r.zoom_shown && a[80].clicks == 4);                       // let go: closed, one click
    EXPECT(!a[80].r.radar_large && a[90].r.radar_large && a[90].clicks == 1);  // R, radar_up once
    EXPECT(a[100].r.mfd_mode != a[90].r.mfd_mode);                          // D
    EXPECT(a[110].r.objectives_shown != a[100].r.objectives_shown);         // F12
    EXPECT(a[110].r.hud_on && !a[120].r.hud_on);                            // H
    EXPECT(a[120].r.sensor_range == 1000.0f && !a[120].r.radar_amber);     // Alt+R: active at the top,
    EXPECT(a[130].r.sensor_range == 500.0f && a[130].r.radar_amber);       // then passive, amber,
    EXPECT(a[150].r.sensor_range == 500.0f && !a[150].r.radar_amber);      // then 500 m active, green
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// Playtest #8: in c1m1, F2's ER Laser (S) (a beam: its bar is its charge)
// fired, its line's bar follows the recharge down and back to full. It
// stayed full while WeaponCalls::hud_fire was unwired (before 258364d): the
// list only refreshes every frame once a shot marks it (hud.md section 13).
// The drawing is mw3sdltests' (sdl_program_weapon_list_shows_the_recharge).
TEST(playtest_hud_weapon_bars_recharge_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("hud_bars_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  struct Keys : platform::Input {
    std::vector<platform::KeyEvent> queue;
    uint32_t t = 0;
    void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
    bool open(Device d) override { return d == kKeyboard; }
    void close(Device) override {}
    int32_t acquire(Device) override { return 0; }
    int32_t unacquire(Device) override { return 0; }
    int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
      const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
      std::copy(queue.begin(), queue.begin() + n, out);
      queue.erase(queue.begin(), queue.begin() + n);
      *count = n;
      return 0;
    }
    int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
    int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
  } keys;
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0, partial_at = 0, full_again = 0;
    float bar_low = 1.0f, recycle_low = 1.0f;
    uint32_t fire = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (frames == 6) keys.key(zin::key1_of(0x2d) & 0xff, true), keys.key(zin::key1_of(0x2d) & 0xff, false);  // F2
      if (frames == 10) fire = zin::key1_of(0x3d) & 0xff, keys.key(fire, true);
      if (frames == 13) keys.key(fire, false);
      mech3::HudReport r;
      if (frames < 10 || !mech3::program_hud_report(&r)) return;
      recycle_low = std::min(recycle_low, r.recycle_min);
      bar_low = std::min(bar_low, r.bar_min);
      if (!partial_at && r.bars_partial) partial_at = frames;
      if (partial_at && !full_again && !r.bars_partial) full_again = frames;
    };
    CountingPump pump;
    pump.limit = 500;
    app.run(pump);
    std::printf("bars: recycle low %.3f, bar low %.3f, partial at %d, full again at %d\n", recycle_low, bar_low,
                partial_at, full_again);
    EXPECT(recycle_low < 0.5f);                          // firing drains the laser's charge
    EXPECT(bar_low == recycle_low && partial_at > 0);    // the bar shows it
    EXPECT(full_again > partial_at + 100);               // and fills back up over seconds
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The reports' formats filled with the exe's arguments (zerr::reportf; the
// exe's 0x0049bfd0 is empty, the text is the rebuild's): a button the dialog
// lacks names itself, as the mission's quit box's five do.
namespace reports {
std::string g_last;
}  // namespace reports

TEST(reports_fill_their_formats) {
  zerr::set_sink([](int, const char*, int, const char* m) { reports::g_last = m ? m : ""; });
  zerr::reportf(0x400, "f", 1, "CZSIScript::SetLoopTime( t=%.2f )", -0.5);
  EXPECT(reports::g_last == "CZSIScript::SetLoopTime( t=-0.50 )");
  {
    ztwod::Dialog d;
    ztwod::Control c;
    EXPECT(!ztwod::bind_button(&d, nullptr, &c, "MB_YES"));
    EXPECT(reports::g_last == "BindButton (MB_YES) failed");
  }
  zerr::set_sink(nullptr);
}

// The mission's objectives in text (the program's handles resolved).
static std::string objective_text(mech3::GameEngine& engine, const mech3::Objective* o, int depth) {
  std::string t(static_cast<size_t>(depth) * 2, ' ');
  const char* kinds[] = {"group", "destroy", "node_change", "reach_area", "valve_change"};
  t += kinds[o->kind];
  t += o->done ? " done" : " open";
  if (o->desc) t += " '" + *o->desc + "'";
  switch (o->kind) {
    case mech3::Objective::kDestroy: {
      auto* d = static_cast<const mech3::DestroyObjective*>(o);
      t += " " + (d->name ? *d->name : std::string("-")) + (d->entity ? " found" : " missing");
      break;
    }
    case mech3::Objective::kValveChange: {
      auto* v = static_cast<const mech3::ValveChangeObjective*>(o);
      auto* valve = static_cast<mech3::ai::Valve*>(engine.object_of(v->watch));
      t += std::string(" ") + (valve ? valve->name : "?") + " " + std::to_string(valve ? valve->get() : -1) + "->" +
           std::to_string(v->target);
      break;
    }
    default: break;
  }
  t += "\n";
  if (o->kind == mech3::Objective::kGroup)
    for (const auto& c : static_cast<const mech3::ObjectiveGroup*>(o)->items) t += objective_text(engine, c.get(), depth + 1);
  return t;
}

static std::string objectives_text(mech3::GameEngine& engine) {
  const mech3::Objectives& o = mech3::mission_objectives();
  std::string t;
  const char* names[] = {"primary", "secondary", "final", "fail"};
  const mech3::ObjectiveGroup* gs[] = {o.primary.get(), o.secondary.get(), o.final.get(), o.fail.get()};
  for (int i = 0; i < 4; ++i) {
    t += std::string(names[i]) + (gs[i] ? "\n" : " none\n");
    if (gs[i]) t += objective_text(engine, gs[i], 1);
  }
  return t;
}

// A survey (MW3_SURVEY=1): c1m1's objectives as the program reads them.
TEST(survey_c1m1_objectives_headless) {
  if (!have_game() || !std::getenv("MW3_SURVEY")) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_obj_survey"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    engine.set_launch(mech3::GameEngine::Launch{});  // c1, mission 1, the Mad Cat
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (frames == 3 || frames == 200) {
        std::printf("frame %d ended %d aborting %d\n%s", frames, mech3::missions().ended, mech3::missions().aborting,
                    objectives_text(engine).c_str());
        for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())
          if (zcls::Node* n = static_cast<zcls::Node*>(engine.object_of(t->node))) std::printf("target %s\n", n->name());
      }
    };
    CountingPump pump;
    pump.limit = 2000;
    pump.stop = [&] { return frames >= 201; };
    app.run(pump);
    mech3::missions().cleanup(true);  // the world unloaded (mechlib's trees too) for the next test
    for (const auto& [k, n] : engine.unported_counts())
      if (k.find("0x0045") == 0 || k.find("0x0040fe") == 0 || k.find("0x00479be0") == 0 || k.find("0x00446ad0") == 0)
        std::printf("unported %s x%u\n", k.c_str(), n);
  }
  mech3::set_app(nullptr);
  mech3::reset_states();
  reset_process_state();
}

// c1 mission 1 won and lost through its objectives (docs/objectives.md,
// piece 4), from the menus as campaign_menus_to_c1m1 plays it, to the
// debriefing. Won: the comm centre's satdish destroyed (its health_below
// sets comcntr_down), reach_mfb set as aiv.zrd's reg_mfb region would (the
// regions are the AI's), Exit Mission. Lost: the player moved out of the
// mission's boundary (his own vlist sets outside_c1m1 from reg_outside_c1m1),
// or the player's mech destroyed. The SRM emplacements
// are aiTurrets.zrd's (0x004a76b0, not ported): never found, they count as
// destroyed, as in the exe.
TEST(mission_c1m1_won_and_lost_by_its_objectives_headless) {
  if (!have_game()) return;
  enum Scenario { kWon, kBoundary, kDeath };
  for (const Scenario sc : {kWon, kBoundary, kDeath}) {
    const char* pilot_name = sc == kWon ? "ObjWon" : sc == kBoundary ? "ObjOut" : "ObjDead";
    SavedMounts keep;
    zutl::mounts() = zutl::Mounts{};
    zutl::mount(MW3_GAME_DIR);
    zutl::set_write_dir(temp_dir("mission_objectives"));
    mech3::reset_states();
    reset_process_state();
    mech3::missions() = mech3::Missions{};
    mech3::mech_types_forget();
    mech3::player_controls_instance() = nullptr;
    mech3::options() = mech3::Options{};
    FakeWindow window;
    FakeAlert alert;
    FakeClock clock;
    FakeScreen screen;
    platform::SilentSoundOutput sound;
    mech3::MountFiles files;
    mech3::Platform p;
    p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
    {
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      int stage = 0, core_frames = 0, ended_at = -1;
      bool primary_open = false, primary_done = false, final_done = false, fail_done = false;
      bool debrief_seen = false, debrief_failed = false, salvage_shown = false, salvage_store = false;
      bool comcntr = false;
      std::string at_end;
      auto group_done = [](const std::unique_ptr<mech3::ObjectiveGroup>& g) { return g && g->done; };
      engine.on_present = [&](int) {
        if (stage == 0 && app.current() == &mech3::states().main && mech3::main_menu()) {
          const_cast<mech3::MainMenu*>(mech3::main_menu())->buttons[1].on_left_up();  // NEW CAMPAIGN
          stage = 1;
        } else if (stage == 1) {
          auto* ps = dynamic_cast<mech3::PilotState*>(app.current());
          if (!ps || !ps->screen) return;
          ps->screen->new_bar.set_text(pilot_name);
          ps->screen->new_bar.on_return();
          ps->screen->on_button(0x18, 0, 0);  // ACCEPT
          stage = 2;
        } else if (stage == 2) {
          mech3::BriefingScreen* b = mech3::briefing_screen();
          if (!b || app.current() != &mech3::states().load || !b->loaded) return;
          b->on_button(mech3::BriefingScreen::kBtnLaunch, 0, 0);
          stage = 3;
        } else if (stage == 3 && app.current() == &mech3::states().core) {
          const mech3::Objectives& o = mech3::mission_objectives();
          ++core_frames;
          if (core_frames == 5) {
            primary_open = o.primary && !o.primary->done;
            if (sc == kWon) {
              for (mech3::ai::CZAiTarget* t : mech3::ai::target_list()) {
                zcls::Node* n = static_cast<zcls::Node*>(engine.object_of(t->node));
                if (n && std::strcmp(n->name(), "satdish") == 0) t->on_hit(0, 0, 1000.0f);
              }
            } else if (sc == kBoundary) {
              // Out past reg_outside_c1m1 (x -5145..-358): the player's own
              // vlist trips outside_c1m1 (0x004d8990, docs/ai.md 17.5).
              const float at[3] = {-100.0f, ground_under(-100.0f, 600.0f, -1225.0f), -1225.0f};
              mech3::move_mech(mech3::mech_mission_player(), at, 0.0f);
            } else {
              mech3::mech_kill(mech3::mech_mission_player());
            }
          }
          if (core_frames == 15 && sc == kWon) {
            mech3::ai::Valve* v = mech3::ai::valve_get("comcntr_down");
            comcntr = v && v->get() == 1;
            primary_done = group_done(o.primary);
            mech3::ai::valve_get("reach_mfb")->set(1);
          }
          if (mech3::missions().ended && ended_at < 0) {
            ended_at = core_frames;
            final_done = group_done(o.final);
            fail_done = group_done(o.fail);
            at_end = objectives_text(engine);
            if (sc == kWon) mech3::missions().on_command(0x71);  // Exit Mission
          }
          if (ended_at >= 0) stage = 4;
        } else if (stage == 4 && app.current() == &mech3::states().end_win) {
          auto* d = dynamic_cast<mech3::DebriefScreen*>(engine.screen_of(mech3::states().end_win.screen_));
          if (!d) return;
          debrief_seen = true;
          debrief_failed = engine.mission_failed();
          salvage_shown = !(d->salvage.flags & ztwod::kHidden);
          salvage_store = d->salvage.store == engine.mission_salvage_store();
          stage = 5;
        }
      };
      CountingPump pump;
      pump.limit = 3000;
      pump.stop = [&] { return stage == 5; };
      app.run(pump);
      EXPECT(primary_open);
      EXPECT(ended_at > 0);
      EXPECT(debrief_seen);
      if (sc == kWon) {
        EXPECT(comcntr && primary_done);  // the satdish's death set the valve; the turrets count destroyed
        EXPECT(final_done && !fail_done);
        EXPECT(!debrief_failed && salvage_shown && salvage_store);
      } else {
        if (sc == kBoundary) EXPECT(fail_done);  // the fail group; the death is the mission's own loss
        EXPECT(debrief_failed && !salvage_shown);
      }
      if (std::getenv("MW3_SURVEY"))
        std::printf("scenario %d: ended at core frame %d, failed %d\n%s", sc, ended_at, debrief_failed,
                    at_end.c_str());
      mech3::missions().cleanup(false);
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
    }
    zrdr::close_archives(true);
    mech3::reset_states();
    reset_process_state();
    mech3::options() = mech3::Options{};
  }
}

namespace mech3 {
float& lod_scale();  // mech_data.h (its Handle clashes with this file's)
}

// The options' effects detail and object detail (playtest census #6 and #8):
// through the program's hooks, zEffect's "if" level (0x00508fb0, counted the
// other way) and the mechs' range scale (0x00430440).
TEST(options_effects_and_mech_detail_reach_their_modules) {
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  mech3::GameEngine engine(p, "mech3.exe -nofmv");
  engine.quiet = true;
  const int32_t level0 = zeff::anim().level;
  const float scale0 = mech3::lod_scale();
  const int32_t want_level[3] = {2, 1, 0};
  const float want_scale[3] = {1.0f, 0.75f, 0.0f};
  for (int v = 0; v < 3; ++v) {
    mech3::set_effects_level(v);
    EXPECT(zeff::anim().level == want_level[v]);
    mech3::set_object_lod(v);
    EXPECT(mech3::lod_scale() == want_scale[v]);
  }
  EXPECT(engine.unported_counts().count("0x00508fb0") == 0);
  EXPECT(engine.unported_counts().count("0x00430440") == 0);
  zeff::anim().level = level0;
  mech3::lod_scale() = scale0;
}

// The options' "shadow parts" (census #9, docs/shade.md): c1 mission 1 with
// it on. The mission start applies it (0x004e6900): the shading is on with
// the sun, every mech has its shade, the turn goes round, and the scene
// walk's calls leave the sun's diffuse as it was after each frame.
TEST(mission_c1m1_mech_sun_shading_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_shade"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    engine.set_launch(mech3::GameEngine::Launch{});
    app.post_intro_ = &mech3::states().load;
    mech3::set_shadow_parts(1);
    int frames = 0, turns_seen = 0, last_turn = -1;
    bool on = false, shaded_player = false, diffuse_kept = true;
    float sun_diffuse = -1, player_level = -1;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      const mech3::ShadeGlobals& g = mech3::shade_globals();
      if (frames == 3) {
        on = g.on && g.sun;
        sun_diffuse = mech3::shade_hooks().sun_diffuse(g.sun);
      }
      if (frames > 3 && g.sun && mech3::shade_hooks().sun_diffuse(g.sun) != sun_diffuse) diffuse_kept = false;
      if (g.turn != last_turn) ++turns_seen, last_turn = g.turn;
      if (frames == 60) {
        if (auto* m = static_cast<mech3::SunShade*>(mech3::mech_shade_of(mech3::mech_mission_player()))) {
          shaded_player = true;
          player_level = m->level.current;
        }
      }
    };
    CountingPump pump;
    pump.limit = 2000;
    pump.stop = [&] { return frames >= 61; };
    app.run(pump);
    mech3::missions().cleanup(true);  // the world unloaded (mechlib's trees too) for the next test
    EXPECT(on);
    EXPECT(shaded_player);
    EXPECT(turns_seen > 1 || mech3::shade_globals().count == 1);
    EXPECT(diffuse_kept);  // each draw's swap is undone after it
    EXPECT(engine.unported_counts().count("0x004e6900") == 0);
    EXPECT(engine.unported_counts().count("0x004e6480") == 0);
    if (std::getenv("MW3_SURVEY"))
      std::printf("  sun diffuse %g, shades %d, the player's level %g\n", sun_diffuse, mech3::shade_globals().count,
                  player_level);
    mech3::set_shadow_parts(0);
  }
  mech3::set_app(nullptr);
  mech3::reset_states();
  reset_process_state();
}

// A survey (MW3_SURVEY=1): each campaign world's sun diffuse (the light's
// +0x9c), which a lit mech's shade does not follow (docs/shade.md).
TEST(survey_world_sun_diffuse_headless) {
  if (!have_game() || !std::getenv("MW3_SURVEY")) return;
  // One world (MW3_SURVEY_WORLD, 0..5) or the first n (MW3_SURVEY_WORLDS),
  // each in an engine of its own, its mission cleaned up with the world.
  const char* only = std::getenv("MW3_SURVEY_WORLD");
  const char* upto = std::getenv("MW3_SURVEY_WORLDS");  // several in one process (the material_copy repro)
  for (int index = only ? std::atoi(only) : 0; index < (only ? std::atoi(only) + 1 : upto ? std::atoi(upto) : 1);
       ++index) {
    SavedMounts keep;
    zutl::mounts() = zutl::Mounts{};
    zutl::mount(MW3_GAME_DIR);
    zutl::set_write_dir(temp_dir("survey_sun"));
    mech3::reset_states();
    reset_process_state();
    mech3::missions() = mech3::Missions{};
    mech3::mech_types_forget();
    mech3::player_controls_instance() = nullptr;
    mech3::options() = mech3::Options{};
    FakeWindow window;
    FakeAlert alert;
    FakeClock clock;
    FakeScreen screen;
    platform::SilentSoundOutput sound;
    mech3::MountFiles files;
    mech3::Platform p;
    p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
    {
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      if (!app.init_instance()) continue;
      mech3::GameEngine::Launch l;
      l.index = index;
      engine.set_launch(l);
      app.post_intro_ = &mech3::states().load;
      mech3::set_shadow_parts(1);
      int frames = 0;
      engine.on_present = [&](int) {
        if (app.current() != &mech3::states().core || ++frames != 3) return;
        const mech3::ShadeGlobals& g = mech3::shade_globals();
        std::printf("world %d: sun %s, diffuse %g\n", index, g.sun ? "yes" : "no",
                    g.sun ? mech3::shade_hooks().sun_diffuse(g.sun) : 0.0f);
      };
      CountingPump pump;
      pump.limit = 2000;
      pump.stop = [&] { return frames >= 3; };
      app.run(pump);
      mech3::set_shadow_parts(0);
      mech3::missions().cleanup(true);  // the world unloaded with its engine (mechlib's trees too)
      app.stop_game();
      app.exit_instance();
    }
    mech3::set_app(nullptr);
  }
}

// A second world in one process (the campaign's c1 to c2, Instant Action's
// worlds): c1 mission 1 played a little and cleaned up as the debriefing's
// CONTINUE does (the world kept), then the Load state with c2's first
// mission, which unloads c1's world and must load and run.
TEST(mission_second_world_loads_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_second_world"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FakeClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    engine.set_launch(mech3::GameEngine::Launch{});
    app.post_intro_ = &mech3::states().load;
    int stage = 0, frames = 0;
    int32_t first_world = -1, second_world = -1;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (stage == 0 && frames == 20) {
        first_world = mech3::missions().current;
        engine.mission_cleanup(0);  // the debriefing's CONTINUE (0x0047d030: 0x00491780(0))
        mech3::GameEngine::Launch l;
        l.index = 1;  // c2, mission 1
        engine.set_launch(l);
        engine.replace_state(6);  // the Load state
        stage = 1, frames = 0;
      } else if (stage == 1 && frames == 20) {
        second_world = mech3::missions().current;
        stage = 2;
      }
    };
    CountingPump pump;
    pump.limit = 4000;
    pump.stop = [&] { return stage == 2; };
    app.run(pump);
    EXPECT(first_world == 0);
    EXPECT(stage == 2 && second_world == 1);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}
