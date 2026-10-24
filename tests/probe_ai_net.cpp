// The AI's nets at run time (mech3/zainet, zainetedit, zainet_region) for
// tests/differential/test_ai_regions.py. Each run reads files from an archive
// the test writes, through the same calls its emulator side makes on the
// original, and writes one byte stream: regions as their fields, objects of
// the registry as (id, kind). zerr reports are logged on both sides.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/znetwork/znet_object.h"
#include "gamez/zreader/zreader.h"
#include "mech3/zai_path.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_region.h"
#include "mech3/zainetedit.h"
#include "mech3/zrdrainet.h"
#include "mech3/zrdraiv.h"
#include "mech3/mech_waynet.h"

namespace {
using namespace mech3::ai;

struct Out {
  std::vector<uint8_t> b;
  void u(uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
  }
  void f(float x) {
    uint32_t v;
    std::memcpy(&v, &x, 4);
    u(v);
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

std::vector<std::string> g_strings;

uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  if (!s) return 0;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}

void log(std::initializer_list<uint32_t> v) {
  g_log.push_back(static_cast<uint32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}

void sink(int level, const char*, int line, const char*) { log({1, uint32_t(level), uint32_t(line)}); }

// As probe_ai.cpp's: every call into another module logged, the same on the
// emulator side (ai_world.py).
void set_hooks() {
  RdrHooks& h = rdr_hooks();
  h = RdrHooks{};
  h.find_loose = [](const char* n) -> const char* {
    log({2, fnv(n)});
    return nullptr;
  };
  h.localize = [](const char* n) -> const char* {
    log({3, fnv(n)});
    g_strings.push_back(std::string("L:") + n);
    return g_strings.back().c_str();
  };
  h.sound_find = [](const char* n) -> void* {
    log({4, fnv(n)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x5000000u | (fnv(n) & 0xffffff)));
  };
  zerr::set_sink(sink);
}

void put_region(Out& o, Region* r) {
  if (!r) return o.u(0);
  o.u(1);
  o.u(uint32_t(r->get_id())), o.s(r->region_name), o.u(uint32_t(r->count())), o.u(uint32_t(r->cursor));
  for (int32_t i = 0; i < r->count(); ++i) {
    const RegionEdge* e = r->edge(i);
    for (float v : e->point) o.f(v);
    for (float v : e->normal) o.f(v);
  }
}

// What kind of object: 1 region, 2 builder, 3 compiled net, 9 other.
uint32_t kind(znet::Object* o) {
  if (dynamic_cast<Region*>(o)) return 1;
  if (dynamic_cast<CZAiNetEdit*>(o)) return 2;
  if (dynamic_cast<CompiledNet*>(o)) return 3;
  return 9;
}

// A valve as its place in the registry, from 1; 0 for none.
uint32_t valve_token(const Valve* v) {
  if (!v) return 0;
  uint32_t i = 1;
  for (const Valve* w : valves()) {
    if (w == v) return i;
    ++i;
  }
  return 0xdeadu;
}

// A step as its slot in the net; -2 the null step, -3 the default behaviour,
// -4 not in the net.
uint32_t slot_token(const CompiledNet& n, const Behaviour* b) {
  if (b == null_step()) return uint32_t(-2);
  if (b == default_behaviour()) return uint32_t(-3);
  const int32_t i = n.index_of(b);
  return i >= 0 ? uint32_t(i) : uint32_t(-4);
}

void put_step(Out& o, const CompiledNet& n, const Behaviour* b) {
  o.u(uint32_t(b->kind));
  if (b == null_step() || b == default_behaviour()) return;
  o.u(b->flags), o.u(uint32_t(b->x08));
  if (auto* s = dynamic_cast<const Step*>(b)) {
    o.u(s->unresolved ? uint32_t(s->next_slot) : slot_token(n, s->next));
    switch (b->kind) {
      case StepKind::Speed: case StepKind::WaitInterval: case StepKind::WaitTime: case StepKind::Jump:
        o.u(static_cast<const ValueStep*>(b)->value);
        break;
      case StepKind::Sound:
        o.u(uint32_t(reinterpret_cast<uintptr_t>(static_cast<const SoundStep*>(b)->sound)));
        break;
      case StepKind::Valve: {
        auto* v = static_cast<const ValveStep*>(b);
        o.u(valve_token(v->valve)), o.u(uint32_t(v->value));
        break;
      }
      case StepKind::Scan: case StepKind::CanLeave: case StepKind::CanLeaveNot: {
        auto* v = static_cast<const ScanStep*>(b);
        o.u(v->count), o.u(uint32_t(v->tests.size()));
        for (auto& t : v->tests) o.u(uint32_t(t.first)), o.u(uint32_t(t.second));
        break;
      }
      case StepKind::Gate: {
        auto* v = static_cast<const GateStep*>(b);
        o.s(v->has_gate ? v->gate.c_str() : nullptr);
        break;
      }
      case StepKind::ValveUnion: {
        auto* v = static_cast<const UnionStep*>(b);
        o.u(uint32_t(v->valves.size()));  // the tag is junk in the exe (ai_world.py)
        for (const ValveCount& c : v->valves) o.u(valve_token(c.valve)), o.u(uint32_t(c.count));
        break;
      }
      case StepKind::ValveAssign: {
        auto* v = static_cast<const AssignStep*>(b);
        o.u(valve_token(v->valve)), o.u(uint32_t(v->value));
        break;
      }
      default: break;
    }
    return;
  }
  auto* nd = dynamic_cast<const NodeStep*>(b);
  if (!nd) return;
  o.s(nd->has_name ? nd->name.c_str() : nullptr);
  for (float v : nd->pos) o.f(v);
  o.u(uint32_t(nd->links.size()));
  for (const Behaviour* l : nd->links) o.u(slot_token(n, l));
  if (auto* m = dynamic_cast<const McbWaitNode*>(b)) o.u(valve_token(m->valve)), o.u(m->b), o.f(m->time);
  if (auto* d = dynamic_cast<const DeployNode*>(b))
    o.u(valve_token(d->valve_a)), o.u(d->a), o.u(valve_token(d->valve_b)), o.u(d->b), o.u(slot_token(n, d->target_step));
}

void put_compiled(Out& o, const CompiledNet& n) {
  o.u(uint32_t(n.get_id())), o.u(n.x18), o.u(uint32_t(n.count()));
  for (int32_t i = 0; i < n.count(); ++i) put_step(o, n, n.step(i));
}

// The k-th node step of a net (in slot order), null past the last.
Behaviour* node_step(const CompiledNet& n, int32_t k) {
  for (int32_t i = 0; i < n.count(); ++i) {
    Behaviour* b = n.step(i);
    if (dynamic_cast<NodeStep*>(b) && k-- == 0) return b;
  }
  return nullptr;
}

CompiledNet* compiled(int32_t id) {
  znet::Object* o = znet::find(id);
  return o ? dynamic_cast<CompiledNet*>(o) : nullptr;
}

void put_way(Out& o, const CompiledNet& n, const PathFinder* f) {
  if (!f) return o.u(0xffffffffu);
  o.u(f->found ? 1 : 0), o.f(f->best_length), o.u(uint32_t(f->best.size()));
  for (const Behaviour* b : f->best) o.u(slot_token(n, b));
}

// One query, 12 words: op, net, then by op
//   1 near:  x y z (floats)
//   2 path:  start, target (k-th node steps), jets, name index, x y z, follows,
//            then a valve (name index, 0 none) set to a value before following
//   3 valve: name index, value
void run_query(Out& o, const int32_t* q, const char* const* names) {
  const int32_t op = q[0];
  auto f = [&](int i) {
    float v;
    std::memcpy(&v, &q[i], 4);
    return v;
  };
  if (op == 3) {
    if (Valve* v = valve_find(names[q[2]])) v->value->value = q[3];
    return;
  }
  CompiledNet* n = compiled(q[1]);
  if (!n) return o.u(0xeeeeeeeeu);
  if (op == 1) {
    const float p[3] = {f(2), f(3), f(4)};
    return o.u(slot_token(*n, nearest_step(*n, p)));
  }
  Behaviour* start = node_step(*n, q[2]);
  Behaviour* target = node_step(*n, q[3]);
  if (!start || !target) return o.u(0xddddddddu);
  const float pos[3] = {f(6), f(7), f(8)};
  Pilot pilot;
  pilot.jump_jets = q[4];
  pilot.name = names[q[5]];
  pilot.pos = pos;
  PathHolder h;
  o.u(h.find(&pilot, n, start, target) ? 1 : 0);
  put_way(o, *n, h.finder);
  if (q[10] > 0)
    if (Valve* v = valve_find(names[q[10]])) v->value->value = q[11];
  Behaviour* at = start;
  for (int32_t k = 0; k < q[9] && at; ++k) {
    at = h.next(at);
    o.u(at ? slot_token(*n, at) : 0xffffffffu);
    put_way(o, *n, h.finder);
  }
}

void put_state(Out& o) {
  o.u(uint32_t(znet::registry().list.size()));
  for (znet::Object* x : znet::registry().list) o.u(uint32_t(x->get_id())), o.u(kind(x));
  o.u(uint32_t(regions().size()));
  for (Region* r : regions()) o.s(r->region_name);
}

void reset() {
  g_log.clear();
  g_strings.clear();
  g_strings.reserve(8192);
  g_out = Out{};
  set_hooks();
  regions_clear();
  znet::clear();
  // What outlives the clear (the regions' builders never do; the waynets,
  // 10000 up, do), deleted without their own unregistering.
  znet::registry().clearing = 1;
  for (znet::Object* x : znet::registry().list) delete x;
  znet::registry().clearing = 0;
  znet::registry().list.clear();
  valves_clear();
  net_files_clear();
}

int finish(uint8_t* out, int cap, uint32_t* logbuf, int logcap, int* nlog) {
  zerr::set_sink(nullptr);
  if (static_cast<int>(g_out.b.size()) > cap || static_cast<int>(g_log.size()) > logcap) return -101;
  std::memcpy(out, g_out.b.data(), g_out.b.size());
  std::memcpy(logbuf, g_log.data(), g_log.size() * 4);
  *nlog = static_cast<int>(g_log.size());
  return static_cast<int>(g_out.b.size());
}
}  // namespace

extern "C" {

// Regions from `archive`: each of `names` looked up in turn (0x004c5c00) and
// dumped, then the registry and the region list; then for each lookup that
// gave a region, its points (pts[first[i]*3 ..], count[i] of them) tested in
// turn (0x004c57d0) with the result and where the test stopped.
int probe_regions_run(const char* archive, const char* const* names, int n, const float* pts, const int32_t* first,
                      const int32_t* counts, uint8_t* out, int cap, uint32_t* logbuf, int logcap, int* nlog) {
  reset();
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -100;
  std::vector<Region*> got;
  for (int i = 0; i < n; ++i) {
    got.push_back(region_find(names[i]));
    put_region(g_out, got.back());
  }
  put_state(g_out);
  for (int i = 0; i < n; ++i) {
    if (!got[size_t(i)]) continue;
    for (int k = 0; k < counts[i]; ++k) {
      const bool in = got[size_t(i)]->inside(pts + 3 * (first[i] + k));
      g_out.u(in ? 1 : 0), g_out.u(uint32_t(got[size_t(i)]->cursor));
    }
  }
  regions_clear();
  put_state(g_out);
  return finish(out, cap, logbuf, logcap, nlog);
}

// Nets from `archive`: 0x00402240 for each of `nets` in turn (a mission asks
// 1..99), then the registry and every compiled net dumped.
int probe_nets_run(const char* archive, const int32_t* nets, int n, uint8_t* out, int cap, uint32_t* logbuf,
                   int logcap, int* nlog) {
  reset();
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -100;
  for (int i = 0; i < n; ++i) net_build(nets[i]);
  put_state(g_out);
  for (znet::Object* x : znet::registry().list)
    if (auto* c = dynamic_cast<CompiledNet*>(x)) put_compiled(g_out, *c);
  g_out.u(static_cast<uint32_t>(valves().size()));
  for (const Valve* v : valves()) g_out.s(v->name);
  return finish(out, cap, logbuf, logcap, nlog);
}

// Nets from `archive` built as probe_nets_run builds them, then `nq`
// queries of 12 words each (run_query) at the 24 bits of play.
int probe_paths_run(const char* archive, const int32_t* nets, int n, const int32_t* queries, int nq,
                    const char* const* names, int nnames, uint8_t* out, int cap, uint32_t* logbuf, int logcap,
                    int* nlog) {
  (void)nnames;
  reset();
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -100;
  for (int i = 0; i < n; ++i) net_build(nets[i]);
  for (int i = 0; i < nq; ++i) run_query(g_out, queries + 12 * i, names);
  return finish(out, cap, logbuf, logcap, nlog);
}

// test_nav_waynet.py: each named version-107 file built by a builder with
// the build flag (0x004bcba0 -> 0x004be690) under id 10000 + i, the waynet
// written (its id, label and elements: a null step as -2, a point as its
// flags, nav line, first link, reached, place, name and label); the next
// run's reset frees it with the registry.
int probe_waynets_run(const char* archive, const char* const* names, int n, uint8_t* out, int cap,
                      uint32_t* logbuf, int logcap, int* nlog) {
  struct Hooks : mech3::WaynetHooks {
    mech3::Handle sound_find(const char* name) override {
      log({4, fnv(name)});
      return reinterpret_cast<mech3::Handle>(static_cast<uintptr_t>(0x5000000u | (fnv(name) & 0xffffff)));
    }
  };
  static Hooks hooks;
  mech3::waynet_icons_count() = 1000;  // the waynets' deletes (the reset's, with the registry) count out, never to 0
  reset();
  mech3::set_waynet_hooks(&hooks);
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -100;
  for (int i = 0; i < n; ++i) {
    auto* edit = new CZAiNetEdit(10000 + i, names[i], 1);
    auto* w = dynamic_cast<mech3::Waynet*>(edit->net);
    delete edit;
    if (!w) {
      g_out.u(0);
      continue;
    }
    g_out.u(uint32_t(w->get_id()));
    g_out.s(w->label.empty() ? nullptr : w->label.c_str());  // the exe's strdup of a missing name: null
    g_out.u(uint32_t(w->count()));
    for (int32_t k = 0; k < w->count(); ++k) {
      znet::Element* e = w->elem(k);
      const auto* p = mech3::as_point(e);
      if (!p) {
        g_out.u(e == null_step() ? 0xfffffffeu : 0xfffffffdu);
        continue;
      }
      g_out.u(0x5c3828u);
      for (uint8_t b : {p->mfb, p->xd, p->xe, p->xf, p->hidden}) g_out.u(b);
      g_out.u(uint32_t(reinterpret_cast<uintptr_t>(p->sound))), g_out.u(uint32_t(p->first_link)), g_out.u(p->reached);
      for (float f : p->pos) g_out.f(f);
      g_out.s(p->has_name ? p->name.c_str() : nullptr);
      g_out.s(p->has_label ? p->label.c_str() : nullptr);
    }
  }
  mech3::set_waynet_hooks(nullptr);
  return finish(out, cap, logbuf, logcap, nlog);
}

}  // extern "C"
