#include "mech3/credits.h"

#include <cstring>

#include "crt/msvc_math.h"
#include "gamez/zreader/zreader.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

// ---- the scrolling names ----

// Mech3.exe 1.2 @ 0x004290c0
void CreditsScroll::update(float dt) {
  Object2D::update(dt);
  for (CreditSection& s : sections)
    for (CreditLine& l : s.lines) l.text.update(dt);
}

// Mech3.exe 1.2 @ 0x00429200: RECT ((left top) (right bottom)) from the
// control's place; each SCROLLING_TEXT entry a list of lines (("key" ...)
// "offset" (dx dy) "font" ("name")), the keys' messages run together. A
// section is as tall as its tallest line; the sections stack.
bool CreditsScroll::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  Control::read_control(node, dialog);
  if (zrdr::Node* r = zrdr::find(node, "RECT")) {
    const zrdr::Node* a = r->list[1].list;
    const zrdr::Node* b = r->list[2].list;
    left = a[1].i + base_x;
    top = a[2].i + base_y;
    right = b[1].i + base_x;
    bottom = b[2].i + base_y;
  }
  if (zrdr::Node* r = zrdr::find(node, "SCROLL_RATE")) rate = r->f;
  zrdr::Node* st = zrdr::find(node, "SCROLLING_TEXT");
  if (!st) return true;
  CreditSection work;
  int32_t off[2] = {0, 0};  // the line's offset, on the stack across lines
  const int32_t n = st->list[0].i;
  for (int32_t i = 1; i < n; ++i) {
    zrdr::Node* sec = st->list[i].list;
    const int32_t m = sec[0].i;
    work.lines.clear();
    for (int32_t j = 1; j < m; ++j) {
      CreditLine tmp;
      char text[256] = {};
      const zrdr::Node* keys = sec[j].list[1].list;
      for (int32_t k = 1; k < keys[0].i; ++k) {
        // strcat in the original, unbounded into a 256-byte buffer
        const char* s = ztwod::host().message(keys[k].s);
        std::strncat(text, s ? s : "", sizeof text - 1 - std::strlen(text));
      }
      tmp.text.set_text("%s", text);
      // A line without a two-number "offset" keeps the one before: the
      // original reads it into the same stack slot each time (docs/menus.md).
      zrdr::get_ivec2(&sec[j], "offset", off);
      tmp.dx = off[0];
      tmp.dy = off[1];
      if (const char* f = zrdr::get_string(&sec[j], "font")) tmp.text.set_font(ztwod::host().font_find(f));
      work.lines.push_back(tmp);
    }
    sections.push_back(work);
  }
  total = 0;
  for (CreditSection& s : sections) {
    int32_t tallest = 0;
    for (CreditLine& l : s.lines) {
      const int32_t h = l.text.text_height() + l.dy;
      if (h > tallest) tallest = h;
    }
    for (CreditLine& l : s.lines) l.dy += total;
    total += tallest;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x00429120: the lines' offset goes from `bottom` (t = 0) to
// `top - total` (t = 1); a line shows only wholly inside the rectangle.
void CreditsScroll::scroll(float t) {
  const float a = static_cast<float>(top - total) * t;
  const float b = (1.0f - t) * static_cast<float>(bottom);
  const int32_t y = crt::msvc_ftol(a + b);
  for (CreditSection& s : sections) {
    for (CreditLine& l : s.lines) {
      const int32_t iy = l.dy + y;
      l.text.set_pos(l.dx + left, iy);
      if (iy > top && l.text.text_height() + iy < bottom) l.text.show_for(-1.0f);
      else l.text.hide();
    }
  }
}

// ---- the screen ----

// Mech3.exe 1.2 @ 0x00428c60
void CreditsScreen::BackButton::on_left_up() {
  if (auto* s = dynamic_cast<MenuScreen*>(owner)) s->state_escape();
}

// Mech3.exe 1.2 @ 0x004289a0
CreditsScreen::CreditsScreen(Engine& e, Handle s) : MenuScreen(e, s), cancel(e) {
  const bool moving = !e.fast_menu();
  if (load("dlgCredits.zrd", "CREDITSDIALOG", moving)) {
    borrow_background(false);
    bind_primitive(&lines, "CREDIT_LINES");
    bind_primitive(&title, "CREDIT_TEXT");
    if (!moving) {
      bake();
      remove(&background);
      remove(&lines);
      remove(&title);
    }
    bind_button(&cancel, "CANCEL");
    bind_button(&names, "CREDITS_SCREEN");
  }
  unload();
  set_flags_all(0);
  set_active(1);
}

// Mech3.exe 1.2 @ 0x00429030: a whole pass takes SCROLL_RATE seconds a
// section, then starts again from the bottom.
void CreditsScreen::update(float dt) {
  clock = dt + clock;
  const float n = static_cast<float>(static_cast<int32_t>(names.sections.size()));
  float t = clock / (n * names.rate);
  if (t > 1.0f) {
    clock = 0.0f;
    t = 0.0f;
  }
  names.scroll(t);
  Window::update(dt);
}

}  // namespace mech3
