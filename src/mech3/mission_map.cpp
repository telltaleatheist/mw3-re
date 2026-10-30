#include "mech3/mission_map.h"

#include <cstdlib>
#include <cstring>

#include "gamez/zclass/cls_di.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"

namespace mech3 {

namespace {
MissionMaps g_maps;
char* dup(const char* s) { return s ? strdup(s) : nullptr; }
}  // namespace

MissionMaps& mission_maps() { return g_maps; }

// Mech3.exe 1.2 @ 0x004580c0
MissionMap::MissionMap(zrdr::Node* block) : Image(0) {
  // The offset's default is [0x0063d870], zero.
  if (const char* s = zrdr::get_string(block, "name")) name = dup(s);
  if (const char* s = zrdr::get_string(block, "bitmap")) bitmap = dup(s);
  zrdr::get_vec3(block, "offset", offset);
  zrdr::get_float(block, "mpp", &mpp);
  inv_mpp = mpp == 0.0f ? 0.0f : 1.0f / mpp;
}

// Mech3.exe 1.2 @ 0x00458200
MissionMap::~MissionMap() {
  std::free(name);
  std::free(bitmap);
  std::free(rect);
  name = bitmap = nullptr;
  rect = nullptr;
}

// Mech3.exe 1.2 @ 0x00458300
void MissionMap::load_image() {
  if (!image) {
    set_image(ztwod::host().image_get(bitmap));  // 0x005179d0
    if (!image) return;
  }
  if (!rect) rect = static_cast<ztwod::Rect*>(std::calloc(1, sizeof(ztwod::Rect)));
  if (rect) *rect = {0, 0, image->width - 1, image->height - 1};
}

// Mech3.exe 1.2 @ 0x00458390
void MissionMap::unload_image() {
  if (!image) return;
  ztwod::host().image_release(image);  // 0x00517b10
  set_image(static_cast<zimg::Texture*>(nullptr));
}

// Mech3.exe 1.2 @ 0x004586e0
bool MissionMap::point(const float p[3], int32_t out[2]) const {
  const float dx = p[0] - offset[0], dz = p[2] - offset[2];
  out[0] = static_cast<int32_t>(-(dx * inv_mpp));  // 0x005ac340 (_ftol)
  out[1] = static_cast<int32_t>(-(dz * inv_mpp));
  return out[0] >= 0 && out[0] <= 0x280 && out[1] >= 0 && out[1] <= 0x280;
}

// Mech3.exe 1.2 @ 0x004587b0 (the lance command panel's pick on the map,
// 0x00469d00; playtest: the map pick ported 2026-10-30).
bool MissionMap::pick(float px, float py, float place[3], zcls::GroundHit* out,
                      int32_t (*ground)(float x, float y, float z, zcls::GroundHits* hits)) const {
  if (!ground) return false;
  const float m = mpp;
  place[0] = -(px * m) + offset[0];
  const float z = -(py * m) + offset[2];
  place[2] = z;
  zcls::GroundHits hits;
  hits.count = 0;
  if (ground(place[0], 1000.0f, z, &hits) == 0 && hits.count != 0) {
    place[1] = hits.hit[0].height;
    if (out) *out = hits.hit[0];
    return true;
  }
  place[1] = 0.0f;
  return false;
}

// Mech3.exe 1.2 @ 0x004583b0: the file's "maps" list, its items after the key.
bool maps_read(const char* file) {
  if (g_maps.read) return true;
  zrdr::Node* root = zrdr::load(file);
  zrdr::Node* maps = zrdr::find(root, "maps");
  if (!maps) return false;  // the exe returns before freeing the file
  const int32_t n = zrdr::list_count(maps);  // slot 0 the count, the maps after it
  for (int32_t i = 1; i < n; ++i) g_maps.list.push_back(new MissionMap(&maps->list[i]));
  g_maps.read = true;
  zrdr::free_node(root);
  return true;
}

// Mech3.exe 1.2 @ 0x004584e0
void maps_free() {
  if (!g_maps.read) return;
  for (MissionMap*& m : g_maps.list) delete m, m = nullptr;  // vt+0x00(1)
  g_maps.list.clear();
}

// Mech3.exe 1.2 @ 0x004582c0
void maps_unload() {
  for (MissionMap* m : g_maps.list) m->unload_image();
}

// Mech3.exe 1.2 @ 0x00458660
void maps_load_named(const char* s) {
  if (!s) return;
  for (MissionMap* m : g_maps.list)
    if (m->name && std::strstr(m->name, s)) m->load_image();
}

// Mech3.exe 1.2 @ 0x00458590
MissionMap* maps_find(const char* s) {
  for (MissionMap* m : g_maps.list) {
    if (!m->image) continue;
    if (!s || (m->name && std::strstr(m->name, s))) return m;
  }
  return nullptr;
}

// Mech3.exe 1.2 @ 0x004582a0 ([0x005e3eb4] "large").
MissionMap* maps_pick(const char* s) {
  maps_load_named(s);
  if (MissionMap* m = maps_find("large")) return m;
  return maps_find(nullptr);
}

}  // namespace mech3
