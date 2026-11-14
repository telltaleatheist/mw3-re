// zEffect's effect table for tests/differential/test_effects.py: effects.zrd's entries read (0x0050d2c0), spawns
// (0x0050d650), node ticks (0x0050d910) and the free (0x0050d570), every call out through the trace's callback.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "gamez/zeffect/zeff_init.h"
#include "gamez/zreader/zreader.h"
#include "probe_app.h"

using probe_app::call;
using zeff::EffectHandle;

namespace {
zrdr::Node* g_tree = nullptr;

struct Traced : zeff::EffectHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  zrdr::Node* load(const char*) override { return g_tree; }
  void report(const char* fmt, const char* a, const char* b) override { call("fprintf", {fmt, a, b}); }
} g_hooks;
}  // namespace

extern "C" {
// ops: 5 words each: 1 spawn (index, x, y, z as float bits), 2 tick (node), 3 free. Returns 0.
int32_t probe_effects_run(const uint8_t* zrd, int32_t len, uint32_t world, uint32_t camera, const uint32_t* ops,
                          int32_t nops) {
  std::FILE* f = std::tmpfile();
  std::fwrite(zrd, 1, size_t(len), f);
  std::rewind(f);
  g_tree = static_cast<zrdr::Node*>(std::calloc(1, sizeof(zrdr::Node)));
  zrdr::read_node(f, g_tree);
  std::fclose(f);
  zeff::set_effect_hooks(&g_hooks);
  zeff::effects_init();
  zeff::effects_init("effects.zrd", zeff::EffectHooks::p(world), zeff::EffectHooks::p(camera));
  for (int32_t i = 0; i < nops; ++i) {
    const uint32_t* o = ops + 5 * i;
    if (o[0] == 1) {
      float at[3];
      std::memcpy(at, o + 2, 12);
      zeff::effect_spawn(int32_t(o[1]), at);
    } else if (o[0] == 2) {
      zeff::effect_tick(zeff::EffectHooks::p(o[1]));
    } else if (o[0] == 3) {
      zeff::effects_free();
    }
  }
  zeff::effects_free();
  zeff::set_effect_hooks(nullptr);
  g_tree = nullptr;  // freed by effects_free (0x00556820)
  return 0;
}

// The table's entry i (index, flags, node, gfx) and, for a running node, its copy (t, scale, grow, life bits).
int32_t probe_effects_entry(int32_t i, uint32_t* out) {
  const zeff::Effect* e = zeff::effect_entry(i);
  if (!e) return 0;
  out[0] = uint32_t(e->index), out[1] = e->flags, out[2] = zeff::EffectHooks::h(e->node);
  out[3] = zeff::EffectHooks::h(e->gfx);
  return 1;
}
}
