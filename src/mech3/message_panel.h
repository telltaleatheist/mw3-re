#pragma once
#include <cstdarg>
#include <cstdint>

#include "mech3/hud.h"

// The message panel (docs/hud.md section 18): the in-mission messages, a
// text box of four lines on a see-through panel that slides up from under
// the display's bottom edge while it has something to show, and back down
// when its time runs out. Original file unknown: the code sits between
// mech3\briefing.cpp and mech3\mech.cpp (0x0040f040..0x0040fed0).
namespace mech3 {

// Vtable 0x005bc7a0, 0x600 bytes.
class MessagePanel : public ztwod::TextBox {
 public:
  MessagePanel();                    // inlined in 0x0040f410
  ~MessagePanel() override = default;  // 0x0040f710
  void set_pos(int32_t px, int32_t py) override;  // +0x0c  0x0040f6b0: the panel 4 left, sized to the text
  void update(float dt) override;    // +0x24  0x0040f680: the panel, then the text
  void show_for(float seconds) override;  // +0x60  0x0040f5d0: a time slides it in (or out), else only shown
  void set_sliding(bool in);         // 0x0040fa90: out also clears the time
  void slide(float dt);              // 0x0040f8d0's first part: the time, then a step in or out
  float timer = -1.0f;               // +0x4c8 seconds left shown, -1 none
  float speed = 60.0f;               // +0x4cc pixels a second
  int32_t width = 0x140;             // +0x4d0
  uint8_t sliding_in = 0;            // +0x4d4
  int32_t on_x = 0, on_y = 0;        // +0x4d8 shown: centred, its height above the bottom
  int32_t off_x = 0, off_y = 0;      // +0x4e0 hidden: just under the bottom
  int32_t x = 0, y = 0;              // +0x4e8 where it is (0x0040fed0)
  HudPanel panel;                    // +0x4f0
  ztwod::Handle font = 0;            // +0x5f8 hud_messages
  ztwod::Handle font_bright = 0;     // +0x5fc hud_messages_bright
  float carry = 0.0f;                // not in the original: the step's fraction (FIX (B), slide)
};

// [0x00603d7c] and the calls on it.
MessagePanel* message_panel();
int32_t message_panel_create();      // 0x0040f410: made once, 1
void message_panel_destroy();        // 0x0040f7d0
void message_panel_reset();          // 0x0040f040: cleared, out, at its hidden place
void message_panel_toggle();         // 0x0040f070 (the mech command 0xa6)
void message_panel_set(bool in);     // 0x0040f0b0
void message_panel_clear();          // 0x0040f0a0
void message_panel_add_for(float seconds, const char* format, ...);  // 0x0040fe20
void message_panel_add(const char* format, ...);                     // 0x0040fe50
void message_panel_draw();           // 0x0040fb30: its quad while shown
void message_panel_point(int32_t out[2]);  // 0x0040fed0

}  // namespace mech3
