// zTwoD's text copies and the scrolling text box (docs/twod.md, Text).
// Original file unknown: after twod_wintext.cpp (0x005a18f0..0x005a2ae0, the
// copies 0x005a2e30 / 0x005a2ed0) and in it (0x005a0200 / 0x005a03f0); the
// vector's destructor sits in briefing.cpp's code (0x004097e0).
#include <cstdlib>
#include <cstring>
#include <new>

#include "gamez/zimage/zimg_library.h"
#include "gamez/ztwod/twod.h"

namespace ztwod {

namespace {
void copy_text(char* dst, const char* src) {  // MSVCRT strncpy(dst, src, 0x200)
  size_t i = 0;
  for (; i < 0x200 && src[i]; ++i) dst[i] = src[i];
  for (; i < 0x200; ++i) dst[i] = 0;
}
}  // namespace

// ---- copies ----

// Mech3.exe 1.2 @ 0x005a2e30
TextBase::TextBase(const TextBase& o)
    : Object2D(o), font(o.font), centered(o.centered), centre_left(o.centre_left),
      centre_right(o.centre_right), align(o.align) {
  copy_text(text, o.text);
}

// Mech3.exe 1.2 @ 0x005a2ed0
void TextBase::assign(const TextBase& o) {
  Object2D::operator=(o);
  copy_text(text, o.text);
  font = o.font;
  centered = o.centered;
  centre_left = o.centre_left;
  centre_right = o.centre_right;
  align = o.align;
}

namespace {
// The fields after the font that both copies take whole.
void copy_render_state(FontText& d, const FontText& o) {
  d.u25c = o.u25c;
  copy_text(d.rendered, o.rendered);
  d.width = o.width;
  d.height = o.height;
  d.shadow = o.shadow;
  d.bk_mode = o.bk_mode;
  d.bk_color = o.bk_color;
  d.needs_render = o.needs_render;
  d.leading = o.leading;
  d.fixed = o.fixed;
  d.fixed_rect = o.fixed_rect;
  d.text_rect = o.text_rect;
  d.align = o.align;
  d.shadow_dx = o.shadow_dx;
  d.shadow_dy = o.shadow_dy;
  d.max_width = o.max_width;
}
}  // namespace

// Mech3.exe 1.2 @ 0x005a0200
FontText::FontText(const FontText& o) : TextBase(o) {
  image = nullptr;
  named = 0;
  hfont = 0;  // not set before the font copy in the original (zeroed memory in practice)
  color_top = o.color_top;
  color_bottom = o.color_bottom;
  shadow_color = o.shadow_color;
  if (o.named) {
    set_named_font(o.named, true);  // 0x005a0660, FontText's at this point
  } else if (host().GetObjectA(o.hfont)) {
    hfont = host().CreateFontIndirectA(o.hfont);
    if (!hfont) host().GetLastError();
  }
  copy_render_state(*this, o);
}

// Mech3.exe 1.2 @ 0x005a03f0
void FontText::assign(const FontText& o) {
  TextBase::assign(o);
  image = nullptr;  // dropped without freeing (as written)
  color_top = o.color_top;
  color_bottom = o.color_bottom;
  shadow_color = o.shadow_color;
  if (o.named) {
    set_font(o.named);
  } else if (host().GetObjectA(o.hfont)) {
    hfont = host().CreateFontIndirectA(o.hfont);  // the old font is not deleted (as written)
    if (!hfont) host().GetLastError();
  }
  copy_render_state(*this, o);
  needs_render = 1;
}

// Mech3.exe 1.2 @ 0x005a2ae0
Text::Text(const Text& o)
    : FontText(o), flash_timer(o.flash_timer), flash_rate(o.flash_rate), alt_top(o.alt_top),
      alt_bottom(o.alt_bottom), flash_on(o.flash_on), flash_mode(o.flash_mode), phase(o.phase) {}

// Mech3.exe 1.2 @ 0x005a2a70
void Text::assign(const Text& o) {
  FontText::assign(o);
  flash_timer = o.flash_timer;
  flash_rate = o.flash_rate;
  alt_top = o.alt_top;
  alt_bottom = o.alt_bottom;
  flash_on = o.flash_on;
  flash_mode = o.flash_mode;
  phase = o.phase;
}

// ---- TextVector ----

namespace {
Text* allocate(size_t n) {
  return static_cast<Text*>(::operator new(n * sizeof(Text)));
}
}  // namespace

// Mech3.exe 1.2 @ 0x004097e0
TextVector::~TextVector() {
  for (Text* p = first_; p != last_; ++p) p->~Text();
  ::operator delete(first_);
}

// Mech3.exe 1.2 @ 0x005a2700
// HP STL's insert(pos, n, x): a new block of size + max(size, n) when it does
// not fit; else the tail copied up and x copied or assigned into the gap.
void TextVector::insert(Text* pos, size_t n, const Text& x) {
  const size_t size = this->size();
  if (static_cast<size_t>(end_ - last_) < n) {
    const size_t len = size + (n < size ? size : n);
    Text* block = allocate(len);
    Text* d = block;
    for (Text* p = first_; p != pos; ++p, ++d) new (d) Text(*p);
    for (size_t i = 0; i < n; ++i, ++d) new (d) Text(x);
    for (Text* p = pos; p != last_; ++p, ++d) new (d) Text(*p);
    for (Text* p = first_; p != last_; ++p) p->~Text();
    ::operator delete(first_);
    end_ = block + len;
    first_ = block;
    last_ = block + size + n;
    return;
  }
  const size_t after = static_cast<size_t>(last_ - pos);
  if (after < n) {
    Text* d = pos + n;
    for (Text* p = pos; p != last_; ++p, ++d) new (d) Text(*p);
    d = last_;
    for (size_t i = 0; i < n - after; ++i, ++d) new (d) Text(x);
    for (Text* p = pos; p != last_; ++p) p->assign(x);
    last_ += n;
    return;
  }
  if (n == 0) return;
  Text* d = last_;
  for (Text* p = last_ - n; p != last_; ++p, ++d) new (d) Text(*p);
  for (Text* p = last_ - n; p != pos;) {
    --p;
    (p + n)->assign(*p);
  }
  for (Text* p = pos; p != pos + n; ++p) p->assign(x);
  last_ += n;
}

// The inline erase(from, end()): nothing after `end` to move up, then the
// tail destroyed.
void TextVector::erase_to_end(Text* from) {
  for (Text* p = from; p != last_; ++p) p->~Text();
  last_ = from;
}

// ---- TextBox ----

// Mech3.exe 1.2 @ 0x005a18f0
TextBox::TextBox(uint32_t count) : FontText(nullptr, 0, 0) {
  {
    Text proto;
    const size_t size = lines.size();
    if (size < count) {
      lines.insert(lines.end(), count - size, proto);
    } else if (count < size) {
      lines.erase_to_end(lines.begin() + count);
    }
  }
  FontText::set_text("W");  // 0x005ed7ac: the line height is the box's own text's, a "W"
  TextBox::set_pos(0, 0);
  set_line_count(count);
  Object2D::show_for(-1.0f);
}

// Mech3.exe 1.2 @ 0x005a1ac0 (0x00406020)
TextBox::~TextBox() = default;

// Mech3.exe 1.2 @ 0x005a1ba0
// One line height apart: downward from the first line, or upward from the
// current one when bottom_up.
void TextBox::set_pos(int32_t px, int32_t py) {
  y = py;
  x = px;
  const int32_t h = text_height();
  if (!bottom_up) {
    int32_t dy = 0;
    for (size_t i = 0; i < lines.size(); ++i, dy += h) {
      const int32_t ly = get_y() + dy;
      lines[i].set_pos(get_x(), ly);
    }
    return;
  }
  int32_t dy = 0;
  for (int32_t i = current; i >= 0; --i, dy += h) {
    const int32_t ly = get_y() + dy;
    lines[static_cast<size_t>(i)].set_pos(get_x(), ly);
  }
}

// Mech3.exe 1.2 @ 0x005a1ae0
void TextBox::update(float dt) {
  if (flags & kHidden) return;
  if (flags & kTimed) {
    timer -= dt;
    if (timer <= 0.0f) {
      flags &= ~kTimed;
      hide();
    }
  }
  if (reposition) {
    const int32_t py = get_y();
    set_pos(get_x(), py);
    reposition = 0;
  }
  for (size_t i = 0; i < lines.size(); ++i) lines[i].update(dt);
}

// Mech3.exe 1.2 @ 0x005a20a0
void TextBox::create_font(const char* face, int32_t h, int32_t weight, int32_t w, uint32_t italic,
                          uint32_t charset, uint32_t pitch) {
  for (size_t i = 0; i < lines.size(); ++i) lines[i].create_font(face, h, weight, w, italic, charset, pitch);
  FontText::create_font(face, h, weight, w, italic, charset, pitch);
  const int32_t py = get_y();
  set_pos(get_x(), py);
}

// Mech3.exe 1.2 @ 0x005a2160
void TextBox::set_font(Handle f) {
  for (size_t i = 0; i < lines.size(); ++i) lines[i].set_font(f);
  set_named_font(f, true);  // 0x005a0660 on itself
  const int32_t py = get_y();
  set_pos(get_x(), py);
}

// Mech3.exe 1.2 @ 0x005a21e0
void TextBox::set_color(uint32_t c) {
  for (size_t i = 0; i < lines.size(); ++i) {
    Text& t = lines[i];
    if (c != t.color_top || c != t.color_bottom) {
      t.color_top = c;
      t.color_bottom = c;
      t.needs_render = 1;
    }
  }
  if (c != color_top || c != color_bottom) {
    color_top = c;
    color_bottom = c;
    needs_render = 1;
  }
}

// Mech3.exe 1.2 @ 0x005a1d70
void TextBox::set_text(const char* format, ...) {
  va_list args;
  va_start(args, format);
  set_text_v(format, args);
  va_end(args);
}

// Mech3.exe 1.2 @ 0x005a1d90
void TextBox::set_text_v(const char* format, va_list args) {
  advance();
  Text& t = lines[static_cast<size_t>(current)];
  t.set_text_v(format, args);
  t.show_for(-1.0f);
  after_write(1);
}

// Mech3.exe 1.2 @ 0x005a1e10
void TextBox::set_text_top(const char* format, ...) {
  va_list args;
  va_start(args, format);
  set_text_top_v(format, args);
  va_end(args);
}

// Mech3.exe 1.2 @ 0x005a1e30
void TextBox::set_text_top_v(const char* format, va_list args) {
  advance_down();
  lines[0].set_text_v(format, args);
  lines[0].show_for(-1.0f);
  after_write(1);
}

// Mech3.exe 1.2 @ 0x005a1e80
// Past a shown line to the next; at the end every line takes the one below's
// text and the last is written again.
void TextBox::advance() {
  const int32_t old = current;
  // FIX (bug): after the line count shrinks, current is one past the last line
  // and the original reads the flags of a line that is not there; taken as
  // hidden here (docs/fixes.md).
  const bool there = static_cast<uint32_t>(current) < lines.size();
  if (there && !(lines[static_cast<size_t>(current)].flags & kHidden)) current = old + 1;
  if (static_cast<uint32_t>(current) >= lines.size()) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(lines.size()) - 1; ++i) {
      const char* below = lines[i + 1].rendered_text();
      lines[i].set_text_plain(below);
    }
    --current;
  }
  if (bottom_up && old != current) reposition = 1;
  dirty();
}

// Mech3.exe 1.2 @ 0x005a1f90
void TextBox::advance_down() {
  if (static_cast<uint32_t>(current) >= lines.size()) return;  // FIX (bug): see advance
  if (lines[static_cast<size_t>(current)].flags & kHidden) return;
  uint32_t n = static_cast<uint32_t>(current) + 1;
  if (n >= static_cast<uint32_t>(lines.size()) - 1) n = static_cast<uint32_t>(lines.size()) - 1;
  current = static_cast<int32_t>(n);
  for (uint32_t i = n; i != 0; --i) {
    const char* above = lines[i - 1].rendered_text();
    lines[i].set_text_plain(above);
  }
  lines[static_cast<size_t>(current)].show_for(-1.0f);
  dirty();
}

// Mech3.exe 1.2 @ 0x005a2320
void TextBox::set_line_count(uint32_t n) {
  const uint32_t size = static_cast<uint32_t>(lines.size());
  if (n != size) {
    {
      Text proto;
      if (size < n) {
        lines.insert(lines.end(), n - size, proto);
      } else if (n < size) {
        lines.erase_to_end(lines.begin() + n);
      }
    }
    clear_lines(static_cast<int32_t>(size), static_cast<int32_t>(n));
  } else {
    clear();
  }
  const int32_t py = get_y();
  set_pos(get_x(), py);
}

// Mech3.exe 1.2 @ 0x005a24e0
void TextBox::clear() { clear_lines(0, static_cast<int32_t>(lines.size())); }

// Mech3.exe 1.2 @ 0x005a2520
void TextBox::clear_lines(int32_t from, int32_t to) {
  int32_t start = from;
  if (from > to) start = to;
  else if (from < 0) start = 0;
  int32_t end = to;
  if (static_cast<uint32_t>(to) > static_cast<uint32_t>(lines.size())) end = static_cast<int32_t>(lines.size());
  for (int32_t i = start; i < end; ++i) {
    Text& t = lines[static_cast<size_t>(i)];
    t.set_text("");
    t.hide();
  }
  current = start;
}

// Mech3.exe 1.2 @ 0x005a2280
// FIX (bug): one past the last line after a shrink (see advance) is the last line.
Text& TextBox::line() {
  const size_t i = static_cast<size_t>(current) < lines.size() ? static_cast<size_t>(current) : lines.size() - 1;
  return lines[i];
}

// Mech3.exe 1.2 @ 0x005a22a0
void TextBox::new_line() {
  advance();
  after_write(0);
}

// Mech3.exe 1.2 @ 0x005a22f0
void TextBox::append(const char* s) {
  Text& t = line();
  t.show_for(-1.0f);
  t.set_text_plain(s);
}

// Mech3.exe 1.2 @ 0x005a25f0
int32_t TextBox::total_height() {
  const int32_t h = text_height();
  return lines.size() ? h * static_cast<int32_t>(lines.size()) : 0;
}

// Mech3.exe 1.2 @ 0x005a2640
int32_t TextBox::max_width() {
  int32_t best = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    Text& t = lines[i];
    if (t.flags & kHidden) continue;
    if (t.needs_render) t.render();
    if (best <= t.width) {
      if (t.needs_render) t.render();
      best = t.width;
    }
  }
  return best;
}

}  // namespace ztwod
