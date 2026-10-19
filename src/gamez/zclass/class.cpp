#include "gamez/zclass/class.h"

#include <cstdio>
#include <cstring>

#pragma STDC FP_CONTRACT OFF

#include "gamez/zclass/cls_data.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmath/zmth_matrix.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\Class.c";
UpdateHooks g_hooks;

const float kOnes[3] = {1.0f, 1.0f, 1.0f};  // 0x005e9e20
const float kZeros[3] = {};                 // 0x0060b940

zmth::Mat34* mat(void* p) { return static_cast<zmth::Mat34*>(p); }

// 0x004ebf72..: local * top. The x87 sums the 3x3 terms as (0 + 2) + 1 and
// the translation in order.
void product(const float* l, const float* t, float* r) {
  for (int row = 0; row < 3; ++row) {
    const float* a = l + 3 * row;
    for (int c = 0; c < 3; ++c) {
      const float p0 = a[0] * t[c], p1 = a[1] * t[3 + c], p2 = a[2] * t[6 + c];
      r[3 * row + c] = (p0 + p2) + p1;
    }
  }
  for (int c = 0; c < 3; ++c) {
    const float p0 = l[9] * t[c], p1 = l[10] * t[3 + c], p2 = l[11] * t[6 + c];
    r[9 + c] = ((p0 + p1) + p2) + t[9 + c];
  }
}

// 0x00522ab0 (zmth, not ported there): the box's corners through m, in the
// order of the identity case below; each is (x r0 + y r1) + (z r2 + t).
void box_through(const float* m, const float* box, float out[8][3]) {
  static const int kPick[8][3] = {{0, 1, 5}, {3, 1, 5}, {3, 1, 2}, {0, 1, 2},
                                  {0, 4, 5}, {3, 4, 5}, {3, 4, 2}, {0, 4, 2}};
  for (int i = 0; i < 8; ++i) {
    const float x = box[kPick[i][0]], y = box[kPick[i][1]], z = box[kPick[i][2]];
    for (int c = 0; c < 3; ++c) {
      const float xy = x * m[c] + y * m[3 + c];
      const float zt = z * m[6 + c] + m[9 + c];
      out[i][c] = xy + zt;
    }
  }
}
}  // namespace

UpdateHooks& update_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x004ed280 (gwNodeBuildNodeToAncestorMatrix). A tree-valid
// object3d with a clean cache loads its cached world matrix (+0x60). Else the
// single-parent chain is applied root first; the first changed node on it
// (list 7) is brought up to date before.
int node_to_world(Node* node, int mode) {
  if (!node) return zerr::report(0x400, kFile, 0xef1, "Null node pointer."), 5;
  if (node->cls() == 5 && (node->flags() & 0x80000)) {
    auto* d = static_cast<Object3d*>(node->data);
    if (!(d->get<uint32_t>(0) & 0x20)) {
      zmth::load(*mat(d->floats(0x60)));
      return 0;
    }
  }
  Node* chain[64];  // the exe's stack array holds 13; no bound check there
  int n = 1;
  chain[0] = node;
  Node* p = node;
  if (p->parent_count() > 1) {
    char msg[160];
    std::snprintf(msg, sizeof msg, "node has multiple parents; count = %d.\n  node = %s class_type = %d\n",
                  p->parent_count(), p->name(), p->cls());
    return zerr::report(0x800, kFile, 0xf0b, msg), 1;
  }
  while (p->parent_count() == 1 && (p = p->parents[0]) != nullptr) {
    chain[n++] = p;
    if (p->parent_count() > 1) {
      char msg[160];
      std::snprintf(msg, sizeof msg, "node has multiple parents; count = %d.\n  node = %s class_type = %d\n",
                    p->parent_count(), p->name(), p->cls());
      return zerr::report(0x800, kFile, 0xf0b, msg), 1;
    }
  }
  for (int i = 0; i < n; ++i) {
    if (chain[i]->cls() != 2 && (chain[i]->flags() & 1)) {
      if (g_hooks.update_node) g_hooks.update_node(g_hooks.ctx, chain[i]);
      break;
    }
  }
  for (int i = n - 1; i >= 0; --i) {
    Node* q = chain[i];
    const uint32_t flags = q->flags() & ~0x2000000u;
    q->set_flags(flags);
    switch (q->cls()) {
      case 1: {
        auto* d = static_cast<Camera*>(q->data);
        if (d->get<uint32_t>(0x10) & 2)
          zmth::multiply(*mat(d->floats(0x80)), 1);
        else
          zmth::euler(d->floats(0x20), d->floats(0x14), kOnes);
        break;
      }
      case 2:
      case 6:
        break;
      case 5: {
        auto* d = static_cast<Object3d*>(q->data);
        const uint32_t df = d->get<uint32_t>(0);
        if (!(df & 8)) {
          if (!(flags & 0x80000)) {
            zmth::multiply(*mat(d->floats(0x30)), mode);
          } else if (df & 0x20) {
            zmth::multiply(*mat(d->floats(0x30)), mode);
            *mat(d->floats(0x60)) = zmth::matrix_stack().matrix();
            d->put<uint32_t>(0, d->get<uint32_t>(0) & ~0x20u);
          } else {
            zmth::load(*mat(d->floats(0x60)));
          }
        } else if ((flags & 0x80000) && (df & 0x20)) {
          *mat(d->floats(0x60)) = zmth::matrix_stack().matrix();
          d->put<uint32_t>(0, df & ~0x20u);
        }
        break;
      }
      case 8: {  // animate: data +4 bit 2, its matrix at +8 (class not ported)
        auto* d = static_cast<uint8_t*>(q->data);
        if ((flags & 4) && (d[4] & 4)) zmth::multiply(*mat(d + 8), mode);
        break;
      }
      case 9: {
        auto* d = static_cast<Light*>(q->data);
        zmth::euler(d->floats(0), d->floats(0xc), kOnes);
        break;
      }
      case 10: {
        auto* d = static_cast<Sound*>(q->data);
        zmth::euler(kZeros, d->floats(0x30), kOnes);
        break;
      }
      default: {
        char msg[160];
        std::snprintf(msg, sizeof msg,
                      "gwNodeBuildNodeToAncestorMatrix(): Unrecognized node class type: node [%s] type [%d]",
                      q->name(), q->cls());
        return zerr::report(0x200, kFile, 0xf83, msg), 3;
      }
    }
  }
  return 0;
}

namespace {
// The cached world matrix of an object3d in the world tree, when still valid.
const zmth::Mat34* cached_world(Node* node) {
  if (node->cls() != 5 || !(node->flags() & 0x80000)) return nullptr;
  auto* d = static_cast<Object3d*>(node->data);
  return d->get<uint32_t>(0) & 0x20 ? nullptr : mat(d->floats(0x60));
}

// node_to_world into a matrix of its own, from the identity.
void world_matrix(Node* node, zmth::Mat34* m) {
  zmth::push(m);
  zmth::load_identity();
  node_to_world(node, 1);
  zmth::pop();
}
}  // namespace

// Mech3.exe 1.2 @ 0x004ed5d0
int node_world_euler(Node* node, float out[3]) {
  if (!node) return 1;
  if (const zmth::Mat34* c = cached_world(node)) return zmth::matrix_to_euler(*c, out), 0;
  zmth::Mat34 m;
  world_matrix(node, &m);
  zmth::matrix_to_euler(m, out);
  return 0;
}

// Mech3.exe 1.2 @ 0x004ed530
int node_world_position(Node* node, float out[3]) {
  if (!node) return 1;
  if (const zmth::Mat34* c = cached_world(node)) return std::memcpy(out, c->m + 9, 12), 0;
  out[0] = out[1] = out[2] = 0.0f;
  zmth::Mat34 m;
  world_matrix(node, &m);
  std::memcpy(out, m.m + 9, 12);
  return 0;
}

// Mech3.exe 1.2 @ 0x004ebdb0. Box min at +0x74, max at +0x80. The local
// matrix: camera +0x80, object3d +0x30 (none with data flag 8), animate +8
// when active; worlds, lods, sequences, lights and sounds have none.
int node_box_corners(Node* node, float out[8][3]) {
  if (!node) return zerr::report(0x400, kFile, 0x9cc, "Null node pointer."), 5;
  if (!node->data) return zerr::report(0x400, kFile, 0x9cd, "Null class data pointer"), 5;
  const uint32_t flags = node->flags();
  if (!(flags & 0x100)) return 1;
  int status = 0;
  const float* local = nullptr;
  bool local_identity = false;
  auto* raw = static_cast<uint8_t*>(node->data);
  switch (node->cls()) {
    case 1: local = reinterpret_cast<float*>(raw + 0x80); break;
    case 2: case 6: case 7: local_identity = true; break;
    case 3: case 4: status = 3; break;
    case 5:
      local_identity = (static_cast<Object3d*>(node->data)->get<uint32_t>(0) >> 3) & 1;
      local = reinterpret_cast<float*>(raw + 0x30);
      break;
    case 8:
      if (!(flags & 4) || !(raw[4] & 4)) local_identity = true;
      local = reinterpret_cast<float*>(raw + 8);
      break;
    case 9: case 10: break;
    default: status = 3; break;
  }
  if (!local) local_identity = true;
  zmth::MatrixStack& s = zmth::matrix_stack();
  const bool top_identity = s.slot[s.top] ? s.is_identity() != 0 : true;
  float box[6];
  std::memcpy(box, node->raw + 0x74, sizeof box);
  if (top_identity) {
    if (local_identity) {
      static const int kPick[8][3] = {{0, 1, 5}, {3, 1, 5}, {3, 1, 2}, {0, 1, 2},
                                      {0, 4, 5}, {3, 4, 5}, {3, 4, 2}, {0, 4, 2}};
      for (int i = 0; i < 8; ++i)
        for (int c = 0; c < 3; ++c) out[i][c] = box[kPick[i][c]];
      return status;
    }
    box_through(local, box, out);
    return status;
  }
  const float* top = s.matrix().m;
  if (local_identity) {
    box_through(top, box, out);
    return status;
  }
  float m[12];
  product(local, top, m);
  box_through(m, box, out);
  return status;
}

// Mech3.exe 1.2 @ 0x004ec150. 0x004ebdb0 in x and z only; the combined
// matrix fills just the entries the corner transform reads.
int node_box_xz(Node* node, float out[8][2]) {
  if (!node) return zerr::report(0x400, kFile, 0xa71, "Null node pointer."), 5;
  if (!node->data) return zerr::report(0x400, kFile, 0xa72, "Null class data pointer"), 5;
  const uint32_t flags = node->flags();
  if (!(flags & 0x100)) return 1;
  int status = 0;
  const float* local = nullptr;
  bool local_identity = false;
  auto* raw = static_cast<uint8_t*>(node->data);
  switch (node->cls()) {
    case 1: local = reinterpret_cast<float*>(raw + 0x80); break;
    case 2: case 6: case 7: local_identity = true; break;
    case 5:
      local_identity = (static_cast<Object3d*>(node->data)->get<uint32_t>(0) >> 3) & 1;
      local = reinterpret_cast<float*>(raw + 0x30);
      break;
    case 8:
      if (!(flags & 4) || !(raw[4] & 4)) local_identity = true;
      local = reinterpret_cast<float*>(raw + 8);
      break;
    case 9: case 10: break;
    default: status = 3; break;
  }
  if (!local) local_identity = true;
  zmth::MatrixStack& s = zmth::matrix_stack();
  const bool top_identity = s.slot[s.top] ? s.is_identity() != 0 : true;
  float box[6];
  std::memcpy(box, node->raw + 0x74, sizeof box);
  if (top_identity && local_identity) {
    static const int kPick[8][2] = {{0, 5}, {3, 5}, {3, 2}, {0, 2}, {0, 5}, {3, 5}, {3, 2}, {0, 2}};
    for (int i = 0; i < 8; ++i) out[i][0] = box[kPick[i][0]], out[i][1] = box[kPick[i][1]];
    return status;
  }
  const float* m = top_identity ? local : s.matrix().m;
  float c[12] = {};
  if (!top_identity && !local_identity) {
    const float* l = local;
    const float* t = s.matrix().m;
    c[0] = (l[1] * t[3] + l[2] * t[6]) + l[0] * t[0];
    c[3] = (l[3] * t[0] + l[4] * t[3]) + l[5] * t[6];
    c[6] = (l[6] * t[0] + l[7] * t[3]) + l[8] * t[6];
    c[2] = (l[0] * t[2] + l[2] * t[8]) + l[1] * t[5];
    c[5] = (l[5] * t[8] + l[3] * t[2]) + l[4] * t[5];
    c[8] = (l[6] * t[2] + l[7] * t[5]) + l[8] * t[8];
    c[9] = ((l[9] * t[0] + l[11] * t[6]) + l[10] * t[3]) + t[9];
    c[11] = ((l[9] * t[2] + l[10] * t[5]) + l[11] * t[8]) + t[11];
    m = c;
  }
  zmth::box_corners_xz(m, box, out);
  return status;
}

}  // namespace zcls
