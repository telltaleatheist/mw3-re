// Probe for test_debriefing.py (docs/campaign.md section 3): the debriefing
// screen built from dlgWin.zrd (or a synthetic archive) on the ported zTwoD,
// with the game's kind, operation and mission, the mission's flags, time and
// salvage store and the Win State's "won" byte from the test's setup, then a
// plan of frames, presses and the buttons' calls; every call out recorded
// through the calltrace callback, the screen dumped as text.
#include <cstdarg>
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
#include "mech3/debriefing.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

struct Setup {
  uint32_t kind = 1, op = 1, mission = 1, failed = 0, cheat = 0, time = 0, salvage = 0, won = 0, ia44 = 1, b0 = 0,
           round = 0, x6c = 0, x70 = 0, c4 = 0, b4[4] = {}, ended = 0, allies = 0;
  std::vector<mech3::Engine::LanceMech> lance;
  std::vector<mech3::Engine::Objective> objectives[2];
};
const char* const kNames[] = {"", "Me14", "Me15", "Me01", "Kobra"};

std::string hex_s(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}

struct DbHost : RecordingHost {
  uint32_t format_message(char* buf, uint32_t size, uint32_t id, va_list args) override {
    va_list c;
    va_copy(c, args);
    const int32_t a = va_arg(c, int32_t), b = va_arg(c, int32_t);
    va_end(c);
    const uint32_t n = call("0x00575ff0", {size, id});
    const std::string text = "msg" + std::to_string(id) + ":" + std::to_string(a) + "," + std::to_string(b);
    std::snprintf(buf, size, "%s", n ? text.c_str() : "");
    return n;
  }
  const char* message_text(uint32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    return keep("M" + hex_s(id));
  }
};

struct DbEngine : mech3::Engine {
  Setup c;
  uint8_t b0 = 0;
  int32_t x6c = 0, x70 = 0;
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  void set_fast_menu(int32_t on) override { call("0x0040ce90", {on}); }
  int32_t exchange_fast_menu(int32_t on) override { return static_cast<int32_t>(call("0x0040ce90", {on}) & 0xff); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  const char* message(int32_t id) override {
    if (!call("0x00576040", {id})) return nullptr;
    return keep("M" + hex_s(static_cast<uint32_t>(id)));
  }
  int32_t game_kind() override { return static_cast<int32_t>(c.kind); }
  bool game_campaign(int32_t* op, int32_t* mission) override {
    *op = static_cast<int32_t>(c.op);
    *mission = static_cast<int32_t>(c.mission);
    return true;
  }
  const char* game_text(int32_t slot) override {
    if (!call("game.vt", {slot})) return nullptr;
    return "Arena";
  }
  bool mission_failed() override { return c.failed != 0; }
  bool mission_cheated() override { return c.cheat != 0; }
  float mission_time() override { return zgame::from_bits(c.time); }
  Handle mission_salvage_store() override { return c.salvage; }
  std::string message_format_is(int32_t id, int32_t a, const char* s) override {
    const std::string text = s ? s : "";
    if (!call("0x00575fd0", {id, a, text.c_str()})) return std::string();
    return ("F" + hex_s(static_cast<uint32_t>(id)) + ":" + std::to_string(a) + "," + text).substr(0, 0xff);
  }

  std::vector<LanceMech> debrief_lance() override { return c.lance; }
  uint32_t player_allies() override { return c.allies; }
  std::vector<Objective> debrief_objectives(int32_t which) override { return c.objectives[which & 1]; }
  const char* objective_text(Handle obj) override {
    if (!call("0x0045e080", {obj})) return nullptr;
    return keep("obj" + hex_s(obj));
  }
  int32_t campaign_score() override { return static_cast<int32_t>(c.c4); }
  int32_t campaign_record(int32_t i) override { return static_cast<int32_t>(c.b4[i & 3]); }
  bool mission_ended() override { return c.ended != 0; }
  void action_rounds(int32_t* mode, int32_t* rounds, int32_t* waves) override {
    *mode = static_cast<int32_t>(c.ia44);
    *rounds = x6c;
    *waves = x70;
  }

  void pilot_save_now() override { call("0x00478ce0", {}); }
  void campaign_reset_lancemates() override { call("0x00435240", {0}); }
  void campaign_set_b0(uint8_t v) override { b0 = v; }
  void action_next_round() override {
    x6c = c.ia44 == 2 ? static_cast<int32_t>(c.round) + 1 : 1;
    x70 = 0;
  }
  void debrief_voice_play(uint32_t state) override { call("0x0047abe0", {state}); }
  void replace_state(int32_t n) override { call("0x00590250", {n}); }
  void replace_state_at(Handle s) override { call("0x00590250", {s}); }
  void mission_cleanup(int32_t unload) override { call("0x00491780", {unload}); }
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

int32_t ref(const Dump& d, const ztwod::Object2D* o) {
  for (size_t i = 0; i < d.objs.size(); ++i)
    if (d.objs[i] == o) return static_cast<int32_t>(i);
  return -1;
}

std::string game_line(const DbEngine& e) {
  return "game b0=" + std::to_string(e.b0) + " x6c=" + std::to_string(e.x6c) + " x70=" + std::to_string(e.x70) + "\n";
}

// The dump test_debriefing.py builds from the emulator's memory.
void dump_screen(mech3::DebriefScreen& s, const DbEngine& e, Dump& d) {
  std::ostringstream& o = d.o;
  o << "screen won=" << int(*s.won) << std::hex << " store=" << s.salvage.store
    << " x218=" << s.salvage.x218 << " font=" << s.salvage.title_font << std::dec << '\n';
  mech3::HelpButton* bs[] = {&s.continue_btn, &s.replay_btn, &s.abort_btn, &s.rank};
  const char* names[] = {"CONTINUE", "REPLAY", "ABORT", "RANK"};
  for (int i = 0; i < 4; ++i) {
    o << names[i] << " id=" << bs[i]->id << std::hex << " en=" << bs[i]->enabled << " blink=" << int(bs[i]->blinking)
      << std::dec << " help=";
    bool first = true;
    for (ztwod::Text* t : bs[i]->help) {
      o << (first ? "" : ",") << 'o' << ref(d, t);
      first = false;
    }
    o << '\n';
  }
  for (int i = 0; i < 4; ++i) {
    const mech3::LanceCard& c = s.cards[i];
    o << "card" << i << " mech=" << c.mech << std::hex << " veh=" << c.vehicle << std::dec
      << " display=" << (c.display ? 1 : 0) << " name=o" << ref(d, &c.name) << '\n';
  }
  o << "results score=" << s.score << " lance=" << s.lance_scores[0] << ',' << s.lance_scores[1] << ','
    << s.lance_scores[2] << ',' << s.lance_scores[3] << " count=" << mech3::debrief_lance_count()
    << " texts=" << s.texts.size() << '\n';
  o << game_line(e);
}

}  // namespace

extern "C" {

int probe_debriefing_run(const char* reader, const char* setup, const char* plan, char* out, int size) {
  DbHost host;
  ztwod::set_host(&host);
  reset();
  DbEngine engine;
  {
    std::istringstream ss(setup);
    Setup& c = engine.c;
    ss >> std::hex >> c.kind >> c.op >> c.mission >> c.failed >> c.cheat >> c.time >> c.salvage >> c.won >> c.ia44 >>
        c.b0 >> c.round >> c.x6c >> c.x70 >> c.c4 >> c.b4[0] >> c.b4[1] >> c.b4[2] >> c.b4[3] >> c.ended >> c.allies;
    engine.b0 = static_cast<uint8_t>(c.b0);
    engine.x6c = static_cast<int32_t>(c.x6c);
    engine.x70 = static_cast<int32_t>(c.x70);
    uint32_t n = 0;
    ss >> n;
    for (uint32_t i = 0; i < n; ++i) {
      mech3::Engine::LanceMech m;
      uint32_t player = 0, team = 0, type = 0, name = 0, st[6] = {};
      ss >> m.mech >> player >> team >> type >> st[0] >> st[1] >> st[2] >> st[3] >> st[4] >> st[5] >> name;
      m.player = player != 0;
      m.team = team;
      m.type = static_cast<int32_t>(type);
      for (int k = 0; k < 6; ++k) m.stats[k] = static_cast<int32_t>(st[k]);
      m.name = kNames[name % 5];
      c.lance.push_back(m);
    }
    for (auto& list : c.objectives) {
      ss >> n;
      for (uint32_t i = 0; i < n; ++i) {
        mech3::Engine::Objective o;
        uint32_t done = 0, shown = 0;
        ss >> o.obj >> done >> shown;
        o.done = done != 0;
        o.shown = shown != 0;
        list.push_back(o);
      }
    }
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
  uint8_t won = static_cast<uint8_t>(engine.c.won);
  std::unique_ptr<mech3::DebriefScreen> s;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> op;
    if (op == "S") {
      s = std::make_unique<mech3::DebriefScreen>(engine, 0x608e70, static_cast<int32_t>(engine.c.op),
                                                 static_cast<int32_t>(engine.c.mission), &won);
    } else if (!s) {
      continue;
    } else if (op == "F") {
      const float dt = fhex(ls);
      g_mouse.push_back(read_mouse(ls));
      s->update(dt);
      ++g_frame;
    } else if (op == "B") {
      s->on_button(static_cast<int32_t>(hex(ls)), 0, 0);
    } else if (op == "P" || op == "N" || op == "O") {
      ztwod::Control* parts[] = {&s->continue_btn, &s->replay_btn, &s->abort_btn, &s->cards[0], &s->cards[1],
                                 &s->cards[2], &s->cards[3], &s->rank};
      const uint32_t k = hex(ls);
      if (k >= 8) continue;
      if (op == "P") parts[k]->on_left_up();
      else if (op == "N") parts[k]->on_enter();
      else parts[k]->on_leave();
    } else if (op == "E") {
      Dump d;
      dump_dialog(*s, d);
      dump_screen(*s, engine, d);
      dump += d.o.str();
    } else if (op == "U") {
      s.reset();
      dump += game_line(engine);
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
