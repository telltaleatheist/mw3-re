// Probes for test_sensors.py (docs/ai.md section 15): the sensors
// (mech3/sensors, 0x004a8cd0) and the entity sort (0x004d5ca0). Every
// virtual call goes to the shared callback keyed by the object's name; the
// vehicles' AiTarget fields, their old contacts, nodes, mechs and brains are
// read from a byte memory model the test fills as it fills the emulator's.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mech3/sensors.h"
#include "mech3/zai_target.h"
#include "probe_app.h"

namespace {
using mech3::ai::AiTarget;
using mech3::ai::Contact;
using probe_app::call;
using zgame::Handle;

std::map<uint32_t, uint8_t> g_mem;
uint32_t peek(uint32_t a) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) {
    auto it = g_mem.find(a + i);
    if (it != g_mem.end()) v |= uint32_t{it->second} << (8 * i);
  }
  return v;
}
uint8_t peek8(uint32_t a) {
  auto it = g_mem.find(a);
  return it == g_mem.end() ? 0 : it->second;
}
float peekf(uint32_t a) { return zgame::from_bits(peek(a)); }

std::map<uint32_t, std::string> g_names;    // object handle -> key prefix
std::map<uint32_t, std::string> g_strings;  // address -> string
std::map<uint32_t, std::unique_ptr<AiTarget>> g_vehicles;
std::vector<std::unique_ptr<Contact>> g_arena;  // every contact, never freed
std::map<uint32_t, Contact*> g_contact_of;      // the old ones by address
std::map<const Contact*, uint32_t> g_addr_of;
std::map<uint32_t, uint8_t> g_sensed;           // ZEntity +0x09 writes
std::vector<Handle> g_targets;

std::string key(Handle o, uint32_t slot) {
  auto it = g_names.find(o);
  char b[16];
  std::snprintf(b, sizeof b, ".vt+%#x", slot);
  return (it == g_names.end() ? std::string("?") : it->second) + b;
}
uint32_t vcall(Handle o, uint32_t slot) {
  const std::string k = key(o, slot);
  return call(k.c_str(), {o});
}

Contact* contact_at(uint32_t a) {
  if (!a) return nullptr;
  auto it = g_contact_of.find(a);
  if (it != g_contact_of.end()) return it->second;
  auto c = std::make_unique<Contact>();
  c->owner = peek(a), c->seen = peek(a + 4), c->entity = peek(a + 8);
  c->dist2 = peekf(a + 0xc);
  for (int i = 0; i < 3; ++i) c->delta[i] = peekf(a + 0x10 + 4 * i), c->dir[i] = peekf(a + 0x20 + 4 * i);
  c->dist = peekf(a + 0x1c);
  c->flags = peek8(a + 0x2c);
  Contact* r = c.get();
  g_contact_of[a] = r;
  g_addr_of[r] = a;
  g_arena.push_back(std::move(c));
  return r;
}

struct Targets : mech3::ai::TargetHooks {
  uint32_t unported(const char* k, std::initializer_list<zgame::Arg> args) override { return call(k, args); }
  Handle seen_vehicle(Handle s) override { return vcall(s, 0x2c); }
  Handle ai_target(Handle v) override { return vcall(v, 0x28); }
  uint32_t team_of(Handle t) override {
    auto it = g_vehicles.find(t);
    return it != g_vehicles.end() ? it->second->team : peek(t + 0xc0);
  }
  void contact_delete(Contact*) override {}
};
Targets g_target_hooks;

struct Sensors : mech3::ai::SensorHooks {
  uint32_t unported(const char* k, std::initializer_list<zgame::Arg> args) override { return call(k, args); }
  uint32_t vcall(Handle o, uint32_t slot, std::initializer_list<zgame::Arg>) override { return ::vcall(o, slot); }
  AiTarget* fields(Handle x) override {
    auto it = g_vehicles.find(x);
    return it == g_vehicles.end() ? nullptr : it->second.get();
  }
  uint32_t team(Handle t) override { return g_target_hooks.team_of(t); }
  int32_t equipment(Handle m, int32_t kind) override { return static_cast<int32_t>(peek(m + 0x3ac + 4 * kind)); }
  uint8_t active(Handle t) override { return peek8(t + 8); }
  void set_sensed(Handle e, uint8_t on) override { g_sensed[e] = on; }
  void position(uint32_t node, float out[3]) {
    const uint32_t data = peek(node + 0x38);
    if (peek(data) & 0x20) call("refresh", {node});
    for (int i = 0; i < 3; ++i) out[i] = peekf(data + 0x84 + 4 * i);
  }
  void centroid(Handle x, float out[3]) override { position(peek(x + 0x14), out); }
  void node_position(Handle x, float out[3]) override { position(peek(x + 4), out); }
  void entity_refresh(Handle x) override {
    const uint32_t node = peek(x + 4);
    if (node && peek(node + 0x54)) ::vcall(x, 0x1c);
  }
  void contact_delete(Contact*) override {}
  void set_target(Handle x, Contact* c, bool keep) override {
    const std::string k = key(x, 0xc0);
    call(k.c_str(), {x, c ? 1u : 0u, keep ? 1u : 0u});
    if (AiTarget* a = fields(x)) a->set_target(c, keep);
  }
  void ping(Handle s, const float p[3]) override {
    call("0x004adce0", {s, zgame::bits(p[0]), zgame::bits(p[1]), zgame::bits(p[2])});
  }
  uint8_t brain_lancemate(Handle b) override { return peek8(b + 0xcd); }
  const char* brain_pilot(Handle b) override {
    auto it = g_strings.find(peek(b + 0xd4));
    return it == g_strings.end() ? nullptr : it->second.c_str();
  }
  const char* message(int32_t id) override {
    auto it = g_strings.find(call("0x00576040", {id}));
    return it == g_strings.end() ? nullptr : it->second.c_str();
  }
  Handle player() override { return peek(peek(0x60861c) + 0x14); }
  Handle view_target() override { return peek(peek(0x5f4c8c) + 0x64); }
  Handle stealth() override { return peek(0x609a2c); }
  std::vector<Handle> targets() override { return g_targets; }
  void set_sorted() override { g_mem[0x60b56c] = 1; }
};
Sensors g_hooks;

std::vector<uint32_t> g_out;
void put(uint32_t v) { g_out.push_back(v); }
// A contact: its fields, then its place in the owner's list (-1 none); 0 for none.
void put_contact(const Contact* c, const AiTarget* in) {
  if (!c) return put(0);
  put(1);
  put(c->owner), put(c->seen), put(c->entity), put(zgame::bits(c->dist2));
  for (float d : c->delta) put(zgame::bits(d));
  put(zgame::bits(c->dist)), put(c->flags & 1);
  uint32_t idx = 0xffffffff;
  if (in)
    for (size_t i = 0; i < in->contacts.size(); ++i)
      if (in->contacts[i] == c) idx = static_cast<uint32_t>(i);
  put(idx);
}

}  // namespace

extern "C" {

void probe_sensors_reset() {
  g_mem.clear(), g_names.clear(), g_strings.clear(), g_vehicles.clear();
  g_arena.clear(), g_contact_of.clear(), g_addr_of.clear(), g_sensed.clear();
  g_targets.clear();
  mech3::ai::set_sensor_hooks(&g_hooks);
  mech3::ai::set_target_hooks(&g_target_hooks);
  mech3::ai::entity_list().clear();
  mech3::ai::sorted_allies() = {};
}
void probe_sensors_poke(uint32_t a, uint32_t v) {
  for (int i = 0; i < 4; ++i) g_mem[a + i] = static_cast<uint8_t>(v >> (8 * i));
}
void probe_sensors_poke8(uint32_t a, uint32_t v) { g_mem[a] = static_cast<uint8_t>(v); }
void probe_sensors_name(uint32_t h, const char* name) { g_names[h] = name; }
void probe_sensors_string(uint32_t a, const char* s) { g_strings[a] = s; }
// A vehicle (or target) whose AiTarget fields are at its original offsets in the memory.
void probe_sensors_vehicle(uint32_t x) {
  auto t = std::make_unique<AiTarget>();
  t->height = peekf(x + 0x68);
  t->target = contact_at(peek(x + 0x6c)), t->ally_target = contact_at(peek(x + 0x70));
  t->enemy_target = contact_at(peek(x + 0x74)), t->saved_target = contact_at(peek(x + 0x78));
  t->own_lance = contact_at(peek(x + 0x7c)), t->nearest_lance = contact_at(peek(x + 0x80));
  const uint32_t rb = peek(x + 0x88), re = peek(x + 0x8c);
  for (uint32_t a = rb, i = 0; a && a < re && i < 2; a += 4, ++i) t->tracked[i] = contact_at(peek(a));
  t->own_player = contact_at(peek(x + 0x94)), t->player_contact = contact_at(peek(x + 0x98));
  t->sensor_range = peekf(x + 0x9c), t->range = peekf(x + 0xa0), t->range2 = peekf(x + 0xa4);
  for (uint32_t a = peek(x + 0xac), e = peek(x + 0xb0); a && a < e; a += 4) t->contacts.push_back(contact_at(peek(a)));
  t->touching = contact_at(peek(x + 0xb8));
  t->xbc = peek8(x + 0xbc);
  t->team = peek(x + 0xc0), t->allies = peek(x + 0xc4), t->enemies = peek(x + 0xc8);
  g_vehicles[x] = std::move(t);
}
void probe_sensors_entities(const uint32_t* h, int32_t n) { mech3::ai::entity_list().assign(h, h + n); }
void probe_sensors_targets(const uint32_t* h, int32_t n) { g_targets.assign(h, h + n); }
// fn 0 the sensors, 1 the sort, 2 the allies turning, 3 one lance flag (arg).
void probe_sensors_run(int32_t fn, int32_t see_all, uint32_t arg) {
  mech3::ai::see_all() = see_all != 0;
  switch (fn) {
    case 0: mech3::ai::sensors_update(); break;
    case 1: mech3::ai::entities_sort(); break;
    case 2: mech3::ai::allies_turn(); break;
    case 3: mech3::ai::lance_flag(arg); break;
    default: break;
  }
}
// A vehicle's state: the picks and kept contacts, the flags and masks, the list.
int32_t probe_sensors_state(uint32_t x, uint32_t* out, int32_t cap) {
  g_out.clear();
  const AiTarget* t = g_vehicles.count(x) ? g_vehicles[x].get() : nullptr;
  if (!t) {
    put(0xffffffff);
  } else {
    for (const Contact* c : {t->target, t->ally_target, t->enemy_target, t->saved_target, t->own_lance,
                             t->nearest_lance, t->tracked[0], t->tracked[1], t->own_player, t->player_contact,
                             t->touching})
      put_contact(c, t);
    put(t->xbc), put(t->team), put(t->allies), put(t->enemies);
    put(static_cast<uint32_t>(t->contacts.size()));
    for (const Contact* c : t->contacts) put_contact(c, nullptr);
  }
  const int32_t n = static_cast<int32_t>(g_out.size());
  for (int32_t i = 0; i < n && i < cap; ++i) out[i] = g_out[i];
  return n;
}
// ZEntity +0x09 as written (-1 when not written).
int32_t probe_sensors_sensed(uint32_t e) { return g_sensed.count(e) ? g_sensed[e] : -1; }
// The entity list after a sort, then the allies' range and [0x0060b56c].
int32_t probe_sensors_list(uint32_t* out) {
  const std::vector<Handle>& l = mech3::ai::entity_list();
  size_t i = 0;
  for (; i < l.size(); ++i) out[i] = l[i];
  out[i++] = static_cast<uint32_t>(mech3::ai::sorted_allies().first);
  out[i++] = static_cast<uint32_t>(mech3::ai::sorted_allies().end);
  out[i++] = peek8(0x60b56c);
  return static_cast<int32_t>(i);
}

}  // extern "C"
