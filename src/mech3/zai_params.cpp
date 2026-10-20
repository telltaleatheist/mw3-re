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

}  // namespace mech3::ai
