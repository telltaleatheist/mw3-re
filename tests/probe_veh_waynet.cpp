// Probes for test_veh_waynet.py (docs/vehicles.md, The mcbwaynet): the
// waynets' point 0x0047a320, the selection (0x00479a50..0x0047a250), the
// mcbwaynet's 0x0047a5c0..0x0047a6b0, the MFB command 0x0047a7a0 and
// 0x0047a730 (mech3/mech_waynet). The port's waynets are built as the
// original's images in the test; the calls out go to the shared callback,
// the original's memory through peek.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zgame/unported.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech_waynet.h"
#include "probe_app.h"

namespace {
namespace m3 = mech3;

uint32_t lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
m3::Handle hd(uint32_t v) { return reinterpret_cast<m3::Handle>(static_cast<uintptr_t>(v)); }
uint32_t peek(uint32_t a) { return probe_app::call("peek32", {a}); }
float peekf(uint32_t a) { return zgame::from_bits(peek(a)); }

m3::McbWaynet* g_mcb = nullptr;
m3::Waynet* g_nav = nullptr;
m3::Handle g_mission = nullptr;
std::vector<m3::WaynetPoint*> g_points;  // by id: the mcb's first, the nav's after
int32_t g_nav_first = 0;
m3::WaynetPoint* g_made = nullptr;
uint32_t g_arrow_img = 0;  // the arrow's image (+0x3c, [0x00608b1c]) and hidden flag, as the probe's calls set them
bool g_arrow_hidden = false;

struct Hooks : m3::WaynetHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return probe_app::call(key, args);
  }
  int32_t bases_alive() override { return static_cast<int32_t>(unported("0x0045a000", {0x605ae0})); }
  bool dock_idle() override { return (peek(0x605b98) & 0xff) != 0; }
  void dock_message(m3::Handle m) override { unported("0x0045a500", {0x605ae0, lo(m)}); }
  void dock_abort() override { unported("0x0045a540", {0x605ae0}); }
  bool dock_set_requester(m3::Handle m) override { return (unported("0x0045b740", {0x605ae0, lo(m)}) & 0xff) != 0; }
  void say(int32_t line) override { unported("0x004e6d50", {peek(0x605b6c + 4 * static_cast<uint32_t>(line)), 0x3f800000}); }
  m3::Handle mission_waynet() override { return g_mission; }
  void set_mission_waynet(m3::Handle w) override {
    g_mission = w;
    if (w) static_cast<m3::Waynet*>(w)->reselect();
  }
  const float* entity_position(m3::Handle e) override {
    static float t[3];
    const uint32_t a = unported("0x005a85e0", {lo(e)});
    for (uint32_t i = 0; i < 3; ++i) t[i] = peekf(a + 4 * i);
    return t;
  }
  m3::Handle entity_vehicle(m3::Handle e) override { return hd(unported("mech.vt+0x50", {lo(e)})); }
  int32_t node_parents(m3::Handle node) override { return static_cast<int32_t>(peek(lo(node) + 0x54)); }
  // test_nav_hud.py: the arrow (the zTwoD image at 0x00608ae0, its flags
  // +0x0c and image +0x3c in the original's memory), the screen, the
  // projection (the test's stub writes x = in.x / 2 + 100, y = in.z / 2 + 50),
  // the icons, the builder.
  void arrow_hide() override {
    unported("0x0059ff50", {0x608ae0});
    g_arrow_hidden = true;
  }
  void arrow_image(m3::Handle img) override {
    unported("0x0059bb50", {0x608ae0, lo(img)});
    g_arrow_img = lo(img);
  }
  void arrow_size(int32_t* w, int32_t* h) override {
    const uint32_t wh = g_arrow_img ? peek(g_arrow_img + 4) : 0;  // the image's +4 / +6, set before the run
    *w = static_cast<int16_t>(wh & 0xffff), *h = static_cast<int16_t>(wh >> 16);
  }
  void arrow_place(int32_t x, int32_t y) override { unported("0x0059bab0", {0x608ae0, x, y}); }
  void arrow_show() override {
    unported("0x0059ff10", {0x608ae0, 0xbf800000u});
    g_arrow_hidden = false;
  }
  bool arrow_hidden() override { return g_arrow_hidden; }
  void arrow_draw() override { unported("0x0059bb70", {0x608ae0}); }
  int32_t screen_w() override { return static_cast<int32_t>(unported("0x0040d760", {})); }
  int32_t screen_h() override { return static_cast<int32_t>(unported("0x0040d770", {})); }
  uint32_t project(const float in[3], float out[3]) override {
    const uint32_t code = unported("0x0053cf10", {zgame::bits(in[0]), zgame::bits(in[1]), zgame::bits(in[2])});
    out[0] = in[0] * 0.5f + 100.0f, out[1] = in[2] * 0.5f + 50.0f, out[2] = 0.0f;
    return code;
  }
  void image_path(const char* path) override { unported("0x00516f50", {path}); }
  m3::Handle icon_make(const char* name) override { return hd(unported("0x0059b830", {name})); }
  void icon_free(m3::Handle icon) override { unported("icon.vt+0x0", {lo(icon), 1}); }
  m3::Handle image_get(const char* name) override { return hd(unported("0x005179d0", {name})); }
  void image_release(m3::Handle img) override { unported("0x00517b10", {lo(img)}); }
  void report_leak(int32_t count) override { unported("0x0049bfd0", {0x200, count}); }
  int32_t alloc_id() override { return static_cast<int32_t>(unported("0x0058b1c0", {})); }
  m3::Waynet* build_waynet(int32_t id, const char* name) override {
    return unported("0x004bcba0", {id, name}) ? g_nav : nullptr;
  }
};
Hooks g_hooks;

uint32_t id_of(const znet::Element* e) {
  if (!e) return 0xffffffff;
  for (size_t i = 0; i < g_points.size(); ++i)
    if (g_points[i] == e) return static_cast<uint32_t>(i);
  return e == mech3::ai::default_behaviour() ? 999u : 998u;
}

void free_all() {
  m3::waynet_icons_count() = 1000;  // the deletes below never free the icons
  // The points are the waynets' (the compiled net frees its steps); a point
  // in both (the chosen one) is taken off the nav first.
  std::vector<znet::Element*> seen;
  for (m3::Waynet* w : {static_cast<m3::Waynet*>(g_mcb), g_nav})
    if (w)
      for (znet::Element*& e : w->elems) {
        bool dup = false;
        for (znet::Element* f : seen) dup |= f == e;
        if (dup) e = mech3::ai::null_step();
        else seen.push_back(e);
      }
  delete g_mcb;
  delete g_nav;
  g_mcb = nullptr, g_nav = nullptr;
  g_points.clear();
}
}  // namespace

extern "C" {
void probe_wn_hooks(int on) { m3::set_waynet_hooks(on ? &g_hooks : nullptr); }

// The waynets: the mcb's points (n, then for each: label (in labels, \0
// separated), x y z), the nav's (nn, the same after), each waynet's
// current; the mcb's command state (passed, picked, chosen, nav_point as
// point ids or -1, nav present, step); the mission's waynet (0 none, 1 the
// mcb, 2 the nav); the marker (the original's node, 0 none); the icons count.
void probe_wn_setup(int32_t n, int32_t nn, const char* labels, const float* pos, int32_t cur, int32_t nav_cur,
                    const int32_t* state, int32_t mission, uint32_t marker, int32_t icons) {
  free_all();
  m3::waynet_icons_count() = icons + 2;  // the two waynets' own counts, never dropped here
  g_mcb = new m3::McbWaynet(10050, "mcbwaynet");
  g_nav = nn >= 0 ? new m3::Waynet(10051, "nav") : nullptr;
  const char* l = labels;
  for (int32_t i = 0; i < n + (nn > 0 ? nn : 0); ++i) {
    auto* p = new m3::WaynetPoint("p", l, pos + 3 * i, nullptr, 0, 1, 0, 1, 0, 0);
    l += std::strlen(l) + 1;
    g_points.push_back(p);
    (i < n ? static_cast<m3::Waynet*>(g_mcb) : g_nav)->add(p);
  }
  g_nav_first = n;
  g_mcb->current = cur;
  if (g_nav) g_nav->current = nav_cur;
  auto pt = [](int32_t id) -> znet::Element* {
    return id >= 0 && id < static_cast<int32_t>(g_points.size()) ? g_points[static_cast<size_t>(id)] : nullptr;
  };
  g_mcb->passed = state[0];
  g_mcb->picked = pt(state[1]);
  g_mcb->chosen = pt(state[2]);
  g_mcb->nav_point = pt(state[3]);
  g_mcb->nav = state[4] && g_nav ? g_nav : nullptr;
  g_mcb->step = state[5];
  g_mission = mission == 1 ? static_cast<m3::Handle>(g_mcb) : mission == 2 ? static_cast<m3::Handle>(g_nav) : nullptr;
  m3::waynet_marker() = hd(marker);
}

// 0 next, 1 prev, 2 nearest(pos), 3 select(i), 4 find(label) -> id, 5
// position_of(i) -> the x bits (or 1 for the zero vector, 0 none), 6 clear,
// 7 command(mech), 8 begin, 9 mfb_set_requester(mech), 10 the nav's
// remove_last(point id), 11 note(point id, add), 12 the nav's next.
uint32_t probe_wn_call(int32_t op, const float* pos, int32_t i, const char* label, uint32_t mech) {
  switch (op) {
    case 0: g_mcb->next(); return 0;
    case 1: g_mcb->prev(); return 0;
    case 2: g_mcb->nearest(pos); return 0;
    case 3: return g_mcb->select(i);
    case 4: return id_of(g_mcb->find(label));
    case 5: {
      const float* p = g_mcb->position_of(i);
      static const float* zero = nullptr;
      if (!zero) zero = g_mcb->position_of(-1);
      return !p ? 0u : p == zero ? 1u : zgame::bits(p[0]);
    }
    case 6: g_mcb->clear(); return 0;
    case 7: g_mcb->command(hd(mech)); return 0;
    case 8: g_mcb->begin(); return 0;
    case 9: return m3::mfb_set_requester(hd(mech));
    case 10: return g_nav ? g_nav->remove_last(i >= 0 && i < static_cast<int32_t>(g_points.size()) ? g_points[static_cast<size_t>(i)] : nullptr) : 0;
    case 11: g_mcb->note(i >= 0 && i < static_cast<int32_t>(g_points.size()) ? g_points[static_cast<size_t>(i)] : nullptr, label != nullptr); return 0;
    default: if (g_nav) g_nav->next(); return 0;
  }
}

// The mcb's current, passed, picked, chosen, nav_point (ids), nav (1 the
// nav), step; the mission's waynet (0/1/2); the marker; the nav's current
// and its elements (count, ids up to 8); the mcb's elements (count); the
// watched list (count, point ids up to 4).
int probe_wn_image(uint32_t* out) {
  int k = 0;
  out[k++] = static_cast<uint32_t>(g_mcb->current), out[k++] = static_cast<uint32_t>(g_mcb->passed);
  out[k++] = id_of(g_mcb->picked), out[k++] = id_of(g_mcb->chosen), out[k++] = id_of(g_mcb->nav_point);
  out[k++] = g_mcb->nav ? 1u : 0u, out[k++] = static_cast<uint32_t>(g_mcb->step);
  out[k++] = g_mission == g_mcb ? 1u : g_mission && g_mission == g_nav ? 2u : g_mission ? 9u : 0u;
  out[k++] = lo(m3::waynet_marker());
  out[k++] = g_nav ? static_cast<uint32_t>(g_nav->current) : 0xffffffff;
  out[k++] = g_nav ? static_cast<uint32_t>(g_nav->count()) : 0xffffffff;
  for (int j = 0; j < 8; ++j)
    out[k++] = g_nav && j < g_nav->count() ? id_of(g_nav->elem(j)) : 0xffffffff;
  out[k++] = static_cast<uint32_t>(g_mcb->count());
  out[k++] = static_cast<uint32_t>(g_mcb->watched.size());
  int j = 0;
  for (znet::Element* v : g_mcb->watched)
    if (j++ < 4) out[k++] = id_of(v);
  for (; j < 4; ++j) out[k++] = 0xffffffff;
  return k;
}

// 0x0047a320 alone: a point from these arguments; its fields.
void probe_wn_point(const char* name, const char* label, const float* pos, const int32_t* links, int32_t n,
                    const uint8_t* b, uint32_t* out) {
  delete g_made;
  g_made = new m3::WaynetPoint(name, label, pos, links, n, b[0], b[1], b[2], b[3], b[4]);
  const m3::WaynetPoint& p = *g_made;
  int k = 0;
  out[k++] = p.mfb, out[k++] = p.xd, out[k++] = p.xe, out[k++] = p.xf, out[k++] = p.hidden;
  out[k++] = lo(p.sound), out[k++] = static_cast<uint32_t>(p.first_link), out[k++] = p.reached;
  for (float f : p.pos) out[k++] = zgame::bits(f);
  out[k++] = p.has_name, out[k++] = p.has_label;
}
// test_nav_hud.py: a point's flags (by id): mfb (+0x0c), autonext (+0x0e),
// reached (+0x34), its nav line (+0x14, the original's handle, 0 none).
void probe_wn_flags(int32_t id, uint8_t mfb, uint8_t xe, uint8_t reached, uint32_t sound) {
  m3::WaynetPoint* p = g_points[static_cast<size_t>(id)];
  p->mfb = mfb, p->xe = xe, p->reached = reached, p->sound = hd(sound);
}
// The icons' and arrows' globals emptied (0x00479850 without its calls).
void probe_wn_reset_icons() {
  probe_app::Mute mute;
  m3::waynet_icons_count() = 0;
  const bool was = zfix::options().bugs;
  zfix::options().bugs = true;  // all eight images forgotten (the original path keeps four)
  m3::waynet_icons_free();
  zfix::options().bugs = was;
}
// FIX (bug) (0x00479c50, 0x00479ef0): with the default behaviour
// selected (no place) the frame answers 0 and draws no arrow, where the
// original reads through null. 1 when so.
int probe_wn_fix_check() {
  probe_app::Mute mute;
  m3::Waynet w(10090, "nav");  // element 0 the default behaviour
  w.current = 0;
  const uint8_t r = w.frame(nullptr);
  w.hud_arrow(true);
  m3::waynet_icons_count() = 1000;  // its delete counts out, not to 0
  return r == 0 ? 1 : 0;
}
// The waynets' count ([0x00608ba0]) and the arrow's image and hidden flag.
void probe_wn_count(int32_t n) { m3::waynet_icons_count() = n; }
void probe_wn_arrow(uint32_t img, int32_t hidden) { g_arrow_img = img, g_arrow_hidden = hidden != 0; }
// 0 the nav's frame(mech) -> its answer, 1 the nav's note(point id, add), 2
// hud_arrow(active) on the mcb (a 0) or the nav (1), 3 the arrow drawn, 4
// the icons in (0x004798f0), 5 out (0x00479a30), 6 freed (0x00479850), 7
// nav_waynet_make(name) -> 1 for the nav, 0 none.
uint32_t probe_wn_nav(int32_t op, int32_t a, int32_t b, uint32_t mech, const char* name) {
  switch (op) {
    case 0: return g_nav->frame(hd(mech));
    case 1: g_nav->note(a >= 0 && a < static_cast<int32_t>(g_points.size()) ? g_points[static_cast<size_t>(a)] : nullptr, b != 0); return 0;
    case 2: (a ? static_cast<m3::Waynet*>(g_nav) : g_mcb)->hud_arrow(b != 0); return 0;
    case 3: m3::waynet_arrow_draw(); return 0;
    case 4: m3::waynet_icons_ref(); return 0;
    case 5: m3::waynet_icons_unref(); return 0;
    case 6: m3::waynet_icons_free(); return 0;
    default: return m3::nav_waynet_make(name) == g_nav ? 1u : 0u;
  }
}
// The nav's elements' reached (up to 8), its current, its watched ids (up to 4).
int probe_wn_nav_image(uint32_t* out) {
  int k = 0;
  for (int j = 0; j < 8; ++j) {
    const m3::WaynetPoint* p = g_nav && j < g_nav->count() ? m3::as_point(g_nav->elem(j)) : nullptr;
    out[k++] = p ? p->reached : 0xffffffffu;
  }
  out[k++] = g_nav ? static_cast<uint32_t>(g_nav->current) : 0xffffffffu;
  int j = 0;
  if (g_nav)
    for (znet::Element* v : g_nav->watched)
      if (j++ < 4) out[k++] = id_of(v);
  for (; j < 4; ++j) out[k++] = 0xffffffffu;
  out[k++] = static_cast<uint32_t>(m3::waynet_icons_count());
  return k;
}

// The made point's reached (0x0047a500).
void probe_wn_reached(uint8_t on) { g_made->set_reached(on); }
uint32_t probe_wn_reached_get() { return g_made->reached; }
}  // extern "C"
