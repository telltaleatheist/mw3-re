#include "mech3/pause_menu.h"

#include "mech3/app.h"
#include "mech3/pilot.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
void target_buffers(Engine& e) {  // 0x00558290 with 0x00576da0 / 0x00576d90
  const Handle a = e.video_back_buffer();
  const Handle b = e.video_buffer_da0();
  const Handle r = e.video_buffer_d90();
  e.render_set_target2(e.video_target(), r, b, a);
}
constexpr Handle kEndWinState = 0x6099a0;  // the missions object's +0xd0
}  // namespace

// Mech3.exe 1.2 @ 0x00492750: the sounds paused and the playing groups taken
// away before the frame behind is grabbed; no FastMenu bake.
PauseScreen::PauseScreen(Engine& e, Handle s)
    : MenuScreen(e, s), resume(e, kResume), quit(e, kQuit), options(e, kOptions), replay(e, kReplay), title(e) {
  sounds = e.sound_listener();
  e.sound_stop_all(sounds);
  groups = e.sound_take_playing();
  target_buffers(e);
  backdrop.set_pos(0, 0);
  backdrop.set_image(ztwod::host().screen_grab(1));
  add(&backdrop);
  if (load("dlgPause.zrd", "PAUSEDIALOG", false)) {
    bind_primitive(&title, "PAUSETEXT");
    bind_button(&quit, "QUIT");
    bind_button(&resume, "RESUME");
    bind_button(&options, "OPTIONS");
    if (e.game_kind() == kCampaign || e.game_kind() == kInstantAction) bind_button(&replay, "REPLAY");
    unload();
  }
  // [0x006099c4] is never written: OPTIONS always stays.
  if (e.peek8(0x6099c4)) {
    options.hide();
    options.enabled = 0;
    options.refresh();
  }
  set_flags_all(0);
  set_active(1);
}

// Mech3.exe 1.2 @ 0x00492ae0
PauseScreen::~PauseScreen() {
  if (zimg::Texture* grab = backdrop.image) {
    backdrop.set_image(static_cast<zimg::Texture*>(nullptr));
    ztwod::host().image_release(grab);
  }
}

// Mech3.exe 1.2 @ 0x00492d70: RESUME gives the sounds and groups back; QUIT
// drops them and leaves the mission by the game's kind; OPTIONS pushes the
// game options; REPLAY ends the mission as a failure (0x0048fa80), so its
// end screen offers the replay.
void PauseScreen::on_button(int32_t id, int32_t a, int32_t b) {
  (void)a, (void)b;
  Engine& e = engine();
  const int32_t kind = e.game_kind();
  switch (id) {
    case kResume:
      state_escape();
      e.sound_resume_all(sounds);
      e.sound_release_all(sounds);
      sounds = 0;
      e.sound_restore_playing(groups);
      groups = 0;
      return;
    case kQuit:
      break;
    case kOptions:
      e.poke32(0x609868, e.construct(0x438be0, {}));
      e.poke32(0x60986c, e.construct(0x43a640, {}));
      e.poke32(0x609870, e.construct(0x42d9d0, {}));
      e.push_state(e.peek32(0x609868), 0);
      return;
    case kReplay:
      e.pop_state(0);
      e.mission_abort();
      return;
    default:
      return;
  }
  e.sound_release_all(sounds);
  sounds = 0;
  e.sound_free_playing(groups);
  groups = 0;
  e.pop_state(0);
  if (kind == kCampaign) {
    e.campaign_merge_salvage();
    e.campaign_reset_lancemates();
  } else if (kind == kInstantAction) {
    e.fade_to_state(kEndWinState, 0, 1, 2, 2.0f, 0, 0, 0);  // [0x005e7274]: 2 s
    return;
  } else if (kind == kTraining) {
    e.set_reopen_training(1);
  }
  e.replace_state(2);
  e.mission_cleanup(1);
}

}  // namespace mech3
