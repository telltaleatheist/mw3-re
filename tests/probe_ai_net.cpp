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
#include "mech3/zai_valve.h"
#include "mech3/zainet_region.h"
#include "mech3/zainetedit.h"
#include "mech3/zrdrainet.h"
#include "mech3/zrdraiv.h"

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
  return 9;
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
  for (znet::Object* x : znet::registry().list) delete x;  // the regions' builders never outlive a call
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

}  // extern "C"
