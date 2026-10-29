#include "mech3/mech_waynet.h"

#include <cstdio>

#include "crt/msvc_math.h"
#include "crt/msvc_string.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zmodel/gmod_screen.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zutil/zfix.h"
#include "gamez/znetwork/znet_object.h"
#include "mech3/mech_fieldbase.h"
#include "mech3/zai_path.h"
#include "mech3/zainet_node.h"
#include "mech3/zainetedit.h"

namespace mech3 {

int32_t display_w();  // options.h (0x0040d760), not includable here
void render_point_to_canvas(float* x, float* y);  // options.h (FIX (B), resolution.md step 4)
int32_t display_h();

namespace {
WaynetHooks g_default;
WaynetHooks* g_hooks = &g_default;
McbWaynet* g_mcb = nullptr;   // [0x0060b544]
Handle g_marker = nullptr;    // [0x00608ba4]
int32_t g_count = 0;          // [0x00608ba0] the waynets alive
const float kZero[3] = {0, 0, 0};  // 0x0063d870

Handle g_icons[2] = {};       // [0x00608ba8] nav, [0x00608bac] MFB
Handle g_arrows[8] = {};      // [0x00608bb0..]: nav left, right, up, down; then the MFB's
constexpr float kReach2 = 625.0f;  // 0x005c3824: 25 m

// Mech3.exe 1.2 @ 0x00479dd0: the arrow `edge` (0 left, 1 right, 2 up, 3
// down) of the nav or MFB set at (x, y), held on the screen along the edge.
void arrow_at(int32_t mfb, int32_t edge, int32_t x, int32_t y) {
  WaynetHooks& h = *g_hooks;
  h.arrow_image(g_arrows[edge + mfb * 4]);
  int32_t w = 0, ht = 0;
  h.arrow_size(&w, &ht);
  if (edge == 0 || edge == 1) {
    if (h.screen_h() - ht < y) y = h.screen_h() - ht;
    else if (y < 0) y = 0;
  } else {
    if (h.screen_w() - w < x) x = h.screen_w() - w;
    else if (x < 0) x = 0;
  }
  h.arrow_place(x, y);
  h.arrow_show();
}
}  // namespace

void set_waynet_hooks(WaynetHooks* h) { g_hooks = h ? h : &g_default; }
WaynetHooks& waynet_hooks() { return *g_hooks; }
Handle& waynet_marker() { return g_marker; }
int32_t& waynet_icons_count() { return g_count; }

int32_t WaynetHooks::alloc_id() { return znet::alloc_id(znet::kKeptIds, znet::kKeptIds, znet::kKeptIds + 100); }
int32_t WaynetHooks::bases_alive() { return field_bases().alive(); }
bool WaynetHooks::dock_idle() { return field_bases().idle_on; }
void WaynetHooks::dock_message(Handle mech) { field_bases().message(mech); }
void WaynetHooks::dock_abort() { field_bases().abort_front(); }
bool WaynetHooks::dock_set_requester(Handle mech) { return field_bases().set_requester(mech); }
void WaynetHooks::say(int32_t line) { field_base_hooks().say(field_bases().sounds[line]); }

namespace {
ztwod::Image& arrow() {  // 0x00608ae0, 0x0059b970(0) at start-up
  static ztwod::Image a(0);
  return a;
}
}  // namespace

bool waynet_arrow_place(int32_t* x, int32_t* y) {
  *x = arrow().get_x(), *y = arrow().get_y();
  return !(arrow().flags & ztwod::kHidden);
}

void WaynetHooks::image_path(const char* path) { ztwod::host().image_path(path); }
Handle WaynetHooks::icon_make(const char* name) { return new ztwod::Image(name, 0, 0, 1); }
void WaynetHooks::icon_free(Handle icon) { delete static_cast<ztwod::Image*>(icon); }
Handle WaynetHooks::image_get(const char* name) { return ztwod::host().image_get(name); }
void WaynetHooks::image_release(Handle image) { ztwod::host().image_release(static_cast<zimg::Texture*>(image)); }
void WaynetHooks::report_leak(int32_t count) {
  char msg[160];
  std::snprintf(msg, sizeof msg, "memory leak of %d waynet%s detected: should be unloaded at end of mission", count,
                count == 1 ? "" : "s");  // 0x005e59ac
  zerr::report(0x200, "C:\\zipper\\Mech3\\mech_waynet.cpp", 0x59, msg);
}
void WaynetHooks::arrow_hide() { arrow().Object2D::hide(); }
void WaynetHooks::arrow_image(Handle image) { arrow().set_image(static_cast<zimg::Texture*>(image)); }
void WaynetHooks::arrow_size(int32_t* w, int32_t* h) {
  const zimg::Texture* t = arrow().image;
  *w = t ? t->width : 0, *h = t ? t->height : 0;
}
void WaynetHooks::arrow_place(int32_t x, int32_t y) { arrow().set_pos(x, y); }
void WaynetHooks::arrow_show() { arrow().Object2D::show_for(-1.0f); }
bool WaynetHooks::arrow_hidden() { return (arrow().flags & ztwod::kHidden) != 0; }
void WaynetHooks::arrow_draw() { arrow().draw(); }
int32_t WaynetHooks::screen_w() { return display_w(); }  // 0x0040d760
int32_t WaynetHooks::screen_h() { return display_h(); }  // 0x0040d770
uint32_t WaynetHooks::project(const float in[3], float out[3]) {  // 0x0053cf10
  zmth::Vec3 o;
  const uint32_t code = zmod::project_point(reinterpret_cast<const zmth::Vec3*>(in), &o);
  out[0] = o.x, out[1] = o.y, out[2] = o.z;
  return code;
}
Waynet* WaynetHooks::build_waynet(int32_t id, const char* name) {
  ai::CZAiNetEdit edit(id, name, 1);  // 0x004bcba0, then vt+0x3c on what it built
  return dynamic_cast<Waynet*>(edit.net);
}

// Mech3.exe 1.2 @ 0x0047a320. A dock point is shown and has no nav line.
WaynetPoint::WaynetPoint(const char* n, const char* l, const float p[3], const int32_t* links, int32_t count,
                         uint8_t m, uint8_t f, uint8_t d, uint8_t e, uint8_t hid)
    : mfb(m), xd(d), xe(e), xf(f), hidden(hid) {
  pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
  if (n) name = n, has_name = true;
  if (l) label = l, has_label = true;
  int32_t last = count - 1;
  while (last >= 0 && links[last] == -1) --last;  // trailing -1s
  if (last != -1 && last + 1 > 0) first_link = links[0];
  if (!mfb) {
    if (has_label) {
      char text[0x40];
      std::snprintf(text, sizeof text, "nav%s", label.c_str());  // 0x005e5adc
      sound = g_hooks->sound_find(text);
    }
  } else {
    xe = 0, mfb = 1, xd = 1, xf = 0;
  }
}

// Mech3.exe 1.2 @ 0x0047a530
bool WaynetPoint::set_position(const float* p) {
  pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
  return true;
}

// Mech3.exe 1.2 @ 0x0047a500
void WaynetPoint::set_reached(uint8_t on) {
  if (!reached && sound) g_hooks->sound_play(sound);
  reached = on;
}

// Mech3.exe 1.2 @ 0x00479640 (over 0x004bfe40: the default behaviour first).
Waynet::Waynet(int32_t id, const char* name) : ai::CompiledNet(id, 1), label(name ? name : "") {
  elems.push_back(ai::default_behaviour());
  x24 = static_cast<uint8_t>(id);
}

// Mech3.exe 1.2 @ 0x004796f0 (then the compiled net's 0x004bff20 frees the points).
Waynet::~Waynet() { waynet_icons_unref(); }

// Mech3.exe 1.2 @ 0x00479a50 (after the net base's 0x004d5350, a no-op on
// these elements).
void Waynet::begin() {
  if (g_marker) return;
  g_marker = g_hooks->node_named("waypoint");  // 0x005e0590
  if (!g_marker) return;
  if (g_hooks->node_parents(g_marker) == 0) g_hooks->world_add(g_marker);
  select(0);
}

// Mech3.exe 1.2 @ 0x00479aa0
const float* Waynet::position_of(int32_t i) const {
  znet::Element* e = elem(i);
  if (!e) return kZero;
  ai::Behaviour* b = static_cast<ai::AiElem*>(e)->as_behaviour();
  return b ? b->position() : nullptr;  // vt+0x18
}

// Mech3.exe 1.2 @ 0x00479ac0: the marker shown on the selected point.
void Waynet::place_marker() {
  if (!g_marker) return;
  const float* p = position_of(current);
  if (!p) return;
  g_hooks->node_set_active(g_marker, 1);
  g_hooks->node_set_translation(g_marker, p[0], p[1], p[2]);
}

// Mech3.exe 1.2 @ 0x00479b10 / 0x00479b30
void Waynet::next() {
  current = static_cast<int32_t>((static_cast<uint32_t>(current) + 1u) % static_cast<uint32_t>(count()));
  place_marker();
}
void Waynet::prev() {
  current = (current == 0 ? count() : current) - 1;
  place_marker();
}

// Mech3.exe 1.2 @ 0x00479b50: the nearest point with a place (else the
// selected one) selected.
void Waynet::nearest(const float* pos) {
  znet::Element* e = as_point(ai::nearest_step(*this, pos));  // 0x004d51d0, then vt+0x78
  if (!e) e = elem(current);
  if (!e) return;
  select_point(e);
}

// Mech3.exe 1.2 @ 0x00479b90
bool Waynet::select(int32_t i) {
  if (i >= 0 && i < count()) {
    current = i;
    place_marker();
    return true;
  }
  if (g_marker) g_hooks->node_set_active(g_marker, 0);
  return false;
}

// Mech3.exe 1.2 @ 0x00479be0
WaynetPoint* Waynet::find(const char* l) const {
  for (int32_t i = 0; i < count(); ++i) {
    auto* p = dynamic_cast<WaynetPoint*>(elem(i));  // vt+0x24: only the points have a label
    const char* s = p ? p->label_or_null() : nullptr;
    if (s && crt::msvc_stricmp(s, l) == 0) return p;
  }
  return nullptr;
}

// Mech3.exe 1.2 @ 0x0047a1e0
bool Waynet::remove_last(znet::Element* e) {
  if (elems.empty() || elems.back() != e) return false;
  elems.pop_back();
  return true;
}

// Mech3.exe 1.2 @ 0x0047a270
void Waynet::note(znet::Element* p, bool add) {
  if (!p) return;
  watched.remove(p);
  if (add) watched.push_back(p);
}

// Mech3.exe 1.2 @ 0x00479c50. FIX (bug), behind `bugs`: the exe's loop
// compares the point before with the selected one, so once past the selected
// point it skips the rest of the list (their nav lines unsaid until they are
// selected); here only the selected point is left to the end. And a selected
// element with no place (the default behaviour, a null step) is read through
// null in the exe; here it is passed over.
uint8_t Waynet::frame(Handle vehicle) {
  WaynetHooks& h = *g_hooks;
  znet::Element* sel = elem(current);  // 0x0058b060
  const bool fix = zfix::options().bugs;
  znet::Element* prev = nullptr;
  for (znet::Element* e : watched) {
    if (fix ? e == sel : prev == sel) continue;
    prev = e;
    const float* p = static_cast<ai::AiElem*>(e)->as_behaviour()->position();  // vt+0x18
    const float* v = h.entity_position(vehicle);                              // 0x005a85e0
    const float d[3] = {v[0] - p[0], v[1] - p[1], v[2] - p[2]};
    if ((d[0] * d[0] + d[1] * d[1]) + d[2] * d[2] < kReach2) as_point(e)->set_reached(1);  // 0x0047a500
  }
  WaynetPoint* sp = as_point(sel);
  const float* p = sel ? static_cast<ai::AiElem*>(sel)->as_behaviour()->position() : nullptr;
  if (!sp || !p) return 0;
  const float* v = h.entity_position(vehicle);
  const float d[3] = {v[0] - p[0], v[1] - p[1], v[2] - p[2]};
  if ((d[0] * d[0] + d[1] * d[1]) + d[2] * d[2] < kReach2) {
    sp->set_reached(1);
    if (sp->xe) next();  // autonext, vt+0x60
  }
  return sp->reached;
}

// Mech3.exe 1.2 @ 0x00479ef0. FIX (bug): a selected element with no place
// is read through null in the exe (0x00479f34); here no arrow.
void Waynet::hud_arrow(bool active) {
  WaynetHooks& h = *g_hooks;
  h.arrow_hide();
  if (!active) return;
  znet::Element* e = elem(current);
  if (!e) return;
  WaynetPoint* pt = as_point(e);
  const int32_t mfb = pt && pt->mfb ? 1 : 0;  // +0x0c
  const float* p = static_cast<ai::AiElem*>(e)->as_behaviour()->position();  // vt+0x18
  if (!p) return;
  const float at[3] = {p[0], p[1], p[2]};
  float s[3];
  const uint32_t off = h.project(at, s);  // 0x0053cf10
  render_point_to_canvas(&s[0], &s[1]);  // FIX (B): onto the canvas (resolution.md step 4); none before
  if (off & 1) return arrow_at(mfb, 0, 0, crt::msvc_ftol(s[1]));
  if (off & 2) {
    int32_t w = 0, ht = 0;
    h.arrow_size(&w, &ht);  // the arrow's image before this one's
    return arrow_at(mfb, 1, h.screen_w() - w, crt::msvc_ftol(s[1]));
  }
  if (off & 4) return arrow_at(mfb, 2, crt::msvc_ftol(s[0]), 0);
  if (off & 8) {
    int32_t w = 0, ht = 0;
    h.arrow_size(&w, &ht);
    return arrow_at(mfb, 3, crt::msvc_ftol(s[0]), h.screen_h() - ht);
  }
}

// Mech3.exe 1.2 @ 0x0047a5c0 / 0x0047a620: the picked point's count goes up
// (down) when it is left and when it comes round.
void McbWaynet::next() {
  if (picked && picked == elem(current)) ++passed;
  Waynet::next();
  if (picked && picked == elem(current)) ++passed;
}
void McbWaynet::prev() {
  if (picked && picked == elem(current)) --passed;
  Waynet::prev();
  if (picked && picked == elem(current)) --passed;
}

// Mech3.exe 1.2 @ 0x0047a680
void McbWaynet::nearest(const float* pos) {
  Waynet::nearest(pos);
  picked = elem(current);
  passed = 0;
}

// Mech3.exe 1.2 @ 0x0047a6b0: the mission's waynet back, the marker hidden,
// the command reset.
void McbWaynet::clear() {
  WaynetHooks& h = *g_hooks;
  if (this == h.mission_waynet()) {
    h.set_mission_waynet(nav);
  } else if (nav) {
    auto* n = static_cast<Waynet*>(nav);
    if (nav_point) n->select_point(nav_point);
    if (chosen) n->remove_last(chosen);
  }
  select(-1);
  picked = nullptr;
  passed = 0;
  step = 0;
  nav = nullptr;
  nav_point = nullptr;
  chosen = nullptr;
}

// Mech3.exe 1.2 @ 0x0047a7a0, the MFB command (MCB_COMMAND, 0x0047a910) while
// a field base is up. First: with the dock idle, the mcbwaynet the mission's
// (its own kept), the point nearest the mech picked, "wait for position";
// else "occupied". Second: the request; the chosen point onto the mission's
// own waynet when there is one, which is the mission's again. Third: the
// front state's abort.
void McbWaynet::command(Handle mech) {
  WaynetHooks& h = *g_hooks;
  if (h.bases_alive() < 1) return;
  if (step == 0) {
    if (!h.dock_idle()) {
      h.say(3);  // +0x98 mcb_occupied
      return;
    }
    step = 1;
    const float* p = h.entity_position(mech);
    nav = h.mission_waynet();
    nav_point = nav ? static_cast<Waynet*>(nav)->elem(static_cast<Waynet*>(nav)->current) : nullptr;  // 0x0058b060
    chosen = nullptr;
    h.set_mission_waynet(this);  // 0x00491d00
    auto* w = static_cast<Waynet*>(h.mission_waynet());
    znet::Element* e = w ? as_point(ai::nearest_step(*w, p)) : nullptr;  // 0x004d51d0, then vt+0x78
    if (!e) e = elem(current);
    if (e) {
      select_point(e);
      picked = e;
      passed = 0;
    }
    h.say(4);  // +0x9c mcb_pos_wait
    return;
  }
  if (step == 1) {
    step = 2;
    h.dock_message(mech);  // 0x0045a500
    if (this != h.mission_waynet()) return;
    // The selected element as it is held (unchecked: the pick left it in
    // range), vt+0x60 then vt+0x78.
    znet::Element* e = current >= 0 && current < count() ? elems[static_cast<size_t>(current)] : nullptr;
    chosen = e ? as_point(static_cast<ai::AiElem*>(e)->as_behaviour()) : nullptr;
    if (nav) {
      auto* n = static_cast<Waynet*>(nav);
      n->add(chosen);
      n->select_point(chosen);
    }
    h.set_mission_waynet(nav);
    return;
  }
  if (step == 2) h.dock_abort();  // 0x0045a540
}

WaynetPoint* as_point(znet::Element* e) { return dynamic_cast<WaynetPoint*>(e); }

McbWaynet* mcb_waynet() { return g_mcb; }

// Mech3.exe 1.2 @ 0x004d5460
McbWaynet* mcb_waynet_make() {
  if (g_mcb) return g_mcb;
  g_mcb = new McbWaynet(g_hooks->alloc_id(), "mcbwaynet");  // 0x0047a550
  g_hooks->on_new(0x48, g_mcb);
  waynet_icons_ref();
  return g_mcb;
}

// Mech3.exe 1.2 @ 0x004d54f0
void mcb_waynet_free() {
  delete g_mcb;  // vt+0x00(1)
  g_mcb = nullptr;
}

// Mech3.exe 1.2 @ 0x0047a730
bool mfb_set_requester(Handle mech) {
  WaynetHooks& h = *g_hooks;
  if (h.bases_alive() <= 0) return false;
  const Handle v = mech ? h.entity_vehicle(mech) : nullptr;  // vt+0x50
  return v && h.dock_set_requester(v);
}

// Mech3.exe 1.2 @ 0x004798f0: the first waynet makes the icons.
void waynet_icons_ref() {
  if (g_count++ != 0) return;
  WaynetHooks& h = *g_hooks;
  h.image_path("..\\data\\common\\images\\hud");  // 0x005e5ac0
  g_icons[0] = h.icon_make("hud_waypoint_icon");     // 0x005e5aac
  g_icons[1] = h.icon_make("hud_mfbwaypoint_icon");  // 0x005e5a94
  static const char* const kArrows[8] = {"hud_navarrow_left", "hud_navarrow_right", "hud_navarrow_up",
                                         "hud_navarrow_down", "hud_mfbarrow_left",  "hud_mfbarrow_right",
                                         "hud_mfbarrow_up",   "hud_mfbarrow_down"};  // 0x005e5a80..0x005e59fc
  for (int i = 0; i < 8; ++i) g_arrows[i] = h.image_get(kArrows[i]);
}
// Mech3.exe 1.2 @ 0x00479a30: the last frees them.
void waynet_icons_unref() {
  if (--g_count != 0) return;
  waynet_icons_free();
}

// Mech3.exe 1.2 @ 0x004797c0
Waynet* nav_waynet_make(const char* name) {
  WaynetHooks& h = *g_hooks;
  Waynet* w = h.build_waynet(h.alloc_id(), name);
  if (!w) return nullptr;
  waynet_icons_ref();  // 0x004798f0
  w->place_marker();  // 0x00479ac0
  return w;
}

// Mech3.exe 1.2 @ 0x00479850
void waynet_icons_free() {
  WaynetHooks& h = *g_hooks;
  if (g_count != 0) {
    h.report_leak(g_count);
    g_count = 0;
  }
  g_marker = nullptr;
  for (Handle& i : g_icons) {
    if (i) h.icon_free(i);  // vt+0x00(1)
    i = nullptr;
  }
  // FIX (bug), behind `bugs`: the exe's loop releases two images, then steps
  // four slots on ([0x00608bb0] / [0x00608bb4], then [0x00608bc0] / [0x00608bc4]): the up and down arrows'
  // images are never released (zImage keeps them, a reference more each
  // mission). Here all eight.
  const bool all = zfix::options().bugs;
  for (int i = 0; i < 8; ++i) {
    if (!all && (i & 2)) continue;
    if (g_arrows[i]) h.image_release(g_arrows[i]);  // 0x00517b10
    g_arrows[i] = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x00479ed0
void waynet_arrow_draw() {
  if (!g_hooks->arrow_hidden()) g_hooks->arrow_draw();
}

ztwod::Image* waynet_icon(bool mfb) { return static_cast<ztwod::Image*>(g_icons[mfb ? 1 : 0]); }

// The net compile's 0x004bde20: a dock point from an arrangenodes node.
void waynet_point_from_node(const ai::NetNodeRec& r, const float pos[3]) {
  if (!r.arrange_set) return;
  McbWaynet* w = mcb_waynet_make();
  if (!w || w->find(r.auxname)) return;
  auto* p = new WaynetPoint(r.name, r.auxname, pos, nullptr, 0, 1, 0, 1, 0, r.hidden);  // new 0x38
  w->add(p);
}

}  // namespace mech3
