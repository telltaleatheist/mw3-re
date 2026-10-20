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

// The segment query in progress: the segments' boxes (0x0060b990, min xyz
// then max xyz) and the records ([0x0060bab0]).
struct SegmentQuery {
  float box[12][6];
  SegmentHits* out = nullptr;
  float* segs = nullptr;           // [0x0060babc]
  int32_t count = 0;               // [0x0060bac0], segments
};
SegmentQuery g_sq;
SegmentOptions g_opt;

bool stop_early() { return g_opt.first_hit && g_sq.out[0].count > 0; }

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

// Mech3.exe 1.2 @ 0x004f1640. Masks out the segments whose box misses the
// node's (both boxes in world space); 1 when none is left. With
// INTERSECT_BBOX the box's faces are tested too and decide.
int box_segments_test(Node* node, int32_t* mask) {
  if (!(node->flags() & kHasBox)) return 1;
  float c[8][3];
  node_box_corners(node, c);
  float lo[3] = {c[0][0], c[0][1], c[0][2]}, hi[3] = {c[0][0], c[0][1], c[0][2]};
  for (int i = 1; i < 8; ++i) {
    if (c[i][0] < lo[0]) lo[0] = c[i][0];
    else if (c[i][0] > hi[0]) hi[0] = c[i][0];
    if (c[i][1] < lo[1]) lo[1] = c[i][1];
    else if (c[i][1] > hi[1]) hi[1] = c[i][1];
    if (c[i][2] < lo[2]) lo[2] = c[i][2];
    else if (hi[2] < c[i][2]) hi[2] = c[i][2];
  }
  int32_t any = 0;
  for (int32_t k = 0; k < g_sq.count; ++k) {
    if (!mask[k]) continue;
    const float* b = g_sq.box[k];
    if (!(b[3] > lo[0]) || !(b[0] < hi[0]) || !(b[4] > lo[1]) || !(b[1] < hi[1]) || !(b[5] > lo[2]) ||
        !(hi[2] > b[2]))
      mask[k] = 0;
    else
      any = 1;
  }
  if (any && (node->flags() & 0x20)) any = zmod::box_segments(node, g_sq.out, g_sq.segs, mask, g_sq.count, c);
  return any == 0;
}

int segment_node(Node* node, int32_t siblings, const int32_t* mask);

void model_segments_of(Node* node, const int32_t* mask) {
  if (node->model) zmod::model_segments(node, node->model, g_sq.segs, mask, g_sq.count, g_sq.out);
}

void segment_children(Node* node, const int32_t* mask, bool all) {
  for (int32_t i = 0; i < node->child_count(); ++i) {
    Node* c = node->children[i];
    if (!all && !((c->flags() & kActive) && (c->flags() & kFade))) continue;
    segment_node(c, node->child_count(), mask);
    if (stop_early()) return;
  }
}

// The box test the segment visitors share: alone and without INTERSECT_BBOX
// a node skips it; a node that is its box ends there. -1 goes on.
int segment_box(Node* node, int32_t siblings, int32_t* mask) {
  if (siblings > 1 || (node->flags() & 0x20)) {
    if (int r = box_segments_test(node, mask)) return r;
    if (node->flags() & 0x20) return 0;
  }
  return -1;
}

// Mech3.exe 1.2 @ 0x004f08c0 (class 8)
int segment_animate(Node* node, int32_t siblings, const int32_t* in) {
  int32_t mask[kMaxProbes];
  std::memcpy(mask, in, 4 * g_sq.count);
  const int b = segment_box(node, siblings, mask);
  if (b >= 0) return b;
  auto* d = static_cast<uint8_t*>(node->data);
  const bool pushed = node->flags() & kActive;
  if (pushed) {
    zmth::push_copy(mat(d, 0x38));
    zmth::multiply(*mat(d, 8), 1);
  }
  model_segments_of(node, mask);
  if (!stop_early()) segment_children(node, mask, true);
  if (pushed) zmth::pop();
  return 0;
}

// Mech3.exe 1.2 @ 0x004f09b0 (class 9)
int segment_light(Node* node, int32_t siblings, const int32_t* in) {
  int32_t mask[kMaxProbes];
  std::memcpy(mask, in, 4 * g_sq.count);
  const int b = segment_box(node, siblings, mask);
  if (b >= 0) return b;
  auto* d = static_cast<uint8_t*>(node->data);
  zmth::push_copy(mat(d, 0x30));
  zmth::translate(at<float>(d, 0xc), at<float>(d, 0x10), at<float>(d, 0x14));
  zmth::rotate_y(at<float>(d, 4));
  zmth::rotate_x(at<float>(d, 0));
  zmth::rotate_z(at<float>(d, 8));
  model_segments_of(node, mask);
  if (!stop_early()) segment_children(node, mask, false);
  zmth::pop();
  return 0;
}

// Mech3.exe 1.2 @ 0x004f0480. Unlike the ground walk, the zone is always
// tested, and the object3d's matrix is multiplied in mode 3 (all of it, as 1).
// Class 10 returns its mask pointer in the exe (non-zero); 1 here.
int segment_node(Node* node, int32_t siblings, const int32_t* in) {
  const uint32_t flags = node->flags();
  if (!(flags & kActive) || !(flags & kFade)) return 1;
  if (g_opt.exclude & flags) return 1;
  if (!zmod::zone_visible(node->raw[0x30])) return 1;
  const uint32_t kept = node->flags() & ~0x2000000u;
  node->set_flags(kept);
  int32_t mask[kMaxProbes];
  auto* d = static_cast<uint8_t*>(node->data);
  switch (node->cls()) {
    case 1: {
      const float scale[3] = {1.0f, 1.0f, 1.0f};
      bool pushed = false;
      if (kept & kActive) {
        pushed = true;
        zmth::push_copy(mat(d, 0x44));
        zmth::euler(reinterpret_cast<float*>(d + 0x20), reinterpret_cast<float*>(d + 0x14), scale);
      }
      model_segments_of(node, in);
      if (!stop_early()) segment_children(node, in, true);
      if (pushed) zmth::pop();
      return 0;
    }
    case 5: {
      std::memcpy(mask, in, 4 * g_sq.count);
      const int b = segment_box(node, siblings, mask);
      if (b >= 0) return b;
      auto* o = static_cast<Object3d*>(node->data);
      uint32_t of = o->get<uint32_t>(0);
      bool pushed = false;
      if (!(of & 8)) {
        pushed = true;
        if (!(node->flags() & 0x80000)) {
          zmth::push_copy(mat(d, 0x60));
          zmth::multiply(*mat(d, 0x30), 3);
        } else if (!(of & 0x20)) {
          zmth::push(mat(d, 0x60));
        } else {
          zmth::push_copy(mat(d, 0x60));
          zmth::multiply(*mat(d, 0x30), 1);
          of = o->get<uint32_t>(0);
          if (!(of & 1)) o->put<uint32_t>(0, of & ~0x20u);
        }
      }
      model_segments_of(node, mask);
      if (!stop_early()) segment_children(node, mask, false);
      if (pushed) zmth::pop();
      return 0;
    }
    case 6: {
      if (at<float>(d, 4) > 5.0) return 1;  // 0x005cdaf8
      std::memcpy(mask, in, 4 * g_sq.count);
      const int b = segment_box(node, siblings, mask);
      if (b >= 0) return b;
      segment_children(node, mask, true);
      return 0;
    }
    case 7: {
      if (at<int32_t>(d, 0) == 0) return 1;
      std::memcpy(mask, in, 4 * g_sq.count);
      const int b = segment_box(node, siblings, mask);
      if (b >= 0) return b;
      Node* child;
      std::memcpy(&child, d + 0x20 + 8 * at<int32_t>(d, 0x14), sizeof child);  // FIX (A): a 32-bit pointer in the exe
      return segment_node(child, node->child_count(), mask);
    }
    case 8: return segment_animate(node, siblings, in);
    case 9: return segment_light(node, siblings, in);
    case 10: return 1;
    default: {
      char msg[96];
      std::snprintf(msg, sizeof msg, "Unrecognized node class type:  node = %s class_type = %d", node->name(),
                    node->cls());
      zerr::report(0x200, kFile, 0xe43, msg);
      return 3;
    }
  }
}

int32_t cell_of(float v, float origin, float inv) {
  return crt::msvc_ftol(std::floor(static_cast<double>((v - origin) * inv)));
}
// Mech3.exe 1.2 @ 0x004efef0: the cells under the segments' boxes (all of
// them, dropped ones too), each cell's nodes and fades.
void segment_cells(Node* world, const int32_t* valid) {
  auto* w = static_cast<World*>(world->data);
  int32_t x_lo = 0, x_hi = 0, z_lo = 0, z_hi = 0;
  for (int32_t k = 0; k < g_sq.count; ++k) {
    const float* b = g_sq.box[k];
    const int32_t a = cell_of(b[0], w->get<float>(0x34), w->get<float>(0x74));
    const int32_t zb = cell_of(b[5], w->get<float>(0x38), w->get<float>(0x78));
    const int32_t c = cell_of(b[3], w->get<float>(0x34), w->get<float>(0x74));
    const int32_t zd = cell_of(b[2], w->get<float>(0x38), w->get<float>(0x78));
    if (k == 0) {
      x_lo = a, z_lo = zb, x_hi = c, z_hi = zd;
      continue;
    }
    if (a < x_lo) x_lo = a;
    if (zb < z_lo) z_lo = zb;
    if (c > x_hi) x_hi = c;
    if (zd > z_hi) z_hi = zd;
  }
  for (int32_t z = z_lo; z <= z_hi; ++z) {
    for (int32_t x = x_lo; x <= x_hi; ++x) {
      bool wrapped = false;
      int32_t sx = x, sz = z;
      float dx = 0, dz = 0;
      uint8_t saved[12];
      if (!(x >= 0 && x < w->get<int32_t>(0x88) && z >= 0 && z < w->get<int32_t>(0x8c))) {
        if (w->get<int32_t>(0x50) == 0) continue;
        wrapped = true;
        for (int32_t k = 0; k < g_sq.count; ++k) saved[k] = g_sq.out[k].count;
        int32_t inside;
        world_wrap_cell(world, x, z, &sx, &sz, &inside);
        dx = static_cast<float>(sx - x) * w->get<float>(0x64);
        dz = static_cast<float>(sz - z) * w->get<float>(0x68);
        for (int32_t k = 0; k < g_sq.count; ++k) {
          float* s = g_sq.segs + 6 * k;
          s[0] = dx + s[0], s[2] = dz + s[2], s[3] = dx + s[3], s[5] = dz + s[5];
          float* b = g_sq.box[k];
          b[0] = dx + b[0], b[2] = dz + b[2], b[3] = dx + b[3], b[5] = dz + b[5];
        }
      }
      Partition& p = w->rows[sz][sx];
      for (int32_t i = 0; i < p.get<int16_t>(0x3a); ++i) {
        Node* n = p.nodes[i];
        if (!(n->flags() & kActive) || !(n->flags() & kFade)) continue;
        segment_node(n, p.get<int16_t>(0x3a) + 1, valid);
        if (stop_early()) break;
      }
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
        segment_node(n, p.get<int32_t>(0x40) + 1, valid);
        if (stop_early()) return;  // the shifted segments are not moved back
      }
      if (!wrapped) continue;
      for (int32_t k = 0; k < g_sq.count; ++k) {
        float* s = g_sq.segs + 6 * k;
        s[0] = s[0] - dx, s[2] = s[2] - dz, s[3] = s[3] - dx, s[5] = s[5] - dz;
        float* b = g_sq.box[k];
        b[0] = b[0] - dx, b[2] = b[2] - dz, b[3] = b[3] - dx, b[5] = b[5] - dz;
      }
      for (int32_t k = 0; k < g_sq.count; ++k) {
        SegmentHits& r = g_sq.out[k];
        for (int32_t j = saved[k]; j < r.count; ++j) {
          r.hit[j].point[0] = r.hit[j].point[0] - dx;
          r.hit[j].point[2] = r.hit[j].point[2] - dz;
        }
      }
    }
  }
}
}  // namespace

SegmentOptions& segment_options() { return g_opt; }

// Mech3.exe 1.2 @ 0x004efcd0
void segment_query(Node* world, float* segs, int32_t points, SegmentHits* out) {
  if (points > kMaxProbes) {
    char msg[64];
    std::snprintf(msg, sizeof msg, "More test pnts than space for: %d", points);
    zerr::report(0x200, kFile, 0xc83, msg);
    points = kMaxProbes;
  }
  if (update_hooks().pending) update_hooks().pending(update_hooks().ctx);
  const int32_t n = points >> 1;
  int32_t valid[12];
  for (int32_t k = 0; k < n; ++k) valid[k] = 1, out[k].count = 0;
  auto* w = static_cast<World*>(world->data);
  int32_t any = 0;
  for (int32_t k = 0; k < n; ++k) {
    const float* s = segs + 6 * k;
    float* b = g_sq.box[k];
    for (int c = 0; c < 3; ++c) {
      if (s[c] < s[3 + c]) b[c] = s[c], b[3 + c] = s[3 + c];
      else b[c] = s[3 + c], b[3 + c] = s[c];
    }
    // Quirk: the bounds test reads the first segment's box (fixed addresses
    // 0x0060b990..), so every segment stands or falls with segment 0.
    const float* b0 = g_sq.box[0];
    if (w->get<int32_t>(0x50) != 0) {
      any = 1;
    } else if (!(b0[0] < w->get<float>(0x44)) || b0[3] < w->get<float>(0x34) || b0[2] > w->get<float>(0x38) ||
               !(b0[5] > w->get<float>(0x48))) {
      valid[k] = 0;
    } else {
      any = 1;
    }
  }
  if (any) {
    zmth::Mat34 m;
    zmth::push(&m);
    zmth::load_identity();
    g_sq.out = out;
    g_sq.segs = segs;
    g_sq.count = n;
    segment_cells(world, valid);
    for (int32_t i = 0; i < world->child_count(); ++i) {
      Node* c = world->children[i];
      if (!(c->flags() & kActive) || !(c->flags() & kFade) || !zmod::zone_visible(c->raw[0x30])) continue;
      segment_node(c, world->child_count() + 1, valid);
      if (stop_early()) break;
    }
    zmth::pop();
  }
  g_opt.exclude = 0;
}

namespace {
// The point query in progress: the point (0x0060b978, moved into the
// stored cell while it is tested) and the record (0x0060bab0; the next
// slot, 0x0060bab4, is its count's).
struct PointQuery {
  float p[3] = {};
  GroundHits* out = nullptr;
};
PointQuery g_pq;

int point_node(Node* node, int32_t siblings);

// The node's model under the point: a hit takes the record's next slot.
// FIX (A): the exe's record has no room past 32 (only the node walk's own
// entry checks); the port stops there.
void point_model(Node* node) {
  GroundHits& r = *g_pq.out;
  if (!node->model || r.count >= 32) return;
  if (!zmod::model_ground_point(node->model, &r.hit[r.count], g_pq.p)) return;
  r.hit[r.count].node = node;
  ++r.count;
}

void point_children(Node* node, bool all) {
  for (int32_t i = 0; i < node->child_count(); ++i)
    if (all || tested(node->children[i])) point_node(node->children[i], node->child_count());
}

int point_result() { return g_pq.out->count == 0; }

// Mech3.exe 1.2 @ 0x004f12e0. 0 when the point is inside the node's box
// in x and z; 1 outside, or without a box.
int box_point_test(Node* node) {
  if (!(node->flags() & kHasBox)) return 1;
  float b[6];
  node_box_extent_xz(node, b);
  const float x = g_pq.p[0], z = g_pq.p[2];
  if (!(x >= b[0]) || x > b[3] || !(z >= b[2]) || z > b[5]) return 1;
  return 0;
}

// Mech3.exe 1.2 @ 0x004ee140 (class 8). Its own model is tested before its
// matrix goes on the stack, then its box.
int point_animate(Node* node, int32_t siblings) {
  point_model(node);
  auto* d = static_cast<uint8_t*>(node->data);
  const bool pushed = node->raw[0x24] & kActive;
  if (pushed) {
    zmth::push_copy(mat(d, 0x38));
    zmth::multiply(*mat(d, 8), 1);
  }
  if (siblings > 1) {
    if (int r = box_point_test(node)) {
      if (pushed) zmth::pop();
      return r;
    }
  }
  point_children(node, true);
  if (pushed) zmth::pop();
  return point_result();
}

// Mech3.exe 1.2 @ 0x004ee220 (class 9)
int point_light(Node* node, int32_t siblings) {
  if (siblings > 1) {
    if (int r = box_point_test(node)) return r;
  }
  auto* d = static_cast<uint8_t*>(node->data);
  zmth::push_copy(mat(d, 0x30));
  zmth::translate(at<float>(d, 0xc), at<float>(d, 0x10), at<float>(d, 0x14));
  zmth::rotate_y(at<float>(d, 4));
  zmth::rotate_x(at<float>(d, 0));
  zmth::rotate_z(at<float>(d, 8));
  point_model(node);
  point_children(node, true);
  zmth::pop();
  return point_result();
}

// Mech3.exe 1.2 @ 0x004eddb0. As ground_node, for one point; class 10
// returns a stack slot never written (0 here; no caller reads it).
int point_node(Node* node, int32_t siblings) {
  const uint32_t flags = node->flags();
  if (!(flags & kActive) || !(flags & kAltitude)) return 1;
  if ((flags & kZoned) && !zmod::zone_visible(node->raw[0x30])) return 1;
  const uint32_t kept = node->flags() & ~0x2000000u;
  node->set_flags(kept);
  if (g_pq.out->count > 0x1f) {
    zerr::report(0x200, kFile, 0x2e3, "Database intersections array is full");
    return 1;
  }
  auto* d = static_cast<uint8_t*>(node->data);
  switch (node->cls()) {
    case 1: {  // camera
      const float scale[3] = {1.0f, 1.0f, 1.0f};
      const bool pushed = kept & kActive;
      if (pushed) {
        zmth::push_copy(mat(d, 0x44));
        zmth::euler(reinterpret_cast<float*>(d + 0x20), reinterpret_cast<float*>(d + 0x14), scale);
      }
      point_model(node);
      point_children(node, false);
      if (pushed) zmth::pop();
      return point_result();
    }
    case 5: {
      if (siblings > 1) {
        if (int r = box_point_test(node)) return r;
      }
      auto* o = static_cast<Object3d*>(node->data);
      uint32_t of = o->get<uint32_t>(0);
      bool pushed = false;
      if (!(of & 8)) {
        pushed = true;
        if (!(node->flags() & 0x80000)) {
          zmth::push_copy(mat(d, 0x60));
          zmth::multiply(*mat(d, 0x30), 1);
        } else if (!(of & 0x20)) {
          zmth::push(mat(d, 0x60));
        } else {
          zmth::push_copy(mat(d, 0x60));
          zmth::multiply(*mat(d, 0x30), 1);
          of = o->get<uint32_t>(0);
          if (!(of & 1)) o->put<uint32_t>(0, of & ~0x20u);
        }
      }
      point_model(node);
      point_children(node, false);
      if (pushed) zmth::pop();
      return point_result();
    }
    case 6:  // lod: past 5 it is not tested
      if (at<float>(d, 4) > 5.0) return 1;  // 0x005cdaf8
      if (siblings > 1) {
        if (int r = box_point_test(node)) return r;
      }
      point_children(node, true);
      return point_result();
    case 7: {  // sequence: only its current child
      if (at<int32_t>(d, 0) == 0) return 1;
      if (siblings > 1) {
        if (int r = box_point_test(node)) return r;
      }
      Node* child;
      std::memcpy(&child, d + 0x20 + 8 * at<int32_t>(d, 0x14), sizeof child);  // FIX (A): a 32-bit pointer in the exe
      return point_node(child, node->child_count());
    }
    case 8: return point_animate(node, siblings);
    case 9: return point_light(node, siblings);
    case 10: return 0;
    default: {
      char msg[96];
      std::snprintf(msg, sizeof msg, "Unrecognized node class type:  node = %s class_type = %d", node->name(),
                    node->cls());
      zerr::report(0x200, kFile, 0x30d, msg);
      return 3;
    }
  }
}
}  // namespace

// Mech3.exe 1.2 @ 0x004edab0. The stored cell's nodes, then its fading
// ones (each given its stored pose and zone first), then the world's own
// children at the point as given.
int ground_point(Node* world, float x, float y, float z, GroundHits* out) {
  out->count = 0;
  if (update_hooks().pending) update_hooks().pending(update_hooks().ctx);  // 0x004f7e50 when [0x0060bea0]
  g_pq.p[0] = x, g_pq.p[1] = y, g_pq.p[2] = z;
  g_pq.out = out;
  auto* w = static_cast<World*>(world->data);
  zmth::Mat34 m;
  zmth::push(&m);
  zmth::load_identity();
  const int32_t cx = cell_of(x, w->get<float>(0x34), w->get<float>(0x74));
  const int32_t cz = cell_of(z, w->get<float>(0x38), w->get<float>(0x78));
  Partition* cell = nullptr;
  bool wrapped = false;
  float shift[2] = {};
  if (cx >= 0 && cx < w->get<int32_t>(0x88) && cz >= 0 && cz < w->get<int32_t>(0x8c)) {
    cell = &w->rows[cz][cx];
  } else if (w->get<int32_t>(0x50) != 0) {
    int32_t sx, sz, inside;
    world_wrap_cell(world, cx, cz, &sx, &sz, &inside);
    wrapped = true;
    shift[0] = static_cast<float>(sx - cx) * w->get<float>(0x64);
    shift[1] = static_cast<float>(sz - cz) * w->get<float>(0x68);
    cell = &w->rows[sz][sx];
    g_pq.p[0] = shift[0] + g_pq.p[0];
    g_pq.p[2] = shift[1] + g_pq.p[2];
  }
  if (cell) {
    Partition& p = *cell;
    for (int32_t i = 0; i < p.get<int16_t>(0x3a); ++i) {
      Node* n = p.nodes[i];
      if (!tested(n)) continue;
      if ((n->flags() & kZoned) && !zmod::zone_visible(n->raw[0x30])) continue;
      point_node(n, p.get<int16_t>(0x3a) + 1);
    }
    for (int32_t i = 0; i < p.get<int32_t>(0x40); ++i) {
      uint8_t* e = p.fades + 0x44 * i;
      Node* n;
      std::memcpy(&n, e, sizeof n);  // FIX (A): a 32-bit pointer in the exe
      if (!(n->flags() & kActive) || !(n->flags() & kFade)) continue;
      n->raw[0x30] = e[0x40];
      if ((n->flags() & kZoned) && !zmod::zone_visible(n->raw[0x30])) continue;
      std::memcpy(n->raw + 0x64, e + 0x28, 12);
      if (n->cls() == 5) {
        auto* o = static_cast<Object3d*>(n->data);
        o->put<uint32_t>(0, o->get<uint32_t>(0) & ~8u);
        std::memcpy(o->raw + 0x60, e + 4, 48);
        std::memcpy(o->raw + 0x30, e + 4, 48);
      }
      point_node(n, p.get<int32_t>(0x40) + 1);
    }
    if (wrapped) {
      g_pq.p[0] = g_pq.p[0] - shift[0];
      g_pq.p[2] = g_pq.p[2] - shift[1];
    }
  }
  for (int32_t i = 0; i < world->child_count(); ++i) {
    Node* n = world->children[i];
    if (!tested(n)) continue;
    if ((n->flags() & kZoned) && !zmod::zone_visible(n->raw[0x30])) continue;
    point_node(n, world->child_count() + 1);
  }
  zmth::pop();
  return out->count == 0;
}

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
