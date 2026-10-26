#include "mech3/lab_items.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "gamez/zimage/zimg_library.h"
#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/app_hooks.h"
#include "mech3/mech_config.h"
#include "mech3/pilot.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
ztwod::Object2D* cursor_of(ztwod::Layer* owner) {
  auto* w = dynamic_cast<ztwod::Window*>(owner);
  return w ? w->cursor : nullptr;
}
bool shown(const ztwod::Object2D* o) { return !(o->flags & ztwod::kHidden); }
}  // namespace

// Mech3.exe 1.2 @ 0x0048c0b0
void flash_text(ztwod::Text& t, bool on, Handle font) {
  if (!on) {
    t.stop_flash();
    return;
  }
  if (font) t.set_flash(ztwod::host().font_fields(font).color_top, 0.0f);
}

// ---- the row ----

// Mech3.exe 1.2 @ 0x00451410
LabItemRow::LabItemRow(LabItemList* l, Handle font) : list(l) {
  FontText::set_font(font);
  flash_rate = 0.01f;
  panel.hide();
  panel.set_color(0);
  panel.set_opacity(1.0f);
  count.hide();
  count.align = 2;
  line.color = zvid::pack_bytes(0, 0xff, 0);
  line.hide();
}

// Mech3.exe 1.2 @ 0x004519a0: the texts at their columns, the line under the
// name from its end to 0x89.
void LabItemRow::set_pos(int32_t px, int32_t py) {
  x = px;
  y = py;
  panel.set_pos(px, py);
  count.set_pos(px + 0xac, py);
  mass.set_pos(px + 0x93, py);
  qty.set_pos(px + 0x7a, py);
  if (needs_render) render();
  line.set(width + 2 + px, py + 6, px + 0x89, py + 6);
}

// Mech3.exe 1.2 @ 0x00451a50 with 0x004516a0: the item's name (an empty one
// for none) and count; with a name the panel and the count show, the other
// two texts and the line hide; the item's font, when set, on the name and
// the count.
void LabItemRow::fill(int32_t i, const LabItem* item) {
  index = i;
  set_text_plain(item->text ? item->text : "");
  const LabItem* it = i >= 0 && i < list->item_count() ? list->items[static_cast<size_t>(i)] : nullptr;
  const Handle font = it ? it->font : 0;
  const int32_t n = it->count;
  height = 0xd;
  if (it->text) {
    panel.show_for(-1.0f);
    panel.set_size(0xac, height);
    panel.set_color(zvid::pack_bytes(0x19, 0x19, 0x19));
    count.show_for(-1.0f);
    count.set_text("%02d", n);
    qty.hide();
    mass.hide();
    if (font) {
      set_font(font);
      count.set_font(font);
    }
    line.hide();
  }
  if (font) {
    set_font(font);
    count.set_font(font);
    count.align = 2;
  }
}

// Mech3.exe 1.2 @ 0x00451b20
int32_t LabItemRow::panel_width() const { return panel.image ? panel.image->width : 0; }

// Mech3.exe 1.2 @ 0x004515e0
void LabItemRow::update(float dt) {
  if (flags & ztwod::kHidden) return;
  const LabItem* it = index >= 0 && index < list->item_count() ? list->items[static_cast<size_t>(index)] : nullptr;
  if (!it || !it->text) return;  // (the original reads the item unchecked)
  panel.update(dt);
  Text::update(dt);
  count.update(dt);
  mass.update(dt);
  qty.update(dt);
  line.update(dt);
}

// Mech3.exe 1.2 @ 0x00456820
void LabItemRow::on_left_up() { list->select(index); }

// Mech3.exe 1.2 @ 0x00451aa0
int32_t LabItemRow::hit(int32_t px, int32_t py) {
  if (flags & ztwod::kHidden) return 0;
  ztwod::Rect r{};
  rect_at(&r);  // vt+0x74
  return px >= r.left && px <= r.left + 0xac && py >= r.top && py <= r.bottom ? 1 : 0;
}

// ---- the arrows ----

// Mech3.exe 1.2 @ 0x004567c0 (UP), 0x004567e0 (DOWN)
void LabListArrow::on_left_up() {
  Control::on_left_up();
  list->scroll(by);
}

// Mech3.exe 1.2 @ 0x00456800
void LabListArrow::after_bind() { flags &= ztwod::kHidden; }

// ---- the list ----

// Mech3.exe 1.2 @ 0x00451b40
LabItemList::LabItemList(Engine& e, int32_t i) : ShellButton(e, i) {}

// Mech3.exe 1.2 @ 0x00451ce0
LabItemList::~LabItemList() {
  clear();
  delete selected_row;
  for (LabItemRow* r : rows) delete r;
  rows.clear();
  delete bar_image;
  delete roll_image;
  delete up;
  delete down;
}

// Mech3.exe 1.2 @ 0x00453420
void LabItemList::clear() {
  for (LabItem* it : items) {
    if (!it) continue;
    if (it->text) std::free(it->text);
    delete it;
  }
  items.clear();
  layout();
}

// Mech3.exe 1.2 @ 0x00453210: the item appended (not laid out).
int32_t LabItemList::add(const char* text, int32_t n, Handle object) {
  const int32_t old = item_count();
  auto* it = new LabItem;
  it->object = object;
  it->count = n;
  it->text = text ? strdup(text) : nullptr;
  items.push_back(it);
  return old;
}

// Mech3.exe 1.2 @ 0x004531f0
int32_t LabItemList::add_text(const char* text, int32_t n) { return add(text, n, 0); }

// Mech3.exe 1.2 @ 0x00453160: ammo by message 0xf59 with its name; -1 takes
// the item's slots (vt+0x18).
int32_t LabItemList::add_object(Handle item, int32_t n) {
  Engine& e = e_;
  std::string text;
  if (e.lab_item_kind(item) == 2) {
    const std::string name = e.lab_item_name(item);
    const char* t = e.message_format(0xf59, name.c_str());
    text = t ? t : "";
  } else {
    text = e.lab_item_name(item);
    if (text.size() > 0xff) text.resize(0xff);  // strncpy into 0x100 (unterminated past it)
  }
  if (n == -1) n = e.lab_item_slots(item);
  return add(text.c_str(), n, item);
}

// Mech3.exe 1.2 @ 0x004534b0
Handle LabItemList::object_at(int32_t i) const {
  const LabItem* it = i >= 0 && i < item_count() ? items[static_cast<size_t>(i)] : nullptr;
  return it ? it->object : 0;
}

// Mech3.exe 1.2 @ 0x00453140
int32_t LabItemList::add_part(int32_t part) { return add(e_.part_text(part), 0, part); }

// Mech3.exe 1.2 @ 0x00452a90
void LabItemList::scroll(int32_t by) {
  top += by;
  if (top > item_count()) top = item_count();
  else if (top < 0) top = 0;
  refill();
}

// Mech3.exe 1.2 @ 0x00452a40
void LabItemList::select(int32_t index) {
  if (index < 0 || index >= item_count()) return;
  selected = index;
  notify(10, index);
  layout();
}

// Mech3.exe 1.2 @ 0x00453540 (reached by neither list: both have their own).
void LabItemList::refill() {
  if (flags & ztwod::kHidden) return;
  clear();
  Store* s = store ? e_.store_of(store) : nullptr;
  if (!s) return;
  for (const ConfigItem* it : s->items) {
    const Handle h = e_.item_handle(it);
    if (!filter || filter(e_, h)) add_object(h, -1);
  }
  layout();
}

// Mech3.exe 1.2 @ 0x00452bc0
void LabItemList::place_at(ztwod::Image* o) {
  if (!o) return;
  const int32_t oy = o->get_y();
  set_pos(o->get_x(), oy);
  rect.left = o->get_x();
  rect.top = o->get_y();
  rect.right = rect.left + (o->image ? o->image->width : 0);
  rect.bottom = rect.top + (o->image ? o->image->height : 0);
}

// Mech3.exe 1.2 @ 0x00453020
void LabItemList::set_rows(const int32_t sz[2]) {
  size[0] = sz[0];
  size[1] = sz[1];
  const int32_t n = size[1] / row_height;
  while (static_cast<int32_t>(rows.size()) < n) rows.push_back(new LabItemRow(this, font_normal));
  while (static_cast<int32_t>(rows.size()) > n) {
    delete rows.back();
    rows.pop_back();
  }
}

// Mech3.exe 1.2 @ 0x00451e60
bool LabItemList::read_control(zrdr::Node* node, ztwod::Dialog* dialog) {
  ztwod::Host& h = ztwod::host();
  reading = 1;
  Control::read_control(node, dialog);
  reading = 0;
  font_selected = h.font_find(zrdr::get_string(node, "SELECTED_FONT"));
  font_normal = h.font_find(zrdr::get_string(node, "NORMAL_FONT"));
  font_large = h.font_find(zrdr::get_string(node, "LARGE_FONT"));
  font_static = h.font_find(zrdr::get_string(node, "STATIC_FONT"));
  font_mass = h.font_find(zrdr::get_string(node, "MASS_FONT"));
  font_count = h.font_find(zrdr::get_string(node, "COUNT_FONT"));
  font_grey = h.font_find(zrdr::get_string(node, "GREY_FONT"));
  if (const char* sn = zrdr::get_string(node, "ROLLOVER_SOUND")) sounds[0] = h.sound_find(sn);
  if (const char* sn = zrdr::get_string(node, "SELECT_SOUND")) sounds[1] = h.sound_find(sn);
  if (!font_selected) font_selected = font_normal;
  zrdr::get_ivec2(node, "SPACING", spacing);
  zrdr::get_ivec2(node, "OFFSET", offset);
  zrdr::get_ivec2(node, "SIZE", size);
  zrdr::get_int(node, "HEIGHT", &row_height);
  int32_t v[2];
  if (zrdr::Node* n = zrdr::find(node, "BAR")) {
    bar_image = new ztwod::Image(0);
    const char* name = zrdr::get_string(n, "BITMAP");
    if (zrdr::get_ivec2(n, "OFFSET", v)) bar_dx = v[0], bar_dy = v[1];
    bar_image->set_image(name);
  }
  if (zrdr::Node* n = zrdr::find(node, "ITEMROLLOVER")) {
    roll_image = new ztwod::Image(0);
    const char* name = zrdr::get_string(n, "BITMAP");
    if (zrdr::get_ivec2(n, "OFFSET", v)) roll_dx = v[0], roll_dy = v[1];
    roll_image->set_image(name);
    font_rollover = h.font_find(zrdr::get_string(node, "FONT"));
  }
  // The arrows are placed from the list and taken out of the dialog: the list updates them.
  for (int k = 0; k < 2; ++k) {
    zrdr::Node* n = zrdr::find(node, k ? "DOWN" : "UP");
    if (!n) continue;
    LabListArrow*& a = k ? down : up;
    a = new LabListArrow(e_, this, k ? 1 : -1);
    a->read_control(n, dialog);
    a->after_bind();
    const int32_t ay = a->get_y() + get_y();
    a->set_pos(a->get_x() + get_x(), ay);
    dialog->remove(a);
  }
  place_at(this);
  return true;
}

// Mech3.exe 1.2 @ 0x004522d0
void LabItemList::after_bind() {
  set_rows(size);
  selected_row = new LabItemRow(this, font_selected);
  set_pos(x, y);
}

// Mech3.exe 1.2 @ 0x00452b90
void LabItemList::set_pos(int32_t px, int32_t py) {
  Image::set_pos(px, py);
  layout();
}

// Mech3.exe 1.2 @ 0x00452360
void LabItemList::show_for(float seconds) {
  Control::show_for(seconds);
  layout();
  if (up) up->show_for(seconds);
  if (down) down->show_for(seconds);
}

// Mech3.exe 1.2 @ 0x004523a0
void LabItemList::hide() {
  Control::hide();
  if (up) up->hide();
  if (down) down->hide();
  if (bar_image) bar_image->hide();
  if (roll_image) roll_image->hide();
}

// Mech3.exe 1.2 @ 0x00452780: the rows from `top` down from OFFSET, SPACING.y
// + 13 apart while they fit SIZE's height (3 pixels over allowed); the
// selected item's row in SELECTED_FONT over the BAR; the rectangle round
// the rows (as wide as the widest panel) when the list has no image.
void LabItemList::layout() {
  if (flags & ztwod::kHidden) return;
  const int32_t x0 = get_x() + offset[0];
  const int32_t y0 = get_y() + offset[1];
  int32_t n = item_count() - top;
  const int32_t nrows = static_cast<int32_t>(rows.size());
  if (n > nrows) n = nrows;
  else if (n < 0) n = 0;
  ztwod::Rect r{spacing[0] + x0, y0, 0, y0};
  const int32_t limit = size[1] + y0;
  if (selected_row) selected_row->hide();
  if (bar_image) bar_image->hide();
  int32_t i = 0;
  for (; i < n && r.bottom < limit; ++i) {
    LabItemRow* row = rows[static_cast<size_t>(i)];
    row->show_for(-1.0f);
    row->set_font(font_normal);
    row->fill(i + top, items[static_cast<size_t>(i + top)]);
    if (r.right <= row->panel_width()) r.right = row->panel_width();
    row->set_pos(r.left, r.bottom);
    const int32_t next = spacing[1] + r.bottom + 0xd;
    if (next > limit + 3) {
      r.bottom = limit;
      break;
    }
    r.bottom = next;
    if (i + top == selected && selected_row) {
      const int32_t rx = row->get_x();
      const int32_t ry = row->get_y();
      row->set_font(font_selected);
      if (bar_image) {
        bar_image->set_pos(bar_dx + rx, bar_dy + ry);
        bar_image->show_for(-1.0f);
      }
    }
  }
  end = top + i;
  if (end < 0) end = 0;
  for (; i < nrows; ++i) rows[static_cast<size_t>(i)]->hide();
  r.right += r.left;
  if (!image) rect = r;
}

// Mech3.exe 1.2 @ 0x00452af0
int32_t LabItemList::hit(int32_t px, int32_t py) {
  int32_t in = shown(this) && px >= rect.left && px <= rect.right && py >= rect.top && py <= rect.bottom ? 1 : 0;
  if (up && (in || up->hit(px, py))) in = 1;
  if (down) {
    if (!in && !down->hit(px, py)) return 0;
    in = 1;
  }
  return in;
}

// Mech3.exe 1.2 @ 0x00452ea0: a shown row under the point, else the selected row.
LabItemRow* LabItemList::row_at(int32_t px, int32_t py) {
  for (LabItemRow* r : rows)
    if (shown(r) && r->hit(px, py)) return r;
  if (selected_row && shown(selected_row) && selected_row->hit(px, py)) return selected_row;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00452da0
LabItemRow* LabItemList::row_under_mouse() {
  ztwod::Object2D* c = cursor_of(owner);
  const int32_t cx = c->get_x();
  const int32_t cy = c->get_y();
  return row_at(cx, cy);
}

// Mech3.exe 1.2 @ 0x00452c50
void LabItemList::stop_sound(int32_t i) {
  ztwod::host().sound_stop(playing[i]);
  playing[i] = 0;
}

// Mech3.exe 1.2 @ 0x00451870
void LabItemRow::show_slots(int32_t used, int32_t total) {
  const LabItem* it = index >= 0 && index < list->item_count() ? list->items[static_cast<size_t>(index)] : nullptr;
  const Handle font = it ? it->font : 0;
  count.show_for(-1.0f);
  if (font) count.set_font(font);
  count.set_text("%2d/%2d", used, total);
  count.align = 2;
  line.show_for(-1.0f);
  panel.hide();
  const int32_t px = get_x(), py = get_y();
  if (needs_render) render();
  line.set(width + px + 2, py + 6, px + 0x89, py + 6);
}

// Mech3.exe 1.2 @ 0x00452c80
void LabItemList::play_sound(int32_t i, float volume) { playing[i] = ztwod::host().sound_play(sounds[i], volume); }

// Mech3.exe 1.2 @ 0x00452cb0: the row under the mouse lights in FONT's colour
// with the rollover sound.
void LabItemList::on_drag(int32_t px, int32_t py) {
  if (!inside) return;
  LabItemRow* r = row_at(px, py);
  if (r == lit) return;
  if (lit) {
    stop_sound(0);
    flash_text(*lit, false, 0);
  }
  if (r && can_light(r->index)) {
    play_sound(0, 1.0f);
    lit = r;
    flash_text(*r, true, font_rollover);
    return;
  }
  lit = nullptr;
}

// Mech3.exe 1.2 @ 0x00452d50
void LabItemList::on_enter() {
  Control::on_enter();
  inside = 1;
}

// Mech3.exe 1.2 @ 0x00452d70
void LabItemList::on_leave() {
  Control::on_leave();
  inside = 0;
  if (lit) flash_text(*lit, false, 0);
  lit = nullptr;
}

// Mech3.exe 1.2 @ 0x00452f40: a row presses itself (the press shown, the
// screen told); off the rows, a press on an arrow, unless repeating.
void LabItemList::on_left_up() {
  if (LabItemRow* r = row_under_mouse()) {
    r->on_left_up();
    if (enabled) {
      Control::on_left_up();
      notify(1, 0);
    }
    return;
  }
  if (repeating) return;
  ztwod::Object2D* c = cursor_of(owner);
  const int32_t cx = c->get_x();
  const int32_t cy = c->get_y();
  if (up && up->hit(cx, cy)) {
    up->on_left_up();
    return;
  }
  if (down && down->hit(cx, cy)) down->on_left_up();
}

// Mech3.exe 1.2 @ 0x004523f0: the arrows light under the cursor (by their
// hit alone) and repeat while the left button is held on them (after
// repeat_delay, then every repeat_rate seconds of the frame time); the
// rollover image follows the lit row; UP is enabled past the first item,
// DOWN while items are below the last shown.
void LabItemList::update(float dt) {
  if (flags & ztwod::kHidden) return;
  Object2D::update(dt);
  if (!inside) {
    repeating = 0;
    repeat_timer = repeat_delay;
  } else {
    ztwod::Object2D* c = cursor_of(owner);
    const int32_t cx = c->get_x();
    c = cursor_of(owner);
    const int32_t cy = c->get_y();
    on_drag(cx, cy);
    const bool u = up && up->hit(cx, cy) == 1;
    const bool d = down && down->hit(cx, cy) == 1;
    if ((arrow_lit == 1) != u) {
      if (u) {
        up->on_enter();
        arrow_lit = 1;
      } else {
        arrow_lit = 0;
        up->on_leave();
      }
    } else if ((arrow_lit == 2) != d) {
      if (d) {
        down->on_enter();
        arrow_lit = 2;
      } else {
        arrow_lit = 0;
        down->on_leave();
      }
    }
    const ztwod::MouseState m = ztwod::host().mouse();
    if (m.left == 2) {
      const float t = repeat_timer - dt;
      const bool waiting = 0.0f <= t;  // [0x005c1d50]
      repeat_timer = t;
      if (!repeating) {
        repeating = waiting ? 0 : 1;
      } else if (!waiting) {
        repeat_timer = repeat_rate;
        if (u) up->on_left_up();
        else if (d) down->on_left_up();
        else repeating = 0;
      }
    } else {
      repeating = 0;
      repeat_timer = repeat_delay;
    }
  }
  if (bar_image) bar_image->update(dt);
  if (roll_image) {
    if (lit && can_roll(lit->index)) {
      const int32_t ly = lit->get_y() + roll_dy;
      roll_image->set_pos(lit->get_x() + roll_dx, ly);
      roll_image->show_for(-1.0f);
    } else {
      roll_image->hide();
    }
    roll_image->update(dt);
  }
  for (size_t i = 0; i < rows.size(); ++i) rows[i]->update(dt);
  if (selected_row) selected_row->update(dt);
  if (up) {
    up->enabled = top != 0;
    up->refresh();
    up->update(dt);
  }
  if (down) {
    down->enabled = end < item_count();
    down->refresh();
    down->update(dt);
  }
}

}  // namespace mech3
