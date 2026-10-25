// Probes for test_entity_bind.py (docs/vehicles.md, The node binding): the
// bind 0x0040eaa0 and the unbind 0x0040ed00 over a node tree the test lays
// out. Objects are the emulator's addresses; what each answers to the casts
// comes in a table, as its vtable answers there.
#include <cstdint>
#include <map>
#include <vector>

#include "gamez/zclass/cls_node.h"
#include "mech3/entity_bind.h"

namespace {
void* h(uint32_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); }
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
// Each object: +0x04 (a part's mech, 0x00460000), then its answers at vt+0x40,
// +0x48, +0x50, +0x60, +0xdc, +0xe4, +0xec.
std::map<uint32_t, std::vector<uint32_t>> g_obj;
uint32_t ans(uint32_t o, int slot) {
  auto it = g_obj.find(o);
  return it == g_obj.end() ? 0 : it->second[static_cast<size_t>(slot)];
}
enum { kMech4, k40, k48, k50, k60, kDc, kE4, kEc };
}  // namespace

extern "C" {

// nodes: n x {id, owner, flags, parent} (parent -1 for the root, the first;
// children in order); objects: m x 8 words {address, +4, vt+0x40, +0x48,
// +0x50, +0x60, +0xdc, +0xe4, +0xec} (9). out: per node {id, owner, flags}.
void probe_entity_bind(int32_t op, const uint32_t* nodes, int32_t n, const uint32_t* objects, int32_t m, uint32_t self,
                       uint32_t* out) {
  g_obj.clear();
  for (int32_t i = 0; i < m; ++i) g_obj[objects[9 * i]] = std::vector<uint32_t>(objects + 9 * i + 1, objects + 9 * i + 9);
  std::vector<zcls::Node> tree(static_cast<size_t>(n));
  std::vector<std::vector<zcls::Node*>> kids(static_cast<size_t>(n));
  for (int32_t i = 0; i < n; ++i) {
    zcls::Node& d = tree[static_cast<size_t>(i)];
    d.put<uint32_t>(0x28, nodes[4 * i]);
    d.owner = h(nodes[4 * i + 1]);
    d.put<uint32_t>(0x24, nodes[4 * i + 2]);
    const int32_t parent = static_cast<int32_t>(nodes[4 * i + 3]);
    if (parent >= 0) kids[static_cast<size_t>(parent)].push_back(&d);
  }
  for (int32_t i = 0; i < n; ++i) {
    zcls::Node& d = tree[static_cast<size_t>(i)];
    d.put<int32_t>(0x5c, static_cast<int32_t>(kids[static_cast<size_t>(i)].size()));
    d.children = kids[static_cast<size_t>(i)].empty() ? nullptr : kids[static_cast<size_t>(i)].data();
  }
  if (op == 0) {
    mech3::EntityCasts c;
    c.turret = h(ans(self, k48));
    c.vehicle_base = h(ans(self, k40));
    if (c.vehicle_base) c.mech = h(ans(u(c.vehicle_base), k50)), c.vehicle = h(ans(u(c.vehicle_base), kEc));
    c.target_base = h(ans(self, k60));
    if (c.target_base) c.trap = h(ans(u(c.target_base), kE4)), c.target = h(ans(u(c.target_base), kDc));
    mech3::entity_bind(&tree[0], h(self), c);
  } else {
    mech3::UnbindCalls calls;
    calls.part_mech = [](zcls::Node* nd) { return nd->owner ? h(ans(u(nd->owner), kMech4)) : nullptr; };
    calls.trap_of = [](void* o) { return h(ans(u(o), kE4)); };
    calls.target_of = [](void* o) { return h(ans(u(o), kDc)); };
    mech3::entity_unbind(&tree[0], h(self), calls);
  }
  for (int32_t i = 0; i < n; ++i) {
    const zcls::Node& d = tree[static_cast<size_t>(i)];
    out[3 * i] = d.get<uint32_t>(0x28);
    out[3 * i + 1] = u(d.owner);
    out[3 * i + 2] = d.get<uint32_t>(0x24);
  }
  for (zcls::Node& d : tree) d.children = nullptr;
}

}  // extern "C"
