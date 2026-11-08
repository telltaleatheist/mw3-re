// Probe for test_pm_pocket_panel.py: Pirate's Moon's pocket commander panel (mech3/pocket_panel: its constructor
// 0x00469dc0 with the lancemates' lines 0x00469a70 and the orders' 0x00469bf0, the lines' virtuals, the pick, the
// count, the destructor) built on the ported zTwoD over made-up lancemates, then a plan of picks, mouse calls and
// refreshes; every call out recorded through the calltrace callback, the panel dumped as text the way the test reads
// it from the emulator.
#include <cstdint>
#include <cstdio>
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
std::vector<std::string> g_mate_names;  // each one's vt+0xb0 ("-": none), given, not replayed
std::vector<uint32_t> g_handles;
const char* const kActions[3] = {"Attack my target", "Form on me", "Hold"};  // the test's ACTIONS

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

void wire(PanelCalls& pc) {
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
  // The keys (0x00536a60, 0x00536c90), the actions' names (0x00536c70) and the pick's binding (0x00536c00,
  // 0x00536c30): the test's stubs, whose answers say what they wrote.
  pc.key_binding = [](int32_t action) -> zgame::Handle { return call("key_of", {action}); };
  pc.key_name = [](zgame::Handle k, char* out, int32_t n) -> const char* {
    if (!call("key_name", {k, n})) return "";
    std::snprintf(out, static_cast<size_t>(n), "K%x", k);
    return out;
  };
  pc.command_text = [](int32_t action, char* out, int32_t n) -> const char* {
    const uint32_t v = call("action_name", {action, n});
    if (!v) return nullptr;
    std::snprintf(out, static_cast<size_t>(n), "%s", kActions[(v - 1) % 3]);
    return out;
  };
  pc.bind_pick = [](int32_t action, int32_t message, uint32_t key) {
    call("bind", {action, message, key});
    call("callback", {action});
  };
}

}  // namespace

extern "C" {

// Runs `plan` (test_pm_pocket_panel.py's lines) and dumps the panel into `out` before destroying it. Returns the
// dump's length, or -1.
int probe_pm_pocket_panel_run(const char* plan, char* out, int size) {
  RecordingHost host;
  ztwod::set_host(&host);
  reset();
  g_mates.clear();
  g_mate_names.clear();
  g_handles.clear();
  PocketGlobals saved_globals = pocket_globals();
  pocket_globals() = PocketGlobals{};
  PanelCalls& pc = panel_calls();
  const PanelCalls saved_pc = pc;
  wire(pc);
  PocketCommands& cmds = pocket_commands();
  cmds = PocketCommands{};
  Mech player;
  PocketControls controls(nullptr, nullptr, 2, 1);
  controls.owner = &player;
  controls.order = 0;  // the test's made-up controls start at 0
  float cursor_place[3] = {};
  std::unique_ptr<PocketPanel> panel;
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
    if (op == 'V') {  // an entity: its handle, its vehicle's active, +0xbc, allies, name
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
    } else if (op == 'P') {  // the player's team, allies and enemies
      ls >> std::hex >> player.team >> player.allies >> player.enemies;
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
    } else if (op == 'u') {
      int d;
      ls >> d;
      panel->picked(d, panel->places);
    } else if (op == 'X') {
      break;
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
  const std::string text = d.o.str();
  panel.reset();
  controls.owner = nullptr;
  ai::entity_list().clear();
  pc = saved_pc;
  cmds = PocketCommands{};
  pocket_globals() = saved_globals;
  ztwod::set_host(nullptr);
  if (static_cast<int>(text.size()) >= size) return -1;
  std::memcpy(out, text.data(), text.size() + 1);
  return static_cast<int>(text.size());
}

}  // extern "C"
