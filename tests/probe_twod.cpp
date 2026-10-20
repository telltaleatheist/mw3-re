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
