// Probe for test_lab_screen.py (docs/mechlab.md, step 2): the mech lab's
// screen built from dlgMechLabII.zrd (or a synthetic archive) on the ported
// zTwoD, the Lab State's globals and the game's slots read from the
// emulator, then a plan of page switches; every call out recorded through
// the calltrace callback, the screen dumped as text.
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
#include "mech3/app_states.h"
#include "mech3/lab_screen.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

std::string hex_s(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}

struct LabHost : RecordingHost {
  const char* message_text(uint32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    return keep("M" + hex_s(id));
  }
};

struct LabEngine : mech3::Engine {
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  void set_fast_menu(int32_t on) override { call("0x0040ce90", {on}); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  const char* message(int32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    return keep("M" + hex_s(static_cast<uint32_t>(id)));
  }
  const char* mech_name_of(int32_t mech) override {
    if (!call("0x00484650", {mech})) return nullptr;
    return keep("mech" + std::to_string(mech));
  }
  void pop_state(uint32_t arg) override { call("0x005905a0", {arg}); }
  void lab_store_remove(Handle store, int32_t mech) override { call("0x00472a30", {store, mech}); }
  void lab_store_add(Handle store, int32_t mech) override { call("0x00471c10", {store, mech}); }
  mech3::LabScreen* screen = nullptr;
  mech3::LabScreen* lab_screen(Handle) override { return screen; }
};

std::vector<uint32_t> g_slots;  // the game's slots, by their address in the emulator

uint32_t hex(std::istringstream& ls) {
  uint32_t v = 0;
  ls >> std::hex >> v >> std::dec;
  return v;
}

int32_t ref(const Dump& d, const ztwod::Object2D* o) {
  for (size_t i = 0; i < d.objs.size(); ++i)
    if (d.objs[i] == o) return static_cast<int32_t>(i);
  return -1;
}

// The dump test_lab_screen.py builds from the emulator's memory.
void dump_screen(mech3::LabScreen& s, Dump& d) {
  std::ostringstream& o = d.o;
  o << "screen page=" << s.page << '\n';
  for (int i = 0; i < 7; ++i) {
    mech3::HelpButton& t = s.tabs[i];
    o << "tab" << i << " id=" << t.id << " en=" << t.enabled << " sel=" << int(t.selected)
      << " blink=" << int(t.blinking) << " help=";
    bool first = true;
    for (ztwod::Text* h : t.help) {
      o << (first ? "" : ",") << 'o' << ref(d, h);
      first = false;
    }
    o << '\n';
  }
  for (int i = 0; i < 12; ++i) {
    mech3::MechButton& m = s.mechs[i];
    o << "mech" << i << " id=" << m.id << " mech=" << m.mech << " name=o" << ref(d, &m.name) << '\n';
  }
  o << "parts head=" << s.part_head.part << ',' << s.part_head.id;
  for (const mech3::LabPart& p : s.parts) o << ' ' << p.part << ',' << p.id;
  o << '\n';
  const mech3::LabPanel* ps[] = {&s.panel_center, &s.panel_left, &s.panel_right};
  for (const mech3::LabPanel* p : ps)
    o << "panel " << std::hex << id(p->single) << ' ' << id(p->twice) << ' ' << id(p->twice2) << std::dec << '\n';
  o << "message sound=" << std::hex << s.message.x4b0 << " vol=" << zgame::bits(s.message.x4b4) << std::dec << '\n';
  const mech3::LabState& lab = mech3::states().lab;
  o << "lab slot=" << lab.slot_ << " mech=" << lab.mech_ << " chassis sel=" << s.chassis.selected << std::hex
    << " x218=" << s.chassis.x218 << " store=" << s.chassis.store << std::dec << " slots=";
  for (size_t i = 0; i < g_slots.size(); ++i)
    o << (i ? "," : "") << static_cast<int32_t>(call("peek32", {g_slots[i] + 4})) << '/' << std::hex
      << call("peek32", {g_slots[i] + 0xc}) << std::dec;
  o << '\n';
}

}  // namespace

extern "C" {

// setup: the slot count the screen is made with, the pilot, the lab's
// store, the slot shown, the flag, the game's slots.
int probe_lab_run(const char* reader, const char* setup, const char* plan, char* out, int size) {
  LabHost host;
  ztwod::set_host(&host);
  reset();
  LabEngine engine;
  std::unique_ptr<mech3::Mech3App> app;
  {
    probe_app::Mute mute;
    app = std::make_unique<mech3::Mech3App>(&engine);
    mech3::reset_states();
  }
  mech3::set_app(app.get());
  uint32_t slots = 0;
  {
    std::istringstream ss(setup);
    uint32_t pilot = 0, store = 0, shown = 0, flag = 0, n = 0;
    ss >> std::hex >> slots >> pilot >> store >> shown >> flag >> n;
    g_slots.assign(n, 0);
    for (uint32_t& a : g_slots) ss >> a;
    mech3::LabState& lab = mech3::states().lab;
    lab.pilot_ = pilot;
    lab.store_ = store;
    lab.slot_ = static_cast<int32_t>(shown);
    lab.flag_ = static_cast<uint8_t>(flag);
    lab.slots_ = static_cast<int32_t>(slots);
    lab.mech_ = mech3::LabState::kNoMech;
    lab.lab_screen_ = 0x7700;
  }
  const bool per_second = zfix::options().per_second;
  const bool bugs = zfix::options().bugs;
  zfix::options().per_second = false;  // the original's path (zfix.h)
  zfix::options().bugs = false;
  zrdr::init();
  zrdr::close_archives(true);
  zrdr::open_archive(reader, true);
  ztwod::set_page_flip(true);
  std::unique_ptr<mech3::LabScreen> s;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> op;
    if (op == "S") {
      s = std::make_unique<mech3::LabScreen>(engine, static_cast<int32_t>(slots));
      engine.screen = s.get();
      g_kind[&s->message] = 'F';
    } else if (!s) {
      continue;
    } else if (op == "G") {
      const uint32_t p = hex(ls), show = hex(ls);
      s->show_page(static_cast<int32_t>(p), show != 0);
    } else if (op == "B") {
      s->on_button(static_cast<int32_t>(hex(ls)), 0, 0);
    } else if (op == "C") {
      mech3::states().lab.set_mech(static_cast<int32_t>(hex(ls)));
    } else if (op == "X") {
      s->config_edit.x38c = hex(ls);
    } else if (op == "M") {
      const uint32_t x = hex(ls), y = hex(ls);
      s->config_edit.set_pos(static_cast<int32_t>(x), static_cast<int32_t>(y));
    } else if (op == "E") {
      Dump d;
      dump_dialog(*s, d);
      dump_screen(*s, d);
      dump += d.o.str();
    } else if (op == "U") {
      engine.screen = nullptr;
      s.reset();
      dump += "gone\n";
    }
  }
  engine.screen = nullptr;
  s.reset();
  {
    probe_app::Mute mute;
    mech3::reset_states();
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
