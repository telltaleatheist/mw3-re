// The mech lab's state: the way in from the briefing, the frame and ACCEPT
// (docs/mechlab.md section 1). Original file unknown: the lab's code sits
// between mech_data.cpp and mech_map.cpp (0x00448830..0x004561ff).
#include "mech3/app.h"
#include "mech3/app_states.h"
#include "mech3/lab_screen.h"

#include <cstdio>

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
  mech_ = kNoMech;
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

// Mech3.exe 1.2 @ 0x00448a80. A mech chosen for the slot (+0x38, set by
// 0x00449010) brings its model to the 3D view and redraws the page. Two developer keys work only with the flag clear (from the briefing):
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
  if (mech_ != kNoMech) {
    const int32_t i = model_index(mech_);
    model_ = model_valid(i) ? models_[static_cast<size_t>(i)] : 0;
    en.lab_page_part(lab_screen_, model_);
    mech_ = kNoMech;
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
        // 0x00448c1d..: a taken name gets a random number (CString "%s%d"
        // with rand()); then it goes at the end of the shown mech's list.
        if (en.lab_list_find(list, en.lab_config_name(read).c_str())) {
          char b[300];
          std::snprintf(b, sizeof b, "%s%d", en.lab_config_name(read).c_str(), static_cast<int>(en.rand()));
          en.lab_config_set_name(read, b);
        }
        en.lab_list_append(list, read);
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
  en.pop_state(4);
  return true;
}

// Mech3.exe 1.2 @ 0x00448f80: another slot shown. The old one's mech goes
// back out of the lab's store, the new one's in; false for a slot the game
// does not have.
bool LabState::show_slot(int32_t slot) {
  Engine& en = e();
  const Handle old = en.lab_slot(pilot_, slot_);
  const Handle now = en.lab_slot(pilot_, slot);
  if (!now) return false;
  if (old == now) return true;
  if (old) en.lab_store_remove(store_, en.lab_slot_mech(old));
  slot_ = slot;
  const int32_t mech = en.lab_slot_mech(en.lab_slot(pilot_, slot));
  en.lab_store_add(store_, mech);
  set_mech(mech);
  if (LabScreen* s = en.lab_screen(lab_screen_)) s->show_slots(slot_);
  else en.lab_show_slots(lab_screen_, slot_);
  return true;
}

// Mech3.exe 1.2 @ 0x00449010: the shown slot's chassis. A new one drops the
// slot's loadout back into the store and shows on the slot's button and in
// CHASSIS_LIST; either way the 3D view shows it next frame (+0x38).
bool LabState::set_mech(int32_t mech) {
  Engine& en = e();
  en.lab_slot(pilot_, slot_);  // asked, unused
  if (en.lab_slot_mech(en.lab_slot(pilot_, slot_)) != mech) {
    en.lab_slot_drop_config(en.lab_slot(pilot_, slot_), store_);
    en.lab_slot_set_mech(en.lab_slot(pilot_, slot_), mech);
    // The exe calls the screen ([0x00605a58]) unchecked: it always has one
    // here; only the Lab State's own test (no screen) takes the guard.
    if (LabScreen* s = en.lab_screen(lab_screen_)) {
      s->mechs[slot_].set_mech(mech, true, &s->pilot_glow);
      s->chassis.select_mech(mech);
    }
  }
  mech_ = mech;
  return true;
}

// Mech3.exe 1.2 @ 0x00448e20
bool LabState::model_valid(int32_t i) const {
  return !models_.empty() && i >= 0 && static_cast<size_t>(i) < models_.size();
}

// Mech3.exe 1.2 @ 0x004507c0
int32_t LabState::model_index(int32_t mech) {
  Engine& en = e();
  for (size_t i = 0; i < models_.size(); ++i)
    if (static_cast<int32_t>(en.field32(models_[i], 0x08)) == mech) return static_cast<int32_t>(i);
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
