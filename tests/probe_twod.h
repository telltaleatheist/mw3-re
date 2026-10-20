// Shared by the zTwoD probes (probe_twod.cpp, probe_mainmenu.cpp; docs/twod.md,
// Testing): the recording Host every call out of zTwoD goes through, the
// textures the stubs handed out, and the dump of a dialog and its objects that
// test_twod.py builds the same way from the emulator's memory.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zimage/zimg_library.h"
#include "gamez/ztwod/twod.h"
#include "probe_app.h"

namespace probe_twod {
using probe_app::call;
using ztwod::Handle;
using ztwod::Rect;


// Textures the stubs handed out, by their emulator address, and back.
inline std::map<uint32_t, zimg::Texture*> g_tex;
inline std::map<const zimg::Texture*, uint32_t> g_tex_id;
inline std::map<const zimg::Texture*, uint32_t> g_own_id;  // made by the port: 0xe0000000 + n
inline uint32_t g_own_next = 0;
inline std::deque<std::string> g_strings;

inline zimg::Texture* tex(uint32_t a) {
  if (!a) return nullptr;
  auto it = g_tex.find(a);
  if (it != g_tex.end()) return it->second;
  auto* t = static_cast<zimg::Texture*>(std::calloc(1, sizeof(zimg::Texture)));
  const uint32_t wh = call("peek32", {a + 4});
  t->width = static_cast<int16_t>(wh & 0xffff);
  t->height = static_cast<int16_t>(wh >> 16);
  g_tex[a] = t;
  g_tex_id[t] = a;
  return t;
}
inline uint32_t id(const zimg::Texture* t) {
  if (!t) return 0;
  auto it = g_tex_id.find(t);
  if (it != g_tex_id.end()) return it->second;
  auto o = g_own_id.find(t);
  if (o != g_own_id.end()) return o->second;
  const uint32_t n = 0xe0000000u + g_own_next++;
  g_own_id[t] = n;
  return n;
}

inline const char* keep(const std::string& s) {
  g_strings.push_back(s);
  if (g_strings.size() > 4096) g_strings.pop_front();
  return g_strings.back().c_str();
}

// The fake GDI both sides share (test_twod.py): 7 pixels a character, 13 lines
// plus the length mod 3.
inline int32_t text_w(int32_t len) { return 7 * len; }
inline int32_t text_h(int32_t len) { return 13 + len % 3; }

inline std::vector<ztwod::MouseState> g_mouse;
inline size_t g_frame = 0;
inline size_t g_polls = 0;

struct RecordingHost : ztwod::Host {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  zimg::Texture* image_get(const char* name) override { return tex(call("0x005179d0", {name})); }
  zimg::Texture* image_release(zimg::Texture* t) override { return tex(call("0x00517b10", {id(t)})); }
  void texture_free(zimg::Texture* t) override { call("0x00517cc0", {id(t)}); }
  Handle font_find(const char* name) override { return call("0x00517680", {name}); }
  ztwod::NamedFont font_fields(Handle f) override {
    ztwod::NamedFont n;
    n.hfont = call("peek32", {f + 0x44});
    n.color_top = call("peek32", {f + 0x48});
    n.color_bottom = call("peek32", {f + 0x4c});
    n.align = static_cast<int32_t>(call("peek32", {f + 0x50}));
    n.shadow = static_cast<uint8_t>(call("peek8", {f + 0x54}));
    return n;
  }
  void font_measure(const char* text, Handle font, int32_t* w, int32_t* h) override {
    call("0x0051a0d0", {text, font});
    const int32_t len = static_cast<int32_t>(std::strlen(text));
    *w = text_w(len);
    *h = text_h(len);
  }
  zimg::Texture* screen_grab(int32_t buffer) override { return tex(call("0x00579500", {buffer})); }
  zimg::Texture* screen_grab_rect(int32_t buffer, Rect* r, zimg::Texture* into) override {
    const uint32_t a = call("0x00579660", {buffer, r->left, r->top, r->right, r->bottom, id(into)});
    if (r->left < 0) r->left = 0;
    if (r->top < 0) r->top = 0;
    if (r->right > 640) r->right = 640;
    if (r->bottom > 480) r->bottom = 480;
    return a ? into : nullptr;
  }
  void blit(zimg::Texture* image, int32_t x, int32_t y, int32_t flags, const Rect* src) override {
    if (src) {
      call("0x0055b1f0", {id(image), x, y, flags, src->left, src->top, src->right, src->bottom});
    } else {
      call("0x0055b1f0", {id(image), x, y, flags});
    }
  }
  std::map<Handle, zimg::Texture*> dc_image;
  std::map<Handle, uint32_t> dc_color;
  Handle image_dc(zimg::Texture* image) override {
    const Handle dc = call("[0x0072f394]", {id(image)});
    dc_image[dc] = image;
    return dc;
  }
  void SetTextColor(Handle dc, uint32_t color) override {
    call("SetTextColor", {dc, color});
    dc_color[dc] = color;
  }
  void image_dc_release(zimg::Texture* image, Handle dc) override { call("[0x0072f398]", {id(image), dc}); }
  const char* message(const char* key) override {
    const uint32_t a = call("0x00575fb0", {key});
    return a ? keep(std::string("T:") + key) : nullptr;
  }
  ztwod::MouseState mouse() override {
    call("0x0051b7b0", {});
    return g_frame < g_mouse.size() ? g_mouse[g_frame] : ztwod::MouseState{};
  }
  // The message box's loop polls once a frame; the frame's mouse follows.
  void input_poll(uint8_t a) override {
    call("0x0051b360", {a});
    g_frame = g_polls++;
  }
  ztwod::Rect screen_rect() override {
    const uint32_t a = call("0x00579880", {});
    ztwod::Rect r;
    r.left = static_cast<int32_t>(call("peek32", {a}));
    r.top = static_cast<int32_t>(call("peek32", {a + 4}));
    r.right = static_cast<int32_t>(call("peek32", {a + 8}));
    r.bottom = static_cast<int32_t>(call("peek32", {a + 12}));
    return r;
  }
  Handle render_target(int32_t* w, int32_t* h, int32_t* depth, int32_t* pitch) override {
    const Handle base = call("0x00558080", {});
    *w = 640;  // the fake target (test_twod.py)
    *h = 480;
    *depth = 16;
    *pitch = 1280;
    return base;
  }
  void crt_free(void* p) override { call("@free", {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))}); }
  void ripple(int32_t x, int32_t y, int32_t inner, int32_t outer, int32_t amplitude, float wavelength, float phase,
              const Rect* clip) override {
    call("0x00558dd0", {x, y, inner, outer, amplitude, zgame::bits(wavelength), zgame::bits(phase), clip ? 1 : 0});
  }
  int32_t GetTextExtentPoint32A(Handle dc, const char* text, int32_t length, int32_t* cx, int32_t* cy) override {
    const uint32_t ok = call("GetTextExtentPoint32A", {dc, text, length});
    *cx = text_w(length);
    *cy = text_h(length);
    return static_cast<int32_t>(ok);
  }
  int32_t DrawTextA(Handle dc, const char* text, int32_t length, Rect* r, uint32_t format) override {
    const uint32_t ok = call("DrawTextA", {dc, text, length, r->left, r->top, r->right, r->bottom, format});
    if (format & 0x400) {
      const int32_t len = static_cast<int32_t>(std::strlen(text));
      r->right = r->left + text_w(len);
      r->bottom = r->top + text_h(len);
    } else if (dc_image.count(dc)) {  // the fake glyphs (twod_world.py, paint)
      zimg::Texture* t = dc_image[dc];
      const uint32_t c = dc_color.count(dc) ? dc_color[dc] : 0;
      const uint16_t p = static_cast<uint16_t>(((c & 0xf8) << 8) | (((c >> 8) & 0xfc) << 3) | (((c >> 16) & 0xff) >> 3));
      for (int32_t y = r->top < 0 ? 0 : r->top; y < (r->bottom < t->height ? r->bottom : t->height); ++y)
        for (int32_t x = r->left < 0 ? 0 : r->left; x < (r->right < t->width ? r->right : t->width); ++x)
          if ((x + y) % 3) static_cast<uint16_t*>(t->pixels)[y * t->width + x] = p;
    }
    return static_cast<int32_t>(ok);
  }
  int32_t GetTextMetricsA(Handle dc, ztwod::TextMetrics* tm) override {
    const uint32_t ok = call("GetTextMetricsA", {dc});
    tm->height = 13;
    tm->ascent = 11;
    tm->internal_leading = 2;
    tm->external_leading = 1;
    return static_cast<int32_t>(ok);
  }
};

// ---- the dump (test_twod.py builds the same from the emulator) ----

struct Dump {
  std::ostringstream o;
  std::vector<ztwod::Object2D*> objs;
  int index(const ztwod::Object2D* p) const {
    for (size_t i = 0; i < objs.size(); ++i)
      if (objs[i] == p) return static_cast<int>(i);
    return -1;
  }
  void kv(const char* k, int64_t v) { o << k << '=' << std::hex << static_cast<uint32_t>(v) << std::dec << ' '; }
  void f(const char* k, float v) {
    uint32_t b;
    std::memcpy(&b, &v, 4);
    kv(k, b);
  }
  void t(const char* k, const zimg::Texture* p) { o << k << "=T" << std::hex << id(p) << std::dec << ' '; }
  void s(const char* k, const char* v) {
    o << k << "=\"";
    for (const char* c = v; c && *c; ++c) o << (*c == ' ' ? '_' : *c);
    o << "\" ";
  }
  void r(const char* k, const Rect& v) {
    o << k << '=' << v.left << ',' << v.top << ',' << v.right << ',' << v.bottom << ' ';
  }
  void ref(const char* k, const ztwod::Object2D* p) { o << k << "=o" << index(p) << ' '; }
  void labels(const char* k, const std::vector<ztwod::Text*>& v) {
    o << k << '=';
    for (ztwod::Text* x : v) o << 'o' << index(x) << ',';
    o << ' ';
  }

  void base(ztwod::Object2D* p) {
    ref("next", p->next);
    kv("flags", p->flags);
    f("timer", p->timer);
    kv("x", p->x);
    kv("y", p->y);
    t("bg", p->background);
    r("bgr", p->bg_rect);
    kv("mouse", p->mouse);
  }
  void image(ztwod::Image* p) {
    base(p);
    kv("owned", p->owned);
    kv("queued", p->queued);
    t("img", p->image);
    kv("u40", p->u40);
    if (p->src) r("src", *p->src); else o << "src=0 ";
    kv("center", p->center);
  }
  void text(ztwod::Text* p) {
    base(p);
    s("text", p->text);
    kv("font", p->font);
    kv("centered", p->centered);
    kv("align", p->align);
    t("timg", p->image);
    if (p->image && !g_tex_id.count(p->image)) {
      uint32_t hsh = 2166136261u;
      const uint16_t* px = static_cast<const uint16_t*>(p->image->pixels);
      for (int32_t i = 0; i < p->image->width * p->image->height; ++i) hsh = (hsh ^ px[i]) * 16777619u;
      kv("tw", p->image->width);
      kv("th", p->image->height);
      kv("tflags", p->image->flags);
      kv("tpix", hsh);
    }
    kv("top", p->color_top);
    kv("bottom", p->color_bottom);
    kv("shcol", p->shadow_color);
    kv("hfont", p->hfont);
    s("rendered", p->rendered);
    kv("w", p->width);
    kv("h", p->height);
    kv("shadow", p->shadow);
    kv("bk", p->bk_mode);
    if (p->bk_mode == 2) kv("bkc", p->bk_color);  // else unset (stack leftovers in a text box's lines)
    kv("nr", p->needs_render);
    kv("lead", p->leading);
    kv("fixed", p->fixed);
    r("fr", p->fixed_rect);
    r("tr", p->text_rect);
    kv("sdx", p->shadow_dx);
    kv("sdy", p->shadow_dy);
    kv("maxw", p->max_width);
    kv("named", p->named);
    f("ft", p->flash_timer);
    f("frate", p->flash_rate);
    kv("atop", p->alt_top);
    kv("fon", p->flash_on);
    kv("fmode", p->flash_mode);
    kv("phase", p->phase);
  }
  void control(ztwod::Control* p) {
    image(p);
    kv("bx", p->base_x);
    kv("by", p->base_y);
    kv("en", p->enabled);
    r("bounds", p->bounds);
    t("inorm", p->img_normal);
    t("idis", p->img_disabled);
    t("iroll", p->img_rollover);
    kv("sroll", p->snd_rollover);
    kv("proll", p->rollover_playing);
    f("vroll", p->vol_rollover);
    t("iact", p->img_activate);
    kv("sact", p->snd_activate);
    kv("pact", p->activate_playing);
    f("vact", p->vol_activate);
    kv("sdis", p->snd_disable);
    labels("L", p->labels);
    labels("LR", p->rollover_labels);
    labels("LA", p->activate_labels);
    labels("LD", p->disable_labels);
  }
  void slider(ztwod::Slider* p) {
    control(p);
    f("value", p->value);
    t("fill", p->fill);
    r("fsrc", p->fill_src);
    kv("fdx", p->fill_dx);
    kv("fdy", p->fill_dy);
    t("empty", p->empty);
    r("esrc", p->empty_src);
    kv("edx", p->empty_dx);
    kv("edy", p->empty_dy);
  }
  void fonttext(ztwod::FontText* p) {
    base(p);
    s("text", p->text);
    kv("align", p->align);
    t("timg", p->image);
    kv("top", p->color_top);
    kv("bottom", p->color_bottom);
    kv("hfont", p->hfont);
    s("rendered", p->rendered);
    kv("w", p->width);
    kv("h", p->height);
    kv("shadow", p->shadow);
    kv("bk", p->bk_mode);
    kv("nr", p->needs_render);
    r("tr", p->text_rect);
    kv("maxw", p->max_width);
    kv("named", p->named);
  }
  void item(ztwod::ListItem* p) {
    fonttext(p);
    kv("index", p->index);
    o << "box=" << (p->box ? 1 : 0) << ' ';
  }
  void textbox(ztwod::TextBox* p) {
    fonttext(p);
    kv("current", p->current);
    kv("bup", p->bottom_up);
    kv("repos", p->reposition);
    kv("nlines", static_cast<int64_t>(p->lines.size()));
    for (size_t i = 0; i < p->lines.size(); ++i) {
      o << "\n  line" << i << ' ';
      text(&p->lines[i]);
    }
  }
  void listbox(ztwod::ListBox* p) {
    check(p);
    kv("count", p->count);
    kv("before", p->before);
    kv("spacing", p->spacing);
    kv("first", p->first);
    kv("nstr", static_cast<int64_t>(p->strings.size()));
    f("bx0", p->before_x);
    f("by0", p->before_y);
    f("bx1", p->after_x);
    f("by1", p->after_y);
    ref("shown", &p->shown);
  }
  void check(ztwod::CheckButton* p) {
    control(p);
    ref("ctext", p->text);
    kv("checked", p->checked);
    t("idc", p->img_disabled_checked);
    t("idu", p->img_disabled_unchecked);
    t("iun", p->img_unchecked);
    t("ich", p->img_checked);
  }
};

inline std::map<ztwod::Object2D*, char> g_kind;  // 'I' image, 'T' text, 'C' control, 'K' check, 'U' cursor

inline void dump_dialog(ztwod::Dialog& d, Dump& out) {
  for (ztwod::Object2D* p = d.head; p; p = p->next) out.objs.push_back(p);
  out.o << "dialog ";
  out.kv("active", d.active);
  out.kv("cursor", d.cursor ? 1 : 0);
  out.t("behind", d.behind);
  out.t("static", d.static_bg);
  out.kv("ox", d.origin_x);
  out.kv("oy", d.origin_y);
  out.kv("mute", d.mute);
  out.kv("nsnd", d.sound_count);
  out.kv("snext", d.sound_next);
  out.kv("mx", d.m.x);
  out.kv("my", d.m.y);
  out.kv("ml", d.m.left);
  out.kv("mr", d.m.right);
  out.o << '\n';
  for (int i = 0; i < 10; ++i) {
    const ztwod::DialogSound& s = d.sounds[i];
    out.o << "sound" << i << ' ';
    out.kv("cd", s.cd);
    out.kv("id", s.id);
    out.kv("playing", s.playing);
    out.f("vol", s.volume);
    out.o << '\n';
  }
  for (int i = 0; i < 20; ++i) {
    const ztwod::FontEntry& f = d.fonts[i];
    out.o << "font" << i << ' ';
    out.kv("used", f.used);
    if (f.used) {
      out.s("face", f.face);
      out.kv("height", f.height);
      out.kv("color", f.color);
      out.kv("color2", f.color2);
      out.kv("mode", f.mode);
      out.kv("shadow", f.shadow);
      out.kv("align", f.align);
    }
    out.kv("weight", f.weight);
    out.o << '\n';
  }
  out.o << "cursorobj ";
  out.image(&d.cursor_obj);
  out.t("under", d.cursor_obj.under);
  out.kv("capture", d.cursor_obj.capture);
  out.o << '\n';
  for (size_t i = 0; i < out.objs.size(); ++i) {
    ztwod::Object2D* p = out.objs[i];
    out.o << 'o' << i << ' ';
    char k = 'I';
    if (p == &d.cursor_obj) k = 'U';
    else if (g_kind.count(p)) k = g_kind[p];
    else if (dynamic_cast<ztwod::ListItem*>(p)) k = 'Y';
    else if (dynamic_cast<ztwod::Text*>(p)) k = 'T';
    else if (dynamic_cast<ztwod::CheckButton*>(p)) k = 'K';
    else if (dynamic_cast<ztwod::Control*>(p)) k = 'C';
    out.o << k << ' ';
    switch (k) {
      case 'T': out.text(static_cast<ztwod::Text*>(p)); break;
      case 'C': out.control(static_cast<ztwod::Control*>(p)); break;
      case 'K': out.check(static_cast<ztwod::CheckButton*>(p)); break;
      case 'L': out.slider(static_cast<ztwod::Slider*>(p)); break;
      case 'X': out.listbox(static_cast<ztwod::ListBox*>(p)); break;
      case 'Y': out.item(static_cast<ztwod::ListItem*>(p)); break;
      case 'Z': out.textbox(static_cast<ztwod::TextBox*>(p)); break;
      default: out.image(static_cast<ztwod::Image*>(p)); break;
    }
    out.o << '\n';
  }
}

// A fresh start for the texture ids and the mouse.
inline void reset() {
  g_tex.clear();
  g_tex_id.clear();
  g_own_id.clear();
  g_own_next = 0;
  g_kind.clear();
  g_mouse.clear();
  g_frame = 0;
  g_polls = 0;
}

}  // namespace probe_twod
