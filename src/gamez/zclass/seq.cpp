#include "gamez/zclass/seq.h"

#include <cstring>

#pragma STDC FP_CONTRACT OFF

#include "crt/msvc_math.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zutil/ztimer.h"

namespace zcls {

namespace {
const char kSeqFile[] = "C:\\zipper\\GameZMech3\\zClass\\Seq.c";
const char kAnimFile[] = "C:\\zipper\\GameZMech3\\zClass\\Animate.c";

// Mech3.exe 1.2 @ 0x004fda20 (ECX the motion record, stack dt): 2 once
// stopped. Past the duration it stops (no loop) or goes back to the
// restart time; with a loop, past the loop end too.
int motion_advance(Animate* a, float dt) {
  if (a->get<int16_t>(0xcc) == 2) return 2;
  const float t = dt + a->get<float>(0xc0);
  a->put<float>(0xc0, t);
  if (a->get<float>(0xb8) < t) {
    if (a->get<int16_t>(0xd2) == -1) {
      a->put<float>(0xc0, 0.0f);
      a->put<int16_t>(0xcc, 2);
      return 1;
    }
    a->put<float>(0xc0, (t - a->get<float>(0xb8)) + a->get<float>(0xc4));
    return 1;
  }
  if (a->get<int16_t>(0xd2) != -1 && a->get<float>(0xc8) < t)
    a->put<float>(0xc0, (t - a->get<float>(0xc8)) + a->get<float>(0xc4));
  return 1;
}

// Mech3.exe 1.2 @ 0x004fdab0 (ECX the motion record): the pose between
// keys i and i + 1, i the truncated (count - 1) * time / duration, each
// value times its multiplier.
int motion_sample(Animate* a) {
  if (a->get<int16_t>(0xcc) == 2) return 2;
  const float pos = (static_cast<float>(a->get<int16_t>(0xd0)) - 1.0f) * a->get<float>(0xc0) / a->get<float>(0xb8);
  const int32_t i = static_cast<int16_t>(crt::msvc_ftol(pos));
  const float f = pos - static_cast<float>(i);
  const float* k0 = a->keys + 9 * i;
  const float* k1 = a->keys + 9 * (i + 1);
  for (int c = 0; c < 9; ++c) {
    const float v = (k1[c] - k0[c]) * f + k0[c];
    a->put<float>(0x70 + 4 * c, a->get<float>(0x94 + 4 * c) * v);
  }
  return 1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004fdf50. Entries all at time 0 with the time past
// them loop for ever, as in the exe (docs/action-lists.md).
int seq_update(Node* node) {
  if (!node) return zerr::report(0x400, kSeqFile, 0x193, "Null node pointer."), 5;
  auto* s = static_cast<Seq*>(node->data);
  if (!s) return zerr::report(0x400, kSeqFile, 0x194, "Null class data pointer"), 5;
  if (s->get<int32_t>(0xc) != 0 || s->get<int32_t>(0) == 0) return 0;
  const float t = ztimer::state().dt + s->get<float>(0x18);
  s->put<float>(0x18, t);
  // FIX (bug): a sequence without entries reads past its data in the exe.
  if (s->get<int32_t>(0x1c) <= 0 || !s->entries) return 0;
  int32_t i = s->get<int32_t>(0x14);
  if (!(s->entries[i].time < t)) return 0;
  const int32_t count = s->get<int32_t>(0x1c);
  do {
    const float at = s->entries[i].time;
    const int32_t step = s->get<int32_t>(0x10);
    i += step;
    s->put<int32_t>(0x14, i);
    s->put<float>(0x18, s->get<float>(0x18) - at);
    if (i >= count || i < 0) {
      const bool past_end = i >= count;
      if (s->get<int32_t>(8) == 0) {
        s->put<int32_t>(0x14, past_end ? count - 1 : 0);
        s->put<int32_t>(0x10, -step);
      } else {
        s->put<int32_t>(0x14, past_end ? 0 : count - 1);
      }
      if (s->get<int32_t>(4) == 0) s->put<int32_t>(0, 0);
    }
    i = s->get<int32_t>(0x14);
  } while (s->entries[i].time < s->get<float>(0x18));
  return 0;
}

// Mech3.exe 1.2 @ 0x004fd950
int animate_update(Node* node) {
  if (!node) return zerr::report(0x400, kAnimFile, 0x1b6, "Null node pointer."), 5;
  auto* a = static_cast<Animate*>(node->data);
  if (!a) return zerr::report(0x400, kAnimFile, 0x1b7, "Null class data pointer"), 5;
  if (!(a->get<uint32_t>(4) & 4)) return 0;
  if (motion_advance(a, ztimer::state().dt) == 2) {
    a->put<uint32_t>(4, a->get<uint32_t>(4) & ~4u);
    return 0;
  }
  motion_sample(a);
  a->put<uint32_t>(0, a->get<uint32_t>(0) | 1);
  if (!(node->flags() & 1) && push_front(kChanged, node) == 0) node->set_flags(node->flags() | 1);
  node->set_flags(node->flags() | 2);
  if (node->changed) node->changed->removable = 0;
  return 0;
}

// Mech3.exe 1.2 @ 0x004fdd90. The child's entry leaves the sequence too; an
// index left at or past the end goes back to 0.
int seq_remove_child(Node* parent, Node* child) {
  if (!parent) return zerr::report(0x400, kSeqFile, 0xd3, "Null node pointer."), 5;
  if (!child) return zerr::report(0x400, kSeqFile, 0xd4, "Null node pointer."), 5;
  auto* s = static_cast<Seq*>(parent->data);
  if (!s) return zerr::report(0x400, kSeqFile, 0xd5, "Null class data pointer"), 5;
  if (int r = node_unlink(parent, child)) return r;
  const int32_t count = s->get<int32_t>(0x1c);
  int32_t at = -1;
  for (int32_t i = 0; i < count; ++i)
    if (s->entries[i].node == child) {
      at = i;
      break;
    }
  if (at >= 0) {
    for (int32_t i = at; i < count - 1; ++i) s->entries[i] = s->entries[i + 1];
    s->put<int32_t>(0x1c, count - 1);
  }
  if (s->get<int32_t>(0x1c) <= s->get<int32_t>(0x14)) s->put<int32_t>(0x14, 0);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fd890
int animate_delete(Node* node) {
  if (!node) return zerr::report(0x400, kAnimFile, 0x7f, "Null node pointer."), 5;
  return node_release(node);
}

}  // namespace zcls
