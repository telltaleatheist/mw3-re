#include "gamez/zclass/object3d.h"

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\Object3d.c";
}  // namespace

// Mech3.exe 1.2 @ 0x004fb600. mech3ax UNK25; children that have it are skipped.
void node_mark_unk25(Node* node) {
  if (node->cls() == 5) {
    auto* d = static_cast<Object3d*>(node->data);
    d->put<uint32_t>(0, d->get<uint32_t>(0) | 0x20);
  }
  node->put<uint32_t>(0x2c, node->get<uint32_t>(0x2c) | 1);
  node->set_flags(node->flags() | 0x2000000);
  for (int32_t i = 0; i < node->child_count(); ++i)
    if ((node->children[i]->flags() & 0x2000000) == 0) node_mark_unk25(node->children[i]);
}

// Mech3.exe 1.2 @ 0x004fb650. Identity transform, unit scale, then marked changed.
int object3d_init(Node* node) {
  if (!node) return zerr::report(0x400, kFile, 0x112, "Null node pointer."), 5;
  auto* d = static_cast<Object3d*>(node->data);
  if (!d) return zerr::report(0x400, kFile, 0x113, "Null class data pointer"), 5;
  for (int i = 0; i < 3; ++i) d->put<uint32_t>(0x18 + 4 * i, 0), d->put(0x24 + 4 * i, 1.0f);
  for (int i = 0; i < 12; ++i) d->put<uint32_t>(0x30 + 4 * i, 0);
  d->put(0x30, 1.0f), d->put(0x40, 1.0f), d->put(0x50, 1.0f);
  d->put<uint32_t>(0, (d->get<uint32_t>(0) & ~0x10u) | 9);
  node_mark_unk25(node);
  if ((node->flags() & 1) == 0) {
    push_front(kChanged, node);
    node->set_flags(node->flags() | 1);
  } else if (node->get<uint32_t>(0xc4) <= frames().frame) {
    frames().expiry = frames().frame + 0xa0;
    set_expiry(node);
  }
  node->set_flags(node->flags() | 2);
  if (node->changed) node->changed->removable = 0;
  return 0;
}

// Mech3.exe 1.2 @ 0x004fb740. A failed init leaks the slot (it stays on list 6).
Node* object3d_create() {
  Node* node = node_alloc();
  if (!node) return zerr::report(0x400, kFile, 0x159, "Null node pointer."), nullptr;
  node->put<int32_t>(0x34, 5);
  node->data = new Object3d{};
  return object3d_init(node) == 0 ? node : nullptr;
}

// Mech3.exe 1.2 @ 0x004fb7a0
int object3d_add_child(Node* parent, Node* child) {
  if (!parent) return zerr::report(0x400, kFile, 0x1a2, "Null node pointer."), 5;
  if (!child) return zerr::report(0x400, kFile, 0x1a3, "Null node pointer."), 5;
  if (!parent->data) return zerr::report(0x400, kFile, 0x1a4, "Null class data pointer"), 5;
  return node_link(parent, child);
}

// Mech3.exe 1.2 @ 0x004fc020
int object3d_get_translation(Node* node, float* x, float* y, float* z) {
  if (int r = check_class(node, 5, kFile, 0x461)) return r;
  const auto* d = static_cast<const Object3d*>(node->data);
  *x = d->get<float>(0x54), *y = d->get<float>(0x58), *z = d->get<float>(0x5c);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fc0b0. No class check. A non-zero translation drops
// the identity flag (8); the node is marked changed like object3d_init's tail.
int object3d_set_translation(Node* node, float x, float y, float z) {
  if (!node) return zerr::report(0x400, kFile, 0x488, "Null node pointer."), 5;
  auto* d = static_cast<Object3d*>(node->data);
  if (!d) return zerr::report(0x400, kFile, 0x489, "Null class data pointer"), 5;
  d->put(0x54, x), d->put(0x58, y), d->put(0x5c, z);
  uint32_t f = d->get<uint32_t>(0);
  auto nz = [](float v) { return v < 0.0f || v > 0.0f; };  // x87: NaN compares equal
  if ((f & 8) && (nz(x) || nz(y) || nz(z))) f &= ~8u;
  d->put<uint32_t>(0, f | 1);
  node_mark_unk25(node);
  if ((node->flags() & 1) == 0) {
    push_front(kChanged, node);
    node->set_flags(node->flags() | 1);
  } else if (node->get<uint32_t>(0xc4) <= frames().frame) {
    frames().expiry = frames().frame + 0xa0;
    set_expiry(node);
  }
  node->set_flags(node->flags() | 2);
  if (node->changed) node->changed->removable = 0;
  return 0;
}

}  // namespace zcls
