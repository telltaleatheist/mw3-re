#include "mech3/zrdrvalve.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gamez/zsound/zsnd.h"
#include "mech3/zai_steps.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
ValveHooks g_default_hooks;
ValveHooks* g_hooks = &g_default_hooks;

template <class... A>
void report(int line, const char* fmt, A... args) {
  char msg[512];
  std::snprintf(msg, sizeof msg, fmt, args...);
  g_hooks->report(0x200, line, msg);
}

char* dup(const char* s) { return s ? strdup(s) : nullptr; }
void set_str(char*& slot, const char* s) {
  std::free(slot);
  slot = dup(s);
}

// A found tag's item k. FIX (bug): the exe reads any value as the list; FIX
// (A): an item past the list's end is whatever the heap holds there (none).
const zrdr::Node& item(const zrdr::Node* f, int32_t k) {
  static const zrdr::Node kNone{};
  if (!f || f->type != zrdr::List || k >= f->list[0].i) return kNone;
  return f->list[k];
}
uint32_t word(const zrdr::Node& n) {
  if (n.type == zrdr::Int) return static_cast<uint32_t>(n.i);
  if (n.type == zrdr::Float) {
    uint32_t u;
    std::memcpy(&u, &n.f, 4);
    return u;
  }
  return 0;  // FIX (A): a string's or list's pointer bits in the exe
}
// FIX (bug): the exe takes any value as a string pointer.
const char* text(const zrdr::Node& n) { return n.type == zrdr::String ? n.s : nullptr; }
float as_float(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

ValveFile*& file_slot() {
  static ValveFile* f = new ValveFile;  // 0x0060b668, made by the static initializer 0x004e1ca0
  return f;
}

// The team masks from a team number packed 5 bits at a time (as player.cpp's).
void masks(uint32_t team, uint32_t* mask, uint32_t* allies, uint32_t* enemies) {
  uint32_t m = 1u << (team & 0x1f);
  while ((team >>= 5) != 0) m |= 1u << (team & 0x1f);
  *mask = m;
  *allies = m & ~1u;
  *enemies = ~*allies & ~1u;
}
}  // namespace

ValveHooks& valve_hooks() { return *g_hooks; }
void set_valve_hooks(ValveHooks* h) { g_hooks = h ? h : &g_default_hooks; }
Handle ValveHooks::sound_find(const char* name) {
  return static_cast<Handle>(reinterpret_cast<uintptr_t>(zsnd::find(name)));
}

ValveFile& valve_file() { return *file_slot(); }
void valve_file_reset() {
  ValveFile*& f = file_slot();
  if (f->tree) f->RdrFile::unload();
  delete f;
  f = new ValveFile;
}

// --- the record ----------------------------------------------------------------

// Mech3.exe 1.2 @ 0x004e1d70
ValveDef::~ValveDef() { clear(); }

// The defaults the parse starts from (0x004e2330's statics at 0x0060b770..
// 0x0060b7cc: all zero); the namelist emptied, bits 0-4 of the flags cleared.
void ValveDef::clear() {
  init = {};
  for (Named* n : {&sound, &anim, &scan[0], &scan[1], &scan[2], &scan[3], &shutdown, &destroy}) {
    std::free(n->name);
    *n = {};
  }
  complete = abort = reset = {};
  std::free(update), update = nullptr;
  std::free(delay_valve), delay_valve = nullptr;
  delay = 0;
  std::free(team_entity), team_entity = nullptr;
  team_value = 0, team = 0;
  std::free(attack_entity), attack_entity = nullptr;
  attack_value = 0, strategy = 0;
  std::free(teleport), teleport = nullptr;
  teleport_value = 0;
  pos[0] = pos[1] = pos[2] = 0;  // 0x0063d870
  yaw = 0;
  names.names.clear();
  flags &= 0xe0;
}

// Mech3.exe 1.2 @ 0x004e2330. Each tag's value is a list; names are copied,
// numbers kept as their words. A later tag of a pair (sound_once after
// sound, animation_once after animation) replaces the first.
ValveDef* ValveDef::parse(zrdr::Node* value, const char* /*name*/) {
  if (!value) return nullptr;
  clear();
  // FIX (bug): the exe reads a value that is no list as one.
  if (value->type != zrdr::List || value->list[0].i <= 0) return this;
  const zrdr::Node* f;
  auto find = [&](const char* key) { return f = zrdr::find(value, key); };
  auto named = [&](Named& n, int vi, int fi, bool flag_is_item) {
    set_str(n.name, text(item(f, 1)));
    n.value = static_cast<int32_t>(word(item(f, vi)));
    if (flag_is_item) n.flag = word(item(f, fi)) != 0;
  };
  if (find("value")) init.on = 1, init.value = static_cast<int32_t>(word(item(f, 1)));
  if (find("sound")) named(sound, 2, 0, false);
  if (find("sound_once")) named(sound, 2, 0, false), sound.flag = 1;
  if (find("animation")) named(anim, 2, 0, false);
  if (find("animation_once")) named(anim, 2, 0, false), anim.flag = 1;
  const char* const kScans[4] = {"scan_aim", "scan_hit", "scan_time", "canleave"};
  for (int k = 0; k < 4; ++k)
    if (find(kScans[k])) named(scan[k], 3, 2, true);
  if (find("shutdown")) named(shutdown, 3, 2, true);
  if (find("missionreset")) reset.on = 1, reset.value = static_cast<int32_t>(word(item(f, 1)));
  if (find("missionabort")) abort.on = 1, abort.value = static_cast<int32_t>(word(item(f, 1)));
  if (find("missioncomplete")) complete.on = 1, complete.value = static_cast<int32_t>(word(item(f, 1)));
  if (find("destroy")) named(destroy, 2, 0, false);
  if (find("setteam")) {
    set_str(team_entity, text(item(f, 1)));
    team = word(item(f, 2));
    team_value = static_cast<int32_t>(word(item(f, 3)));
  }
  if (find("attack_strategy")) {
    set_str(attack_entity, text(item(f, 1)));
    strategy = word(item(f, 2));
    attack_value = static_cast<int32_t>(word(item(f, 3)));
  }
  if (find("update")) set_str(update, text(item(f, 1)));
  if (find("delayupdate")) {
    set_str(delay_valve, text(item(f, 1)));
    delay = word(item(f, 2));
  }
  if (find("all_nonzero")) flags |= 4;
  if (find("all_zero")) flags |= 8;
  if (find("any_nonzero")) flags |= 1;
  if (find("any_zero")) flags |= 2;
  if (find("namelist") && f->type == zrdr::List) {
    const int32_t n = f->list[0].i;
    for (int32_t k = 1; k < n; ++k) {
      const char* s = text(f->list[k]);
      names.names.push_back(s ? std::optional<std::string>(s) : std::nullopt);
    }
  }
  if (find("teleport")) {
    // 0x00557030 on item 1 (a string, or 0 for none); 0x00556b70 on item 2,
    // 0x00556fe0 on item 3, item 4's word.
    const char* s = nullptr;
    if (zrdr::get_string_value(const_cast<zrdr::Node*>(&item(f, 1)), &s)) set_str(teleport, s);
    zrdr::get_vec3(const_cast<zrdr::Node*>(&item(f, 2)), pos);
    zrdr::get_float_value(const_cast<zrdr::Node*>(&item(f, 3)), &yaw);
    teleport_value = static_cast<int32_t>(word(item(f, 4)));
  }
  return this;
}

// Mech3.exe 1.2 @ 0x004e5e30 (the editor's cached records, 0x004e6150, are
// never there in play).
ValveDef* ValveFile::read(int32_t i, const char** name) {
  zrdr::Node* value = record(i, name);
  if (!value) return nullptr;
  return def.parse(value, *name);
}

// --- the actions -------------------------------------------------------------------

// Mech3.exe 1.2 @ 0x004e2ff0 (sound_once 0x004e3070)
SoundAction::SoundAction(const char* name, int32_t v, bool once)
    : ValveAction(once ? SoundOnce : Sound), value(v) {
  sound = g_hooks->sound_find(name);
}

// Mech3.exe 1.2 @ 0x004e3040 / 0x004e30b0 (once: forgotten when played).
void SoundAction::fire(Valve& v) {
  if (v.get() != value) return;
  if (kind == SoundOnce) {
    if (!sound) return;
    g_hooks->sound_play(sound);
    sound = 0;
    return;
  }
  g_hooks->sound_play(sound);
}

// Mech3.exe 1.2 @ 0x004e30f0 (animation_once 0x004e3150)
AnimAction::AnimAction(const char* name, int32_t v, bool once) : ValveAction(once ? AnimOnce : Anim), value(v) {
  anim = g_hooks->anim_find(name);
}

// Mech3.exe 1.2 @ 0x004e3120 / 0x004e3180
void AnimAction::fire(Valve& v) {
  if (v.get() != value) return;
  if (kind == AnimOnce) {
    if (!anim) return;
    g_hooks->anim_run(anim);
    anim = 0;
    return;
  }
  g_hooks->anim_run(anim);
}

// Mech3.exe 1.2 @ 0x004e31c0
ScanAction::ScanAction(Handle b, uint32_t t, int32_t a, int32_t v)
    : ValveAction(Scan), brain(b), test(t), arg(a), value(v) {}

// Mech3.exe 1.2 @ 0x004e3200: the test function on the brain (+arg).
// FIX (bug): without a brain (a vehicle that is not there, or not the AI's)
// the exe writes through null.
void ScanAction::fire(Valve& v) {
  if (v.get() != value || !brain) return;
  const Handle b = brain + static_cast<Handle>(arg);
  switch (static_cast<ScanTest>(test)) {  // 0x004d9c20..0x004d9ca0
    case ScanTest::AimOn: return g_hooks->brain_write(b, 0x98, 1);
    case ScanTest::AimOff: return g_hooks->brain_write(b, 0x98, 0);
    case ScanTest::HitOn: return g_hooks->brain_write(b, 0x9c, 1);
    case ScanTest::HitOff: return g_hooks->brain_write(b, 0x9c, 0);
    case ScanTest::TimeOn: return g_hooks->brain_write(b, 0xa0, 1);
    case ScanTest::TimeOff: return g_hooks->brain_write(b, 0xa0, 0);
    case ScanTest::CanLeave:
      g_hooks->brain_write(b, 0x3c, 1);
      return g_hooks->brain_write(b, 0xdc, 0x7f7fffff);
    case ScanTest::CanLeaveNot:
      g_hooks->brain_write(b, 0x3c, 0);
      return g_hooks->brain_write(b, 0xdc, 0x7f7fffff);
  }
}

// Mech3.exe 1.2 @ 0x004e3330: the missions' function (ECX 0x006098d0).
void MissionAction::fire(Valve& v) {
  if (v.get() != value) return;
  if (fn == 0x48f730) g_hooks->mission_abort();
  else g_hooks->mission_complete();
}

// Mech3.exe 1.2 @ 0x004e34b0: a mech is destroyed (0x0041cfa0(1)), anything
// else through its vt+0x70.
void DestroyAction::fire(Valve& v) {
  if (v.get() != value) return;
  const Handle e = g_hooks->target_find(has_name ? name.c_str() : nullptr);
  if (!e) return;
  if (const Handle m = g_hooks->mech_of(e)) return g_hooks->mech_destroy(m);
  g_hooks->destroy(e);
}

// Mech3.exe 1.2 @ 0x004e3650: a mech powered down (down != 0) or up.
void ShutdownAction::fire(Valve& v) {
  if (v.get() != value) return;
  const Handle e = g_hooks->target_find(has_name ? name.c_str() : nullptr);
  if (!e) return;
  const Handle veh = g_hooks->as_vehicle(e);
  const Handle m = veh ? g_hooks->mech_of(veh) : 0;
  if (m) g_hooks->mech_power(m, down != 0);
}

// Mech3.exe 1.2 @ 0x004e37f0: the entity's AiTarget masks from the team, as
// the mission start sets them (mission-start.md section 6).
void SetTeamAction::fire(Valve& v) {
  if (v.get() != value) return;
  const Handle e = g_hooks->target_find(has_name ? name.c_str() : nullptr);
  if (!e) return;
  uint32_t m, a, en;
  masks(team, &m, &a, &en);
  g_hooks->target_masks(e, m, a, en);
}

// Mech3.exe 1.2 @ 0x004e39c0: the brain's +0x40 (aiv.zrd p2's first item).
void AttackAction::fire(Valve& v) {
  if (v.get() != value) return;
  const Handle e = g_hooks->target_find(has_name ? name.c_str() : nullptr);
  if (!e) return;
  const Handle veh = g_hooks->as_vehicle(e);
  const Handle o = veh ? g_hooks->vehicle_of(veh) : 0;
  const Handle c = o ? g_hooks->controls_of(o) : 0;
  const Handle b = c ? g_hooks->brain_of(c) : 0;
  if (b) g_hooks->brain_write(b, 0x40, strategy);
}

TeleportAction::TeleportAction(const char* n, int32_t v, const float p[3], float y)
    : EntityAction(Teleport, n, v), yaw(y) {
  pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
}

// Mech3.exe 1.2 @ 0x004e3ba0: zEntity's vehicle placed, then its pilot given
// its current order's net again (so it plans from the new place). FIX (bug):
// a brain without an order is skipped (the exe reads before its deque).
void TeleportAction::fire(Valve& v) {
  if (v.get() != value) return;
  const Handle z = g_hooks->entity_find(has_name ? name.c_str() : nullptr);
  const Handle veh = z ? g_hooks->as_vehicle(z) : 0;
  const Handle o = veh ? g_hooks->vehicle_of(veh) : 0;
  if (!o) return;
  g_hooks->vehicle_place(o, pos, yaw);
  const Handle c = g_hooks->controls_of(o);
  const Handle b = c ? g_hooks->brain_of(c) : 0;
  if (!b) return;
  const Handle net = g_hooks->brain_order_net(b);
  if (const Handle c2 = g_hooks->controls_of(o)) g_hooks->controls_set_net(c2, net);
}

// Mech3.exe 1.2 @ 0x004e3d90
void UpdateAction::fire(Valve&) {
  if (target) target->fire();
}

// Mech3.exe 1.2 @ 0x004e3fb0: on any change, a timer that sets the target to
// the trigger's value after the delay.
void DelayUpdateAction::fire(Valve& v) {
  if (!target) return;
  const int32_t val = v.get();
  new DelayTimer(*this, val);
}

// Mech3.exe 1.2 @ 0x004e3e60: joins the timers (0x004d4480).
DelayTimer::DelayTimer(const DelayUpdateAction& a, int32_t v) : ValveAction(Timer), target(a.target), value(v) {
  if (!target) return;
  time = a.delay + g_hooks->game_time();
  valve_timers_add(this);
}

// Mech3.exe 1.2 @ 0x004e3f10
bool DelayTimer::tick(float now) {
  if (!(time < now)) return false;
  target->set(value);
  delete this;
  return true;
}

// --- the conditions -----------------------------------------------------------------

// Mech3.exe 1.2 @ 0x004e40e0 (with 0x004e4d20 / 0x004e4df0 / 0x004e4ec0 /
// 0x004e4f90): the member valves by name (made when new; a name that is
// none is dropped, with a report), each given an update of the owner.
Condition::Condition(Kind k, const NameList& list, Valve& owner) : kind(k), x04(list.x00) {
  bool missing = false;
  members.reserve(list.names.size());
  for (const auto& n : list.names) {
    Valve* v = n ? valve_get(n->c_str()) : nullptr;
    if (v) members.push_back(v);
    else missing = true;
  }
  if (missing) report(0x3d8, "Could not locate all valves in namelist for valve \"%s\"", owner.name);
  for (Valve* m : members) {  // 0x004e4080 -> 0x004e3e00 / 0x004e3d70
    if (!m->add(new UpdateAction(&owner)))
      report(0x3c4, "Failed to create update on valve \"%s\" for valve \"%s\"", m->name, owner.name);
  }
}

// Mech3.exe 1.2 @ 0x004e4360 (any nonzero), 0x004e43b0 (any zero), 0x004e4400
// (all nonzero), 0x004e4430 (all zero).
int32_t Condition::get() const {
  bool any_zero = false, any_nonzero = false;
  for (const Valve* m : members) (m->get() != 0 ? any_nonzero : any_zero) = true;
  switch (kind) {
    case AnyNonzero: return any_nonzero;
    case AnyZero: return any_zero;
    case AllNonzero: return !any_zero;
    case AllZero: return !any_nonzero;
  }
  return 0;
}

// --- building a valve ------------------------------------------------------------------

namespace {
// The actions' common end: added to the valve, else reported and deleted
// (neither can fail here: `new` does not).
void attach(Valve& v, Reaction* r, int line, const char* fmt, const char* what) {
  if (r && v.add(r)) return;
  report(line, fmt, what, v.name);
  delete r;
}

// 0x004e3230: the brain of the vehicle named (0x004d7330, vt+0x40, vt+0x140,
// vt+0x54), then the switch on or off by the record's flag.
Reaction* make_scan(const ValveDef::Named& s, uint32_t on, uint32_t off) {
  const Handle e = g_hooks->target_find(s.name);
  const Handle o = e ? g_hooks->vehicle_of(e) : 0;
  const Handle c = o ? g_hooks->controls_of(o) : 0;
  const Handle b = c ? g_hooks->brain_of(c) : 0;
  return new ScanAction(b, s.flag ? on : off, 0, s.value);
}
}  // namespace

// Mech3.exe 1.2 @ 0x004e44b0 (Ghidra splits it at 0x004e46d1)
Valve* build_valve(const char* name, const ValveDef& d) {
  Valve* v = valve_get(name);
  if (v) {
    const bool cond = (d.flags & 0x0f) != 0;
    const bool listed = !d.names.names.empty();
    if (!cond) {
      if (listed) report(0x41e, "Expected no namelist for valve \"%s\"", name);
    } else if (!listed) {
      report(0x422, "Expected non empty namelist for valve \"%s\"", name);
    } else if (!v->value->replaceable()) {
      report(0x424, "Conflicting definition for valve \"%s\"", name);
      v = nullptr;
    } else {
      const Condition::Kind k = (d.flags & 1)   ? Condition::AnyNonzero
                                : (d.flags & 2) ? Condition::AnyZero
                                : (d.flags & 4) ? Condition::AllNonzero
                                                : Condition::AllZero;
      Condition* c = new Condition(k, d.names, *v);
      delete v->value;
      v->value = c;
    }
  }
  if (!v) {
    report(0x4c4, "Failed to create valve \"%s\"", name);
    return nullptr;
  }
  if (d.init.on) v->value->set(d.init.value);
  if (d.sound.name)
    attach(*v, new SoundAction(d.sound.name, d.sound.value, d.sound.flag != 0), 0x449,
           "Failed to get sound \"%s\" for valve \"%s\"", d.sound.name);
  if (d.anim.name)
    attach(*v, new AnimAction(d.anim.name, d.anim.value, d.anim.flag != 0), 0x455,
           "Failed to get animation \"%s\" for valve \"%s\"", d.anim.name);
  static const struct {
    ScanTest on, off;
    int line;
    const char* fmt;
  } kScans[4] = {
      {ScanTest::AimOn, ScanTest::AimOff, 0x45c, "Failed to create scan aim on vehicle \"%s\" for valve \"%s\""},
      {ScanTest::HitOn, ScanTest::HitOff, 0x463, "Failed to create scan aim on vehicle \"%s\" for valve \"%s\""},
      {ScanTest::TimeOn, ScanTest::TimeOff, 0x46a, "Failed to create scan time on vehicle \"%s\" for valve \"%s\""},
      {ScanTest::CanLeave, ScanTest::CanLeaveNot, 0x471, "Failed to create canleave on vehicle \"%s\" for valve \"%s\""},
  };
  for (int k = 0; k < 4; ++k)
    if (d.scan[k].name)
      attach(*v, make_scan(d.scan[k], static_cast<uint32_t>(kScans[k].on), static_cast<uint32_t>(kScans[k].off)),
             kScans[k].line, kScans[k].fmt, d.scan[k].name);
  if (d.shutdown.name)
    attach(*v, new ShutdownAction(d.shutdown.name, d.shutdown.value, d.shutdown.flag), 0x478,
           "Failed to create shutdown on vehicle \"%s\" for valve \"%s\"", d.shutdown.name);
  if (d.destroy.name)
    attach(*v, new DestroyAction(ValveAction::Destroy, d.destroy.name, d.destroy.value), 0x47f,
           "Failed to create destroy on entity \"%s\" for valve \"%s\"", d.destroy.name);
  if (d.team_entity)
    attach(*v, new SetTeamAction(d.team_entity, d.team_value, d.team), 0x486,
           "Failed to create setteam on entity \"%s\" for valve \"%s\"", d.team_entity);
  if (d.attack_entity)
    attach(*v, new AttackAction(d.attack_entity, d.attack_value, d.strategy), 0x48d,
           "Failed to create attack_strategy on entity \"%s\" for valve \"%s\"", d.attack_entity);
  if (d.teleport)  // the exe reports with the shutdown's message
    attach(*v, new TeleportAction(d.teleport, d.teleport_value, d.pos, d.yaw), 0x494,
           "Failed to create shutdown on vehicle \"%s\" for valve \"%s\"", d.teleport);
  // missionreset is not made at all: the exe only reports it.
  if (d.reset.on) report(0x4a1, "Failed to create mission reset for valve \"%s\"", name);
  if (d.abort.on) {
    Reaction* r = new MissionAction(0x48f730, d.abort.value);
    if (!v->add(r)) report(0x4a8, "Failed to create mission abort for valve \"%s\"", name), delete r;
  }
  if (d.complete.on) {
    Reaction* r = new MissionAction(0x48f710, d.complete.value);
    if (!v->add(r)) report(0x4b0, "Failed to create mission complete for valve \"%s\"", name), delete r;
  }
  if (d.update)
    attach(*v, new UpdateAction(valve_get(d.update)), 0x4b8, "Failed to create update on valve \"%s\" for valve \"%s\"",
           d.update);
  if (d.delay_valve)  // the exe's report names the update's valve
    attach(*v, new DelayUpdateAction(valve_get(d.delay_valve), as_float(d.delay)), 0x4bf,
           "Failed to create delayed update on valve \"%s\" for valve \"%s\"", d.update);
  return v;
}

// Mech3.exe 1.2 @ 0x004e2120: a reference on valves.zrd held (0x004e1d00)
// while every record is built (0x004e20d0, each through 0x004e2fb0).
void valves_load() {
  ValveFile& f = valve_file();
  f.add_ref();
  for (int32_t i = 0; i < f.record_count(); ++i) {
    const char* name = nullptr;
    const ValveDef* d = f.read(i, &name);
    if (!d || !name) continue;
    if (!build_valve(name, *d)) report(0x17a, "Failed to create valve \"%s\"", name);
  }
  f.release();  // 0x004e1f10
}

}  // namespace mech3::ai
