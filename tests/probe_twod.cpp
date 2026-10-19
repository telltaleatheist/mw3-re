// Probe for test_twod.py (docs/twod.md, Testing): a zTwoD dialog loaded from a
// shipped .zrd, its buttons and primitives bound, then frames of mouse input,
// every call out of zTwoD recorded through the calltrace callback; the final
// state of every object dumped as text the Python side builds the same way
// from the emulator's memory.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod.h"
#include "probe_app.h"

namespace {
using probe_app::call;
using ztwod::Handle;
using ztwod::Rect;

// Textures the stubs handed out, by their emulator address, and back.
std::map<uint32_t, zimg::Texture*> g_tex;
std::map<const zimg::Texture*, uint32_t> g_tex_id;
std::map<const zimg::Texture*, uint32_t> g_own_id;  // made by the port: 0xe0000000 + n
uint32_t g_own_next = 0;
std::deque<std::string> g_strings;

zimg::Texture* tex(uint32_t a) {
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
uint32_t id(const zimg::Texture* t) {
  if (!t) return 0;
  auto it = g_tex_id.find(t);
  if (it != g_tex_id.end()) return it->second;
  auto o = g_own_id.find(t);
  if (o != g_own_id.end()) return o->second;
  const uint32_t n = 0xe0000000u + g_own_next++;
  g_own_id[t] = n;
  return n;
}

const char* keep(const std::string& s) {
  g_strings.push_back(s);
  if (g_strings.size() > 4096) g_strings.pop_front();
  return g_strings.back().c_str();
}

// The fake GDI both sides share (test_twod.py): 7 pixels a character, 13 lines
// plus the length mod 3.
int32_t text_w(int32_t len) { return 7 * len; }
int32_t text_h(int32_t len) { return 13 + len % 3; }

std::vector<ztwod::MouseState> g_mouse;
size_t g_frame = 0;

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
    kv("bkc", p->bk_color);
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

std::map<ztwod::Object2D*, char> g_kind;  // 'I' image, 'T' text, 'C' control, 'K' check, 'U' cursor

void dump_dialog(ztwod::Dialog& d, Dump& out) {
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
    else if (dynamic_cast<ztwod::Text*>(p)) k = 'T';
    else if (dynamic_cast<ztwod::CheckButton*>(p)) k = 'K';
    else if (dynamic_cast<ztwod::Control*>(p)) k = 'C';
    out.o << k << ' ';
    switch (k) {
      case 'T': out.text(static_cast<ztwod::Text*>(p)); break;
      case 'C': out.control(static_cast<ztwod::Control*>(p)); break;
      case 'K': out.check(static_cast<ztwod::CheckButton*>(p)); break;
      default: out.image(static_cast<ztwod::Image*>(p)); break;
    }
    out.o << '\n';
  }
}

std::vector<ztwod::Object2D*> g_bound;

}  // namespace

extern "C" {

// Loads `name` from the .zrd at `offset` in `path`, then runs `plan` (lines, as
// test_twod.py writes them). Returns the dump's length, or -1.
int probe_twod_run(const char* path, long offset, const char* name, int no_draw, const char* plan, char* out,
                   int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  g_tex.clear();
  g_tex_id.clear();
  g_own_id.clear();
  g_own_next = 0;
  g_kind.clear();
  g_bound.clear();
  g_mouse.clear();
  g_frame = 0;
  zrdr::Node root{};
  {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return -1;
    std::fseek(f, offset, SEEK_SET);
    zrdr::read_node(f, &root);
    std::fclose(f);
  }
  ztwod::set_page_flip(true);
  auto dialog = std::make_unique<ztwod::Dialog>();
  dialog->load_from(&root, name, no_draw != 0);
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    char op;
    ls >> op;
    if (op == 'B' || op == 'P') {
      std::string n;
      char kind;
      ls >> n >> kind;
      ztwod::Object2D* o;
      if (kind == 'K') o = new ztwod::CheckButton();
      else if (kind == 'C') o = new ztwod::Control();
      else if (kind == 'T') o = new ztwod::Text();
      else o = new ztwod::Image(0);
      g_kind[o] = kind;
      g_bound.push_back(o);
      call("bind", {static_cast<uint32_t>(g_bound.size())});
      if (op == 'B') dialog->bind_button(static_cast<ztwod::Control*>(o), n.c_str());
      else dialog->bind_primitive(o, n.c_str());
    } else if (op == 'A') {
      int v;
      ls >> v;
      dialog->set_active(v);
    } else if (op == 'F') {
      uint32_t dt;
      ztwod::MouseState m{};
      uint32_t* w = reinterpret_cast<uint32_t*>(&m);
      ls >> std::hex >> dt;
      for (int i = 0; i < 15; ++i) ls >> w[i];
      g_mouse.push_back(m);
      float fdt;
      std::memcpy(&fdt, &dt, 4);
      dialog->update(fdt);
      ++g_frame;
    } else if (op == 'E') {
      size_t i;
      int v;
      ls >> i >> v;
      auto* c = static_cast<ztwod::Control*>(g_bound[i]);
      c->enabled = v;
      c->refresh();
    } else if (op == 'H') {
      size_t i;
      ls >> i;
      g_bound[i]->hide();
    } else if (op == 'S') {
      size_t i;
      uint32_t b;
      ls >> i >> std::hex >> b;
      float v;
      std::memcpy(&v, &b, 4);
      g_bound[i]->show_for(v);
    } else if (op == 'X') {
      size_t i;
      int v;
      ls >> i >> v;
      static_cast<ztwod::CheckButton*>(g_bound[i])->set_checked(v);
    } else if (op == 'M') {
      size_t i;
      ls >> i;
      static_cast<ztwod::Text*>(g_bound[i])->measure();
    } else if (op == 'Q') {
      size_t i;
      int v;
      ls >> i >> v;
      static_cast<ztwod::Text*>(g_bound[i])->set_text("V%d", v);
    } else if (op == 'L') {
      size_t i;
      uint32_t color, rate;
      ls >> i >> std::hex >> color >> rate;
      float r;
      std::memcpy(&r, &rate, 4);
      static_cast<ztwod::Text*>(g_bound[i])->set_flash(color, r);
    } else if (op == 'Z') {
      size_t i;
      ls >> i;
      static_cast<ztwod::Text*>(g_bound[i])->stop_flash();
    } else if (op == 'R') {  // a bound object's get_rect, recorded
      size_t i;
      ls >> i;
      ztwod::Rect* r = g_bound[i]->get_rect();
      if (r) call("rect", {r->left, r->top, r->right, r->bottom});
      else call("rect", {});
    }
  }
  Dump d;
  dump_dialog(*dialog, d);
  const std::string s = d.o.str();
  ztwod::set_host(nullptr);
  dialog.release();  // as the original's test leaves it: no teardown compared
  zrdr::free_contents(&root);
  if (static_cast<int>(s.size()) >= size) return -1;
  std::memcpy(out, s.data(), s.size() + 1);
  return static_cast<int>(s.size());
}

}  // extern "C"

#include "gamez/zrender/zrndr_blit.h"

extern "C" {

// zrndr::blit16 on buffers the caller owns (test_twod_blit.py).
void probe_blit16(uint8_t* target, uint32_t tw, uint32_t th, uint32_t pitch, int32_t format, void* pixels,
                  uint8_t* alpha, uint16_t* palette, int32_t w, int32_t h, int32_t stride, uint8_t flags,
                  int32_t x, int32_t y, uint16_t key, const int32_t* rect) {
  zrndr::Target2D& t = zrndr::target2d();
  t.base = target;
  t.width = tw;
  t.height = th;
  t.pitch = pitch;
  t.format = format;
  zimg::Texture img{};
  img.width = static_cast<int16_t>(w);
  img.height = static_cast<int16_t>(h);
  img.width_again = stride;
  img.flags = flags;
  img.pixels = pixels;
  img.alpha = alpha;
  img.palette = palette;
  zrndr::blit16(&img, x, y, key, reinterpret_cast<const zrndr::BlitRect*>(rect));
}

}  // extern "C"
