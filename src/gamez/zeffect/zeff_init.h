#pragma once
#include <cstdint>

// zEffect's effect table (zeffect\zeff_init.c, 0x0050d2c0..0x0050da50): named
// sprite effects from effects.zrd (a node, its gfx data and texture cycles),
// spawned by index (0x0050d650). Both MW3 1.2 and Pirate's Moon ship an empty
// EFFECTS list, so the table stays empty: the entries' reading and the spawn
// are recorded hooks (docs/weapon-effects.md).
namespace zeff {

struct EffectHooks {
  void (*unported)(const char* key, int32_t arg) = nullptr;  // an entry read, or a spawn of one
  void (*error)(const char* text) = nullptr;                 // the "Failed to read" report
};
EffectHooks& effect_hooks();

int32_t effects_init(const char* file);   // 0x0050d2c0: 0, or -1 when the file is missing
int32_t effect_count();                   // [0x0073f3c8]
int32_t effect_find(const char* name);    // 0x0050d9e0: the index, -1 none
void effect_spawn(int32_t index);         // 0x0050d650 (ECX the index, -1 none)
void effects_reset();                     // the table forgotten (between missions in the tests)

}  // namespace zeff
