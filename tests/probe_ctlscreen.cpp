// Probe for test_ctlscreen.py (docs/menus.md, The controls screen): the
// controls screen built from dlgOptions.zrd (or a synthetic one) with its axis
// popup, on the ported zTwoD, then a plan of frames, presses, captures and the
// popup's calls; every call out recorded through the calltrace callback (zTwoD's
// through the shared RecordingHost, the game's through a recording
// mech3::Engine), the screens dumped as text.
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
#include "mech3/controls_cfg.h"
#include "mech3/controls_menu.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

const char* text_at(uint32_t a) {
  if (!a) return nullptr;
  std::string s;
  for (uint32_t i = 0; i < 256; ++i) {
    const char c = static_cast<char>(call("peek8", {a + i}));
    if (!c) break;
    s += c;
  }
  return keep(s);
}

// A name stub's text: empty when it answered 0 (the emulator writes the same).
const char* name_of(const char* key, uint32_t v, int32_t n, const char* prefix, char* out) {
  if (call(key, {v, n})) std::snprintf(out, static_cast<size_t>(n), "%s%x", prefix, v);
  else out[0] = 0;
  return out;
}

struct CtlEngine : mech3::Engine {
  char active[0x104] = {};
  std::map<Handle, mech3::MenuScreen*> popups;  // the axis and load/save popups' screens

  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  const char* message(int32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    char buf[16];
    std::snprintf(buf, sizeof buf, "M%x", static_cast<uint32_t>(id));
    return keep(buf);
  }
  const char* message_format(int32_t id, const char* arg) override {
    call("0x00575fd0", {id, arg});
    return keep("F" + [&] {
      char b[16];
      std::snprintf(b, sizeof b, "%x", static_cast<uint32_t>(id));
      return std::string(b);
    }() + ":" + (arg ? arg : ""));
  }
  bool main_menu_background(zimg::Texture** image, uint8_t* moving) override {
    const uint32_t m = call("peek32", {0x609854});
    if (!m) return false;
    *image = tex(call("peek32", {m + 0x110ec}));
    *moving = static_cast<uint8_t>(call("peek8", {m + 0x111e8}));
    return true;
  }
  uint8_t mission_running() override { return static_cast<uint8_t>(call("peek8", {0x608ed9})); }
  void set_use_joystick(int32_t on) override { call("0x0040d070", {on}); }
  uint32_t input_read_key(int32_t wait) override { return call("0x0051aa00", {wait}); }
  int32_t joy_wait_button(int32_t wait) override { return static_cast<int32_t>(call("0x0051dd20", {wait})); }
  int32_t mouse_wait_button(int32_t wait) override { return static_cast<int32_t>(call("0x0051baf0", {wait})); }
  void joy_use() override { call("0x0051b2c0", {}); }
  void joy_unuse() override { call("0x0051b2f0", {}); }
  uint32_t key1_of(int32_t c) override { return call("0x0051d200", {c}); }
  uint32_t key2_of(int32_t c) override { return call("0x0051d210", {c}); }
  uint32_t joy_of(int32_t c) override { return call("0x0051d220", {c}); }
  uint32_t mouse_of(int32_t c) override { return call("0x0051d230", {c}); }
  int32_t key1_command(uint32_t k) override { return static_cast<int32_t>(call("0x0051d240", {k})); }
  int32_t key2_command(uint32_t k) override { return static_cast<int32_t>(call("0x0051d250", {k})); }
  int32_t joy_command(uint32_t b) override { return static_cast<int32_t>(call("0x0051d260", {b})); }
  int32_t mouse_command(uint32_t b) override { return static_cast<int32_t>(call("0x0051d270", {b})); }
  void bind_key1(uint32_t k, int32_t c) override { call("0x0051d280", {k, c}); }
  void bind_key2(uint32_t k, int32_t c) override { call("0x0051d290", {k, c}); }
  void bind_joy(uint32_t b, int32_t c) override { call("0x0051d2a0", {b, c}); }
  void bind_mouse(uint32_t b, int32_t c) override { call("0x0051d2b0", {b, c}); }
  const char* command_name(int32_t c, char* out, int32_t n) override {
    return name_of("0x0051d310", static_cast<uint32_t>(c), n, "C", out);
  }
  const char* key_name(uint32_t k, char* out, int32_t n) override { return name_of("0x0051d330", k, n, "K", out); }
  const char* joy_button_name(uint32_t b, char* out, int32_t n) override {
    return name_of("0x0051d350", b, n, "J", out);
  }
  const char* mouse_button_name(uint32_t b, char* out, int32_t n) override {
    return name_of("0x0051d370", b, n, "B", out);
  }
  void input_map_refresh() override { call("0x0051d1c0", {}); }
  int32_t controls_defaults() override { return static_cast<int32_t>(call("0x00411150", {})); }
  int32_t controls_group_count() override { return static_cast<int32_t>(call("0x00410f80", {})); }
  const char* controls_group_title(int32_t g) override { return text_at(call("0x00410fa0", {g})); }
  int32_t controls_group_size(int32_t g) override { return static_cast<int32_t>(call("0x00410fb0", {g})); }
  int32_t controls_group_command(int32_t g, int32_t i) override {
    return static_cast<int32_t>(call("0x00410fd0", {g, i}));
  }
  void controls_read(const char* path) override { call("0x004120a0", {path}); }
  void controls_write(const char* path) override { call("0x00411e60", {path}); }
  char* active_ctl_cfg() override { return active; }
  Handle adopt_screen(mech3::MenuScreen* s, uint32_t) override {
    const Handle h = 0x7a000000 + 0x100000 * static_cast<Handle>(popups.size());
    popups[h] = s;
    return h;
  }
  uint32_t vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args) override {
    auto it = popups.find(obj);
    if (it != popups.end() && slot == 0x08) {  // a popup state's destructor
      delete it->second;
      popups.erase(it);
      return 0;
    }
    return Engine::vcall(obj, slot, args);
  }
};

uint32_t hex(std::istringstream& ls) {
  uint32_t v = 0;
  ls >> std::hex >> v >> std::dec;
  return v;
}
float fhex(std::istringstream& ls) { return zgame::from_bits(hex(ls)); }

ztwod::MouseState read_mouse(std::istringstream& ls) {
  ztwod::MouseState m{};
  uint32_t* w = reinterpret_cast<uint32_t*>(&m);
  for (int i = 0; i < 15; ++i) w[i] = hex(ls);
  return m;
}

void dump_list(mech3::ControlsList& l, Dump& d) {
  d.kv("first", l.first);
  d.kv("n", static_cast<int64_t>(l.strings.size()));
  for (size_t i = 0; i < l.strings.size(); ++i) {
    d.s("item", l.strings[i].c_str());
    d.kv("v", l.values[i]);
  }
}

void mini_control(ztwod::Control* c, Dump& d) {
  d.image(c);
  d.kv("bx", c->base_x);
  d.kv("by", c->base_y);
  d.kv("en", c->enabled);
  d.r("bounds", c->bounds);
  d.t("inorm", c->img_normal);
  d.t("idis", c->img_disabled);
  d.t("iroll", c->img_rollover);
  d.t("iact", c->img_activate);
}

void dump_cycle(mech3::CycleButton& c, Dump& d) {
  d.kv("index", c.index);
  d.kv("hover", c.hover);
  d.kv("n", c.count());
  for (mech3::CycleButton::Item* it : c.items) {
    d.s("item", it->text ? it->text : "(null)");
    d.kv("v", it->value);
  }
  d.o << "\n  prev ";
  mini_control(&c.prev, d);
  d.o << "\n  next ";
  mini_control(&c.next, d);
}

void dump_screen(mech3::ControlsScreen& c, Dump& d) {
  d.o << "ctl ";
  d.kv("state", c.state);
  d.kv("busy", c.busy_);
  d.kv("mode", c.mode);
  d.kv("dirty", c.dirty);
  d.kv("guard", mech3::controls_mouse_guard());
  for (int i = 0; i < 8; ++i) {
    d.f("lo", c.range[i][0]);
    d.f("hi", c.range[i][1]);
  }
  d.o << "\nbackground ";
  d.image(&c.background);
  d.kv("moving", c.background.moving);
  d.r("view", c.background.view);
  mech3::ControlsList* lists[] = {&c.commands, &c.key_a, &c.key_b, &c.joy, &c.mouse};
  for (int i = 0; i < 5; ++i) {
    d.o << "\nlist" << i << ' ';
    dump_list(*lists[i], d);
  }
  d.o << "\ngroups ";
  dump_cycle(c.group_cycle, d);
  mech3::SlideButton* slides[] = {&c.accept, &c.cancel, &c.reset_mouse, &c.reset_joy, &c.assign_axis};
  for (int i = 0; i < 5; ++i) {
    d.o << "\nslide" << i << ' ';
    d.kv("rippling", slides[i]->rippling);
    d.kv("radius", slides[i]->radius);
    d.kv("maxr", slides[i]->max_radius);
  }
  const mech3::AxisTable& t = mech3::axis_table();
  d.o << "\naxes ";
  for (const mech3::AxisBinding& a : t.axes) {
    d.kv("src", a.source);
    d.f("scale", a.scale);
    d.f("off", a.offset);
    d.f("dz", a.deadzone);
  }
  d.kv("ready", t.ready);
  d.o << "\nactive ";
  d.s("cfg", c.engine().active_ctl_cfg());
  d.o << "\npopup ";
  d.kv("owner", c.axis_popup.owner ? c.self() : 0);
  d.kv("axis", c.axis_popup.axis);
  d.kv("source", c.axis_popup.source);
  d.kv("lsowner", c.load_save_popup.owner ? c.self() : 0);
  d.kv("lssave", c.load_save_popup.save);
  if (mech3::AxisPopupScreen* p = c.axis_popup.popup) {
    d.kv("paxis", p->axis);
    d.t("grab", p->grab);
    d.kv("pstate", p->state);
    d.kv("inv_id", p->invert.id);
    d.kv("inv_active", p->invert.active);
    d.o << "\n  sources ";
    dump_cycle(p->sources, d);
  }
  d.o << '\n';
}

void kinds(mech3::ControlsScreen& c) {
  mech3::ControlsList* lists[] = {&c.commands, &c.key_a, &c.key_b, &c.joy, &c.mouse};
  for (mech3::ControlsList* l : lists) g_kind[l] = 'X';
  if (mech3::AxisPopupScreen* p = c.axis_popup.popup) g_kind[&p->value] = 'L';
}

}  // namespace

extern "C" {

// Opens `reader` as zReader's archive, builds the controls screen for `state`
// at the original's address `self`, runs `plan` (lines, as test_ctlscreen.py
// writes them), dumps it into `out`, then destroys it. Returns the dump's
// length, or -1.
int probe_ctlscreen_run(const char* reader, uint32_t state, uint32_t self, const char* plan, char* out, int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();
  CtlEngine engine;
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
  mech3::AxisTable& table = mech3::axis_table();
  mech3::ControlsSetup& setup = mech3::controls_setup();
  for (mech3::CommandEntry& c : setup.commands) c = mech3::CommandEntry{};
  std::unique_ptr<mech3::ControlsScreen> ctl;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> op;
    if (op == "guard") {
      mech3::controls_mouse_guard() = static_cast<int32_t>(hex(ls));
    } else if (op == "axis") {
      const int i = static_cast<int>(hex(ls));
      table.axes[i].source = static_cast<int32_t>(hex(ls));
      table.axes[i].scale = fhex(ls);
      table.axes[i].offset = fhex(ls);
      table.axes[i].deadzone = fhex(ls);
    } else if (op == "ready") {
      table.ready = static_cast<uint8_t>(hex(ls));
    } else if (op == "entry") {
      const uint32_t c = hex(ls);
      setup.commands[c].message = static_cast<int32_t>(hex(ls));
      setup.commands[c].axis_key = static_cast<uint8_t>(hex(ls));
      setup.commands[c].axis = static_cast<int32_t>(hex(ls));
    } else if (op == "active") {
      std::string s;
      std::getline(ls, s);
      if (!s.empty() && s[0] == ' ') s.erase(0, 1);
      std::snprintf(engine.active, sizeof engine.active, "%s", s.c_str());
    } else if (op == "N") {
      ctl = std::make_unique<mech3::ControlsScreen>(engine, state, self);
      kinds(*ctl);
    } else if (op == "F") {
      const float dt = fhex(ls);
      g_mouse.push_back(read_mouse(ls));
      ctl->update(dt);
      ++g_frame;
    } else if (op == "f") {  // a frame of the popup
      const float dt = fhex(ls);
      g_mouse.push_back(read_mouse(ls));
      if (ctl->axis_popup.popup) ctl->axis_popup.popup->update(dt);
      ++g_frame;
    } else if (op == "A") {
      ctl->set_active(static_cast<int32_t>(hex(ls)));
    } else if (op == "P") {  // a slide button's +0x30
      mech3::ControlsButton* b[] = {&ctl->accept, &ctl->cancel, &ctl->reset_mouse, &ctl->reset_joy,
                                    &ctl->assign_axis};
      b[hex(ls)]->on_left_up();
    } else if (op == "L" || op == "M" || op == "S") {  // a list's +0x30, +0x34, +0x90
      mech3::ControlsList* l[] = {&ctl->commands, &ctl->key_a, &ctl->key_b, &ctl->joy, &ctl->mouse};
      mech3::ControlsList* x = l[hex(ls)];
      if (op == "L") x->on_left_up();
      else if (op == "M") x->on_right_up();
      else x->select(static_cast<int32_t>(hex(ls)));
    } else if (op == "C") {
      ctl->group_cycle.on_left_up();
    } else if (op == "W") {
      ctl->group_cycle.step(static_cast<int32_t>(hex(ls)));
    } else if (op == "O") {
      mech3::ControlsArrow* a[] = {&ctl->next, &ctl->prev, &ctl->export_btn, &ctl->import_btn};
      a[hex(ls)]->on_left_up();
    } else if (op == "B") {  // 0x0042c670 .. 0x0042cb00
      const uint32_t kind = hex(ls), v = hex(ls);
      const int32_t idx = static_cast<int32_t>(hex(ls));
      if (kind == 0) ctl->bind_key1(v, idx);
      else if (kind == 1) ctl->bind_key2(v, idx);
      else if (kind == 2) ctl->bind_joy(v, idx);
      else ctl->bind_mouse(v, idx);
    } else if (op == "X") {  // 0x0042cca0
      const int32_t src = static_cast<int32_t>(hex(ls));
      const float sc = fhex(ls), off = fhex(ls);
      ctl->bind_axis(src, sc, off);
    } else if (op == "J") {
      ctl->clear_joy_axis();
    } else if (op == "K") {
      ctl->clear_mouse_axis();
    } else if (op == "V") {  // 0x0042d0b0
      std::string name;
      const uint32_t save = hex(ls);
      ls >> name;
      ctl->load_save(name.c_str(), save != 0);
    } else if (op == "y") {  // the popup's fields, as the pushes set them
      ctl->axis_popup.owner = ctl.get();
      ctl->axis_popup.axis = static_cast<int32_t>(hex(ls));
      ctl->axis_popup.source = static_cast<int32_t>(hex(ls));
    } else if (op == "Y") {
      ctl->axis_popup.prepare();
    } else if (op == "Z") {
      ctl->axis_popup.finish();
    } else if (op == "p") {  // the popup's controls' +0x30
      mech3::AxisPopupScreen* p = ctl->axis_popup.popup;
      const uint32_t k = hex(ls);
      if (!p) continue;
      if (k == 0) p->ok.on_left_up();
      else if (k == 1) p->cancel.on_left_up();
      else if (k == 2) p->sources.on_left_up();
      else if (k == 3) p->invert.on_left_up();
      else p->value.on_left_up();
    } else if (op == "q") {
      if (ctl->axis_popup.popup) ctl->axis_popup.popup->sources.step(static_cast<int32_t>(hex(ls)));
    } else if (op == "D") {
      ctl->dirty_all();
    } else if (op == "H") {
      ctl->restore_behind();
    } else if (op == "E") {
      Dump d;
      dump_dialog(*ctl, d);
      if (ctl->axis_popup.popup) {
        Dump p;
        dump_dialog(*ctl->axis_popup.popup, p);
        d.o << "POPUP\n" << p.o.str();
      }
      dump_screen(*ctl, d);
      dump = d.o.str();
      ctl.reset();
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
