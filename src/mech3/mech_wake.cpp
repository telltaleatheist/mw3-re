#include "mech3/mech_wake.h"

#include <cmath>

#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"

namespace mech3 {
namespace {
WakeHooks g_hooks;
void set_active(zcls::Node* n, int on) {
  if (g_hooks.set_active) g_hooks.set_active(n, on);
  else zcls::node_set_active(n, on);
}
}  // namespace

WakeHooks& wake_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x0045f980 with the constructor 0x0045fa00: the node
// joins the world through the wrapper's vt+0 (0x0040bab0, node_add_child).
FootWake* foot_wake_new(zcls::Node* world, const char* weak, const char* mid, const char* strong) {
  zeff::AnimDef* a = zeff::anim_find(weak);
  zeff::AnimDef* b = zeff::anim_find(mid);
  zeff::AnimDef* c = zeff::anim_find(strong);
  if (!world) return nullptr;
  FootWake* w = new FootWake;
  w->node = zcls::object3d_create();
  zcls::node_set_name(w->node, "wake_node");  // 0x005e4f74
  w->anims[0] = a, w->anims[1] = b, w->anims[2] = c;
  zcls::node_add_child(world, w->node);
  return w;
}

// Mech3.exe 1.2 @ 0x0045fab0
void foot_wake_free(FootWake* w, zcls::Node* world) {
  if (!w) return;
  if (world) zcls::node_remove_child(world, w->node);
  foot_wake_stop(w);
  zcls::node_delete(w->node);
  delete w;
}

// Mech3.exe 1.2 @ 0x0045fa70
int32_t wake_level(float throttle) {
  if (!(throttle >= 0.2f)) return 0;  // fcomp, test ah, 1: NaN counts as below
  return !(throttle >= 0.5f) ? 1 : 2;
}

// Mech3.exe 1.2 @ 0x0045faf0
void foot_wake_start(FootWake* w, float throttle) {
  const int32_t level = wake_level(std::fabs(throttle));
  if (level == w->level) return;
  foot_wake_stop(w);
  set_active(w->node, 1);
  // 0x00508da0: attached to the node, no offset.
  w->playing = g_hooks.start ? g_hooks.start(w->anims[level], w->node)
                             : zeff::anim_start_connector(w->anims[level], nullptr, w->node, nullptr, nullptr);
  w->level = level;
}

// Mech3.exe 1.2 @ 0x0045fb50
void foot_wake_stop(FootWake* w) {
  if (!foot_wake_playing(w)) return;
  if (g_hooks.stop) g_hooks.stop(w->playing);
  else zeff::anim_stop(w->playing);
  w->playing = nullptr;
  w->level = -1;
  set_active(w->node, 0);
}

// Mech3.exe 1.2 @ 0x0045fb80
bool foot_wake_playing(const FootWake* w) { return w->playing != nullptr; }

}  // namespace mech3
