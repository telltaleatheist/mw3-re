// zWeapon's beams (docs/weapons.md, "Beams"). The file name is not in the
// exe; these open the module at 0x00581230.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "crt/msvc_rand.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zweapon/zweapon.h"

#pragma STDC FP_CONTRACT OFF

namespace zwep {

void beam_update(WeaponDef*) {}

}  // namespace zwep
