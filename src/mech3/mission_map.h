#pragma once
#include <cstdint>
#include <list>

#include "gamez/ztwod/twod.h"

namespace zrdr {
struct Node;
}

// C:\zipper\Mech3\mech_map.cpp (its reports): the mission maps (docs/hud.md
// section 25). maps.zrd's "maps" list is read once at the start (0x004583b0)
// into images not yet loaded; a mission's briefing loads the ones whose name
// holds its MAP (0x00458660) and picks one for the missions object's +0xbc,
// which the radar's map mode (Shift+R) and the waypoints draw.
namespace mech3 {

// A map (vtable 0x005c2308, 0xdc bytes): a zTwoD image of the mission seen
// from above, with what turns a world point into a pixel on it.
class MissionMap : public ztwod::Image {
 public:
  explicit MissionMap(zrdr::Node* block);  // 0x004580c0 (null: an empty map, 0x00491c80's)
  ~MissionMap() override;                   // 0x00458200 (vt+0x00 0x004581e0)
  void update(float dt) override { Object2D::update(dt); }  // +0x24  0x00458880
  void load_image();                        // 0x00458300: its bitmap, and the rect over it
  void unload_image();                      // 0x00458390
  // 0x004586e0: the pixel of a world point, -(x, z) from the offset over the
  // metres per pixel; false off 0..640 (the release's report is empty).
  bool point(const float p[3], int32_t out[2]) const;

  char* bitmap = nullptr;      // +0xbc  "bitmap" (strdup'd)
  char* name = nullptr;        // +0xc0  "name"
  float offset[3] = {};        // +0xc4  "offset", default [0x0063d870]
  float mpp = 0;               // +0xd0  "mpp", metres a pixel
  float inv_mpp = 0;           // +0xd4  1 / mpp (0 for 0)
  ztwod::Rect* rect = nullptr; // +0xd8  the image's (calloc'd when loaded)
};

// The list [0x00605acc] (count [0x00605ad0]) and the read flag [0x00605ad8].
struct MissionMaps {
  std::list<MissionMap*> list;
  bool read = false;
};
MissionMaps& mission_maps();

bool maps_read(const char* file);            // 0x004583b0: once
void maps_free();                            // 0x004584e0: every map deleted (the app's exit)
void maps_unload();                          // 0x004582c0: every map's image released (a mission's end)
void maps_load_named(const char* s);         // 0x00458660: the images of the maps whose name holds s
MissionMap* maps_find(const char* s);        // 0x00458590: a loaded one whose name holds s (null: any loaded)
MissionMap* maps_pick(const char* s);        // 0x004582a0: s's loaded, then "large", else the first loaded

}  // namespace mech3
