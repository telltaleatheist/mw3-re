// PM's valve builder and actions, for test_pm_ai_valves.py: valves.zrd built record by record,
// then a script of valve sets and ticks, with every call out written as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
#include <string>

#include "gamez/zreader/zreader.h"
#include "mech3/zai_valve.h"
#include "mech3/zrdraiv.h"
#include "mech3/zrdrvalve.h"

namespace {
using namespace mech3::ai;
using zgame::Handle;

std::string g_log;
std::map<std::string, Handle> g_ids;
std::map<std::string, Handle> g_sounds;
std::map<std::string, Handle> g_anims;
std::map<uint64_t, uint32_t> g_mem;
float g_now = 0;

const char* same(const char* s) { return s; }
uint32_t fb(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
void line(const std::string& s) { g_log += s + '\n'; }
std::string hex(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}
Handle named(std::map<std::string, Handle>& m, const char* n, const char* none) {
  if (!n || std::strncmp(n, none, std::strlen(none)) == 0) return 0;
  auto it = m.find(n);
  if (it != m.end()) return it->second;
  const Handle h = static_cast<Handle>(m.size() + 1);
  m[n] = h;
  return h;
}

struct Hooks : ValveHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg>) override {
    line(std::string("unported ") + key);
    return 0;
  }
  void report(int32_t, int32_t l, const char*) override { line("report " + hex(uint32_t(l))); }
  Handle sound_find(const char* n) override { return named(g_sounds, n, "nosound"); }
  void sound_play(Handle s) override { line("sound " + hex(s)); }
  Handle anim_find(const char* n) override { return named(g_anims, n, "noanim"); }
  void anim_run(Handle a) override { line("anim " + hex(a)); }
  Handle target_find(const char* n) override { return named(g_ids, n, "nobody"); }
  Handle entity_find(const char* n) override { return named(g_ids, n, "nobody"); }
  Handle as_vehicle(Handle o) override { return o; }
  Handle vehicle_of(Handle o) override { return o; }
  Handle mech_of(Handle o) override { return o; }
  Handle controls_of(Handle v) override { return v; }
  Handle brain_of(Handle c) override { return c; }
  void destroy(Handle o) override { line("destroy " + hex(o)); }
  void mech_destroy(Handle m) override { line("destroy " + hex(m)); }
  void mech_power(Handle m, int32_t down) override { line("power " + hex(m) + " " + hex(uint32_t(down))); }
  void vehicle_place(Handle v, const float p[3], float yaw) override {
    line("place " + hex(v) + " " + hex(fb(p[0])) + " " + hex(fb(p[1])) + " " + hex(fb(p[2])) + " " + hex(fb(yaw)));
  }
  Handle brain_order_net(Handle b) override { return b; }
  void controls_set_net(Handle c, Handle net) override { line("net " + hex(c) + " " + hex(net)); }
  void brain_write(Handle b, uint32_t off, uint32_t v) override { g_mem[uint64_t(b) << 32 | off] = v; }
  void target_masks(Handle o, uint32_t t, uint32_t a, uint32_t e) override {
    g_mem[uint64_t(o) << 32 | 0xc0] = t;
    g_mem[uint64_t(o) << 32 | 0xc4] = a;
    g_mem[uint64_t(o) << 32 | 0xc8] = e;
  }
  void mission_abort() override { line("abort"); }
  void mission_complete() override { line("complete"); }
  float game_time() override { return g_now; }
};
Hooks g_hooks;

void dump() {
  std::string o = "values";
  for (const Valve* v : valves()) o += " " + std::string(v->name) + "=" + std::to_string(v->value->get());
  line(o);
  o = "words";
  for (const auto& [name, id] : g_ids)
    for (const uint32_t off : {0x20u, 0x28u, 0x2cu, 0x3cu, 0x40u, 0x44u, 0x64u, 0x68u, 0x6cu, 0x70u, 0x98u, 0x9cu, 0xa0u, 0xc0u, 0xc4u, 0xc8u, 0xdcu}) {
      auto it = g_mem.find(uint64_t(id) << 32 | off);
      if (it != g_mem.end() && it->second) o += " " + name + "+" + hex(off) + "=" + hex(it->second);
    }
  line(o);
}
}  // namespace

// The archive is open already (probe_pm_zrdr_load). A script line is "set name value" or "tick seconds".
extern "C" int probe_pm_ai_valve_run(const char* script, char* out, int size) {
  rdr_hooks().localize = same;
  g_log.clear(), g_ids.clear(), g_sounds.clear(), g_anims.clear(), g_mem.clear();
  g_now = 0;
  set_valve_hooks(&g_hooks);
  valves_end();
  zrdr::Node* root = zrdr::load("valves.zrd");
  if (!root || root->type != zrdr::List) {
    set_valve_hooks(nullptr);
    return -1;
  }
  zrdr::Node* top = root->list[0].i > 1 && root->list[1].type == zrdr::List ? root->list[1].list : root->list;
  auto* d = new ValveDef;
  for (int32_t i = 1; i + 1 < top[0].i; i += 2) {
    if (top[i].type != zrdr::String || top[i + 1].type != zrdr::List) continue;
    d->parse(&top[i + 1], top[i].s);
    if (!build_valve(top[i].s, *d)) line("no valve");
  }
  delete d;
  dump();
  std::istringstream in(script);
  std::string op, name;
  while (in >> op) {
    if (op == "set") {
      int32_t v;
      in >> name >> v;
      if (Valve* x = valve_find(name.c_str())) x->set(v);
    } else {
      float t;
      in >> t;
      g_now = t;
      valves_tick(t);
    }
    dump();
  }
  valves_end();
  set_valve_hooks(nullptr);
  rdr_hooks() = RdrHooks{};
  zrdr::free_node(root);
  if (static_cast<int>(g_log.size()) >= size) return -1;
  std::memcpy(out, g_log.c_str(), g_log.size() + 1);
  return static_cast<int>(g_log.size());
}
