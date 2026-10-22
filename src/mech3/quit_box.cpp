#include "mech3/quit_box.h"

#include "mech3/app_hooks.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
constexpr Handle kLobbyState = 0x609848;  // the multiplayer side's state the fade goes to (not ported)
constexpr float kFadeSeconds = 2.0f;      // [0x005e7274]
constexpr uint32_t kMsgQuitGame = 0xde1;  // the message
constexpr uint32_t kMsgTitle = 7;         // "MechWarrior 3"
}  // namespace

// Mech3.exe 1.2 @ 0x0048dfe0: OK and Cancel shown, the other buttons and the
// backdrop hidden, the texts by message id, the cursor's capture off.
QuitBox::QuitBox(Engine& e, const char* file, const char* name)
    : MessageBox(file, name, true), unused(e, -1), e_(e) {
  buttons[0].show_for(-1.0f);
  buttons[1].show_for(-1.0f);
  for (int i = 2; i < 8; ++i) buttons[i].hide();
  backdrop.hide();
  message.set_text_id(kMsgQuitGame);
  title.set_text_id(kMsgTitle);
  if (cursor) static_cast<ztwod::Cursor*>(cursor)->set_capture(0);
}

// Mech3.exe 1.2 @ 0x0048e390
void QuitBox::answer_ok() {
  e_.network_leave();
  e_.network_set(0);  // 0x0047fc40 on the app's network part ([0x00608c88])
  e_.fade_to_state(kLobbyState, 0, 1, 2, kFadeSeconds, 0, 0, 0);
  MessageBox::answer_ok();
  close();
}

// Mech3.exe 1.2 @ 0x0048e3d0
void QuitBox::answer_cancel() {
  MessageBox::answer_cancel();
  close();
}

// Mech3.exe 1.2 @ 0x0048e2d0: shown once; the command on mouse button 1
// unbound meanwhile (a click answers the box, not the mech).
void QuitBox::prompt() {
  if (active) return;
  set_active(1);
  mouse_cmd = e_.mouse_command(1);
  if (mouse_cmd) {
    mouse_button = e_.mouse_of(mouse_cmd);
    e_.bind_mouse(0, mouse_cmd);
  }
}

// Mech3.exe 1.2 @ 0x0048e320
void QuitBox::close() {
  if (!active) return;
  if (mouse_cmd) e_.bind_mouse(mouse_button, mouse_cmd);
  set_active(0);
}

}  // namespace mech3
