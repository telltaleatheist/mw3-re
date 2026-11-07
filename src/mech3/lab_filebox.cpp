#include "mech3/lab_filebox.h"

#include "gamez/ztwod/twod.h"

namespace mech3 {

// Pirate's Moon 1.0 @ 0x00490270
LabFileBox::LabFileBox(Engine& e, const char* dir, const char* filter, std::string* name, bool load)
    : MenuScreen(e, 0), accept_(e, true), cancel_(e, false), bar_(e), files_(e), load_(load), name_(name),
      pattern_(std::string(dir) + "\\" + filter) {
  if (ztwod::Dialog::load("dlgLoadSave.zrd", "LOADSAVE_DIALOG", true)) {
    add(&backdrop_);
    bind_primitive(&box_, "LOAD_BOX");
    bind_button(&accept_, "BTN_ACCEPT");
    bind_button(&cancel_, "BTN_CANCEL");
    bind_button(&bar_, "EDITBAR");
    bind_button(&files_, "FILELIST");
    unload();
  }
  set_flags_all(0);
}

// Pirate's Moon 1.0 @ 0x00490710, 0x00490750
void LabFileBox::answer(bool accept) {
  result_ = accept;
  done_ = true;
  if (accept) *name_ = bar_.get_text();
}

// Pirate's Moon 1.0 @ 0x00487d80 on these two
void LabFileBox::Button::on_left_up() {
  if (!enabled) return;
  SlideButton::on_left_up();
  if (auto* s = dynamic_cast<LabFileBox*>(dialog)) s->answer(accept_);
}

// Pirate's Moon 1.0 @ 0x0048fe00: 26 bytes (25 characters).
LabFileBox::NameBar::NameBar(Engine& e) : ShellEditBar(e) {
  editor.set_reject(":|\\<>/?\"*");
  set_size(0x1a);
  set_focus(0);
}

// Pirate's Moon 1.0 @ 0x0048feb0, 0x0048fed0
void LabFileBox::NameBar::on_return() {
  set_focus(0);
  if (auto* s = dynamic_cast<LabFileBox*>(dialog)) s->answer(true);
}
void LabFileBox::NameBar::on_cancel() {
  set_focus(0);
  if (auto* s = dynamic_cast<LabFileBox*>(dialog)) s->answer(false);
}

// Pirate's Moon 1.0 @ 0x00490760
void LabFileBox::Files::select(int32_t index) {
  FileList::select(index);
  auto* s = dynamic_cast<LabFileBox*>(owner);
  if (!s) return;
  s->bar_.set_text(index >= 0 && index < item_count() ? items[static_cast<size_t>(index)]->text : nullptr);
}

// Pirate's Moon 1.0 @ 0x00490570: on saving the bar has the default name; on
// loading the first file is chosen, which fills the bar.
void LabFileBox::fill() {
  if (!load_ && !name_->empty()) bar_.set_text(name_->c_str());
  files_.clear_items();
  files_.layout();
  int32_t n = 0;
  for (const std::string& file : engine().list_files(pattern_.c_str())) {
    const size_t slash = file.find_last_of("\\/"), dot = file.rfind('.');
    const size_t from = slash == std::string::npos ? 0 : slash + 1;
    files_.add_item(file.substr(from, dot == std::string::npos || dot < from ? std::string::npos : dot - from).c_str(), n);
    ++n;
  }
  if (load_ && n > 0) files_.select_index(0);
}

// Pirate's Moon 1.0 @ 0x00490480
void LabFileBox::prepare() {
  ztwod::Host& h = ztwod::host();
  grab_ = h.screen_grab(2);
  if (grab_) backdrop_.set_image(grab_);
  fill();
  engine().input_clear();
  bar_.set_focus(1);
}

// Pirate's Moon 1.0 @ 0x004904f0
void LabFileBox::finish() {
  if (grab_) {
    ztwod::host().texture_free(grab_);
    grab_ = nullptr;
    backdrop_.set_image(static_cast<zimg::Texture*>(nullptr));
  }
  engine().input_clear();
}

// Pirate's Moon 1.0 @ 0x004907c0: the frames as MessageBox::run's.
bool LabFileBox::run() {
  ztwod::Host& h = ztwod::host();
  if (h.video_mode() != 0) h.video_double();
  const uint32_t old_cursor = h.show_cursor(0);
  ztwod::set_page_flip(false);
  ztwod::Rect old{}, screen = h.screen_rect();
  int32_t depth = 0, pitch = 0;
  const ztwod::Handle old_base = h.render_target(&old.right, &old.bottom, &depth, &pitch);
  if (h.video_mode() != 0)
    h.render_set_target(h.video_get(0x576d80), &screen, h.video_get(0x576c70), h.video_get(0x576db0));
  else
    h.render_set_target(h.video_get(0x576ca0), &screen, h.video_get(0x576c70), h.video_get(0x576cd0));
  prepare();
  set_active(1);
  result_ = done_ = false;
  while (!done_) {
    h.input_poll(0);
    h.timer_update();
    h.video_lock();
    update(h.frame_dt());
    h.video_unlock();
    h.video_flip(&screen, &screen, 1, 1);
  }
  restore_behind();
  set_active(0);
  h.show_cursor(old_cursor);
  ztwod::set_page_flip(old_cursor != 0);
  h.render_set_target(old_base, &old, depth, pitch);
  finish();
  return result_;
}

}  // namespace mech3
