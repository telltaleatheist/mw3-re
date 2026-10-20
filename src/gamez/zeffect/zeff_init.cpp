#include "gamez/zeffect/zeff_init.h"

#include <cstring>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"

namespace zeff {

namespace {
EffectHooks g_hooks;
bool g_loaded = false;               // 0x0073f3c0
int32_t g_count = 0;                 // 0x0073f3c8
std::vector<std::string> g_names;    // 0x0073f3d4 +8 each (0x4c a record): none are read
}  // namespace

EffectHooks& effect_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x0050d2c0 (ECX the world, EDX the camera, kept at
// 0x0073f3d8 / 0x0073f3dc). Once: the EFFECTS list's records. An entry's
// node, gfx data (0x0050d610) and texture cycles are not ported; no shipped
// file has one.
int32_t effects_init(const char* file) {
  if (g_loaded) return 0;
  zrdr::Node* root = zrdr::load(file);
  if (!root) {
    if (g_hooks.error) g_hooks.error(file);
    return -1;
  }
  zrdr::Node* list = zrdr::find(root, "EFFECTS");
  g_count = list ? zrdr::list_count(list) - 1 : 0;
  for (int32_t i = 0; i < g_count; ++i)
    if (g_hooks.unported) g_hooks.unported("0x0050d2c0 entry", i);
  g_loaded = true;
  return 0;
}

int32_t effect_count() { return g_count; }

// Mech3.exe 1.2 @ 0x0050d9e0
int32_t effect_find(const char* name) {
  for (int32_t i = 0; i < static_cast<int32_t>(g_names.size()); ++i)
    if (std::strcmp(name, g_names[i].c_str()) == 0) return i;
  return -1;
}

// Mech3.exe 1.2 @ 0x0050d650: a free copy of the effect (0x0050d820) placed
// (0x0050d6a0), shown and ticking. Only with a loaded table and an index.
void effect_spawn(int32_t index) {
  if (!g_loaded || index == -1) return;
  if (g_hooks.unported) g_hooks.unported("0x0050d650", index);
}

void effects_reset() {
  g_loaded = false;
  g_count = 0;
  g_names.clear();
}

}  // namespace zeff
