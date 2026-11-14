// The version-105 AI nets for tests/differential/test_net105.py: a block read (0x004025b0) from a serialized reader
// tree, then link operations on its nodes by number; the net, its nodes, its elements and every measure call out.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "mech3/zainet105.h"

using mech3::ai::Net105;
using mech3::ai::Net105Node;

namespace {
std::vector<uint32_t> g_out, g_read;
bool g_reading = false;  // the read's fills come after its dump, as the test's exe side logs them
void u32(uint32_t v) { (g_reading ? g_read : g_out).push_back(v); }
void f32(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  u32(u);
}
// A node as the dump names it: its number, -1 for none at all, -2 for the null node.
uint32_t who(const Net105Node* n) { return !n ? 0xffffffffu : n->is_none() ? 0xfffffffeu : uint32_t(n->number); }

void fill_log(mech3::ai::Net105Link&, const float from[3], const float to[3], float width) {
  u32(0x4023d0);
  for (int i = 0; i < 3; ++i) f32(from[i]);
  for (int i = 0; i < 3; ++i) f32(to[i]);
  f32(width);
}

void dump(const Net105& n) {
  u32(0xd0d0);
  u32(uint32_t(n.x18));
  for (int i = 0; i < 20; i += 4) {
    uint32_t w;
    std::memcpy(&w, n.name_ + i, 4);
    u32(w);
  }
  u32(uint32_t(n.kind));
  f32(n.path_width), f32(n.activate_rad), f32(n.attack_rad), f32(n.attack_dwell), f32(n.not_pursuit_dwell);
  f32(n.pursuit_far), f32(n.pursuit_near), f32(n.return_range);
  for (const Net105Node* k = n.first; k; k = k->next) {
    u32(0xaaaa);
    u32(uint32_t(k->number)), u32(uint32_t(k->x28));
    for (int i = 0; i < 3; ++i) f32(k->place[i]);
    for (int i = 0; i < 3; ++i) u32(who(k->links[i])), u32(k->link_rec[i] ? 1u : 0u);
  }
  u32(0xeeee);
  u32(uint32_t(n.elems.size()));
  for (const znet::Element* e : n.elems) u32(who(static_cast<const Net105Node*>(e)));
}
}  // namespace

extern "C" {
// ops: 4 words each (op, a, b, i); op 1 add_link(a, b), 2 remeasure_all(a), 3 remeasure_to(a, b),
// 4 remeasure(a, b, i), 5 link_pair(a, b, i), 6 set_link(a, b, i), 7 measure(a, b, i). Returns the words written.
int32_t probe_net105(const uint8_t* zrd, int32_t len, int32_t x18, const char* default_name, const int32_t* ops,
                     int32_t nops, uint32_t* out, int32_t cap) {
  g_out.clear();
  std::FILE* f = std::tmpfile();
  std::fwrite(zrd, 1, size_t(len), f);
  std::rewind(f);
  zrdr::Node tree{};
  zrdr::read_node(f, &tree);
  std::fclose(f);
  auto* const fill_was = mech3::ai::net105_link_fill;  // the default (the basic controls' 0x004023d0) put back after
  mech3::ai::net105_link_fill = fill_log;
  g_read.clear();
  g_reading = true;
  Net105* n = mech3::ai::net105_read(&tree, default_name, x18);
  g_reading = false;
  dump(*n);
  g_out.insert(g_out.end(), g_read.begin(), g_read.end());
  std::map<int32_t, Net105Node*> by;
  for (Net105Node* k = n->first; k; k = k->next) by[k->number] = k;
  for (int32_t o = 0; o < nops; ++o) {
    const int32_t* p = ops + 4 * o;
    Net105Node* a = by.count(p[1]) ? by[p[1]] : nullptr;
    Net105Node* b = by.count(p[2]) ? by[p[2]] : nullptr;
    if (!a || !b) continue;
    u32(0x0b0b), u32(uint32_t(p[0]));
    switch (p[0]) {
      case 1: u32(a->add_link(b) ? 1 : 0); break;
      case 2: a->remeasure_all(); break;
      case 3: a->remeasure_to(b); break;
      case 4: u32(a->remeasure(b, p[3]) ? 1 : 0); break;
      case 5: u32(a->link_pair(b, p[3]) ? 1 : 0); break;
      case 6: a->set_link(b, p[3]); break;
      case 7: a->measure(b, p[3]); break;
      default: break;
    }
  }
  dump(*n);
  mech3::ai::net105_link_fill = fill_was;
  mech3::ai::nets105_free();
  zrdr::free_contents(&tree);
  const int32_t k = int32_t(g_out.size());
  if (k <= cap) std::memcpy(out, g_out.data(), size_t(k) * 4);
  return k;
}
}
