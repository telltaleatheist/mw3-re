// Pirate's Moon's chat panel line (0x0041f1f0, the port's ChatPanel::line_entered with the table's
// chat_resets_input): after the new line the input is reset (0x005349d0, zin::reset: the keyboard's buffer read
// and its states cleared), so keys held while typing are let go; MW3's 0x0041e660 leaves the input alone.
#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zinput/zinput.h"
#include "mech3/chat_panel.h"
#include "pm/pm_game.h"
#include "platform/platform.h"

namespace {
struct Keys : platform::Input {
  int reads = 0;
  bool open(Device) override { return true; }
  void close(Device) override {}
  int32_t acquire(Device) override { return 0; }
  int32_t unacquire(Device) override { return 0; }
  int32_t keyboard_data(platform::KeyEvent*, uint32_t* count) override {
    ++reads;
    *count = 0;
    return 0;
  }
  int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
  int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
};
}  // namespace

TEST(pm_chat_line_resets_the_input) {
  for (const bool pm : {true, false}) {
    zgame::set_game(pm ? pm::table() : zgame::mw3_table());
    Keys keys;
    zin::reset_state();
    zin::state().di = &keys;
    zin::state().kbd_ready = 1;
    zin::state().keys[0x1e].state = 1;  // A held while typing
    {
      mech3::ChatPanel panel(true, nullptr);
      panel.line_entered();  // an empty line: nothing sent
    }
    std::printf("  %s: keyboard reads %d, A %u\n", pm ? "PM" : "MW3", keys.reads, zin::state().keys[0x1e].state);
    EXPECT(pm ? keys.reads == 1 : keys.reads == 0);
    EXPECT(zin::state().keys[0x1e].state == (pm ? 0u : 1u));  // let go on PM, still held on MW3
    zin::reset_state();
  }
  zgame::set_game(pm::table());
}
