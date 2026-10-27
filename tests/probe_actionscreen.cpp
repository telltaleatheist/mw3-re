// Probe for test_actionscreen.py (docs/menus.md, The Action screen): a pilot
// made with an Instant Action record, the mechs' loadouts, the kept choices
// ([0x00605820..]), then the Action State and its screen from dialog.zrd's
// ACTIONDIALOG (or a synthetic one) on the ported zTwoD; a plan of frames,
// presses, choices and steps, the state's enter and resume, the reset and
// the buttons; every call out recorded through the calltrace callback, the
// screen, the kept choices and the pilot's game dumped as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/action_menu.h"
#include "mech3/app.h"
#include "mech3/app_states.h"
#include "mech3/mission.h"
#include "mech3/mech_config.h"
#include "mech3/pilot.h"
#include "mech3/pilot_menu.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

std::string hex_s(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}

// The formatted lines both sides make (test_actionscreen.py's 0x00575fd0 stub).
std::string formatted(int32_t id, std::initializer_list<int32_t> args) {
  std::vector<uint32_t> v;
  v.push_back(static_cast<uint32_t>(id));
  for (int32_t a : args) v.push_back(static_cast<uint32_t>(a));
  if (v.size() == 1) call("0x00575fd0", {v[0]});
  else call("0x00575fd0", {v[0], v[1]});
  std::string s = "F" + hex_s(static_cast<uint32_t>(id)) + ":";
  bool first = true;
  for (int32_t a : args) {
    if (!first) s += ",";
    s += std::to_string(a);
    first = false;
  }
  return s.substr(0, 0xff);
}

const char* message_of(int32_t id) {
  if (!call("0x00576040", {id})) return nullptr;
  return keep("M" + hex_s(static_cast<uint32_t>(id)));
}

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

struct AsEngine : mech3::Engine {
  zrdr::Node* tree = nullptr;
  mech3::ActionScreen* screen = nullptr;
  Handle screen_handle = 0x7b000000;
  std::map<int32_t, std::unique_ptr<mech3::ConfigList>> loadouts;  // the plan's (D lines)
  std::map<int32_t, bool> no_dfn;

  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  const char* message(int32_t id) override { return message_of(id); }
  std::string message_format_ints(int32_t id, std::initializer_list<int32_t> args) override {
    return formatted(id, args);
  }
  const char* message_format(int32_t id, const char* arg) override {
    call("0x00575fd0", {static_cast<uint32_t>(id), arg ? arg : ""});
    return keep(("F" + hex_s(static_cast<uint32_t>(id)) + ":" + (arg ? arg : "")).substr(0, 0xff));
  }
  bool main_menu_background(zimg::Texture** image, uint8_t* moving) override {
    const uint32_t m = call("peek32", {0x609854});
    if (!m) return false;
    *image = tex(call("peek32", {m + 0x110ec}));
    *moving = static_cast<uint8_t>(call("peek8", {m + 0x111e8}));
    return true;
  }
  zrdr::Node* main_menu_definition() override { return tree; }
  void pop_state(uint32_t arg) override { call("0x005905a0", {arg}); }
  void replace_state(int32_t n) override { call("0x00590250", {n == 6 ? 0x608f18u : 0u, 0u}); }
  const mech3::ConfigList* mech_loadouts(int32_t mech) override {
    call("0x004846a0", {static_cast<uint32_t>(mech)});
    if (mech < 0 || mech >= 19 || no_dfn[mech]) return nullptr;
    auto& l = loadouts[mech];
    if (!l) l = std::make_unique<mech3::ConfigList>(mech, 0);
    return l.get();
  }
  bool file_exists(const char* path) override { return call("0x00576080", {path}) & 0xff; }
  Handle construct(uint32_t ctor, std::initializer_list<Arg> args) override {
    if (ctor != 0x488bc0) return Engine::construct(ctor, args);
    screen = new mech3::ActionScreen(*this, args.begin()->v);
    return screen_handle;
  }
  void action_open(Handle s) override {
    if (s == screen_handle && screen) screen->reset();
  }
  void pilot_state_set_mode(Handle state, int32_t mode) override {
    call("0x00488ad0", {state, static_cast<uint32_t>(mode)});
  }
  void push_state(Handle state, uint32_t arg) override { call("0x00590400", {state, arg}); }
  void action_tally() override {}  // the original's loop runs over an empty zEntity list
};

// The pilot module's calls out, as the original's stubs (test_actionscreen.py).
struct AsPilotHooks : mech3::PilotHooks {
  void store_fill(mech3::Store&, int32_t game) override { call("0x00470e30", {game}); }
  int32_t store_add_mech(mech3::Store&, int32_t mech, const char* config) override {
    return static_cast<int32_t>(call("0x004733b0", {mech, config}));
  }
  const char* message(int32_t id) override { return message_of(id); }
  std::string format_ints(int32_t id, std::initializer_list<int32_t> args) override { return formatted(id, args); }
};

// The Action State whose shell halves are the original's stubs (silent).
struct ProbeActionState : mech3::GameMenuState {
  ProbeActionState(Handle addr) : GameMenuState(kAction, addr) {}
  bool shell_enter() override { return true; }
  void shell_resume(uint32_t) override {}
  bool shell_update() override { return false; }
};

// The dump test_actionscreen.py builds from the emulator's memory.
void cycle(Dump& d, mech3::ActionCycle& c, bool lineup, bool values = true) {
  d.control(&c);
  d.kv("index", c.index);
  d.kv("click", c.click_steps);
  d.kv("pen", c.prev.enabled);
  d.kv("nen", c.next.enabled);
  d.kv("f", c.font);
  d.kv("fi", c.inactive_font);
  if (lineup) {
    auto& l = static_cast<mech3::LineupCycle&>(c);
    d.kv("ox", l.offset_x);
    d.kv("oy", l.offset_y);
  }
  for (mech3::CycleButton::Item* it : c.items) {
    d.s("item", it->text);
    if (values) d.kv("v", it->value);  // (a loadout's is its address in the exe)
  }
}

void dump_setup(std::ostringstream& o) {
  const mech3::ActionSetup& k = mech3::action_setup();
  o << "setup" << std::hex;
  for (int32_t v : k.g820) o << ' ' << static_cast<uint32_t>(v);
  for (int32_t v : k.g838) o << ' ' << static_cast<uint32_t>(v);
  for (int32_t v : k.g858) o << ' ' << static_cast<uint32_t>(v);
  o << ' ' << static_cast<uint32_t>(k.g874) << ' ' << static_cast<uint32_t>(k.g878) << ' '
    << static_cast<uint32_t>(k.g87c) << std::dec << '\n';
}

void dump_game(std::ostringstream& o) {
  mech3::Pilot* p = mech3::current_pilot();
  mech3::ActionGame* g = p ? p->action_game : nullptr;
  if (!g) {
    o << "game -\n";
    return;
  }
  auto str = [](const char* s) { return std::string(s ? s : "-"); };
  o << std::hex << "game mode=" << static_cast<uint32_t>(g->mode) << " fb=" << int(g->field_bases)
    << " x6c=" << static_cast<uint32_t>(g->x6c) << " x70=" << static_cast<uint32_t>(g->x70)
    << " x74=" << static_cast<uint32_t>(g->x74) << " x78=" << static_cast<uint32_t>(g->x78)
    << " count=" << static_cast<uint32_t>(g->count) << " slot=" << static_cast<uint32_t>(g->slot.mech)
    << " cur=" << (p->current ? p->current->type() : -1) << " stats=" << g->player_kills << ',' << g->lancemate_kills
    << ',' << g->enemy_kills << ',' << g->attack_wins << ',' << g->attack_losses << ',' << g->defend_wins << ','
    << g->defend_losses << ',' << g->waves << " app=" << int(mech3::app().reopen_instant_action()) << ','
    << int(mech3::app().instant_action_arg()) << "," << int(mech3::app().reopen_training()) << " tr="
    << int(p->training->passed[0]) << int(p->training->passed[1]) << int(p->training->passed[2])
    << int(p->training->passed[3]) << std::dec << " t38=" << str(g->x38)
    << " t3c=" << str(g->x3c) << " t40=" << str(g->x40);
  for (int i = 0; i < 10; ++i) {
    const mech3::GameSlot& s = g->lineup[i];
    o << " [" << s.mech << ',' << int(g->side[i]) << ',' << (s.config && s.config->name ? s.config->name : "-") << ','
      << s.skin << ']';
  }
  o << '\n';
}

void dump_screen(mech3::ActionScreen& s, Dump& d) {
  d.o << "screen fixed=" << int(s.fixed) << " wave=" << s.wave << " filling=" << s.filling;
  d.o << "\nworlds ";
  cycle(d, s.worlds, false);
  d.o << "\nscenarios ";
  cycle(d, s.scenarios, false);
  d.o << "\ndesc ";
  cycle(d, s.descriptions, false);
  d.o << "\netitle ";
  cycle(d, s.enemy_title, false);
  for (int i = 0; i < 5; ++i) {
    d.o << "\nally" << i << ' ';
    cycle(d, s.allies[i], true);
  }
  for (int i = 0; i < 6; ++i) {
    d.o << "\nenemy" << i << ' ';
    cycle(d, s.enemies[i], true);
    d.o << "\nconfig" << i << ' ';
    cycle(d, s.enemy_configs[i], true, false);
  }
  d.o << "\nwaves ";
  cycle(d, s.waves, true);
  d.o << '\n';
}

}  // namespace

extern "C" {

// Opens `reader` as zReader's archive and its dialog.zrd as the main menu's
// tree, `dir` as the write dir; builds the Action State at the original's
// address `state`, runs `plan` (test_actionscreen.py's lines) and dumps into
// `out`. Returns the dump's length, or -1.
int probe_actionscreen_run(const char* reader, const char* dir, uint32_t state, const char* plan, char* out,
                           int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();
  AsEngine engine;
  AsPilotHooks hooks;
  mech3::set_pilot_hooks(&hooks);
  zutl::Mounts saved = zutl::mounts();
  zutl::mounts().read = {dir};
  zutl::mounts().write = dir;
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
  engine.tree = zrdr::load("dialog.zrd");
  ztwod::set_page_flip(true);
  mech3::current_pilot() = nullptr;
  mech3::action_setup() = mech3::ActionSetup{};
  mech3::missions() = mech3::Missions{};
  std::unique_ptr<ProbeActionState> st;
  mech3::ActionScreen* s = nullptr;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> op;
    std::string a;
    if (op == "C") {  // the pilot, chosen, Instant Action its game
      ls >> a;
      mech3::Pilot* p = mech3::create_pilot(unhex(a).c_str());
      mech3::current_pilot() = p;
      if (p) p->select_game(mech3::kInstantAction);
    } else if (op == "V") {  // a field of the pilot's game: +0x68 waves, +0x24 the slot's mech
      const uint32_t off = hex(ls), v = hex(ls);
      mech3::Pilot* p = mech3::current_pilot();
      if (!p) continue;
      mech3::ActionGame* g = p->action_game;
      int32_t* stats[] = {&g->player_kills, &g->lancemate_kills, &g->enemy_kills, &g->attack_wins,
                          &g->attack_losses, &g->defend_wins, &g->defend_losses, &g->waves};
      if (off >= 0x4c && off <= 0x68) *stats[(off - 0x4c) / 4] = static_cast<int32_t>(v);
      if (off == 0x44) g->mode = static_cast<int32_t>(v);
      if (off == 0x24) g->slot.mech = static_cast<int32_t>(v);
    } else if (op == "K") {  // the kept choices: index in [0x00605820..0x0060587f] by dword
      const uint32_t i = hex(ls), v = hex(ls);
      mech3::ActionSetup& k = mech3::action_setup();
      int32_t* w[] = {&k.g820[0], &k.g820[1], &k.g820[2], &k.g820[3], &k.g820[4], nullptr, &k.g838[0],
                      &k.g838[1], &k.g838[2], &k.g838[3], &k.g838[4], &k.g838[5], nullptr, nullptr,
                      &k.g858[0], &k.g858[1], &k.g858[2], &k.g858[3], &k.g858[4], &k.g858[5], nullptr,
                      &k.g874, &k.g878, &k.g87c};
      if (i < 24 && w[i]) *w[i] = static_cast<int32_t>(v);
    } else if (op == "D") {  // a mech's loadouts: D mech -  (no definition) | D mech t0 t1 ... (training flags)
      const int32_t mech = static_cast<int32_t>(hex(ls));
      std::string t;
      auto l = std::make_unique<mech3::ConfigList>(mech, 0);
      int i = 0;
      while (ls >> t) {
        if (t == "-") {
          engine.no_dfn[mech] = true;
          break;
        }
        auto* c = new mech3::Config(mech, ("L" + std::to_string(mech) + "_" + std::to_string(i++)).c_str());
        c->training = static_cast<uint8_t>(std::stoi(t));
        l->configs.push_back(c);
      }
      engine.loadouts[mech] = std::move(l);
    } else if (op == "Z") {  // the mission's end: won or lost ([0x00609925]), lost ([0x00609926])
      mech3::missions().ended = static_cast<uint8_t>(hex(ls));
      mech3::missions().aborting = static_cast<uint8_t>(hex(ls));
    } else if (op == "J") {  // the IA Dummy State entered (0x0047ce80)
      mech3::states().ia_dummy.enter();
    } else if (op == "TR") {  // the TR Dummy State entered (0x00491ab0) with that training mission
      const uint32_t m = hex(ls);
      if (mech3::Pilot* p = mech3::current_pilot()) {
        p->select_game(mech3::kTraining);
        p->training->mission = static_cast<int32_t>(m);
      }
      mech3::states().tr_dummy.enter();
    } else if (op == "S") {
      st = std::make_unique<ProbeActionState>(state);
      s = engine.screen;
    } else if (op == "Y") {
      if (st) st->enter();
    } else if (op == "R") {
      const uint32_t r = hex(ls);
      if (st && !(r == 2 && !mech3::current_pilot())) st->resume(r);
    } else if (op == "flags") {
      app->instant_action_arg() = static_cast<uint8_t>(hex(ls));
    } else if (op == "E" && !s) {
      std::ostringstream o;
      dump_setup(o);
      dump_game(o);
      dump += o.str();
    } else if (!s) {
      continue;
    } else if (op == "A") {
      s->set_active(static_cast<int32_t>(hex(ls)));
    } else if (op == "F") {
      const float dt = fhex(ls);
      g_mouse.push_back(read_mouse(ls));
      s->update(dt);
      ++g_frame;
    } else if (op == "I") {
      s->reset();
    } else if (op == "B") {
      s->on_button(static_cast<int32_t>(hex(ls)), 1, 0);
    } else if (op == "P" || op == "Q" || op == "T") {  // a part: pressed, a choice, a step
      const uint32_t k = hex(ls), v = hex(ls);
      mech3::CycleButton* parts[24] = {&s->worlds, &s->scenarios, &s->descriptions, &s->waves};
      for (int i = 0; i < 5; ++i) parts[4 + i] = &s->allies[i];
      for (int i = 0; i < 6; ++i) parts[9 + i] = &s->enemies[i], parts[15 + i] = &s->enemy_configs[i];
      ztwod::Control* c = k < 21 ? parts[k] : k == 21 ? static_cast<ztwod::Control*>(&s->cancel_btn)
                                            : k == 22 ? static_cast<ztwod::Control*>(&s->accept_btn) : nullptr;
      if (!c) continue;
      if (op == "P") c->on_left_up();
      else if (k < 21 && op == "Q") parts[k]->select(static_cast<int32_t>(v));
      else if (k < 21) parts[k]->step(static_cast<int32_t>(v));
    } else if (op == "W") {
      s->show_wave(static_cast<int32_t>(hex(ls)));
    } else if (op == "M") {
      s->show_custom();
    } else if (op == "E") {
      Dump d;
      dump_dialog(*s, d);
      dump_screen(*s, d);
      dump_setup(d.o);
      dump_game(d.o);
      dump += d.o.str();
    }
  }
  {
    probe_app::Mute mute;  // the cleanup is the probe's, not the plan's
    st.reset();
    delete engine.screen;
    engine.screen = nullptr;
  }
  mech3::current_pilot() = nullptr;
  while (!mech3::pilots().empty()) {  // anything left goes quietly (not saved)
    mech3::Pilot* p = mech3::pilots().front();
    mech3::pilots().pop_front();
    p->dirty = 0;
    delete p;
  }
  if (engine.tree) zrdr::free_node(engine.tree);
  zrdr::close_archives(true);
  zutl::mounts() = saved;
  mech3::set_pilot_hooks(nullptr);
  zfix::options().per_second = per_second;
  zfix::options().bugs = bugs;
  mech3::set_app(nullptr);
  ztwod::set_host(nullptr);
  if (static_cast<int>(dump.size()) >= size) return -1;
  std::memcpy(out, dump.data(), dump.size() + 1);
  return static_cast<int>(dump.size());
}

}  // extern "C"
