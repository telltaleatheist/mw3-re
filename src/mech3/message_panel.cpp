#include "mech3/message_panel.h"

#include <algorithm>

#include "crt/msvc_math.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zutil/zfix.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
MessagePanel* g_panel = nullptr;  // [0x00603d7c]
}

MessagePanel* message_panel() { return g_panel; }

// Inlined in 0x0040f410: hidden, at its hidden place.
MessagePanel::MessagePanel() : TextBox(4) {
  Object2D::hide();  // 0x0059ff50
  font = ztwod::host().font_find("hud_messages");  // [0x005e15cc]
  font_bright = ztwod::host().font_find("hud_messages_bright");  // [0x005e15b8]
  TextBox::set_font(font);
  x = y = 0;
  TextBox::set_pos(0, 0);
  panel.set_pos(-4, 0);
  panel.set_size(width, total_height());
  panel.set_color(hud_green());
  panel.set_opacity(hud_opacity());
  HudHooks& h = hud_hooks();
  on_x = (h.display_w() - width) / 2;
  on_y = h.display_h() - total_height();
  off_x = on_x;
  off_y = h.display_h() + 1;
  x = off_x, y = off_y;
  TextBox::set_pos(off_x, off_y);
  panel.set_pos(off_x - 4, off_y);
  panel.set_size(width, total_height());
}

// Mech3.exe 1.2 @ 0x0040f6b0
void MessagePanel::set_pos(int32_t px, int32_t py) {
  x = px, y = py;
  TextBox::set_pos(px, py);
  panel.set_pos(px - 4, py);
  panel.set_size(width, total_height());
}

// Mech3.exe 1.2 @ 0x0040f680
void MessagePanel::update(float dt) {
  panel.update(dt);
  TextBox::update(dt);
}

// Mech3.exe 1.2 @ 0x0040f5d0: a time while hidden or sliding out flips the
// slide (in, with the time); while in, a longer time replaces a shorter.
void MessagePanel::show_for(float seconds) {
  if (seconds > 0.0f) {  // [0x005bc740]
    if ((flags & ztwod::kHidden) || !sliding_in) {
      timer = seconds;
      sliding_in = !sliding_in;
      if (!sliding_in) timer = -1.0f;
    } else if (timer > 0.0f && timer < seconds) {
      timer = seconds;
    }
  }
  Object2D::show_for(-1.0f);
}

// Mech3.exe 1.2 @ 0x0040fa90
void MessagePanel::set_sliding(bool in) {
  sliding_in = in;
  if (!in) timer = -1.0f;
}

// Mech3.exe 1.2 @ 0x0040f8d0 (its first part): `speed` pixels a second,
// truncated each frame, so above 60 fps the step is 0 and the panel never
// moves. FIX (B): the fraction carried to the next frame (zfix per_second).
void MessagePanel::slide(float dt) {
  if (timer > 0.0f) {
    timer -= dt;
    if (timer <= 0.0f) set_sliding(!sliding_in);
  }
  auto step = [&]() {
    if (!zfix::options().per_second) return crt::msvc_ftol(speed * dt);
    carry += speed * dt;
    const int32_t s = static_cast<int32_t>(carry);
    carry -= static_cast<float>(s);
    return s;
  };
  if (sliding_in) {
    show_for(-1.0f);
    panel.show_for(-1.0f);
    if (y <= on_y) return;
    y -= step();
  } else {
    if (y >= off_y) {
      if (!(flags & ztwod::kHidden)) clear();
      hide();
      panel.hide();  // 0x00409a90(0)
      return;
    }
    y += step();
  }
  if (y > off_y) y = off_y;
  else if (y < on_y) y = on_y;
  set_pos(x, y);
}

// Mech3.exe 1.2 @ 0x0040f410 (the program's set-up)
int32_t message_panel_create() {
  if (!g_panel) g_panel = new MessagePanel();
  return 1;
}

// Mech3.exe 1.2 @ 0x0040f7d0
void message_panel_destroy() {
  delete g_panel;
  g_panel = nullptr;
}

// Mech3.exe 1.2 @ 0x0040f0a0
void message_panel_clear() { g_panel->clear(); }

// Mech3.exe 1.2 @ 0x0040f0b0
void message_panel_set(bool in) { g_panel->set_sliding(in); }

PanelArea& panel_area() {
  static PanelArea a;
  return a;
}

// Mech3.exe 1.2 @ 0x0040f330. The exe makes the panel in the app's start
// (0x0040f410 from 0x00480570) and uses it unchecked; without one, nothing.
void message_panel_place(const int32_t* kept, const int32_t* area) {
  PanelArea& a = panel_area();
  if (kept) std::copy(kept, kept + 4, a.kept);
  if (area) std::copy(area, area + 4, a.area);
  a.w = a.area[2] - a.area[0];
  a.h = a.area[3] - a.area[1];
  a.centre_x = a.w / 2 + a.area[0];
  MessagePanel* p = g_panel;
  if (!p) return;
  HudHooks& h = hud_hooks();
  p->on_x = (h.display_w() - p->width) / 2;  // 0x0040d760
  const int32_t text = p->total_height();     // 0x005a25f0
  p->on_y = h.display_h() - text;              // 0x0040d770
  p->off_x = p->on_x;
  p->off_y = h.display_h() + 1;
  p->set_pos(p->off_x, p->off_y);              // vt+0x0c
}

// Mech3.exe 1.2 @ 0x0040f040
void message_panel_reset() {
  if (!g_panel) return;
  message_panel_clear();
  message_panel_set(false);
  g_panel->set_pos(g_panel->off_x, g_panel->off_y);
}

// Mech3.exe 1.2 @ 0x0040f070
void message_panel_toggle() { g_panel->set_sliding(!g_panel->sliding_in); }

// Mech3.exe 1.2 @ 0x0040fe20: shown for a time, then a line.
void message_panel_add_for(float seconds, const char* format, ...) {
  g_panel->show_for(seconds);
  va_list args;
  va_start(args, format);
  g_panel->set_text_v(format, args);
  va_end(args);
}

// Mech3.exe 1.2 @ 0x0040fe50
void message_panel_add(const char* format, ...) {
  va_list args;
  va_start(args, format);
  g_panel->set_text_v(format, args);
  va_end(args);
}

// Mech3.exe 1.2 @ 0x0040fb30 (none made: nothing, where the exe reads null)
void message_panel_draw() {
  if (g_panel && !(g_panel->flags & ztwod::kHidden)) g_panel->panel.draw_quad();
}

// Mech3.exe 1.2 @ 0x0040fed0 (none made: 0, 0, where the exe reads null)
void message_panel_point(int32_t out[2]) {
  out[0] = g_panel ? g_panel->x : 0;
  out[1] = g_panel ? g_panel->y : 0;
}

}  // namespace mech3
