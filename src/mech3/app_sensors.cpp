// The sensors' hooks wired to the program (docs/ai.md section 15): the
// virtual calls the exe makes on vehicles answered as the Mech's and
// CZAiTarget's vtables do (0x005bcef8, 0x005ccb90), their fields read from
// the ports' objects, the nodes' world positions from zClass. Not original
// code. What the mech's slots do beyond the sensors (the HUD's lock 0x0041d890,
// the brain's refresh 0x004aedd0, empty) is noted; the level of detail is mech_lod's.
// The animation-owner registry's hooks too (ai.md section 18).
#include "mech3/app_sensors.h"
#include "mech3/app_hud.h"
#include "mech3/app_mech.h"
#include "mech3/app_turret.h"
#include "mech3/app_vehicle.h"
#include "mech3/mech_hud.h"
#include "mech3/mech_lod.h"

#include <cmath>

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zmath/zmth_matrix.h"
#include "mech3/mech.h"
#include "mech3/mech_control.h"
#include "mech3/sensors.h"
#include "mech3/entity_classes.h"
#include "mech3/turret.h"
#include "mech3/zai_anim_owner.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_target.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_inout.h"

namespace mech3 {

void player_set_60b56c(uint8_t v);  // mech3/player.h, whose Handle is not mech.h's
const char* program_pilot_name();  // app_menus.cpp: the current pilot's (+0x18)

namespace {

MechWiring g_w;
const char* (*g_message)(void* e, int32_t id) = nullptr;

uint32_t log_unported(const char* key, std::initializer_list<zgame::Arg> args) {
  return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
}
uint32_t handle(const void* p) { return p && g_w.handle_of ? g_w.handle_of(g_w.engine, p) : 0; }
void* object(uint32_t h) { return h && g_w.object_of ? g_w.object_of(g_w.engine, h) : nullptr; }

// The mechs are zEntity's entries that are mechs (entity_is_mech, the exe's
// vt+0x2c then vt+0x50); the targets are targets.zrd's.
Mech* mech(uint32_t h) {
  void* o = object(h);
  return entity_is_mech(o) ? static_cast<Mech*>(o) : nullptr;
}
// The basic vehicles (app_vehicle.cpp).
void* vehicle(uint32_t h) {
  void* o = object(h);
  return is_program_vehicle(o) ? o : nullptr;
}
ai::CZAiTarget* target(uint32_t h) {
  for (ai::CZAiTarget* t : ai::target_list())
    if (h && t->self == h) return t;
  return nullptr;
}
ai::Brain* brain_of(const Mech* m) {
  auto* c = m ? dynamic_cast<AiControls*>(m->controls) : nullptr;
  return c ? c->brain.get() : nullptr;
}

// 0x00448600 on a node: its world matrix refreshed when stale (0x0051e9d0,
// 0x0051fde0, 0x004ed280(node, 1), 0x0051ea00), the translation (+0x84).
void world_position(zcls::Node* n, float out[3]) {
  out[0] = out[1] = out[2] = 0;
  if (!n || !n->data || n->cls() != 5) return;
  auto* d = static_cast<zcls::Object3d*>(n->data);
  if (d->get<uint32_t>(0) & 0x20) {
    zmth::Mat34 buf;
    zmth::push(&buf);
    zmth::load_identity();
    zcls::node_to_world(n, 1);
    zmth::pop();
  }
  for (int i = 0; i < 3; ++i) out[i] = d->floats(0x84)[i];
}

struct ProgramSensors : ai::SensorHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return log_unported(key, args);
  }
  uint32_t vcall(zgame::Handle o, uint32_t slot, std::initializer_list<zgame::Arg> args) override {
    if (Mech* m = mech(o)) {  // 0x005bcef8
      switch (slot) {
        case 0x24: case 0x28: case 0x2c: case 0x40: case 0x50: return o;  // 0x004d2a70: itself
        case 0xa0: return m->power_down_pending || m->powered_down ? 1 : 0;  // 0x0041cc10
        case 0xb4: case 0xec: return 0;
        case 0xcc: return m->active ? 1 : 0;  // 0x004aee00: +0x08, the controls' answer
        case 0xd0: return static_cast<uint32_t>(lod_update(*m));  // 0x0041e870: the level of detail
        case 0xc8:  // 0x0041e810: the brain told, then the detail budget
          // 0x004aedd0: the controls' (vt+0x140) brain (vt+0x54) vt+0x08, the
          // empty 0x005ac5c0 in every brain's vtable (ai.md, The brain's
          // refresh): nothing to do.
          if (Mech* p = world().mission_player) lod_budget(*p);
          return 0;
        case 0x140: return handle(m->controls);
        case 0x1c: log_unported("0x005a85a0", {o}); return 0;
        default: break;
      }
    } else if (void* v = vehicle(o)) {  // the basic vehicle's 0x005c88e8
      switch (slot) {
        case 0x24: case 0x28: case 0x2c: case 0x40: case 0xec: return o;  // 0x004d2a70
        case 0x50: case 0xa0: case 0xb4: return 0;  // 0x0040bae0, 0x004d7290
        case 0xcc: return program_vehicle_awake(v) ? 1 : 0;   // 0x004aee00
        case 0xd0: return program_vehicle_sees(v, o) ? 1 : 0; // 0x004aee40
        case 0xc8: program_vehicle_sense_refresh(v); return 0;  // 0x004add90
        case 0x140: return handle(program_vehicle_controls(v));  // 0x004020b0
        case 0x1c: program_vehicle_refresh(v); return 0;      // 0x005a85a0 as 0x005a85c0's
        default: break;
      }
    } else if (tur::Turret* t = program_turret_by_handle(o)) {  // 0x005c8770
      switch (slot) {
        case 0x20: case 0x24: case 0x28: case 0x2c: case 0x48: case 0x4c: return o;  // 0x004d2a70
        case 0xa0: return t->idle_now() ? 1 : 0;  // 0x004a7a60
        case 0xcc: return t->awake() ? 1 : 0;     // 0x004a7a70
        case 0xd0: return t->in_range() ? 1 : 0;  // 0x004a7ab0
        default: return 0;  // 0x0040bae0, 0x004d7290, the empty 0x005ac5c0 (vt+0xc8)
      }
    } else if (ai::CZAiTarget* t = target(o)) {  // 0x005ccb90
      switch (slot) {
        case 0x24: case 0x28: case 0x2c: return o;
        case 0xb4: return (t->desc.flags & 2) ? 0 : 1;  // 0x004d3380: hidden unless "radar"
        case 0xcc: return t->active;                    // 0x004d7200
        case 0xd0: return 1;                            // 0x004d7210
        default: return 0;
      }
    } else if (slot == 0x54) {  // the controls' brain (IControls vt+0x54)
      for (zgame::Handle e : ai::entity_list()) {
        Mech* m = mech(e);
        if (m && handle(m->controls) == o) return handle(brain_of(m));
      }
      if (void* b = program_vehicle_brain(object(o))) return handle(b);
      return 0;
    }
    return SensorHooks::vcall(o, slot, args);
  }
  ai::AiTarget* fields(zgame::Handle x) override {
    if (Mech* m = mech(x)) return m;
    if (void* v = vehicle(x)) return static_cast<ai::AiTarget*>(program_vehicle_target(v));
    if (tur::Turret* t = program_turret_by_handle(x)) return t;
    return target(x);
  }
  int32_t equipment(zgame::Handle m, int32_t kind) override {
    const Mech* p = mech(m);
    return p && kind >= 0 && kind < 15 ? p->equipment[kind] : 0;
  }
  uint8_t active(zgame::Handle t) override {
    const ai::CZAiTarget* p = target(t);
    return p ? p->active : 0;
  }
  void set_sensed(zgame::Handle e, uint8_t on) override {
    if (Mech* m = mech(e)) m->x09 = on != 0;
    else if (void* v = vehicle(e)) program_vehicle_set_sensed(v, on != 0);
    else if (tur::Turret* t = program_turret_by_handle(e)) t->sensed = on;
    else if (ai::CZAiTarget* t = target(e)) t->sensed = on;
  }
  zcls::Node* node(zgame::Handle x, bool centroid) {
    if (Mech* m = mech(x)) return static_cast<zcls::Node*>(centroid ? m->centroid : m->node);
    if (void* v = vehicle(x))
      return static_cast<zcls::Node*>(centroid ? program_vehicle_centroid(v) : program_vehicle_node(v));
    if (tur::Turret* t = program_turret_by_handle(x))
      return static_cast<zcls::Node*>(object(centroid ? t->centroid_node : t->node));
    if (ai::CZAiTarget* t = target(x)) return static_cast<zcls::Node*>(object(centroid ? t->centroid_node : t->node));
    return nullptr;
  }
  void centroid(zgame::Handle x, float out[3]) override { world_position(node(x, true), out); }
  void node_position(zgame::Handle x, float out[3]) override { world_position(node(x, false), out); }
  void entity_refresh(zgame::Handle x) override {
    if (mech(x)) log_unported("0x005a85c0", {x});  // the mech's own update keeps its node
    else if (void* v = vehicle(x)) program_vehicle_refresh(v);
  }
  // 0x004adce0 on the pinged vehicle's engine sound (vt+0xec is the vehicle).
  void ping(zgame::Handle s, const float pos[3]) override {
    if (void* v = vehicle(s)) return program_vehicle_engine_play(v, pos);
    SensorHooks::ping(s, pos);
  }
  // vt+0xd4 by the class: a mech's and a basic vehicle's 0x004aeec0, the
  // rest (turrets, the command marker) AiTarget's 0x004aa4d0.
  void forget(zgame::Handle v, zgame::Handle x) override {
    if (Mech* m = mech(v)) return program_mech_forget(m, v, x);
    if (void* b = vehicle(v)) return program_vehicle_forget(b, v, x);
    ai::forget_entity(v, x);
  }
  void anim_owner_release(zgame::Handle x) override { ai::anim_owner_release(x); }
  void set_target(zgame::Handle x, ai::Contact* c, bool keep) override {
    ai::AiTarget* a = fields(x);
    if (!a) return;
    a->set_target(c, keep);
    if (!keep && mech(x) && g_w.view && x == g_w.peek32(g_w.engine, g_w.view(g_w.engine) + 0x64))
      program_hud_target_set(mech(x));  // 0x0041d890's HUD part: TARGET on for the view mech's new target
  }
  uint8_t brain_lancemate(zgame::Handle b) override {
    const auto* p = static_cast<const ai::Brain*>(object(b));
    return p ? p->params.raw[0xcd] : 0;
  }
  const char* brain_pilot(zgame::Handle b) override {
    const auto* p = static_cast<const ai::Brain*>(object(b));
    return p ? p->params.pilot : nullptr;
  }
  const char* message(int32_t id) override { return g_message ? g_message(g_w.engine, id) : nullptr; }
  zgame::Handle player() override {
    const uint32_t p = g_w.pilot ? g_w.pilot(g_w.engine) : 0;
    return p ? g_w.peek32(g_w.engine, p + 0x14) : 0;
  }
  zgame::Handle view_target() override {
    const uint32_t v = g_w.view ? g_w.view(g_w.engine) : 0;
    return v ? g_w.peek32(g_w.engine, v + 0x64) : 0;
  }
  zgame::Handle stealth() override { return g_w.peek32 ? g_w.peek32(g_w.engine, 0x609a2c) : 0; }
  void set_sorted() override { player_set_60b56c(1); }  // [0x0060b56c]
};
ProgramSensors g_sensors;

// 0x004d4b10, the animations' callback the registry sets.
void owner_callback(zeff::AnimDef* d, void* rec, int32_t msg) {
  ai::anim_owner_message(handle(d), static_cast<ai::AnimOwnerRec*>(rec), msg);
}

// The animation-owner registry's hooks (zai_anim_owner.h): zEffect's
// animations, each class's messages (vt+0x74) and placement (vt+0xdc).
struct ProgramAnimOwner : ai::AnimOwnerHooks {
  float place_[3] = {};
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return log_unported(key, args);
  }
  static zeff::AnimDef* def(zgame::Handle a) { return static_cast<zeff::AnimDef*>(object(a)); }
  ai::AnimCallback anim_callback(zgame::Handle a) override {
    const zeff::AnimDef* d = def(a);
    if (!d || !d->callback) return ai::AnimCallback::none;
    return d->callback == &owner_callback ? ai::AnimCallback::ours : ai::AnimCallback::other;
  }
  ai::AnimOwnerRec* anim_record(zgame::Handle a) override {
    const zeff::AnimDef* d = def(a);
    return d ? static_cast<ai::AnimOwnerRec*>(d->callback_arg) : nullptr;
  }
  void anim_set_record(zgame::Handle a, ai::AnimOwnerRec* r) override { zeff::anim_set_callback(def(a), &owner_callback, r); }
  // vt+0x74: a target's 0x004d7270 (0 and 0x38), a turret's 0x004a74f0; the
  // mechs', vehicles' and field bases' 0x004d7270 reaches empty slots.
  void entity_message(zgame::Handle o, zgame::Handle a, int32_t msg) override {
    if (tur::Turret* t = program_turret_by_handle(o)) return t->message(a, msg);
    if (ai::CZAiTarget* t = target(o)) t->message(msg);
  }
  void valve_set(zgame::Handle v, int32_t x) override {
    if (auto* valve = static_cast<ai::Valve*>(object(v))) valve->set(x);
  }
  // 0x004c3bd0: the animation's node (0x0050b5e0, none once freed) names the polygons.
  void inout_message(zgame::Handle a, int32_t msg) override {
    const zeff::AnimDef* d = def(a);
    const zcls::Node* n = d && d->raw[0x98] != 5 ? d->node : nullptr;
    ai::inout_set_state(n ? n->name() : nullptr, msg);
  }
  zgame::Handle player_vehicle() override { return g_sensors.player(); }
  zgame::Handle node_named(const char* name) override { return handle(zcls::list_find_by_name(6, name)); }
  void node_translation(zgame::Handle n, float out[3]) override {
    zcls::object3d_get_translation(static_cast<zcls::Node*>(object(n)), &out[0], &out[1], &out[2]);
  }
  void node_rotation(zgame::Handle n, float out[3]) override {
    zcls::object3d_get_rotation(static_cast<zcls::Node*>(object(n)), &out[0], &out[1], &out[2]);
  }
  const float* entity_position(zgame::Handle e) override {
    world_position(g_sensors.node(e, false), place_);
    return place_;
  }
  // vt+0xdc: the Mech's 0x00418f30, a basic vehicle's 0x004a3630. A turret's
  // slot is empty (never the player's vehicle).
  void place(zgame::Handle v, const float pos[3], float yaw) override {
    if (Mech* m = mech(v)) return m->set_place(pos, yaw);
    if (void* b = vehicle(v)) return program_vehicle_place(b, pos, yaw);
    AnimOwnerHooks::place(v, pos, yaw);
  }
  // The binder's: zEffect's defs and nodes, zClass's parents, the valves.
  zgame::Handle anims_next(zgame::Handle a) override { return handle(zeff::anim_next_with_callbacks(def(a))); }
  void anim_callback_values(zgame::Handle a, std::vector<int32_t>& out) override {
    if (zeff::AnimDef* d = def(a))
      zeff::anim_each_callback(d, [](zeff::AnimDef*, void* v, int32_t x) { static_cast<std::vector<int32_t>*>(v)->push_back(x); }, &out);
  }
  zgame::Handle anim_node(zgame::Handle a) override { return handle(zeff::anim_node(def(a))); }
  zgame::Handle node_entity(zgame::Handle n) override { return handle(mech3::node_entity(object(n))); }  // 0x0040eee0
  zgame::Handle node_parent(zgame::Handle n) override {
    const auto* nd = static_cast<const zcls::Node*>(object(n));
    return nd && nd->parent_count() == 1 && nd->parents ? handle(nd->parents[0]) : 0;
  }
  zgame::Handle valve_of_node(zgame::Handle n) override {  // 0x004d3c40 on the node's name
    const auto* nd = static_cast<const zcls::Node*>(object(n));
    return nd ? handle(ai::valve_get(nd->name())) : 0;
  }
};
ProgramAnimOwner g_anim_owner;

// The entity classes' facts (entity_classes.h) on the program's objects:
// the mechs, the basic vehicles and field bases, the turrets, targets.zrd's
// objects and the command marker. No network mechs in the port.
struct ProgramEntityFacts : EntityFacts {
  float place_[3] = {};
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return log_unported(key, args); }
  EntityClass class_of(zgame::Handle o) override {
    if (!o) return EntityClass::none;
    if (mech(o)) return EntityClass::mech;
    if (void* v = vehicle(o)) return program_is_field_base(v) ? EntityClass::field_base : EntityClass::vehicle;
    if (program_turret_by_handle(o)) return EntityClass::turret;
    if (ai::CZAiTarget* t = target(o)) return t->trap ? EntityClass::trap : EntityClass::target;
    if (program_pocket_marker(o)) return EntityClass::marker;
    return EntityClass::none;
  }
  zgame::Handle blip(BlipTable t, int32_t kind) override {
    switch (t) {
      case BlipTable::mech:
        if (!mech_radar_blip(0)) mech_radar_blips_create();  // our addition: made when first needed (0x00413e60's part)
        return handle(mech_radar_blip(kind));
      case BlipTable::vehicle: return handle(program_vehicle_marker(kind));
      case BlipTable::turret: return handle(program_turret_marker(kind));
      case BlipTable::target: return ai::target_marker(kind);
      default: return 0;
    }
  }
  uint32_t team(zgame::Handle o) override {
    const ai::AiTarget* a = g_sensors.fields(o);
    return a ? a->team : 0;
  }
  float height(zgame::Handle o) override {
    const ai::AiTarget* a = g_sensors.fields(o);
    return a ? a->height : 0.0f;
  }
  uint8_t target_e8(zgame::Handle o) override {
    const ai::CZAiTarget* t = target(o);
    return t ? t->desc.flags : 0;
  }
  const char* target_name(zgame::Handle o) override {
    const ai::CZAiTarget* t = target(o);
    return t && t->desc.named ? t->desc.name.c_str() : nullptr;
  }
  const char* pilot_name(zgame::Handle o) override {  // 0x004aee70
    if (Mech* m = mech(o)) return mech_brain_name(m);
    void* v = vehicle(o);
    const auto* b = v ? static_cast<const ai::Brain*>(program_vehicle_brain(program_vehicle_controls(v))) : nullptr;
    return b ? b->params.pilot : nullptr;
  }
  zgame::Handle player_vehicle() override { return g_sensors.player(); }
  const char* player_name() override { return program_pilot_name(); }
  void place(zgame::Handle o, float out[3]) override { world_position(g_sensors.node(o, false), out); }
  void target_centre(zgame::Handle o, float out[3]) override { world_position(g_sensors.node(o, true), out); }
};
ProgramEntityFacts g_entity_facts;

}  // namespace

void wire_sensors(const MechWiring* w, const char* (*message)(void* e, int32_t id)) {
  g_w = w ? *w : MechWiring{};
  g_message = message;
  ai::set_sensor_hooks(w ? &g_sensors : nullptr);
  ai::set_anim_owner_hooks(w ? &g_anim_owner : nullptr);
  set_entity_facts(w ? &g_entity_facts : nullptr);
}

// The HUD's contact objects (app_sensors.h).
void* hud_contact_vehicle(const void* c) { return object(contact_vehicle_of(static_cast<const ai::Contact*>(c)->seen)); }
void* hud_contact_entity(const void* c) { return object(static_cast<const ai::Contact*>(c)->entity); }
float hud_contact_size(const void* c) { return contact_size_of(static_cast<const ai::Contact*>(c)->seen); }
uint32_t hud_object_team(const void* o) { return entity_team(handle(o)); }
void* hud_object_blip(const void* o, int32_t kind) { return object(entity_blip(handle(o), kind)); }
bool hud_object_hidden(const void* o) { return entity_hidden(handle(o)); }
void* hud_object_mech(const void* o) { return object(entity_mech(handle(o))); }
const char* hud_object_name(const void* o) { return o ? entity_name(handle(o)) : nullptr; }
void hud_object_place(const void* o, float out[3]) { g_entity_facts.place(handle(o), out); }
void hud_object_centre(const void* o, float out[3]) { entity_centre(handle(o), out); }
void hud_object_centroid(const void* o, float out[3]) { world_position(g_sensors.node(handle(o), true), out); }
// 0x005a8630: the target node's (+0x10) place; vt+0xc 0x005a82d0: zEntity's
// velocity (+0x30), the basic vehicles' kept by them, the rest still.
void hud_object_aim_place(const void* o, float out[3]) {
  const zgame::Handle h = handle(o);
  zgame::Handle n = 0;
  if (void* v = vehicle(h)) return world_position(static_cast<zcls::Node*>(program_vehicle_target_node(v)), out);
  if (tur::Turret* t = program_turret_by_handle(h)) n = t->target_node;
  else if (ai::CZAiTarget* t = target(h)) n = t->target_node;
  world_position(static_cast<zcls::Node*>(object(n)), out);
}
void* hud_target_marker(int32_t kind) { return object(ai::target_marker(kind)); }
void hud_object_velocity(const void* o, float out[3]) {
  out[0] = out[1] = out[2] = 0.0f;
  if (void* v = vehicle(handle(o))) program_vehicle_velocity(v, out);
}

int32_t sensed_contacts(const void* mech, SensedContact* out, int32_t cap) {
  const Mech* m = static_cast<const Mech*>(mech);
  if (!m) return 0;
  int32_t n = 0;
  for (const ai::Contact* c : m->contacts) {
    if (n < cap) {
      out[n].seen = c->seen;
      out[n].distance = std::sqrt(c->dist2);
      out[n].in_sight = c->flags & 1;
    }
    ++n;
  }
  return n;
}

int32_t selected_contact(const void* mech) {
  const Mech* m = static_cast<const Mech*>(mech);
  if (!m || !m->target) return -1;
  for (size_t i = 0; i < m->contacts.size(); ++i)
    if (m->contacts[i] == m->target) return static_cast<int32_t>(i);
  return -1;
}

}  // namespace mech3
