// Pirate's Moon's firepoint fix-up 0x0046ea40 and aim cone 0x004a6ee0 (test_pm_firepoints.py).
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_build.h"
#include "mech3/mech_parts.h"
#include "mech3/mech_reader.h"

using namespace mech3;

namespace {
// One firepoint in: id, has an owner (bit 0), the owner has a node (bit 1), already fixed (bit 2), a spare word,
// then aim[5] and the node's place[3] as float bits.
constexpr int kIn = 11;
struct Hooks : MechBuildHooks {
  const uint32_t* in = nullptr;
  std::vector<Hip>* owners = nullptr;
  std::vector<int32_t>* log = nullptr;
  static int index(const void* p) { return static_cast<int>(reinterpret_cast<uintptr_t>(p)) - 0x1000; }
  Handle node_find(Handle, const char* name) override { return reinterpret_cast<Handle>(uintptr_t{0x1000} + static_cast<uint8_t>(name[0])); }
  void* node_owner(Handle n) override {
    const int i = index(n);
    return in[kIn * i + 1] & 1 ? &(*owners)[static_cast<size_t>(i)] : nullptr;
  }
  int32_t node_id(Handle n) override { return static_cast<int32_t>(in[kIn * index(n)]); }
  void node_get_translation(Handle n, float* x, float* y, float* z) override {
    const uint32_t* r = in + kIn * index(n) + 8;
    std::memcpy(x, r, 4), std::memcpy(y, r + 1, 4), std::memcpy(z, r + 2, 4);
  }
  void report(int32_t level, int32_t line, const char*) override { log->push_back(level), log->push_back(line); }
};
}  // namespace

extern "C" {
// limits: the torso's min and max pitch, max yaw; each arm's min, max pitch, min, max yaw (float bits).
// out, for each firepoint: part, place[3], aim[5], cone[18], fixed; then the reports (level, line).
int probe_firepoints(const uint32_t* in, int32_t n, const float* limits, uint32_t* out, int cap) {
  std::vector<int32_t> log;
  std::vector<Hip> owners(static_cast<size_t>(n));
  Hooks h;
  h.in = in, h.owners = &owners, h.log = &log;
  MechBuildHooks& keep = mech_build_hooks();
  set_mech_build_hooks(&h);
  Mech m;
  Torso t;
  Arm arms[2];
  m.torso = &t, m.arms[0] = &arms[0], m.arms[1] = &arms[1];
  t.min_pitch = limits[0], t.max_pitch = limits[1], t.max_yaw = limits[2];
  for (int i = 0; i < 2; ++i) {
    arms[i].min_pitch = limits[3 + 4 * i], arms[i].max_pitch = limits[4 + 4 * i];
    arms[i].min_yaw = limits[5 + 4 * i], arms[i].max_yaw = limits[6 + 4 * i];
  }
  alignas(MechDfn) static uint8_t storage[sizeof(MechDfn)];  // no constructor or destructor: they read files
  std::memset(storage, 0, sizeof storage);
  MechDfn& d = *reinterpret_cast<MechDfn*>(storage);
  std::vector<Firepoint*> list;
  std::vector<char> names(static_cast<size_t>(n) * 2);
  for (int i = 0; i < n; ++i) {
    const uint32_t* r = in + kIn * i;
    names[static_cast<size_t>(i) * 2] = static_cast<char>(i);
    auto* f = new Firepoint{&names[static_cast<size_t>(i) * 2], 0.0f, 0.0f, 0};
    std::memcpy(f->aim, r + 3, 20);
    f->fixed = (r[1] & 4) != 0;
    list.push_back(f);
    if (r[1] & 2) owners[static_cast<size_t>(i)].node = reinterpret_cast<Handle>(uintptr_t{0x1000} + static_cast<unsigned>(i));
  }
  d.firepoints = new std::remove_pointer_t<decltype(d.firepoints)>(list.begin(), list.end());
  mech_firepoints(d, m);
  int k = 0;
  for (Firepoint* f : list) {
    out[k++] = static_cast<uint32_t>(f->model_part);
    std::memcpy(&out[k], f->position, 12), k += 3;
    std::memcpy(&out[k], f->aim, 20), k += 5;
    std::memcpy(&out[k], f->cone, 72), k += 18;
    out[k++] = f->fixed;
    delete f;
  }
  for (int32_t v : log) out[k++] = static_cast<uint32_t>(v);
  delete d.firepoints;
  for (Hip& o : owners) o.node = nullptr;
  m.torso = nullptr, m.arms[0] = m.arms[1] = nullptr;
  set_mech_build_hooks(&keep);
  (void)cap;
  return k;
}
}  // extern "C"
