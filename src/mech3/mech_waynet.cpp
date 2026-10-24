#include "mech3/mech_waynet.h"

#include <cstdio>

#include "crt/msvc_string.h"
#include "gamez/znetwork/znet_object.h"
#include "mech3/mech_fieldbase.h"
#include "mech3/zai_path.h"
#include "mech3/zainet_node.h"

namespace mech3 {

namespace {
WaynetHooks g_default;
WaynetHooks* g_hooks = &g_default;
McbWaynet* g_mcb = nullptr;   // [0x0060b544]
Handle g_marker = nullptr;    // [0x00608ba4]
int32_t g_icons = 0;          // [0x00608ba0]
const float kZero[3] = {0, 0, 0};  // 0x0063d870

// 0x004798f0: the first waynet makes the icons.
void icons_ref() {
  if (g_icons++ == 0) g_hooks->icons_make();
}
// 0x00479a30 -> 0x00479850: the last frees them and forgets the marker.
void icons_unref() {
  if (--g_icons != 0) return;
  g_marker = nullptr;
  g_hooks->icons_free();
}
}  // namespace

void set_waynet_hooks(WaynetHooks* h) { g_hooks = h ? h : &g_default; }
WaynetHooks& waynet_hooks() { return *g_hooks; }
Handle& waynet_marker() { return g_marker; }
int32_t& waynet_icons_count() { return g_icons; }

int32_t WaynetHooks::alloc_id() { return znet::alloc_id(znet::kKeptIds, znet::kKeptIds, znet::kKeptIds + 100); }
int32_t WaynetHooks::bases_alive() { return field_bases().alive(); }
bool WaynetHooks::dock_idle() { return field_bases().idle_on; }
void WaynetHooks::dock_message(Handle mech) { field_bases().message(mech); }
void WaynetHooks::dock_abort() { field_bases().abort_front(); }
bool WaynetHooks::dock_set_requester(Handle mech) { return field_bases().set_requester(mech); }
void WaynetHooks::say(int32_t line) { field_base_hooks().say(field_bases().sounds[line]); }

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
Waynet::~Waynet() { icons_unref(); }

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

// Mech3.exe 1.2 @ 0x0047a270: `v` out of the list, then at its end if `add`.
void Waynet::note(int32_t v, bool add) {
  if (!v) return;
  x28.remove(v);
  if (add) x28.push_back(v);
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
  icons_ref();
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

// The net compile's 0x004bde20: a dock point from an arrangenodes node.
void waynet_point_from_node(const ai::NetNodeRec& r, const float pos[3]) {
  if (!r.arrange_set) return;
  McbWaynet* w = mcb_waynet_make();
  if (!w || w->find(r.auxname)) return;
  auto* p = new WaynetPoint(r.name, r.auxname, pos, nullptr, 0, 1, 0, 1, 0, r.hidden);  // new 0x38
  w->add(p);
}

}  // namespace mech3
