#include "mech3/menu_widgets.h"

#include <cstdlib>
#include <cstring>

#include "gamez/zimage/zimg_library.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

using ztwod::Rect;

namespace {
int32_t width_of(const zimg::Texture* t) { return t ? t->width : 0; }
int32_t height_of(const zimg::Texture* t) { return t ? t->height : 0; }

// FIX (B): the menus move by a step each frame, so they ran faster the faster
// the machine drew. With zfix's per_second the step is taken once for every
// 30 fps frame that has passed (owed: what is left over); off, once a frame.
int32_t frames_due(float dt, float* owed) {
  if (!zfix::options().per_second) return 1;
  *owed += zfix::frame_steps(dt);
  const int32_t n = static_cast<int32_t>(*owed);
  *owed -= static_cast<float>(n);
  return n;
}
}  // namespace

// ---- the background ----

ScrollImage::ScrollImage(Engine& e) : Image(0), e_(e) {
  const int32_t h = e_.display_height();
  const int32_t w = e_.display_width();
  view = Rect{0, 0, w, h};
  moving = e_.fast_menu() ? 0 : 1;
}

// Mech3.exe 1.2 @ 0x0040a490
void ScrollImage::draw() {
  if (!moving) {
    Image::draw();
    return;
  }
  for (int i = 0; i < 4; ++i) {
    if (!piece[i]) continue;
    set_pos(piece_pos[i][0], piece_pos[i][1]);
    src = &piece_src[i];
    Image::draw();
    piece[i] = 0;
  }
}

// Mech3.exe 1.2 @ 0x0040a210: the view moves and wraps at the image's size;
// piece 0 is the view cut at the image's right and bottom edges, pieces 1 and 2
// fill the screen past them from the image's left and top, piece 3 the corner.
void ScrollImage::update(float dt) {
  if (moving) {
    const int32_t n = frames_due(dt, &steps_);
    view.left += step_x * n;
    view.right += step_x * n;
    view.bottom += step_y * n;
    view.top += step_y * n;
    if (view.left >= width_of(image)) {
      view.left -= width_of(image);
      view.right = e_.display_width() + view.left;
    }
    if (view.top >= height_of(image)) {
      view.top -= height_of(image);
      view.bottom = e_.display_height() + view.top;
    }
    piece[0] = 1;
    Rect& p0 = piece_src[0];
    p0 = view;
    // (a second test, right < right, follows each of these: never true)
    piece[1] = 0;
    if (p0.right > width_of(image)) {
      p0.right = width_of(image);
      piece[1] = 1;
    }
    piece[2] = 0;
    if (p0.bottom > height_of(image)) {
      p0.bottom = height_of(image);
      piece[2] = 1;
    }
    piece[3] = piece[1] && piece[2] ? 1 : 0;
    const int32_t w = p0.right - p0.left;
    const int32_t h = p0.bottom - p0.top;
    if (piece[1]) {
      piece_src[1] = Rect{0, p0.top, e_.display_width() - w, p0.bottom};
      piece_pos[1][0] = w;
      piece_pos[1][1] = 0;
    }
    if (piece[2]) {
      piece_src[2] = Rect{p0.left, 0, p0.right, e_.display_height() - h};
      piece_pos[2][0] = 0;
      piece_pos[2][1] = h;
    }
    if (piece[3]) {
      piece_src[3] = Rect{piece_src[1].left, piece_src[2].top, piece_src[1].right, piece_src[2].bottom};
      piece_pos[3][0] = w;
      piece_pos[3][1] = h;
    }
  }
  Object2D::update(dt);
}

// ---- the title ----

Marquee::Marquee(Engine& e) : Image(0), e_(e) {}

// Mech3.exe 1.2 @ 0x0042fb30
void Marquee::update(float dt) {
  if (!e_.fast_menu()) {
    int32_t step = static_cast<int32_t>(speed);
    if (zfix::options().per_second) {
      steps_ += speed * zfix::frame_steps(dt);
      step = static_cast<int32_t>(steps_);
      steps_ -= static_cast<float>(step);
    }
    const int32_t py = get_y();
    set_pos(get_x() - step, py);
    if (get_x() + width_of(image) <= 0) {
      const int32_t y2 = get_y();
      set_pos(e_.display_width(), y2);
    }
  }
  Object2D::update(dt);
}

// ---- the screen ----

// Mech3.exe 1.2 @ 0x0040a110
MenuScreen::MenuScreen(Engine& e, Handle s) : e_(e), state(s), background(e) {
  origin_x = 0;
  origin_y = 0;
}

// Mech3.exe 1.2 @ 0x0040a520
uint8_t MenuScreen::set_busy(uint8_t on) {
  const uint8_t old = busy_;
  busy_ = on;
  return old;
}

// Mech3.exe 1.2 @ 0x00486ed0 (called on the main menu, the option screens and the credits)
void MenuScreen::bake() {
  set_active(1);
  cursor_obj.hide();
  update(0.0f);
  cursor_obj.show_for(-1.0f);
  zimg::Texture* grab = ztwod::host().screen_grab(1);
  set_active(0);
  restore_behind();
  set_static_bg(grab);
  add(&still);
  still.set_image(grab);
}

void MenuScreen::borrow_background(bool option_screen) {
  zimg::Texture* img = nullptr;
  uint8_t moving = 0;
  const bool have = e_.main_menu_background(&img, &moving);
  if (option_screen && !have) {
    bind_primitive(&background, "BACKGROUND");
  } else {
    // FIX (bug): the credits read the main menu's fields without checking it is
    // there (unreachable: they open only from it); here no image, still.
    add(&background);
    background.set_image(img);
    background.moving = moving;
    if (!moving) {
      background.set_pos(0, 0);
      background.src = &background.view;
    }
  }
  if (option_screen && e_.mission_running()) {
    background.moving = 0;
    background.set_pos(0, 0);
    background.src = &background.view;
  }
}

// ---- the push button ----

namespace {
// The owner's +0x14 / +0x1c are the menu screen's (every ShellButton lives in one).
MenuScreen* screen_of(ztwod::Layer* owner) { return dynamic_cast<MenuScreen*>(owner); }
}  // namespace

// Mech3.exe 1.2 @ 0x0040a5d0
ShellButton::ShellButton(Engine& e, int32_t i) : id(i), e_(e) {}

// Mech3.exe 1.2 @ 0x0040a600
ShellButton::~ShellButton() {
  if (borrowed) {
    img_normal = set_image(static_cast<zimg::Texture*>(nullptr));
    img_activate = nullptr;
    img_disabled = nullptr;
    img_rollover = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x0040a6b0
Rect* ShellButton::get_rect() {
  if (reading && e_.fast_menu()) return nullptr;
  return Control::get_rect();
}

// Mech3.exe 1.2 @ 0x0042a3d0
void ShellButton::on_left_up() {
  if (!enabled) return;
  Control::on_left_up();
  notify(1, 0);
}

// Mech3.exe 1.2 @ 0x0040a720: no mouse while the screen is busy.
bool ShellButton::accepts(ztwod::Window*, bool) {
  if (!owner) return true;
  MenuScreen* s = screen_of(owner);
  return s ? !s->busy() : true;
}

// Mech3.exe 1.2 @ 0x0040a680
bool ShellButton::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  reading = 1;
  const bool r = Control::read_control(node, dialog);
  reading = 0;
  return r;
}

// Mech3.exe 1.2 @ 0x0040a6e0
void ShellButton::notify(int32_t a, int32_t b) {
  if (!owner) return;
  if (MenuScreen* s = screen_of(owner)) s->on_button(id, a, b);
}

// ---- the rippling button ----

// Mech3.exe 1.2 @ 0x00438d70
SlideButton::SlideButton(Engine& e, int32_t i) : ShellButton(e, i) {}

// Mech3.exe 1.2 @ 0x0042a450
SlideButton::~SlideButton() {
  if (stashed) ztwod::host().image_release(stashed);
  stashed = nullptr;
  for (ztwod::Object2D*& o : owned) {
    delete o;
    o = nullptr;
  }
  owned.clear();
}

// Mech3.exe 1.2 @ 0x0042a400
void SlideButton::set_pos(int32_t px, int32_t py) {
  Image::set_pos(px, py);
  after_bind();
}

// Mech3.exe 1.2 @ 0x00485650: the ring grows by `speed` a second up to
// MAXRADIUS while the ripple loses 0.3 a frame; it stops below 2.
void SlideButton::update(float dt) {
  if (rippling) dirty();
  Object2D::update(dt);
  if (!rippling) return;
  if (amplitude > 2.0f) {
    const float d = speed * dt;
    radius += static_cast<int32_t>(d);
    if (radius > max_radius) radius = max_radius;
    else if (radius < 0) radius = 0;
    // FIX (B): 0.3 for each 30 fps frame with zfix's per_second, not each frame.
    amplitude = amplitude - 0.3f * zfix::frame_steps(dt);
    const float q = d / wavelength;
    phase = phase - static_cast<float>(static_cast<double>(q) * -3.141592653589793);
  } else {
    rippling = 0;
  }
  ripple.amplitude = static_cast<int32_t>(amplitude);
  ripple.inner = static_cast<int32_t>(static_cast<float>(radius) - wavelength * 3.0f);
  ripple.outer = radius;
  ripple.phase = phase;
  ripple.wavelength = wavelength;
  ripple.clip = nullptr;
  ripple.draw_ripple();
}

// Mech3.exe 1.2 @ 0x00485590: a baked button's bounds are its normal image's.
Rect* SlideButton::get_rect() {
  if (reading) return nullptr;
  if (stashed) set_image(stashed);
  Rect* r = reading && e_.fast_menu() ? nullptr : Control::get_rect();
  if (stashed) set_image(static_cast<zimg::Texture*>(nullptr));
  return r;
}

// Mech3.exe 1.2 @ 0x00485570
void SlideButton::on_enter() {
  restart_ripple();
  Control::on_enter();
}

// Mech3.exe 1.2 @ 0x00485640
void SlideButton::on_leave() {
  rippling = 0;
  Control::on_leave();
}

// Mech3.exe 1.2 @ 0x004855f0: inside the bounds, edges included, hidden or not.
int32_t SlideButton::hit(int32_t px, int32_t py) {
  const Rect* r = get_rect();
  if (!r) return 0;
  return r->left <= px && px <= r->right && r->top <= py && py <= r->bottom ? 1 : 0;
}

void SlideButton::read_ripple(zrdr::Node* node) {
  reach_radius = 20.0f;
  max_radius = 200;
  zrdr::get_float(node, "RADIUS", &reach_radius);
  zrdr::get_int(node, "MAXRADIUS", &max_radius);
  reach_sq = reach_radius * reach_radius;
}

// Mech3.exe 1.2 @ 0x0042a1d0
bool SlideButton::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  reading = 1;
  const bool r = Control::read_control(node, dialog);
  reading = 0;
  read_ripple(node);
  return r;
}

// Mech3.exe 1.2 @ 0x0042a250: the ripple at the image's centre.
void SlideButton::after_bind() {
  const int32_t w = width_of(image), h = height_of(image);
  reach.top = static_cast<int32_t>(static_cast<float>(get_y() + h / 2) - reach_radius);
  reach.left = static_cast<int32_t>(static_cast<float>(get_x() + w / 2) - reach_radius);
  reach.bottom = static_cast<int32_t>(static_cast<float>(get_y() + h / 2) + reach_radius);
  reach.right = static_cast<int32_t>(static_cast<float>(get_x() + w / 2) + reach_radius);
  const float cy = static_cast<float>(get_y() + h / 2);
  const float cx = static_cast<float>(get_x() + w / 2);
  const int32_t iy = static_cast<int32_t>(cy);
  ripple.set_pos(static_cast<int32_t>(cx), iy);
  init_ripple();
}

// Mech3.exe 1.2 @ 0x0042a160
void SlideButton::set_baked(bool on) {
  if (on) {
    stashed = img_normal;
    set_image(static_cast<zimg::Texture*>(nullptr));
    img_normal = set_image(static_cast<zimg::Texture*>(nullptr));
    return;
  }
  if (!stashed) return;
  img_normal = set_image(stashed);
  set_image(stashed);
  stashed = nullptr;
}

// Mech3.exe 1.2 @ 0x00485510
void SlideButton::init_ripple() {
  wavelength = 12.0f;
  speed = 120.0f;
}

// Mech3.exe 1.2 @ 0x00485530
void SlideButton::restart_ripple() {
  radius = 20;
  phase = 0.0f;
  amplitude = 7.0f;
  rippling = e_.fast_menu() ? 0 : 1;
}

// ---- the cycle button ----

// Mech3.exe 1.2 @ 0x0048b280: the ShellButton's id stays -1; `id` is the cycle's.
CycleButton::CycleButton(Engine& e, int32_t id) : ShellButton(e, -1), cycle_id(id) {}

// Mech3.exe 1.2 @ 0x0048b380
CycleButton::~CycleButton() {
  clear_items();
  if (!labels.empty()) labels[0]->set_text_plain("deleted cycle button");
}

// Mech3.exe 1.2 @ 0x0048b9b0
void CycleButton::set_pos(int32_t px, int32_t py) {
  if (!labels.empty()) {
    ztwod::Text* l = labels[0];
    const int32_t lx = l->get_x(), x0 = get_x(), ly = l->get_y(), y0 = get_y();
    l->set_pos(lx - x0 + px, ly - y0 + py);
  }
  Image::set_pos(px, py);
  prev.set_pos(prev_dx + px, prev_dy + py);
  next.set_pos(next_dx + px, next_dy + py);
}

namespace {
// 0x0048b670: the dialog's cursor position.
void cursor_xy(ztwod::Layer* owner, int32_t* x, int32_t* y) {
  auto* w = dynamic_cast<ztwod::Window*>(owner);
  // (the original reads through the owner's cursor unchecked; every dialog
  // the game loads has one)
  if (!w || !w->cursor) {
    *x = *y = 0;
    return;
  }
  *x = w->cursor->get_x();
  *y = w->cursor->get_y();
}
}  // namespace

// Mech3.exe 1.2 @ 0x0048b550: rolling onto the right arrow wins over the left.
void CycleButton::update(float dt) {
  int32_t mx, my;
  cursor_xy(owner, &mx, &my);
  const bool in_next = next.hit(mx, my) == 1;
  const bool in_prev = prev.hit(mx, my) == 1;
  if ((hover == 1) != in_next) {
    if (in_next) {
      next.on_enter();
      hover = 1;
    } else {
      hover = 0;
      next.on_leave();
    }
  } else if ((hover == 2) != in_prev) {
    if (in_prev) {
      prev.on_enter();
      hover = 2;
    } else {
      hover = 0;
      prev.on_leave();
    }
  }
  Object2D::update(dt);
  prev.update(dt);
  next.update(dt);
}

// Mech3.exe 1.2 @ 0x0048b6b0: the left arrow steps back, the right one (or,
// with click_steps, anywhere else) forward; the screen hears of it either way.
void CycleButton::on_left_up() {
  int32_t mx, my;
  cursor_xy(owner, &mx, &my);
  if (prev.enabled && prev.hit(mx, my)) {
    prev.on_left_up();
    step(-1);
    notify(1, 0);
    return;
  }
  if ((next.enabled && next.hit(mx, my)) || click_steps) {
    next.on_left_up();
    step(1);
  }
  notify(1, 0);
}

// Mech3.exe 1.2 @ 0x0048b950
int32_t CycleButton::hit(int32_t px, int32_t py) {
  if (Control::hit(px, py) || prev.hit(px, py)) return 1;
  return next.hit(px, py) ? 1 : 0;
}

// Mech3.exe 1.2 @ 0x0048b4e0
void CycleButton::show_for(float seconds) {
  Control::show_for(seconds);
  prev.show_for(seconds);
  next.show_for(seconds);
}

// Mech3.exe 1.2 @ 0x0048b520
void CycleButton::hide() {
  Control::hide();
  prev.hide();
  next.hide();
}

// Mech3.exe 1.2 @ 0x0048be70: TOGGLE_LEFT and TOGGLE_RIGHT read as controls,
// their places kept and taken out of the dialog (the button draws them).
bool CycleButton::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  reading = 1;
  const bool r = Control::read_control(node, dialog);
  reading = 0;
  if (zrdr::Node* t = zrdr::find(node, "TOGGLE_LEFT")) {
    prev.read_control(t, dialog);
    prev.after_bind();
    prev_dx = prev.get_x();
    prev_dy = prev.get_y();
    dialog->remove(&prev);
  }
  if (zrdr::Node* t = zrdr::find(node, "TOGGLE_RIGHT")) {
    next.read_control(t, dialog);
    next.after_bind();
    next_dx = next.get_x();
    next_dy = next.get_y();
    dialog->remove(&next);
  }
  const int32_t py = get_y();
  set_pos(get_x(), py);
  return r;
}

// Mech3.exe 1.2 @ 0x0048bf60: no backgrounds, every flag but kHidden cleared
// (drawn every frame).
void CycleButton::after_bind() {
  prev.set_background(nullptr, nullptr);
  next.set_background(nullptr, nullptr);
  flags &= ztwod::kHidden;
  prev.flags &= ztwod::kHidden;
  next.flags &= ztwod::kHidden;
}

// Mech3.exe 1.2 @ 0x0042fbc0
void CycleButton::notify(int32_t a, int32_t b) {
  if (!owner) return;
  if (auto* s = dynamic_cast<MenuScreen*>(owner)) s->on_button(cycle_id, a, b);
}

// Mech3.exe 1.2 @ 0x0048b490
void CycleButton::set_baked(bool on) { set_enabled(on ? 1 : 0); }
void CycleButton::set_enabled(int32_t on) {
  enabled = on;
  refresh();
  prev.enabled = on;
  prev.refresh();
  next.enabled = on;
  next.refresh();
}

// Mech3.exe 1.2 @ 0x0048bd40
void CycleButton::set_arrows(uint8_t prev_on, uint8_t click_on, uint8_t next_on) {
  click_steps = click_on;
  prev.enabled = prev_on;
  prev.refresh();
  next.enabled = next_on;
  next.refresh();
}

// Mech3.exe 1.2 @ 0x0048bcf0
void CycleButton::set_arrows(uint8_t prev_on, uint8_t next_on) {
  prev.enabled = prev_on;
  prev.refresh();
  next.enabled = next_on;
  next.refresh();
}

// Mech3.exe 1.2 @ 0x0048ba80
void CycleButton::set_label_font(Handle font) {
  if (!labels.empty() && font) labels[0]->set_font(font);
}

// Mech3.exe 1.2 @ 0x0048b850
void CycleButton::select(int32_t i) {
  index = i;
  if (i >= 0 && i < count()) {
    if (!labels.empty()) labels[0]->set_text_plain(items[static_cast<size_t>(i)]->text);
    return;
  }
  if (!labels.empty()) labels[0]->set_text_plain("");
}

// Mech3.exe 1.2 @ 0x0048b790
void CycleButton::step(int32_t by) {
  int32_t i = index + by;
  index = i;
  const int32_t n = count();
  if (i < 0) {
    i = wraps ? n + i : 0;
  } else if (i >= n) {
    i = wraps ? i - n : n - 1;
  } else {
    select(index);
    return;
  }
  index = i;
  select(index);
}

// Mech3.exe 1.2 @ 0x0048bac0: the first choice added is selected at once.
int32_t CycleButton::add_item(const char* text, int32_t value) {
  const int32_t old = count();
  auto* it = new Item{text ? ::strdup(text) : nullptr, value};
  items.push_back(it);
  if (index < 0) step(-index);
  return old;
}

// Mech3.exe 1.2 @ 0x0048bda0
void CycleButton::clear_items() {
  for (Item*& it : items) {
    if (it) {
      std::free(it->text);
      delete it;
    }
    it = nullptr;
  }
  items.clear();
  if (!labels.empty()) labels[0]->set_text_plain("");
  index = -1;
}

// Mech3.exe 1.2 @ 0x00438e10
CycleButton::Item* CycleButton::item(int32_t i) {
  if (i < 0 || i >= count()) return nullptr;
  return items[static_cast<size_t>(i)];
}

// Mech3.exe 1.2 @ 0x00438e50
int32_t CycleButton::value_at(int32_t i) {
  Item* it = item(i);
  return it ? it->value : 0;
}

// Mech3.exe 1.2 @ 0x0048b8f0
int32_t CycleButton::select_value(int32_t v) {
  for (int32_t i = 0; i < count(); ++i) {
    if (items[static_cast<size_t>(i)]->value == v) {
      select(i);
      return i;
    }
  }
  return -1;
}

// Mech3.exe 1.2 @ 0x0043a710
void CycleButton::select_each_value(int32_t v) {
  for (int32_t i = 0; i < count(); ++i)
    if (value_at(i) == v) select(i);
}

// Mech3.exe 1.2 @ 0x00437fe0: OFF / ON (messages 0x20, 0x21), green or red.
void AlertCycle::after_bind() {
  CycleButton::after_bind();
  add_item(e_.message(0x20), 0);
  add_item(e_.message(0x21), 1);
  font_normal = ztwod::host().font_find("ia_small_green");
  font_alert = ztwod::host().font_find("ia_small_red");
}

// Mech3.exe 1.2 @ 0x00438040
void AlertCycle::select(int32_t i) {
  CycleButton::select(i);
  if (i == alert) {
    if (font_alert) set_label_font(font_alert);
  } else if (font_normal) {
    set_label_font(font_normal);
  }
}

// Mech3.exe 1.2 @ 0x00438ed0
void OnOffCycle::after_bind() {
  CycleButton::after_bind();
  add_item(e_.message(0x20), 0);
  add_item(e_.message(0x21), 1);
}

// Mech3.exe 1.2 @ 0x00438f10
void LevelCycle::after_bind() {
  CycleButton::after_bind();
  add_item(e_.message(0x40), 0);
  add_item(e_.message(0x39), 1);
  add_item(e_.message(0x38), 2);
}

}  // namespace mech3
