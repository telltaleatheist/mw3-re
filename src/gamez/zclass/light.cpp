#include "gamez/zclass/light.h"

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\Light.c";
}  // namespace

// Mech3.exe 1.2 @ 0x004fa720. The node's bounds and the light's defaults
// (mech3ax LightMwC), active, on list 9.
Node* light_create() {
  Node* node = node_alloc();
  if (!node) return zerr::report(0x400, kFile, 0xb8, "Null node pointer."), nullptr;
  node->put(0x74, 1.0f), node->put(0x78, 1.0f), node->put(0x7c, -2.0f);
  node->put(0x80, 2.0f), node->put(0x84, 2.0f), node->put(0x88, -1.0f);
  node->set_flags(node->flags() | 0x100);
  node->put<int32_t>(0x34, 9);
  auto* l = new Light{};
  node->data = l;
  l->put(0x84, 0.0f), l->put(0x88, 1.0f), l->put(0x8c, 0.0f);
  l->put(0x60, 0.0f), l->put(0x64, 0.0f), l->put(0x68, 0.0f);
  l->put(0xa4, 1.0f), l->put(0xa8, 1.0f), l->put(0xac, 1.0f);
  l->put(0x9c, 0.0f), l->put(0xa0, 1.0f);
  l->put(0xb4, 32.0f), l->put(0xb8, 64.0f), l->put(0xbc, 1024.0f), l->put(0xc0, 4096.0f);
  l->put(0xc4, 0.03125f);
  l->put<uint32_t>(0xb0, (l->get<uint32_t>(0xb0) & 0xfffffe33u) | 0x233);
  node_set_active(node, 1);
  l->put<int32_t>(0xc8, 0);
  l->nodes = nullptr;
  push_front(kLights, node);
  return node;
}

}  // namespace zcls
