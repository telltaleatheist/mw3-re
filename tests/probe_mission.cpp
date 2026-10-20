// Probes for test_mission_load.py (docs/mission-load.md): the mission load
// 0x0048faf0, its texture budget 0x004902a0 and the mission setup 0x00490ab0
// (mech3/mission). Every call out goes to the shared callback; the memory the
// code reads and writes outside mission.cpp is a model the test fills and
// reads back, as the emulator's memory is on the original's side.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "mech3/mission.h"
#include "probe_app.h"

namespace {
using probe_app::call;
using zgame::Handle;

std::map<uint32_t, uint32_t> g_mem;  // dwords by address; bytes are dwords here
uint32_t g_texmem[2] = {};           // [0x0072f38c]'s total and free
int32_t g_rect[4] = {};              // the render rectangle 0x0040d6a0 gives
int32_t g_abort_at = -1;             // set the abort flag after this many calls
int32_t g_calls = 0;
std::string g_strings[8];

uint32_t rec(const char* key, std::initializer_list<zgame::Arg> args) {
  const uint32_t r = call(key, args);
  if (++g_calls == g_abort_at) mech3::missions().abort = 1;
  return r;
}
uint32_t peek(uint32_t a) {
  auto it = g_mem.find(a);
  return it == g_mem.end() ? 0 : it->second;
}

// A function that returns a string: "<tag><answer>", as the test's stub.
const char* str(int slot, const char* tag, const char* key, std::initializer_list<zgame::Arg> args) {
  char b[32];
  std::snprintf(b, sizeof b, "%s%u", tag, rec(key, args));
  g_strings[slot] = b;
  return g_strings[slot].c_str();
}

struct Recording : mech3::MissionHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return rec(key, args); }
  int32_t game_mission_index() override { return static_cast<int32_t>(rec("game.vt+0x54", {})); }
  uint8_t mp_flag() override { return static_cast<uint8_t>(peek(peek(0x60861c) + 0xa8)); }
  uint8_t app_rgb() override { return static_cast<uint8_t>(peek(0x608e41)); }
  uint8_t app_turrets() override { return static_cast<uint8_t>(peek(0x608e3a)); }
  void set_holes(uint8_t on) override { g_mem[0x6061cc] = on; }
  void set_use_archive() override { g_mem[0x5f4244] = 1; }
  void clear_9a17c0() override { g_mem[0x9a17c0] = 0; }
  void set_view(Handle v) override { g_mem[0x5f4c8c] = v; }
  Handle view() override { return peek(0x5f4c8c); }
  void set_608c84(Handle v) override { g_mem[0x608c84] = v; }
  Handle get_608c84() override { return peek(0x608c84); }
  void set_stealth(Handle v) override { g_mem[0x609a2c] = v; }
  Handle stealth() override { return peek(0x609a2c); }
  uint32_t field32(Handle h, uint32_t off) override { return peek(h + off); }
  void set_field32(Handle h, uint32_t off, uint32_t v) override { g_mem[h + off] = v; }
  void set_field8(Handle h, uint32_t off, uint8_t v) override { g_mem[h + off] = v; }
  void render_rect(int32_t out[4]) override {
    rec("0x0040d6a0", {});
    std::memcpy(out, g_rect, sizeof g_rect);
  }
  uint32_t video_texture_memory(int32_t dev, uint32_t* total, uint32_t* free_) override {
    const uint32_t r = rec("[0x0072f38c]", {dev});
    if (r) *total = g_texmem[0], *free_ = g_texmem[1];
    return r;
  }
  const char* video_driver_name() override { return str(0, "drv", "0x005799f0", {}); }
  const char* video_mode_name() override { return str(1, "mode", "0x00579040", {}); }
  const char* version_number() override { return str(2, "ver", "0x004af290", {}); }
  const char* message(int32_t id) override { return str(3, "msg", "0x00576040", {id}); }
  void unload() override {  // 0x00490480 leaves no mission up
    rec("0x00490480", {});
    mech3::missions().current = -1;
  }
};
Recording g_hooks;

}  // namespace

extern "C" {

void probe_mission_reset() {
  g_mem.clear();
  g_calls = 0;
  g_abort_at = -1;
  mech3::missions() = mech3::Missions{};
  mech3::set_mission_hooks(&g_hooks);
}
void probe_mission_poke(uint32_t addr, uint32_t v) { g_mem[addr] = v; }
uint32_t probe_mission_peek(uint32_t addr) { return peek(addr); }
void probe_mission_set(const int32_t* rect, uint32_t total, uint32_t free_, int32_t abort_at, int32_t current) {
  std::memcpy(g_rect, rect, sizeof g_rect);
  g_texmem[0] = total, g_texmem[1] = free_;
  g_abort_at = abort_at;
  mech3::missions().current = current;
}
void probe_mission_stations(uint32_t v) { mech3::missions().stations = v; }
// The fields' values before the run, in probe_mission_fields' order (+0x60
// and +0x70 are set apart).
void probe_mission_init_fields(const uint32_t* in) {
  mech3::Missions& m = mech3::missions();
  m.f55 = static_cast<uint8_t>(in[0]), m.f56 = static_cast<uint8_t>(in[1]), m.f58 = in[2];
  m.loaded = static_cast<uint8_t>(in[3]);
  for (int i = 0; i < 5; ++i) m.objectives[i] = in[6 + i];
  for (int i = 0; i < 4; ++i) m.sounds[i] = in[11 + i];
}

int32_t probe_mission_load_world(int32_t index) { return mech3::missions().load_world(index) ? 1 : 0; }
int32_t probe_mission_setup() { return mech3::missions().setup() ? 1 : 0; }
int32_t probe_mission_budget(uint32_t* mb) { return mech3::missions().texture_budget(mb) ? 1 : 0; }

// The missions object's fields at their offsets: +0x55, +0x56, +0x58, +0x5c,
// +0x60, +0x70, +0x78..+0x88, +0x8c..+0x98.
void probe_mission_fields(uint32_t* out) {
  const mech3::Missions& m = mech3::missions();
  out[0] = m.f55, out[1] = m.f56, out[2] = m.f58, out[3] = m.loaded, out[4] = static_cast<uint32_t>(m.current);
  out[5] = m.abort;
  for (int i = 0; i < 5; ++i) out[6 + i] = m.objectives[i];
  for (int i = 0; i < 4; ++i) out[11 + i] = m.sounds[i];
}

}  // extern "C"
