// The AI's reader files (mech3/zrdrainet, zrdraiv, zainet_node, zai_valve)
// for tests/differential/test_ai_readers.py. Each run reads files from an
// archive the test writes, through the same calls its emulator side makes on
// the original, and writes what they produced as one byte stream: pointers
// to valves as their place in the registry, sounds as tokens made from the
// name, strings as their bytes. Every call into another module (zerr,
// localize, zSound) is logged on both sides.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
#include "mech3/zai_valve.h"
#include "mech3/zrdrainet.h"
#include "mech3/zrdraiv.h"

namespace {
using namespace mech3::ai;

uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  if (!s) return 0;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}

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
std::vector<std::string> g_strings;
Out g_out;

void log(std::initializer_list<uint32_t> v) {
  g_log.push_back(static_cast<uint32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}

void sink(int level, const char*, int line, const char*) { log({1, uint32_t(level), uint32_t(line)}); }

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

void put_record(Out& o, const NetNodeRec& r) {
  o.u(uint32_t(r.kind)), o.u(uint32_t(r.index));
  for (float p : r.pos) o.f(p);
  o.u(uint32_t(r.speed_on)), o.f(r.speed);
  o.u(uint32_t(r.wait_interval_on)), o.f(r.wait_interval);
  o.u(uint32_t(r.wait_time_on)), o.f(r.wait_time);
  o.u(uint32_t(r.valve_on)), o.u(valve_token(r.valve)), o.u(r.valve_has_value), o.u(uint32_t(r.valve_value));
  o.u(uint32_t(r.canleave[0])), o.u(uint32_t(r.canleave[1]));
  o.u(uint32_t(r.jump_on)), o.f(r.jump);
  for (int i = 0; i < 2; ++i) o.u(uint32_t(r.scan_aim[i]));
  for (int i = 0; i < 2; ++i) o.u(uint32_t(r.scan_hit[i]));
  for (int i = 0; i < 2; ++i) o.u(uint32_t(r.scan_time[i]));
  o.u(r.hidden_set | r.hidden << 8 | r.fixed_set << 16 | uint32_t(r.fixed) << 24);
  o.u(r.autonext_set | r.autonext << 8 | r.visible_set << 16 | uint32_t(r.visible) << 24);
  o.u(r.deploy_set | r.deploy_a << 8 | r.deploy_b << 16), o.u(uint16_t(r.deploy_c));
  o.u(valve_token(r.deploy_valve_a)), o.u(valve_token(r.deploy_valve_b));
  o.u(r.arrange_set), o.f(r.arrange);
  o.u(r.mcbwait_set | r.mcbwait_b << 8), o.f(r.mcbwait_time), o.u(valve_token(r.mcbwait_valve));
  o.u(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(r.sound)));
  o.u(r.backup | r.shunt << 8);
  o.s(r.name), o.s(r.auxname), o.s(r.gate);
  o.u(static_cast<uint32_t>(r.links.size()));
  for (int32_t l : r.links) o.u(uint32_t(l));
  if (!r.valve_union) {
    o.u(0xffffffffu);
  } else {
    o.u(static_cast<uint32_t>(r.valve_union->valves.size()));
    for (const ValveCount& v : r.valve_union->valves) o.u(valve_token(v.valve)), o.u(uint32_t(v.count));
  }
  if (!r.valve_assign)
    o.u(0xffffffffu);
  else
    o.u(valve_token(r.valve_assign->valve)), o.u(uint32_t(r.valve_assign->value));
}

void put_node(Out& o, const NetNodeRec* r) {
  if (!r) return o.u(0);
  o.u(1);
  put_record(o, *r);
}

void put_file(Out& o, NetRef& ref, const char* const* named, int nnamed) {
  NetFile* f = ref.file;
  o.u(f ? 1 : 0);
  if (!f) return;
  const NetHeader& h = f->header;
  o.u(uint32_t(h.net)), o.u(uint32_t(h.version)), o.u(uint32_t(h.has_pos));
  for (float p : h.pos) o.f(p);
  o.s(h.name), o.u(uint32_t(h.first));
  o.u(uint32_t(f->first)), o.u(uint32_t(f->refs)), o.u(f->tree ? 1 : 0);
  const int32_t n = ref.count();
  o.u(uint32_t(n));
  for (int32_t i = -1; i <= n; ++i) {
    const char* key = reinterpret_cast<const char*>(1);
    NetNodeRec* r = ref.node(i, &key);
    o.s(key == reinterpret_cast<const char*>(1) ? "<unset>" : key);
    put_node(o, r);
  }
  for (int i = 0; i < nnamed; ++i) put_node(o, f->node_named(named[i]));
}
}  // namespace

extern "C" {

// Opens `archive`, then for each of the `n` requests (net, name, op) runs one
// step: op 0 takes a reference and dumps the file, 1 drops the last one taken,
// 2 dumps the file of the last reference again, 3 makes `names[i]` the archive
// later loads read. `named` keys are read with
// node_named after each dump. Returns the bytes written to `out`, the log in
// `log` (count in *nlog), or < 0.
int probe_ainet_run(const char* archive, const int32_t* nets, const char* const* names, const int32_t* ops, int n,
                    const char* const* named, int nnamed, uint8_t* out, int cap, uint32_t* logbuf, int logcap,
                    int* nlog) {
  g_log.clear();
  g_strings.clear();
  g_strings.reserve(8192);
  g_out = Out{};
  set_hooks();
  valves_clear();
  net_files_clear();
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -100;
  std::vector<NetRef*> refs;
  for (int i = 0; i < n; ++i) {
    if (ops[i] == 0) {
      refs.push_back(new NetRef(nets[i], names[i]));
      put_file(g_out, *refs.back(), named, nnamed);
    } else if (ops[i] == 1) {
      if (!refs.empty()) delete refs.back(), refs.pop_back();
      g_out.u(0x77777777u);
    } else if (ops[i] == 3) {  // later loads read another archive
      zrdr::close_archives(true);
      if (!zrdr::open_archive(names[i], true)) return -102;
    } else if (!refs.empty()) {
      put_file(g_out, *refs.back(), named, nnamed);
    }
  }
  for (NetRef* r : refs) delete r;
  g_out.u(static_cast<uint32_t>(valves().size()));
  for (const Valve* v : valves()) g_out.s(v->name);
  g_out.u(static_cast<uint32_t>(net_files().size()));
  for (auto& [k, f] : net_files()) g_out.s(k.c_str()), g_out.u(uint32_t(f->refs)), g_out.u(f->tree ? 1 : 0);
  zerr::set_sink(nullptr);
  if (static_cast<int>(g_out.b.size()) > cap || static_cast<int>(g_log.size()) > logcap) return -101;
  std::memcpy(out, g_out.b.data(), g_out.b.size());
  std::memcpy(logbuf, g_log.data(), g_log.size() * 4);
  *nlog = static_cast<int>(g_log.size());
  return static_cast<int>(g_out.b.size());
}

}  // extern "C"
