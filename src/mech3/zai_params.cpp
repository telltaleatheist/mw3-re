#include "mech3/zai_params.h"

#include <cmath>
#include <cstdlib>

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
// 0x005e8dc0: the defaults, +0x04..+0xcf, ranges already squared.
const uint32_t kDefaults[0x33] = {
    0,          0,          0,          0,          0,          0,          0x48742400, 0x461c4000, 0x471c4000,
    0x42480000, 0x42280000, 0x43480000, 0x40000000, 0x3f800000, 0,          0,          0x42480000, 0x42480000,
    0x41a00000, 0x3dcccccd, 0,          0x41f00000, 0,          0,          0x40a00000, 0x41700000, 0x41200000,
    0x41a00000, 0x44610000, 0x41700000, 0x42480000, 1,          0x3f800000, 0,          1,          0x3e4ccccd,
    0x3f800000, 1,          1,          1,          0,          0x40800000, 0x40800000, 0x40800000, 0x3f000000,
    0x3f800000, 0,          0x3f000000, 0xffffffff, 0,          0x2c1};
constexpr double kToDegrees = 57.29577951308;    // 0x005cd350
constexpr double kToRadians = 0.01745329251994;  // 0x005cd358
// The ranges kept squared: +0x1c, +0x20, +0x24, +0x74.
constexpr int kRanges[] = {0x1c, 0x20, 0x24, 0x74};

char* dup(const char* s) { return s ? strdup(s) : nullptr; }
}  // namespace

// Mech3.exe 1.2 @ 0x004d98c0, with the defaults.
AiParams::AiParams() { std::memcpy(raw + kBegin, kDefaults, sizeof kDefaults); }

AiParams::AiParams(const AiParams& o) { *this = o; }

// Mech3.exe 1.2 @ 0x004d99e0 (with 0x004d9680 for the orders): an empty list
// copies as none.
AiParams& AiParams::operator=(const AiParams& o) {
  if (this == &o) return *this;
  std::memcpy(raw + kBegin, o.raw + kBegin, kEnd - kBegin);
  if (o.vlist && !o.vlist->empty()) {
    if (!vlist) vlist = new std::list<VListEntry>;
    *vlist = *o.vlist;
  } else {
    delete vlist, vlist = nullptr;
  }
  std::free(pilot), pilot = dup(o.pilot);
  std::free(sound_tag), sound_tag = dup(o.sound_tag);
  return *this;
}

// Mech3.exe 1.2 @ 0x004d9a50
AiParams::~AiParams() {
  delete vlist;
  std::free(pilot);
  std::free(sound_tag);
}

// Mech3.exe 1.2 @ 0x004d9ac0
void AiParams::reset() {
  std::memcpy(raw + kBegin, kDefaults, sizeof kDefaults);
  put<int32_t>(0x18, team_side(1));
  delete vlist, vlist = nullptr;  // 0x004d91e0
  std::free(pilot), pilot = nullptr;
  std::free(sound_tag), sound_tag = nullptr;
}

// Mech3.exe 1.2 @ 0x004d9b20 (the orders' half 0x004d8e20)
void AiParams::to_file_units() {
  for (int off : kRanges) put<float>(off, static_cast<float>(std::sqrt(static_cast<double>(get<float>(off)))));
  put<float>(0x14, static_cast<float>(get<float>(0x14) * kToDegrees));
  if (!vlist) return;
  for (VListEntry& e : *vlist) {
    e.f10 = static_cast<float>(std::sqrt(static_cast<double>(e.f10)));
    e.f0c = static_cast<float>(std::sqrt(static_cast<double>(e.f0c)));
  }
}

// Mech3.exe 1.2 @ 0x004d9b70 (the orders' half 0x004d8e40)
void AiParams::finish() {
  for (int off : kRanges) put<float>(off, get<float>(off) * get<float>(off));
  put<float>(0x14, static_cast<float>(get<float>(0x14) * kToRadians));
  if (!vlist) return;
  for (VListEntry& e : *vlist) {
    e.f10 = e.f10 * e.f10;
    e.f0c = e.f0c * e.f0c;
  }
}

// Mech3.exe 1.2 @ 0x004d5af0
int32_t team_side(int32_t which) { return which == 0 ? 1 : which == 1 ? 2 : 0; }

// Mech3.exe 1.2 @ 0x004d5b10
int32_t team_for_name(const char* name) {
  if (!name || std::strcmp(name, "stealth") == 0) return team_side(2);
  return team_side(std::strcmp(name, "player") == 0 ? 0 : 1);
}

namespace {
// The exe's float ==: FCOMP's unordered result (a NaN on either side) sets the
// same flag as equal, and the compiled test reads only that one, so a NaN
// equals anything (test_aiv_edit.py; docs/editor.md). -0 equals 0.
bool x87_equal(float a, float b) { return !(a < b) && !(a > b); }
}  // namespace

// Mech3.exe 1.2 @ 0x004df1b0. Word i is at +0x04 + 4i; each compared as a
// float (x87_equal) or as an int, as the exe does, and the flag bytes +0xcc /
// +0xcd by their bits. Word 0x0f's place in the order and the skipped ones
// are the exe's.
bool AiParams::same_numbers(const AiParams& o) const {
  const auto f = [&](int w) { return x87_equal(get<float>(4 + 4 * w), o.get<float>(4 + 4 * w)); };
  const auto i = [&](int w) { return get<int32_t>(4 + 4 * w) == o.get<int32_t>(4 + 4 * w); };
  const uint8_t a = raw[0xcc], b = o.raw[0xcc];
  const uint8_t x = a ^ b;
  if (!(i(0) && f(1) && f(2) && f(3) && f(4) && i(5))) return false;                                // 0x004bbd70
  if (!(f(6) && f(7) && f(8) && f(9) && f(10) && f(11) && f(12) && f(13) && i(14))) return false;  // 0x004bbdd0
  if (!(f(0x10) && f(0x11) && f(0x12) && f(0x13) && i(0x0f) && !(x & 7))) return false;             // 0x004bbe60
  if (!(i(0x14) && f(0x15) && i(0x16) && i(0x17))) return false;
  if (!(f(0x18) && f(0x19) && f(0x1a) && f(0x1b) && f(0x1c) && f(0x1d) && f(0x1e) && i(0x1f) && f(0x20) &&
        i(0x21) && i(0x22) && f(0x23) && f(0x24) && i(0x25) && i(0x26) && i(0x27) && i(0x28)))
    return false;                                                                                   // 0x004bbed0
  if (!(f(0x29) && f(0x2a) && f(0x2b) && !(x & 8) && f(0x2c))) return false;                        // 0x004bc010
  if (!(f(0x2d) && i(0x2e))) return false;                                                          // 0x004bc090
  if (!f(0x2f)) return false;
  if (!(i(0x30) && !(x & 0x30))) return false;                                                      // 0x004bc0d0
  if (x & 0x40) return false;
  // 0x004bc110 (ECX the first): bit 1 of +0xcd equal, and bit 0 too unless the first has bit 1.
  const uint8_t y = raw[0xcd] ^ o.raw[0xcd];
  if ((y & 2) || (!(raw[0xcd] & 2) && (y & 1))) return false;
  return i(0x31) && !(x & 0x80);
}

// Mech3.exe 1.2 @ 0x004d9140: one vlist entry against another.
static bool same_entry(const VListEntry& a, const VListEntry& b) {
  const uint8_t x = a.flags ^ b.flags;
  return !(x & 7) && x87_equal(a.f00, b.f00) && !(x & 0x18) && x87_equal(a.f04, b.f04) && !(x & 0x20) &&
         x87_equal(a.f08, b.f08) && !(x & 0x40) && x87_equal(a.f0c, b.f0c) && !(x & 0x80) && x87_equal(a.f10, b.f10) &&
         !((a.flags2 ^ b.flags2) & 3) && a.region == b.region;
}

// Mech3.exe 1.2 @ 0x004d91d0 -> 0x004d9090: the same list object, or both
// there with as many entries, each the same valve and the same settings.
bool AiParams::same_vlist(const AiParams& o) const {
  if (vlist == o.vlist) return true;
  if (!vlist || !o.vlist || vlist->size() != o.vlist->size()) return false;
  auto j = o.vlist->begin();
  for (auto k = vlist->begin(); k != vlist->end(); ++k, ++j)
    if (k->valve != j->valve || !same_entry(*j, *k)) return false;
  return true;
}

// Mech3.exe 1.2 @ 0x0049d2f0
bool same_name(const char* a, const char* b) { return a && b && std::strcmp(a, b) == 0; }

}  // namespace mech3::ai
