#pragma once
#include "gamez/zinterp/zinterp.h"

// MW3's interpreter (docs/interp.md, "MW3 hooks"): the GameZ interpreter with
// MW3's pre and error hooks, its registered variables and the current mech.
// Mech3.exe 0x410700..0x4108f0, between briefing.cpp and mech.cpp; the file's
// name is unknown.
namespace mech3 {

struct InterpGlobals {
  float gravity = 30.0f;               // 0x005e5d34
  float inv_gravity = 0.0333333351f;   // 0x005e5d38
  float damage_scale = 0.0062f;        // 0x005e1f68, docs/damage.md
};
InterpGlobals& interp_globals();

zint::Interp*& interp();                                   // 0x006040b0
const zint::Vtable& interp_vtable();                       // 0x005bccc8
void interp_create(zint::Interp& in);                      // 0x00410700
bool interp_pre(zint::Interp& in, const char* argv0);      // 0x004107e0
bool interp_on_error(zint::Interp& in, const char* argv0); // 0x00410770
// The app constructor's part (0x0047f2e0): `gravity` and `ceiling`.
void interp_register_world_vars(zint::Interp& in, float* ceiling);
// 0x004108a0. Mechs are not ported: the caller passes the mech (null: the
// player's, [0x0060861c]+0x14, already looked up) and its node (mech +4).
bool interp_set_mech(zint::Interp& in, void* mech, zcls::Node* node);

}  // namespace mech3
