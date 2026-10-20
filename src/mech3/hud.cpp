#include "mech3/hud.h"

#include <cstdio>

#include "crt/msvc_math.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zutil/zfix.h"
#include "gamez/ztwod/twod_host.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
DamageSet g_damage;
HudHooks g_default_hooks;
HudHooks* g_hooks = &g_default_hooks;

// The parts' images by the table's order (0x00605888 + 0x20 i) and where each
// sits in the silhouette.
struct PartImages {
  int index;
  const char* names[5];
  int32_t x, y;
};
// In the constructor's order: head, ltorso, rtorso, ctorso, larm, rarm, lleg, rleg.
constexpr PartImages kParts[8] = {
    {5, {"dmg_head_blue", "dmg_head_grn", "dmg_head_yel", "dmg_head_red", "dmg_head_blk"}, 0x1e, 2},
    {4, {"dmg_ltorso_blue", "dmg_ltorso_grn", "dmg_ltorso_yel", "dmg_ltorso_red", "dmg_ltorso_blk"}, 0x2e, 6},
    {0, {"dmg_rtorso_blue", "dmg_rtorso_grn", "dmg_rtorso_yel", "dmg_rtorso_red", "dmg_rtorso_blk"}, 0x13, 6},
    {3, {"dmg_ctorso_blue", "dmg_ctorso_grn", "dmg_ctorso_yel", "dmg_ctorso_red", "dmg_ctorso_blk"}, 0x1f, 0xf},
    {6, {"dmg_larm_blue", "dmg_larm_grn", "dmg_larm_yel", "dmg_larm_red", "dmg_larm_blk"}, 0x3a, 8},
    {2, {"dmg_rarm_blue", "dmg_rarm_grn", "dmg_rarm_yel", "dmg_rarm_red", "dmg_rarm_blk"}, 2, 8},
    {1, {"dmg_lleg_blue", "dmg_lleg_grn", "dmg_lleg_yel", "dmg_lleg_red", "dmg_lleg_blk"}, 0x27, 0x2a},
    {7, {"dmg_rleg_blue", "dmg_rleg_grn", "dmg_rleg_yel", "dmg_rleg_red", "dmg_rleg_blk"}, 7, 0x2a},
};
}  // namespace

DamageSet& damage_set() { return g_damage; }
HudHooks& hud_hooks() { return *g_hooks; }
void set_hud_hooks(HudHooks* h) { g_hooks = h ? h : &g_default_hooks; }

void HudHooks::piece(uint32_t ctor, std::initializer_list<zgame::Arg> args) {
  char key[16];
  std::snprintf(key, sizeof key, "0x%08x", ctor);
  unported(key, args);
}

PendingPiece::PendingPiece(uint32_t ctor, std::initializer_list<zgame::Arg> args) : Object2D(0, 0) {
  g_hooks->piece(ctor, args);
}

HudTimer::HudTimer() : FontText("00:00:00.00", 0x244, 0xf) { set_font(ztwod::host().font_find("mech_string")); }

// Mech3.exe 1.2 @ 0x0043ae50: a null name keeps the slot.
void DamageImages::set(const char* const names[5], int32_t px, int32_t py) {
  for (int i = 0; i < 5; ++i)
    if (names[i]) image[i] = ztwod::host().image_get(names[i]);
  x = px, y = py;
}

// Mech3.exe 1.2 @ 0x0043afc0 (also inlined in the constructor): once; the
// background size from the first background found.
void load_damage_set() {
  DamageSet& d = g_damage;
  if (d.loaded) return;
  for (const PartImages& p : kParts) d.parts[p.index].set(p.names, p.x, p.y);
  d.bg[0] = ztwod::host().image_get("dmg_bg_blue");
  d.bg[1] = ztwod::host().image_get("dmg_bg_red");
  d.bg[2] = ztwod::host().image_get("dmg_bg_grn");
  const zimg::Texture* t = d.bg[0] ? d.bg[0] : d.bg[1] ? d.bg[1] : d.bg[2];
  if (t) d.bg_w = t->width, d.bg_h = t->height;
  d.loaded = true;
}

// Mech3.exe 1.2 @ 0x0043f3e0
HudCompass::HudCompass()
    : Object2D(0, 0), tape("hud_compass", 0xf5, 0x13, 0), frame("hud_compass_frame", 0xf5, 0xb, 0), text(nullptr, 0, 0) {
  half = (frame.image ? frame.image->width : 0) / 2;
  src.top = 0;
  src.bottom = tape.image ? tape.image->height : 0;
  set_heading(0.0f);
  tape.src = &src;
  show_for(-1.0f);
}

// Mech3.exe 1.2 @ 0x0043f590: the tape's window around the heading, turned
// half round on a flipped map.
void HudCompass::set_heading(float h) {
  HudHooks& k = *g_hooks;
  if (k.mission_entry() && k.entry_flip(k.mission_entry()) > 0.0f) h = h - 3.1415927f;  // 0x005c0100
  while (h < 0.0f) h = h - -6.2831855f;            // 0x005c0104
  const int32_t x = crt::msvc_ftol(scale * h) + centre;
  src.right = x + half;
  src.left = x - half;
}

// Mech3.exe 1.2 @ 0x0043b190. The pieces are made in the members' order
// (the ones not ported yet are recorded calls).
Hud::Hud(Mech* m) {
  HudHooks& h = *g_hooks;
  if (zfix::options().bugs) block = nullptr;
  load_damage_set();
  mech = nullptr;
  h.setup(this, m);
  add(&compass);
  add(&gauges);
  add(&target);
  add(&mfd);
  add(&weapons);
  add(&equipment);
  timer.show_for(-1.0f);
}

}  // namespace mech3
