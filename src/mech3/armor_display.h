#pragma once
#include <cstdint>
#include <functional>

#include "gamez/ztwod/twod.h"
#include "mech3/lance_armor.h"

// The armor display, kept apart from debriefing.h so the HUD's program side
// (app_hud.cpp) can use it without the engine's hooks (debriefing.cpp has
// the code: between the Win State and the Salvage State, 0x0047e520..).
namespace mech3 {

// The armor display (0x0047e840, vtable 0x005c3de8), 0x2fb8 bytes: a lance
// card's, from HTAL, and the HUD's MFD and TARGET's in their mode 3: a bar for each armor section, its standing part green
// over the lost red (the rear torso's dimmer), "H" "T" "A" "L" over the head,
// torso, arms and legs, a line under them (campaign.md section 3).
class LanceDisplay : public ztwod::Line2D {
 public:
  // A bar (0x290 bytes, vtable 0x005c3e68): two polygons (vtable 0x005c0c10).
  struct Bar {
    ztwod::Polygon full, lost;       // +0x000, +0x140
    int32_t full_h = 0, full_w = 0;  // +0x138, +0x13c (the polygons' own)
    int32_t lost_h = 0, lost_w = 0;  // +0x278, +0x27c
    int32_t height = 0, width = 0;   // +0x280, +0x284
    int32_t x = 0, y = 0;            // +0x288, +0x28c
    void place(int32_t px, int32_t py, int32_t w, int32_t h, bool dim);  // 0x0047e520
    void fill(float left);           // 0x0047e680: the standing part
    void update(float dt);           // +0x24  0x0047e810
  };

  // `message`: Mech3Msg.dll's text by id (the labels' "H" "T" "A" "L").
  LanceDisplay(const std::function<const char*(int32_t)>& message, int32_t px, int32_t py, int32_t height,
               int32_t width, int32_t space);  // 0x0047e840
  void place(int32_t x, int32_t y, int32_t h, int32_t w, int32_t sp);  // 0x0047eb60
  void update(float dt) override;    // +0x24  0x0047f0e0: the bars, the line, the labels
  void layout(const LanceArmor& a);  // 0x0047eba0
  void fill(const LanceArmor& a);    // 0x0047ef10

  Bar bars[11];                      // +0x4c  by armor section
  int32_t px = 0, py = 0;            // +0x1c7c
  int32_t left = 0, top = 0, right = 0, bottom = 0;  // +0x1c84
  int32_t height, x1c98 = 0, width, space;  // +0x1c94, +0x1c98, +0x1c9c, +0x1ca0
  int32_t most = 0;                  // +0x1ca4  the most armor seen (kept between vehicles)
  float scale = 0.0f;                // +0x1ca8  height / most
  ztwod::FontText labels[4]{{nullptr, 0, 0}, {nullptr, 0, 0}, {nullptr, 0, 0}, {nullptr, 0, 0}};  // +0x1cac
  ztwod::Line2D line{0, 0, 0, 0, 0};  // +0x2f6c
};

}  // namespace mech3
