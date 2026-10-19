// zTwoD's message box and its modal loop (docs/twod.md, The message box).
// Original file unknown: after the line primitives, 0x005a6e60..0x005a7730.
#include "gamez/zimage/zimg_library.h"
#include "gamez/ztwod/twod.h"

namespace ztwod {

// Mech3.exe 1.2 @ 0x005a7650 .. 0x005a7730 (one per button)
void MbButton::on_left_up() {
  auto* box = static_cast<MessageBox*>(dialog);
  switch (slot) {
    case 0: box->answer_ok(); break;
    case 1: box->answer_cancel(); break;
    case 2: box->answer_yes(); break;
    case 3: box->answer_no(); break;
    case 4: box->answer_abort(); break;
    case 5: box->answer_retry(); break;
    case 6: box->answer_ignore(); break;
    default: box->answer_user(); break;
  }
  Control::on_left_up();
}

// Mech3.exe 1.2 @ 0x005a6e60
MessageBox::MessageBox(const char* file, const char* name, bool no_draw) {
  static const char* const kButtons[8] = {"MB_OK", "MB_CANCEL", "MB_YES", "MB_NO",
                                          "MB_ABORT", "MB_RETRY", "MB_IGNORE", "MB_USERBTN"};
  for (int i = 0; i < 8; ++i) buttons[i].slot = i;
  screen = host().screen_rect();
  if (!no_draw) {
    grab = host().screen_grab(2);
    backdrop.set_image(grab);
    backdrop.set_pos(0, 0);
    add(&backdrop);
  }
  if (!file || !name) return;
  if (load(file, name, no_draw)) {
    for (int i = 0; i < 8; ++i) bind_button(&buttons[i], kButtons[i]);
    bind_primitive(&title, "TITLE");
    bind_primitive(&message, "MESSAGE");
    unload();
  }
  title.show_for(-1.0f);
  message.show_for(-1.0f);
  set_flags_all(0);
}

// Mech3.exe 1.2 @ 0x005a7150
MessageBox::~MessageBox() {
  if (grab) {
    if (grab->pixels) host().crt_free(grab->pixels);
    grab->pixels = nullptr;
    host().texture_free(grab);
    grab = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x005a7280
// Its own frames (input, clock, lock, update, unlock, flip) into the whole
// screen until a button answers or 100,000 frames pass; the cursor, the page
// flipping (set from the old cursor state, as written) and the render target
// are put back after.
int32_t MessageBox::run(const char* text, const char* caption, uint32_t style) {
  Host& h = host();
  if (h.video_mode() != 0) h.video_double();
  const uint32_t old_cursor = h.show_cursor(0);
  set_page_flip(false);
  Rect old{};
  int32_t depth = 0, pitch = 0;
  const Handle old_base = h.render_target(&old.right, &old.bottom, &depth, &pitch);
  if (h.video_mode() != 0) {
    const Handle p = h.video_get(0x576db0);
    const Handle d = h.video_get(0x576c70);
    const Handle t = h.video_get(0x576d80);
    h.render_set_target(t, &screen, d, p);
  } else {
    const Handle p = h.video_get(0x576cd0);
    const Handle d = h.video_get(0x576c70);
    const Handle t = h.video_get(0x576ca0);
    h.render_set_target(t, &screen, d, p);
  }
  result = 0;
  frames = 100000;
  set_active(1);
  message.set_text(text);
  title.set_text(caption);
  for (MbButton& b : buttons) b.hide();
  enum { kOk, kCancel, kYes, kNo, kAbort, kRetry, kIgnore };
  switch (style & 0xf) {
    case 0:
      buttons[kOk].show_for(-1.0f);
      break;
    case 1:
      buttons[kOk].show_for(-1.0f);
      buttons[kCancel].show_for(-1.0f);
      break;
    case 2:
      buttons[kAbort].show_for(-1.0f);
      buttons[kRetry].show_for(-1.0f);
      buttons[kIgnore].show_for(-1.0f);
      break;
    case 3:
      buttons[kCancel].show_for(-1.0f);
      buttons[kYes].show_for(-1.0f);
      buttons[kNo].show_for(-1.0f);
      break;
    case 4:
      buttons[kYes].show_for(-1.0f);
      buttons[kNo].show_for(-1.0f);
      break;
    case 5:
      buttons[kRetry].show_for(-1.0f);
      buttons[kCancel].show_for(-1.0f);
      break;
    default:
      break;
  }
  while (frames-- > 0) {
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
  set_page_flip(old_cursor != 0);
  h.render_set_target(old_base, &old, depth, pitch);
  return result;
}

}  // namespace ztwod
