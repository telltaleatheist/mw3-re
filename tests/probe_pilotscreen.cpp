// Probe for test_pilotscreen.py (docs/menus.md, The pilot screen): pilots
// made and saved, the Pilot State and its screen built from dialog.zrd's
// PILOTDIALOG (or a synthetic one) on the ported zTwoD, then a plan of
// frames, presses, typed characters, the bars' and lists' calls, renames and
// the state's halves; every call out recorded through the calltrace callback,
// the screen and the pilots dumped as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/pilot.h"
#include "mech3/pilot_menu.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using mech3::Arg;

struct PsHost : RecordingHost {
  void (*cb)(int32_t, int32_t, uintptr_t) = nullptr;
  uintptr_t ctx = 0;
  std::map<uintptr_t, uint32_t> offsets;  // the bars' places in the original's screen
  void set_char_callback(void (*c)(int32_t, int32_t, uintptr_t), uintptr_t x) override {
    cb = c;
    ctx = x;
    call("0x0051a9a0", {c ? 1u : 0u, x ? offsets[x] : 0u});
  }
};

std::string hex_s(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}

// The formatted line both sides make (test_pilotscreen.py's 0x00575fd0 stub).
std::string formatted(int32_t id, std::initializer_list<int32_t> args) {
  std::vector<uint32_t> v;
  v.push_back(static_cast<uint32_t>(id));
  for (int32_t a : args) v.push_back(static_cast<uint32_t>(a));
  switch (v.size()) {
    case 1: call("0x00575fd0", {v[0]}); break;
    case 2: call("0x00575fd0", {v[0], v[1]}); break;
    default: call("0x00575fd0", {v[0], v[1], v[2]}); break;
  }
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

struct PsEngine : mech3::Engine {
  zrdr::Node* tree = nullptr;
  Handle screen_handle = 0;
  mech3::MenuScreen* screen = nullptr;

  uint32_t unported(const char* key, std::initializer_list<Arg> args) override { return call(key, args); }
  int32_t fast_menu() override { return static_cast<int32_t>(call("0x0040cec0", {}) & 0xff); }
  int32_t display_width() override { return static_cast<int32_t>(call("0x0040d760", {})); }
  int32_t display_height() override { return static_cast<int32_t>(call("0x0040d770", {})); }
  const char* message(int32_t id) override { return message_of(id); }
  std::string message_format_ints(int32_t id, std::initializer_list<int32_t> args) override {
    return formatted(id, args);
  }
  bool main_menu_background(zimg::Texture** image, uint8_t* moving) override {
    const uint32_t m = call("peek32", {0x609854});
    if (!m) return false;
    *image = tex(call("peek32", {m + 0x110ec}));
    *moving = static_cast<uint8_t>(call("peek8", {m + 0x111e8}));
    return true;
  }
  zrdr::Node* main_menu_definition() override { return tree; }
  mech3::PilotState* pilot_state = nullptr;
  void pilot_state_set_mode(Handle state, int32_t mode) override {
    if (pilot_state && state == pilot_state->addr()) pilot_state->set_mode(mode);
    else Engine::pilot_state_set_mode(state, mode);
  }
  void pop_state(uint32_t arg) override { call("0x005905a0", {arg}); }
  const char* lobby_player_name(Handle session) override {
    const uint32_t v = call("0x00588a60", {session});
    return keep("Lobby" + hex_s(v & 0xff));
  }
  Handle adopt_screen(mech3::MenuScreen* s, uint32_t) override {
    screen = s;
    screen_handle = 0x7b000000;
    return screen_handle;
  }
  uint32_t vcall(Handle obj, uint32_t slot, std::initializer_list<Arg> args) override {
    if (obj == screen_handle && screen && slot == 0x08) {  // the state's destructor
      delete screen;
      screen = nullptr;
      return 0;
    }
    return Engine::vcall(obj, slot, args);
  }
};

// The pilot module's calls out, as the original's stubs (test_pilotscreen.py).
struct PsPilotHooks : mech3::PilotHooks {
  void store_fill(mech3::Store&, int32_t game) override { call("0x00470e30", {game}); }
  int32_t store_add_mech(mech3::Store&, int32_t mech, const char* config) override {
    return static_cast<int32_t>(call("0x004733b0", {mech, config}));
  }
  const char* message(int32_t id) override { return message_of(id); }
  std::string format_ints(int32_t id, std::initializer_list<int32_t> args) override { return formatted(id, args); }
};

// A game state whose shell halves are the original's stubs (silent).
struct ProbeGameState : mech3::GameMenuState {
  using GameMenuState::GameMenuState;
  bool shell_enter() override { return true; }
  void shell_resume(uint32_t) override {}
  bool shell_update() override { return false; }
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

// The dump test_pilotscreen.py builds from the emulator's memory.
struct ScreenDump {
  Dump& d;
  void file_list(mech3::FileList& f, bool values) {
    d.control(&f);
    d.kv("selected", f.selected);
    d.kv("top", f.top);
    d.kv("size", f.size);
    d.r("rect", f.rect);
    d.kv("arrow", f.arrow_lit);
    d.kv("inside", f.inside);
    int lit = -1;
    for (size_t i = 0; i < f.rows.size(); ++i)
      if (f.rows[i] == f.lit) lit = static_cast<int>(i);
    if (f.lit && f.lit == f.selected_row) lit = 100;
    d.kv("lit", lit);
    d.kv("rep", f.repeating);
    d.kv("n", f.item_count());
    for (mech3::FileList::Item* it : f.items) {
      d.s("item", it->text);
      if (values) d.kv("v", it->value);
    }
    for (size_t i = 0; i < f.rows.size(); ++i) {
      d.o << "\n row" << i << ' ';
      d.text(f.rows[i]);
      d.kv("index", f.rows[i]->index);
    }
    if (f.selected_row) {
      d.o << "\n selrow ";
      d.text(f.selected_row);
      d.kv("index", f.selected_row->index);
    }
  }
  void bar(mech3::ShellEditBar& b, uint8_t active) {
    d.control(&b);
    d.kv("focus", b.focus);
    d.kv("active", active);
    d.kv("reading", b.reading);
    d.kv("lx", b.label_x);
    d.kv("ly", b.label_y);
    d.kv("size", b.editor.size);
    d.kv("cursor", b.editor.cursor);
    d.s("buf", b.editor.buf);
    d.s("reject", b.editor.reject);
  }
  void cycle(mech3::CycleButton& c) {
    d.control(&c);
    d.kv("index", c.index);
    for (mech3::CycleButton::Item* it : c.items) {
      d.s("item", it->text);
      d.kv("v", it->value);
    }
  }
};

// The game states' dump (test_pilotscreen.py dump_games): the app's queued
// replaces, its two flags, and each state's +0x10, +0x14, +0x19.
void dump_games(std::ostringstream& o, mech3::GameMenuState* g) {
  mech3::Mech3App& ap = mech3::app();
  o << "queue [";
  bool first = true;
  for (const auto& p : ap.pending_list()) {
    if (!first) o << ", ";
    const auto* st = dynamic_cast<const mech3::State*>(p.state);
    o << '(' << static_cast<int>(p.type) << ", " << (st ? st->addr() : 0) << ", " << p.arg << ')';
    first = false;
  }
  o << "] fmv=" << std::hex << int(ap.fmv()) << " ia=" << int(ap.instant_action_arg()) << std::dec << '\n';
  for (int k = 0; k < 4; ++k) {
    uint32_t a10 = 0, a14 = 0, b19 = 0;
    if (g && g->kind == k) {
      a10 = k == mech3::GameMenuState::kCampaign ? g->movie : k == mech3::GameMenuState::kMultiplayer ? static_cast<uint32_t>(g->page) : 0;
      a14 = static_cast<uint32_t>(g->want);
      b19 = g->x19;
    }
    o << "game" << k << std::hex << " movie=" << a10 << " want=" << a14 << " x19=" << b19 << std::dec << '\n';
  }
}

std::string pilot_name(const mech3::Pilot* p) { return p ? "\"" + std::string(p->name) + "\"" : "-"; }

void dump_pilots(std::ostringstream& o) {
  o << "list " << mech3::pilots().size() << '\n';
  for (mech3::Pilot* p : mech3::pilots()) {
    int kind = p->current ? p->current->type() : -99;
    int idx = -1;
    for (size_t i = 0; i < p->campaigns.size(); ++i)
      if (p->campaigns[i] == p->current) idx = static_cast<int>(i);
    o << "pilot \"" << p->name << "\" dirty=" << int(p->dirty) << " game=" << kind << " campaign=" << idx
      << " ncamp=" << p->campaign_count() << " training=" << p->training->mission << '\n';
  }
}

void dump_screen(mech3::PilotScreen& s, Dump& d) {
  ScreenDump sd{d};
  d.o << "screen mode=" << s.mode << " chosen=" << pilot_name(s.chosen)
      << " current=" << pilot_name(mech3::current_pilot()) << " x200=" << int(s.pilots.x200)
      << " pstate=" << std::hex << s.state << std::dec;
  d.o << "\npilots ";
  sd.file_list(s.pilots, false);
  d.o << "\ngames ";
  sd.file_list(s.games, true);
  d.o << "\nnewbar ";
  sd.bar(s.new_bar, s.new_bar.active);
  d.o << "\nrenamebar ";
  sd.bar(s.rename_bar, s.rename_bar.active);
  d.o << "\nscenarios ";
  sd.cycle(s.scenarios);
  d.o << "\ndesc ";
  sd.cycle(s.descriptions);
  d.o << "\ncomplete ";
  sd.cycle(s.completed);
  d.o << '\n';
  dump_pilots(d.o);
}

void write_file(const std::string& game_path, const std::string& data) {
  const std::string host = zutl::write_path(game_path.c_str());
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(host).parent_path(), ec);
  if (std::FILE* f = std::fopen(host.c_str(), "r+b")) {  // in place: the name keeps its case
    std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    std::filesystem::resize_file(host, data.size(), ec);
  } else if (std::FILE* g = std::fopen(host.c_str(), "wb")) {
    std::fwrite(data.data(), 1, data.size(), g);
    std::fclose(g);
  }
}

}  // namespace

extern "C" {

// Opens `reader` as zReader's archive and its dialog.zrd as the main menu's
// tree, with `dir` as the write dir; builds the Pilot State at the original's
// address `state`, runs `plan` (test_pilotscreen.py's lines) and dumps into
// `out`. Returns the dump's length, or -1.
int probe_pilotscreen_run(const char* reader, const char* dir, uint32_t state, const char* plan, char* out,
                          int size) {
  PsHost host;
  ztwod::set_host(&host);
  reset();
  PsEngine engine;
  PsPilotHooks hooks;
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
  std::unique_ptr<mech3::PilotState> st;
  std::unique_ptr<ProbeGameState> game;
  mech3::PilotScreen* s = nullptr;
  std::string dump;
  std::istringstream in(plan);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string op;
    ls >> op;
    std::string a, b;
    if (op == "C") {
      ls >> a;
      mech3::create_pilot(unhex(a).c_str());
    } else if (op == "N") {
      int c = 0, m = 0;
      ls >> a >> c >> m;
      if (mech3::Pilot* p = mech3::find_pilot(unhex(a).c_str())) p->campaigns.push_back(new mech3::CampaignGame(p, c, m));
    } else if (op == "V") {
      int g = 0, i = 0;
      ls >> a >> g >> i;
      const uint32_t off = hex(ls), v = hex(ls);
      mech3::Pilot* p = mech3::find_pilot(unhex(a).c_str());
      if (!p) continue;
      if (g == 2 && off >= 0x4c && off <= 0x68) {
        int32_t* st8[] = {&p->action_game->player_kills, &p->action_game->lancemate_kills,
                          &p->action_game->enemy_kills, &p->action_game->attack_wins,
                          &p->action_game->attack_losses, &p->action_game->defend_wins,
                          &p->action_game->defend_losses, &p->action_game->waves};
        *st8[(off - 0x4c) / 4] = static_cast<int32_t>(v);
      }
      if (g == 3 && off == 0x98) p->multi->kills = static_cast<int32_t>(v);
      if (g == 3 && off == 0x9c) p->multi->deaths = static_cast<int32_t>(v);
      if (g == 4 && off >= 0x3c && off <= 0x3f) p->training->passed[off - 0x3c] = static_cast<uint8_t>(v);
      if (g == 4 && off == 0x40) p->training->mission = static_cast<int32_t>(v);
    } else if (op == "B") {
      ls >> a >> b;
      write_file(unhex(a), unhex(b));
    } else if (op == "Z") {
      mech3::current_pilot() = nullptr;
      mech3::delete_other_pilots();
    } else if (op == "W" && !s) {
      ls >> a;
      if (mech3::Pilot* p = mech3::find_pilot(unhex(a).c_str())) p->save();
    } else if (op == "X" && !s && (ls >> a, !ls.fail())) {
      mech3::current_pilot() = mech3::find_pilot(unhex(a).c_str());
    } else if (op == "S") {
      st = std::make_unique<mech3::PilotState>(engine, state);
      engine.pilot_state = st.get();
      s = st->screen;
      if (s) {
        host.offsets[reinterpret_cast<uintptr_t>(&s->new_bar)] = 0x11c04;
        host.offsets[reinterpret_cast<uintptr_t>(&s->rename_bar)] = 0x11874;
      }
    } else if (op == "M") {
      if (st) st->set_mode(static_cast<int32_t>(hex(ls)));
    } else if (op == "Y") {
      if (st) st->prepare();
    } else if (op == "U") {
      engine.pilot_state = nullptr;
      st.reset();
      s = nullptr;
    } else if (op == "flags") {
      app->fmv() = static_cast<uint8_t>(hex(ls));
      app->instant_action_arg() = static_cast<uint8_t>(hex(ls));
    } else if (op == "g") {
      const auto k = static_cast<mech3::GameMenuState::Kind>(hex(ls));
      std::string what;
      ls >> what;
      if (what == "new") {
        game = std::make_unique<ProbeGameState>(k, hex(ls));  // at the original's address
      } else if (!game) {
        continue;
      } else if (what == "enter") {
        game->enter();
      } else if (what == "resume") {
        const uint32_t r = hex(ls);
        const bool no_pilot = r == 2 && (k == mech3::GameMenuState::kAction || k == mech3::GameMenuState::kTraining) &&
                              !mech3::current_pilot();
        if (!no_pilot) game->resume(r);
      } else if (what == "update") {
        if (k == mech3::GameMenuState::kCampaign || k == mech3::GameMenuState::kMultiplayer) game->update();
      } else if (what == "leave") {
        if (k == mech3::GameMenuState::kCampaign) game->leave();
      } else if (what == "escape") {
        if (k == mech3::GameMenuState::kAction || k == mech3::GameMenuState::kTraining) game->escape();
      }
    } else if (op == "E" && !s) {
      std::ostringstream o;
      o << "gone\n";
      dump_pilots(o);
      dump_games(o, game.get());
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
    } else if (op == "P") {
      ztwod::Object2D* const parts[] = {&s->cancel_btn, &s->accept, &s->delete_btn, &s->rename_btn, &s->new_bar,
                                        &s->rename_bar, &s->pilots, &s->games, &s->scenarios};
      const uint32_t k = hex(ls);
      if (k < 9) static_cast<ztwod::Control*>(parts[k])->on_left_up();
    } else if (op == "K") {  // zInput's text callback
      const int32_t ch = static_cast<int32_t>(hex(ls));
      const int32_t key = static_cast<int32_t>(hex(ls));
      if (host.cb) host.cb(ch, key, host.ctx);
    } else if (op == "X") {
      const uint32_t k = hex(ls), c = hex(ls);
      ztwod::EditBar* bar = k == 0 ? static_cast<ztwod::EditBar*>(&s->new_bar) : &s->rename_bar;
      if (c) bar->on_cancel();
      else bar->on_return();
    } else if (op == "L") {
      s->pilots.select(static_cast<int32_t>(hex(ls)));
    } else if (op == "G") {
      if (s->chosen || s->mode != 1) s->games.select(static_cast<int32_t>(hex(ls)));
    } else if (op == "Q") {
      s->scenarios.select(static_cast<int32_t>(hex(ls)));
    } else if (op == "R") {
      const uint32_t start = hex(ls), cancel = hex(ls);
      s->rename(start != 0, cancel != 0);
    } else if (op == "T") {
      const uint32_t k = hex(ls);
      ls >> a;
      (k == 0 ? static_cast<ztwod::EditBar&>(s->new_bar) : s->rename_bar).set_text(unhex(a).c_str());
    } else if (op == "O") {
      st->escape();
    } else if (op == "I") {
      s->reset();
    } else if (op == "W") {
      s->refresh(true, nullptr);
    } else if (op == "D") {
      s->dirty_all();
    } else if (op == "H") {
      s->restore_behind();
    } else if (op == "E") {
      Dump d;
      dump_dialog(*s, d);
      dump_screen(*s, d);
      dump_games(d.o, game.get());
      dump += d.o.str();
    }
  }
  game.reset();
  engine.pilot_state = nullptr;
  st.reset();
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
