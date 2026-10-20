#pragma once
#include <cstdint>

#include "mech3/mech.h"

// MW3's side of mechlib.zbd (docs/mechlib.md, "MW3 side"): the library
// object, the mech types' models by name, and two voice tables loaded with
// it. Original file unknown: between mech3\mech_waynet.cpp and
// mech3\mission.cpp, named for what it holds.
namespace zcls {
struct MechLib;
struct Node;
}

namespace mech3 {

// The calls it makes outside the file; the defaults run the ports.
class MechLibHooks {
 public:
  virtual ~MechLibHooks() = default;
  virtual Handle sound_find(const char* name);  // 0x0056f6f0: zsnd::find
  virtual void load_textures(int32_t group);    // 0x00519080: zimg::load_group
};
void set_mechlib_hooks(MechLibHooks* h);        // null: the defaults

zcls::MechLib* mechlib();                        // [0x00609844], null until opened
// 0x00484270: the library made once and zbd\mechlib.zbd opened (its result
// returned), then the voice tables: 0x00608fc0 by equipment (1 ams .. 15 tc,
// damage.md) and 0x00609400 by weapon kind (1 ac .. 13 streak, 14 tag).
bool open_mechlib();
Handle equipment_voice(int32_t i);              // [0x00608fc0 + 4 i], 0..15
Handle weapon_voice(int32_t i);                 // [0x00609400 + 4 i], 0..14
// 0x00484740: a tree by entry name, null without the library.
zcls::Node* load_tree(const char* name);
// 0x00484760 (ECX roster, EDX variant): "mech_<name>.flt", or
// "mech_<name>_<variant>.flt", loaded with the texture group set to 3 and
// that group's textures loaded when found; the group is restored.
zcls::Node* mech_model(int32_t roster, int32_t variant);
zcls::Node* hud_model(int32_t roster);          // 0x004847e0: "mech_<name>_hud.flt"
// 0x004844b0, at the mission's unload and the shutdown: the library closed
// and freed (its trees are the world's), the definitions freed (0x0046b9b0)
// and the voice tables cleared.
bool close_mechlib();

}  // namespace mech3
