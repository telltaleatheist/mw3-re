// The mech lab's state: the way in from the briefing, the frame and ACCEPT
// (docs/mechlab.md section 1). Original file unknown: the lab's code sits
// between mech_data.cpp and mech_map.cpp (0x00448830..0x004561ff).
#include "mech3/app.h"
#include "mech3/app_states.h"

namespace mech3 {

// Mech3.exe 1.2 @ 0x00448830
bool mech_lab_open(Handle pilot, Handle store, uint8_t flag) {
  if (!pilot || !store) return false;
  LabState& lab = states().lab;
  lab.pilot_ = pilot;
  lab.store_ = store;
  lab.flag_ = flag;
  app().push(&lab, 0);
  app().engine().input_reset();
  return true;
}

// Mech3.exe 1.2 @ 0x00448930 (deleting 0x00448910): the parts' vector freed,
// then the shell base's.
LabState::~LabState() = default;

// Mech3.exe 1.2 @ 0x00448960. FastMenu is off while the 3D world loads; the
// state's result is the load's.
bool LabState::enter() {
  Engine& en = e();
  if (!pilot_) return false;
  if (!a().no_voices()) {
    en.voice_start("MECHLAB");
    sounds_[0] = en.sound_find("mechlab_select");
    sounds_[1] = en.sound_find("mechlab_move");
    sounds_[2] = en.sound_find("mechlab_rollover");
    sounds_[3] = en.sound_find("mechlab_error");
  }
  slots_ = en.lab_slot_count(pilot_);
  slot_ = -1;
  node_ = kNoNode;
  en.geometry_init("..\\data\\common\\images\\lab;zbd");
  const uint8_t fast = static_cast<uint8_t>(en.exchange_fast_menu(0));
  const bool loaded = en.lab_world_load(addr());
  make_screen();
  en.exchange_fast_menu(fast);
  return loaded;
}

// Mech3.exe 1.2 @ 0x00448a20
void LabState::leave() {
  Engine& en = e();
  en.voice_release("MECHLAB");
  for (Handle& s : sounds_) s = 0;
  en.lab_world_unload(addr());
  delete_screen();
  en.video_set_clear(clear_);
  pilot_ = 0;
  store_ = 0;
  en.input_reset();
  en.model_purge(1, 1);
}

// Mech3.exe 1.2 @ 0x00448a80. A part pointed at in the 3D view shows its
// page. Two developer keys work only with the flag clear (from the briefing):
// Ctrl+X writes the slot's loadout to a file, Ctrl+I reads one into its mech's
// list (Windows file dialogs; mechlab.md section 1). The result [0x00605a88]
// is never set, so the lab never leaves by its update.
bool LabState::update() {
  Engine& en = e();
  en.input_poll(0);
  if (!a().fullscreen() && (a().input_mode_ & 0xc) == 8) {
    int32_t x = 0, y = 0;
    en.cursor_pos(&x, &y);
    en.mouse_set_pos(x, y);
  }
  en.node_frame_update();
  en.lab_view_begin(addr());
  en.lab_view_end(addr());
  if (node_ != kNoNode) {
    const int32_t i = part_index(node_);
    part_ = part_valid(i) ? parts_[static_cast<size_t>(i)] : 0;
    en.lab_page_part(lab_screen_, part_);
    node_ = kNoNode;
  }
  draw(en.frame_dt());
  en.lab_world_frame(addr(), en.frame_dt());
  en.video_flip(0, 0, 1, en.video_mode() == 0 ? 1 : 0);
  const Handle config = en.lab_slot_config(pilot_, slot_);
  if (config && !flag_) {
    if (en.key_state(0x22d) == 1) {
      if (en.video_mode() != 0) {
        en.video_begin_3d();
        en.video_end_3d();
      }
      en.config_export(config);
    } else if (en.key_state(0x217) == 1) {
      if (en.video_mode() != 0) {
        en.video_begin_3d();
        en.video_end_3d();
      }
      const Handle read = en.config_import();
      const Handle dfn = en.mech_dfn_handle(en.config_mech(config));
      const Handle list = dfn ? en.field32(dfn, 0x20) : 0;
      if (read && list) {
        en.config_list_add_import(list, read);
        if (lab_screen_) en.lab_page_refresh(lab_screen_);
      }
    }
  }
  return result_ != 0;
}

// Mech3.exe 1.2 @ 0x00448870: the slots' +0x0c cleared, then back to the
// briefing with 4 (LoadState::resume refreshes it).
bool LabState::accept() {
  Engine& en = e();
  for (int32_t i = 0; i < slots_; ++i) en.lab_slot_release(pilot_, i);
  a().pop(4);
  return true;
}

// Mech3.exe 1.2 @ 0x00448e20
bool LabState::part_valid(int32_t i) const {
  return !parts_.empty() && i >= 0 && static_cast<size_t>(i) < parts_.size();
}

// Mech3.exe 1.2 @ 0x004507c0
int32_t LabState::part_index(Handle node) {
  Engine& en = e();
  for (size_t i = 0; i < parts_.size(); ++i)
    if (en.field32(parts_[i], 0x08) == node) return static_cast<int32_t>(i);
  return -1;
}

// Mech3.exe 1.2 @ 0x00448e70: the screen (new(0x1b468), 0x0044b920 with the
// slot count), the first slot and the first page.
void LabState::make_screen() {
  Engine& en = e();
  lab_screen_ = en.construct(0x44b920, {slots_});
  en.lab_show_slot(addr(), 0);
  en.lab_page(lab_screen_, 0);
}

// Mech3.exe 1.2 @ 0x00448ef0
void LabState::delete_screen() {
  if (lab_screen_) e().vcall(lab_screen_, 0x08, {1});
  lab_screen_ = 0;
}

// Mech3.exe 1.2 @ 0x00448f10: the screen drawn, under the 3D view's lock
// when the world is up. (0x004bf620 and 0x005ac5c0 on the screen are empty.)
void LabState::draw(float dt) {
  Engine& en = e();
  if (world_) {
    en.video_begin_3d();
    en.render_flush(en.video_screen_rect());
  } else {
    en.video_lock();
  }
  en.vcall(lab_screen_, 0x00, {zgame::bits(dt)});
  if (world_) en.video_end_3d();
  else en.video_unlock();
}

}  // namespace mech3
