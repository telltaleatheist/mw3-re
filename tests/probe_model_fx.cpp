// Probes for test_model_fx.py (docs/render.md section 3, Texture scroll and
// the colour blend): the model texture scroll (zmod model_set_scroll
// 0x00526ae0, model_scroll_tick 0x00541ad0) and the object3d blend (zcls
// object3d_set_blend 0x004fb8c0, object3d_use_blend 0x004fb840) on data the
// test lays out as it lays out the emulator's.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zutil/ztimer.h"

extern "C" {

// The scroll's step on a model: `header` (0x5c bytes, in and out), `n`
// polygons with `counts` vertices each, `textured` (the material's +1 bit 0)
// and their UV pairs one after another in `uvs` (in and out; each polygon
// has at least one pair). Returns the step's result.
int32_t probe_mfx_tick(uint8_t* header, int32_t n, const int32_t* counts, const int32_t* textured, float* uvs,
                       uint32_t frame, float dt, int32_t null_model) {
  zcls::frames().frame = frame;
  ztimer::state().dt = dt;
  if (null_model) return zmod::model_scroll_tick(nullptr);
  zmod::Model m{};
  std::memcpy(m.header, header, sizeof m.header);
  std::vector<zmod::Polygon> polys(static_cast<size_t>(n));
  std::vector<zmod::Material> mats(static_cast<size_t>(n));
  float* at = uvs;
  for (int32_t i = 0; i < n; ++i) {
    zmod::Polygon& p = polys[static_cast<size_t>(i)];
    std::memset(&p, 0, sizeof p);
    p.flags = static_cast<uint32_t>(counts[i]) & 0xff;
    std::memset(&mats[static_cast<size_t>(i)], 0, sizeof(zmod::Material));
    mats[static_cast<size_t>(i)].flags = textured[i] ? 1 : 0;
    p.material = &mats[static_cast<size_t>(i)];
    p.uvs = at;
    at += 2 * (counts[i] > 0 ? counts[i] : 1);
  }
  m.polygons = polys.data();
  const int32_t r = zmod::model_scroll_tick(&m);
  std::memcpy(header, m.header, sizeof m.header);
  m.polygons = nullptr;
  return r;
}

int32_t probe_mfx_set_scroll(uint8_t* header, int32_t on, float u, float v, int32_t null_model) {
  if (null_model) return zmod::model_set_scroll(nullptr, on, u, v);
  zmod::Model m{};
  std::memcpy(m.header, header, sizeof m.header);
  const int32_t r = zmod::model_set_scroll(&m, on, u, v);
  std::memcpy(header, m.header, sizeof m.header);
  return r;
}

// The blend on a node: `what` 0 no node, 1 no data, else a node of class
// `cls` with the object3d data `data` (0x90 bytes, in and out). `op` 0 the
// use (on), 1 the set (rgb or none, weight).
int32_t probe_mfx_blend(int32_t what, int32_t cls, uint8_t* data, int32_t op, int32_t on, const float* rgb,
                        float weight) {
  zcls::Node node{};
  zcls::Object3d d{};
  std::memcpy(d.raw, data, sizeof d.raw);
  node.put<int32_t>(0x34, cls);
  node.data = what == 1 ? nullptr : &d;
  zcls::Node* n = what == 0 ? nullptr : &node;
  const int32_t r = op == 0 ? zcls::object3d_use_blend(n, on) : zcls::object3d_set_blend(n, rgb, weight);
  std::memcpy(data, d.raw, sizeof d.raw);
  return r;
}

}  // extern "C"
