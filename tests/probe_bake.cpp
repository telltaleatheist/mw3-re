// Probes for test_bake.py (docs/drafts/bake-textures.md): the baked static
// light 0x544ed0 on the models of a world read by the ported reader.
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_bake.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zvideo/zvid_format.h"

namespace {
std::string g_world;
std::vector<std::unique_ptr<zcls::Light>> g_lights;
void quiet(int, const char*, int, const char*) {}
}  // namespace

extern "C" {

// Reads a world (0x4ff520) once; returns the model pool size, or < 0.
int32_t probe_bake_world(const char* path) {
  if (g_world == path) return zmod::models().size;
  zimg::textures().count = 0;
  zimg::textures().group = 3;
  zmod::materials() = zmod::MaterialTable{};
  zmod::models() = zmod::ModelPool{};
  zvid::set_pixel_format(5, 6, 5);
  zcls::nodes() = zcls::NodePool{};
  zcls::lists() = zcls::Lists{};
  zmod::fog() = zmod::Fog{};
  zrndr::fog_color() = zrndr::FogColor{};
  zrndr::software_format() = zrndr::SoftwareFormat{0xf800, 0x07e0, 8, 3, 3};
  zvid::clear_color() = 0;
  zerr::set_sink(quiet);
  const int r = zcls::read_zbd(path);
  zerr::set_sink(nullptr);
  if (r != 0) return -1;
  g_world = path;
  return zmod::models().size;
}

// 0x544ed0 on model `index`. flags: the model's +0x08 for this run (restored
// after). cam / normals: its vertices and normals in camera space. lights: n
// light data blocks (0xd0 each) with per-light full and reach. stale: the
// carried vector, in and out. out: every polygon without flag 0x100, its
// vertices' baked light in file order. Returns the floats written, or < 0.
int32_t probe_bake(int32_t index, uint32_t flags, const float* cam, const float* normals, const uint8_t* lights,
                   const int32_t* full_reach, int32_t n, float* stale, float* out, int32_t cap) {
  if (index < 0 || index >= zmod::models().size) return -1;
  zmod::Model& m = zmod::models().slots[index];
  if (!m.polygons) return -2;
  g_lights.clear();
  zmod::LightList& L = zmod::light_list();
  L.count = n;
  for (int32_t i = 0; i < n; ++i) {
    auto d = std::make_unique<zcls::Light>();
    std::memcpy(d->raw, lights + 0xd0 * i, 0xd0);
    L.list[i].data = d.get();
    L.list[i].full = full_reach[2 * i], L.list[i].reach = full_reach[2 * i + 1];
    g_lights.push_back(std::move(d));
  }
  uint32_t saved;
  std::memcpy(&saved, m.header + 8, 4);
  std::memcpy(m.header + 8, &flags, 4);
  std::memcpy(zmod::bake_state().stale, stale, 12);
  zmod::bake_static_light(m, reinterpret_cast<const float(*)[3]>(cam), reinterpret_cast<const float(*)[3]>(normals));
  std::memcpy(m.header + 8, &saved, 4);
  std::memcpy(stale, zmod::bake_state().stale, 12);
  int32_t k = 0;
  for (uint32_t p = 0; p < m.count(0x10); ++p) {
    const zmod::Polygon& q = m.polygons[p];
    if (q.flags & 0x100) continue;
    const uint32_t c = q.flags & 0xff;
    if (k + static_cast<int32_t>(c) > cap) return -3;
    std::memcpy(out + k, q.scratch, 4 * c);
    k += static_cast<int32_t>(c);
  }
  return k;
}

}  // extern "C"
