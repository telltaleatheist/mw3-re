// The vehicle editor's compares for test_aiv_edit.py: AiParams::same_numbers
// (0x004df1b0), same_vlist (0x004d9090 / 0x004d9140) and same_name
// (0x0049d2f0), on records given as the exe's bytes.
#include <cstdint>
#include <cstring>
#include <list>

#include "mech3/zai_params.h"

namespace {
using mech3::ai::AiParams;
using mech3::ai::VListEntry;

template <class T>
T* ptr(uint32_t v) {
  return reinterpret_cast<T*>(static_cast<uintptr_t>(v));
}

// raw: the 0xcc bytes at +0x04; entries: n entries of 0x20 bytes (the exe's
// layout), -1 for no list.
void fill(AiParams& p, const uint8_t* raw, const uint8_t* entries, int n) {
  std::memcpy(p.raw + AiParams::kBegin, raw, AiParams::kEnd - AiParams::kBegin);
  if (n < 0) return;
  p.vlist = new std::list<VListEntry>;
  for (int i = 0; i < n; ++i) {
    const uint8_t* e = entries + 0x20 * i;
    VListEntry v;
    std::memcpy(&v.f00, e + 0x00, 4), std::memcpy(&v.f04, e + 0x04, 4), std::memcpy(&v.f08, e + 0x08, 4);
    std::memcpy(&v.f0c, e + 0x0c, 4), std::memcpy(&v.f10, e + 0x10, 4);
    uint32_t r, valve;
    std::memcpy(&r, e + 0x14, 4), std::memcpy(&valve, e + 0x1c, 4);
    v.region = ptr<mech3::ai::Region>(r), v.valve = ptr<mech3::ai::Valve>(valve);
    v.flags = e[0x18], v.flags2 = e[0x19];
    p.vlist->push_back(v);
  }
}
void unhook(AiParams& p) {
  delete p.vlist, p.vlist = nullptr;  // the entries own nothing here
}
}  // namespace

extern "C" int probe_aiv_same_numbers(const uint8_t* a, const uint8_t* b) {
  AiParams x, y;
  fill(x, a, nullptr, -1), fill(y, b, nullptr, -1);
  return x.same_numbers(y) ? 1 : 0;
}

extern "C" int probe_aiv_same_vlist(const uint8_t* ea, int na, const uint8_t* eb, int nb) {
  static const uint8_t zero[0xcc] = {};
  AiParams x, y;
  fill(x, zero, ea, na), fill(y, zero, eb, nb);
  const int r = x.same_vlist(y) ? 1 : 0;
  unhook(x), unhook(y);
  return r;
}

extern "C" int probe_aiv_same_name(const char* a, const char* b) { return mech3::ai::same_name(a, b) ? 1 : 0; }
