// The repair dock's states and the field bases' drives (docs/vehicles.md,
// The repair dock; mech_fieldbase.h). Same original file as
// mech_fieldbase.cpp (0x004598a0..0x0045b9c0).
#include <algorithm>
#include <cmath>

#include "gamez/zmath/zmth_vector.h"
#include "mech3/mech_fieldbase.h"
#include "mech3/zai_valve.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
FieldBaseHooks& hk() { return field_base_hooks(); }

// The dock point's radius, squared ([0x005e4a18], 12 m), and a base's
// stand-off from it ([0x005e4a10]).
constexpr float kNear2 = 144.0f;
constexpr float kStandOff = 15.4f;

// A drive made for each base and queued at the front of its steps, begun at
// its next frame, when it is active. FIX (bug): the exe follows a missing
// base (a mission with fewer than three); none is made for it.
void queue_drives(FieldBases& r, FieldBaseDrive* (&d)[3], bool open) {
  for (int i = 0; i < 3; ++i) {
    FieldBase* b = r.bases[i];
    d[i] = b ? hk().drive_make(b, open) : nullptr;
  }
  for (FieldBaseDrive* x : d) {
    if (!x || !x->base->active) continue;
    x->base->actions.push_front(x);
    x->base->begin_next = true;
  }
}
// 0x004598c0 and the delete. FIX (bug): a drive still on its base's steps
// (its state ended before the base dropped it) is left to the base, which
// frees it as it drops it: the exe reads the freed drive until then.
void free_drives(FieldBaseDrive* (&d)[3]) {
  for (FieldBaseDrive*& x : d) {
    if (!x) continue;
    std::list<DockStep*>* l = x->base ? &x->base->actions : nullptr;
    if (l && std::find(l->begin(), l->end(), x) != l->end()) x->orphan = true;
    else hk().drive_free(x);
    x = nullptr;
  }
}
bool drives_done(FieldBaseDrive* const (&d)[3]) {
  for (FieldBaseDrive* x : d)
    if (x && !x->done()) return false;
  return true;
}
}  // namespace

float FieldBaseHooks::game_time() { return zgame::from_bits(unported("[0x0060991c]", {})); }
uint32_t FieldBaseHooks::frame() { return unported("[0x0072f338]", {}); }
int32_t FieldBaseHooks::valve_value(ai::Valve* v) { return v->get(); }

// ---------------------------------------------------------------------------
// The base step (0x005c2600)

Handle DockStep::message(Handle m) {
  abort();  // vt+0x14
  return m;
}
void DockStep::abort() {
  hk().marker_clear();
  finished = true;
}

// ---------------------------------------------------------------------------
// The drive (0x005c2540)

// Mech3.exe 1.2 @ 0x004598d0: out, the travel is the stand-off less the
// distance from the dock point; in, the distance to its place in.
void FieldBaseDrive::begin() {
  const float* p = hk().entity_position(base);  // 0x005a85e0
  const zmth::Vec3 at{p[0], p[1], p[2]};
  zmth::Vec3 dir{};
  if (open) {
    const zmth::Vec3 dock{base->dock_at[0], base->dock_at[1], base->dock_at[2]};
    travel = -(zmth::direction(dock, at, dir) - kStandOff);  // 0x0051e3d0
  } else {
    const float* q = base->dock_in;
    travel = zmth::direction(at, zmth::Vec3{q[0], q[1], q[2]}, dir);
  }
  base->arms->aim(open);  // 0x004589a0
  reached = false;
  finished = false;
  way = open ? -1.0f : 1.0f;  // [0x005c23b4], [0x005c23a4]
}

// Mech3.exe 1.2 @ 0x00459a80
bool FieldBaseDrive::there(float d) const { return open ? -0.2f < d : d < 0.2f; }

// Mech3.exe 1.2 @ 0x00459990: a quarter throttle its way until the stopping
// distance would carry it there; done with its arms there too.
bool FieldBaseDrive::step(float) {
  const float t = base->arms->now;
  const bool arms = open ? (t == 1.0f || std::isnan(t)) : (t == 0.0f || std::isnan(t));  // test ah, 0x40
  VehicleControls& c = *base->controls;  // vt+0x140
  if (!reached) {
    const float s = hk().stopping_now(*base);  // vt+0x124
    if (there(travel - s * way)) c.throttle = 0.0f;  // IControls vt+0x20
    else c.throttle = static_cast<float>(way * 0.25);
  } else {
    c.throttle = 0.0f;
  }
  return done() || (arms && reached);
}

// Mech3.exe 1.2 @ 0x00459ac0: the controls' +0x78 (the local z speed) adds up.
void FieldBaseDrive::after(float dt) {
  travel = base->controls->x70.z * dt + travel;
  reached = there(travel);
}

// Mech3.exe 1.2 @ 0x00459af0: the base's front step dropped (this one, the
// deploy's abort assumes), the next begun.
void FieldBaseDrive::abort() {
  end();
  if (base->actions.empty()) return;
  field_base_drop_front(*base);
  base->begin_next = true;
}

// Mech3.exe 1.2 @ 0x0045b9c0
bool FieldBaseDrive::done() { return finished || !base || !base->active; }

// ---------------------------------------------------------------------------
// The registry's helpers

// Mech3.exe 1.2 @ 0x0045a720
bool FieldBases::requester_ok() const { return requester && hk().powered_down(requester); }

// Mech3.exe 1.2 @ 0x0045b740
void FieldBases::set_requester(Handle m) {
  if (!busy && field_bases_drive()) requester = m;
}

// Mech3.exe 1.2 @ 0x0045a500
void FieldBases::message(Handle m) { requester = states.empty() ? m : states.front()->message(m); }

// Mech3.exe 1.2 @ 0x0045a540
void FieldBases::abort_front() {
  if (!states.empty()) states.front()->abort();
}

// Mech3.exe 1.2 @ 0x0045b290 / 0x0045b2c0: the three slots, whatever the count.
void FieldBases::effects_on() {
  for (FieldBase* b : bases)
    if (b && b->active) field_base_effects_start(*b);
}
void FieldBases::effects_off() {
  for (FieldBase* b : bases)
    if (b) field_base_effects_stop(*b);
}

// ---------------------------------------------------------------------------
// The states

// Mech3.exe 1.2 @ 0x0045b7d0
void DockState::say_soon() {
  if (sound && field_bases_quiet_until() <= hk().game_time()) hk().say(sound);  // test ah, 0x41
}

// The aborts' unwind (0x0045ade0, 0x0045b1b0, 0x0045b3b0): the front state
// and those after it dropped until `to` is the front; the dropped front is
// ended and `to`, when reached, begun.
void DockState::cancel(DockStep* to) {
  std::list<DockStep*>& l = reg->states;
  if (l.empty() || l.front() == to) return;
  DockStep* const was = l.front();
  DockStep* at = nullptr;
  do {
    at = l.front();
    if (at == to) break;
    l.pop_front();
  } while (!l.empty());
  if (was) was->end();
  if (at == to && at) at->begin();
}

// Mech3.exe 1.2 @ 0x0045a740 / 0x0045a770
void DockIdle::begin() {
  finished = false;
  asked = false;
  reg->idle_on = true;
}
void DockIdle::end() {
  reg->idle_on = false;
  asked = false;
}
Handle DockIdle::message(Handle m) { return hk().dock_request(*this, m); }

// Mech3.exe 1.2 @ 0x0045acb0
void DockContact::begin() {
  sound = reg->sounds[7];  // +0xa8 mcb_waiting
  field_bases_contact() = true;
  finished = false;
}
// Mech3.exe 1.2 @ 0x0045ace0: once its line is due, said (and then not
// again); done with the mech near the dock point and powered down.
bool DockContact::step(float) {
  FieldBaseHooks& h = hk();
  if (field_bases_quiet_until() <= h.game_time()) {
    if (sound) h.say(sound);
    field_bases_quiet_until() = 3.4028235e38f;
  }
  const float* at = h.dock_point_position(reg->dock_point);
  const float* p = h.entity_position(reg->requester);
  const float dx = p[0] - at[0], dy = p[1] - at[1], dz = p[2] - at[2];
  const float d2 = dz * dz + dy * dy + dx * dx;
  return d2 < kNear2 && reg->requester_ok();
}
void DockContact::end() { field_bases_quiet_until() = 0.0f; }  // 0x0045acd0
// Mech3.exe 1.2 @ 0x0045ade0: "cancelled", to the reset.
void DockContact::abort() {
  field_bases_quiet_until() = 0.0f;
  sound = reg->sounds[5];  // +0xa0 mcb_pos_cancel
  say_soon();
  hk().marker_clear();
  cancel(&reg->reset);
}

// Mech3.exe 1.2 @ 0x0045ab50: "wait" after five frames.
void DockPlaces::begin() {
  until = hk().frame() + 5;
  sound = reg->sounds[6];  // +0xa4 mcb_contact
}
// Mech3.exe 1.2 @ 0x0045ac20
bool DockPlaces::step(float) {
  FieldBaseHooks& h = hk();
  if (until < h.frame()) {
    say_soon();
    field_bases_quiet_until() = h.game_time() + 5.0f;
    until = 0x7fffffff;
  }
  return done();  // vt+0x18
}
// Mech3.exe 1.2 @ 0x0045ac70
void DockPlaces::end() {
  for (ai::Valve* v : reg->valves)
    if (v) hk().valve_set(v, 0);
  finished = false;
}
void DockPlaces::abort() {  // 0x0045ade0, as the contact's
  field_bases_quiet_until() = 0.0f;
  sound = reg->sounds[5];
  say_soon();
  hk().marker_clear();
  cancel(&reg->reset);
}
// Mech3.exe 1.2 @ 0x0045ab70: each base out of play, without a valve, or its
// valve set (by its net: in place). FIX (bug): a missing base counts as in
// place (the exe follows it).
bool DockPlaces::done() {
  if (finished) return true;
  for (int i = 0; i < 3; ++i) {
    const FieldBase* b = reg->bases[i];
    ai::Valve* v = reg->valves[i];
    if (b && b->active && v && hk().valve_value(v) == 0) return false;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x0045af00: busy; the three drives out; the player's view
// onto the dock's camera (mode 4), its old one kept; the mech at rest at the
// dock point.
void DockDeploy::begin() {
  FieldBaseHooks& h = hk();
  reg->busy = true;
  sound = reg->sounds[8];  // +0xac mcb_deploy
  finished = false;
  queue_drives(*reg, drives, true);
  if (reg->requester == h.view_vehicle()) {
    reg->camera = h.view_camera();
    h.view_set_camera(4);
  }
  h.mech_rest_at(reg->requester, h.dock_point_position(reg->dock_point));
}
// Mech3.exe 1.2 @ 0x0045b0e0: the mech powering up aborts.
bool DockDeploy::step(float) {
  if (drives_done(drives)) {
    say_soon();
    return true;
  }
  if (!reg->requester_ok()) abort();  // vt+0x14
  return false;
}
// Mech3.exe 1.2 @ 0x0045b150
void DockDeploy::end() {
  reg->busy = false;
  free_drives(drives);
}
// Mech3.exe 1.2 @ 0x0045b770
Handle DockDeploy::message(Handle m) {
  if (reg->requester != m) hk().say(reg->sounds[3]);  // +0x98 mcb_occupied
  return reg->requester;
}
// Mech3.exe 1.2 @ 0x0045b1b0: "aborted", the drives dropped, to the close.
void DockDeploy::abort() {
  field_bases_quiet_until() = 0.0f;
  sound = reg->sounds[9];  // +0xb0 mcb_abort
  say_soon();
  for (FieldBaseDrive* d : drives)
    if (d) d->abort();  // vt+0x14
  hk().marker_clear();
  cancel(&reg->close);
}

// Mech3.exe 1.2 @ 0x0045b2f0
void DockRepair::begin() {
  reg->busy = true;
  sound = reg->sounds[10];  // +0xb4 mcb_complete
  finished = false;
  timer = 12.0f;
  reg->effects_on();
}
// Mech3.exe 1.2 @ 0x0045b320: the mech repaired when the time is up (with
// this frame's time); powering up first aborts.
bool DockRepair::step(float dt) {
  timer = timer - dt;
  if (timer <= 0.0f) {
    if (reg->requester) hk().mech_repair(reg->requester, dt);
    hk().marker_clear();
    return true;
  }
  if (!reg->requester_ok()) {
    abort();
    return true;
  }
  return false;
}
// Mech3.exe 1.2 @ 0x0045b380: "complete".
void DockRepair::end() {
  reg->busy = false;
  field_bases_quiet_until() = 0.0f;
  say_soon();
  reg->effects_off();
}
Handle DockRepair::message(Handle m) {  // 0x0045b770
  if (reg->requester != m) hk().say(reg->sounds[3]);
  return reg->requester;
}
// Mech3.exe 1.2 @ 0x0045b3b0: to the close, its line not said.
void DockRepair::abort() {
  sound = reg->sounds[9];
  hk().marker_clear();
  cancel(&reg->close);
}

// Mech3.exe 1.2 @ 0x0045b460: the drives in; the player's view back from the
// dock's camera.
void DockClose::begin() {
  FieldBaseHooks& h = hk();
  finished = false;
  queue_drives(*reg, drives, false);
  if (reg->requester == h.view_vehicle() && h.view_camera() == 4) h.view_set_camera(reg->camera);
}
bool DockClose::step(float) { return drives_done(drives); }  // 0x0045b620
// Mech3.exe 1.2 @ 0x0045b660
void DockClose::end() {
  free_drives(drives);
  field_bases_contact() = false;
}
Handle DockClose::message(Handle m) {  // 0x0045b770
  if (reg->requester != m) hk().say(reg->sounds[3]);
  return reg->requester;
}

// Mech3.exe 1.2 @ 0x0045b6c0: the bases held again, "mcbdeploy" 0.
void DockReset::begin() {
  finished = false;
  field_bases_contact() = false;
  field_bases_drive() = false;
  hk().valve_set(reg->deploy_valve, 0);
}
// Mech3.exe 1.2 @ 0x0045b6f0
void DockReset::end() { reg->states.push_back(&reg->idle); }

}  // namespace mech3
