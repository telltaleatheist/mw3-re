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

#include "probe_twod.h"

namespace {
using namespace probe_twod;

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
      else if (kind == 'L') o = new ztwod::Slider();
      else if (kind == 'X') o = new ztwod::ListBox();
      else if (kind == 'Z') {
        uint32_t lines;
        ls >> lines;
        o = new ztwod::TextBox(lines);
      }
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
    } else if (op == 'V') {
      size_t i;
      uint32_t b;
      ls >> i >> std::hex >> b;
      float v;
      std::memcpy(&v, &b, 4);
      static_cast<ztwod::Slider*>(g_bound[i])->set_value(v);
    } else if (op == 'W') {
      size_t i;
      ls >> i;
      static_cast<ztwod::Slider*>(g_bound[i])->set_from_mouse();
    } else if (op == 'G') {
      size_t i;
      std::string str;
      ls >> i >> str;
      static_cast<ztwod::ListBox*>(g_bound[i])->strings.push_back(str);
    } else if (op == 'J') {
      size_t i;
      int v;
      ls >> i >> v;
      static_cast<ztwod::ListBox*>(g_bound[i])->select(v);
    } else if (op == 'N' || op == 'O') {
      size_t i;
      int v;
      ls >> i >> v;
      auto* tb = static_cast<ztwod::TextBox*>(g_bound[i]);
      if (op == 'N') tb->set_text("L%d", v);
      else tb->set_text_top("T%d", v);
    } else if (op == 'U') {
      size_t i;
      ls >> i;
      static_cast<ztwod::TextBox*>(g_bound[i])->new_line();
    } else if (op == 'C') {
      size_t i;
      uint32_t c;
      ls >> i >> std::hex >> c;
      static_cast<ztwod::TextBox*>(g_bound[i])->set_color(c);
    } else if (op == 'K') {
      size_t i;
      ls >> i;
      static_cast<ztwod::TextBox*>(g_bound[i])->clear();
    } else if (op == 'Y') {
      size_t i;
      uint32_t n;
      ls >> i >> n;
      static_cast<ztwod::TextBox*>(g_bound[i])->set_line_count(n);
    } else if (op == 'D') {
      size_t i;
      int v;
      ls >> i >> v;
      static_cast<ztwod::TextBox*>(g_bound[i])->bottom_up = static_cast<uint8_t>(v);
    } else if (op == 'a') {
      size_t i;
      std::string str;
      ls >> i >> str;
      static_cast<ztwod::TextBox*>(g_bound[i])->append(keep(str));
    } else if (op == 'T') {
      size_t i;
      ls >> i;
      auto* tb = static_cast<ztwod::TextBox*>(g_bound[i]);
      const int32_t th = tb->total_height();
      const int32_t mw = tb->max_width();
      call("tb", {th, mw});
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
#include "crt/msvc_rand.h"
#include "gamez/zrender/zrndr_prim2d.h"

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

// zRender's 2D primitives (zrndr_prim2d) into a buffer the caller owns
// (test_prim2d.py): op 0 dashed, 1 clipped line, 2 line list, 3 pixel,
// 4 circle, 5 jittered circle, 6 noisy line; a[] the integers in the
// original's order, rand seeded first.
void probe_prim2d(int32_t op, uint8_t* target, uint32_t tw, uint32_t th, uint32_t pitch, const int32_t* a, float noise,
                  const int32_t* clip, const int32_t* points, int32_t count, uint32_t seed) {
  zrndr::Target2D& t = zrndr::target2d();
  t.base = target, t.width = tw, t.height = th, t.pitch = pitch;
  crt::msvc_srand(seed);
  const uint16_t px = static_cast<uint16_t>(a[4]);
  switch (op) {
    case 0: zrndr::line16_dashed(a[0], a[1], a[2], a[3], px, a[5]); break;
    case 1: zrndr::line16_clipped(clip, a[0], a[1], a[2], a[3], px); break;
    case 2: zrndr::line16_list(points, count, clip, px); break;
    case 3: zrndr::pixel16(a[0], a[1], px); break;
    case 4: zrndr::circle16(a[0], a[1], a[2], px, clip); break;
    case 5: zrndr::circle16_noisy(a[0], a[1], a[2], px, noise, clip); break;
    case 6: zrndr::line16_noisy(a[0], a[1], a[2], a[3], px, noise, clip); break;
    default: break;
  }
}

// zTwoD's HUD primitives drawn through a logging host (test_twod_prims.py):
// kind 0 Line2D, 1 ClipLine2D, 2 Frame, 3 FilledBox, 4 Polygon, 5 PolyLine;
// the fields as the test set them in the original's object (raw, its layout).
namespace {
struct PrimHost : ztwod::Host {
  std::string log;
  float clip_min[3] = {}, clip_max[3] = {};
  void add(const char* k, std::initializer_list<uint32_t> v) {
    log += k;
    char b[16];
    for (uint32_t x : v) std::snprintf(b, sizeof b, " %x", x), log += b;
    log += "\n";
  }
  static uint32_t f(float v) { return zgame::bits(v); }
  static void rect(std::initializer_list<uint32_t>&, const ztwod::Rect*) {}
  void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t p) override {
    add("line", {uint32_t(x0), uint32_t(y0), uint32_t(x1), uint32_t(y1), p});
  }
  void line_dashed(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t p, int32_t d) override {
    add("dashed", {uint32_t(x0), uint32_t(y0), uint32_t(x1), uint32_t(y1), p, uint32_t(d)});
  }
  void line_list(const int32_t* pts, int32_t n, const ztwod::Rect* c, uint32_t p) override {
    add("list", {uint32_t(n), uint32_t(c->left), uint32_t(c->top), uint32_t(c->right), uint32_t(c->bottom), p});
    for (int32_t i = 0; i <= n; ++i) add(" pt", {uint32_t(pts[2 * i]), uint32_t(pts[2 * i + 1])});
  }
  void circle(int32_t cx, int32_t cy, int32_t r, uint32_t p, const ztwod::Rect* c) override {
    add("circle", {uint32_t(cx), uint32_t(cy), uint32_t(r), p});
    if (c) add(" clip", {uint32_t(c->left), uint32_t(c->top), uint32_t(c->right), uint32_t(c->bottom)});
  }
  void circle_noisy(int32_t cx, int32_t cy, int32_t r, uint32_t p, float n, const ztwod::Rect* c) override {
    add("noisy circle", {uint32_t(cx), uint32_t(cy), uint32_t(r), p, f(n)});
    if (c) add(" clip", {uint32_t(c->left), uint32_t(c->top), uint32_t(c->right), uint32_t(c->bottom)});
  }
  void line_noisy(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t p, float n, const ztwod::Rect* c) override {
    add("noisy line", {uint32_t(x0), uint32_t(y0), uint32_t(x1), uint32_t(y1), p, f(n)});
    if (c) add(" clip", {uint32_t(c->left), uint32_t(c->top), uint32_t(c->right), uint32_t(c->bottom)});
  }
  void polygon(const float* v, int32_t n, uint32_t color, int32_t winding) override {
    add("polygon", {uint32_t(n), color, uint32_t(winding)});
    for (int32_t i = 0; i < n; ++i) add(" v", {f(v[3 * i]), f(v[3 * i + 1])});
  }
  void render_clip(float mn[3], float mx[3]) override {
    for (int i = 0; i < 3; ++i) mn[i] = clip_min[i], mx[i] = clip_max[i];
  }
};
}  // namespace

int probe_twod_prim(int32_t kind, const uint8_t* raw, const float* saved_clip, const float* line_clip, char* out,
                    int size) {
  PrimHost h;
  for (int i = 0; i < 3; ++i) h.clip_min[i] = saved_clip[i], h.clip_max[i] = saved_clip[3 + i];
  ztwod::set_host(&h);
  ztwod::LineClip& lc = ztwod::line_clip();
  for (int i = 0; i < 3; ++i) lc.min[i] = line_clip[i], lc.max[i] = line_clip[3 + i];
  auto i32 = [&](int off) {
    int32_t v;
    std::memcpy(&v, raw + off, 4);
    return v;
  };
  auto f32 = [&](int off) {
    float v;
    std::memcpy(&v, raw + off, 4);
    return v;
  };
  ztwod::Rect rect{i32(0x200), i32(0x204), i32(0x208), i32(0x20c)};  // the test's rect, after the object
  auto base = [&](ztwod::Object2D& o) { o.x = i32(0x14), o.y = i32(0x18); };
  if (kind <= 1) {
    ztwod::ClipLine2D l(0, 0, 0, 0, 0);
    base(l);
    l.end_x = i32(0x34), l.end_y = i32(0x38), l.color = static_cast<uint32_t>(i32(0x3c));
    l.clipped = i32(0x40), l.clip = i32(0x44) ? &rect : nullptr, l.dash = i32(0x48);
    if (kind == 1) {
      l.noise = f32(0x4c);
      l.draw();
    } else {
      l.Line2D::draw();
    }
  } else if (kind <= 3) {
    ztwod::FilledBox c(0, 0, 0, 0, nullptr);
    base(c);
    c.radius = i32(0x34), c.color = static_cast<uint32_t>(i32(0x3c)), c.clipped = raw[0x40];
    c.clip = {i32(0x44), i32(0x48), i32(0x4c), i32(0x50)};
    if (kind == 3) {
      c.noise = f32(0x54);
      c.draw();
    } else {
      c.Frame::draw();
    }
  } else if (kind == 4) {
    ztwod::Polygon p;
    base(p);
    std::memcpy(p.pts, raw + 0x34, sizeof p.pts);
    p.count = i32(0x130), p.color = static_cast<uint32_t>(i32(0x134));
    p.draw();
  } else {
    ztwod::PolyLine p;
    base(p);
    std::memcpy(p.pts, raw + 0x34, sizeof p.pts);
    p.count = i32(0xdc), p.color = static_cast<uint32_t>(i32(0xe0));
    p.clip = i32(0xe4) ? &rect : nullptr, p.round = raw[0xe8];
    p.draw();
  }
  char b[16];
  h.log += "lineclip";
  for (float v : lc.min) std::snprintf(b, sizeof b, " %x", zgame::bits(v)), h.log += b;
  for (float v : lc.max) std::snprintf(b, sizeof b, " %x", zgame::bits(v)), h.log += b;
  for (int32_t v : lc.ints) std::snprintf(b, sizeof b, " %x", static_cast<uint32_t>(v)), h.log += b;
  h.log += "\n";
  ztwod::set_host(nullptr);
  if (static_cast<int>(h.log.size()) >= size) return -1;
  std::memcpy(out, h.log.c_str(), h.log.size() + 1);
  return static_cast<int>(h.log.size());
}

// zrndr::line16 into a buffer the caller owns (test_twod_line.py).
void probe_line16(uint8_t* target, uint32_t tw, uint32_t th, uint32_t pitch, int32_t x0, int32_t y0, int32_t x1,
                  int32_t y1, uint16_t pixel) {
  zrndr::Target2D& t = zrndr::target2d();
  t.base = target;
  t.width = tw;
  t.height = th;
  t.pitch = pitch;
  zrndr::line16(x0, y0, x1, y1, pixel);
}

}  // extern "C"

extern "C" {

// The message box (test_twod_msgbox.py): dlgMessage.zrd's MESSAGEBOX from
// `reader` (opened as zReader's archive), run() with the mouse frames (15
// dwords each), then destroyed. Returns run()'s answer.
int probe_twod_msgbox(const char* reader, int no_draw, const char* text, const char* caption, uint32_t style,
                      const uint32_t* frames, int nframes) {
  RecordingHost host;
  ztwod::set_host(&host);
  g_tex.clear();
  g_tex_id.clear();
  g_own_id.clear();
  g_own_next = 0;
  g_mouse.assign(static_cast<size_t>(nframes), ztwod::MouseState{});
  for (int i = 0; i < nframes; ++i) std::memcpy(&g_mouse[static_cast<size_t>(i)], frames + 15 * i, 60);
  g_frame = 0;
  g_polls = 0;
  zrdr::init();
  zrdr::close_archives(true);
  zrdr::open_archive(reader, true);
  ztwod::set_page_flip(true);
  int r;
  {
    ztwod::MessageBox box("dlgMessage.zrd", "MESSAGEBOX", no_draw != 0);
    r = box.run(text, caption, style);
    call("result", {static_cast<uint32_t>(r), ztwod::dirty_bits(), static_cast<uint32_t>(box.frames)});
  }
  zrdr::close_archives(true);
  ztwod::set_host(nullptr);
  return r;
}

}  // extern "C"
