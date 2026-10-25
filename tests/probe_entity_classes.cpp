// Probes for test_entity_classes.py (docs/ai.md section 15, Radar
// contacts): the entity classes' answers (mech3/entity_classes) on objects
// the test lays out as it lays out the emulator's: their class, fields, the
// blip tables and strings from a memory model; the calls the slots make
// (0x005a85e0, 0x004aee70, 0x004709c0, 0x004ed280) to the shared callback.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

#include "mech3/entity_classes.h"
#include "probe_app.h"

namespace {
using mech3::BlipTable;
using mech3::EntityClass;
using probe_app::call;
using zgame::Handle;

std::map<uint32_t, uint32_t> g_mem;  // dwords by address
std::map<uint32_t, uint8_t> g_mem8;
std::map<uint32_t, std::string> g_strings;
std::map<Handle, EntityClass> g_class;
uint32_t peek(uint32_t a) {
  auto it = g_mem.find(a);
  return it == g_mem.end() ? 0 : it->second;
}
const char* str(uint32_t a) {
  if (!a) return nullptr;
  auto it = g_strings.find(a);
  return it == g_strings.end() ? "?" : it->second.c_str();
}
void floats(uint32_t a, float out[3]) {
  for (int i = 0; i < 3; ++i) out[i] = zgame::from_bits(peek(a + 4 * i));
}

struct Facts : mech3::EntityFacts {
  uint32_t unported(const char* k, std::initializer_list<zgame::Arg> args) override { return call(k, args); }
  EntityClass class_of(Handle o) override {
    auto it = g_class.find(o);
    return it == g_class.end() ? EntityClass::none : it->second;
  }
  Handle blip(BlipTable t, int32_t kind) override {
    static const uint32_t kTables[] = {0, 0x605618, 0x60b120, 0x609a38, 0x60b4f0};
    return peek(kTables[static_cast<int>(t)] + 4 * kind);
  }
  uint32_t team(Handle o) override { return peek(o + 0xc0); }
  float height(Handle o) override { return zgame::from_bits(peek(o + 0x68)); }
  uint8_t target_e8(Handle o) override { return g_mem8[o + 0xe8]; }
  const char* target_name(Handle o) override { return str(peek(o + 0xec)); }
  const char* pilot_name(Handle o) override { return str(call("0x004aee70", {o})); }
  const char* net_name(Handle o) override { return str(call("0x004709c0", {o})); }
  Handle player_vehicle() override { return peek(peek(0x60861c) + 0x14); }
  const char* player_name() override { return str(peek(0x60861c) + 0x18); }
  void place(Handle o, float out[3]) override { floats(call("0x005a85e0", {o}), out); }
  void target_centre(Handle o, float out[3]) override {
    const uint32_t node = peek(o + 0x14), data = peek(node + 0x38);
    if (g_mem8[data] & 0x20) call("0x004ed280", {node, 1});
    floats(data + 0x84, out);
  }
};
Facts g_facts;
}  // namespace

extern "C" {

void probe_ec_reset() {
  g_mem.clear(), g_mem8.clear(), g_strings.clear(), g_class.clear();
  mech3::set_entity_facts(&g_facts);
}
void probe_ec_poke(uint32_t a, uint32_t v) { g_mem[a] = v; }
void probe_ec_poke8(uint32_t a, uint32_t v) { g_mem8[a] = static_cast<uint8_t>(v); }
void probe_ec_string(uint32_t a, const char* s) { g_strings[a] = s; }
void probe_ec_class(uint32_t o, int32_t c) { g_class[o] = static_cast<EntityClass>(c); }
uint32_t probe_ec_blip(uint32_t o, int32_t kind) { return mech3::entity_blip(o, kind); }
uint32_t probe_ec_team(uint32_t o) { return mech3::entity_team(o); }
uint32_t probe_ec_mech(uint32_t o) { return mech3::entity_mech(o); }
int32_t probe_ec_hidden(uint32_t o) { return mech3::entity_hidden(o) ? 1 : 0; }
// The name's text into `out` (cap bytes); -1 none.
int32_t probe_ec_name(uint32_t o, char* out, int32_t cap) {
  const char* n = mech3::entity_name(o);
  if (!n) return -1;
  std::strncpy(out, n, cap - 1);
  out[cap - 1] = 0;
  return 0;
}
void probe_ec_centre(uint32_t o, float* out) { mech3::entity_centre(o, out); }
uint32_t probe_ec_contact_vehicle(uint32_t seen) { return mech3::contact_vehicle_of(seen); }
float probe_ec_contact_size(uint32_t seen) { return mech3::contact_size_of(seen); }

}  // extern "C"
