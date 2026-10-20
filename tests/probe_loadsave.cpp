// Probe for test_loadsave.py (docs/menus.md, The load/save popup): the
// controls' load/save popup state and its screen (the edit bar, the file list)
// built from dlgOptions.zrd (or a synthetic one) on the ported zTwoD, then a
// plan of frames, presses, typed characters, the state's enter and leave
// halves and the list's calls; every call out recorded through the calltrace
// callback, the screen dumped as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "mech3/app.h"
#include "mech3/controls_menu.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

struct LsHost : RecordingHost {
  void (*cb)(int32_t, int32_t, uintptr_t) = nullptr;
  uintptr_t ctx = 0;
  std::map<uintptr_t, uint32_t> offsets;  // the bars' places in the original's screen
  void set_char_callback(void (*c)(int32_t, int32_t, uintptr_t), uintptr_t x) override {
    cb = c;
    ctx = x;
    call("0x0051a9a0", {c ? 1u : 0u, x ? offsets[x] : 0u});
  }
};

struct LsEngine : mech3::Engine {
  char active[0x104] = {};
  std::vector<std::string> listing;
  Handle popup_handle = 0;
  mech3::MenuScreen* popup = nullptr;

  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  char* active_ctl_cfg() override { return active; }
  std::vector<std::string> list_files(const char* pattern) override {
    call("FindFirstFileA", {pattern});
    return listing;
  }
  Handle adopt_screen(mech3::MenuScreen* s, uint32_t) override {
    popup = s;
    popup_handle = 0x7b000000;
    return popup_handle;
  }
  uint32_t vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args) override {
    if (obj == popup_handle && popup && slot == 0x08) {  // the state's destructor
      delete popup;
      popup = nullptr;
      return 0;
    }
    return Engine::vcall(obj, slot, args);
  }
};

// The state with its owner's load or save recorded (the original's 0x0042f380 is a stub).
struct ProbeState : mech3::LoadSavePopupState {
  using LoadSavePopupState::LoadSavePopupState;
  void apply(const char* name, uint8_t save) override { call("0x0042f380", {name, save}); }
};

uint32_t hex(std::istringstream& ls) {
  uint32_t v = 0;
  ls >> std::hex >> v >> std::dec;
  return v;
}
float fhex(std::istringstream& ls) { return zgame::from_bits(hex(ls)); }

std::string unhex(const std::string& h) {
  std::string s;
  for (size_t i = 0; i + 1 < h.size(); i += 2) s += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
  return s;
}

ztwod::MouseState read_mouse(std::istringstream& ls) {
  ztwod::MouseState m{};
  uint32_t* w = reinterpret_cast<uint32_t*>(&m);
  for (int i = 0; i < 15; ++i) w[i] = hex(ls);
  return m;
}

void dump_text(Dump& d, ztwod::Text* t) { d.text(t); }

void dump_popup(mech3::LoadSavePopupScreen& p, Dump& d) {
  d.o << "popup ";
  d.t("grab", p.grab);
  d.kv("save", p.save);
  d.kv("pstate", p.state);
  d.o << "\nbar ";
  mech3::LoadSavePopupScreen::NameBar& b = p.bar;
  d.control(&b);
  d.kv("focus", b.focus);
  d.kv("clicked", b.clicked);
  d.kv("numeric", b.numeric);
  d.kv("caretw", b.caret_w);
  d.kv("active", b.active);
  d.kv("reading", b.reading);
  d.kv("lx", b.label_x);
  d.kv("ly", b.label_y);
  d.kv("size", b.editor.size);
  d.kv("cursor", b.editor.cursor);
  d.s("buf", b.editor.buf);
  d.s("reject", b.editor.reject);
  d.o << "\ncaret ";
  ztwod::Caret& c = b.caret;
  d.base(&c);
  d.kv("count", c.count);
  if (c.count) d.kv("color", c.color);
  for (int i = 0; i < c.count; ++i) d.o << c.pts[i][0] << ',' << c.pts[i][1] << ' ';
  d.kv("cx", c.cx);
  d.kv("cy", c.cy);
  d.kv("cw", c.cw);
  d.kv("ch", c.ch);
  d.kv("blink", c.blink);
  d.f("period", c.period);
  d.f("btimer", c.blink_timer);
  d.kv("phase", c.phase);
  d.o << "\nfiles ";
  mech3::FileList& f = p.files;
  d.control(&f);
  d.kv("selected", f.selected);
  d.kv("top", f.top);
  d.kv("size", f.size);
  d.r("rect", f.rect);
  d.kv("sx", f.spacing_x);
  d.kv("sy", f.spacing_y);
  d.kv("ox", f.offset_x);
  d.kv("oy", f.offset_y);
  d.kv("fn", f.font_normal);
  d.kv("fs", f.font_selected);
  d.kv("fr", f.font_rollover);
  d.kv("bdx", f.bar_dx);
  d.kv("bdy", f.bar_dy);
  d.kv("rdx", f.roll_dx);
  d.kv("rdy", f.roll_dy);
  d.kv("s0", f.sounds[0]);
  d.kv("s1", f.sounds[1]);
  d.kv("p0", f.playing[0]);
  d.kv("p1", f.playing[1]);
  d.kv("arrow", f.arrow_lit);
  d.kv("inside", f.inside);
  int lit = -1;
  for (size_t i = 0; i < f.rows.size(); ++i)
    if (f.rows[i] == f.lit) lit = static_cast<int>(i);
  if (f.lit && f.lit == f.selected_row) lit = 100;
  d.kv("lit", lit);
  d.f("rt", f.repeat_timer);
  d.f("rd", f.repeat_delay);
  d.f("rr", f.repeat_rate);
  d.kv("rep", f.repeating);
  d.kv("norep", f.no_repeat);
  d.kv("n", f.item_count());
  for (mech3::FileList::Item* it : f.items) {
    d.s("item", it->text);
    d.kv("v", it->value);
  }
  d.kv("nrows", static_cast<int64_t>(f.rows.size()));
  for (size_t i = 0; i < f.rows.size(); ++i) {
    d.o << "\nrow" << i << ' ';
    dump_text(d, f.rows[i]);
    d.kv("index", f.rows[i]->index);
  }
  if (f.selected_row) {
    d.o << "\nselrow ";
    dump_text(d, f.selected_row);
    d.kv("index", f.selected_row->index);
  }
  if (f.up) {
    d.o << "\nup ";
    d.control(f.up);
  }
  if (f.down) {
    d.o << "\ndown ";
    d.control(f.down);
  }
  if (f.bar_image) {
    d.o << "\nbarimg ";
    d.image(f.bar_image);
  }
  if (f.roll_image) {
    d.o << "\nrollimg ";
    d.image(f.roll_image);
  }
  d.o << '\n';
}

}  // namespace

extern "C" {

// Opens `reader` as zReader's archive, builds the popup's state at the
// original's address `state`, runs `plan` (lines, as test_loadsave.py writes
// them), dumps it into `out`, then destroys it. Returns the dump's length, or -1.
int probe_loadsave_run(const char* reader, uint32_t state, const char* plan, char* out, int size) {
  LsHost host;
  ztwod::set_host(&host);
  reset();
  LsEngine engine;
  std::unique_ptr<mech3::Mech3App> app;
  {
    probe_app::Mute mute;
    app = std::make_unique<mech3::Mech3App>(&engine);
  }
  mech3::set_app(app.get());
  const bool per_second = zfix::options().per_second;
  const bool bugs = zfix::options().bugs;
  zfix::options().per_second = false;  // the original's path (zfix.h)
  zfix::options().bugs = false;
  zrdr::init();
  zrdr::close_archives(true);
  zrdr::open_archive(reader, true);
  ztwod::set_page_flip(true);
  std::unique_ptr<ProbeState> st;
  mech3::LoadSavePopupScreen* p = nullptr;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> op;
    if (op == "active") {
      std::string s;
      ls >> s;
      std::snprintf(engine.active, sizeof engine.active, "%s", unhex(s).c_str());
    } else if (op == "N") {
      st = std::make_unique<ProbeState>(engine, state);
      p = st->popup;
      host.offsets[reinterpret_cast<uintptr_t>(&p->bar)] = 0x115c4;
    } else if (op == "y") {
      st->save = static_cast<uint8_t>(hex(ls));
    } else if (op == "Y") {  // enter, with this listing
      const uint32_t n = hex(ls);
      engine.listing.clear();
      for (uint32_t i = 0; i < n; ++i) {
        std::string name;
        ls >> name;
        const uint32_t attr = hex(ls);
        if (!(attr & 0x10)) engine.listing.push_back(unhex(name));
      }
      st->prepare();
    } else if (op == "Z") {
      st->finish();
    } else if (op == "F") {
      const float dt = fhex(ls);
      g_mouse.push_back(read_mouse(ls));
      p->update(dt);
      ++g_frame;
    } else if (op == "P") {  // a part's +0x30
      const uint32_t k = hex(ls);
      if (k == 0) p->ok.on_left_up();
      else if (k == 1) p->cancel.on_left_up();
      else if (k == 2) p->bar.on_left_up();
      else p->files.on_left_up();
    } else if (op == "R") {
      p->files.on_right_up();
    } else if (op == "S") {
      const int32_t i = static_cast<int32_t>(hex(ls));
      if (i < p->files.item_count()) p->files.select(i);
    } else if (op == "W") {
      p->files.scroll(static_cast<int32_t>(hex(ls)));
    } else if (op == "K") {  // zInput's text callback
      const int32_t ch = static_cast<int32_t>(hex(ls));
      const int32_t key = static_cast<int32_t>(hex(ls));
      if (host.cb) host.cb(ch, key, host.ctx);
    } else if (op == "T") {
      std::string s;
      ls >> s;
      p->bar.set_text(unhex(s).c_str());
    } else if (op == "U") {
      p->bar.numeric = 1;
    } else if (op == "c") {
      p->bar.EditBar::on_left_up();
    } else if (op == "X") {
      if (hex(ls)) p->bar.on_cancel();
      else p->bar.on_return();
    } else if (op == "h" || op == "v") {
      const uint32_t k = hex(ls);
      ztwod::Object2D* o = k == 0 ? static_cast<ztwod::Object2D*>(&p->bar) : static_cast<ztwod::Object2D*>(&p->files);
      if (op == "h") o->hide();
      else o->show_for(-1.0f);
    } else if (op == "G") {
      p->files.layout();
    } else if (op == "A") {
      p->set_active(static_cast<int32_t>(hex(ls)));
    } else if (op == "D") {
      p->dirty_all();
    } else if (op == "H") {
      p->restore_behind();
    } else if (op == "E") {
      Dump d;
      dump_dialog(*p, d);
      dump_popup(*p, d);
      dump = d.o.str();
      st.reset();
    }
  }
  zrdr::close_archives(true);
  zfix::options().per_second = per_second;
  zfix::options().bugs = bugs;
  mech3::set_app(nullptr);
  ztwod::set_host(nullptr);
  if (static_cast<int>(dump.size()) >= size) return -1;
  std::memcpy(out, dump.data(), dump.size() + 1);
  return static_cast<int>(dump.size());
}

}  // extern "C"
