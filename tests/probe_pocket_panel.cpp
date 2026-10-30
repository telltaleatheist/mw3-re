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

#include "gamez/zmath/zmth_matrix.h"
#include "mech3/zai_brain.h"
#include "mech3/mech.h"
#include "mech3/mech_pocket.h"
#include "mech3/pocket_panel.h"
#include "mech3/sensors.h"
#include "probe_twod.h"
#include "gamez/zclass/cls_di.h"
#include "mech3/mission_map.h"

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

// The frame's world (test_pocket_frame.py): its answers come through the
// trace, its objects' fields through peek.
bool g_frame_test = false, g_viewed = false, g_marker_active = false, g_net = false;
ai::Contact g_target;
std::deque<std::string> g_texts;
const char* text_at(uint32_t a) {
  if (!a) return nullptr;
  std::string s;
  for (uint32_t i = 0; i < 256; ++i) {
    const char ch = static_cast<char>(call("peek8", {a + i}));
    if (!ch) break;
    s += ch;
  }
  g_texts.push_back(s);
  if (g_texts.size() > 64) g_texts.pop_front();
  return g_texts.back().c_str();
}
uint32_t fake(const float* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
const float* unfake(uint32_t a) { return reinterpret_cast<const float*>(static_cast<uintptr_t>(a)); }
PocketControls* g_ctl;
const float* g_k_place;  // the K op's made-up place
uint32_t place_arg(const float* p) { return p == g_ctl->place ? 1u : p && p == g_k_place ? 2u : fake(p); }

struct FrameHost : RecordingHost {
  const char* message_text(uint32_t id) override { return text_at(call("0x00576040", {id})); }
  void line_list(const int32_t* points, int32_t count, const Rect* clip, uint32_t pixel) override {
    call("0x00562cb0", {count, clip->left, clip->top, clip->right, clip->bottom, pixel});
    for (int32_t i = 0; i <= count; ++i) call("pt", {points[2 * i], points[2 * i + 1]});
  }
};
std::vector<uint32_t> g_vehicle_addr;  // each mate's vehicle (the exe's +0x4b8 of its line)
struct DrawBrainHooks : ai::BrainHooks {
  zgame::Handle vehicle_handle(void* v) override {
    const int i = mate_index(static_cast<Mech*>(v));
    return i >= 0 ? g_vehicle_addr[static_cast<size_t>(i)] : 0;
  }
};
DrawBrainHooks g_draw_hooks;
float peekf(uint32_t a) {
  const uint32_t b = call("peek32", {a});
  float f;
  std::memcpy(&f, &b, 4);
  return f;
}
void wire_draws() {
  DrawCalls& d = draw_calls();
  d = DrawCalls{};
  d.entity_position = [](zgame::Handle e, float out[3]) {
    const uint32_t node = call("peek32", {e + 0x14});
    const uint32_t data = call("peek32", {node + 0x38});
    if (call("peek8", {data}) & 0x20) call("0x004ed280", {node, 1});
    for (uint32_t i = 0; i < 3; ++i) out[i] = peekf(data + 0x84 + 4 * i);
  };
  d.entity_size = [](zgame::Handle e) { return peekf(e + 0x68); };
  d.project = [](const float in_[3], float out[3]) -> uint32_t {
    // The cursor's place may be the emulator's (an entity's, the net's): read it there.
    float in[3];
    if (in_ == g_ctl->place || in_ == g_k_place || reinterpret_cast<uintptr_t>(in_) > 0xffffffffu) {
      std::memcpy(in, in_, 12);
    } else {
      for (uint32_t i = 0; i < 3; ++i) in[i] = peekf(fake(in_) + 4 * i);
    }
    const uint32_t v = call("0x0053cf10", {zgame::bits(in[0]), zgame::bits(in[1]), zgame::bits(in[2])});
    static const float kDepth[4] = {0.5f, 0.0f, 1.5f, 1.0f};
    out[0] = static_cast<float>(v & 0x3ff), out[1] = static_cast<float>((v >> 10) & 0x3ff), out[2] = kDepth[(v >> 20) & 3];
    return (v >> 24) & 0xff;
  };
  d.map_point = [](zgame::Handle map, const float in[3], int32_t out[2]) {
    const uint32_t v = call("0x004586e0", {map, zgame::bits(in[0]), zgame::bits(in[1]), zgame::bits(in[2])});
    out[0] = static_cast<int32_t>(v & 0xfff), out[1] = static_cast<int32_t>((v >> 12) & 0xfff);
  };
  d.screen_rect = [](ztwod::Rect* r) {
    const uint32_t a = call("0x0040d7b0", {});
    r->left = static_cast<int32_t>(call("peek32", {a})), r->top = static_cast<int32_t>(call("peek32", {a + 4}));
    r->right = static_cast<int32_t>(call("peek32", {a + 8})), r->bottom = static_cast<int32_t>(call("peek32", {a + 12}));
  };
  d.node_marked = [](uint32_t node) { return call("0x0040ee50", {node}) != 0; };
  d.view_prepare = [] { call("0x004b51f0", {}); };
  ai::set_brain_hooks(&g_draw_hooks);
}

void wire_frame() {
  FrameCalls& f = frame_calls();
  f = FrameCalls{};
  f.view_hud = [](Mech&) -> zgame::Handle { return g_viewed ? call("0x00430aa0", {}) : 0; };
  f.hud_map_up = [](zgame::Handle h) { return (call("peek8", {h + 0x70}) & 0xff) == 1; };
  f.map = []() -> zgame::Handle { return call("0x00491c80", {}); };
  f.map_size = [](zgame::Handle m, int32_t* w, int32_t* h) {
    const uint32_t r = call("peek32", {m + 0xd8});
    *w = static_cast<int32_t>(call("peek32", {r + 8}));
    *h = static_cast<int32_t>(call("peek32", {r + 0xc}));
  };
  f.map_mouse = [](zgame::Handle, int32_t* x, int32_t* y) {
    const uint32_t v = call("0x00444670", {});
    *x = static_cast<int32_t>(v & 0xfff), *y = static_cast<int32_t>((v >> 12) & 0xfff);
  };
  f.map_pick = [](float x, float y, float place[3], uint32_t* node) -> bool {
    const uint32_t v = call("0x004587b0", {zgame::bits(x), zgame::bits(y)});
    if (!(v & 1)) return false;
    place[0] = static_cast<float>(v >> 8), place[1] = 2.0f, place[2] = -static_cast<float>(v >> 8);
    *node = call("peek32", {0x2c3f0000u + ((v >> 1) & 0x7f) * 4});  // the stub's node table
    return true;
  };
  f.reticle_pick = [](zgame::Handle) -> zgame::Handle { return call("0x00444330", {0}); };
  f.record_node = [](zgame::Handle r) -> uint32_t { return call("peek32", {r + 0x24}); };
  f.reticle_place = [](zgame::Handle h, float place[3]) {
    for (uint32_t i = 0; i < 3; ++i) {
      const uint32_t b = call("peek32", {h + 0x144c + 0x8fc + 4 * i});
      std::memcpy(&place[i], &b, 4);
    }
  };
  f.trigger = []() -> uint32_t { return call("0x0051d300", {}); };
  f.contact_entity = [](void*) -> zgame::Handle { return call("0x004e6fe0", {}); };
  f.entity_node = [](zgame::Handle e) -> uint32_t { return call("peek32", {e + 4}); };
  f.entity_place = [](zgame::Handle e) -> const float* { return unfake(call("0x005a85e0", {e})); };
  f.entity_name = [](zgame::Handle e) -> const char* { return text_at(call("name", {e})); };
  f.pick_target = [](Mech&, zgame::Handle e) { call("0x004a7b40", {e}); };
  f.marker_move = [](zgame::Handle, const float* p) { call("marker.vt+0x18", {place_arg(p)}); };
  f.net_entity = [](uint32_t net) -> zgame::Handle {
    const uint32_t holder = call("peek32", {net + 0x18});
    return holder ? call("holder.vt+0x2c", {holder}) : 0;
  };
  f.net_place = [](uint32_t net) -> const float* { return unfake(call("net.vt+0x18", {net})); };
  PanelCalls& pc = panel_calls();
  pc.node_entity = [](uint32_t node) -> zgame::Handle { return call("0x0040e920", {node}); };
  pc.marker_on = [](zgame::Handle mk, bool on) {
    call("0x004eb170", {call("peek32", {mk + 4}), on ? 1 : 0});
    g_marker_active = on;
  };
  PocketCalls& k = pocket_calls();
  k.net_element = []() -> uint32_t { return g_net ? call("0x0058b060", {}) : 0; };
  k.view_pocket = [](bool on) { call("0x004b5290", {on ? 1 : 0}); };
  k.set_camera_mode = [](int32_t m) { call("0x0040ae70", {m}); };
}

void dump_frame(PocketPanel& p, PocketControls& c, Dump& d) {
  d.o << "frame ";
  d.kv("x148", c.x148), d.kv("x14c", c.x14c), d.kv("x154", c.x154), d.kv("x164", c.x164);
  d.kv("x168", place_arg(c.cursor_pos));
  for (float v : c.place) d.f("pl", v);
  d.kv("x184", c.x184), d.kv("x1b8", c.x1b8), d.kv("net", c.net_element), d.kv("x1c0", c.x1c0), d.kv("x1c1", c.x1c1);
  d.kv("done", c.wrapped), d.kv("aim_o", c.aim.owner), d.kv("aim_e", c.aim.entity);
  d.kv("cursor", pocket_globals().cursor), d.kv("x50", pocket_globals().x50), d.kv("x60", pocket_globals().x60);
  d.kv("tgt", p.target_entity), d.kv("mk_on", g_marker_active);
  d.r("clip", pocket_clip());
  d.o << '\n';
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
  FrameHost host;
  g_frame_test = false;
  g_marker_active = false;
  frame_calls() = FrameCalls{};
  ztwod::set_host(&host);
  reset();
  g_mates.clear();
  g_mate_names.clear();
  g_vehicle_addr.clear();
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
  g_k_place = cursor_place;
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
      uint32_t h, act, xbc, allies, veh = 0;
      std::string name;
      ls >> std::hex >> h >> act >> xbc >> allies >> name >> veh;
      g_vehicle_addr.push_back(veh);
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
    } else if (op == 'W') {  // the frame's world: viewed, the target contact's owner and entity (0: none)
      uint32_t viewed, owner, entity, saved_camera, active, net;
      ls >> std::hex >> viewed >> owner >> entity >> saved_camera >> active >> net;
      g_net = net != 0;
      g_frame_test = true;
      g_ctl = &controls;
      g_viewed = viewed != 0;
      g_marker_active = active != 0;
      controls.saved_camera = static_cast<int32_t>(saved_camera);
      wire_frame();
      wire_draws();
      g_target = ai::Contact{};
      g_target.owner = owner, g_target.entity = entity;
      player.target = owner || entity ? &g_target : nullptr;
    } else if (op == 'T') {
      uint32_t owner, entity;
      ls >> std::hex >> owner >> entity;
      g_target = ai::Contact{};
      g_target.owner = owner, g_target.entity = entity;
      player.target = owner || entity ? &g_target : nullptr;
    } else if (op == 'Y') {
      ls >> controls.x184;
    } else if (op == 'Q') {  // the projection's fy (bits)
      uint32_t b;
      ls >> std::hex >> b;
      std::memcpy(&zmth::projection().fy, &b, 4);
    } else if (op == 'D') {  // the HUD's overlay pass (0x0046a410), the mode up
      uint32_t dt;
      ls >> std::hex >> dt;
      controls.panel = panel.get();
      pocket_globals().active = &controls;
      pocket_draw(zgame::from_bits(dt));
      pocket_globals().active = nullptr;
    } else if (op == 'F') {  // a frame: dt, then the mouse's 15 dwords
      uint32_t dt;
      ztwod::MouseState ms{};
      uint32_t* w = reinterpret_cast<uint32_t*>(&ms);
      ls >> std::hex >> dt;
      for (int i = 0; i < 15; ++i) ls >> w[i];
      g_mouse.push_back(ms);
      controls.panel = panel.get();
      pocket_frame(controls, zgame::from_bits(dt));
      ++g_frame;
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
  if (g_frame_test) dump_frame(*panel, controls, d);
  controls.panel = nullptr;
  player.target = nullptr;
  frame_calls() = FrameCalls{};
  draw_calls() = DrawCalls{};
  pocket_calls() = PocketCalls{};
  ai::set_brain_hooks(nullptr);
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

// The mission map's pick 0x004587b0 (MissionMap::pick) for test_map_pick.py:
// a map with `mpp` and `offset`; the ground (0x004edab0) answers `ground_ret`
// and, when 0, `count` hits, hit[0] at `height` on `node`; `world` 0 is no
// world ([0x00608c84] 0). Out: the answer, the place (as it starts: `place`
// in), the node, the ground's x, y, z.
namespace {
struct MapPickGround {
  int32_t ret = 0, count = 0;
  float height = 0;
  uint32_t node = 0;
  float asked[3] = {};
  int32_t calls = 0;
} g_mp;
}  // namespace
extern "C" int probe_map_pick(float mpp, const float offset[3], float px, float py, int world, int ground_ret, int count,
                              float height, uint32_t node, float place[3], uint32_t* out_node, float asked[3]) {
  mech3::MissionMap m(nullptr);
  m.mpp = mpp;
  for (int i = 0; i < 3; ++i) m.offset[i] = offset[i];
  g_mp = MapPickGround{};
  g_mp.ret = ground_ret, g_mp.count = count, g_mp.height = height, g_mp.node = node;
  auto ground = [](float x, float y, float z, zcls::GroundHits* h) -> int32_t {
    g_mp.asked[0] = x, g_mp.asked[1] = y, g_mp.asked[2] = z, ++g_mp.calls;
    if (g_mp.ret == 0) {
      h->count = static_cast<uint8_t>(g_mp.count);
      h->hit[0].height = g_mp.height;
      h->hit[0].node = reinterpret_cast<zcls::Node*>(static_cast<uintptr_t>(g_mp.node));
    }
    return g_mp.ret;
  };
  zcls::GroundHit hit{};
  const bool r = m.pick(px, py, place, &hit, world ? +ground : nullptr);
  *out_node = r ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hit.node)) : 0;
  for (int i = 0; i < 3; ++i) asked[i] = g_mp.asked[i];
  return r ? 1 + 2 * g_mp.calls : 2 * g_mp.calls;
}
