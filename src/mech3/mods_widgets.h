#pragma once
#include <cstdint>
#include <string>

#include "mech3/menu_states.h"
#include "mech3/menu_widgets.h"

// Ours (docs/modding.md, The MODS screen): the MODS screen's own widgets and
// drawing pieces, apart from the screen (mods_menu.h) so the tick list
// (upgrades/mods/mods_tick_list.h) and Instant Action's MODS use them too.
namespace mech3 {

// The screen's pieces other screens of ours on its look use (upgrades/mods/
// ia_mods_menu.h): the render target, a text placed and said, and the pilot
// screen's lines with the pilots' box taken out and (`left_box`) a copy of the right box in its place.
void mods_screen_target(Engine& e);
void mods_text_put(ztwod::Text& t, const char* font, int32_t x, int32_t y);
void mods_text_say(ztwod::Text& t, const std::string& s);
zimg::Texture* mods_screen_lines(zimg::Texture* src, bool left_box = true);

// Ours (lead, 2026-11-10): ENTER and BACK hit where they are drawn, their
// ring and knob (the image's pixels of alpha 64 and up) and their label's own
// box. SlideButton::hit
// (0x004855f0) takes get_rect, which adds every label's height below the
// image and reaches to the label's end: past BACK's circle, over the tab row
// beside it, so a tab there also left the screen. The original's screens keep theirs.
class ModsSlideButton : public SlideButton {
 public:
  using SlideButton::SlideButton;
  int32_t hit(int32_t px, int32_t py) override;
};

class ModsList : public FileList {
 public:
  ModsList(Engine& e, int32_t id) : FileList(e, id) {}
  void select(int32_t index) override;  // and tells the screen (its id, the index)
  // Ours: an item whose value has kGreyRow is shown in the grey font (a
  // campaign's own mods in its tick list: on, not the player's to turn off).
  static constexpr int32_t kGreyRow = 0x40000000;
  void layout() override;
};

}  // namespace mech3
