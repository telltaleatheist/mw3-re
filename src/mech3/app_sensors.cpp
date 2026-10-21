// The sensors' hooks wired to the program (docs/ai.md section 15): the
// virtual calls the exe makes on vehicles answered as the Mech's and
// CZAiTarget's vtables do (0x005bcef8, 0x005ccb90), their fields read from
// the ports' objects, the nodes' world positions from zClass. Not original
// code. What the mech's slots do beyond the sensors (the HUD's lock 0x0041d890,
// the brain's refresh 0x004aedd0) is logged; the level of detail is mech_lod's.
#include "mech3/app_sensors.h"
#include "mech3/app_hud.h"
#include "mech3/mech_lod.h"

#include <cmath>

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zmath/zmth_matrix.h"
#include "mech3/mech.h"
#include "mech3/mech_control.h"
#include "mech3/sensors.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_target.h"

namespace mech3 {

void player_set_60b56c(uint8_t v);  // mech3/player.h, whose Handle is not mech.h's

namespace {

MechWiring g_w;
const char* (*g_message)(void* e, int32_t id) = nullptr;

uint32_t log_unported(const char* key, std::initializer_list<zgame::Arg> args) {
  return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
}
uint32_t handle(const void* p) { return p && g_w.handle_of ? g_w.handle_of(g_w.engine, p) : 0; }
void* object(uint32_t h) { return h && g_w.object_of ? g_w.object_of(g_w.engine, h) : nullptr; }

// The mechs are the entities on zEntity's list; the targets are targets.zrd's.
Mech* mech(uint32_t h) {
  for (zgame::Handle e : ai::entity_list())
    if (e == h) return static_cast<Mech*>(object(h));
  return nullptr;
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
        case 0xc8:  // 0x0041e810: the brain told (0x004aedd0, logged), then the detail budget
          log_unported("0x004aedd0", {o});
          if (Mech* p = world().mission_player) lod_budget(*p);
          return 0;
        case 0x140: return handle(m->controls);
        case 0x1c: log_unported("0x005a85a0", {o}); return 0;
        default: break;
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
        Mech* m = static_cast<Mech*>(object(e));
        if (m && handle(m->controls) == o) return handle(brain_of(m));
      }
      return 0;
    }
    return SensorHooks::vcall(o, slot, args);
  }
  ai::AiTarget* fields(zgame::Handle x) override {
    if (Mech* m = mech(x)) return m;
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
    else if (ai::CZAiTarget* t = target(e)) t->sensed = on;
  }
  zcls::Node* node(zgame::Handle x, bool centroid) {
    if (Mech* m = mech(x)) return static_cast<zcls::Node*>(centroid ? m->centroid : m->node);
    if (ai::CZAiTarget* t = target(x)) return static_cast<zcls::Node*>(object(centroid ? t->centroid_node : t->node));
    return nullptr;
  }
  void centroid(zgame::Handle x, float out[3]) override { world_position(node(x, true), out); }
  void node_position(zgame::Handle x, float out[3]) override { world_position(node(x, false), out); }
  void entity_refresh(zgame::Handle x) override {
    if (mech(x)) log_unported("0x005a85c0", {x});  // the mech's own update keeps its node
  }
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

}  // namespace

void wire_sensors(const MechWiring* w, const char* (*message)(void* e, int32_t id)) {
  g_w = w ? *w : MechWiring{};
  g_message = message;
  ai::set_sensor_hooks(w ? &g_sensors : nullptr);
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
