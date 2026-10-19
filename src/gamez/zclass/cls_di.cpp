#include "gamez/zclass/cls_di.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "crt/msvc_math.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_hit.h"

#pragma STDC FP_CONTRACT OFF

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\cls_di.c";
constexpr int32_t kMaxProbes = 24;
constexpr uint32_t kActive = 4, kAltitude = 8, kFade = 0x10, kHasBox = 0x100, kZoned = 0x1000000;

// The query in progress (0x0060bab0..0x0060bac0).
struct Query {
  GroundHits* out = nullptr;   // 0x0060bab0 (0x0060bab4: its first hit)
  float top = 0;               // 0x0060bab8
  float* probes = nullptr;     // 0x0060babc
  int32_t count = 0;           // 0x0060bac0
};
Query g_q;

template <typename T> T at(const void* base, int off) {
  T v;
  std::memcpy(&v, static_cast<const uint8_t*>(base) + off, sizeof v);
  return v;
}
template <typename T> void put_at(void* base, int off, T v) { std::memcpy(static_cast<uint8_t*>(base) + off, &v, sizeof v); }

bool tested(const Node* n) { return (n->flags() & kActive) && (n->flags() & kAltitude); }

zmth::Mat34* mat(uint8_t* d, int off) { return reinterpret_cast<zmth::Mat34*>(d + off); }

int ground_node(Node* node, int32_t siblings, const int32_t* mask);

void model_test(Node* node, const int32_t* mask) {
  if (node->model) zmod::model_ground(node, node->model, g_q.probes, mask, g_q.count, g_q.top, g_q.out);
}

void ground_children(Node* node, const int32_t* mask, bool all) {
  for (int32_t i = 0; i < node->child_count(); ++i)
    if (all || tested(node->children[i])) ground_node(node->children[i], node->child_count(), mask);
}

// Mech3.exe 1.2 @ 0x004f1350. Masks out the probes outside the node's box in
// x and z; 1 when none is left inside (the node is skipped), also for a node
// without a box.
int box_test(Node* node, int32_t* mask) {
  if (!(node->flags() & kHasBox)) return 1;
  float c[8][2];
  node_box_xz(node, c);
  float minx = c[0][0], maxx = c[0][0], minz = c[0][1], maxz = c[0][1];
  for (int i = 1; i < 8; ++i) {
    if (c[i][0] < minx) minx = c[i][0];
    else if (c[i][0] > maxx) maxx = c[i][0];
    if (c[i][1] < minz) minz = c[i][1];
    else if (maxz < c[i][1]) maxz = c[i][1];
  }
  int none = 1;
  for (int32_t k = 0; k < g_q.count; ++k) {
    if (!mask[k]) continue;
    const float* p = g_q.probes + 3 * k;
    if (p[0] < minx || p[0] > maxx || p[2] < minz || maxz < p[2]) mask[k] = 0;
    else none = 0;
  }
  return none;
}

// Mech3.exe 1.2 @ 0x004eeb50 (an animated node, class 8)
int ground_animate(Node* node, int32_t siblings, const int32_t* in) {
  int32_t mask[kMaxProbes];
  std::memcpy(mask, in, 4 * g_q.count);
  if (siblings > 1) {
    if (int r = box_test(node, mask)) return r;
  }
  auto* d = static_cast<uint8_t*>(node->data);
  const bool pushed = node->raw[0x24] & kActive;
  if (pushed) {
    zmth::push_copy(mat(d, 0x38));
    zmth::multiply(*mat(d, 8), 1);
  }
  model_test(node, mask);
  ground_children(node, mask, true);
  if (pushed) zmth::pop();
  return 0;
}

// Mech3.exe 1.2 @ 0x004eec10 (a light, class 9: its position and angles)
int ground_light(Node* node, int32_t siblings, const int32_t* in) {
  int32_t mask[kMaxProbes];
  std::memcpy(mask, in, 4 * g_q.count);
  if (siblings > 1) {
    if (int r = box_test(node, mask)) return r;
  }
  auto* d = static_cast<uint8_t*>(node->data);
  zmth::push_copy(mat(d, 0x30));
  zmth::translate(at<float>(d, 0xc), at<float>(d, 0x10), at<float>(d, 0x14));
  zmth::rotate_y(at<float>(d, 4));
  zmth::rotate_x(at<float>(d, 0));
  zmth::rotate_z(at<float>(d, 8));
  model_test(node, mask);
  ground_children(node, mask, true);
  zmth::pop();
  return 0;
}

// Mech3.exe 1.2 @ 0x004ee790. `siblings` (EDX) is the count of the list the
// node came from: alone, it skips the box test. The return is ignored by
// every caller; class 10 returns a stack slot never written (0 here).
int ground_node(Node* node, int32_t siblings, const int32_t* in) {
  const uint32_t flags = node->flags();
  if (!(flags & kActive) || !(flags & kAltitude)) return 1;
  if ((flags & kZoned) && !zmod::zone_visible(node->raw[0x30])) return 1;
  const uint32_t kept = node->flags() & ~0x2000000u;
  node->set_flags(kept);
  int32_t mask[kMaxProbes];
  auto* d = static_cast<uint8_t*>(node->data);
  switch (node->cls()) {
    case 1: {  // camera: its pose at unit scale, in the slot at +0x44
      std::memcpy(mask, in, 4 * g_q.count);
      const float scale[3] = {1.0f, 1.0f, 1.0f};
      bool pushed = false;
      if (kept & kActive) {
        pushed = true;
        zmth::push_copy(mat(d, 0x44));
        zmth::euler(reinterpret_cast<float*>(d + 0x20), reinterpret_cast<float*>(d + 0x14), scale);
      }
      model_test(node, mask);
      ground_children(node, mask, false);
      if (pushed) zmth::pop();
      return 0;
    }
    case 5: {
      std::memcpy(mask, in, 4 * g_q.count);
      if (siblings > 1) {
        if (int r = box_test(node, mask)) return r;
      }
      auto* o = static_cast<Object3d*>(node->data);
      uint32_t of = o->get<uint32_t>(0);
      bool pushed = false;
      if (!(of & 8)) {  // not the identity
        pushed = true;
        if (!(node->flags() & 0x80000)) {
          zmth::push_copy(mat(d, 0x60));
          zmth::multiply(*mat(d, 0x30), 1);
        } else if (!(of & 0x20)) {
          zmth::push(mat(d, 0x60));  // the cached world matrix
        } else {
          zmth::push_copy(mat(d, 0x60));
          zmth::multiply(*mat(d, 0x30), 1);
          of = o->get<uint32_t>(0);
          if (!(of & 1)) o->put<uint32_t>(0, of & ~0x20u);
        }
      }
      model_test(node, mask);
      ground_children(node, mask, false);
      if (pushed) zmth::pop();
      return 0;
    }
    case 6:  // lod: past 5 it is not tested
      if (at<float>(d, 4) > 5.0) return 1;  // 0x005cdaf8
      std::memcpy(mask, in, 4 * g_q.count);
      if (siblings > 1) {
        if (int r = box_test(node, mask)) return r;
      }
      ground_children(node, mask, true);
      return 0;
    case 7: {  // sequence: only its current child
      if (at<int32_t>(d, 0) == 0) return 1;
      std::memcpy(mask, in, 4 * g_q.count);
      if (siblings > 1) {
        if (int r = box_test(node, mask)) return r;
      }
      Node* child;
      std::memcpy(&child, d + 0x20 + 8 * at<int32_t>(d, 0x14), sizeof child);  // FIX (A): a 32-bit pointer in the exe
      return ground_node(child, node->child_count(), mask);
    }
    case 8: return ground_animate(node, siblings, in);
    case 9: return ground_light(node, siblings, in);
    case 10: return 0;
    default: {
      char msg[96];
      std::snprintf(msg, sizeof msg, "Unrecognized node class type:  node = %s class_type = %d", node->name(),
                    node->cls());
      zerr::report(0x200, kFile, 0x626, msg);
      return 3;
    }
  }
}

int32_t cell_of(float v, float origin, float inv) {
  return crt::msvc_ftol(std::floor(static_cast<double>((v - origin) * inv)));
}
}  // namespace

// Mech3.exe 1.2 @ 0x004ee2f0
int ground_query(Node* world, float* probes, int32_t count, float top, GroundHits* out) {
  if (count > kMaxProbes) {
    char msg[64];
    std::snprintf(msg, sizeof msg, "More test pnts than space for: %d", count);
    zerr::report(0x200, kFile, 0x50d, msg);
    count = kMaxProbes;
  }
  if (update_hooks().pending) update_hooks().pending(update_hooks().ctx);  // 0x004f7e50 when [0x0060bea0]
  auto* w = static_cast<World*>(world->data);
  int32_t valid[kMaxProbes], wrapped[kMaxProbes];
  Partition* cell[kMaxProbes];
  float shift[kMaxProbes][2];
  for (int32_t i = 0; i < count; ++i) {
    float* p = probes + 3 * i;
    wrapped[i] = 0;
    valid[i] = 1;
    out[i].count = 0;
    const int32_t cx = cell_of(p[0], w->get<float>(0x34), w->get<float>(0x74));
    const int32_t cz = cell_of(p[2], w->get<float>(0x38), w->get<float>(0x78));
    if (cx >= 0 && cx < w->get<int32_t>(0x88) && cz >= 0 && cz < w->get<int32_t>(0x8c)) {
      cell[i] = &w->rows[cz][cx];
    } else if (w->get<int32_t>(0x50) == 0) {
      wrapped[i] = 0;
      valid[i] = 0;
    } else {
      int32_t sx, sz, inside;
      world_wrap_cell(world, cx, cz, &sx, &sz, &inside);
      wrapped[i] = 1;
      cell[i] = &w->rows[sz][sx];
      shift[i][0] = static_cast<float>(sx - cx) * w->get<float>(0x64);
      shift[i][1] = static_cast<float>(sz - cz) * w->get<float>(0x68);
      p[0] = shift[i][0] + p[0];
      p[2] = shift[i][1] + p[2];
    }
  }
  Partition* cells[kMaxProbes];
  int32_t ncells = 0;
  for (int32_t i = 0; i < count; ++i) {
    if (!valid[i]) continue;
    bool fresh = true;
    for (int32_t k = 0; k < ncells; ++k)
      if (cell[i] == cells[k]) fresh = false;
    if (fresh) cells[ncells++] = cell[i];
  }
  zmth::Mat34 m;
  zmth::push(&m);
  zmth::load_identity();
  g_q = {out, top, probes, count};
  for (int32_t c = 0; c < ncells; ++c) {
    Partition& p = *cells[c];
    int32_t mask[kMaxProbes];
    for (int32_t i = 0; i < count; ++i) mask[i] = valid[i] && cell[i] == &p;
    for (int32_t i = 0; i < p.get<int16_t>(0x3a); ++i)
      if (tested(p.nodes[i])) ground_node(p.nodes[i], p.get<int16_t>(0x3a) + 1, mask);
    for (int32_t i = 0; i < p.get<int32_t>(0x40); ++i) {
      uint8_t* e = p.fades + 0x44 * i;
      Node* n;
      std::memcpy(&n, e, sizeof n);  // FIX (A): a 32-bit pointer in the exe
      if (!(n->flags() & kActive) || !(n->flags() & kFade)) continue;
      std::memcpy(n->raw + 0x64, e + 0x28, 12);
      if (n->cls() == 5) {
        auto* o = static_cast<Object3d*>(n->data);
        o->put<uint32_t>(0, o->get<uint32_t>(0) & ~8u);
        std::memcpy(o->raw + 0x60, e + 4, 48);
        std::memcpy(o->raw + 0x30, e + 4, 48);
      }
      n->raw[0x30] = e[0x40];
      ground_node(n, p.get<int32_t>(0x40) + 1, mask);
    }
  }
  for (int32_t i = 0; i < count; ++i) {
    if (!wrapped[i]) continue;
    probes[3 * i] = probes[3 * i] - shift[i][0];
    probes[3 * i + 2] = probes[3 * i + 2] - shift[i][1];
  }
  for (int32_t i = 0; i < world->child_count(); ++i) {
    Node* n = world->children[i];
    if (!tested(n)) continue;
    if ((n->flags() & kZoned) && !zmod::zone_visible(n->raw[0x30])) continue;
    ground_node(n, world->child_count() + 1, valid);
  }
  zmth::pop();
  return 0;
}

}  // namespace zcls
