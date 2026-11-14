#include "pm/gamez/zmodel/gmod_shadows.h"

#include <cstring>
#include <vector>

#pragma STDC FP_CONTRACT OFF

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_shadows.h"

namespace pm::zmod {

// Pirate's Moon 1.0 @ 0x00563760: MW3's 0x00548250 over PM's model: its polygons 0x28 bytes, their count the low 10
// bits, their colours at +0x1c (the same 0.2, the same weight test, the same 1,024-word stack array: the FIX (A) as
// MW3's).
void shadow_model(Model& m) {
  uint32_t head, flags;
  std::memcpy(&head, m.header, 4);
  std::memcpy(&flags, m.header + 8, 4);
  if (head != 0 || m.count(0x10) == 0) return;
  const int32_t verts = m.count(0x14);
  const int32_t n = verts > 0 ? verts : 0;
  float weight;
  std::memcpy(&weight, m.header + 0x24, 4);
  std::vector<zmth::Vec3> pts(n);
  if (n) std::memcpy(pts.data(), m.vertices, sizeof(zmth::Vec3) * n);
  if ((flags & 8) && weight != 0.0f && m.count(0x1c) != 0)
    zmth::morph(pts.data(), reinterpret_cast<const zmth::Vec3*>(m.vertices),
                reinterpret_cast<const zmth::Vec3*>(m.morphs), static_cast<uint32_t>(m.count(0x1c)), weight);
  zmth::transform_points(pts.data(), static_cast<uint32_t>(n));
  const ::zmod::BakeShadowState& st = ::zmod::bake_shadow_state();
  std::vector<uint8_t> dark(n);
  for (int32_t i = 0; i < n; ++i) {
    const float from[3] = {pts[i].x, pts[i].y, pts[i].z};
    const float to[3] = {pts[i].x + st.offset[0], pts[i].y + st.offset[1], pts[i].z + st.offset[2]};
    zcls::LineHits hits;
    hits.count = 0;
    dark[i] = ::zmod::shadow_query()(st.world, from, to, &hits) == 0 && hits.count != 0;
  }
  for (int32_t p = 0; p < m.count(0x10); ++p) {
    Polygon& q = m.polygons[p];
    for (uint32_t k = 0; k < (q.flags & 0x3ff); ++k) {
      if (!dark[q.vertex_indices[k]]) continue;
      for (int c = 0; c < 3; ++c) q.colors[3 * k + c] = q.colors[3 * k + c] * 0.2f;  // 0x005deffc
    }
  }
}

}  // namespace pm::zmod
