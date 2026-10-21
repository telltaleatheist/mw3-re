#pragma once
#include <cstdint>
#include <cstring>
#include <list>

#include "mech3/zai_valve.h"

// A vehicle's AI settings, as one aiv.zrd record gives them (docs/ai.md, The
// vehicle records). Original file unknown: between zaitarget.cpp and
// zrdrainet.cpp (0x004d8d30..0x004d9b70). What most numbers mean is the AI
// brain's business (not ported yet), so the block is kept as the exe lays it
// out, with the offsets and the file's tags in docs/ai.md.
namespace mech3::ai {

class Region;

// One "vlist" entry, 0x20 bytes, the defaults at 0x005e8da0 (0x004d8d30): a
// trigger that sets its valve each frame its conditions hold (docs/ai.md
// 17.5; the brain's 0x004d8e70 reads the flags' bits 2-7).
struct VListEntry {
  float f00 = 0.25f;           // +0x00  item 3
  float f04 = 0.1f;            // +0x04  item 6
  float f08 = 0.8f;            // +0x08  item 8
  float f0c = 0;               // +0x0c  item 12, squared by AiParams::finish
  float f10 = 3600.0f;         // +0x10  item 14, squared by AiParams::finish
  Region* region = nullptr;    // +0x14  item 10: a region by name (0x004c5c00)
  uint8_t flags = 0;           // +0x18  bit 0 item 15; bits 1-7 items 1, 2, 4, 5, 7, 11, 13 (!= 0)
  uint8_t flags2 = 0;          // +0x19  bit 0 item 9, bit 1 item 17 (!= 0)
  uint8_t pad[2] = {};         // +0x1a
  Valve* valve = nullptr;      // +0x1c  item 16
};

// The settings block (vtable 0x005cd348): 0xcc bytes of numbers and flags at
// +0x04, copied whole from the defaults at 0x005e8dc0, then the orders and
// two names.
class AiParams {
 public:
  static constexpr int kBegin = 0x04, kEnd = 0xd0;

  AiParams();                                 // 0x004d98c0(0x005e8dc0)
  AiParams(const AiParams& o);
  AiParams& operator=(const AiParams& o);     // 0x004d99e0
  virtual ~AiParams();                        // 0x004d9a50

  void reset();                               // 0x004d9ac0: the defaults, no orders or names
  void to_file_units();                       // 0x004d9b20: ranges back to plain, the yaw to degrees
  void finish();                              // 0x004d9b70: ranges squared, the yaw in radians

  template <class T>
  T get(int off) const {
    T v;
    std::memcpy(&v, raw + off, sizeof v);
    return v;
  }
  template <class T>
  void put(int off, T v) {
    std::memcpy(raw + off, &v, sizeof v);
  }
  void put_raw(int off, uint32_t v) { put<uint32_t>(off, v); }
  void set_bit(int off, int bit, bool on) {
    raw[off] = static_cast<uint8_t>((raw[off] & ~(1u << bit)) | (on ? 1u << bit : 0u));
  }

  // The named few (docs/ai.md has them all).
  int32_t kind() const { return get<int32_t>(0x04); }     // the record's first item
  float yaw() const { return get<float>(0x14); }          // radians once finished
  int32_t team() const { return get<int32_t>(0x18); }     // 0x004d5af0's: 1 the player's side, 2 the enemy, 0 none
  int32_t mech() const { return get<int32_t>(0xc4); }     // p12: the mech by name, -1 none
  int32_t skin() const { return get<int32_t>(0xc8); }

  uint8_t raw[kEnd] = {};                     // +0x04..+0xcf used
  std::list<VListEntry>* vlist = nullptr;     // +0xd0  a 0x0c-byte list owner in the exe
  char* pilot = nullptr;                      // +0xd4  localized
  char* sound_tag = nullptr;                  // +0xd8  the radio voice: "dom", "alan", "epona"
};

int32_t team_side(int32_t which);             // 0x004d5af0: 0 -> 1, 1 -> 2, else 0
int32_t team_for_name(const char* name);      // 0x004d5b10: "player" 1, "stealth" or none 0, else 2

}  // namespace mech3::ai
