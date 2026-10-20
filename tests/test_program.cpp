// The program's pieces that run without a window (docs/program.md): the
// message DLL, the mounts and write dir, the music lookup, and the start-up
// through InitInstance and the original main loop to the main menu's state.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zsound/zsnd_cd.h"
#include "gamez/zutil/zutl_msg.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_reg.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "mech3/controls_menu.h"
#include "mech3/mainmenu.h"
#include "mech3/mech_control.h"
#include "mech3/mission.h"
#include "mech3/options.h"
#include "mech3/pilot.h"
#include "mech3/pilot_menu.h"
#include "mech3/player.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_steps.h"
#include "mech3/sensors.h"
#include "mech3/app_sensors.h"
#include "mech3/zai_target.h"
#include "mech3/zrdrvalve.h"
#include "platform/reg_file.h"
#include "platform/silent_sound.h"

namespace {

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
  bool peek() override { return ++passes > limit; }
  bool pump() override { return false; }
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
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
  mech3::options() = mech3::Options{};
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
    EXPECT(count("0x004161c0") == 1);  // its placement is not ported
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
          EXPECT(o.owner == b && o.net == &o.net_part && o.state == &o.state_part);
          EXPECT(o.net->net && o.net->net->get_id() == 4 && !o.net->builtin);
          EXPECT(o.net->order == &o && o.state->order == &o);
        }
        EXPECT(b->order.owner == b);
        EXPECT(b->firing.brain == b && b->firing.aim && b->firing.x00[0] == 8);
        EXPECT(b->radio == nullptr);  // owens_01 has no sound_tag
        EXPECT(b->step.next == mech3::ai::null_step());
        EXPECT(b->steer.x34 == 20.0f && b->steer.x38 > 0.7071f && b->steer.x38 < 0.7072f);
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
  uint32_t player_ticks() const {
    const auto& n = engine->unported_counts();
    return n.count("0x0041e3f0") ? n.at("0x0041e3f0") : 0;  // a call out of player_tick (0x004a1160)
  }
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
  mech3::missions() = mech3::Missions{};  // as a new process starts: no mission up
  mech3::mech_types_forget();             // and no mech type cached
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
    app.run(pump);
    EXPECT(seen.frames > 200);
    EXPECT(seen.start >= 0 && seen.woke);
    EXPECT(seen.time_steps);
    EXPECT(mech3::mech_game_time() == mech3::missions().time);  // the ports' copy of [0x0060991c]
    // Each fired on the frame its time was first past, not before.
    EXPECT(seen.truck > seen.start + 18.0f && !(seen.truck_before > seen.start + 18.0f));
    EXPECT(seen.dialogue > seen.start + 22.0f && !(seen.dialogue_before > seen.start + 22.0f));
    EXPECT(mech3::ai::valve_timers() == nullptr);  // both done, the list gone
    // The cleanup: the valves end, the game time goes back to 0.
    EXPECT(mech3::missions().loaded == 1);
    EXPECT(mech3::missions().cleanup(false));
    EXPECT(valve_find("bramp") == nullptr && mech3::missions().time == 0.0f);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
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
    void overlay_16(const uint16_t* px, int, int, int, bool, uint16_t k) override {
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

TEST(mission_c1m1_sensors_fill_the_contacts_headless) {
  if (!have_game()) return;
  SavedMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3_GAME_DIR);
  zutl::set_write_dir(temp_dir("mission_sensors_write"));
  mech3::reset_states();
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
    } seen;
    mech3::Handle player = 0, owens = 0;
    int fade_frames = 0;  // the mission start's fade in (0x0048eea0) runs first
    engine.on_present = [&](int) {
      if (app.current() == &mech3::states().fade && seen.frames == 0) ++fade_frames;
      if (app.current() != &mech3::states().core) return;
      ++seen.frames;
      if (seen.frames == 3) {  // the Owens powered down: sensed, not in sight
        player = engine.field32(engine.pilot(), 0x14);
        int32_t made = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
        owens = made == 1 ? engine.handle_of(ai[0].mech) : 0;
        const std::vector<mech3::Handle>& list = mech3::ai::entity_list();
        seen.entities = list.size();
        seen.player_first = !list.empty() && list[0] == player;
        seen.sorted = mech3::player_get_60b56c() == 1;
        mech3::SensedContact c[64];
        seen.player_n = mech3::sensed_contacts(engine.object_of(player), c, 64);
        for (int32_t i = 0; i < seen.player_n && i < 64; ++i) {
          if (i > 0 && c[i].distance < c[i - 1].distance) seen.nearest_first = false;
          if (c[i].seen == owens) seen.player_sees_owens = true, seen.owens_hidden = !c[i].in_sight;
        }
        if (owens) {
          seen.owens_n = mech3::sensed_contacts(ai[0].mech, c, 64);
          for (int32_t i = 0; i < seen.owens_n && i < 64; ++i) seen.owens_sees_player |= c[i].seen == player;
        }
        if (mech3::ai::Valve* b = mech3::ai::valve_find("bramp")) b->set(1);  // wakes the Owens
      } else if (seen.frames == 5) {  // awake: in sight, and Ctrl+E (0x004d6a80) picks it
        mech3::SensedContact c[64];
        const int32_t n = mech3::sensed_contacts(engine.object_of(player), c, 64);
        for (int32_t i = 0; i < n && i < 64; ++i) seen.owens_in_sight |= c[i].seen == owens && c[i].in_sight;
        if (mech3::ai::AiTarget* t = mech3::ai::sensor_hooks().fields(player)) {
          t->nearest_enemy(0);
          const int32_t k = mech3::selected_contact(engine.object_of(player));
          seen.picked = k >= 0 && k < n && c[k].seen == owens;
        }
      } else if (seen.frames == 6) {  // the pick holds over a frame: the sensors find it again
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
    EXPECT(seen.frames >= 6);
    EXPECT(seen.entities == 2 && seen.player_first && seen.sorted);
    EXPECT(seen.player_sees_owens && seen.owens_hidden && seen.owens_sees_player && seen.nearest_first);
    EXPECT(seen.owens_in_sight && seen.picked && seen.kept);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
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
  mech3::options().sound_volume = 0.25f;
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
