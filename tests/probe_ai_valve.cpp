// valves.zrd at run time (mech3/zrdrvalve, mech3/zai_valve) for
// tests/differential/test_ai_valves.py: the mission start's load (0x004e2120)
// from an archive the test writes, then a script of valve sets and timer
// ticks, then the mission's end. Everything is written as one byte stream
// (valves as their place in the registry, strings as their bytes, objects as
// the original's addresses), and every call out is logged, as the
// emulator side does for the original.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
#include "gamez/znetwork/znet_object.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_region.h"
#include "mech3/zrdraiv.h"
#include "mech3/zrdrvalve.h"

namespace {
using namespace mech3::ai;

uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  if (!s) return 0;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}
uint32_t fb(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

struct Out {
  std::vector<uint8_t> b;
  void u(uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
  }
  void s(const char* p) {
    if (!p) return u(0xffffffffu);
    const size_t n = std::strlen(p);
    u(static_cast<uint32_t>(n));
    b.insert(b.end(), p, p + n);
  }
};

std::vector<uint32_t> g_log;
Out g_out;
void log(std::initializer_list<uint32_t> v) {
  g_log.push_back(static_cast<uint32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}
void sink(int level, const char*, int line, const char*) { log({1, uint32_t(level), uint32_t(line)}); }

// The fake world the test describes: names to objects, each object's answers
// by vtable slot, the brains' words and the targets' masks written.
std::map<uint32_t, uint32_t> g_names;                // fnv(name) -> object (0 none)
std::map<uint32_t, uint32_t> g_entities;             // fnv(name) -> zEntity's object
std::map<uint64_t, uint32_t> g_answers;              // (object << 32 | slot) -> answer
std::map<uint64_t, uint32_t> g_mem;                  // (object << 32 | offset) -> word written
uint8_t g_60b56c = 1;
float g_time = 0;

uint32_t answer(uint32_t obj, uint32_t slot) {
  auto it = g_answers.find(uint64_t(obj) << 32 | slot);
  return it == g_answers.end() ? 0 : it->second;
}
uint32_t vcall(uint32_t obj, uint32_t slot) {
  log({16, slot, obj});
  return answer(obj, slot);
}

struct Hooks : ValveHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg>) override {
    log({99, fnv(key)});
    return 0;
  }
  void report(int32_t level, int32_t line, const char*) override { log({1, uint32_t(level), uint32_t(line)}); }
  Handle sound_find(const char* n) override {
    log({4, fnv(n)});
    return 0x5000000u | (fnv(n) & 0xffffff);
  }
  void sound_play(Handle s) override { log({8, s}); }
  Handle anim_find(const char* n) override {
    log({5, fnv(n)});
    return 0x6000000u | (fnv(n) & 0xffffff);
  }
  void anim_run(Handle a) override { log({9, a}); }
  Handle target_find(const char* n) override {
    log({6, fnv(n)});
    auto it = g_names.find(fnv(n));
    return it == g_names.end() ? 0 : it->second;
  }
  Handle entity_find(const char* n) override {
    log({7, fnv(n)});
    auto it = g_entities.find(fnv(n));
    return it == g_entities.end() ? 0 : it->second;
  }
  Handle as_vehicle(Handle o) override { return vcall(o, 0x2c); }
  Handle vehicle_of(Handle o) override { return vcall(o, 0x40); }
  Handle mech_of(Handle o) override { return vcall(o, 0x50); }
  Handle controls_of(Handle v) override { return vcall(v, 0x140); }
  Handle brain_of(Handle c) override { return vcall(c, 0x54); }
  void destroy(Handle o) override { vcall(o, 0x70); }
  void mech_destroy(Handle m) override { log({10, m, 1}); }
  void mech_power(Handle m, int32_t down) override { log({11, m, uint32_t(down)}); }
  void vehicle_place(Handle v, const float p[3], float yaw) override {
    log({17, v, fb(p[0]), fb(p[1]), fb(p[2]), fb(yaw)});
  }
  Handle brain_order_net(Handle b) override {
    log({15, b});
    return answer(b, 0x1000);
  }
  void controls_set_net(Handle c, Handle net) override { log({18, c, net}); }
  void brain_write(Handle b, uint32_t off, uint32_t v) override { g_mem[uint64_t(b) << 32 | off] = v; }
  void target_masks(Handle o, uint32_t t, uint32_t a, uint32_t e) override {
    g_mem[uint64_t(o) << 32 | 0xc0] = t;
    g_mem[uint64_t(o) << 32 | 0xc4] = a;
    g_mem[uint64_t(o) << 32 | 0xc8] = e;
    g_60b56c = 0;
  }
  void mission_abort() override { log({12}); }
  void mission_complete() override { log({13}); }
  float game_time() override { return g_time; }
};
Hooks g_hooks;

uint32_t valve_token(const Valve* v) {
  if (!v) return 0;
  uint32_t i = 1;
  for (const Valve* w : valves()) {
    if (w == v) return i;
    ++i;
  }
  return 0xdeadu;
}

void put_reaction(Out& o, const Reaction* r, int depth = 0) {
  if (!r) return o.u(0);
  if (const auto* c = const_cast<Reaction*>(r)->as_composite()) {
    o.u(0x5ccd70), o.u(static_cast<uint32_t>(c->items.size()));
    for (const Reaction* x : c->items) put_reaction(o, x, depth + 1);
    return;
  }
  const auto* a = static_cast<const ValveAction*>(r);
  o.u(a->kind);
  switch (a->kind) {
    case ValveAction::Sound:
    case ValveAction::SoundOnce: {
      const auto* s = static_cast<const SoundAction*>(a);
      o.u(s->sound), o.u(uint32_t(s->value));
      break;
    }
    case ValveAction::Anim:
    case ValveAction::AnimOnce: {
      const auto* s = static_cast<const AnimAction*>(a);
      o.u(s->anim), o.u(uint32_t(s->value));
      break;
    }
    case ValveAction::Scan: {
      const auto* s = static_cast<const ScanAction*>(a);
      o.u(s->brain), o.u(s->test), o.u(uint32_t(s->arg)), o.u(uint32_t(s->value));
      break;
    }
    case ValveAction::Mission: {
      const auto* s = static_cast<const MissionAction*>(a);
      o.u(s->fn), o.u(uint32_t(s->value));
      break;
    }
    case ValveAction::Destroy:
    case ValveAction::Shutdown:
    case ValveAction::SetTeam:
    case ValveAction::Attack:
    case ValveAction::Teleport: {
      const auto* s = static_cast<const EntityAction*>(a);
      o.s(s->has_name ? s->name.c_str() : nullptr), o.u(uint32_t(s->value));
      if (a->kind == ValveAction::Shutdown) o.u(static_cast<const ShutdownAction*>(a)->down);
      if (a->kind == ValveAction::SetTeam) o.u(static_cast<const SetTeamAction*>(a)->team);
      if (a->kind == ValveAction::Attack) o.u(static_cast<const AttackAction*>(a)->strategy);
      if (a->kind == ValveAction::Teleport) {
        const auto* t = static_cast<const TeleportAction*>(a);
        for (float p : t->pos) o.u(fb(p));
        o.u(fb(t->yaw));
      }
      break;
    }
    case ValveAction::Update:
      o.u(valve_token(static_cast<const UpdateAction*>(a)->target));
      break;
    case ValveAction::DelayUpdate: {
      const auto* s = static_cast<const DelayUpdateAction*>(a);
      o.u(valve_token(s->target)), o.u(fb(s->delay));
      break;
    }
    case ValveAction::Timer: {
      const auto* s = static_cast<const DelayTimer*>(a);
      o.u(valve_token(s->target)), o.u(fb(s->time)), o.u(uint32_t(s->value));
      break;
    }
  }
}

void put_valves(Out& o) {
  o.u(static_cast<uint32_t>(valves().size()));
  for (const Valve* v : valves()) {
    o.s(v->name);
    if (const auto* c = dynamic_cast<const Condition*>(v->value)) {
      o.u(c->kind), o.u(static_cast<uint32_t>(c->members.size()));
      for (const Valve* m : c->members) o.u(valve_token(m));
    } else {
      o.u(0x5cd558);
    }
    o.u(uint32_t(v->get()));
    put_reaction(o, v->chain);
  }
  put_reaction(o, valve_timers());
}

}  // namespace

extern "C" {

// The world: `n` names (fnv) to target objects, `ne` zEntity names, `na`
// answers (object, slot, answer; slot 0x1000 a brain's order net).
void probe_valves_world(const uint32_t* names, int n, const uint32_t* ents, int ne, const uint32_t* ans, int na) {
  g_names.clear(), g_entities.clear(), g_answers.clear();
  for (int i = 0; i < n; ++i) g_names[names[2 * i]] = names[2 * i + 1];
  for (int i = 0; i < ne; ++i) g_entities[ents[2 * i]] = ents[2 * i + 1];
  for (int i = 0; i < na; ++i) g_answers[uint64_t(ans[3 * i]) << 32 | ans[3 * i + 1]] = ans[3 * i + 2];
}

// valves.zrd from `archive` loaded as the mission starts, then the ops
// (op, a, b): 0 set the valve whose name has fnv a to b; 1 tick at the game
// time b (float bits); 2 the game time b for the next ops; 3 the mission's
// end. The state after the load and after every op, then the brains' and
// targets' words (the `nw` (object, offset) pairs) and [0x0060b56c].
int probe_valves_run(const char* archive, const uint32_t* ops, int nops, const uint32_t* words, int nw, uint8_t* out,
                     int cap, uint32_t* logbuf, int logcap, int* nlog) {
  g_log.clear();
  g_out = Out{};
  g_mem.clear();
  g_60b56c = 1;
  g_time = 0;
  valve_reentries() = 0;
  zerr::set_sink(sink);
  set_valve_hooks(&g_hooks);
  rdr_hooks() = RdrHooks{};
  rdr_hooks().find_loose = [](const char* n) -> const char* {  // the file's loose path (none)
    log({2, fnv(n)});
    return nullptr;
  };
  regions_clear();
  znet::clear();
  delete valve_timers();
  valve_timers() = nullptr;
  valves_clear();
  valve_file_reset();
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -100;
  valves_load();
  put_valves(g_out);
  for (int i = 0; i < nops; ++i) {
    const uint32_t op = ops[3 * i], a = ops[3 * i + 1], b = ops[3 * i + 2];
    float t;
    std::memcpy(&t, &b, 4);
    if (op == 0) {
      for (Valve* v : valves())
        if (fnv(v->name) == a) {
          g_out.u(uint32_t(v->set(static_cast<int32_t>(b))));
          break;
        }
    } else if (op == 1) {
      g_time = t;
      valves_tick(t);
    } else if (op == 2) {
      g_time = t;
    } else if (op == 3) {
      valves_end();
    }
    g_out.u(0x55555555u);
    put_valves(g_out);
  }
  for (int i = 0; i < nw; ++i) {
    auto it = g_mem.find(uint64_t(words[2 * i]) << 32 | words[2 * i + 1]);
    g_out.u(it == g_mem.end() ? 0xcdcdcdcdu : it->second);
  }
  g_out.u(g_60b56c);
  zerr::set_sink(nullptr);
  set_valve_hooks(nullptr);
  if (static_cast<int>(g_out.b.size()) > cap || static_cast<int>(g_log.size()) > logcap) return -101;
  std::memcpy(out, g_out.b.data(), g_out.b.size());
  std::memcpy(logbuf, g_log.data(), g_log.size() * 4);
  *nlog = static_cast<int>(g_log.size());
  return static_cast<int>(g_out.b.size());
}

// How many times the last run's valves fired again while their reactions
// ran (the re-entry guard's skips: none where the original survives).
int probe_valves_reentries() { return valve_reentries(); }

}  // extern "C"
