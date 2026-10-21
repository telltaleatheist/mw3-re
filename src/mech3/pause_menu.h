#pragma once
#include <cstdint>

#include "mech3/menu_widgets.h"

// The pause screen (docs/menus.md, The pause screen): dlgPause.zrd's
// PAUSEDIALOG over a grab of the mission's last frame. Escape in a mission
// pushes the Pause State (0x00493040), which makes it. Original file unknown:
// 0x00492750..0x00493030, in mech3\mission.cpp's neighbourhood (between
// 0x00491780 and the Pause State).
namespace mech3 {

// The screen (vtable 0x005c6110), 0x11b20 bytes.
class PauseScreen : public MenuScreen {
 public:
  enum Id : int32_t { kResume = 0, kQuit = 1, kOptions = 2, kReplay = 3 };

  PauseScreen(Engine& e, Handle state);  // 0x00492750
  ~PauseScreen() override;               // 0x00492ae0
  void on_button(int32_t id, int32_t a, int32_t b) override;  // +0x14  0x00492d70

  SlideButton resume;                    // +0x111ec  RESUME
  SlideButton quit;                      // +0x113d8  QUIT
  SlideButton options;                   // +0x115c4  OPTIONS
  SlideButton replay;                    // +0x117b0  REPLAY (campaign and Instant Action only)
  Marquee title;                         // +0x1199c  PAUSETEXT
  ztwod::Image backdrop{0};              // +0x11a5c  the grab of the frame behind
  Handle sounds = 0;                     // +0x11b18  the sounds paused (0x0056eb10)
  Handle groups = 0;                     // +0x11b1c  the sound groups taken away (0x005732f0)
};

}  // namespace mech3
