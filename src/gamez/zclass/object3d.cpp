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

// Mech3.exe 1.2 @ 0x004fb7f0
int object3d_remove_child(Node* parent, Node* child) {
  if (!parent) return zerr::report(0x400, kFile, 0x1be, "Null node pointer."), 5;
  if (!child) return zerr::report(0x400, kFile, 0x1bf, "Null node pointer."), 5;
  if (!parent->data) return zerr::report(0x400, kFile, 0x1c0, "Null class data pointer"), 5;
  return node_unlink(parent, child);
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

namespace {
// The x87 compare's "equal" also holds for NaN (unordered sets C3).
bool same(float a, float b) { return !(a < b || a > b); }

// The setters' common tail, as in object3d_set_translation: changed (+0 bit
// 0), UNK25 down the tree, on list 7 or the expiry pushed, flag 2.
void touch(Node* node, Object3d* d) {
  d->put<uint32_t>(0, d->get<uint32_t>(0) | 1);
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
}

Object3d* data_or_report(Node* node, int line) {
  if (!node) return zerr::report(0x400, kFile, line, "Null node pointer."), nullptr;
  auto* d = static_cast<Object3d*>(node->data);
  if (!d) zerr::report(0x400, kFile, line + 1, "Null class data pointer");
  return d;
}

void drop_matrix(Object3d* d) { d->put<uint32_t>(0, d->get<uint32_t>(0) & ~0x10u); }
void drop_initial(Object3d* d) { d->put<uint32_t>(0, d->get<uint32_t>(0) & ~8u); }
}  // namespace

// Mech3.exe 1.2 @ 0x004fba20
int object3d_set_opacity(Node* node, float v) {
  if (int r = check_class(node, 5, kFile, 0x249)) return r;
  static_cast<Object3d*>(node->data)->put(0x4, v);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fbaa0
int object3d_get_opacity(Node* node, float* out) {
  if (int r = check_class(node, 5, kFile, 0x262)) return r;
  *out = static_cast<Object3d*>(node->data)->get<float>(0x4);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fbb10
int object3d_opacity_used(Node* node, uint32_t* out) {
  if (int r = check_class(node, 5, kFile, 0x280)) return r;
  *out = static_cast<Object3d*>(node->data)->get<uint32_t>(0) >> 1 & 1;
  return 0;
}

// Mech3.exe 1.2 @ 0x004fbb90
int object3d_use_opacity(Node* node, int on) {
  if (int r = check_class(node, 5, kFile, 0x29b)) return r;
  auto* d = static_cast<Object3d*>(node->data);
  d->put<uint32_t>(0, on ? d->get<uint32_t>(0) | 2 : d->get<uint32_t>(0) & ~2u);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fb840 (ECX node, EDX on).
int object3d_use_blend(Node* node, int on) {
  if (int r = check_class(node, 5, kFile, 0x1db)) return r;
  auto* d = static_cast<Object3d*>(node->data);
  d->put<uint32_t>(0, on ? d->get<uint32_t>(0) | 4 : d->get<uint32_t>(0) & ~4u);
  return 0;
}

namespace {
float clamp01(float v) { return v > 1.0f ? 1.0f : !(v >= 0.0f) ? 0.0f : v; }  // NaN to 0, as the x87 compares
}  // namespace

// Mech3.exe 1.2 @ 0x004fb8c0 (ECX node, EDX rgb, stack weight).
int object3d_set_blend(Node* node, const float* rgb, float weight) {
  if (int r = check_class(node, 5, kFile, 0x203)) return r;
  auto* d = static_cast<Object3d*>(node->data);
  d->put(0x14, clamp01(weight));
  if (rgb) {
    for (int i = 0; i < 3; ++i) d->put(8 + 4 * i, rgb[i]);
    for (int i = 0; i < 3; ++i) d->put(8 + 4 * i, clamp01(d->get<float>(8 + 4 * i)));
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x004fbc10. No class check; any scale but (1, 1, 1) drops
// the identity flag.
int object3d_set_scale(Node* node, float x, float y, float z) {
  Object3d* d = data_or_report(node, 0x2db);
  if (!d) return 5;
  drop_matrix(d);
  d->put(0x24, x), d->put(0x28, y), d->put(0x2c, z);
  if (!same(x, 1.0f) || !same(y, 1.0f) || !same(z, 1.0f)) drop_initial(d);
  touch(node, d);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fbd20
int object3d_get_scale(Node* node, float* x, float* y, float* z) {
  Object3d* d = data_or_report(node, 0x378);
  if (!d) return 5;
  *x = d->get<float>(0x24), *y = d->get<float>(0x28), *z = d->get<float>(0x2c);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fbd80. The identity flag is only tested while still set.
int object3d_set_rotation(Node* node, float x, float y, float z) {
  Object3d* d = data_or_report(node, 0x39e);
  if (!d) return 5;
  drop_matrix(d);
  d->put(0x18, x), d->put(0x1c, y), d->put(0x20, z);
  if ((d->get<uint32_t>(0) & 8) && (!same(x, 0.0f) || !same(y, 0.0f) || !same(z, 0.0f))) drop_initial(d);
  touch(node, d);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fbe90
int object3d_get_rotation(Node* node, float* x, float* y, float* z) {
  Object3d* d = data_or_report(node, 0x3f0);
  if (!d) return 5;
  *x = d->get<float>(0x18), *y = d->get<float>(0x1c), *z = d->get<float>(0x20);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fbef0. The identity test is on the sums.
int object3d_add_rotation(Node* node, float x, float y, float z) {
  Object3d* d = data_or_report(node, 0x416);
  if (!d) return 5;
  drop_matrix(d);
  x = x + d->get<float>(0x18), y = y + d->get<float>(0x1c), z = z + d->get<float>(0x20);
  d->put(0x18, x), d->put(0x1c, y), d->put(0x20, z);
  if ((d->get<uint32_t>(0) & 8) && (!same(x, 0.0f) || !same(y, 0.0f) || !same(z, 0.0f))) drop_initial(d);
  touch(node, d);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fc1b0. The given-matrix flag stays.
int object3d_add_translation(Node* node, float x, float y, float z) {
  if (int r = check_class(node, 5, kFile, 0x4c5)) return r;
  auto* d = static_cast<Object3d*>(node->data);
  x = x + d->get<float>(0x54), y = y + d->get<float>(0x58), z = z + d->get<float>(0x5c);
  d->put(0x54, x), d->put(0x58, y), d->put(0x5c, z);
  if ((d->get<uint32_t>(0) & 8) && (!same(x, 0.0f) || !same(y, 0.0f) || !same(z, 0.0f))) drop_initial(d);
  touch(node, d);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fc300. USE_MATRIX set, the identity flag dropped.
int object3d_set_matrix(Node* node, const float* m) {
  if (int r = check_class(node, 5, kFile, 0x502)) return r;
  auto* d = static_cast<Object3d*>(node->data);
  if (m != d->floats(0x30)) std::memcpy(d->raw + 0x30, m, 0x30);
  d->put<uint32_t>(0, (d->get<uint32_t>(0) & ~8u) | 0x11);
  touch(node, d);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fc3f0
float* object3d_matrix(Node* node) {
  if (check_class(node, 5, kFile, 0x545)) return nullptr;
  return static_cast<Object3d*>(node->data)->floats(0x30);
}

}  // namespace zcls
