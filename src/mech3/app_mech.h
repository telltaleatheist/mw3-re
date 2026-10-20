#pragma once
#include <cstdint>
#include <initializer_list>

#include "gamez/zgame/unported.h"

// The mech build's and the launch's hooks wired to the program (app_mech.cpp,
// docs/mech-build.md section 9). Not original code. The engine's side comes
// as plain functions, so this header has neither of the two Handle types
// (app_hooks.h's numbers, mech.h's pointers).
namespace mech3 {

struct MechWiring {
  void* engine = nullptr;
  uint32_t (*unported)(void* e, const char* key, std::initializer_list<zgame::Arg> args) = nullptr;
  uint32_t (*peek32)(void* e, uint32_t a) = nullptr;
  void (*poke32)(void* e, uint32_t a, uint32_t v) = nullptr;
  void (*poke8)(void* e, uint32_t a, uint8_t v) = nullptr;
  void* (*object_of)(void* e, uint32_t h) = nullptr;
  uint32_t (*handle_of)(void* e, const void* p) = nullptr;
  uint32_t (*vcall)(void* e, uint32_t obj, uint32_t slot, std::initializer_list<zgame::Arg> args) = nullptr;
  uint32_t (*pilot)(void* e) = nullptr;   // [0x0060861c]
  uint32_t (*view)(void* e) = nullptr;    // [0x005f4c8c]
  const char* mechlib = "zbd/mechlib.zbd";  // the mech types' models, through the mounts
};
void wire_mech_build(const MechWiring* w);  // null: the hooks' defaults again

// What the last launch built (0x00491150), for the program's log and its tests.
struct LaunchedMech {
  int32_t roster = -1;     // the type
  int32_t parts = 0;       // hip, torso, head, arms, hands, legs, calves, feet with their nodes
  int32_t weapons = 0;     // mounted (the manager's count)
  int32_t sinks = 0;       // the heat sinks
  int32_t toes = 0;        // both feet's
  float height = 0;        // from the model's box
  float max_speed = 0;     // m/s, from the engine
  float torso_armor_scale = 0;  // the centre torso's 100 / armor points
  uint32_t team = 0;       // the AiTarget mask
  bool skis = false, ams = false;
};
bool launched_mech(LaunchedMech* out);  // false when the launch made none

// player.cpp's AI mechs (mission-start.md section 4): 0x00414150 on a
// controller the program made (a MechControls), the definition's skin
// (0x00414390), the mech's definition (0x0041e9a0). Pointers as void*.
void* build_ai_mech(int32_t mech, void* controls, int32_t config, const char* name);
bool set_mech_skin(void* mech, void* skin);
const void* mech_definition(void* mech);

// The debug launch's slot (our addition, mission-load.md section 4): the
// mech by roster index, its first configuration, the definition's skin 0.
uint32_t debug_slot_config(int32_t mech, void* e, uint32_t (*handle_of)(void* e, const void* p));

}  // namespace mech3
