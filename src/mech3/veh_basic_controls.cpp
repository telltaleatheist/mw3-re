#include "mech3/veh_basic_controls.h"

#include <cstring>

#include "gamez/zclass/cls_node.h"
#include "gamez/zreader/zreader.h"
#include "mech3/veh_basic_object.h"

namespace mech3 {

namespace {
Net105Record* g_records = nullptr;  // [0x005f4c4c]
BasicControlsHooks g_default;
BasicControlsHooks* g_hooks = &g_default;
BasicControlsHooks& hk() { return *g_hooks; }
// The exe's fcomp against 0 tested on C3: equal, or unordered (a NaN counts as 0).
bool zero(float f) { return !(f < 0.0f || f > 0.0f); }
}  // namespace

Net105Record*& net105_records() { return g_records; }
BasicControlsHooks& basic_controls_hooks() { return *g_hooks; }
void set_basic_controls_hooks(BasicControlsHooks* h) { g_hooks = h ? h : &g_default; }

void BasicControlsHooks::base_init(BasicControls& c) { c.VehicleControls::init(); }
void BasicControlsHooks::base_setup(BasicControls& c, zrdr::Node* def, uint32_t) { c.VehicleControls::setup(def); }

// Mech3.exe 1.2 @ 0x004022c0: the record whose +0x18 is the number.
Net105Record* BasicControlsHooks::record(int32_t id) {
  for (Net105Record* r = net105_records(); r; r = r->next)
    if (r->id == id) return r;
  return nullptr;
}

float BasicControlsHooks::reader_float(zrdr::Node* n, int32_t i) { return n->list[i].f; }

// Mech3.exe 1.2 @ 0x004ae270
BasicControls::BasicControls(void* vehicle, int32_t net_number) {
  owner = vehicle;
  net = net_number;
  init();
}

// Mech3.exe 1.2 @ 0x004ae3c0: with an owner and its node, the base's init; a net number that names a record
// takes its mode from the record's kind and its node nearest the vehicle, the vehicle's "healthy" node shown
// to the box test (and its flag 8 off), and the ranges from the record (each 0 its default); else stopped (4).
void BasicControls::init() {
  BasicControlsHooks& h = hk();
  auto* v = static_cast<BasicVehicle*>(owner);
  if (!v || !v->node) return;
  h.base_init(*this);
  state = 4;
  if (net > 0 && (record = h.record(net))) state = 2;  // no positive number: +0xa08 as it was
  if (state != 2) return;
  if (record->kind == 0) mode = 0;
  else if (record->kind == 1) mode = 2;
  else if (record->kind == 2) mode = 3;
  const Handle e = h.nearest_element(record, h.owner_position(owner));
  net_node = e ? h.element_node(e) : nullptr;
  if (const Handle healthy = h.node_find(v->node, "healthy")) {
    h.node_set_intersect_bbox(healthy, 1);
    h.node_set_flag8(healthy, 0);
  }
  const auto sq = [](float f, float otherwise) { return zero(f) ? otherwise : f * f; };
  activate_rad = sq(record->activate_rad, 102400.0f);   // [0x005c8c90]
  attack_rad = sq(record->attack_rad, 1500.0f);                   // [0x005c8c98]
  attack_dwell = zero(record->attack_dwell) ? 10.0f : record->attack_dwell;
  pursuit_near = sq(record->pursuit_range[1], 4624.0f);
  pursuit_far = sq(record->pursuit_range[0], 4900.0f);
  not_pursuit_dwell = zero(record->not_pursuit_dwell) ? 3.0f : record->not_pursuit_dwell;
  return_range = sq(record->return_range, 62500.0f);
  xa18 = 0;
  v->xe0 = v;
}

// Mech3.exe 1.2 @ 0x004ae6b0: the net's number kept, the base's setup; with a record, each of its ranges left 0
// taken from the definition ("activation", "pursuit_range"'s two, "not_pursuit_dwell", "return_range"), the
// pursuit pair put in order.
void BasicControls::setup_with(zrdr::Node* def, uint32_t arg2, int32_t net_number) {
  BasicControlsHooks& h = hk();
  auto* v = static_cast<BasicVehicle*>(owner);
  if (!v || !v->node) return;
  net = net_number;
  h.base_setup(*this, def, arg2);
  if (state != 2) return;
  zrdr::Node* n = h.reader_find(def, "activation");
  if (zero(record->activate_rad) && n) {
    const float f = h.reader_float(n, 1);
    activate_rad = f * f;
  }
  if ((n = h.reader_find(def, "pursuit_range"))) {
    if (zero(record->pursuit_range[1])) {
      const float f = h.reader_float(n, 1);
      pursuit_near = f * f;
    }
    if (zero(record->pursuit_range[0])) {
      const float f = h.reader_float(n, 2);
      pursuit_far = f * f;
    }
  }
  if (pursuit_far < pursuit_near) std::swap(pursuit_near, pursuit_far);
  if ((n = h.reader_find(def, "not_pursuit_dwell")) && zero(record->not_pursuit_dwell)) not_pursuit_dwell = h.reader_float(n, 1);
  if ((n = h.reader_find(def, "return_range")) && zero(record->return_range)) {
    const float f = h.reader_float(n, 1);
    return_range = f * f;
  }
}

// Mech3.exe 1.2 @ 0x004ae350
int32_t BasicControls::path_goal() { return hk().node_goal(net_node); }

}  // namespace mech3
