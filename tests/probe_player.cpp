// Probes for test_mission_start.py (docs/mission-start.md): the mission
// start's vehicles 0x0049e810 and what it calls in player.cpp, the player's
// tick 0x004a1160 and the launch's lancemates 0x0049e5e0 (mech3/player). Every call out goes to the shared
// callback; the memory the code reads and writes outside player.cpp is a
// model the test fills and reads back, as the emulator's memory is on the
// original's side. The aiv.zrd records are the test's: read from the shipped
// archives by the ported reader (probe_player_load_aiv), then changed or not.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zreader/zreader.h"
#include "mech3/player.h"
#include "mech3/zrdraiv.h"
#include "probe_app.h"

namespace {
using probe_app::call;
using zgame::Handle;
namespace ai = mech3::ai;

constexpr uint32_t kScratch = 0xfeedf00d;  // 0x0049f8a0's own scratch word, as the test names it

std::map<uint32_t, uint32_t> g_mem;     // dwords by address
std::map<uint32_t, uint32_t> g_writes;  // what the port wrote
struct Rec {
  std::string name;
  uint8_t raw[0xd0];
};
std::vector<Rec> g_recs;
uint32_t g_rec_addr = 0;  // the emulator's record object (0x0049e810's stack)

uint32_t peek(uint32_t a) {
  auto it = g_mem.find(a);
  return it == g_mem.end() ? 0 : it->second;
}
void fill(ai::AivRecord& rec, const Rec& r) { std::memcpy(rec.raw + 4, r.raw + 4, sizeof r.raw - 4); }
int32_t vec_count(uint32_t begin_at) {
  const uint32_t b = peek(begin_at);
  return b ? static_cast<int32_t>(peek(begin_at + 4) - b) >> 2 : 0;
}

struct Recording : mech3::PlayerHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  float reader_item_float(Handle n, int32_t i) override { return zgame::from_bits(peek(peek(n + 4) + 8 * i + 4)); }
  Handle reader_item(Handle n, int32_t i) override { return peek(n + 4) + 8 * i; }
  ai::AivRecord* aiv_read_named(ai::AivRecord& rec, const char* name) override {
    if (!call("0x004e0680", {g_rec_addr, name})) return nullptr;
    for (const Rec& r : g_recs)
      if (r.name == name) {
        fill(rec, r);
        return &rec;
      }
    return nullptr;
  }
  ai::AivRecord* aiv_read(ai::AivRecord& rec, int32_t i, const char** name) override {
    if (!call("0x004e0600", {g_rec_addr, i})) return nullptr;
    if (i < 0 || i >= static_cast<int32_t>(g_recs.size())) return nullptr;
    fill(rec, g_recs[i]);
    *name = g_recs[i].name.c_str();
    return &rec;
  }
  int32_t aiv_count() override { return static_cast<int32_t>(call("0x004decf0", {0x60b5e0})); }
  Handle rec_handle(const ai::AivRecord*) override { return g_rec_addr; }
  int32_t dfn_config_count(Handle dfn) override { return vec_count(peek(dfn + 0x20) + 0xc); }
  Handle dfn_config(Handle dfn, int32_t i) override { return peek(peek(peek(dfn + 0x20) + 0xc) + 4 * i); }
  int32_t dfn_skin_count(Handle dfn) override { return vec_count(peek(dfn + 0x1c) + 4); }
  Handle dfn_skin(Handle dfn, int32_t i) override { return peek(peek(peek(dfn + 0x1c) + 4) + 4 * i); }
  void set_field32(Handle h, uint32_t off, uint32_t v) override { g_mem[h + off] = g_writes[h + off] = v; }
  uint32_t field32(Handle h, uint32_t off) override { return peek(h + off); }
  Handle controls_interface(Handle ctl) override { return ctl + peek(peek(ctl + 4) + 4) + 4; }
  void vehicle_anims(Handle area, Handle node, Handle mode, const char* name, const char* type, Handle slot) override {
    call("0x0049ef40", {area, node, mode, name, type, slot ? slot : kScratch});
  }
  float frame_dt() override { return zgame::from_bits(peek(0x72c52c)); }
  Handle pilot_vehicle() override { return peek(peek(0x60861c) + 0x14); }
  Handle view() override { return peek(0x5f4c8c); }
  Handle view_target(Handle v) override { return peek(v + 0x64); }
  Handle stealth() override { return peek(0x609a2c); }
  void clear_6099c0() override { g_mem[0x6099c0] = g_writes[0x6099c0] = 0; }
};
Recording g_hooks;

}  // namespace

extern "C" {

void probe_player_reset() {
  g_mem.clear();
  g_writes.clear();
  g_recs.clear();
  g_rec_addr = 0;
  mech3::player_state() = mech3::PlayerState{};
  zmod::zones().mode = 0;
  mech3::set_player_hooks(&g_hooks);
}
void probe_player_poke(uint32_t addr, uint32_t v) { g_mem[addr] = v; }
void probe_player_set_record_addr(uint32_t a) { g_rec_addr = a; }
void probe_player_add_record(const char* name, const uint8_t* raw) {
  Rec r;
  r.name = name;
  std::memcpy(r.raw, raw, sizeof r.raw);
  g_recs.push_back(r);
}

// The shipped records of `archive`'s aiv.zrd, by the ported reader: the
// count, then each one's name and AiParams bytes +0x00..+0xcf.
int32_t probe_player_load_aiv(const char* archive) {
  g_recs.clear();
  ai::aiv_file_reset();
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -1;
  {
    probe_app::Mute mute;
    ai::AivRecord rec;
    const int32_t n = ai::aiv_file().record_count();
    for (int32_t i = 0; i < n; ++i) {
      const char* name = nullptr;
      if (!rec.read(i, &name) || !name) continue;
      Rec r;
      r.name = name;
      std::memcpy(r.raw, rec.raw, sizeof r.raw);
      g_recs.push_back(r);
    }
  }
  zrdr::close_archives(true);
  ai::aiv_file_reset();
  return static_cast<int32_t>(g_recs.size());
}
int32_t probe_player_get_record(int32_t i, uint8_t* raw, char* name, int32_t cap) {
  if (i < 0 || i >= static_cast<int32_t>(g_recs.size())) return 0;
  std::memcpy(raw, g_recs[i].raw, sizeof g_recs[i].raw);
  std::snprintf(name, static_cast<size_t>(cap), "%s", g_recs[i].name.c_str());
  return 1;
}

void probe_player_setup(int32_t full) { mech3::player_setup(full != 0); }
void probe_player_tick() { mech3::player_tick(); }
int32_t probe_player_launch() { return mech3::launch_lancemates() ? 1 : 0; }

// The writes, as (address, value) pairs; returns how many (up to cap).
int32_t probe_player_writes(uint32_t* out, int32_t cap) {
  int32_t n = 0;
  for (const auto& [a, v] : g_writes) {
    if (n >= cap) break;
    out[2 * n] = a, out[2 * n + 1] = v, ++n;
  }
  return n;
}
// player.cpp's globals: 0x005ea1d0, 0x00609a00, 0x00609a14, 0x00609a0c,
// 0x00609a10, 0x0060b56c, 0x005e8110, 0x005e8114.
void probe_player_globals(uint32_t* out) {
  const mech3::PlayerState& s = mech3::player_state();
  out[0] = static_cast<uint32_t>(zmod::zones().mode), out[1] = s.tick;
  out[2] = zgame::bits(s.gravity), out[3] = zgame::bits(s.water_gravity), out[4] = zgame::bits(s.quicksand_gravity);
  out[5] = s.x60b56c, out[6] = zgame::bits(s.dt), out[7] = zgame::bits(s.inv_dt);
}
void probe_player_set_globals(const uint32_t* in) {
  mech3::PlayerState& s = mech3::player_state();
  zmod::zones().mode = static_cast<int32_t>(in[0]), s.tick = in[1];
  s.gravity = zgame::from_bits(in[2]), s.water_gravity = zgame::from_bits(in[3]);
  s.quicksand_gravity = zgame::from_bits(in[4]), s.x60b56c = static_cast<uint8_t>(in[5]);
  s.dt = zgame::from_bits(in[6]), s.inv_dt = zgame::from_bits(in[7]);
}
}  // extern "C"
