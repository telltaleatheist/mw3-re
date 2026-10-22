// C:\zipper\GameZMech3\zTwoD\twod_dialog.cpp: a screen read from a .zrd
// definition (0x0059f810 BindButton and 0x0059f8d0 BindPrimitive by their
// error reports; the loader and the label readers just before them).
// docs/twod.md, Dialogs.
#include <cstring>

#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod.h"
#include "gamez/zvideo/zvid_format.h"

namespace ztwod {

namespace {
constexpr const char* kFile = "C:\\zipper\\GameZMech3\\zTwoD\\twod_dialog.cpp";
// 0x007319b0: read_labels' results, shared by every call. The original has no
// bound; 64 is more than any shipped LABEL holds.
Text* g_labels[64];
}  // namespace

// Mech3.exe 1.2 @ 0x0059cd90
const char* name_and_offset(zrdr::Node* n, int32_t* offset) {
  if (!n) return nullptr;
  zrdr::Node* l = n->list;
  if (l[0].i <= 1 || l[1].type != zrdr::String) return nullptr;
  const char* s = l[1].s;
  if (offset) zrdr::get_ivec2(n, "offset", offset);
  return s;
}

// Mech3.exe 1.2 @ 0x0059ca90
// FLASH (RATE period COLOR (r g b)): every label flashes to the colour.
void read_flash(zrdr::Node* value, std::vector<Text*>* labels) {
  float rate = 0.0f;
  int32_t c[3] = {0, 0, 0};
  zrdr::get_float(value, "RATE", &rate);
  zrdr::get_ivec3(value, "COLOR", c);
  const uint32_t color = ((static_cast<uint32_t>(c[2]) & 0xff) << 8 | (static_cast<uint32_t>(c[1]) & 0xff)) << 8 |
                         (static_cast<uint32_t>(c[0]) & 0xff);
  if (rate == 0.0f) return;
  for (Text* t : *labels) t->set_flash(color, rate);
}

// Mech3.exe 1.2 @ 0x0059cb30
// LABEL ("MSG_KEY" [x y | "offset" (x y)] ["font" ("name") | font number]),
// or a list of them. Each becomes a Text at the base plus its offset.
Text** read_labels(Dialog* d, zrdr::Node* value, int32_t* count, const int32_t* base) {
  if (!value) return nullptr;  // the original reads through null here
  zrdr::Node* l = value->list;
  const int32_t n = l[0].i;
  if (l[1].type == zrdr::List) {
    for (int32_t i = 1; i < n; ++i) read_labels(d, &l[i], count, base);
    return g_labels;
  }
  if (n <= 1) return g_labels;
  int32_t bx = base[0], by = base[1];
  Text* t = new Text();
  t->flags = ((t->flags >> 4) & 1) << 4 | kManaged;
  if (l[1].type == zrdr::String) {
    t->set_text(host().message(l[1].s));  // the message is the format (as written)
  } else {
    t->set_text("");
  }
  if (n > 2 && l[2].type == zrdr::Int && l[3].type == zrdr::Int) {
    bx += l[2].i;
    by += l[3].i;
  } else {
    int32_t off[2] = {0, 0};
    zrdr::get_ivec2(value, "offset", off);
    bx += off[0];
    by += off[1];
  }
  t->set_pos(bx, by);
  if (const char* fname = zrdr::get_string(value, "font")) {
    t->set_font(host().font_find(fname));
  } else if (n > 4 && l[4].type == zrdr::Int) {
    FontEntry& f = d->fonts[l[4].i];
    if (f.used) {
      t->align = f.align;
      t->create_font(f.face, f.height, f.weight, 0, 0, 0, 2);
      if (f.color != t->color_top || f.color != t->color_bottom) {
        t->color_top = f.color;
        t->color_bottom = f.color;
        t->needs_render = 1;
      }
      t->shadow_dx = 1;
      t->shadow = f.shadow;
      t->shadow_dy = 1;
      t->bk_mode = f.mode;
      t->bk_color = f.color2;
    }
  }
  if (*count < 64) g_labels[*count] = t;
  ++*count;
  return g_labels;
}

// Mech3.exe 1.2 @ 0x0059ec40
// The constructor also looks up "VMode" (0x00597d10) and drops the result.
Dialog::Dialog() : Window(1, 1) {}

// Mech3.exe 1.2 @ 0x0059edc0
Dialog::~Dialog() {
  if (behind) behind = host().image_release(behind);
  if (static_bg) static_bg = host().image_release(static_bg);
}

// Mech3.exe 1.2 @ 0x0059eeb0
void Dialog::set_static_bg(zimg::Texture* t) {
  if (static_bg) host().image_release(static_bg);
  static_bg = t;
}

// Mech3.exe 1.2 @ 0x0059eed0
void Dialog::set_active(int32_t on) {
  if (!on) {
    if (active) {
      stop_sounds(0);
      Window::set_active(on);
    }
    return;
  }
  if (active) return;
  play_sounds(sound_next);
  dirty_all();
  Window::set_active(on);
}

// Mech3.exe 1.2 @ 0x0059ef20
void Dialog::stop_sounds(int32_t stop_cd) {
  for (DialogSound& s : sounds) {
    if (s.playing) {
      host().sound_stop(s.playing);
      s.playing = 0;
    }
  }
  if (stop_cd) host().cd_stop();
}

// Mech3.exe 1.2 @ 0x0059ef60
// BACKGROUND_SOUNDS from `from` on until one starts; the CD stops unless a
// track was started.
void Dialog::play_sounds(int32_t from) {
  bool started = false, cd = false;
  if (mute) return;
  for (int32_t i = from;; ++i) {
    const int32_t n = sound_count < 10 ? sound_count : 10;
    if (i >= n || started) break;
    DialogSound& s = sounds[i];
    if (s.cd && s.id) {
      if (host().cd_play(s.id, 5)) {
        started = true;
        cd = true;
      }
      continue;
    }
    if (s.id) {
      s.playing = host().sound_play(s.id, s.volume);
      if (s.playing) started = true;
    }
  }
  if (!cd) host().cd_stop();
}

// Mech3.exe 1.2 @ 0x0059f010
bool Dialog::load(const char* file, const char* name, bool no_draw) {
  tree = zrdr::load(file);
  const bool ok = load_from(tree, name, no_draw);
  if (!ok) unload();
  return ok;
}

// Mech3.exe 1.2 @ 0x0059f050
bool Dialog::load_from(zrdr::Node* t, const char* name, bool no_draw) {
  return load_node(t, zrdr::find(t, name), no_draw);
}

// Mech3.exe 1.2 @ 0x0059f080
// SHARED_IMAGE_PATH (in the file), IMAGE_PATH, FONTS, BACKGROUND_IMAGES,
// _VIDEOS, _TEXT and _SOUNDS, CURSOR. Unless no_draw, the static parts are drawn
// once, with the screen grabbed before (behind) and after (static_bg).
bool Dialog::load_node(zrdr::Node* t, zrdr::Node* entry, bool no_draw) {
  if (!t || !entry) return false;
  node = entry;
  host().video_lock();
  if (!no_draw) behind = host().screen_grab(1);
  if (zrdr::Node* p = zrdr::find(t, "SHARED_IMAGE_PATH")) host().image_path(p->list[1].s);
  if (zrdr::Node* p = zrdr::find(node, "IMAGE_PATH")) host().image_path(p->list[1].s);

  if (zrdr::Node* fl = zrdr::find(node, "FONTS")) {
    int32_t n = fl->list[0].i;
    if (n > 0x13) n = 0x14;
    for (int32_t i = 1; i < n; ++i) {
      zrdr::Node* e = fl->list[i].list;  // (id "face" height colour [shadow] [weight] [align])
      FontEntry& f = fonts[e[1].i];
      f.used = 1;
      f.color2 = 0;
      f.mode = 1;
      f.face = e[2].s;
      f.height = e[3].i;
      zrdr::Node* c = &e[4];
      if (c->list[1].type == zrdr::List) {  // ((r g b) (r g b)): text and background
        zrdr::Node* a = c->list[1].list;
        f.color = (static_cast<uint32_t>(a[3].i & 0xff) << 8 | static_cast<uint32_t>(a[2].i & 0xff)) << 8 |
                  static_cast<uint32_t>(a[1].i & 0xff);
        zrdr::Node* b = c->list[2].list;
        f.color2 = (static_cast<uint32_t>(b[3].i & 0xff) << 8 | static_cast<uint32_t>(b[2].i & 0xff)) << 8 |
                   static_cast<uint32_t>(b[1].i & 0xff);
        f.mode = 2;
      } else {
        zrdr::Node* a = c->list;
        f.color = (static_cast<uint32_t>(a[3].i & 0xff) << 8 | static_cast<uint32_t>(a[2].i & 0xff)) << 8 |
                  static_cast<uint32_t>(a[1].i & 0xff);
      }
      const int32_t count = e[0].i;
      if (count >= 6) f.shadow = e[5].i;
      if (count >= 7) f.weight = e[6].i;
      if (count >= 8) {
        const char* a = e[7].s;
        if (std::strcmp(a, "LEFT") == 0) {
          f.align = 0;
        } else if (std::strcmp(a, "RIGHT") == 0) {
          f.align = 2;
        } else if (std::strcmp(a, "CENTER") == 0) {
          f.align = 1;
        }  // anything else leaves the alignment as it was
      } else {
        f.align = 0;
      }
    }
  }

  if (zrdr::Node* bi = zrdr::find(node, "BACKGROUND_IMAGES")) {
    int32_t n = bi->list[0].i;
    if (n >= 0x14) n = 0x14;
    for (int32_t i = 1; i < n; ++i) {
      Image& im = images[i - 1];
      const int32_t ox = origin_x, oy = origin_y;
      zrdr::Node* e = bi->list[i].list;  // ("name" x y)
      im.set_image(e[1].s);
      if (e[0].i >= 4) im.set_pos(e[2].i + ox, e[3].i + oy);
      im.flags = ((im.flags >> 4) & 1) << 4 | kManaged;
      im.show_for(-1.0f);
      im.dirty();
      add(&im);
    }
  }

  if (zrdr::Node* bv = zrdr::find(node, "BACKGROUND_VIDEOS")) {
    int32_t n = bv->list[0].i;
    if (n >= 0xa) n = 0xa;
    for (int32_t i = 1; i < n; ++i) {
      Movie2D& mv = movies[i - 1];
      const int32_t ox = origin_x, oy = origin_y;
      zrdr::Node* e = bv->list[i].list;  // ("name" x y [(r g b) colour key])
      mv.open(e[1].s);
      if (e[0].i >= 4) mv.set_pos(e[2].i + ox, e[3].i + oy);
      if (e[0].i >= 5) {
        zrdr::Node* c = e[4].list;
        mv.set_color_key(zvid::pack_bytes(static_cast<uint8_t>(c[1].i), static_cast<uint8_t>(c[2].i),
                                   static_cast<uint8_t>(c[3].i)));
      }
      mv.show_for(-1.0f);
      mv.dirty();
      mv.get_x();
      mv.get_y();
      mv.get_x();
      mv.get_y();
      mv.set_background(behind, nullptr);
      mv.start();
      add(&mv);
    }
  }

  if (zrdr::Node* bt = zrdr::find(node, "BACKGROUND_TEXT")) {
    int32_t n = bt->list[0].i;
    if (n >= 0x32) n = 0x32;
    for (int32_t i = 1; i < n; ++i) {
      Text& tx = texts[i - 1];
      zrdr::Node* e = bt->list[i].list;  // ("MSG_KEY" x y font)
      tx.set_text(host().message(e[1].s));
      tx.set_pos(e[2].i + origin_x, e[3].i + origin_y);
      FontEntry& f = fonts[e[4].i];  // read whether or not the entry has a font number
      if (f.used) {
        tx.align = f.align;
        tx.create_font(f.face, f.height, f.weight, 0, 0, 0, 2);
        if (f.color != tx.color_top || f.color != tx.color_bottom) {
          tx.color_top = f.color;
          tx.color_bottom = f.color;
          tx.needs_render = 1;
        }
        tx.shadow_dx = 1;
        tx.shadow = f.shadow;
        tx.shadow_dy = 1;
        tx.bk_mode = f.mode;
        tx.bk_color = f.color2;
      }
      tx.show_for(-1.0f);
      add(&tx);
    }
  }

  if (zrdr::Node* bs = zrdr::find(node, "BACKGROUND_SOUNDS")) {
    int32_t n = bs->list[0].i;
    if (n >= 0xa) n = 0xa;
    sound_count = n;
    sound_next = 0;
    mute = 0;
    for (int32_t i = 1; i < sound_count; ++i) {
      DialogSound& s = sounds[i - 1];
      zrdr::Node* e = bs->list[i].list;  // ("name" [volume])
      float vol = 1.0f;
      const char* name = e[1].s;
      if (e[0].i >= 3) vol = e[2].f;
      if (const int32_t track = host().cd_find(name)) {
        s.cd = 1;
        s.id = track;
      } else {
        s.cd = 0;
        s.id = host().sound_find(name);
        s.volume = vol;
      }
    }
  }

  if (!no_draw) {
    const int32_t was = mute;
    mute = 1;
    set_active(1);
    update(0.0f);
    static_bg = host().screen_grab(1);
    set_active(0);
    restore_behind();
    mute = was;
  }

  if (zrdr::Node* cu = zrdr::find(node, "CURSOR")) {
    if (zrdr::Node* b = zrdr::find(cu, "BITMAP")) {
      cursor_obj.set_cursor_image(name_and_offset(b, nullptr));
      set_cursor(&cursor_obj);
    }
    if (zrdr::Node* c = zrdr::find(cu, "CENTER")) cursor_obj.center = c->list[1].i;
    int32_t capture = 1;
    zrdr::get_int(cu, "CAPTURE", &capture);
    cursor_obj.set_capture(capture);
  }
  host().video_unlock();
  return true;
}

// Mech3.exe 1.2 @ 0x0059f810
bool bind_button(Dialog* d, zrdr::Node* entry, Control* c, const char* name) {
  if (!entry) {
    zerr::reportf(0x200, kFile, 0x730, "BindButton (%s) failed", name);
    return false;
  }
  zrdr::Node* n = zrdr::find(zrdr::find(entry, "BUTTONS"), name);
  if (!n) {
    zerr::reportf(0x200, kFile, 0x73c, "BindButton (%s) failed", name);
    return false;
  }
  c->read_control(n, d);
  c->after_bind();
  return true;
}

// Mech3.exe 1.2 @ 0x0059f8b0
bool Dialog::bind_button(Control* c, const char* name) { return ztwod::bind_button(this, node, c, name); }

// Mech3.exe 1.2 @ 0x0059f8d0
// PRIMITIVES entry: the object reads itself; MOUSETRAP (l t r b) is written over
// the four dwords at +0x34 of whatever object it is (the trap classes keep a
// rectangle there); its background is the static one with an empty rectangle.
bool Dialog::bind_primitive(Object2D* o, const char* name) {
  if (!node) return false;
  zrdr::Node* p = zrdr::find(node, "PRIMITIVES");
  zrdr::Node* n = p ? zrdr::find(p, name) : nullptr;
  if (!n) {
    zerr::reportf(0x200, kFile, 0x752, "BindPrimitive (%s) failed", name);
    return false;
  }
  add(o);
  const bool ok = o->read(n, this);
  if (zrdr::Node* m = zrdr::find(n, "MOUSETRAP")) {
    Rect r;
    r.left = m->list[1].i + origin_x;
    r.top = m->list[2].i + origin_y;
    r.right = m->list[3].i + origin_x;
    r.bottom = m->list[4].i + origin_y;
    o->set_trap_rect(r);
  }
  Rect r;
  r.left = o->get_x();
  r.top = o->get_y();
  r.right = o->get_x();
  r.bottom = o->get_y();
  o->set_background(static_bg, &r);
  o->flags = ((o->flags >> 4) & 1) << 4 | kManaged;
  return ok;
}

// Mech3.exe 1.2 @ 0x0059fa10
void Dialog::unload() {
  if (tree) zrdr::free_node(tree);
  node = nullptr;
  tree = nullptr;
}

// Mech3.exe 1.2 @ 0x0059fa40
void Dialog::restore_behind() {
  if (behind) host().blit(behind, 0, 0, 0, nullptr);
}

}  // namespace ztwod
