// Probe for test_pocket_panel.py (docs/ai.md 17.6b): the pocket commander's
// panel (mech3/pocket_panel) built on the ported zTwoD over made-up
// lancemates and orders, then a plan of picks, mouse calls, refreshes and
// draws; every call out recorded through the calltrace callback, the panel
// dumped as text the way the test reads it from the emulator.
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_pocket.h"
#include "mech3/pocket_panel.h"
#include "mech3/sensors.h"
#include "probe_twod.h"

namespace {
using namespace probe_twod;
using namespace mech3;

std::vector<std::unique_ptr<Mech>> g_mates;
std::vector<std::string> g_mate_names;  // each one's vt+0xa4 ("-": none), given, not replayed
std::vector<uint32_t> g_handles;
int mate_index(const Mech* m) {
  for (size_t i = 0; i < g_mates.size(); ++i)
    if (g_mates[i].get() == m) return static_cast<int>(i);
  return -1;
}

char kind_of(PocketItem* p) {
  if (!p) return 'Z';
  if (dynamic_cast<PocketMateItem*>(p)) return 'M';
  return "AELSO"[static_cast<PocketOrderItem*>(p)->kind];
}

void dump_item(Dump& d, PocketItem* p) {
  const char k = kind_of(p);
  d.o << k << ' ';
  if (!p) return;
  d.kv("state", p->state);
  if (k == 'M') d.kv("unit", mate_index(static_cast<PocketMateItem*>(p)->unit));
  else d.kv("order", static_cast<PocketOrderItem*>(p)->order);
  d.fonttext(p);
}

void dump(PocketPanel& p, PocketControls& c, Dump& d) {
  for (ztwod::Object2D* o = p.head; o; o = o->next) d.objs.push_back(o);
  d.o << "panel ";
  d.kv("active", p.active);
  d.kv("mates", p.mates);
  d.kv("places", p.places);
  d.kv("picked", p.picked_count);
  d.kv("lh", p.line_h);
  d.r("rect", p.rect);
  d.kv("n", static_cast<int64_t>(p.items.size()));
  d.kv("x184", c.x184);
  d.kv("order", c.order);
  d.kv("cursor", pocket_globals().cursor);
  d.o << "\ncolours ";
  const PocketColours& col = pocket_colours();
  for (const PocketColour& x : col.state) d.kv("c", x.ref), d.kv("p", x.pixel);
  d.kv("y", col.yellow.ref), d.kv("yp", col.yellow.pixel), d.kv("w", col.white.ref), d.kv("wp", col.white.pixel);
  d.o << "\ntarget ";
  dump_item(d, &p.target);
  for (size_t i = 0; i < p.items.size(); ++i) {
    d.o << "\nitem" << i << ' ';
    dump_item(d, p.items[i]);
  }
  d.o << '\n';
}

}  // namespace

extern "C" {

// Runs `plan` (test_pocket_panel.py's lines) and dumps the panel into `out`
// before destroying it. Returns the dump's length, or -1.
int probe_pocket_panel_run(const char* plan, char* out, int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();
  g_mates.clear();
  g_mate_names.clear();
  g_handles.clear();
  PocketGlobals saved_globals = pocket_globals();
  pocket_globals() = PocketGlobals{};
  PanelCalls& pc = panel_calls();
  pc = PanelCalls{};
  pc.entity_mech = [](zgame::Handle e) -> Mech* {
    for (size_t i = 0; i < g_handles.size(); ++i)
      if (g_handles[i] == e) return g_mates[i].get();
    return nullptr;
  };
  pc.unit_name = [](Mech& m) -> const char* {
    const std::string& n = g_mate_names[static_cast<size_t>(mate_index(&m))];
    return n == "-" ? nullptr : n.c_str();
  };
  pc.node_entity = [](uint32_t node) -> zgame::Handle { return call("0x0040e920", {node}); };
  pc.entity_team = [](zgame::Handle t) -> uint32_t { return call("peek32", {t + 0xc0}); };
  pc.repair_bays = []() -> int32_t { return static_cast<int32_t>(call("0x0045a000", {})); };
  PocketCommands& cmds = pocket_commands();
  cmds = PocketCommands{};
  Mech player;
  PocketControls controls(nullptr, nullptr, 2, 1);
  controls.owner = &player;
  controls.order = 0;  // the test's made-up controls start at 0
  float cursor_place[3] = {};
  std::unique_ptr<PocketPanel> panel;
  std::string text;
  std::istringstream in(plan);
  std::string line;
  auto item = [&panel](int i) -> PocketItem* {
    if (i < 0) return &panel->target;
    return static_cast<size_t>(i) < panel->items.size() ? panel->items[static_cast<size_t>(i)] : nullptr;
  };
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    char op;
    ls >> op;
    if (op == 'c') {  // an order's row: its key, name (_ for spaces), order
      std::string k, n;
      int32_t o;
      ls >> k >> n >> o;
      PocketCommand& r = cmds.rows[cmds.count++];
      for (char& ch : n) if (ch == '_') ch = ' ';
      std::strncpy(r.key, k == "-" ? "" : k.c_str(), 0xf);
      std::strncpy(r.name, n.c_str(), 0x3f);
      r.order = o;
    } else if (op == 'V') {  // an entity: its handle, its vehicle's active, +0xbc, allies
      uint32_t h, act, xbc, allies;
      std::string name;
      ls >> std::hex >> h >> act >> xbc >> allies >> name;
      for (char& ch : name) if (ch == '_') ch = ' ';
      g_mate_names.push_back(name);
      auto m = std::make_unique<Mech>();
      m->active = act != 0;
      m->xbc = static_cast<uint8_t>(xbc);
      m->allies = allies;
      g_handles.push_back(h);
      g_mates.push_back(std::move(m));
    } else if (op == 'P') {  // the player's team and allies
      ls >> std::hex >> player.team >> player.allies;
    } else if (op == 'G') {  // the colours made (then a known table), the sounds, the marker
      uint32_t made, sel, roll, marker;
      ls >> std::hex >> made >> sel >> roll >> marker;
      PocketColours& col = pocket_colours();
      col = PocketColours{};
      col.made = made != 0;
      if (made) {
        for (int i = 0; i < 8; ++i) col.state[i] = {0x10101u * static_cast<uint32_t>(i + 1), static_cast<uint16_t>(i)};
        col.yellow = {0x111, 0x11}, col.white = {0x222, 0x22};
      }
      pocket_globals().select = static_cast<int32_t>(sel);
      pocket_globals().rollover = static_cast<int32_t>(roll);
      pocket_globals().marker = marker;
      ai::entity_list() = g_handles;
    } else if (op == 'K') {  // the mode's cursor node and place
      uint32_t node, place;
      ls >> std::hex >> node >> place;
      controls.x164 = node;
      controls.cursor_pos = place ? cursor_place : nullptr;
    } else if (op == 'N') {
      int32_t x, y, lh;
      ls >> x >> y >> lh;
      panel = std::make_unique<PocketPanel>(&controls, x, y, lh);
    } else if (op == 'M') {
      ls >> panel->m.x >> panel->m.y;
    } else if (op == 'p') {
      int i;
      ls >> i;
      call("pick", {panel->pick(i) ? 1 : 0});
    } else if (op == 'X') {
      break;
    } else if (op == 'u') {
      int d;
      ls >> d;
      panel->picked(d, panel->places);
    } else {
      int i;
      ls >> i;
      PocketItem* it = item(i);
      if (!it) continue;
      switch (op) {
        case 'e': it->on_enter(); break;
        case 'l': it->on_leave(); break;
        case 's': it->on_press(); break;
        case 'r': it->on_release(); break;
        case 'f': it->refresh(); break;
        case 'd': it->draw(); break;
      }
    }
  }
  Dump d;
  dump(*panel, controls, d);
  text = d.o.str();
  panel.reset();
  controls.owner = nullptr;
  ai::entity_list().clear();
  pc = PanelCalls{};
  cmds = PocketCommands{};
  pocket_globals() = saved_globals;
  ztwod::set_host(nullptr);
  if (static_cast<int>(text.size()) >= size) return -1;
  std::memcpy(out, text.data(), text.size() + 1);
  return static_cast<int>(text.size());
}

}  // extern "C"

namespace {
const uint32_t* g_bound;
const char* const* g_keys;
std::vector<std::string> g_msgs;
int g_nbound;
struct MessageHost : ztwod::Host {
  const char* message_text(uint32_t id) override {
    static const uint32_t kIds[6] = {2195, 2196, 2142, 2143, 2197, 2204};
    for (int i = 0; i < 6; ++i)
      if (kIds[i] == id) return g_msgs[static_cast<size_t>(i)].c_str();
    return "";
  }
};
}  // namespace

extern "C" {

// 0x00467d80 with the bindings (by call), each binding's key name and the
// six messages (by row, '\n' between): the rows into `out` (0x54 each).
int probe_pocket_load_commands(const uint32_t* bound, const char* const* keys, const char* msgs, char* out) {
  MessageHost host;
  ztwod::set_host(&host);
  g_bound = bound, g_keys = keys, g_nbound = 0;
  g_msgs.clear();
  std::istringstream in(msgs);
  std::string s;
  while (std::getline(in, s)) g_msgs.push_back(s);
  g_msgs.resize(6);
  PanelCalls& pc = panel_calls();
  pc = PanelCalls{};
  pc.key_binding = [](int32_t) -> zgame::Handle { return g_bound[g_nbound++]; };
  pc.key_name = [](zgame::Handle b, char* o, int32_t n) {
    for (int i = 0; i < 6; ++i)
      if (g_bound[i] == b) {
        std::strncpy(o, g_keys[i], static_cast<size_t>(n));
        return;
      }
  };
  PocketCommands& c = pocket_commands();
  c = PocketCommands{};
  pocket_load_commands();
  const int n = c.count;
  for (int i = 0; i < n; ++i) {
    std::memcpy(out + 0x54 * i, c.rows[i].key, 0x10);
    std::memcpy(out + 0x54 * i + 0x10, c.rows[i].name, 0x40);
    std::memcpy(out + 0x54 * i + 0x50, &c.rows[i].order, 4);
  }
  c = PocketCommands{};
  pc = PanelCalls{};
  ztwod::set_host(nullptr);
  return n;
}

}  // extern "C"
