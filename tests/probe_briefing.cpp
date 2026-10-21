// Probe for test_briefing.py (docs/campaign.md section 2): the briefing
// screen built from briefing.zrd (or a synthetic archive) on the ported
// zTwoD, with the current game's kind, campaign, multiplayer kind and salvage
// from the test's setup, then a plan of frames, presses, the buttons' calls,
// set_loaded, ABORT, LAUNCH, the load's fraction and lines; every call out
// recorded through the calltrace callback, the screen dumped as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "mech3/app.h"
#include "mech3/briefing.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

struct Setup {
  int32_t kind = 1, op = 1, mission = 1, mp = 1, salvage = 0;
  float dt = 0.0f;
};

std::string hex_s(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}

struct BrEngine : mech3::Engine {
  Setup c;
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  void set_fast_menu(int32_t on) override { call("0x0040ce90", {on}); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  const char* message(int32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    return keep("M" + hex_s(static_cast<uint32_t>(id)));
  }
  const char* version_number() override { return "1.2"; }
  float frame_dt() override { return c.dt; }
  int32_t game_kind() override { return c.kind; }
  bool game_campaign(int32_t* op, int32_t* mission) override {
    if (c.kind != 1) return false;
    *op = c.op;
    *mission = c.mission;
    return true;
  }
  bool game_salvage_left() override { return c.kind == 1 && c.salvage != 0; }
  int32_t game_multiplayer_kind() override { return c.kind == 3 ? c.mp : 0; }
  void pilot_record_mission(Handle, int32_t a) override { call("0x00478f10", {a}); }
  void campaign_merge_salvage() override { call("0x00434cb0", {}); }
  uint32_t message_box(int32_t a, int32_t caption, int32_t text, int32_t) override {
    return call("0x004ac150", {a, caption, text});
  }
};

uint32_t hex(std::istringstream& ls) {
  uint32_t v = 0;
  ls >> std::hex >> v >> std::dec;
  return v;
}
float fhex(std::istringstream& ls) { return zgame::from_bits(hex(ls)); }

std::string unhex(const std::string& h) {
  if (h == "-") return "";
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

int32_t ref(const Dump& d, const ztwod::Object2D* o) {
  for (size_t i = 0; i < d.objs.size(); ++i)
    if (d.objs[i] == o) return static_cast<int32_t>(i);
  return -1;
}

// The dump test_briefing.py builds from the emulator's memory.
void dump_screen(mech3::BriefingScreen& s, Dump& d) {
  std::ostringstream& o = d.o;
  o << std::hex << "screen seen=" << int(s.salvage_seen) << " target=" << zgame::bits(s.target)
    << " loaded=" << int(s.loaded) << " playing=" << int(s.script.playing)
    << " at_end=" << (s.script.at == s.script.steps.end() ? 1 : 0) << " steps=" << s.script.steps.size()
    << std::dec << " reached=" << mech3::briefing_reached() << std::hex << " voice=" << mech3::briefing_voice()
    << " bar=" << zgame::bits(s.bar.value) << std::dec << '\n';
  const char* names[] = {"ABORT", "LAUNCH", "MECHLAB", "REPLAY", "SALVAGE", "LOADING"};
  mech3::HelpButton* bs[] = {&s.abort_btn, &s.launch_btn, &s.lab_btn, &s.replay_btn, &s.salvage_btn, &s.loading};
  for (int i = 0; i < 6; ++i) {
    mech3::HelpButton& b = *bs[i];
    o << names[i] << " id=" << b.id << std::hex << " en=" << b.enabled << " sel=" << int(b.selected)
      << " blink=" << int(b.blinking) << " period=" << zgame::bits(b.period) << " timer=" << zgame::bits(b.timer)
      << std::dec << " glow=" << b.glow[0] << ',' << b.glow[1] << " help=";
    for (ztwod::Text* t : b.help) o << 'o' << ref(d, t) << ',';
    o << '\n';
  }
  int32_t at = -1, i = 0;
  for (auto it = s.script.steps.begin(); it != s.script.steps.end(); ++it, ++i)
    if (it == s.script.at) at = i;
  o << "script at=" << at;
  for (mech3::BriefingStep* st : s.script.steps) {
    if (auto* v = dynamic_cast<mech3::BriefingVoice*>(st)) {
      o << " V\"" << std::string(v->name, strnlen(v->name, sizeof v->name)) << "\"," << std::hex
        << zgame::bits(v->volume) << std::dec << ',' << int(v->cues) << ',' << v->x5c;
    } else if (auto* m = dynamic_cast<mech3::BriefingMarker*>(st)) {
      o << " M" << m->marker;
    } else if (auto* sh = dynamic_cast<mech3::BriefingShow*>(st)) {
      o << " So" << ref(d, sh->obj);
    }
  }
  o << '\n';
  o << "REALLOC_OK id=" << s.realloc_ok.id << std::hex << " en=" << s.realloc_ok.enabled << std::dec << '\n';
}

}  // namespace

extern "C" {

// Opens `reader` as zReader's archive, takes the game's setup from `setup`
// ("kind op mission mp salvage dt-bits"), runs `plan` (test_briefing.py's
// lines) and dumps into `out`. Returns the dump's length, or -1.
int probe_briefing_run(const char* reader, const char* setup, const char* plan, char* out, int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();
  BrEngine engine;
  {
    std::istringstream ss(setup);
    ss >> engine.c.kind >> engine.c.op >> engine.c.mission >> engine.c.mp >> engine.c.salvage;
    engine.c.dt = fhex(ss);
  }
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
  mech3::briefing_reached() = -1;
  mech3::briefing_voice() = 0;
  std::unique_ptr<mech3::BriefingScreen> s;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> op;
    if (op == "V") {
      mech3::briefing_voice() = hex(ls);
    } else if (op == "C") {
      mech3::briefing_cue(static_cast<int32_t>(hex(ls)));
    } else if (op == "R") {
      mech3::briefing_reached() = static_cast<int32_t>(hex(ls));
    } else if (op == "S") {
      std::string a;
      ls >> a;
      s = std::make_unique<mech3::BriefingScreen>(engine, unhex(a).c_str());
      mech3::briefing_screen() = s.get();
      g_kind[&s->bar] = 'L';
      g_kind[&s->lines] = 'Z';
    } else if (!s) {
      continue;
    } else if (op == "F") {
      const float dt = fhex(ls);
      g_mouse.push_back(read_mouse(ls));
      s->update(dt);
      ++g_frame;
    } else if (op == "B") {
      s->on_button(static_cast<int32_t>(hex(ls)), 0, 0);
    } else if (op == "P") {
      ztwod::Control* parts[] = {&s->abort_btn, &s->launch_btn, &s->lab_btn,   &s->replay_btn,
                                 &s->salvage_btn, &s->loading,  &s->realloc_ok};
      const uint32_t k = hex(ls);
      if (k < 7) parts[k]->on_left_up();
    } else if (op == "N" || op == "O") {
      mech3::HelpButton* bs[] = {&s->abort_btn, &s->launch_btn, &s->lab_btn,
                                 &s->replay_btn, &s->salvage_btn, &s->loading};
      const uint32_t k = hex(ls);
      if (k >= 6) continue;
      if (op == "N") bs[k]->on_enter();
      else bs[k]->on_leave();
    } else if (op == "Y") {
      s->script.start();
    } else if (op == "L") {
      s->set_loaded(hex(ls) != 0);
    } else if (op == "A") {
      s->abort();
    } else if (op == "Q") {
      s->launch();
    } else if (op == "G") {
      s->progress(fhex(ls));
    } else if (op == "T") {
      std::string a;
      ls >> a;
      s->log(unhex(a).c_str());
    } else if (op == "E") {
      Dump d;
      dump_dialog(*s, d);
      dump_screen(*s, d);
      dump += d.o.str();
    } else if (op == "U") {
      s.reset();
      dump += "gone reached=" + std::to_string(mech3::briefing_reached()) + " voice=" +
              hex_s(mech3::briefing_voice()) + "\n";
    }
  }
  s.reset();
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
