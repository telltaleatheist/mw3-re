// Probe for test_pm_pocket_frame.py: Pirate's Moon's pocket commander frame (mech3/pocket_panel's pocket_frame under
// zgame's pocket_frame_pm; PM's 0x0046b980 with 0x0046b8d0) on a real, empty panel, its calls out answered from the
// case's words and logged in the order made, then the mode's fields, the cursor and the target line's text.
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "gamez/zgame/zgame_table.h"
#include "mech3/mech.h"
#include "mech3/mech_pocket.h"
#include "mech3/pocket_panel.h"
#include "gamez/ztwod/twod_host.h"
#include "pm/pm_game.h"
#include "probe_app.h"
#include "probe_twod.h"

namespace {
using namespace mech3;

// The case (test_pm_pocket_frame.py's CASE, in order).
enum : int {
  kMapUp, kMap, kMapW, kMapH, kMouseX, kMouseY, kTrigger, kMapHit, kMapNode, kMapPlace,  // +3
  kRecord = kMapPlace + 3, kRecordNode, kReticle,  // +3
  kNodeEntity = kReticle + 3, kMarker, kMarkerNode, kTarget, kTargetEntity, kTargetOwner, kAimEntity, kAimOwner,
  kContactEntity, kNet, kNetElement, kNetPrev, kNetEntity, kNetPlace,  // +3
  kExiting = kNetPlace + 3, kState, kCount, kGroundRet, kGroundHits, kGroundHeight, kNamed, kWords
};
const uint32_t* g_in = nullptr;
std::string g_log;
float f(int i) {
  float v;
  std::memcpy(&v, &g_in[i], 4);
  return v;
}
void log(const char* fmt, ...) {
  char b[256];
  va_list a;
  va_start(a, fmt);
  std::vsnprintf(b, sizeof b, fmt, a);
  va_end(a);
  g_log += b;
  g_log += '\n';
}
uint32_t bits(float v) {
  uint32_t b;
  std::memcpy(&b, &v, 4);
  return b;
}
// An entity's place: its handle, +1, +2 (as the test's 0x005cb260).
float g_places[64][3];
const float* place_of(zgame::Handle e) {
  float* p = g_places[e % 64];
  p[0] = static_cast<float>(e), p[1] = static_cast<float>(e + 1), p[2] = static_cast<float>(e + 2);
  return p;
}
float g_net_place[3];
}  // namespace

extern "C" int probe_pm_pocket_frame(const uint32_t* in, char* out, int size) {
  g_in = in;
  g_log.clear();
  const zgame::Table& was = zgame::game();
  zgame::set_game(pm::table());
  struct Host : probe_twod::RecordingHost {
    const char* message_text(uint32_t) override { return "Unknown"; }  // 0x00596e50(0x89d), as the test's
  } host;
  ztwod::set_host(&host);
  PocketGlobals saved = pocket_globals();
  pocket_globals() = PocketGlobals{};
  pocket_globals().marker = in[kMarker];
  FrameCalls& fc = frame_calls();
  const FrameCalls saved_fc = fc;
  fc = FrameCalls{};
  PanelCalls& pc = panel_calls();
  const PanelCalls saved_pc = pc;
  pc = PanelCalls{};
  PocketCalls& kc = pocket_calls();
  const PocketCalls saved_kc = kc;
  kc = PocketCalls{};
  Mech player;
  ai::Contact target;
  target.owner = in[kTargetOwner], target.entity = in[kTargetEntity];
  player.target = in[kTarget] ? &target : nullptr;
  PocketControls c(nullptr, nullptr, 2, 1);
  c.owner = &player;
  std::unique_ptr<PocketPanel> panel;
  {
    probe_app::Mute quiet;
    panel = std::make_unique<PocketPanel>(&c, 0, 0, 14);
  }
  c.panel = panel.get();
  panel->target.text[0] = 0;  // the line's own start (the test's made-up panel has none): only the frame's text compared
  c.aim.entity = in[kAimEntity], c.aim.owner = in[kAimOwner];
  c.net_element = in[kNetPrev];
  c.x184 = in[kExiting], c.x148 = static_cast<int32_t>(in[kState]), c.x14c = static_cast<int32_t>(in[kCount]);
  c.saved_camera = 3;

  fc.view_hud = [](Mech&) -> zgame::Handle { return 0x4000; };
  fc.hud_map_up = [](zgame::Handle) { return g_in[kMapUp] == 1; };
  fc.map = []() -> zgame::Handle { return g_in[kMap]; };
  fc.map_size = [](zgame::Handle, int32_t* w, int32_t* h) { *w = static_cast<int32_t>(g_in[kMapW]), *h = static_cast<int32_t>(g_in[kMapH]); };
  fc.map_mouse = [](zgame::Handle, int32_t* x, int32_t* y) {
    log("map mouse");
    *x = static_cast<int32_t>(g_in[kMouseX]), *y = static_cast<int32_t>(g_in[kMouseY]);
  };
  fc.map_pick = [](float x, float y, float place[3], uint32_t* node) {
    log("map pick %08x %08x", bits(x), bits(y));
    if (!g_in[kMapHit]) return false;
    for (int i = 0; i < 3; ++i) place[i] = f(kMapPlace + i);
    *node = g_in[kMapNode];
    return true;
  };
  fc.reticle_pick = [](zgame::Handle) -> zgame::Handle {
    log("reticle pick");
    return g_in[kRecord] ? 0x4100 : 0;
  };
  fc.record_node = [](zgame::Handle) -> uint32_t { return g_in[kRecordNode]; };
  fc.reticle_place = [](zgame::Handle, float place[3]) {
    for (int i = 0; i < 3; ++i) place[i] = f(kReticle + i);
  };
  fc.trigger = []() -> uint32_t { return g_in[kTrigger]; };
  fc.contact_entity = [](void*) -> zgame::Handle {
    log("contact entity");
    return g_in[kContactEntity];
  };
  fc.entity_node = [](zgame::Handle e) -> uint32_t { return e == g_in[kMarker] ? g_in[kMarkerNode] : e + 0x10; };
  fc.entity_place = [](zgame::Handle e) -> const float* { return place_of(e); };
  fc.entity_name = [](zgame::Handle e) -> const char* {
    static char b[32];
    if (!(g_in[kNamed] & 1)) return nullptr;
    std::snprintf(b, sizeof b, "E%x", e);
    return b;
  };
  fc.pick_target = [](Mech&, zgame::Handle e) { log("pick target %x", e); };
  fc.marker_move = [](zgame::Handle m, const float* p) {
    if (p) log("marker move %x %08x %08x %08x", m, bits(p[0]), bits(p[1]), bits(p[2]));
    else log("marker move %x none", m);
  };
  fc.net_entity = [](uint32_t) -> zgame::Handle { return g_in[kNetEntity]; };
  fc.net_place = [](uint32_t) -> const float* {
    for (int i = 0; i < 3; ++i) g_net_place[i] = f(kNetPlace + i);
    return g_net_place;
  };
  fc.route_mouse = [](ztwod::MouseState* m) {
    log("route");
    *m = ztwod::MouseState{};
    m->x = m->y = -1000;  // over no line
  };
  fc.lines_picked = [](PocketPanel*, int32_t d, int32_t from) { log("picked %d %d", d, from); };
  fc.ground_snap = [](float p[3]) {
    log("ground %08x %08x %08x", bits(p[0]), bits(p[1] + 10.0f), bits(p[2]));
    if (g_in[kGroundRet] == 0 && g_in[kGroundHits] > 0) p[1] = f(kGroundHeight) + 1.0f;
  };
  pc.node_entity = [](uint32_t node) -> zgame::Handle {
    log("node entity %x", node);
    return g_in[kNodeEntity];
  };
  pc.marker_on = [](zgame::Handle m, bool on) { log("marker on %x %d", m, on); };
  kc.net_element = []() -> uint32_t { return g_in[kNet] ? g_in[kNetElement] : 0; };
  kc.view_pocket = [](bool on) { log("view pocket %d", on); };
  kc.set_camera_mode = [](int32_t m) { log("camera %d", m); };

  pocket_frame(c, 0.0f);

  const int place_id = c.cursor_pos == c.place ? 1 : c.cursor_pos == g_net_place ? 2 : c.cursor_pos ? 3 : 0;
  log("fields node %x place %08x %08x %08x pos %d e0 %d e1 %d net %x map %x state %d count %d x154 %x wrapped %d",
      c.x164, bits(c.place[0]), bits(c.place[1]), bits(c.place[2]), place_id, c.x1c0, c.x1c1, c.net_element, c.x1b8,
      c.x148, c.x14c, c.x154, c.wrapped);
  log("globals cursor %x map %x %x target %x text [%s]", pocket_globals().cursor, pocket_globals().x50,
      pocket_globals().x60, panel->target_entity, panel->target.text);
  log("aim %x %x", c.aim.entity, c.aim.owner);
  c.panel = nullptr;  // the panel is this probe's to free
  player.target = nullptr;
  {
    probe_app::Mute quiet;
    panel.reset();
  }
  fc = saved_fc, pc = saved_pc, kc = saved_kc;
  pocket_globals() = saved;
  ztwod::set_host(nullptr);
  zgame::set_game(was);
  const int n = static_cast<int>(g_log.size());
  if (n + 1 > size) return -1;
  std::memcpy(out, g_log.c_str(), static_cast<size_t>(n) + 1);
  return n;
}
