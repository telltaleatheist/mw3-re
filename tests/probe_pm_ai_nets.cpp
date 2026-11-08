// PM's nets compiled, for test_pm_ai_nets.py: 0x004022b0 for each net number over a stand-in world's
// ground, then every compiled net as text, valves and sounds by name.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "gamez/zclass/cls_di.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/znetwork/znet_object.h"
#include "mech3/mech_waynet.h"
#include "mech3/veh_basic.h"
#include "mech3/zai_path.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_region.h"
#include "mech3/zainet_inout.h"
#include "mech3/zainetedit.h"
#include "mech3/zrdrainet.h"
#include "mech3/zrdraiv.h"

namespace {
using namespace mech3::ai;

std::vector<std::string> g_log;
int g_grounded = 0, g_water = 0;

const char* same(const char* s) { return s; }
void* sound_named(const char* s) {
  static std::deque<std::string> kept;  // the reader's tree goes with the file
  kept.push_back(s);
  return kept.back().data();
}
void sink(int level, const char*, int line, const char*) {
  char b[64];
  std::snprintf(b, sizeof b, "%x,%x", level, line);
  g_log.push_back(b);
}
uint32_t fb(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
const char* vn(const Valve* v) { return v && v->name ? v->name : "-"; }
const char* sn(const char* s) { return s ? s : "-"; }

// The hits a query at (x, y, z) stands in for, as stand_in_hits in the test.
void hits_at(float x, float y, float z, zcls::GroundHits* out) {
  static zmod::Material water, rock;
  water.soil = 1, rock.soil = 0;
  static const float offs[8] = {-30.0f, -12.0f, -5.0f, 0.0f, 4.0f, 9.5f, 10.0f, 25.0f};
  const uint32_t a = fb(x), b = fb(z);
  const uint32_t k = (a ^ (b >> 3) ^ (b << 7)) * 2654435761u;
  out->count = uint8_t((k >> 29) & 3);
  for (int i = 0; i < out->count; ++i) {
    out->hit[i].height = y + offs[(k >> (2 + 3 * i)) & 7];
    out->hit[i].material = (k >> (20 + i)) & 1 ? &water : &rock;
  }
}

const char* kind_name(StepKind k) {
  switch (k) {
    case StepKind::Speed: return "speed";
    case StepKind::WaitInterval: return "wait_interval";
    case StepKind::WaitTime: return "wait_time";
    case StepKind::Valve: return "valve";
    case StepKind::Scan: return "scan";
    case StepKind::CanLeave: return "canleave";
    case StepKind::CanLeaveNot: return "canleave_not";
    case StepKind::Jump: return "jump";
    case StepKind::Sound: return "sound";
    case StepKind::Gate: return "gate";
    case StepKind::Shunt: return "shunt";
    case StepKind::ValveUnion: return "union";
    case StepKind::ValveAssign: return "assign";
    case StepKind::Node: return "node";
    case StepKind::BackupNode: return "backup";
    case StepKind::McbWaitNode: return "mcbwait";
    case StepKind::DeployNode: return "deploy";
    case StepKind::ViaNode: return "via";
    case StepKind::TerminusNode: return "terminus";
    case StepKind::Null: return "null";
    default: return "?";
  }
}
const char* test_name(ScanTest t) {
  switch (t) {
    case ScanTest::AimOn: return "aim1";
    case ScanTest::AimOff: return "aim0";
    case ScanTest::HitOn: return "hit1";
    case ScanTest::HitOff: return "hit0";
    case ScanTest::TimeOn: return "time1";
    case ScanTest::TimeOff: return "time0";
    case ScanTest::CanLeave: return "leave1";
    case ScanTest::CanLeaveNot: return "leave0";
  }
  return "?";
}

int token(const CompiledNet& n, const Behaviour* b) {
  if (!b) return -5;
  if (b == null_step()) return -2;
  if (b == default_behaviour()) return -3;
  const int i = n.index_of(b);
  return i >= 0 ? i : -4;
}

std::string hex(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x", v);
  return b;
}

void put_step(std::string& o, const CompiledNet& n, const Behaviour* b) {
  o += kind_name(b->kind);
  if (b == null_step()) {
    o += '\n';
    return;
  }
  o += ' ' + std::to_string(b->flags) + ' ' + std::to_string(b->x08);
  if (auto* s = dynamic_cast<const Step*>(b)) {
    o += ' ' + std::to_string(token(n, s->next));
    switch (b->kind) {
      case StepKind::Speed: case StepKind::WaitInterval: case StepKind::WaitTime: case StepKind::Jump:
        o += ' ' + hex(static_cast<const ValueStep*>(b)->value);
        break;
      case StepKind::Sound:
        o += ' ' + std::string(static_cast<const char*>(static_cast<const SoundStep*>(b)->sound));
        break;
      case StepKind::Valve: {
        auto* v = static_cast<const ValveStep*>(b);
        o += ' ' + std::string(vn(v->valve)) + ' ' + std::to_string(v->value);
        break;
      }
      case StepKind::Scan: case StepKind::CanLeave: case StepKind::CanLeaveNot: {
        auto* v = static_cast<const ScanStep*>(b);
        o += ' ' + std::to_string(v->count);
        for (const auto& t : v->tests) o += ' ' + std::string(test_name(t.first)) + ':' + std::to_string(t.second);
        break;
      }
      case StepKind::Gate: {
        auto* v = static_cast<const GateStep*>(b);
        o += ' ' + (v->has_gate ? v->gate : std::string("-"));
        break;
      }
      case StepKind::ValveUnion:
        for (const ValveCount& c : static_cast<const UnionStep*>(b)->valves)
          o += ' ' + std::string(vn(c.valve)) + ':' + std::to_string(c.count);
        break;
      case StepKind::ValveAssign: {
        auto* v = static_cast<const AssignStep*>(b);
        o += ' ' + std::string(vn(v->valve)) + ' ' + std::to_string(v->value);
        break;
      }
      default: break;
    }
  } else if (auto* s = dynamic_cast<const NodeStep*>(b)) {
    o += ' ' + (s->has_name ? s->name : std::string("-"));
    for (float f : s->pos) o += ' ' + hex(fb(f));
    o += " [";
    for (const Behaviour* l : s->links) o += std::to_string(token(n, l)) + ',';
    o += ']';
    if (auto* m = dynamic_cast<const McbWaitNode*>(b)) {
      o += ' ' + std::string(vn(m->valve)) + ' ' + std::to_string(m->b) + ' ' + hex(fb(m->time));
    } else if (auto* d = dynamic_cast<const DeployNode*>(b)) {
      o += ' ' + std::string(vn(d->valve_a)) + ' ' + std::to_string(d->a) + ' ' + vn(d->valve_b) + ' ' +
           std::to_string(d->b) + ' ' + std::to_string(token(n, d->target_step));
    } else if (auto* v = dynamic_cast<const ViaNode*>(b)) {
      o += ' ' + hex(fb(v->radius2)) + ' ' + std::to_string(token(n, v->next_step)) + ' ' + v->via;
    }
  }
  o += '\n';
}
}  // namespace

void ground_hook(float p[3]) {
  zcls::GroundHits hits{};
  hits_at(p[0], p[1] + 10.0f, p[2], &hits);
  if (hits.count == 0) return;
  ++g_grounded;
  p[1] = mech3::net_ground_height(hits, p[1]);
}

// Heights are the ground's through net_ground_height, the shared half of NetHooks::ground.
extern "C" int probe_pm_ai_nets(int with_world, char* out, int size) {
  g_log.clear();
  g_grounded = g_water = 0;
  regions_clear();
  znet::clear();
  znet::registry().clearing = 1;
  for (znet::Object* x : znet::registry().list) delete x;
  znet::registry().clearing = 0;
  znet::registry().list.clear();
  valves_clear();
  net_files_clear();
  rdr_hooks() = RdrHooks{};
  rdr_hooks().localize = same;
  rdr_hooks().sound_find = sound_named;
  zerr::set_sink(sink);
  const NetHooks saved = net_hooks();
  net_hooks() = NetHooks{};
  if (with_world) net_hooks().ground = ground_hook;
  for (int n = 1; n < 100; ++n) net_build(n);
  std::string o;
  for (znet::Object* x : znet::registry().list)
    if (auto* c = dynamic_cast<CompiledNet*>(x)) {
      o += "net " + std::to_string(c->get_id()) + ' ' + std::to_string(c->x18) + ' ' + std::to_string(c->count()) +
           ' ' + std::to_string(int(c->has_vias)) + '\n';
      for (int32_t i = 0; i < c->count(); ++i) put_step(o, *c, c->step(i));
    }
  for (const std::string& l : g_log) o += "report " + l + '\n';
  zerr::set_sink(nullptr);
  net_hooks() = saved;
  rdr_hooks() = RdrHooks{};
  if (static_cast<int>(o.size()) >= size) return -2;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}

// The waynets of the file names given, one a line (0x004c39e0 with the build flag -> 0x004c55b0), under
// ids from 10000, the points as text. Run after probe_pm_ai_nets.
extern "C" int probe_pm_ai_waynets(const char* names, int with_world, char* out, int size) {
  struct Hooks : mech3::WaynetHooks {
    mech3::Handle sound_find(const char* name) override {
      static std::deque<std::string> kept;
      kept.push_back(name);
      return reinterpret_cast<mech3::Handle>(kept.back().data());
    }
  };
  static Hooks hooks;
  mech3::waynet_icons_count() = 1000;
  mech3::set_waynet_hooks(&hooks);
  rdr_hooks().localize = same;
  rdr_hooks().sound_find = sound_named;
  const NetHooks saved = net_hooks();
  net_hooks() = NetHooks{};
  if (with_world) net_hooks().ground = ground_hook;
  g_log.clear();
  zerr::set_sink(sink);
  std::string o, list = names;
  int i = 0;
  for (size_t at = 0; at < list.size();) {
    size_t nl = list.find('\n', at);
    const std::string name = list.substr(at, nl - at);
    at = nl == std::string::npos ? list.size() : nl + 1;
    auto* edit = new CZAiNetEdit(10000 + i++, name.c_str(), 1);
    auto* w = dynamic_cast<mech3::Waynet*>(edit->net);
    delete edit;
    if (!w) {
      o += name + " none\n";
      continue;
    }
    o += name + ' ' + std::to_string(w->get_id()) + ' ' + w->label + ' ' + std::to_string(w->count()) + '\n';
    for (int32_t k = 0; k < w->count(); ++k) {
      znet::Element* e = w->elem(k);
      const auto* p = mech3::as_point(e);
      if (!p) {
        o += e == null_step() ? "null\n" : "other\n";
        continue;
      }
      char b[160];
      std::snprintf(b, sizeof b, "point %d %d %d %d %d %d %s %d %d %x %x %x ", p->mfb, p->static_mfb, p->xd, p->xe, p->xf,
                    p->hidden, p->sound ? reinterpret_cast<const char*>(p->sound) : "-", p->first_link, p->reached,
                    fb(p->pos[0]), fb(p->pos[1]), fb(p->pos[2]));
      o += b;
      o += (p->has_name ? p->name : std::string("-")) + '|' + (p->has_label ? p->label : std::string("-")) + '\n';
    }
  }
  for (const std::string& l : g_log) o += "report " + l + '\n';
  zerr::set_sink(nullptr);
  net_hooks() = saved;
  rdr_hooks() = RdrHooks{};
  mech3::set_waynet_hooks(nullptr);
  if (static_cast<int>(o.size()) >= size) return -2;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}

namespace {
CompiledNet* net_by_id(int32_t id) {
  for (znet::Object* x : znet::registry().list)
    if (auto* c = dynamic_cast<CompiledNet*>(x); c && c->get_id() == id) return c;
  return nullptr;
}
Behaviour* node_at(CompiledNet& n, int k) {
  for (int32_t i = 0; i < n.count(); ++i)
    if (auto* s = dynamic_cast<NodeStep*>(n.step(i)); s && k-- == 0) return s;
  return nullptr;
}
void put_way(std::string& o, CompiledNet& n, const PathFinder* f) {
  if (!f) {
    o += " none";
    return;
  }
  o += ' ' + std::to_string(int(f->found)) + ' ' + hex(fb(f->best_length)) + ' ' + std::to_string(f->best.size());
  for (const Behaviour* b : f->best) o += ' ' + std::to_string(token(n, b));
}
}  // namespace

// Queries on the nets probe_pm_ai_nets built, a line each, floats as words (test_pm_ai_paths.py):
//   N net x y z   V name value   P net a b jets name x y z follows valve value from (a node, -1 the start)
extern "C" int probe_pm_ai_paths(const char* text, char* out, int size) {
  std::string o, t = text;
  size_t at = 0;
  auto word = [&](std::string& w) {
    const size_t e = t.find_first_of(" \n", at);
    w = t.substr(at, e - at);
    at = e == std::string::npos ? t.size() : e + 1;
  };
  auto fl = [&] {
    std::string w;
    word(w);
    const uint32_t u = static_cast<uint32_t>(std::stoul(w, nullptr, 16));
    float f;
    std::memcpy(&f, &u, 4);
    return f;
  };
  auto num = [&] {
    std::string w;
    word(w);
    return std::stoi(w);
  };
  auto set_valve = [](const std::string& name, int v) {
    if (Valve* x = valve_find(name.c_str())) x->value->value = v;
  };
  while (at < t.size()) {
    std::string op;
    word(op);
    if (op == "V") {
      std::string name;
      word(name);
      set_valve(name, num());
      continue;
    }
    CompiledNet* n = net_by_id(num());
    if (op == "N") {
      const float p[3] = {fl(), fl(), fl()};
      o += "near " + std::to_string(token(*n, nearest_step(*n, p))) + '\n';
      continue;
    }
    const int a = num(), b = num();
    Pilot pilot;
    pilot.jump_jets = num();
    std::string name;
    word(name);
    pilot.name = name.c_str();
    const float pos[3] = {fl(), fl(), fl()};
    pilot.pos = pos;
    const int follows = num();
    std::string flip;
    word(flip);
    const int value = num();
    const int from = num();
    Behaviour* start = node_at(*n, a);
    Behaviour* target = node_at(*n, b);
    PathHolder h;
    o += "way " + std::to_string(int(h.find(&pilot, n, start, target)));
    put_way(o, *n, h.finder);
    o += '\n';
    if (flip != "-") set_valve(flip, value);
    Behaviour* now = from >= 0 ? node_at(*n, from) : start;
    for (int k = 0; k < follows && now; ++k) {
      now = h.next(now);
      o += "next " + std::to_string(now ? token(*n, now) : -1);
      put_way(o, *n, h.finder);
      o += '\n';
    }
  }
  if (static_cast<int>(o.size()) >= size) return -2;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}

// Regions (test_pm_ai_regions.py), a line each: F name, then the region's words; T i x y z: the point in
// the i-th found, with where the test stopped; L: the regions' list and the registry.
extern "C" int probe_pm_ai_regions(const char* text, char* out, int size) {
  rdr_hooks().localize = same;
  rdr_hooks().sound_find = sound_named;
  g_log.clear();
  zerr::set_sink(sink);
  std::vector<Region*> got;
  std::string o, t = text;
  size_t at = 0;
  auto word = [&](std::string& w) {
    const size_t e = t.find_first_of(" \n", at);
    w = t.substr(at, e - at);
    at = e == std::string::npos ? t.size() : e + 1;
  };
  while (at < t.size()) {
    std::string op, w;
    word(op);
    if (op == "F") {
      word(w);
      Region* r = region_find(w.c_str());
      got.push_back(r);
      if (!r) {
        o += "none\n";
        continue;
      }
      o += std::to_string(r->get_id()) + ' ' + r->region_name + ' ' + std::to_string(r->count()) + ' ' + std::to_string(r->cursor);
      for (int32_t i = 0; i < r->count(); ++i) {
        const RegionEdge* e = r->edge(i);
        for (float v : e->point) o += ' ' + hex(fb(v));
        for (float v : e->normal) o += ' ' + hex(fb(v));
      }
      o += '\n';
    } else if (op == "T") {
      word(w);
      Region* r = got[size_t(std::stoi(w))];
      float p[3];
      for (float& f : p) {
        word(w);
        const uint32_t u = static_cast<uint32_t>(std::stoul(w, nullptr, 16));
        std::memcpy(&f, &u, 4);
      }
      const bool in = r->inside(p);
      o += std::to_string(int(in)) + ' ' + std::to_string(r->cursor) + '\n';
    } else {
      for (Region* r : regions()) o += std::string("listed ") + r->region_name + '\n';
    }
  }
  for (const std::string& l : g_log) o += "report " + l + '\n';
  zerr::set_sink(nullptr);
  rdr_hooks() = RdrHooks{};
  if (static_cast<int>(o.size()) >= size) return -2;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}

// 0x004c7d60's walk (zainet_inout for_each_via) over the nets probe_pm_ai_nets built: each via node by net and slot,
// once per polygon of the test's two. Run after probe_pm_ai_nets.
extern "C" int probe_pm_ai_vias(char* out, int size) {
  std::string o;
  mech3::ai::for_each_via(
      [](void* ctx, mech3::ai::AiNet* net, const mech3::ai::ViaNode* v) {
        auto& o = *static_cast<std::string*>(ctx);
        auto* c = static_cast<mech3::ai::CompiledNet*>(net);
        int32_t slot = -1;
        for (int32_t i = 0; i < c->count(); ++i)
          if (c->step(i) == v) slot = i;
        for (int poly = 0; poly < 2; ++poly)
          o += "via " + std::to_string(c->get_id()) + ' ' + std::to_string(slot) + ' ' + std::to_string(poly) + '\n';
      },
      &o);
  if (static_cast<int>(o.size()) >= size) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
