// Probes for test_gfx_flags.py (docs/render-lighting.md, the GfxFlags):
// zModel's and zRender's GfxFlags pointer (zmod gfx_flags_start /
// use_gfx_flags: 0x005266f0, 0x00526750; zrndr: 0x005578f0's part,
// 0x00557ae0) over settings entries the test makes on both sides.
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>

#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zrender/zrndr_draw.h"
#include "gamez/zutil/zutl_reg.h"

namespace {
std::deque<int32_t> g_values;  // the entries' storage, by the order made
std::deque<std::string> g_names;

// Where `p` points: an entry's value by its index, -1 the base.
int32_t which(const uint32_t* p, const uint32_t* base) {
  if (p == base) return -1;
  for (size_t i = 0; i < g_values.size(); ++i)
    if (p == reinterpret_cast<const uint32_t*>(&g_values[i])) return static_cast<int32_t>(i);
  return -2;
}
}  // namespace

extern "C" {

void probe_gf_reset() {
  zreg::init("probe", "gfx", "1", zreg::Machine{}, 0);  // so that the shutdown empties the list
  zreg::shutdown();
  g_values.clear(), g_names.clear();
  zmod::lighting().mask_base = 0, zmod::lighting().mask = &zmod::lighting().mask_base;
  zrndr::gfx_flags().base = 0, zrndr::gfx_flags().flags = &zrndr::gfx_flags().base;
}
void probe_gf_entry(const char* name, int32_t value) {
  g_names.emplace_back(name);
  g_values.push_back(value);
  zreg::create(g_names.back().c_str(), zreg::kDword, 4, zreg::kNotSaved, &g_values.back());
}
// op 0 zModel's start, 1 zRender's (hardware), 2 zModel's use, 3 zRender's
// (name or null, flags). out: zModel's target, base, value; zRender's.
void probe_gf_op(int32_t op, int32_t hardware, const char* name, uint32_t flags, int32_t* out) {
  switch (op) {
    case 0: zmod::gfx_flags_start(hardware != 0); break;
    case 1: zrndr::gfx_flags_start(hardware != 0); break;
    case 2: zmod::use_gfx_flags(name, flags); break;
    default: zrndr::use_gfx_flags(name, flags); break;
  }
  const zmod::Lighting& l = zmod::lighting();
  const zrndr::GfxFlags& g = zrndr::gfx_flags();
  out[0] = which(l.mask, &l.mask_base), out[1] = static_cast<int32_t>(l.mask_base), out[2] = static_cast<int32_t>(*l.mask);
  out[3] = which(g.flags, &g.base), out[4] = static_cast<int32_t>(g.base), out[5] = static_cast<int32_t>(*g.flags);
}

}  // extern "C"
