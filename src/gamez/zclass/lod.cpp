#include "gamez/zclass/lod.h"

#include "gamez/zclass/cls_data.h"

namespace zcls {

// Mech3.exe 1.2 @ 0x004fe030. Unlike the other classes it joins no list of
// its own.
Node* lod_create() {
  Node* node = node_alloc();
  node->put<int32_t>(0x34, 6);
  auto* d = new Lod{};
  node->data = d;
  d->put<uint32_t>(0, 1);
  d->put(8, 1000.0f), d->put(0xc, 1000000.0f);  // 0x447a0000, 0x49742400
  d->put<uint32_t>(0x44, 1);
  return node;
}

// Mech3.exe 1.2 @ 0x004fe0c0
int lod_set_level(Node* node, uint32_t level) {
  static_cast<Lod*>(node->data)->put<uint32_t>(0, level);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fe0d0
int lod_set_range2(Node* node, uint32_t on, float range) {
  auto* d = static_cast<Lod*>(node->data);
  d->put<uint32_t>(0x48, on);
  if (on) d->put(0x4c, range * range);
  return 0;
}

}  // namespace zcls
