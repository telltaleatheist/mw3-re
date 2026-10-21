#include "mech3/mission_launch.h"

#include <cstring>

#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech3_interp.h"
#include "mech3/mech_build.h"
#include "mech3/mech_control.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_skin.h"
#include "mech3/weapon.h"
#include "mech3/zrdraiv.h"

// docs/mech-build.md section 8.
namespace mech3 {

void player_clear_60b56c();  // mech3/player.h

namespace {

LaunchHooks g_default_hooks;
LaunchHooks* g_hooks = &g_default_hooks;
Mech* g_launched = nullptr;

}  // namespace

void set_launch_hooks(LaunchHooks* h) { g_hooks = h ? h : &g_default_hooks; }
LaunchHooks& launch_hooks() { return *g_hooks; }
Mech* launched_mech() { return g_launched; }

ai::AivRecord* LaunchHooks::aiv_read_named(ai::AivRecord& rec, const char* name) { return rec.read_named(name); }
void LaunchHooks::mech_attach_view(Mech& m) { unported("0x0041ce30", {mech_build_hooks().ref(&m)}); }
void LaunchHooks::pilot_set_mech(Handle pilot, Mech& m) {
  unported("0x004782c0", {low(pilot), mech_build_hooks().ref(&m)});
}
void LaunchHooks::interp_set_mech(Handle interp, Mech& m) {
  unported("0x004108a0", {low(interp), mech_build_hooks().ref(&m)});
}
Handle LaunchHooks::mech_screen(MechData* data) { return h(unported("0x00430aa0", {mech_build_hooks().ref(data)})); }
void LaunchHooks::controls_set_record(MechControls* c, const ai::AivRecord* rec) {
  unported("ictl.vt+0x88", {mech_build_hooks().controls_ref(c), low(rec_ref(rec))});
}
void LaunchHooks::mech_update(Mech& m, float dt) { unported("mech.vt+0x34", {mech_build_hooks().ref(&m), zgame::bits(dt)}); }
void LaunchHooks::target_4d6d20(Mech& m, uint32_t v) { unported("0x004d6d20", {mech_build_hooks().ref(&m), v}); }

// Mech3.exe 1.2 @ 0x004782f0. Detail bit 0 (the sounds) is off for the
// update; the mech then starts the mission powered down, the HUD's saved
// state 1.
int32_t place_pilot_mech(LaunchHooks& h, Handle pilot, Mech& m, const float pos[3], uint32_t yaw) {
  float angle;
  std::memcpy(&angle, &yaw, 4);
  h.mech_place(m, pos, angle);
  m.invincible = h.cheat_invincible() != 0 || h.option_invincible() != 0;
  m.detail &= ~1u;
  h.mech_update(m, 1.0f / 30.0f);  // 0x3d088889
  m.detail |= 1u;
  m.power_down_pending = false;
  m.powered_down = true;
  m.hud_saved = 1;
  if (auto* w = static_cast<WeaponManager*>(m.ammo)) w->mode = static_cast<int32_t>(h.field32(pilot, 0x80));
  return 1;
}

// Mech3.exe 1.2 @ 0x00491150. The exe first closes the loader thread's
// handles (+0x68, +0x6c) and gives the game thread its priority back; the
// port loads without a thread (FIX (A), mission-load.md section 4).
bool launch_player() {
  LaunchHooks& h = *g_hooks;
  MechBuildHooks& b = mech_build_hooks();
  const Handle missions = h.missions();
  h.fn_491d20(missions);
  ai::AivRecord rec;
  const ai::AivRecord* r = h.aiv_read_named(rec, "player");
  float pos[3] = {0, 0, 0};  // 0x0063d870
  if (!r) return false;
  pos[0] = r->get<float>(0x08), pos[1] = r->get<float>(0x0c), pos[2] = r->get<float>(0x10);
  const uint32_t yaw = r->get<uint32_t>(0x14);
  const uint32_t team = r->get<uint32_t>(0x18);
  const uint32_t mode = h.use_joystick() == 1 ? 6 : 3;
  const int32_t cam = h.camera_mode();
  PlayerControls* ctl = player_controls(mode);
  const Handle pilot = h.pilot();
  const Handle game = h.h(h.field32(pilot, 0x6c));
  const Handle slot = h.game_slot(game);
  const int32_t index = static_cast<int32_t>(h.field32(slot, 0x04));
  MechData* data = b.mech_data(index, 1);
  Mech* m = mech_new(data, ctl, h.config_of(h.field32(slot, 0x08)), "player");
  g_launched = m;
  if (!m) {
    b.message_box(0xf30, mech_name(index));
    return false;
  }
  const int32_t skin = static_cast<int32_t>(h.field32(slot, 0x10));
  // The slot's skin (+0x10) of the definition's; a definition without skins
  // reads through null in the exe (every shipped one has them).
  if (const SkinSet* skins = b.dfn(m->pilot->roster)->skins) {
    const int32_t n = static_cast<int32_t>(skins->skins.size());
    if (skin < n && skin >= 0 && skins->skins[skin]) mech_set_skin(*m, skins->skins[skin]);
  }
  // The team masks, as player.cpp sets them (0x0049f924).
  m->team = 1u << (team & 31);
  for (uint32_t t = team >> 5; t; t >>= 5) m->team |= 1u << (t & 31);
  m->allies = m->team & ~1u;
  m->enemies = ~m->allies & ~1u;
  player_clear_60b56c();
  mech_control_mode(*m, mode);
  h.mech_attach_view(*m);
  h.view_set_camera(h.view(), static_cast<uint32_t>(cam));
  h.camera_update(h.h(h.field32(h.view(), 0x04)));
  h.pilot_set_mech(pilot, *m);
  h.pilot_place(pilot, pos, yaw);
  const int32_t zone = zmod::zones().mode;  // [0x005ea1d0]: every node passes while it attaches again
  zmod::zones().mode = 0;
  h.mech_attach_view(*m);
  zmod::zones().mode = zone;
  h.view_set_camera(h.view(), static_cast<uint32_t>(cam));
  h.camera_update(h.h(h.field32(h.view(), 0x04)));
  h.interp_set_mech(h.interp(), *m);
  h.hud_reset(h.mech_screen(m->pilot));
  h.controls_set_record(m->controls, r);
  h.set_camera_mode(h.camera_mode());
  const int32_t type = h.game_type(game);
  // The cockpit's animated parts: each cpit_ani<x> node found runs its cycle script.
  if (const int32_t* cockpit = m->pilot->x228) {
    const Handle root = h.h(static_cast<uint32_t>(cockpit[1]));
    static const char* const kNodes[3] = {"cpit_ania", "cpit_anib", "cpit_anic"};
    static const char* const kScripts[3] = {"support\\cpit_cycleA.gw", "support\\cpit_cycleB.gw",
                                            "support\\cpit_cycleC.gw"};
    bool any = false;
    for (int i = 0; i < 3; ++i) {
      if (const Handle n = b.node_find(root, kNodes[i])) {
        h.interp_set_cycle_node(h.interp(), n);
        h.interp_source(h.interp(), kScripts[i]);
        any = true;
      }
    }
    if (any) h.textures_load();
  }
  h.set_field8(pilot, 0x59, type != 3 && (r->get<uint8_t>(0xcc) & 0x20) ? 1 : 0);
  h.clear_60b570();
  switch (type) {
    case 1:
      h.lancemates();
      h.pilot_478f10(pilot);
      break;
    case 2:
    case 3: {
      if (type == 2) {
        h.instant_action();
        const uint8_t* o = static_cast<const uint8_t*>(missions);
        h.objectives(h.h(h.low(o + 0x7c)), h.h(h.low(o + 0x80)), h.h(h.low(o + 0x84)), h.h(h.low(o + 0x88)));
      } else {
        h.multiplayer(pilot);
      }
      h.pilot_478ce0(pilot);
      if (h.pilot_4794f0(pilot)) h.pilot_vt4(pilot);
      const uint32_t handle = h.field32(pilot, 0x10);
      if (handle != 0xffffffff) h.close_handle(handle);
      h.set_field32(pilot, 0x10, 0xffffffff);
      break;
    }
    case 4: {
      const Handle player = h.h(h.field32(pilot, 0x14));
      const Handle screen = player == h.h(h.field32(h.view(), 0x64)) ? h.mech_screen(m->pilot) : nullptr;
      if (screen) h.hud_zoom(screen, -1.0f);
      break;
    }
    default:
      break;
  }
  h.target_4d6d20(*m, zgame::bits(m->sensor_range));
  {
    const Handle player = h.h(h.field32(pilot, 0x14));
    const Handle screen = player == h.h(h.field32(h.view(), 0x64)) ? h.mech_screen(m->pilot) : nullptr;
    // FIX (bug), behind `bugs`: with the view on another vehicle the exe
    // passes 0x14 (the missing HUD + 0x14) and writes through it (a crash);
    // there is no HUD to reset.
    if (screen || !zfix::options().bugs) h.hud_43d920(h.h(h.low(screen) + 0x14));
  }
  // The list at +0xc0: every entry's object freed, then every entry.
  const Handle list = h.h(h.field32(missions, 0xc0));
  const Handle head = h.h(h.field32(list, 0x08));
  for (Handle n = h.h(h.field32(head, 0)); n != head; n = h.h(h.field32(n, 0))) {
    h.free(h.h(h.field32(n, 0x08)));
    h.set_field32(n, 0x08, 0);
  }
  for (Handle n = h.h(h.field32(head, 0)); n != head;) {
    const Handle next = h.h(h.field32(n, 0)), prev = h.h(h.field32(n, 4));
    h.set_field32(prev, 0, h.low(next));
    h.set_field32(next, 4, h.low(prev));
    h.free(n);
    h.set_field32(list, 0x0c, h.field32(list, 0x0c) - 1);
    n = next;
  }
  h.fn_40f0a0();
  h.fn_537520();
  return true;
}

}  // namespace mech3
