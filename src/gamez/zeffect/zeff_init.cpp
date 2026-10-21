#include "gamez/zeffect/zeff_init.h"

#include <cstring>
#include <string>
#include <vector>

#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zreader/zreader.h"

namespace zeff {

namespace {
EffectHooks g_hooks;
bool g_loaded = false;               // 0x0073f3c0
int32_t g_count = 0;                 // 0x0073f3c8
zrdr::Node* g_root = nullptr;        // 0x0073f3cc  the file, kept
std::vector<std::string> g_names;    // 0x0073f3d4 +8 each (0x4c a record): none are read
}  // namespace

EffectHooks& effect_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x0050d2c0 (ECX the world, EDX the camera, kept at
// 0x0073f3d8 / 0x0073f3dc). Once: the EFFECTS list's records. An entry's
// node, gfx data (0x0050d610) and texture cycles are not ported; no shipped
// file has one.
int32_t effects_init(const char* file) {
  if (g_loaded) return 0;
  zrdr::Node* root = g_root = zrdr::load(file);
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

// Mech3.exe 1.2 @ 0x0050d270: the table cleared, then zEffect's animation
// state started (0x00508fc0).
void effects_init() {
  g_loaded = false;
  g_count = 0;
  g_root = nullptr;
  g_names.clear();
  anim_init();
}

// Mech3.exe 1.2 @ 0x0050d570: the file and the table freed (the spawned
// effects' list 0x0073f3d0 is never filled: no shipped effect), then 0x0050d270.
void effects_free() {
  if (g_root) zrdr::free_node(g_root);
  g_root = nullptr;
  g_names.clear();
  effects_init();
}

// Mech3.exe 1.2 @ 0x0050d2b0, at the game's end.
void effects_shutdown() {
  effects_free();
  anim_shutdown();
}

}  // namespace zeff
