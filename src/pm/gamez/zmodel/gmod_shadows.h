#pragma once

#include "pm/gamez/zmodel/gmod_const.h"

// MakeShadows' builder on PM's models (MW3's in zmod's gmod_shadows; ported for preservation, the engine's entry
// left null): the same walk hands it an object's PM model.
namespace pm::zmod {

void shadow_model(Model& m);  // 0x00563760 (MW3's 0x00548250)

}  // namespace pm::zmod
