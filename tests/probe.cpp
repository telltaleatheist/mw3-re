// C entry points over ported functions, loaded by tests/differential/*.py to compare
// them with the original x86 code running in an emulator.
#include <cstdint>

#include "gamez/zrender/zrndr_span.h"

extern "C" {

void probe_span_fill16(uint16_t* dest, uint32_t color, uint32_t count) {
  zrndr::span_state().dest = reinterpret_cast<uint8_t*>(dest);
  zrndr::span_fill16(color, count);
}

void probe_span_textured16(uint16_t* dest, const uint16_t* texture, uint32_t du, uint32_t dv,
                           uint32_t v_mask, uint32_t u, uint32_t v, int32_t count) {
  zrndr::SpanState& s = zrndr::span_state();
  s.dest = reinterpret_cast<uint8_t*>(dest);
  s.texture = texture;
  s.du = du;
  s.dv = dv;
  s.v_mask = v_mask;
  zrndr::span_textured16(u, v, count);
}

}  // extern "C"

#include "gamez/zutil/ztimer.h"
#include "platform/platform.h"

namespace {
struct ProbeClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now; }
};
ProbeClock g_probe_clock;
}  // namespace

extern "C" {

// Runs ztimer::update() for each tick value; writes dt and raw_dt per frame.
void probe_ztimer(const uint32_t* ticks, int frames, float scale_on_frame, int scale_frame,
                  float* dt_out, float* raw_out) {
  ztimer::state() = ztimer::State{};
  ztimer::set_clock(&g_probe_clock);
  for (int i = 0; i < frames; ++i) {
    if (i == scale_frame) ztimer::state().scale = scale_on_frame;
    g_probe_clock.now = ticks[i];
    ztimer::update();
    dt_out[i] = ztimer::state().dt;
    raw_out[i] = ztimer::state().raw_dt;
  }
  ztimer::set_clock(nullptr);
}

}  // extern "C"

#include <cstdio>
#include <cstring>
#include <string>

#include "gamez/zreader/zreader.h"

namespace {
zrdr::Node g_probe_root;
bool g_probe_loaded = false;

void dump(const zrdr::Node& n, std::string& out) {
  char buf[32];
  switch (n.type) {
    case zrdr::Int: std::snprintf(buf, sizeof buf, "I%d ", n.i); out += buf; break;
    case zrdr::Float: {
      uint32_t bits;
      std::memcpy(&bits, &n.f, 4);
      std::snprintf(buf, sizeof buf, "F%08x ", bits);
      out += buf;
      break;
    }
    case zrdr::String:
      std::snprintf(buf, sizeof buf, "S%zu:", std::strlen(n.s));
      out += buf;
      out += n.s;
      out += ' ';
      break;
    case zrdr::List:
      std::snprintf(buf, sizeof buf, "L%d[", n.list[0].i);
      out += buf;
      for (int32_t i = 1; i < n.list[0].i; ++i) dump(n.list[i], out);
      out += "] ";
      break;
    default: out += "? ";
  }
}

// The index path from the root to `target`, e.g. "3/1/2", or "" if not inside.
bool path_to(zrdr::Node* n, const zrdr::Node* target, std::string& out) {
  if (n->type != zrdr::List) return false;
  for (int32_t i = 1; i <= n->list[0].i; ++i) {  // <= : one past the end is a valid answer
    if (&n->list[i] == target) { out = std::to_string(i); return true; }
    if (i < n->list[0].i) {
      std::string sub;
      if (path_to(&n->list[i], target, sub)) { out = std::to_string(i) + "/" + sub; return true; }
    }
  }
  return false;
}

int copy_out(const std::string& s, char* buf, int size) {
  int n = static_cast<int>(s.size());
  if (n >= size) return -1;
  std::memcpy(buf, s.data(), s.size() + 1);
  return n;
}
}  // namespace

extern "C" {

// Reads one node tree from `path` at `offset` with zrdr::read_node; returns bytes read.
int probe_zrdr_load(const char* path, long offset) {
  if (g_probe_loaded) zrdr::free_contents(&g_probe_root);
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  std::fseek(f, offset, SEEK_SET);
  int n = static_cast<int>(zrdr::read_node(f, &g_probe_root));
  std::fclose(f);
  g_probe_loaded = true;
  return n;
}

int probe_zrdr_dump(char* buf, int size) {
  std::string s;
  dump(g_probe_root, s);
  return copy_out(s, buf, size);
}

// zrdr::find, get_float and get_int on the loaded tree. Writes the hit's index path.
int probe_zrdr_find(const char* key, char* buf, int size, int* got_float, float* f, int* got_int, int32_t* i) {
  *got_float = zrdr::get_float(&g_probe_root, key, f);
  *got_int = zrdr::get_int(&g_probe_root, key, i);
  std::string p;
  if (zrdr::Node* hit = zrdr::find(&g_probe_root, key)) path_to(&g_probe_root, hit, p);
  return copy_out(p, buf, size);
}

}  // extern "C"
