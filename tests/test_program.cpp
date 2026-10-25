#include <array>
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

#include "crt/msvc_rand.h"
#include "check.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/camera.h"
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
#include "mech3/zainet_region.h"
#include "mech3/cockpit_sound.h"
#include "mech3/app_sensors.h"
#include "mech3/app_states.h"
#include "mech3/lab_screen.h"
#include "mech3/app_view.h"
#include "mech3/landmines.h"
#include "mech3/player_view.h"
#include "mech3/app_vehicle.h"
#include "mech3/controls_menu.h"
#include "mech3/mainmenu.h"
#include "mech3/mech_control.h"
#include "mech3/mission.h"
#include "mech3/mission_world.h"
#include "mech3/pocket_panel.h"
#include "mech3/mech_pocket.h"
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
#include "mech3/turret.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_firing.h"
#include "mech3/zai_list.h"
#include "mech3/zai_attack.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_target.h"
#include "mech3/zainet_inout.h"
#include "mech3/zai_anim_owner.h"
#include "mech3/zainet.h"
#include "gamez/znetwork/znet_object.h"
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
  // Every zone's ground, whatever the camera's zone list holds (as the
  // render's own probe asks, [0x005ea1d0] off).
  const int32_t mode = zmod::zones().mode;
  zmod::zones().mode = 0;
  zcls::ground_query(w, probe, 1, top, &hits);
  zmod::zones().mode = mode;
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
  crt::rand_fixed_seed() = 1u;  // the clock seeds the game's rand: a fixed seed makes the runs repeat
  // The mission animations' random cursor and range-test stagger: the
  // exe's start (0x00508fc0) refills the table but neither, so they run on
  // from mission to mission (0x0060c5f4, 0x0060c1d8: 0 in a new process).
  zeff::anim().random_index = 0, zeff::anim().stagger = 0;
  // The animation owners' records go with the world (0x004d4740), which a
  // test that resets the missions never unloads.
  mech3::ai::anim_owner_free();
  // And the interp's ScrollAlways list and holder, freed by the world's
  // unload (0x005964c0(1)): a new process's (0x005962a0).
  zint::scroll_list_init();
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
// Two missions in one session (docs/mission-frame.md, The cleanup): c1
// mission 1 started and cleaned up, then mission 4 played. The cleanup frees
// the regions (0x004c5fa0); unfreed, mission 4's aiv records found mission
// 1's regions by name, and the player's "mission_start" trigger (inside its
// region) never fired.
TEST(mission_after_another_gets_its_own_regions_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_twice_write"));
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
    mech3::GameEngine::Launch first;  // c1 mission 1
    engine.set_launch(first);
    mech3::missions().start();
    EXPECT(!mech3::missions().failed && !mech3::ai::regions().empty());
    mech3::missions().cleanup(false);
    EXPECT(mech3::ai::regions().empty());  // freed with the mission
    mech3::GameEngine::Launch second;  // then mission 4, played
    second.number = 4;
    engine.set_launch(second);
    app.post_intro_ = &mech3::states().load;
    int frames = 0, set_at = -1;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      mech3::ai::Valve* v = mech3::ai::valve_find("mission_start");
      if (set_at < 0 && v && v->get() == 1) set_at = frames;
    };
    CountingPump pump;
    pump.limit = 200;
    app.run(pump);
    EXPECT(frames > 30);
    EXPECT(set_at >= 1 && set_at <= 3);  // the player's trigger, on his first ticks
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

// The mission's end (0x00491780; docs/mission-frame.md, The cleanup): c1
// mission 1 played, then cleaned up without the unload. The animations'
// instances with their own node tree (0x0050b2f0) are freed, and one at the
// player's node made from such a copy stays (FIX (bug): the exe's 0x005097c0
// gives it the copy's flag, so its node's tree would go too); every model
// nothing holds is released and every material no model uses freed
// (0x005394c0(1, 1)); the mechs' end (0x00414000) closes the motion archive,
// deletes the radar blips and drops the scripts' path. Mission 2 then loads
// with its motions open again. Before the port all of them were kept.
TEST(mission_end_frees_what_the_next_does_not_need_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_end_write"));
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
  auto copies = [](bool own_tree) {
    int n = 0;
    const zeff::AnimState& a = zeff::anim();
    for (int32_t i = 0; i < a.def_count(); ++i)
      for (const zeff::AnimDef* c = a.defs[i].instance; c; c = c->instance)
        n += ((c->get<uint32_t>(0x94) & 0x20000) != 0) == own_tree;
    return n;
  };
  int others_before = -1, others_after = -1;
  auto idle_models = [] {  // slots in use that nothing holds
    static const uint8_t empty[sizeof(zmod::Model::header)] = {};
    int n = 0;
    for (const zmod::Model& m : zmod::models().slots)
      n += m.count(0xc) == 0 && std::memcmp(m.header, empty, sizeof empty) != 0;
    return n;
  };
  auto unused_materials = [] {  // in use, flagged none of 0x80 0x40 0x20 0x08, and on no model
    std::set<const zmod::Material*> used;
    for (const zmod::Model& md : zmod::models().slots)
      for (uint32_t i = 0; md.polygons && i < md.count(0x10); ++i) used.insert(md.polygons[i].material);
    zmod::MaterialTable& t = zmod::materials();
    int n = 0;
    for (int32_t i = 0; i < t.capacity; ++i) n += (t.at(i).flags & 0xe8) == 0 && !used.count(&t.at(i));
    return n;
  };
  auto freed_but_used = [] {  // a model's material on the free list
    int n = 0;
    for (const zmod::Model& md : zmod::models().slots)
      for (uint32_t i = 0; md.polygons && i < md.count(0x10); ++i)
        n += md.polygons[i].material && (md.polygons[i].material->flags & 0x20) != 0;
    return n;
  };
  int copies_before = -1, copies_after = -1, idle_before = -1, idle_after = -1, unused_before = -1, unused_after = -1;
  int bad_after = -1, materials_before = -1, materials_after = -1, frames2 = 0, up = -1;
  bool blips_before = false, blips_after = true, motion_before = false, motion_after = true, path_after = true;
  bool blips_again = false, motion_again = false;
  bool path_before = false, unused_bool = false;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch first;  // c1 mission 1, played
    engine.set_launch(first);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      if (++frames == 10) {
        // Three impacts as a shot's (0x00508aa0, zWeapon's effect): each
        // instance copies the def's tree (its +0x94 bit 17).
        if (zeff::AnimDef* d = zeff::anim_find("impact_missle_mech")) {
          for (int i = 0; i < 3; ++i) {
            const float at[3] = {static_cast<float>(i), 0, 0}, rot[3] = {}, scale[3] = {1, 1, 1};
            zeff::anim_start_at(d, nullptr, at, rot, scale);
          }
        }
        mech3::mechs_make_blips();
      }
      if (frames == 11) {
        // A model made and never held (as the script's ModelNew leaves one).
        if (zmod::Model* m = zmod::model_alloc()) m->header[0] = 'x';
        // And an instance on the player's node (0x005097c0, as 0x00508760
        // makes one): no tree of its own, so it stays.
        if (zeff::AnimDef* d = zeff::anim_find("impact_missle_mech")) {
          zeff::AnimDef* last = d;
          while (last->instance) last = last->instance;
          auto* node = static_cast<zcls::Node*>(mech3::mech_node(mech3::mech_mission_player()));
          if (node) last->instance = zeff::anim_instance(last, node);
        }
      }
      if (frames != 12) return;
      copies_before = copies(true), others_before = copies(false), idle_before = idle_models(), unused_before = unused_materials();
      materials_before = zmod::materials().count;
      mech3::mechs_end_state(&blips_before, &motion_before, &path_before);
    };
    CountingPump pump;
    pump.stop = [&] { return frames >= 12; };
    pump.limit = 2000;
    app.run(pump);
    up = mech3::missions().loaded ? 1 : 0;
    mech3::missions().cleanup(false);
    copies_after = copies(true), others_after = copies(false), idle_after = idle_models(), unused_after = unused_materials();
    materials_after = zmod::materials().count, bad_after = freed_but_used();
    mech3::mechs_end_state(&blips_after, &motion_after, &path_after);
    mech3::GameEngine::Launch second;  // then mission 2, loaded
    second.number = 2;
    engine.set_launch(second);
    mech3::missions().start();
    frames2 = mech3::missions().failed ? 0 : 1;
    mech3::mechs_end_state(&blips_again, &motion_again, &unused_bool);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  if (std::getenv("MW3_END_LOG"))
    std::fprintf(stderr, "END others %d/%d up %d copies %d/%d idle models %d/%d unused materials %d/%d materials %d/%d bad %d blips %d/%d motion %d/%d path %d again %d %d frames %d\n",
                 others_before, others_after, up, copies_before, copies_after, idle_before, idle_after, unused_before, unused_after, materials_before,
                 materials_after, bad_after, blips_before, blips_after, motion_before, motion_after, path_after,
                 blips_again, motion_again, frames2);
  EXPECT(copies_before > 0 && copies_after == 0 && others_before > 0 && others_after == others_before);
  EXPECT(idle_before > 0 && idle_after == 0);
  EXPECT(unused_before > 0 && unused_after == 0 && materials_after < materials_before && bad_after == 0);
  EXPECT(motion_before && !motion_after && blips_before && !blips_after && !path_after);
  EXPECT(frames2 == 1 && motion_again);
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The nets' begin (0x004d52b0, docs/ai.md The nets' begin and end): every
// znet object's vt+0x2c in the registry's order. The nav waynet (made by the
// mission reset 0x00490720) comes before the mcbwaynet, so its begin
// (0x00479a50) finds the "waypoint" marker and selects its point 0, and the
// mcbwaynet's finds it taken; the nav waynet's frame keeps the marker on its
// selected point. c1 mission 1, c2 mission 3, c4b mission 5 (no mcbwaynet).
TEST(missions_waynets_begin_in_the_registrys_order_headless) {
  if (!have_game()) return;
  for (const auto& [index, number] : std::vector<std::pair<int, int>>{{0, 1}, {1, 3}, {4, 5}}) {
    SavedMounts keep;
    zutl::mounts() = zutl::Mounts{};
    zutl::mount(MW3_GAME_DIR);
    zutl::set_write_dir(temp_dir("mission_wn_write"));
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
    std::vector<std::string> labels;
    bool marker = false, on_nav = false;
    {
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      mech3::GameEngine::Launch l;
      l.index = index, l.number = number;
      engine.set_launch(l);
      app.post_intro_ = &mech3::states().load;
      int frames = 0;
      engine.on_present = [&](int) {
        if (app.current() != &mech3::states().core || ++frames != 2) return;
        const char* names[8];
        int32_t pts[8];
        const int32_t n = mech3::program_waynets(names, pts, 8);
        for (int32_t k = 0; k < n && k < 8; ++k) labels.push_back(names[k]);
        float m[3], q[3];
        marker = mech3::program_waynet_marker(m);
        on_nav = marker && mech3::program_waynet_selected(0, q) && m[0] == q[0] && m[1] == q[1] && m[2] == q[2];
      };
      CountingPump pump;
      pump.stop = [&] { return frames >= 2; };
      pump.limit = 2000;
      app.run(pump);
      mech3::missions().cleanup(false);
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
    }
    const std::string nav = "waynet_" + std::to_string(number);
    EXPECT(!labels.empty() && labels[0] == nav);
    EXPECT(index == 4 ? labels.size() == 1 : labels.size() == 2 && labels[1] == "mcbwaynet");
    EXPECT(marker && on_nav);
    zrdr::close_archives(true);
    mech3::reset_states();
    reset_process_state();
    mech3::options() = mech3::Options{};
  }
}

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
    EXPECT(count("0x004589d0") == 0);  // mobilehq_01..03 (over net 2) are the port's (mech_fieldbase)
    EXPECT(count("0x004ac320") == 0);  // the basic vehicles are the port's (app_vehicle.cpp)
    EXPECT(count("0x004ac860") == 1);  // the stealth entity's block (the vehicles' controllers are made)
    // mobilehq_01..03, ttruck_01, ttruck_02, apc_01, apc_02: AI-driven, on
    // the ground at their records' places, in the world with their own nodes,
    // set up from vehicle.zrd (health, motion block, weapons).
    {
      const std::vector<void*> vs = mech3::program_vehicles();
      EXPECT(vs.size() == 7);
      int named = 0;
      for (void* p : vs) {
        mech3::VehicleSummary s;
        EXPECT(mech3::describe_vehicle(p, &s));
        EXPECT(s.in_world && s.ai_controls && s.owned);
        const std::string name = s.name ? s.name : "";
        named += name == "ttruck_01" || name == "ttruck_02" || name == "apc_01" || name == "apc_02" ||
                 name.rfind("mobilehq_0", 0) == 0;
        EXPECT(s.health > 0.0f && s.health == s.health_max);
        // TRACK on four supports; the model's collide nodes give the twelve
        // collision points but no count (+0x214 stays 0, as the exe's).
        EXPECT(s.mode == 3 && s.supports == 4 && s.points == 0);
        EXPECT(s.team != 0 && s.active);
        EXPECT(std::isfinite(s.pos[1]));
      }
      EXPECT(named == 7);
    }
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
      // the terrain there is at 3; placed 0.01 above it (0x004161c0), then its
      // powered-down start's one 0.1 s update (0x0049fa20) sets it down on it.
      EXPECT(m.in_world);
      EXPECT(m.pos[0] == -1767.7f && m.pos[2] == -1229.0f);
      EXPECT(std::fabs(m.pos[1] - ground_under(m.pos[0], 103.0f, m.pos[2])) < 1e-4f);
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
          zwep::set_query_zone(0xffffff01u), zwep::run_state().tick_zone = 0;  // the weapon tick copies it ({1, all})
        }, false);
        watcher(3, [](zcls::Node*) { g_watch.add(zwep::run_state().tick_zone == 0xffffff01u ? "3>" : "3>no-tick"); }, true);
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

namespace {
int light_errors;  // zClass's "ERROR deleting light" reports
}  // namespace

// A laser (IMPACT HEAT) hit leaves zWeapon's glow on a c1 mission 1 truck
// (0x00586a30), the first from a real beam fired by the player (its ray, the
// truck's node, its handler): one of the pool's 16 lights, the weapon's +0x1d4 colour,
// the pool's range 0.1 to 0.2, under the truck's node; a second hit keeps
// it; the truck's death gives it back (0x00586af0). Another truck hit and
// left alive gives its light back when the cleanup frees it (FIX (bug): the
// exe leaves it for zClass to refuse). Run under ASan too.
// docs/vehicles.md, The glow.
TEST(mission_c1m1_laser_leaves_its_glow_on_a_truck_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_glow_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      const char* weapon = nullptr;
      int pool0 = -1, pool_hit = -1, pool_end = -1, frames = 0;
      bool on_node = false, colour = false, range = false, kept = false, returned = false, off_node = false;
      bool second_lit = false, killed = false, lit_by_shot = false;
    } seen;
    auto pool_size = [] {
      int n = 0;
      for (zwep::Handle l = zwep::run_state().lights; l && n < 64; l = zwep::run_hooks().link(l)) ++n;
      return n;
    };
    // The first hit is a real shot: the player, put 40 m from the truck, fires
    // the laser's beam at it (0x00584a70, 0x00581230); the beam's ray meets
    // the truck's node, whose handler damages it and lights it (0x00586a30).
    std::vector<void*> trucks;
    zwep::Beam* beam = nullptr;
    static zwep::Vec3f3 s_from, s_dir;
    zcls::Node* light = nullptr;
    bool done = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || done) return;
      ++seen.frames;
      if (seen.frames == 3) {
        for (const zwep::WeaponDef& d : zwep::state().defs)
          if ((d.flags & zwep::kTargetHeat) && !(d.flags & zwep::kFreeze)) {
            seen.weapon = d.name;
            break;
          }
        for (void* v : mech3::program_vehicles()) {
          mech3::VehicleSummary vs;
          if (mech3::describe_vehicle(v, &vs) && vs.name && std::strstr(vs.name, "truck")) trucks.push_back(v);
        }
        if (!seen.weapon || trucks.size() < 2) return;
        seen.pool0 = pool_size();
        float at[3], me[3], to[3];
        mech3::node_world_cached(mech3::program_vehicle_node(trucks[0]), at);
        const float place[3] = {at[0] + 40.0f, ground_under(at[0] + 40.0f, at[1] + 30.0f, at[2]), at[2]};
        void* player = mech3::mech_mission_player();
        mech3::move_mech(player, place, 0.0f);
        mech3::node_world_cached(mech3::mech_centroid_of(player), me);
        mech3::node_world_cached(mech3::program_vehicle_centroid(trucks[0]), to);
        s_from = {me[0], me[1], me[2]};
        const float dx = to[0] - me[0], dy = to[1] - me[1], dz = to[2] - me[2];
        const float n = std::sqrt(dx * dx + dy * dy + dz * dz);
        s_dir = {dx / n, dy / n, dz / n};
        beam = zwep::beam_make(zwep::find_def(seen.weapon), mech3::mech_node_of(player), nullptr, &s_from, &s_dir, 1);
        zwep::beam_start(beam);
        return;
      }
      if (!beam) return;
      light = static_cast<zcls::Node*>(mech3::program_vehicle_glow_light(trucks[0]));
      if (!light && seen.frames < 40) return;
      zwep::beam_stop(beam);
      zwep::beam_free(beam);
      beam = nullptr;
      done = true;
      seen.lit_by_shot = light != nullptr;
      const zwep::WeaponDef* def = zwep::find_def(seen.weapon);
      seen.pool_hit = pool_size();
      if (light) {
        auto* node = static_cast<zcls::Node*>(mech3::program_vehicle_node(trucks[0]));
        seen.on_node = light->parent_count() == 1 && light->parents[0] == node;
        const auto* ld = static_cast<const zcls::Light*>(light->data);
        seen.colour = ld->get<float>(0xa4) == def->target_heat[3] && ld->get<float>(0xa8) == def->target_heat[4] &&
                      ld->get<float>(0xac) == def->target_heat[5];
        seen.range = ld->get<float>(0xb4) == 0.1f && ld->get<float>(0xb8) == 0.2f;
      }
      mech3::program_vehicle_hit_by(trucks[0], seen.weapon, 1.0f);  // a second hit
      seen.kept = light && mech3::program_vehicle_glow_light(trucks[0]) == light && pool_size() == seen.pool_hit;
      seen.killed = mech3::program_vehicle_kill(trucks[0], nullptr, seen.weapon);
      seen.returned = !mech3::program_vehicle_glow_light(trucks[0]);
      seen.pool_end = pool_size();
      seen.off_node = light && light->parent_count() == 0;
      mech3::program_vehicle_hit_by(trucks[1], seen.weapon, 1.0f);  // lit, and left alive to the end
      seen.second_lit = mech3::program_vehicle_glow_light(trucks[1]) != nullptr;
    };
    // The run's end cleans the mission up: the lit truck gives its light back
    // as it is freed (FIX (bug)), so zClass frees every light without complaint.
    light_errors = 0;
    zerr::set_sink([](int, const char*, int, const char* m) {
      if (m && std::strstr(m, "ERROR deleting light")) ++light_errors;
    });
    CountingPump pump;
    pump.stop = [&] { return done; };
    pump.limit = 600;
    app.run(pump);
    if (std::getenv("MW3_GLOW_LOG"))
      std::fprintf(stderr, "GLOW lit by the shot %d at frame %d; %s pool %d -> %d -> %d on node %d colour %d range %d kept %d killed %d returned %d off %d second %d\n",
                   seen.lit_by_shot, seen.frames, seen.weapon ? seen.weapon : "-", seen.pool0, seen.pool_hit, seen.pool_end, seen.on_node, seen.colour,
                   seen.range, seen.kept, seen.killed, seen.returned, seen.off_node, seen.second_lit);
    EXPECT(seen.weapon && seen.lit_by_shot && seen.pool0 > 0 && seen.pool_hit == seen.pool0 - 1);
    EXPECT(seen.on_node && seen.colour && seen.range && seen.kept);
    EXPECT(seen.killed && seen.returned && seen.off_node && seen.pool_end == seen.pool0);
    EXPECT(seen.second_lit);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zerr::set_sink(nullptr);
  EXPECT(light_errors == 0);
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The game time [0x0060991c] starts at 0 with each mission (0x0048f400 in
// the setup 0x00490ab0, before the launch): c1 mission 1 played for 300
// frames, then c1 mission 4 in the same process. Through c4's load (where
// the powered-down AI mechs' one update runs their brains) the mechs' clock
// never reads c1's time; at c4's first frame every copy (the missions', the
// mechs' and zWeapon's) agrees and is under a second, and no brain's wait
// started in the future. docs/mission-frame.md section 2.
TEST(missions_game_time_starts_at_zero_each_mission_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_clock_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int leg = 0, frames = 0, waits = 0, future = 0, load_frames = 0;
    float end_of_first = -1, at_load = -1, mech_clock = -1, load_max = -1;
    engine.on_present = [&](int) {
      if (leg == 1 && app.current() == &mech3::states().load) {
        ++load_frames;
        load_max = std::max(load_max, mech3::mech_game_time());
      }
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (leg == 0 && frames == 300) {
        end_of_first = mech3::mech_game_time();
        leg = 1, frames = 0;
        mech3::missions().cleanup(false);
        mech3::GameEngine::Launch next;
        next.number = 4;
        engine.set_launch(next);
        app.replace(&mech3::states().load, 0);
      } else if (leg == 1 && frames == 1) {
        at_load = mech3::missions().time;
        mech_clock = mech3::mech_game_time();
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        for (int32_t q = 0; q < made; ++q) {
          auto* c = static_cast<mech3::AiControls*>(ai[q].controls);
          if (!c || !c->brain) continue;
          for (mech3::ai::Order& o : c->brain->orders)
            if (auto* w = dynamic_cast<mech3::ai::WaitState*>(o.state.get())) {
              ++waits;
              future += w->start > mech_clock;
            }
        }
        leg = 2;
      }
    };
    CountingPump pump;
    pump.stop = [&] { return leg == 2; };
    pump.limit = 2000;
    app.run(pump);
    if (std::getenv("MW3_CLOCK_LOG"))
      std::fprintf(stderr, "CLOCK first %g load %g mech %g waits %d future %d load frames %d max %g\n", end_of_first, at_load,
                   mech_clock, waits, future, load_frames, load_max);
    EXPECT(leg == 2 && end_of_first > 5.0f);
    EXPECT(load_frames > 0 && load_max >= 0.0f && load_max < 1.0f);
    EXPECT(at_load >= 0.0f && at_load < 1.0f && mech_clock == at_load);
    EXPECT(future == 0);
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

// c1 mission 1's Owens starts powered down (p12's second flag): player.cpp
// gives it one 0.1 s update with its controller held (0x0049fa20, vt+0x34),
// then the power-down (0x0041cc30), which settles it on the ground.
// docs/mission-start.md section 4.
TEST(mission_c1m1_powered_down_ai_mech_settles_once_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_settle_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    mech3::LaunchedMech first;
    bool seen = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || seen) return;
      int32_t made = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
      if (made > 0) seen = mech3::describe_mech(ai[0].mech, &first);
    };
    CountingPump pump;
    pump.stop = [&] { return seen; };
    pump.limit = 400;
    app.run(pump);
    if (std::getenv("MW3_SETTLE_LOG"))
      std::fprintf(stderr, "SETTLE down %d/%d feet %d in world %d pos %g %g %g yaw %g armor %g heat %g motions %d\n",
                   first.powering_down, first.powered_down, first.feet_down, first.in_world, first.pos[0], first.pos[1],
                   first.pos[2], first.yaw_sin, first.armor, first.heat, first.motions);
    // The update set it on the ground (without it the mech stays 1 cm up, as placed).
    EXPECT(seen && first.powering_down && first.feet_down);
    EXPECT(seen && std::fabs(first.pos[1] - ground_under(first.pos[0], first.pos[1] + 100.0f, first.pos[2])) < 1e-3f);
    EXPECT(engine.unported_counts().count("mech.vt+0x34") == 0);
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

// The camera's zone (0x0040b080 -> 0x004f6c80): c4b mission 6's tunnel is
// zone 44. At the start the ground under the camera is zone 1, so the camera,
// the render's list [0x0074a328] and the player's mech (ZEntity3d +0x64, its
// node's byte) read {1}: the tunnel's nodes (and zone 22's) are not drawn.
// Put in the tunnel, they read {1, 44}: the tunnel is. Before the port no
// zone was ever culled. docs/view.md section 7.
TEST(mission_c4b_m6_camera_zone_follows_the_ground_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_zone_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c4b mission 6
    l.index = 4, l.number = 6;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Snap {
      uint8_t cam[4] = {}, render[4] = {}, mech[4] = {};
      int32_t set = -1, node_zone = -1, v1 = -1, v22 = -1, v44 = -1, anim_known = -1, probe_hit = -1;
      uint8_t anim[4] = {}, probe[4] = {};
    };
    Snap outside, inside;
    bool nan_query_ended = false;
    int frames = 0;
    // A wall only in zone 44 (zWeapon's queries read the one [0x0074a328]):
    // found by the weapons' segment query under {1, 44}, missed under {1}; a
    // shot with the player's zones stops on it, one with {1} does not.
    struct Wall {
      zwep::Handle node = nullptr;
      zwep::Vec3f3 from{}, dir{};
      float dist = 0;
      bool missed_outside = false;
      const char* weapon = nullptr;
      int marks_inside = 0, marks_outside = 0;
      bool hit_inside = false, hit_outside = false;
      int shot = 0;  // 1 the inside shot is flying, 2 the outside one
    } wall;
    static Wall* s_wall;
    s_wall = &wall;
    auto nearest_off_player = [](const zwep::Vec3f3& a, const zwep::Vec3f3& b, float* d2) -> zwep::Handle {
      static zwep::HitList hl;
      hl = zwep::HitList{};
      zwep::run_hooks().segment_list(&a, &b, &hl);
      zwep::Handle best = nullptr;
      for (int i = 0; i < hl.count; ++i) {
        const zwep::HitRecord& r = hl.rec[i];
        if (!r.node || mech3::node_entity(r.node) == mech3::mech_mission_player()) continue;
        const float dx = r.point.x - a.x, dy = r.point.y - a.y, dz = r.point.z - a.z;
        const float e = dx * dx + dy * dy + dz * dz;
        if (!best || e < *d2) best = r.node, *d2 = e;
      }
      return best;
    };
    const auto marks = zwep::hit_hooks().surface_mark;
    auto snap = [&](Snap& n) {
      auto* v = mech3::program_view();
      auto* c = v ? static_cast<zcls::Node*>(v->camera) : nullptr;
      if (!c || !c->data) return;
      const uint8_t* raw = static_cast<zcls::Camera*>(c->data)->raw;
      n.set = *reinterpret_cast<const int32_t*>(raw + 0x1e0);
      std::memcpy(n.cam, raw + 0x1e4, 4);
      std::memcpy(n.render, zmod::zones().list, 4);
      mech3::mech_zones(mech3::mech_mission_player(), n.mech, &n.node_zone);
      n.v1 = zmod::zone_visible(1), n.v22 = zmod::zone_visible(22), n.v44 = zmod::zone_visible(44);
      n.anim_known = zeff::anim().block.get<int32_t>(0x28);  // 0x00502d70's
      std::memcpy(n.anim, zeff::anim().block.raw + 0x38, 4);
      // The render's own probe (0x004eda10 at the camera), used while the
      // camera has no zone set: the same list from the ground below it.
      const zcls::WorldRenderHooks& wh = zcls::world_render_hooks();
      float at[3];
      std::memcpy(at, raw + 0x2c, 12);
      uint32_t zone = 0;
      if (wh.zone_probe && mech3::world_hooks().world) {
        n.probe_hit = wh.zone_probe(wh.ctx, mech3::world_hooks().world, at, &zone);
        std::memcpy(n.probe, &zone, 4);
      }
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (frames == 5) {
        snap(outside);
        if (zcls::Node* t = zcls::list_find_by_name(6, "m6tun")) {
          float x = 0, y = 0, z = 0;
          zcls::object3d_get_translation(t, &x, &y, &z);
          const float at[3] = {x, ground_under(x, y + 20.0f, z), z};
          mech3::move_mech(mech3::mech_mission_player(), at, 0.0f);
        }
      } else if (frames == 15) {
        snap(inside);
        // A segment toward a NaN place ends (FIX (bug): the exe's walk does not).
        const float from[3] = {0, 50, 0}, nowhere[3] = {std::nanf(""), std::nanf(""), std::nanf("")};
        uint8_t count = 0;
        mech3::world_segment_first(from, nowhere, 0, &count);
        nan_query_ended = true;
        float at[3];
        mech3::node_world_cached(mech3::mech_centroid_of(mech3::mech_mission_player()), at);
        wall.from = {at[0], at[1], at[2]};
        const uint32_t tunnel_zone = 0xff2c0102u, one_zone = 0xffff0101u;  // {2, 1, 44}, {1, 1}
        for (int i = 0; i < 32 && !wall.node; ++i) {
          const float a = static_cast<float>(i) * 0.19634954f;
          const zwep::Vec3f3 d{std::sin(a), 0.0f, std::cos(a)};
          const zwep::Vec3f3 to{wall.from.x + 150 * d.x, wall.from.y, wall.from.z + 150 * d.z};
          float d2 = 0, d2_one = 0;
          zwep::set_query_zone(tunnel_zone);
          zwep::Handle n = nearest_off_player(wall.from, to, &d2);
          if (!n || static_cast<zcls::Node*>(n)->raw[0x30] != 44) continue;
          zwep::set_query_zone(one_zone);
          zwep::Handle m = nearest_off_player(wall.from, to, &d2_one);
          wall.node = n, wall.dir = d, wall.dist = std::sqrt(d2), wall.missed_outside = m != n;
        }
        std::memcpy(zmod::zones().list, inside.render, 4);
        for (zwep::WeaponDef& d : zwep::state().defs)
          if (d.key && !(d.flags & (zwep::kBeam | zwep::kTargetHeat | zwep::kMine | zwep::kLockOn)) && d.range > wall.dist + 5) {
            wall.weapon = d.key;
            break;
          }
        zwep::hit_hooks().surface_mark = [](zwep::HitRecord* h) {
          if (s_wall->shot == 1) ++s_wall->marks_inside, s_wall->hit_inside |= h->node == s_wall->node;
          if (s_wall->shot == 2) ++s_wall->marks_outside, s_wall->hit_outside |= h->node == s_wall->node;
        };
      } else if ((frames == 16 || frames == 46) && wall.node && wall.weapon) {
        // The inside shot: the camera's list and the player's zones ({2, 1, 44},
        // ZEntity3d +0x64). The outside one: both {1, 1}, as with the camera
        // over zone-1 ground (the spawn's straight-shot cast reads the list).
        static uint32_t zone;
        zone = frames == 16 ? 0xff2c0102u : 0xffff0101u;
        wall.shot = frames == 16 ? 1 : 2;
        const uint32_t keep = zwep::query_zone();
        zwep::set_query_zone(zone);
        const zwep::Vec3f3 still{0, 0, 0};
        zwep::spawn(zwep::find_def(wall.weapon), mech3::mech_node_of(mech3::mech_mission_player()), &zone, &wall.from,
                    &wall.dir, &still, 0, nullptr);
        zwep::set_query_zone(keep);
      }
    };
    CountingPump pump;
    pump.stop = [&] { return frames > 75; };
    pump.limit = 2000;
    app.run(pump);
    zwep::hit_hooks().surface_mark = marks;
    if (std::getenv("MW3_ZONE_LOG")) {
      std::fprintf(stderr, "WALL %s dist %.1f missed_outside %d weapon %s marks %d/%d hit %d/%d\n",
                   wall.node ? static_cast<zcls::Node*>(wall.node)->name() : "-", wall.dist, wall.missed_outside,
                   wall.weapon ? wall.weapon : "-", wall.marks_inside, wall.marks_outside, wall.hit_inside, wall.hit_outside);
    }
    if (std::getenv("MW3_ZONE_LOG"))
      for (const Snap* n : {&outside, &inside})
        std::fprintf(stderr, "ZONE set %d cam %d %d %d %d render %d %d %d %d mech %d %d %d %d node %d visible 1:%d 22:%d 44:%d\n", n->set,
                     n->cam[0], n->cam[1], n->cam[2], n->cam[3], n->render[0], n->render[1], n->render[2], n->render[3],
                     n->mech[0], n->mech[1], n->mech[2], n->mech[3], n->node_zone, n->v1, n->v22, n->v44);
    const uint8_t one[4] = {1, 1, 0xff, 0xff}, tunnel[4] = {2, 1, 44, 0xff};
    EXPECT(outside.set == 1 && !std::memcmp(outside.cam, one, 4) && !std::memcmp(outside.render, one, 4));
    EXPECT(!std::memcmp(outside.mech, one, 4) && outside.node_zone == 1);
    EXPECT(outside.v1 == 1 && outside.v22 == 0 && outside.v44 == 0);
    EXPECT(inside.set == 1 && !std::memcmp(inside.cam, tunnel, 4) && !std::memcmp(inside.render, tunnel, 4));
    EXPECT(!std::memcmp(inside.mech, tunnel, 4) && inside.node_zone == 1);
    EXPECT(inside.v1 == 1 && inside.v22 == 0 && inside.v44 == 1);
    EXPECT(nan_query_ended);
    EXPECT(outside.anim_known == 1 && !std::memcmp(outside.anim, one, 4) && !std::memcmp(inside.anim, tunnel, 4));
    EXPECT(outside.probe_hit == 1 && !std::memcmp(outside.probe, one, 4));
    EXPECT(inside.probe_hit == 1 && !std::memcmp(inside.probe, tunnel, 4));
    EXPECT(wall.node && wall.missed_outside && wall.weapon);
    EXPECT(wall.hit_inside && !wall.hit_outside);
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

// A vehicle's node binding (0x004d6c90 -> 0x0040eaa0 -> 0x0040ebe0; docs/
// vehicles.md, The node binding): every node of a c1 mission 1 truck carries
// bit 19 and the truck, so node_entity (0x0040e920) finds it from any of
// them and the HUD's reticle pick does too. A shot from the player stops on
// the truck's node and damages it (its handler); a shot from the truck's node
// leaves the truck as the player's last attacker (+0x664, through
// node_entity on the shooter's node) until the next player tick clears it
// (0x0041aba0, as the exe). Before the port none of it did: the
// nodes were id 0 with no owner. Turrets and field bases bind the same way.
TEST(mission_c1m1_truck_nodes_name_the_truck_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_bind_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FastClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  struct Seen {
    int trucks = 0, nodes = 0, bound = 0, turrets = 0, turrets_bound = 0, bases = 0, bases_bound = 0;
    bool entity = false, child_entity = false, pick = false, hit_truck = false, damaged = false, attacker = false;
    float health0 = 0, health1 = 0;
    std::string weapon;
    void* truck = nullptr;
    void* hit_node = nullptr;
    int shot = 0, set_at = -1, cleared_at = -1;
  };
  static Seen seen;
  seen = Seen{};
  const auto marks = zwep::hit_hooks().surface_mark;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    auto world_of = [](void* node, float out[3]) { mech3::node_world_cached(node, out); };
    auto fire = [&](void* owner_node, void* from_node, void* to_node) {
      float a[3], b[3];
      world_of(from_node, a), world_of(to_node, b);
      zwep::Vec3f3 from{a[0], a[1], a[2]}, d{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const float n = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
      d = {d.x / n, d.y / n, d.z / n};
      static uint32_t zone;
      zone = zwep::query_zone();
      const zwep::Vec3f3 still{0, 0, 0};
      zwep::spawn(zwep::find_def(seen.weapon.c_str()), owner_node, &zone, &from, &d, &still, 0, nullptr);
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (frames == 3) {
        for (void* v : mech3::program_vehicles()) {
          mech3::VehicleSummary vs;
          if (mech3::describe_vehicle(v, &vs) && vs.name && std::strstr(vs.name, "truck")) {
            ++seen.trucks;
            if (!seen.truck) seen.truck = v;
          }
        }
        if (!seen.truck) return;
        auto* root = static_cast<zcls::Node*>(mech3::program_vehicle_node(seen.truck));
        std::vector<zcls::Node*> stack{root};
        zcls::Node* deep = root;
        while (!stack.empty()) {
          zcls::Node* n = stack.back();
          stack.pop_back();
          ++seen.nodes;
          seen.bound += n->get<uint32_t>(0x28) == 0x80000u && n->owner == seen.truck;
          if (n != root) deep = n;
          for (int32_t i = 0; i < n->child_count(); ++i) stack.push_back(n->children[i]);
        }
        // The turrets (bit 17, 0x0040eb10) and the field bases (bit 19, as vehicles).
        for (mech3::tur::Turret* t : mech3::tur::turret_list()) {
          auto* n = static_cast<zcls::Node*>(engine.object_of(t->node));
          ++seen.turrets;
          seen.turrets_bound += n && n->get<uint32_t>(0x28) == 0x20000u && n->owner == t && mech3::node_entity(n) == t;
        }
        for (void* v : mech3::program_vehicles()) {
          mech3::VehicleSummary vs;
          if (!mech3::describe_vehicle(v, &vs) || !vs.field_base) continue;
          auto* n = static_cast<zcls::Node*>(mech3::program_vehicle_node(v));
          ++seen.bases;
          seen.bases_bound += n && n->get<uint32_t>(0x28) == 0x80000u && n->owner == v && mech3::node_entity(n) == v;
        }
        seen.entity = mech3::node_entity(root) == seen.truck;
        seen.child_entity = deep != root && mech3::node_entity(deep) == seen.truck;
        seen.pick = mech3::program_hud_node_object(deep) == seen.truck;
        // The player put 40 m from the truck, then each shoots the other.
        float at[3];
        world_of(root, at);
        const float me[3] = {at[0] + 40.0f, ground_under(at[0] + 40.0f, at[1] + 30.0f, at[2]), at[2]};
        mech3::move_mech(mech3::mech_mission_player(), me, 0.0f);
        for (zwep::WeaponDef& d : zwep::state().defs)
          if (d.key && !(d.flags & (zwep::kBeam | zwep::kTargetHeat | zwep::kMine | zwep::kLockOn)) && d.range > 60) {
            seen.weapon = d.key;
            break;
          }
        mech3::VehicleSummary vs;
        mech3::describe_vehicle(seen.truck, &vs);
        seen.health0 = vs.health;
        zwep::hit_hooks().surface_mark = [](zwep::HitRecord* h) {
          if (seen.shot == 1 && !seen.hit_node) seen.hit_node = h->node;
        };
      } else if (frames == 6 && seen.truck && !seen.weapon.empty()) {
        seen.shot = 1;
        void* me = mech3::mech_mission_player();
        fire(mech3::mech_node_of(me), mech3::mech_centroid_of(me), mech3::program_vehicle_centroid(seen.truck));
      } else if (frames == 12 && seen.truck && !seen.weapon.empty()) {
        seen.hit_truck = seen.hit_node && mech3::node_entity(seen.hit_node) == seen.truck;
        mech3::VehicleSummary vs;
        mech3::describe_vehicle(seen.truck, &vs);
        seen.health1 = vs.health;
        seen.damaged = vs.health < seen.health0;
        seen.shot = 2;
        void* me = mech3::mech_mission_player();
        fire(mech3::program_vehicle_node(seen.truck), mech3::program_vehicle_centroid(seen.truck), mech3::mech_centroid_of(me));
      }
      // +0x664 holds the truck from the hit (the weapon tick, after the player's)
      // to the next player tick's end, whose 0x0041aba0 clears it (damage.md).
      if (frames > 12 && seen.truck) {
        void* a = mech3::mech_last_attacker(mech3::mech_mission_player());
        if (a == seen.truck && seen.set_at < 0) seen.set_at = frames;
        if (seen.set_at >= 0 && seen.cleared_at < 0 && a == nullptr) seen.cleared_at = frames;
        seen.attacker = seen.set_at >= 0;
      }
    };
    CountingPump pump;
    pump.stop = [&] { return frames > 18; };
    pump.limit = 2000;
    app.run(pump);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zwep::hit_hooks().surface_mark = marks;
  if (std::getenv("MW3_BIND_LOG"))
    std::fprintf(stderr, "BIND turrets %d/%d bases %d/%d trucks %d nodes %d bound %d entity %d child %d pick %d weapon %s hit %d damaged %d (%.1f -> %.1f) attacker %d\n",
                 seen.turrets_bound, seen.turrets, seen.bases_bound, seen.bases, seen.trucks, seen.nodes, seen.bound, seen.entity, seen.child_entity, seen.pick, seen.weapon.c_str(),
                 seen.hit_truck, seen.damaged, seen.health0, seen.health1, seen.attacker);
  EXPECT(seen.trucks >= 2 && seen.nodes > 1 && seen.bound == seen.nodes);
  EXPECT(seen.entity && seen.child_entity && seen.pick);
  EXPECT(seen.hit_truck && seen.damaged);
  EXPECT(seen.attacker && seen.cleared_at == seen.set_at + 1);
  EXPECT(seen.turrets > 0 && seen.turrets_bound == seen.turrets);
  EXPECT(seen.bases > 0 && seen.bases_bound == seen.bases);
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The landmines (0x004135c0, 0x00413560, 0x00413470; docs/landmines.md):
// c1 mission 2 lists landmines.zrd's 82, LANDMINE at 20 m. The player put on
// one sets it off (the impact on the ground under it, the splash), and it
// stays listed, disarmed (off the radar's armed ones); an AI mech put on
// another far from the first sets that one off too (every mech's update
// checks them); the mission's end empties the list.
TEST(mission_c1m2_landmines_go_off_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_mines_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  FakeWindow window;
  FakeAlert alert;
  FastClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  struct Seen {
    size_t loaded = 0, listed_after = 0, after_cleanup = 99;
    std::string weapon;
    float radius = 0;
    int armed_start = -1, impacts = 0, splashes = 0, ai_count = 0;
    int player_mine = -1, ai_mine = -1;
    bool player_off = false, ai_off = false, ai_far_armed_before = false;
    int impacts_player = 0, impacts_ai = 0;
  };
  static Seen seen;
  seen = Seen{};
  mech3::LandmineHooks keep_hooks = mech3::landmine_hooks();
  mech3::landmine_hooks().impact = [](zwep::WeaponDef* d, zwep::HitRecord* r, zwep::Projectile* s, zwep::Handle a) {
    ++seen.impacts;
    return zwep::impact(d, r, s, a);
  };
  mech3::landmine_hooks().splash = [](zwep::WeaponDef* d, zwep::Projectile* s, zwep::HitList* l, uintptr_t e) {
    ++seen.splashes;
    return zwep::splash(d, s, l, e);
  };
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1 mission 2
    l.number = 2;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    auto armed = [] {
      int n = 0;
      for (const mech3::Landmine& m : mech3::landmines().list) n += m.armed != 0;
      return n;
    };
    auto put_on = [](void* mech, const mech3::Landmine& m) {
      const float at[3] = {m.at[0], ground_under(m.at[0], m.at[1] + 30.0f, m.at[2]), m.at[2]};
      mech3::move_mech(mech, at, 0.0f);
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      std::vector<mech3::Landmine>& list = mech3::landmines().list;
      if (frames == 3) {
        seen.loaded = list.size();
        if (const zwep::WeaponDef* d = mech3::landmines().weapon) seen.weapon = d->key ? d->key : "";
        seen.radius = mech3::landmines().radius;
        seen.armed_start = armed();
        float me[3];
        mech3::mech_world_position(mech3::mech_mission_player(), me);
        // The mine nearest the player's start, so the walk there is short.
        float best = 1e30f;
        for (size_t i = 0; i < list.size(); ++i) {
          const float dx = list[i].at[0] - me[0], dz = list[i].at[2] - me[2];
          if (dx * dx + dz * dz < best) best = dx * dx + dz * dz, seen.player_mine = static_cast<int>(i);
        }
        if (seen.player_mine >= 0) put_on(mech3::mech_mission_player(), list[static_cast<size_t>(seen.player_mine)]);
      } else if (frames == 8 && seen.player_mine >= 0) {
        seen.player_off = !list[static_cast<size_t>(seen.player_mine)].armed;
        seen.impacts_player = seen.impacts;
        // An armed mine at least 100 m from every disarmed one and the player.
        float me[3];
        mech3::mech_world_position(mech3::mech_mission_player(), me);
        for (size_t i = 0; i < list.size() && seen.ai_mine < 0; ++i) {
          if (!list[i].armed) continue;
          auto far = [&](const float* q) {
            const float dx = list[i].at[0] - q[0], dz = list[i].at[2] - q[2];
            return dx * dx + dz * dz > 10000.0f;
          };
          bool ok = far(me);
          for (const mech3::Landmine& o : list) ok = ok && (o.armed || far(o.at));
          if (ok) seen.ai_mine = static_cast<int>(i);
        }
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        seen.ai_count = made;
        if (made > 0 && seen.ai_mine >= 0) {
          seen.ai_far_armed_before = list[static_cast<size_t>(seen.ai_mine)].armed != 0;
          put_on(ai[0].mech, list[static_cast<size_t>(seen.ai_mine)]);
        }
      } else if (frames == 13 && seen.ai_mine >= 0) {
        seen.ai_off = !list[static_cast<size_t>(seen.ai_mine)].armed;
        seen.impacts_ai = seen.impacts - seen.impacts_player;
        seen.listed_after = list.size();
      }
    };
    CountingPump pump;
    pump.stop = [&] { return frames > 13; };
    pump.limit = 3000;
    app.run(pump);
    mech3::missions().cleanup(false);
    seen.after_cleanup = mech3::landmines().list.size();
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  mech3::landmine_hooks() = keep_hooks;
  if (std::getenv("MW3_MINES_LOG"))
    std::fprintf(stderr, "MINES %zu %s r %.1f armed %d player %d off %d ai %d mine %d off %d impacts %d/%d splashes %d after %zu/%zu\n",
                 seen.loaded, seen.weapon.c_str(), seen.radius, seen.armed_start, seen.player_mine,
                 seen.player_off, seen.ai_count, seen.ai_mine, seen.ai_off, seen.impacts_player, seen.impacts_ai,
                 seen.splashes, seen.listed_after, seen.after_cleanup);
  EXPECT(seen.loaded == 82 && seen.armed_start == 82);
  EXPECT(seen.weapon == "LANDMINE" && seen.radius == 20.0f);
  EXPECT(seen.player_off && seen.impacts_player >= 1);
  EXPECT(seen.ai_count > 0 && seen.ai_far_armed_before && seen.ai_off && seen.impacts_ai >= 1);
  EXPECT(seen.splashes >= 2);
  EXPECT(seen.listed_after == 82);  // disarmed, still listed
  EXPECT(seen.after_cleanup == 0);
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// c1 mission 1's four vehicles (docs/vehicles.md, In the program) run their
// frames (0x004a1040 from the entity walk) under their brains: each frame
// keeps them in the world, alive, on the ground; after truck_go (18 s past
// bramp) the trucks drive off along their path.
TEST(mission_c1m1_vehicles_run_their_frames_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_vehicles_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Track {
      std::string name;
      float first[3] = {}, at_go[3] = {}, last[3] = {};
      bool sane = true, dead = false;
      mech3::VehicleSummary end;
    };
    std::vector<Track> tracks;
    int frames = 0;
    bool went = false, killed = false, base_killed = false;
    mech3::LaunchedMech before, after;
    size_t left = 99;
    int32_t registered = -1, lines = -1;
    mech3::DockSummary dock;
    int32_t deploy = -1;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      if (frames == 0)
        if (mech3::ai::Valve* b = mech3::ai::valve_find("bramp")) b->set(1);
      const std::vector<void*> vs = mech3::program_vehicles();
      if (tracks.empty()) tracks.resize(vs.size());
      const mech3::ai::Valve* t = mech3::ai::valve_find("truck_go");
      const bool go = t && t->get() == 1 && !went;
      for (size_t i = 0; i < vs.size() && i < tracks.size(); ++i) {
        mech3::VehicleSummary s;
        if (!mech3::describe_vehicle(vs[i], &s)) continue;
        Track& k = tracks[i];
        if (frames == 0) k.name = s.name ? s.name : "", std::copy(s.pos, s.pos + 3, k.first);
        if (go) std::copy(s.pos, s.pos + 3, k.at_go);
        std::copy(s.pos, s.pos + 3, k.last);
        k.dead = !s.active;
        k.end = s;
        // mobilehq_01's arms opened through its frames (0x004597d0; the dock's call, 4b).
        if (frames == 30 && k.name == "mobilehq_01") mech3::program_field_base_aim_arms(vs[i], true);
        k.sane = k.sane && s.in_world && (s.active ? s.health > 0 : s.health == 0) && std::isfinite(s.pos[0]) &&
                 std::isfinite(s.pos[1]) && std::isfinite(s.pos[2]);
      }
      went = went || go;
      if (frames == 90)  // the player shoots mobilehq_03 down (0x00458f50, 0x004590b0)
        for (void* v : vs) {
          mech3::VehicleSummary s;
          if (mech3::describe_vehicle(v, &s) && s.name && std::string(s.name) == "mobilehq_03") {
            void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
            base_killed = mech3::program_vehicle_kill(v, player, "MGUN");
          }
        }
      if (frames == 60)  // the player kills apc_02 (the kill credit 0x0041b420)
        for (void* v : vs) {
          mech3::VehicleSummary s;
          if (mech3::describe_vehicle(v, &s) && s.name && std::string(s.name) == "apc_02") {
            void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
            mech3::describe_mech(player, &before);
            killed = mech3::program_vehicle_kill(v, player, "MGUN");
            mech3::describe_mech(player, &after);
          }
        }
      ++frames;
    };
    CountingPump pump;
    pump.limit = 1200;
    std::map<std::string, uint32_t> calls;
    pump.on_quit = [&] {
      for (const auto& [k, v] : engine.unported_counts()) calls[k] = v;
      registered = mech3::program_field_bases_count();
      lines = mech3::program_field_bases_lines();
      dock = mech3::program_dock_summary();
      if (const mech3::ai::Valve* v = mech3::ai::valve_find("mcbdeploy")) deploy = v->get();
      mech3::missions().cleanup(false);  // the valves end with the mission
      left = mech3::program_vehicles().size();  // freed with the entities (0x0049dc40)
    };
    app.run(pump);
    if (std::getenv("MW3_SURVEY"))
      for (const auto& [k, v] : calls) std::fprintf(stderr, "  %6u %s\n", v, k.c_str());
    EXPECT(frames > 200);
    EXPECT(went);
    EXPECT(tracks.size() == 7);  // the three field bases first, made by 0x004589d0
    EXPECT(registered == 3);
    // The dock's eleven radio lines and "mfbeffects" (0x00459e80): all but
    // mcb_deploy, which no shipped sound set has.
    EXPECT(lines == 11);
    // The dock (0x00459e80, then its frame 0x0045a270): the idle state
    // alone, begun on the first frame, no one asking; the bases held;
    // "mcbdeploy" made.
    EXPECT(dock.states == 1 && dock.idle_first && !dock.begin_next && !dock.drive && dock.deploy_valve);
    EXPECT(deploy == 0);
    auto dist = [](const float a[3], const float b[3]) {
      return std::hypot(a[0] - b[0], a[2] - b[2]);
    };
    for (const Track& k : tracks) {
      std::fprintf(stderr, "  %s: %.1f %.1f %.1f -> go %.1f %.1f %.1f -> %.1f %.1f %.1f\n", k.name.c_str(),
                   k.first[0], k.first[1], k.first[2], k.at_go[0], k.at_go[1], k.at_go[2], k.last[0], k.last[1],
                   k.last[2]);
      EXPECT(k.sane);
      // The trucks die (the destroy valves at their last nodes, or the Owens'
      // fire on the way); the APCs drive off along theirs, alive.
      const bool truck = k.name.rfind("ttruck", 0) == 0;
      EXPECT(k.dead == (truck || k.name == "apc_02" || k.name == "mobilehq_03"));
      if (k.name == "apc_01") EXPECT(dist(k.first, k.last) > 100.0f);
      // The field bases: in the registry, their arms' seven parts found;
      // they keep their place (the throttle held at 0 until the dock lets
      // them drive, [0x0060b4d0]); mobilehq_01's arms opened.
      if (k.name.rfind("mobilehq_0", 0) == 0) {
        EXPECT(k.end.field_base && k.end.registry == k.name.back() - '1');
        EXPECT(k.end.arm_parts == 7);
        EXPECT(dist(k.first, k.last) < 0.5f);
        EXPECT(k.end.arms == (k.name == "mobilehq_01" ? 1.0f : 0.0f));
      }
    }
    // The field base's loss: no campaign behind the debug launch, so no cut
    // (0x004590b0 after the basic death); still bases left, so no 0x0047a720.
    EXPECT(base_killed);
    EXPECT(calls.count("0x004590b0 cut") == 0 && calls.count("0x0047a720") == 0);
    // The player's kill: one vehicle, 75 points.
    EXPECT(killed);
    EXPECT(after.kills[1] == before.kills[1] + 1 && after.kills[4] == before.kills[4] + 75);
    EXPECT(left == 0);
    EXPECT(calls.count("entity vt+0x0") == 0);
    // ttruck_01 waits for truck_go at its net's node_08.
    for (const Track& k : tracks)
      if (k.name == "ttruck_01") {
        const float node08[3] = {-1826.3f, -20.0f, -1571.9f};
        EXPECT(dist(k.at_go, node08) < 3.0f);
    }
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// c1 mission 1's field bases repair the player (docs/vehicles.md, The repair
// dock, The mcbwaynet): net_02's arrangenodes node "MFB Baker" is the
// mcbwaynet's one dock point; the player's MFB command picks it (the
// mission's waynet the mcbwaynet meanwhile) and asks (0x0047a7a0); the
// bases go to their places; the player walks onto the dock point and powers
// down; the bases back out and open; after twelve seconds the repair
// (0x004154e0) and the bases close up; the dock is idle again, the mission's
// waynet back to its nav waynet (waynet_1).
TEST(mission_c1m1_field_bases_repair_the_player_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_dock_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    // A pilot whose game's store holds every ammo and armour (the debug
    // launch has none, and the repair then takes nothing from a store).
    mech3::Pilot pilot("Repair");
    mech3::Game game(&pilot);
    pilot.current = &game;
    for (int32_t w = 0; w < 0x25; ++w) game.store.add_ammo(w, 1)->count = 1000;
    for (int32_t kind = 1; kind <= 2; ++kind)
      for (int32_t tech = 0; tech <= 2; ++tech) game.store.add_equipment(kind, tech, 1)->count = 40;
    auto held = [&](int kind) {
      int32_t n = 0;
      for (mech3::ConfigItem* it : game.store.items)
        if (it->kind() == kind && (kind == 2 || it->id() == 1 || it->id() == 2)) n += it->count;
      return n;
    };
    const int32_t ammo_held = held(2), armour_held = held(3);
    mech3::Pilot* was_pilot = mech3::current_pilot();
    mech3::current_pilot() = &pilot;
    mech3::MechWear worn, repaired;
    int frames = 0, asked_at = -1, contact_at = -1, deploy_at = -1, repair_at = -1, close_at = -1, idle_at = -1;
    int repairs_at_close = -1;
    float repair_t = 0, close_t = 0;
    std::vector<int> fronts;
    float point[3] = {};
    std::string label;
    int32_t points = -1;
    bool picking = false, cleared = false;
    void* nav = nullptr;  // the mission's own waynet before the command (waynet_1)
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
      const mech3::DockSummary d = mech3::program_dock_summary();
      if (fronts.empty() || fronts.back() != d.front) fronts.push_back(d.front);
      if (frames == 5) {  // MCB_COMMAND: the nearest dock point picked
        nav = mech3::program_mission_waynet();
        mech3::program_mech_wear_down(player, 0.25f, 0.5f);
        worn = mech3::program_mech_wear(player);
        points = mech3::program_mcb_points();
        mech3::program_mcb_command(player);
        picking = mech3::program_mission_waynet() == mech3::program_mcb_waynet();
      }
      if (frames == 6) {  // again: the request
        mech3::program_mcb_command(player);
        asked_at = frames;
        const char* l = nullptr;
        if (mech3::program_dock_point(&l, point)) label = l ? l : "";
      }
      if (d.front == 2 && contact_at < 0) {  // the contact: onto the dock point, powered down
        contact_at = frames;
        const float at[3] = {point[0], ground_under(point[0], point[1] + 100.0f, point[2]), point[2]};
        mech3::move_mech(player, at, 0.0f);
        mech3::mech_power(player, 1);
      }
      if (d.front == 3 && deploy_at < 0) deploy_at = frames;
      if (d.front == 4 && repair_at < 0) repair_at = frames, repair_t = mech3::mech_game_time();
      if (d.front == 5 && close_at < 0) {
        close_at = frames;
        close_t = mech3::mech_game_time();
        const auto& n = engine.unported_counts();
        repairs_at_close = n.count("0x004154e0") ? static_cast<int>(n.at("0x004154e0")) : 0;
        repaired = mech3::program_mech_wear(player);
      }
      if (d.front == 0 && close_at >= 0 && idle_at < 0) {
        idle_at = frames;
        cleared = mech3::program_mission_waynet() == nav;
      }
      ++frames;
    };
    CountingPump pump;
    pump.limit = 6000;
    pump.stop = [&] { return idle_at >= 0; };
    app.run(pump);
    std::fprintf(stderr, "  dock: asked %d contact %d deploy %d repair %d close %d idle %d; fronts", asked_at,
                 contact_at, deploy_at, repair_at, close_at, idle_at);
    for (int f : fronts) std::fprintf(stderr, " %d", f);
    std::fprintf(stderr, "\n");
    EXPECT(points == 1 && picking);  // the one dock point, "MFB Baker"
    EXPECT(label == "MFB Baker");
    std::fprintf(stderr, "  dock point %s at %.1f %.1f %.1f\n", label.c_str(), point[0], point[1], point[2]);
    EXPECT(asked_at == 6);
    EXPECT(cleared);
    EXPECT(contact_at > asked_at && deploy_at > contact_at && repair_at > deploy_at && close_at > repair_at);
    // The repair: twelve seconds of game time, then Mech::repair (0x004154e0,
    // ported): armour, internals, ammo from the store, coolant back.
    std::fprintf(stderr, "  repair %.2f s\n", close_t - repair_t);
    EXPECT(close_t - repair_t >= 12.0f && close_t - repair_t < 12.2f);
    EXPECT(repairs_at_close == 0);
    std::fprintf(stderr, "  worn armour %.2f internals %.2f ammo %d/%d coolant %.2f fuel %.2f\n", worn.armor,
                 worn.internal, worn.ammo, worn.ammo_max, worn.coolant, worn.jet_fuel);
    std::fprintf(stderr, "  repaired armour %.2f internals %.2f ammo %d/%d coolant %.2f fuel %.2f\n", repaired.armor,
                 repaired.internal, repaired.ammo, repaired.ammo_max, repaired.coolant, repaired.jet_fuel);
    std::fprintf(stderr, "  store: ammo %d -> %d, armour %d -> %d\n", ammo_held, held(2), armour_held, held(3));
    EXPECT(worn.armor == 0.25f && worn.internal == 0.5f && worn.ammo == 0 && worn.ammo_max > 0 && worn.coolant == 0.0f);
    EXPECT(repaired.armor == 1.0f && repaired.internal == 1.0f && repaired.coolant == 1.0f);
    EXPECT(repaired.ammo == repaired.ammo_max && repaired.ammo_max == worn.ammo_max);
    EXPECT(ammo_held - held(2) == repaired.ammo);  // every round from the store
    EXPECT(armour_held > held(3));                 // the armour paid for
    mech3::current_pilot() = was_pilot;
    pilot.current = nullptr;
    EXPECT(idle_at > close_at);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// c1m1's nav waynet (vehicles.md, The nav waynet): waynet_1.zrd built at the
// mission load (0x004797c0, version 107) as the mission's waynet: Start,
// Able and Baker (Able's and Baker's nav lines found), the radar's marker on
// the selected one. The player starts on Start, so autonext has already moved
// on to Able; set on the selected point it is within 25 m: reached (its line
// said) and, autonext, the next one selected.
// The turrets on c1m1 (docs/vehicles.md, The turrets in the program): made
// from aiTurrets.zrd at the load, on zEntity's list; with the player walked
// up to one it wakes, picks the player and fires; killed outright (the
// valves' destroy, vt+0x70) it is shut down, its destroy animation run and
// the kill the player's (+0x3fc, a "building", 200 points).
TEST(mission_c1m1_turrets_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_turret_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    size_t made = 0, listed = 0;
    mech3::tur::Turret* t = nullptr;
    std::string name;
    bool picked = false, targets_player = false, fired = false, sensed = false;
    int32_t kills_before = -1, kills_after = -1, points = 0;
    bool shut = false;
    float next0 = 0;
    auto player_stats = [](int i) {
      for (const mech3::DebriefMech& d : mech3::mech_debrief_lance())
        if (d.player) return d.stats[i];
      return -1;
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      const uint32_t ph = engine.field32(engine.pilot(), 0x14);
      void* player = engine.object_of(ph);
      if (frames == 2) {
        made = mech3::tur::turret_list().size();
        for (mech3::tur::Turret* x : mech3::tur::turret_list()) {
          const auto& el = mech3::ai::entity_list();
          listed += std::find(el.begin(), el.end(), x->self) != el.end();
          if (!t && x->active && !x->idle && x->weapon) t = x;
        }
        if (t) {
          name = static_cast<zcls::Node*>(engine.object_of(t->node))->name();
          next0 = t->next_fire;
          const float at[3] = {t->pos[0] + 60.0f, ground_under(t->pos[0] + 60.0f, t->pos[1] + 100.0f, t->pos[2]),
                               t->pos[2]};
          mech3::move_mech(player, at, 0.0f);
        }
      }
      if (t && frames > 2 && frames < 100) {
        sensed |= t->sensed != 0;
        picked |= t->picked != 0;
        targets_player |= t->target && t->target->seen == ph;
        fired |= t->next_fire != next0;
      }
      if (t && frames == 100) {
        kills_before = player_stats(2);
        t->kill();  // vt+0x70
        shut = !t->active;
        kills_after = player_stats(2);
        points = player_stats(5);
      }
      ++frames;
    };
    CountingPump pump;
    pump.limit = 3000;
    pump.stop = [&] { return frames > 101; };
    app.run(pump);
    std::fprintf(stderr, "  turrets: %zu made, %zu listed; %s: sensed %d, picked %d, the player targeted %d, fired %d; "
                         "kills %d -> %d (%d points)\n",
                 made, listed, name.c_str(), sensed, picked, targets_player, fired, kills_before, kills_after, points);
    EXPECT(made >= 8 && listed == made);
    EXPECT(t != nullptr && name.rfind("tur_", 0) == 0);
    EXPECT(sensed && picked && targets_player && fired);
    EXPECT(shut && kills_after == kills_before + 1 && points >= 200);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The animation-owner registry on c1 (docs/ai.md section 18): the boss
// turret tur_boss01 starts idle and claims its AUX_ANIM raise_bturret1 at the
// load; started (as the mission script does), the animation's callback event
// sends 0x32 to its owner, which wakes it (IS_IDLE off, 0x004a74f0). At the
// mission's end every record is emptied (0x004d4700), with the world freed
// (0x004d4740).
TEST(mission_c1_turret_woken_by_its_animation_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_anim_owner_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0, woke_at = -1;
    mech3::tur::Turret* t = nullptr;
    size_t records = 0;
    bool idle_before = false, claimed = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      if (frames == 2) {
        records = mech3::ai::anim_owner_records().size();
        for (mech3::tur::Turret* x : mech3::tur::turret_list())
          if (std::strcmp(static_cast<zcls::Node*>(engine.object_of(x->node))->name(), "tur_boss01") == 0) t = x;
        if (t && !t->aux.empty()) {
          auto* raise = static_cast<zeff::AnimDef*>(engine.object_of(t->aux.front()));
          const auto* rec = static_cast<const mech3::ai::AnimOwnerRec*>(raise->callback_arg);
          const auto& list = mech3::ai::anim_owner_records();
          claimed = raise->callback && rec && rec->owner == t->self &&
                    std::find(list.begin(), list.end(), rec) != list.end();
          idle_before = t->idle != 0;
          zeff::anim_start(raise, nullptr, 0, 0, 0);  // as the mission script does
        }
      }
      if (t && frames > 2 && woke_at < 0 && !t->idle) woke_at = frames;
      ++frames;
    };
    CountingPump pump;
    pump.limit = 3000;
    pump.stop = [&] { return woke_at >= 0 || frames > 600; };
    app.run(pump);
    mech3::missions().cleanup(false);  // the mission's end, the world kept (0x00491780)
    size_t kept = 0, emptied = 0;
    for (const mech3::ai::AnimOwnerRec* r : mech3::ai::anim_owner_records()) ++kept, emptied += !r->owner && !r->valve;
    mech3::missions().unload();  // the world (0x00490480)
    const size_t left = mech3::ai::anim_owner_records().size();
    std::fprintf(stderr, "  anim owners: %zu records at the load; tur_boss01 claimed %d, idle %d, woken at frame %d; "
                         "the end keeps %zu (%zu emptied), the world's unload leaves %zu\n",
                 records, claimed, idle_before, woke_at, kept, emptied, left);
    EXPECT(records > 0 && t != nullptr && claimed && idle_before);
    EXPECT(woke_at > 2);
    EXPECT(kept == records && emptied == kept && left == 0);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The animations' binder on c1m1 (docs/ai.md section 18): at the load
// (0x004d4b30) citybuild01's animation gets a record naming the valve
// "citybuild01"; rammed down, its good_to_bad sequence sends 0x2b, which
// turns that valve on.
TEST(mission_c1m1_a_destroy_animation_turns_its_valve_on_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_anim_binder_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0, on_at = -1;
    zeff::AnimDef* d = nullptr;
    mech3::ai::Valve* valve = nullptr;
    size_t bound = 0;
    bool names_valve = false;
    int32_t before = -1;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      if (frames == 2) {
        for (const mech3::ai::AnimOwnerRec* r : mech3::ai::anim_owner_records()) bound += r->valve != 0;
        d = zeff::anim_find("citybuild01");
        valve = mech3::ai::valve_get("citybuild01");
        const auto* rec = d ? static_cast<const mech3::ai::AnimOwnerRec*>(d->callback_arg) : nullptr;
        names_valve = rec && valve && engine.object_of(rec->valve) == valve;
        before = valve ? valve->get() : -1;
        if (d) zeff::anim_on_collide(d, 1.0e6f);  // rammed down
      }
      if (valve && frames > 2 && on_at < 0 && valve->get() == 1) on_at = frames;
      ++frames;
    };
    CountingPump pump;
    pump.limit = 3000;
    pump.stop = [&] { return on_at >= 0 || frames > 900; };
    app.run(pump);
    std::fprintf(stderr, "  anim binder: %zu records with a valve; citybuild01 names its valve %d, valve %d before, "
                         "on at frame %d\n", bound, names_valve, before, on_at);
    EXPECT(bound > 0 && d != nullptr && names_valve && before == 0);
    EXPECT(on_at > 2);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The radar's contacts beyond mechs (docs/ai.md section 15, Radar contacts):
// the player is walked up to a turret, a targets.zrd object (one with
// `radar`) and a vehicle in turn, 60 frames each; at each stop the most blips
// a frame of each class's image (the turrets' and targets' markers made at
// the load, the vehicles' by the stealth step's 0x004ac3d0), and the player's
// contacts on a target, in sight or not.
struct RadarStops {
  bool found[3] = {};
  int blips[3][3] = {};  // per stop (turret, target, vehicle): turret, target, vehicle blips
  bool target_radar = false;  // the target visited has `radar`
  int target_contacts = 0, targets_in_sight = 0;
};
RadarStops radar_stops(int32_t index, int32_t number) {
  RadarStops out;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_radar_contacts_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;
    l.index = index, l.number = number;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    float at[3][3] = {};
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
      mech3::HudReport r;
      if (frames == 2) {
        mech3::program_hud_report(&r);  // the counts from here
        for (mech3::tur::Turret* t : mech3::tur::turret_list())
          if (!out.found[0] && t->active) out.found[0] = true, std::copy(t->pos, t->pos + 3, at[0]);
        for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())  // one with `radar` first
          if (t->active && (!out.found[1] || (!out.target_radar && (t->desc.flags & 2))))
            out.found[1] = true, out.target_radar = (t->desc.flags & 2) != 0, mech3::hud_object_place(t, at[1]);
        for (void* v : mech3::program_vehicles())
          if (!out.found[2]) out.found[2] = true, mech3::hud_object_place(v, at[2]);
      }
      const int stop = frames < 3 ? -1 : (frames - 3) / 60;
      if (stop >= 0 && stop < 3 && (frames - 3) % 60 == 0 && out.found[stop]) {
        const float x = at[stop][0] - 40.0f, z = at[stop][2];
        const float pos[3] = {x, ground_under(x, at[stop][1] + 100.0f, z), z};
        mech3::move_mech(player, pos, -1.5707964f);
      }
      if (stop >= 0 && stop < 3 && mech3::program_hud_report(&r)) {
        int* b = out.blips[stop];
        b[0] = std::max(b[0], r.turret_blips), b[1] = std::max(b[1], r.target_blips), b[2] = std::max(b[2], r.vehicle_blips);
      }
      if (frames == 3 + 60 + 30) {
        mech3::SensedContact sc[64];
        const int n = std::min(mech3::sensed_contacts(player, sc, 64), 64);
        for (int i = 0; i < n; ++i)
          for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())
            if (t->self == sc[i].seen) ++out.target_contacts, out.targets_in_sight += sc[i].in_sight;
      }
      ++frames;
    };
    CountingPump pump;
    pump.limit = 3000;
    pump.stop = [&] { return frames > 183; };
    app.run(pump);
    std::fprintf(stderr, "  radar (index %d, m%d): by a turret %d turret / %d target / %d vehicle blips; by a target "
                         "%d / %d / %d; by a vehicle %d / %d / %d (found %d %d %d); target contacts %d, %d in sight\n",
                 index, number, out.blips[0][0], out.blips[0][1], out.blips[0][2], out.blips[1][0], out.blips[1][1],
                 out.blips[1][2], out.blips[2][0], out.blips[2][1], out.blips[2][2], out.found[0], out.found[1],
                 out.found[2], out.target_contacts, out.targets_in_sight);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
  return out;
}

// c1m1: the turrets and the trucks on the radar, each with its class's image.
// Its one target (the satdish) has no `radar` in targets.zrd: sensed from
// 40 m, never in sight, so never on the radar (as in the exe).
TEST(mission_c1m1_radar_shows_turrets_and_vehicles_headless) {
  if (!have_game()) return;
  const RadarStops s = radar_stops(0, 1);
  EXPECT(s.found[0] && s.found[2]);
  EXPECT(s.blips[0][0] >= 1 && s.blips[2][2] >= 1);
  EXPECT(s.found[1] && !s.target_radar && s.target_contacts == 1 && s.targets_in_sight == 0 && s.blips[1][1] == 0);
}

// t1m2 (training): the four targets with `radar`, in sight and on the radar
// with the targets' image.
TEST(mission_t1m2_radar_shows_its_targets_headless) {
  if (!have_game()) return;
  const RadarStops s = radar_stops(5, 2);
  EXPECT(s.found[1] && s.target_radar && s.target_contacts >= 1 && s.targets_in_sight == s.target_contacts);
  EXPECT(s.blips[1][1] >= 1);
}

// The texture scroll in play (docs/render.md section 3, Texture scroll): the
// world's horizon band `horscroll` is on the ScrollAlways list (each .gw's
// Object3DSetScrollAlways on 0.015 0.0), so its textured UVs move by
// 0.015 u a second from frame to frame (0x00541ad0), the v kept.
TEST(mission_c1m1_horizon_scrolls_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_scroll_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    zmod::Model* m = nullptr;
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0, dt_sum = 0;
    uint32_t flags = 0;
    float speed_u = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      if (frames == 2) {
        if (zcls::Node* n = zcls::list_find_by_name(6, "horscroll")) m = n->model;
        for (int32_t k = 0; m && k < static_cast<int32_t>(m->count(0x10)); ++k)
          if (m->polygons[k].material->flags & 1) {
            u0 = m->polygons[k].uvs[0], v0 = m->polygons[k].uvs[1];
            break;
          }
        if (m) std::memcpy(&flags, m->header + 8, 4), std::memcpy(&speed_u, m->header + 0x28, 4);
      }
      if (frames > 2 && frames <= 32) dt_sum += ztimer::state().dt;
      if (frames == 32 && m)
        for (int32_t k = 0; k < static_cast<int32_t>(m->count(0x10)); ++k)
          if (m->polygons[k].material->flags & 1) {
            u1 = m->polygons[k].uvs[0], v1 = m->polygons[k].uvs[1];
            break;
          }
      ++frames;
    };
    CountingPump pump;
    pump.limit = 3000;
    pump.stop = [&] { return frames > 33; };
    app.run(pump);
    const float du = u1 - u0;
    std::fprintf(stderr, "  scroll: horscroll %s, flag 0x10 %d, u speed %g; over 30 frames (%.3f s) u %g -> %g (%+g, %g wanted), "
                         "v %g -> %g\n", m ? "found" : "missing", (flags & 0x10) != 0, speed_u, dt_sum, u0, u1, du,
                 0.015f * dt_sum, v0, v1);
    EXPECT(m != nullptr && (flags & 0x10) && speed_u == 0.015f);
    EXPECT(std::fabs(du - 0.015f * dt_sum) < 1e-4f || std::fabs(std::fabs(du - 0.015f * dt_sum) - 1.0f) < 1e-4f);  // or wrapped
    EXPECT(v1 == v0 && du != 0.0f);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The fog colours to the device on c1m1 (docs/render-lighting.md): with a
// GPU that records FOGCOLOR, the world's fog colour sent at the load
// (0x0057bcf0, installed as 0x00579f50 does); a damaged vehicle tinted as
// TARGET's view does (0x004ad390: red by the damage) sends red as its fog
// colour while drawn, then the world's again. The lighting's mix (0x005680c0,
// 0x00579980) needs lit models: item 4c's (the GfxFlags lighting bit).
TEST(mission_c1m1_fog_mix_reaches_the_device_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_fog_mix_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  struct FogGpu : platform::Render3D {
    std::vector<std::array<uint8_t, 3>> fog;  // this frame's FOGCOLOR sends
    int draws = 0;
    int32_t create_texture(int, int, const uint8_t*, uint8_t) override { return 1; }
    void release_texture(int32_t) override {}
    void update_texture(int32_t, int, int, const uint8_t*) override {}
    void set_viewport(int, int, int, int) override {}
    void clear(bool, uint8_t, uint8_t, uint8_t) override {}
    void begin_scene() override {}
    void end_scene() override {}
    void draw(platform::Primitive, const platform::TLVertex*, int, const platform::DrawState&) override { ++draws; }
    void set_fog_color(uint8_t r, uint8_t g, uint8_t b) override { fog.push_back({r, g, b}); }
    void present() override {}
    bool read_back(std::vector<uint32_t>*, int*, int*) override { return false; }
  } gpu;
  FakeWindow window;
  FakeAlert alert;
  FastClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.render = &gpu;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    void* v = nullptr;
    float left = 1.0f;
    std::set<std::array<uint8_t, 3>> seen[3];  // per phase: plain, the vehicle tinted, after
    std::vector<std::array<uint8_t, 3>> loading;  // the sends before the first phase (the world's fog colour)
    int draws[3] = {};
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
      const int phase = frames < 3 ? -1 : frames < 10 ? 0 : frames < 18 ? 1 : frames < 27 ? 2 : -1;
      if (phase >= 0 && frames != 3 && frames != 10 && frames != 18) {
        for (const auto& c : gpu.fog) seen[phase].insert(c);
        draws[phase] += gpu.draws;
      }
      if (phase < 0 && frames < 3) loading.insert(loading.end(), gpu.fog.begin(), gpu.fog.end());
      gpu.fog.clear(), gpu.draws = 0;
      if (frames == 2) {
        const std::vector<void*> vs = mech3::program_vehicles();
        if (!vs.empty()) v = vs.front();
        if (v) {
          float at[3];
          mech3::hud_object_place(v, at);
          const float pos[3] = {at[0] - 30.0f, ground_under(at[0] - 30.0f, at[1] + 100.0f, at[2]), at[2]};
          mech3::move_mech(player, pos, -1.5707964f);
        }
      }
      if (frames == 9 && v) {
        left = mech3::program_vehicle_damage(v, 60.0f);
        mech3::program_vehicle_view_tint(v, true);  // as TARGET's view does around its draw (vt+0x68)
      }
      if (frames == 17 && v) mech3::program_vehicle_view_tint(v, false);
      ++frames;
    };
    CountingPump pump;
    pump.limit = 3000;
    pump.stop = [&] { return frames > 27; };
    app.run(pump);
    auto show = [](const std::set<std::array<uint8_t, 3>>& s) {
      std::string o;
      for (const auto& c : s) o += " " + std::to_string(c[0]) + "/" + std::to_string(c[1]) + "/" + std::to_string(c[2]);
      return o.empty() ? std::string(" none") : o;
    };
    std::set<std::array<uint8_t, 3>> load_set(loading.begin(), loading.end());
    std::fprintf(stderr, "  fog colour: at the load%s; vehicle %s at %.2f health; plain (%d draws):%s; tinted (%d):%s; "
                         "after (%d):%s\n", show(load_set).c_str(), v ? "found" : "missing", left, draws[0],
                 show(seen[0]).c_str(), draws[1], show(seen[1]).c_str(), draws[2], show(seen[2]).c_str());
    const std::array<uint8_t, 3> red{255, 0, 0};
    EXPECT(!loading.empty());                       // the world's fog colour reaches the device (0x0057bcf0)
    EXPECT(v != nullptr && left < 1.0f && seen[1].count(red));  // the damaged vehicle's red while tinted
    EXPECT(seen[1].count(loading.back()));          // and the world's colour back after it
    EXPECT(!seen[2].count(red));                    // none once the tint is off
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The models lit on c1m1 (docs/render-lighting.md, the GfxFlags): the
// lighting reads the GfxFlags_HW entry's Lighting bit (0x00526750 from the
// app's model load), so with a GPU that records the draws the vertices are
// lit (the sun's Lambert darkens them: a lower mean, more distinct colours);
// the option's bit cleared, live as the options menu's LIGHTING does, they
// are not; and the explosions' point light (light_flash, started at a
// vehicle) with the sun makes two colour sources, so the lighting mixes them
// into the fog colour (0x005680c0).
TEST(mission_c1m1_models_are_lit_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_lit_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::options() = mech3::Options{};
  struct DrawGpu : platform::Render3D {
    double sum = 0;
    long verts = 0;
    std::set<uint32_t> colours;
    int32_t create_texture(int, int, const uint8_t*, uint8_t) override { return 1; }
    void release_texture(int32_t) override {}
    void update_texture(int32_t, int, int, const uint8_t*) override {}
    void set_viewport(int, int, int, int) override {}
    void clear(bool, uint8_t, uint8_t, uint8_t) override {}
    void begin_scene() override {}
    void end_scene() override {}
    void draw(platform::Primitive, const platform::TLVertex* v, int n, const platform::DrawState&) override {
      for (int i = 0; i < n; ++i) {
        const uint32_t c = v[i].diffuse & 0xffffff;
        sum += ((c >> 16) & 0xff) + ((c >> 8) & 0xff) + (c & 0xff);
        ++verts;
        if (colours.size() < 100000) colours.insert(c);
      }
    }
    void set_fog_color(uint8_t, uint8_t, uint8_t) override {}
    void present() override {}
    bool read_back(std::vector<uint32_t>*, int*, int*) override { return false; }
  } gpu;
  FakeWindow window;
  FakeAlert alert;
  FastClock clock;
  FakeScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.screen = &screen, p.sound = &sound, p.files = &files;
  p.render = &gpu;
  static int mixes = 0;
  static void (*mix)(float*) = nullptr;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    void* v = nullptr;
    double mean[3] = {};
    size_t distinct[3] = {};
    int mixed[3] = {};
    uint32_t lighting_bit[2] = {};
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
      const int phase = frames < 3 ? -1 : frames < 10 ? 0 : frames < 18 ? 1 : frames < 27 ? 2 : -1;
      const bool counted = phase >= 0 && frames != 3 && frames != 10 && frames != 18;
      if (counted) mean[phase] += gpu.verts ? gpu.sum / gpu.verts : 0, distinct[phase] += gpu.colours.size(), mixed[phase] += mixes;
      gpu.sum = 0, gpu.verts = 0, gpu.colours.clear(), mixes = 0;
      if (frames == 2) {
        mix = zmod::light_hooks().mix_fog;  // counted, then the port's
        zmod::light_hooks().mix_fog = [](float* c) { ++mixes, mix(c); };
        const std::vector<void*> vs = mech3::program_vehicles();
        if (!vs.empty()) v = vs.front();
        if (v) {
          float at[3];
          mech3::hud_object_place(v, at);
          const float pos[3] = {at[0] - 30.0f, ground_under(at[0] - 30.0f, at[1] + 100.0f, at[2]), at[2]};
          mech3::move_mech(player, pos, -1.5707964f);
        }
        lighting_bit[0] = *zmod::lighting().mask & 1;
      }
      if (frames == 9) {  // LIGHTING off, live: the entry the lighting points at
        for (int32_t& f : mech3::options().gfx_flags) f &= ~1;
        lighting_bit[1] = *zmod::lighting().mask & 1;
      }
      if (frames == 17) {
        for (int32_t& f : mech3::options().gfx_flags) f |= 1;
        if (zeff::AnimDef* d = zeff::anim_find("light_flash"); d && v) {  // at a place, as CALL_ANIMATION AT_NODE does (0x00506640)
          float at[9] = {};
          mech3::hud_object_place(v, at);
          zeff::anim_start_at(d, nullptr, at, at + 3, at + 6);
        }
      }
      ++frames;
    };
    CountingPump pump;
    pump.limit = 3000;
    pump.stop = [&] { return frames > 27; };
    app.run(pump);
    for (int i = 0; i < 3; ++i) mean[i] /= i == 2 ? 8 : 6, distinct[i] /= i == 2 ? 8 : 6;
    std::fprintf(stderr, "  lit: Lighting bit %u, then %u; lit mean %.1f (%zu colours a frame), unlit %.1f (%zu), "
                         "the flash %.1f (%zu); fog mixes lit %d, unlit %d, the flash %d\n",
                 lighting_bit[0], lighting_bit[1], mean[0], distinct[0], mean[1], distinct[1], mean[2], distinct[2], mixed[0],
                 mixed[1], mixed[2]);
    EXPECT(lighting_bit[0] == 1 && lighting_bit[1] == 0);
    EXPECT(mean[0] < mean[1] && distinct[0] > distinct[1]);  // the sun darkens and varies them
    EXPECT(mixed[1] == 0 && mixed[2] > 0);                   // the point light with the sun: the mix
    zmod::light_hooks().mix_fog = mix;
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

TEST(mission_c1m1_nav_waynet_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_nav_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    void* nav = nullptr;
    bool is_nav = false;
    mech3::NavSummary first, after;
    void* marker = nullptr;
    int32_t was = -1;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
      if (frames == 2) {
        nav = mech3::program_mission_waynet();
        is_nav = nav && mech3::program_is_nav_waynet(nav);
        first = mech3::program_nav_summary(nav);
        marker = mech3::program_waypoint_marker();
        was = first.current;
        if (was >= 0 && was < 8) {
          const float* q = first.pos[was];
          const float at[3] = {q[0], ground_under(q[0], q[1] + 100.0f, q[2]), q[2]};
          mech3::move_mech(player, at, 0.0f);
        }
      }
      if (frames == 6) after = mech3::program_nav_summary(nav);
      ++frames;
    };
    CountingPump pump;
    pump.limit = 1200;
    pump.stop = [&] { return frames > 6; };
    app.run(pump);
    std::fprintf(stderr, "  nav: %d points, selected %d, %d watched:", first.count, first.current, first.watched);
    for (int i = 0; i < first.count && i < 8; ++i)
      std::fprintf(stderr, " %s/%s%s", first.labels[i].c_str(), first.names[i].c_str(), first.has_line[i] ? "*" : "");
    std::fprintf(stderr, "; after: selected %d, reached", after.current);
    for (int i = 0; i < after.count && i < 8; ++i) std::fprintf(stderr, " %d", after.reached[i]);
    std::fprintf(stderr, "\n");
    EXPECT(is_nav);
    EXPECT(first.count == 3 && first.labels[0] == "Start" && first.labels[1] == "Able" && first.labels[2] == "Baker");
    EXPECT(marker != nullptr);
    EXPECT(was >= 0 && after.reached[was]);
    EXPECT(after.current == (was + 1) % first.count);  // autonext
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// A field base lost in a campaign (0x004590b0; docs/vehicles.md, A mobile
// field base lost): c1m1 with a campaign game behind the pilot, five spare
// mechs and gear carried; the player shoots mobilehq_03 down. Two bases
// left: four mechs kept (ceil(0.7 x 5), at most 2 a base), the gear cut to
// 70% of its tons, what goes into the game's +0x78 store; the message panel
// shown.
TEST(mission_c1m1_field_base_lost_cuts_the_campaign_store_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_mfb_lost_write"));
  mech3::reset_states();
  reset_process_state();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
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
    mech3::GameEngine::Launch l;  // c1m1
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    mech3::Pilot pilot("Lost");
    mech3::CampaignGame game(&pilot, 2, 1);
    pilot.current = &game;
    for (int32_t m : {0, 4, 7, 9, 12}) game.store.add_mech(m);
    game.store.add_equipment(3, 2, 1)->count = 300;  // heat sinks, a ton each
    game.store.add_equipment(4, 2, 1)->count = 40;
    game.store.add_ammo(2, 1)->count = 2000;
    game.store.add_ammo(0x15, 1)->count = 600;
    auto mechs = [](const mech3::Store& s) {
      int32_t n = 0;
      for (const mech3::ConfigItem* it : s.items)
        if (it->kind() == 0) n += it->count;
      return n;
    };
    auto tons = [](const mech3::Store& s) {
      float t = 0.0f;
      for (const mech3::ConfigItem* it : s.items) t = static_cast<float>(it->weight(-1) + t);
      return t;
    };
    float tons_before = 0.0f;  // in the mission: the ammo's weights read weapons.zrd's lab data
    mech3::Pilot* was_pilot = mech3::current_pilot();
    mech3::current_pilot() = &pilot;
    int frames = 0;
    bool killed = false, shown = false;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      if (frames == 30) tons_before = tons(game.store);
      if (frames == 30)
        for (void* v : mech3::program_vehicles()) {
          mech3::VehicleSummary s;
          if (mech3::describe_vehicle(v, &s) && s.name && std::string(s.name) == "mobilehq_03") {
            void* player = engine.object_of(engine.field32(engine.pilot(), 0x14));
            killed = mech3::program_vehicle_kill(v, player, "MGUN");
            shown = mech3::message_panel() && mech3::message_panel()->timer > 0.0f;
          }
        }
      ++frames;
    };
    CountingPump pump;
    pump.limit = 1200;
    pump.stop = [&] { return frames > 32; };
    app.run(pump);
    const float tons_after = tons(game.store), tons_moved = tons(game.store78);
    std::fprintf(stderr, "  bases %d; mechs %d + %d gone; tons %.1f -> %.1f (+%.1f gone)\n", game.a8,
                 mechs(game.store), mechs(game.store78), tons_before, tons_after, tons_moved);
    EXPECT(killed);
    EXPECT(game.a8 == 2);
    EXPECT(mechs(game.store) == 4 && mechs(game.store78) == 1);
    EXPECT(tons_after <= 0.7f * tons_before && tons_after > 0.7f * tons_before - 2.0f);
    EXPECT(std::fabs(tons_after + tons_moved - tons_before) < 0.01f);
    EXPECT(shown);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
    mech3::current_pilot() = was_pilot;
    pilot.current = nullptr;
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
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
      size_t entities = 0, turrets = 0;
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
        seen.turrets = mech3::tur::turret_list().size();
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
    // The player, the Owens, the command marker, the seven vehicles and the turrets.
    EXPECT(seen.turrets > 0 && seen.entities == 10 + seen.turrets && seen.player_first && seen.sorted);
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
        // 40 m west of the Owens, on the ground, facing east (+x: yaw -90). The
        // opening's ramp animation (call_ramp) turns on the valve "bramp",
        // which powers the Owens up (c1m1's valves.zrd, ai.md section 18):
        // from 80 m the machine guns miss it once it moves.
        mech3::LaunchedMech o;
        mech3::describe_mech(owens, &o);
        const float x = o.pos[0] - 40.0f, z = o.pos[2];
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
// The command marker (cmd_target.flt) is a plain AiTarget (vtable 0x005ccfe0):
// the sensors ask every entity its vt+0x2c and others each frame, and it
// answers as the exe's does (itself, 0x004d2a70; +0x08 for vt+0xcc), none of
// it logged as unported (the survey's 1,540 veh.vt+0x2c a run before).
TEST(mission_c1m1_command_marker_answers_as_an_aitarget_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_marker_write"));
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
    zgame::Handle marker = 0;
    uint32_t answers[8] = {};
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || ++frames != 10) return;
      marker = mech3::pocket_globals().marker;
      mech3::ai::SensorHooks& h = mech3::ai::sensor_hooks();
      const uint32_t slots[8] = {0x24, 0x28, 0x2c, 0x40, 0x50, 0xb4, 0xcc, 0xd0};
      for (int k = 0; k < 8; ++k) answers[k] = h.vcall(marker, slots[k], {});
    };
    CountingPump pump;
    pump.limit = 300;
    app.run(pump);
    const auto& counts = engine.unported_counts();
    EXPECT(marker != 0);
    EXPECT(answers[0] == marker && answers[1] == marker && answers[2] == marker);
    EXPECT(answers[3] == 0 && answers[4] == 0 && answers[5] == 0);
    EXPECT(answers[6] == 0 && answers[7] == 1);  // inactive until the panel places it
    EXPECT(counts.find("veh.vt+0x2c") == counts.end());
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
    // MW3_SURVEY_MISSION: c<op>m<n> (a campaign mission; c4bm<n> or c5m<n> for c4b) or ia<n>
    // (c1's Instant Action map n, wave mode, one ally); c1m1 when unset.
    mech3::GameEngine::Launch l;
    mech3::ActionGame action(nullptr);
    if (const char* m = std::getenv("MW3_SURVEY_MISSION")) {
      int a = 1, b = 1;
      if (std::sscanf(m, "ia%d", &b) == 1) {
        l.type = 2, l.index = 0, l.number = b;
        action.mode = 2, action.x6c = 1, action.slot.mech = 9;
        action.add(5, 1, nullptr, 0);
        l.action = &action;
      } else if (std::sscanf(m, "c4bm%d", &b) == 1) {  // c4b, the campaign's fifth operation
        l.index = 4, l.op = 5, l.number = b;
      } else if (std::sscanf(m, "c%dm%d", &a, &b) == 2) {
        l.index = a - 1, l.op = a, l.number = b;
      }
    }
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
      int32_t home_side = -1;
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
          // its +0x28 here), the Owens is put 250 m off it, facing it: east,
          // else the first side whose straight way back the mission
          // boundary leaves alone (ai.md section 11: a pulled way home ends
          // the order at once).
          for (const auto& ord : br.orders)
            if (auto* r = dynamic_cast<const mech3::ai::AttackReturnState*>(ord.state.get())) {
              const float sides[4][3] = {{1, 0, -1.5707964f}, {-1, 0, 1.5707964f}, {0, 1, 3.1415927f}, {0, -1, 0.0f}};
              int side = 0;
              for (mech3::ai::InOutArea* area = mech3::ai::inout().current; area && side < 3; ++side) {
                const float from[3] = {r->way[0] + 250.0f * sides[side][0], r->way[1], r->way[2] + 250.0f * sides[side][1]};
                if (area->query(from, r->way) == r->way) break;
              }
              seen.home_side = side;
              const float x = r->way[0] + 250.0f * sides[side][0], z = r->way[2] + 250.0f * sides[side][1];
              const float at[3] = {x, ground_under(x, o.pos[1] + 120.0f, z), z};
              mech3::move_mech(owens, at, sides[side][2]);
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
            // order on (+0x80 1) again; the player 30 m ahead of it, facing
            // it, walking at it.
            br.params.put<int32_t>(0x3c, 0);
            br.params.put<int32_t>(0x80, 1);
            float fx = 0, fz = 0;
            mech3::ai::brain_hooks().forward(engine.handle_of(owens), &fx, &fz);
            const float x = o.pos[0] + 30.0f * fx, z = o.pos[2] + 30.0f * fz;
            const float at[3] = {x, ground_under(x, o.pos[1] + 80.0f, z), z};
            mech3::move_mech(player, at, 2.0f * std::asin(o.yaw_sin) + 3.1415927f);
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
    // its range, but the field bases (team 2, 530..600 m off) are within
    // its sensor range (aiv +0x30, 1000 m): it has a plan on one (phase 2),
    // its range the plan's, its look-around going.
    EXPECT(seen.fire_phase == 2 && seen.look_phase >= 1);
    EXPECT(seen.fire_max > 0 && seen.fire_range < seen.fire_max);
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
      std::fprintf(stderr, "HOME chase %g -> %g at %d; side %d done %d orders %d farthest %g last %g range %g\n",
                   seen.chase_from, seen.chase_to, seen.attacked, seen.home_side, seen.home_done, seen.home_orders,
                   seen.home_start, seen.home_last, seen.home_range);
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
// c2 mission 1 (docs/mission-start.md, the lancemates): Dominic's record says
// can_configure, so the mission start makes him a slot of the campaign and
// the launch makes his mech from it (0x0049e5e0) on the player's net; he is
// in the command mode's panel and takes a lance order (0: on his contact on
// the player, the follow).
// Instant Action from a made game (the Action screen is not ported): c1's
// ia1 in wave mode with one ally. The launch's line-up (0x0049dc80(1)) makes
// the ally and wave 1's enemies (0x005e33a8: a Firefly and an Owens) from
// unused aiv.zrd records, with their AI on; with them destroyed the win
// (0x0048f740) counts the wave and the line-up runs again (0x0049dc80(0)):
// wave 2's three (two Fireflies and a Bushwacker). docs/mission-start.md 13.
TEST(mission_ia1_waves_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_ia_write"));
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
    mech3::ActionGame game(nullptr);
    game.mode = 2, game.x6c = 1;
    game.slot.mech = 9, game.slot.skin = 2;
    game.add(5, 1, nullptr, 0);  // an ally: a Champion
    game.add(14, 0, nullptr, 0);  // an enemy the wave replaces
    mech3::GameEngine::Launch l;
    l.type = 2, l.index = 0, l.number = 1;
    l.action = &game;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Made {
      std::vector<int32_t> allies, enemies;  // roster numbers
      int brains = 0;
    };
    auto survey = [&](Made& out) {  // every AI mech now, by its side
      int32_t made = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
      void* player = mech3::mech_mission_player();
      const mech3::ai::AiTarget* pt = mech3::ai::brain_hooks().targetable(engine.handle_of(player));
      for (int32_t q = 0; q < made; ++q) {
        const mech3::ai::AiTarget* t = mech3::ai::brain_hooks().targetable(engine.handle_of(ai[q].mech));
        if (!t || !pt) continue;
        ((pt->allies & t->team) ? out.allies : out.enemies).push_back(mech3::program_mech_roster(ai[q].mech));
        out.brains += static_cast<mech3::AiControls*>(ai[q].controls)->brain != nullptr;
      }
    };
    Made first, second;
    int frames = 0, wave2_at = -1;
    std::vector<void*> killed;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      int32_t made = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
      if (frames == 3) {
        survey(first);
        void* player = mech3::mech_mission_player();
        const mech3::ai::AiTarget* pt = mech3::ai::brain_hooks().targetable(engine.handle_of(player));
        for (int32_t q = 0; q < made; ++q) {  // wave 1 destroyed
          const mech3::ai::AiTarget* t = mech3::ai::brain_hooks().targetable(engine.handle_of(ai[q].mech));
          if (t && pt && !(pt->allies & t->team)) mech3::mech_kill(ai[q].mech, player), killed.push_back(ai[q].mech);
        }
      } else if (frames == 63) {
        // Dead and falling for 2 s: the wrecks deleted, as Instant Action does
        // at 180 s; "destroy the enemies" is met when they leave zEntity's list.
        for (void* m : killed) mech3::program_mech_wreck_gone(m);
      } else if (frames > 3 && wave2_at < 0 && game.x6c == 2) {
        wave2_at = frames;
        survey(second);
      }
    };
    CountingPump pump;
    pump.limit = 400;
    app.run(pump);
    std::sort(first.enemies.begin(), first.enemies.end());
    std::sort(second.enemies.begin(), second.enemies.end());
    if (std::getenv("MW3_IA_LOG")) {
      auto list = [](const std::vector<int32_t>& v) { std::string t; for (int32_t x : v) t += std::to_string(x) + " "; return t; };
      std::fprintf(stderr, "IA first allies %s enemies %s brains %d; wave2 at %d allies %s enemies %s brains %d; wave %d won %d\n",
                   list(first.allies).c_str(), list(first.enemies).c_str(), first.brains, wave2_at,
                   list(second.allies).c_str(), list(second.enemies).c_str(), second.brains, game.x6c, game.x70);
    }
    EXPECT(first.allies == std::vector<int32_t>{5});
    EXPECT((first.enemies == std::vector<int32_t>{8, 11}));
    EXPECT(first.brains == 3);
    EXPECT(wave2_at > 3 && game.x6c == 2);
    // The ally stays (a new wave remakes the enemies only).
    EXPECT(second.allies == std::vector<int32_t>{5} && (second.enemies == std::vector<int32_t>{3, 8, 8}));
    EXPECT(second.brains == 4 && game.x70 == 1);
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

// A mission restarted twice (its cleanup, then the Load state again): each
// load has the fresh one's AI (zEntity's list, the AI mechs, the targets,
// the basic vehicles, the AI controls alive, the four reset lists 0x004d8610
// empties), and after the last cleanup the AI controls alive are back to
// before the first (the vehicles' 0x004ac800 on the way). Run under ASan too.
TEST(mission_c1m1_restarted_twice_keeps_no_ai_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_restart_write"));
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
    struct Snap {
      size_t entities = 0, targets = 0, vehicles = 0, lists = 0;
      int32_t ai_mechs = 0, mech_controls = 0, vehicle_controls = 0;
      bool operator==(const Snap& o) const {
        return entities == o.entities && targets == o.targets && vehicles == o.vehicles && lists == o.lists &&
               ai_mechs == o.ai_mechs && mech_controls == o.mech_controls && vehicle_controls == o.vehicle_controls;
      }
    };
    auto snap = [] {
      Snap n;
      n.entities = mech3::ai::entity_list().size();
      n.targets = mech3::ai::target_list().size();
      n.vehicles = mech3::program_vehicles().size();
      for (uint32_t a : {0x00604038u, 0x00604048u, 0x00604078u, 0x00603f90u}) n.lists += mech3::ai::reset_list(a)->items.size();
      mech3::mission_ai_mechs(&n.ai_mechs);
      n.mech_controls = mech3::ai_mech_controls_alive();
      n.vehicle_controls = mech3::ai::ai_vehicle_controls_alive();
      return n;
    };
    const Snap before = snap();
    std::vector<Snap> loads;
    mech3::GameEngine::Launch l;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || loads.size() >= 3) return;
      if (++frames == 3) loads.push_back(snap());
      if (frames == 5 && loads.size() < 3) {  // restart: the cleanup, then the Load state again
        frames = 0;
        mech3::missions().cleanup(false);
        engine.set_launch(l);
        app.replace(&mech3::states().load, 0);
      }
    };
    CountingPump pump;
    pump.stop = [&] { return loads.size() >= 3 && frames >= 5; };
    pump.limit = 3000;
    app.run(pump);
    mech3::missions().cleanup(false);
    const Snap after = snap();
    if (std::getenv("MW3_RESTART_LOG"))
      for (const Snap& n : {before, loads.size() > 0 ? loads[0] : Snap{}, loads.size() > 1 ? loads[1] : Snap{},
                            loads.size() > 2 ? loads[2] : Snap{}, after})
        std::fprintf(stderr, "RESTART entities %zu targets %zu vehicles %zu lists %zu ai mechs %d controls %d / %d\n",
                     n.entities, n.targets, n.vehicles, n.lists, n.ai_mechs, n.mech_controls, n.vehicle_controls);
    EXPECT(loads.size() == 3);
    EXPECT(loads.size() == 3 && loads[1] == loads[0] && loads[2] == loads[0]);
    EXPECT(loads.size() == 3 && loads[0].ai_mechs > 0 && loads[0].vehicle_controls > before.vehicle_controls);
    EXPECT(after.mech_controls == before.mech_controls && after.vehicle_controls == before.vehicle_controls);
    EXPECT(after.lists == 0);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  reset_process_state();
  mech3::options() = mech3::Options{};
}

// The AI's target pick (0x004b6790) scores each contact by distance and its
// heat (vt+0x9c, x (1.5 - heat): lower is better), so an enemy pilot turns to
// the hotter of two mechs at nearly the same range. c2 mission 1: an enemy
// with the player 200 m off and Dominic 210 m on its other side; cool, it
// takes one of them; with the other at heat 1.4, that one. docs/ai.md 17.7.
TEST(mission_c2m1_ai_pick_goes_to_the_hotter_mech_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_heatpick_write"));
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
    mech3::GameEngine::Launch l;  // c2 mission 1: Dominic in the lance
    l.index = 1, l.op = 2, l.number = 1;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    void *player = nullptr, *dominic = nullptr, *enemy = nullptr;
    mech3::ai::Brain* brain = nullptr;
    zgame::Handle cool = 0, hot = 0, ph = 0, dh = 0, other_h = 0;
    float other_heat = -1;
    int frames = 0;
    auto pick = [&]() -> zgame::Handle {
      mech3::ai::FirePlan* p = mech3::ai::volley_plan_pick(brain->firing);
      delete p;
      const mech3::ai::AiTarget* t = mech3::ai::brain_hooks().targetable(engine.handle_of(enemy));
      return t && t->target ? mech3::ai::brain_hooks().contact_vehicle(t->target) : 0;
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (frames == 3) {
        player = mech3::mech_mission_player();
        const mech3::ai::AiTarget* pt = mech3::ai::brain_hooks().targetable(engine.handle_of(player));
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        for (int32_t q = 0; q < made; ++q) {
          const mech3::ai::AiTarget* t = mech3::ai::brain_hooks().targetable(engine.handle_of(ai[q].mech));
          if (ai[q].name && std::string(ai[q].name) == "Dominic_Paine") dominic = ai[q].mech;
          else if (!enemy && t && pt && !(pt->allies & t->team) && static_cast<mech3::AiControls*>(ai[q].controls)->brain) {
            enemy = ai[q].mech;
            brain = static_cast<mech3::AiControls*>(ai[q].controls)->brain.get();
          }
        }
        if (!player || !dominic || !enemy) return;
        mech3::LaunchedMech e;
        mech3::describe_mech(enemy, &e);
        auto put = [&](void* m, float dx) {
          const float x = e.pos[0] + dx, z = e.pos[2];
          const float at[3] = {x, ground_under(x, e.pos[1] + 100.0f, z), z};
          mech3::move_mech(m, at, dx > 0 ? -1.5707964f : 1.5707964f);
        };
        // The enemy turned so both are side-on to it: the pick's turn factor
        // ((2 - its forward . the direction) squared) the same for each.
        const float here[3] = {e.pos[0], e.pos[1], e.pos[2]};
        mech3::move_mech(enemy, here, 0.0f);
        put(player, 200.0f), put(dominic, -210.0f);
        ph = engine.handle_of(player), dh = engine.handle_of(dominic);
      } else if (frames == 10 && brain) {  // the sensors' contacts on both
        mech3::mech_set_heat(player, 0), mech3::mech_set_heat(dominic, 0);
        cool = pick();
        void* other = cool == ph ? dominic : player;  // the one it passed over, made hot
        mech3::mech_set_heat(other, 1.4f);
        hot = pick();
        other_h = engine.handle_of(other);
        other_heat = mech3::ai::brain_hooks().heat(other_h);
        mech3::mech_set_heat(other, 0);
      }
    };
    CountingPump pump;
    pump.stop = [&] { return frames > 10; };
    pump.limit = 600;
    app.run(pump);
    if (std::getenv("MW3_PICK_LOG"))
      std::fprintf(stderr, "PICK cool %x hot %x player %x dominic %x heat %g\n", cool, hot, ph, dh, other_heat);
    EXPECT(enemy && dominic);
    EXPECT(cool == ph || cool == dh);   // one of the two mechs, both cool
    EXPECT(hot == other_h && hot != cool && other_heat == 1.4f);  // the other, once hot
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

TEST(mission_c2m1_lancemate_made_from_his_slot_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_mate_write"));
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
    mech3::GameEngine::Launch l;  // c2 mission 1: Dominic in the lance
    l.index = 1, l.op = 2, l.number = 1;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    struct Seen {
      void* dominic = nullptr;
      void* player = nullptr;
      mech3::ai::Brain* brain = nullptr;
      zgame::Handle dv = 0, pv = 0;
      int frames = 0, follows = 0, slots = 0, slot_record = -1, slot_mech = -2;
      int64_t duties = -1, his_triggers = -1;
      bool in_panel = false, picked = false, ordered = false;
      float range = 0;
    } seen;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        if (auto* c = static_cast<mech3::CampaignGame*>(mech3::program_launch_campaign())) {
          seen.slots = c->slot_count();
          if (mech3::GameSlot* s = c->slot_at(1)) seen.slot_record = s->x14, seen.slot_mech = s->mech;
        }
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        for (int32_t i = 0; i < made; ++i)
          if (ai[i].name && std::string(ai[i].name) == "Dominic_Paine") {
            seen.dominic = ai[i].mech;
            seen.brain = static_cast<mech3::AiControls*>(ai[i].controls)->brain.get();
          }
        seen.player = mech3::mech_mission_player();
        if (!seen.dominic || !seen.player) return;
        seen.dv = engine.handle_of(seen.dominic), seen.pv = engine.handle_of(seen.player);
        // The mech lab's empty slot (-1): the launch hands its record's place
        // triggers to the player's settings instead (0x0049e540 -> 0x0049e4e0).
        auto* pc = dynamic_cast<mech3::PlayerControls*>(mech3::mech_controls(seen.player));
        auto* c = static_cast<mech3::CampaignGame*>(mech3::program_launch_campaign());
        if (mech3::GameSlot* s = pc && c ? c->slot_at(1) : nullptr) {
          auto size = [](const std::list<mech3::ai::VListEntry>* v) { return v ? static_cast<int64_t>(v->size()) : 0; };
          seen.his_triggers = 0;
          if (const auto* v = seen.brain->params.vlist)
            for (const mech3::ai::VListEntry& e : *v) seen.his_triggers += e.region != nullptr;
          const int64_t before = size(pc->settings.vlist);
          const int32_t mech = s->mech;
          s->mech = -1;
          mech3::program_launch_lancemates();
          s->mech = mech;
          seen.duties = size(pc->settings.vlist) - before;
        }
        if (const mech3::ai::AiTarget* t = mech3::ai::brain_hooks().targetable(seen.pv)) seen.range = t->range;
        // The player 40 m off Dominic: in sight, his contact on the player made.
        mech3::LaunchedMech d;
        mech3::describe_mech(seen.dominic, &d);
        const float px = d.pos[0] - 40.0f, pz = d.pos[2];
        const float at[3] = {px, ground_under(px, d.pos[1] + 100.0f, pz), pz};
        mech3::move_mech(seen.player, at, 1.5707964f);
        return;
      }
      if (!seen.dominic || !seen.brain) return;
      mech3::MechControls* pc = mech3::mech_controls(seen.player);
      mech3::PocketControls* pocket = pc ? pc->pocket() : nullptr;
      if (seen.frames == 60) {
        mech3::player_command(0x4f);  // the command mode
      } else if (seen.frames == 62 && pocket) {
        pocket->command(0x1b);  // the number key: lancemate 1
        mech3::PocketUnit u[4];
        const int32_t n = mech3::pocket_panel_units(*pocket, u, 4);
        for (int32_t q = 0; q < n; ++q) {
          const bool his = u[q].mech && engine.handle_of(u[q].mech) == seen.dv;
          seen.in_panel |= his;
          seen.picked |= his && u[q].picked;
        }
      } else if (seen.frames == 64 && pocket) {
        mech3::player_command(0x52);  // lance order 0: on his contact on the player
        mech3::ai::Brain& b = *seen.brain;
        auto* fo = dynamic_cast<mech3::ai::FollowState*>(b.current().state.get());
        auto* vp = dynamic_cast<mech3::ai::VehiclePart*>(b.current().net.get());
        seen.ordered = fo && vp && vp->target && vp->target->vehicle == seen.pv;
      } else if (seen.frames > 64) {
        auto* vp = dynamic_cast<mech3::ai::VehiclePart*>(seen.brain->current().net.get());
        if (vp && vp->target && vp->target->vehicle == seen.pv) ++seen.follows;
      }
    };
    CountingPump pump;
    pump.limit = 300;
    app.run(pump);
    if (std::getenv("MW3_LANCE_LOG"))
      std::fprintf(stderr, "MATE slots %d record %d mech %d dominic %d brain %d range %g panel %d picked %d ordered %d follows %d of %d\n",
                   seen.slots, seen.slot_record, seen.slot_mech, seen.dominic != nullptr, seen.brain != nullptr, seen.range,
                   seen.in_panel, seen.picked, seen.ordered, seen.follows, seen.frames - 64);
    if (std::getenv("MW3_LANCE_LOG"))
      std::fprintf(stderr, "MATE duties %lld of %lld\n", static_cast<long long>(seen.duties),
                   static_cast<long long>(seen.his_triggers));
    EXPECT(seen.slots == 2 && seen.slot_mech >= 0 && seen.slot_record > 0);
    EXPECT(seen.dominic && seen.brain);
    EXPECT(seen.range == 1000.0f);  // a campaign's: the cap (0x004d6d20)
    EXPECT(seen.in_panel && seen.picked);  // the panel lists those under orders (+0xbc bit 4) and up
    EXPECT(seen.ordered && seen.follows == seen.frames - 64);  // and keeps it, every frame after
    EXPECT(seen.his_triggers > 0 && seen.duties == seen.his_triggers);
    EXPECT(engine.unported_counts().count("ictl.vt+0x90") == 0);
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

TEST(mission_c1m4_pocket_commander_sends_dominic_to_a_spot_headless) {
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
      void* dominic = nullptr;
      void* player = nullptr;
      mech3::ai::Brain* brain = nullptr;
      zgame::Handle dv = 0, pv = 0, marker = 0;
      int frames = 0, follows_marker = 0;
      bool mode = false, picked = false, placed = false, wired_pick = false, ordered = false;
      float d0 = 0, closest = 1e9f;
      uint32_t marker_team = 0, dominic_enemies = 0;
    } seen;
    static float spot[3];
    static zgame::Handle ground;
    auto dist_to_spot = [&]() {
      mech3::LaunchedMech d;
      mech3::describe_mech(seen.dominic, &d);
      const float dx = d.pos[0] - spot[0], dz = d.pos[2] - spot[2];
      return std::sqrt(dx * dx + dz * dz);
    };
    mech3::FrameCalls saved_frame;
    mech3::ControlsCalls saved_controls;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        for (int32_t i = 0; i < made; ++i)
          if (ai[i].name && std::string(ai[i].name) == "Dominic_Paine") {
            seen.dominic = ai[i].mech;
            seen.brain = static_cast<mech3::AiControls*>(ai[i].controls)->brain.get();
          }
        seen.player = mech3::mech_mission_player();
        if (!seen.dominic || !seen.player) return;
        seen.dv = engine.handle_of(seen.dominic), seen.pv = engine.handle_of(seen.player);
        seen.marker = mech3::pocket_globals().marker;  // made at the mission's start (0x0046aaa0)
        if (mech3::ai::Valve* w = mech3::ai::valve_find("dominic_wakes")) w->set(1);
        mech3::LaunchedMech d;  // the spot: 200 m from Dominic, on the ground
        mech3::describe_mech(seen.dominic, &d);
        spot[0] = d.pos[0] + 200.0f, spot[2] = d.pos[2];
        spot[1] = ground_under(spot[0], d.pos[1] + 100.0f, spot[2]);
        ground = engine.handle_of(mech3::world_hooks().world);  // a node that is no entity's
        // The player 40 m off Dominic: in sight, so his line shows (+0xbc bit 3).
        const float px = d.pos[0] - 40.0f, pz = d.pos[2];
        const float at[3] = {px, ground_under(px, d.pos[1] + 100.0f, pz), pz};
        mech3::move_mech(seen.player, at, 1.5707964f);
        return;
      }
      if (!seen.dominic || !seen.brain) return;
      mech3::MechControls* pc = mech3::mech_controls(seen.player);
      mech3::PocketControls* pocket = pc ? pc->pocket() : nullptr;
      mech3::FrameCalls& f = mech3::frame_calls();
      if (seen.frames == 60) {
        mech3::player_command(0x4f);  // the command mode's key (lance order 2)
        pc = mech3::mech_controls(seen.player);
        seen.mode = pc && pc->pocket() && pc->pocket()->panel;
      } else if (seen.frames == 62 && pocket) {
        pocket->command(0x1b);  // the number key: lancemate 1
        mech3::PocketUnit u[4];
        const int32_t n = mech3::pocket_panel_units(*pocket, u, 4);
        for (int32_t q = 0; q < n; ++q)
          seen.picked |= u[q].picked && u[q].mech && engine.handle_of(u[q].mech) == seen.dv;
      } else if (seen.frames == 64 && pocket) {
        // The program's pick on the real HUD: the reticle's record, its node, its spot.
        mech3::Mech& pm = *static_cast<mech3::Mech*>(seen.player);
        const zgame::Handle hh = f.view_hud ? f.view_hud(pm) : 0;
        auto* hud = static_cast<mech3::Hud*>(engine.object_of(hh));
        if (hud && f.reticle_pick && f.record_node && f.reticle_place) {
          const zgame::Handle rec = f.reticle_pick(hh);
          float at[3] = {1, 2, 3};
          f.reticle_place(hh, at);
          seen.wired_pick = rec == engine.handle_of(&hud->reticle.hit) &&
                            f.record_node(rec) == engine.handle_of(hud->reticle.hit.node) &&
                            at[0] == hud->reticle.aim_point[0] && at[2] == hud->reticle.aim_point[2];
        }
        // Then the cursor on the bare spot for one frame, the trigger let go:
        // the HUD's own aim ray is not what is under test here.
        saved_frame = f, saved_controls = mech3::controls_calls();
        f.reticle_pick = [](zgame::Handle) -> zgame::Handle { return 1; };
        f.record_node = [](zgame::Handle) -> uint32_t { return ground; };
        f.reticle_place = [](zgame::Handle, float out[3]) { std::memcpy(out, spot, 12); };
        mech3::controls_calls().command = [](int cmd) -> uint32_t { return cmd == 0x3d ? 4u : 0u; };
      } else if (seen.frames == 65) {
        f = saved_frame;
        mech3::controls_calls() = saved_controls;
        const mech3::PocketMarker* k = mech3::program_pocket_marker(seen.marker);
        seen.placed = k && k->active && mech3::pocket_globals().cursor == seen.marker;
      } else if (seen.frames == 66 && pocket) {
        seen.d0 = dist_to_spot();
        mech3::player_command(0x54);  // "only an enemy" (order 10) on the cursor: the marker
        pc = mech3::mech_controls(seen.player);
        mech3::ai::Brain& b = *seen.brain;
        auto* fo = dynamic_cast<mech3::ai::FollowState*>(b.current().state.get());
        auto* vp = dynamic_cast<mech3::ai::VehiclePart*>(b.current().net.get());
        seen.ordered = !pc->pocket() && fo && vp && vp->target && vp->target->vehicle == seen.marker;
        const mech3::ai::AiTarget* mt = mech3::ai::brain_hooks().targetable(seen.marker);
        const mech3::ai::AiTarget* dt = mech3::ai::brain_hooks().targetable(seen.dv);
        seen.marker_team = mt ? mt->team : 0;
        seen.dominic_enemies = dt ? dt->enemies : 0;
      } else if (seen.frames > 66) {
        mech3::ai::Brain& b = *seen.brain;
        auto* vp = dynamic_cast<mech3::ai::VehiclePart*>(b.current().net.get());
        if (vp && vp->target && vp->target->vehicle == seen.marker) ++seen.follows_marker;
        seen.closest = std::min(seen.closest, dist_to_spot());
      }
    };
    CountingPump pump;
    pump.limit = 1100;
    app.run(pump);
    if (std::getenv("MW3_LANCE_LOG"))
      std::fprintf(stderr, "POCKET mode %d picked %d wired %d placed %d ordered %d team %x enemies %x follows %d d0 %g closest %g\n",
                   seen.mode, seen.picked, seen.wired_pick, seen.placed, seen.ordered, seen.marker_team,
                   seen.dominic_enemies, seen.follows_marker, seen.d0, seen.closest);
    EXPECT(seen.marker != 0);
    EXPECT(seen.mode && seen.picked && seen.wired_pick && seen.placed);
    EXPECT(seen.ordered && (seen.marker_team & seen.dominic_enemies));
    // He heads for it and stops at his reach of an enemy, as for any (exact:
    // the run repeats with reset_process_state's fixed seed).
    EXPECT(seen.follows_marker == 902 && std::fabs(seen.d0 - 199.349f) < 1e-3f);
    EXPECT(std::fabs(seen.closest - 120.729f) < 1e-3f);
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
      uint32_t player_team = 0;  // copied: the mission's AiTargets are gone after the run
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
        seen.player_team = seen.pt->team;
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
    EXPECT(seen.player_team && !(seen.enemies_before & seen.player_team) && (seen.enemies_after & seen.player_team));
    EXPECT(!(seen.allies_after & seen.player_team));
    // The run repeats bit for bit (reset_process_state's fixed seed: the
    // game's own seed is the clock), so its outcome is exact: these change
    // only when the AI, the walk or the weapons do.
    if (!std::getenv("MW3_LANCE_AT") && !std::getenv("MW3_LANCE_DIR")) {
      EXPECT(seen.fired_before == 53 && seen.fired_after == 1009 && seen.on_enemy == 1139);
      EXPECT(std::fabs(seen.d0 - 399.597f) < 1e-3f && std::fabs(seen.closest - 71.1886f) < 1e-3f);
      EXPECT(seen.enemies_before == 0xfffffffcu && seen.enemies_after == 0x7ffffffeu);
      EXPECT(seen.on_player == 385);  // his next pick, once the player is 30 m off
      EXPECT(seen.phases[2] == 2 && seen.phases[3] == 2 && seen.phases[4] == 1009 && seen.phases[5] == 126);
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

// The player's allies fight only what comes in reach (docs/ai.md 17.8b): in
// c1 mission 4 Dominic, woken, with an enemy put 60 m in front of him and no
// order given, takes it as his target and fires at it while he follows the
// player (his own order), never closing in.
TEST(mission_c1m4_a_wreck_is_deleted_and_forgotten_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_wreck_write"));
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
      void* dominic = nullptr;
      void* enemy = nullptr;
      zgame::Handle eh = 0;
      std::string name;
      int frames = 0, killed_at = -1, gone_at = -1;
      bool dominic_on = false, player_on = false, locked = false;
      std::vector<std::string> before, after, later;
      bool freed = false, off_list = false, found_before = false, found_by_name = true, handle_alive = true;
    } seen;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      void* player = mech3::mech_mission_player();
      if (seen.frames == 3) {
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        for (int32_t i = 0; i < made; ++i) {
          const std::string n = ai[i].name ? ai[i].name : "";
          if (n == "Dominic_Paine") seen.dominic = ai[i].mech;
        }
        if (!seen.dominic) return;
        // The first enemy mech 120 m north of Dominic and the player; the
        // others 3 km off.
        mech3::LaunchedMech d;
        mech3::describe_mech(seen.dominic, &d);
        for (int32_t i = 0; i < made; ++i) {
          if (ai[i].mech == seen.dominic) continue;
          float x = d.pos[0] + 3000.0f, z = d.pos[2] + 3000.0f;
          if (!seen.enemy) {
            seen.enemy = ai[i].mech;
            seen.name = ai[i].name ? ai[i].name : "";
            x = d.pos[0], z = d.pos[2] + 120.0f;
          }
          const float at[3] = {x, ground_under(x, d.pos[1] + 500.0f, z), z};
          mech3::move_mech(ai[i].mech, at, 0.0f);
        }
        const float pat[3] = {d.pos[0] + 30.0f, ground_under(d.pos[0] + 30.0f, d.pos[1] + 500.0f, d.pos[2]), d.pos[2]};
        mech3::move_mech(player, pat, 0.0f);
        seen.eh = engine.handle_of(seen.enemy);
        if (mech3::ai::Valve* w = mech3::ai::valve_find("dominic_wakes")) w->set(1);
        return;
      }
      if (!seen.enemy) return;
      if (seen.frames == 40) {  // both target it, the player's first weapon locked on it
        for (void* who : {seen.dominic, player}) {  // its contact on the enemy made the target (vt+0xc0)
          mech3::ai::AiTarget* t = mech3::ai::sensor_hooks().fields(engine.handle_of(who));
          if (!t) continue;
          for (mech3::ai::Contact* c : t->contacts)
            if (c->seen == seen.eh) t->set_target(c, false);
        }
        auto on = [&](void* who) {
          const mech3::ai::AiTarget* t = mech3::ai::sensor_hooks().fields(engine.handle_of(who));
          return t && t->target && t->target->seen == seen.eh;
        };
        seen.dominic_on = on(seen.dominic);
        seen.player_on = on(player);
        seen.locked = mech3::program_mech_lock_on(player, seen.enemy);
        mech3::program_mech_set_attacker(seen.dominic, seen.enemy);  // it hit Dominic last
        seen.before = mech3::program_holders_of(seen.eh, seen.enemy);
        seen.found_before = mech3::ai::valve_hooks().target_find(seen.name.c_str()) == seen.eh;
        mech3::mech_kill(seen.enemy, player);
        seen.killed_at = seen.frames;
        return;
      }
      if (seen.killed_at > 0 && seen.gone_at < 0 && seen.frames == seen.killed_at + 60) {
        // Dead and falling for 2 s: deleted as instant action does at 180 s.
        mech3::program_mech_wreck_gone(seen.enemy);
        seen.gone_at = seen.frames;
        seen.freed = !mech3::entity_is_mech(seen.enemy);
        const auto& l = mech3::ai::entity_list();
        seen.off_list = std::find(l.begin(), l.end(), seen.eh) == l.end();
        seen.after = mech3::program_holders_of(seen.eh, seen.enemy);
        // The program's own: the valves' lookups by its name, its old handle.
        seen.found_by_name = mech3::ai::valve_hooks().target_find(seen.name.c_str()) != 0 ||
                             mech3::ai::valve_hooks().entity_find(seen.name.c_str()) != 0;
        seen.handle_alive = engine.object_of(seen.eh) != nullptr;
        return;
      }
      if (seen.gone_at > 0 && seen.frames == seen.gone_at + 30) seen.later = mech3::program_holders_of(seen.eh, seen.enemy);
    };
    CountingPump pump;
    pump.limit = 1200;
    pump.stop = [&] { return seen.gone_at > 0 && seen.frames > seen.gone_at + 31; };
    app.run(pump);
    auto list = [](const std::vector<std::string>& v) {
      std::string s;
      for (const std::string& x : v) s += (s.empty() ? "" : ", ") + x;
      return s.empty() ? std::string("none") : s;
    };
    std::fprintf(stderr, "  wreck %s: Dominic on it %d, the player %d, locked %d; holders before: %s; after the delete: %s; "
                         "30 frames on: %s; freed %d, off the list %d; by name before %d after %d; old handle alive %d\n",
                 seen.name.c_str(), seen.dominic_on, seen.player_on, seen.locked, list(seen.before).c_str(),
                 list(seen.after).c_str(), list(seen.later).c_str(), seen.freed, seen.off_list, seen.found_before,
                 seen.found_by_name, seen.handle_alive);
    EXPECT(seen.enemy && seen.dominic_on && seen.player_on && seen.locked);
    // The two targets, their contacts, the lock, Dominic's last attacker.
    EXPECT(seen.before.size() >= 5 &&
           std::find(seen.before.begin(), seen.before.end(), "Dominic Paine: its last attacker") != seen.before.end());
    EXPECT(seen.freed && seen.off_list);
    // The program's lists (not the exe's): its name finds nothing, its handle answers null.
    EXPECT(seen.found_before && !seen.found_by_name && !seen.handle_alive);
    // The targets.zrd objects are not told (not on zEntity's list, as in
    // the exe): their contacts on it go with the next sensors frame.
    for (const std::string& h : seen.after) EXPECT(h == "a targets.zrd object: a contact");
    EXPECT(seen.later.empty());
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

TEST(mission_c1m4_dominic_fires_on_a_target_headless) {
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
      zgame::Handle dv = 0, hut = 0;
      int frames = 0, on_target = 0, firing = 0, aiming = 0, target_contacts = 0;
      float health0 = 0, health = 0;
      std::string first_target;
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
          }
        }
        if (!seen.dominic || !seen.brain) return;
        seen.dv = engine.handle_of(seen.dominic);
        // The enemy mechs taken 3 km off: the huts are the only enemies near.
        for (int32_t i = 0; i < made; ++i) {
          if (ai[i].mech == seen.dominic) continue;
          mech3::LaunchedMech m;
          mech3::describe_mech(ai[i].mech, &m);
          const float x = m.pos[0] + 3000.0f, z = m.pos[2] + 3000.0f;
          const float at[3] = {x, ground_under(x, m.pos[1] + 500.0f, z), z};
          mech3::move_mech(ai[i].mech, at, 0.0f);
        }
        // Dominic 80 m west of the hut ghsD2 (targets.zrd, team 2), facing it,
        // the player 30 m behind him (his own order follows the player).
        mech3::ai::CZAiTarget* hut = nullptr;
        for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())
          if (t->node && std::string(static_cast<const zcls::Node*>(engine.object_of(t->node))->name()) == "ghsD2") hut = t;
        if (!hut) return;
        seen.hut = hut->self;
        seen.health0 = hut->desc.health;
        float hp[3] = {};
        zcls::node_world_position(static_cast<zcls::Node*>(engine.object_of(hut->node)), hp);
        const float dx = hp[0] - 80.0f, dz = hp[2];
        const float at[3] = {dx, ground_under(dx, hp[1] + 100.0f, dz), dz};
        mech3::move_mech(seen.dominic, at, 1.5707964f);
        const float px = dx - 30.0f;
        const float pat[3] = {px, ground_under(px, hp[1] + 100.0f, dz), dz};
        mech3::move_mech(mech3::mech_mission_player(), pat, 1.5707964f);
        if (mech3::ai::Valve* w = mech3::ai::valve_find("dominic_wakes")) w->set(1);  // he starts powered down
        return;
      }
      if (!seen.brain || !seen.hut) return;
      mech3::ai::Brain& b = *seen.brain;
      const mech3::ai::AiTarget* t = mech3::ai::brain_hooks().targetable(seen.dv);
      if (t) {
        for (const mech3::ai::Contact* c : t->contacts)
          for (mech3::ai::CZAiTarget* x : mech3::ai::target_list())
            if (c->seen == x->self) ++seen.target_contacts;
        if (t->target)
          for (mech3::ai::CZAiTarget* x : mech3::ai::target_list())
            if (t->target->seen == x->self) {
              ++seen.on_target;
              if (seen.first_target.empty())
                seen.first_target = static_cast<const zcls::Node*>(engine.object_of(x->node))->name();
            }
      }
      if (b.firing.phase == 2 || b.firing.phase == 3) ++seen.aiming;
      if (b.firing.phase == 4 || b.firing.phase == 7) ++seen.firing;
      float lowest = 1e9f;  // the huts' lowest health
      for (mech3::ai::CZAiTarget* x : mech3::ai::target_list()) {
        const std::string n = x->node ? static_cast<const zcls::Node*>(engine.object_of(x->node))->name() : "";
        if (n.rfind("ghsD", 0) == 0) lowest = std::min(lowest, x->desc.health);
      }
      seen.health = lowest;
    };
    CountingPump pump;
    pump.limit = 1200;  // 40 s
    app.run(pump);
    if (std::getenv("MW3_LANCE_LOG"))
      std::fprintf(stderr, "TARGET %s: contacts %d on it %d aiming %d firing %d health %g -> %g frames %d\n",
                   seen.first_target.c_str(), seen.target_contacts, seen.on_target, seen.aiming, seen.firing,
                   seen.health0, seen.health, seen.frames);
    // His sensors list the huts; the firing picks one (the contact's entity,
    // 0x004e6fe0, a target too), lines up and fires (exact: the run repeats
    // with reset_process_state's fixed seed). His spot is in one of the
    // mission boundary's no-go patches (ai.md section 11), so he walks out of
    // it meanwhile. In the first second he counts as hit (the hit record
    // starts at time 0), so scan_hit gives him a manoeuvre (ai.md 17.2): the
    // first hut is down within the 40 s, untouched at 30.
    EXPECT(seen.hut != 0 && seen.target_contacts == 2551 && seen.first_target == "ghsD2");
    EXPECT(seen.on_target == 1062 && seen.aiming == 54 && seen.firing == 883);
    EXPECT(seen.health0 == 30.0f && seen.health == 0.0f);
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

TEST(mission_c1m4_dominic_stops_at_a_no_go_patch_headless) {
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
    // c1 m4's largest red patch of the mission boundary (ai.md section 11),
    // about 500 m across: the player in its middle, Dominic 230 m east of
    // it on open ground; his own order follows the player.
    struct Seen {
      mech3::ai::Brain* brain = nullptr;
      void* dominic = nullptr;
      const mech3::ai::InOutPolygon* patch = nullptr;
      int frames = 0, inside = 0, first_in = 0, last_in = 0;
      float d0 = 0, east = 0, west = 1e9f, last[3] = {};
    } seen;
    // Even-odd in x and z over the patch's corners.
    auto in_patch = [&](const float* p) {
      bool in = false;
      const auto& v = seen.patch->verts;
      for (size_t i = 0, j = v.size() - 1; i < v.size(); j = i++) {
        const float* a = v[i].pos;
        const float* b = v[j].pos;
        if ((a[2] > p[2]) != (b[2] > p[2]) && p[0] < (b[0] - a[0]) * (p[2] - a[2]) / (b[2] - a[2]) + a[0]) in = !in;
      }
      return in;
    };
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        for (int32_t i = 0; i < made; ++i)
          if (ai[i].name && std::string(ai[i].name) == "Dominic_Paine") {
            seen.dominic = ai[i].mech;
            seen.brain = static_cast<mech3::AiControls*>(ai[i].controls)->brain.get();
          }
        const mech3::ai::InOutArea* area = mech3::ai::inout().current;
        if (!seen.dominic || !seen.brain || !area) return;
        float best = 0;
        for (const auto& q : area->polygons) {
          if (!q->tested || q->inverted) continue;
          float size = 0;  // its area (the shoelace sum)
          for (size_t i = 0, j = q->verts.size() - 1; i < q->verts.size(); j = i++)
            size += q->verts[j].pos[0] * q->verts[i].pos[2] - q->verts[i].pos[0] * q->verts[j].pos[2];
          if (std::fabs(size) > best) best = std::fabs(size), seen.patch = q.get();
        }
        if (!seen.patch) return;
        // The enemy mechs taken 3 km off.
        for (int32_t i = 0; i < made; ++i) {
          if (ai[i].mech == seen.dominic) continue;
          mech3::LaunchedMech m;
          mech3::describe_mech(ai[i].mech, &m);
          const float x = m.pos[0] + 3000.0f, z = m.pos[2] + 3000.0f;
          const float at[3] = {x, ground_under(x, m.pos[1] + 500.0f, z), z};
          mech3::move_mech(ai[i].mech, at, 0.0f);
        }
        const float* lo = seen.patch->min;
        const float* hi = seen.patch->max;
        const float cx = (lo[0] + hi[0]) * 0.5f, cz = (lo[2] + hi[2]) * 0.5f;
        const float pat[3] = {cx, ground_under(cx, hi[1] + 200.0f, cz), cz};
        mech3::move_mech(mech3::mech_mission_player(), pat, 0.0f);
        seen.east = hi[0];
        const float dx = hi[0] + 230.0f;
        const float at[3] = {dx, ground_under(dx, hi[1] + 200.0f, cz), cz};
        mech3::move_mech(seen.dominic, at, -1.5707964f);  // facing west, the player
        seen.d0 = dx - cx;
        if (mech3::ai::Valve* w = mech3::ai::valve_find("dominic_wakes")) w->set(1);  // he starts powered down
        return;
      }
      if (!seen.brain || !seen.patch) return;
      mech3::LaunchedMech d;
      mech3::describe_mech(seen.dominic, &d);
      if (in_patch(d.pos)) {
        if (!seen.inside) seen.first_in = seen.frames;
        seen.last_in = seen.frames;
        ++seen.inside;
      }
      seen.west = std::min(seen.west, d.pos[0]);
      std::memcpy(seen.last, d.pos, 12);
    };
    CountingPump pump;
    pump.limit = 1500;  // 50 s
    app.run(pump);
    const float east_gap = seen.last[0] - seen.east;
    if (std::getenv("MW3_LANCE_LOG"))
      std::fprintf(stderr, "PATCH inside %d (frames %d..%d) of %d, d0 %g, west %g, last %g %g (east edge %g, gap %g)\n",
                   seen.inside, seen.first_in, seen.last_in, seen.frames, seen.d0, seen.west, seen.last[0], seen.last[2],
                   seen.east, east_gap);
    EXPECT(seen.patch && seen.frames > 1000 && std::fabs(seen.d0 - 496.0f) < 1.0f);
    // He walks at it, is pulled up 3 m short of the edge (the far point), runs
    // on 17 m into the patch as he slows, is sent back out to its pushed-out
    // outline (from inside: the nearest point) and stops there, 5.7 m east of
    // the patch (exact: the run repeats with reset_process_state's fixed seed).
    EXPECT(seen.inside == 287 && seen.first_in == 837 && seen.last_in == 1123);
    EXPECT(std::fabs(seen.west - 1004.9f) < 0.05f && std::fabs(east_gap - 5.72693f) < 1e-3f);
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
      // Exact, the run repeating (reset_process_state's fixed seed). The
      // player is across a no-go patch of the mission boundary from him: the
      // follow is pulled round its corner (ai.md section 11; 63.3 m without).
      EXPECT(seen.targeted == 764 && seen.on_enemy == 764 && seen.aiming == 326 && seen.firing == 375);
      EXPECT(seen.follows == 765 && seen.follows_player == 765 && seen.enemy_contacts == 765);
      EXPECT(std::fabs(seen.moved - 70.3761f) < 1e-3f && std::fabs(seen.closest - 60.0f) < 1e-3f);
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
      int markers = 0;  // the waypoint marker: the nav waynet's selected point (vehicles.md, The nav waynet)
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
        seen.markers += r.markers;
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
    EXPECT(seen.markers > 0);  // waynet_1's selected point on the radar
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
