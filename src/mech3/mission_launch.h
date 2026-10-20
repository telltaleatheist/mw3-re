#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"
#include "mech3/mech.h"

// C:\zipper\Mech3\mission.cpp 0x00491150: the launch, from the briefing's
// Launch (0x0040500a): the player's controls and mech from the pilot's game
// slot, the view on it, the cockpit's cycles, then per game type the rest
// of the line-up (docs/mech-build.md section 8). Apart from mission.h
// because its objects are the mech's (mech.h's Handle is a pointer). Every
// call out of mission.cpp is a LaunchHooks method named by address; the mech
// is built by mech3/mech_build, whose hooks it shares.
namespace mech3 {

namespace ai { class AivRecord; }

class LaunchHooks : public zgame::Unported {
 public:
  static uint32_t low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  static Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
  // Silent: objects the port does not model, by their fields.
  virtual Handle missions() { return nullptr; }               // 0x006098d0
  virtual Handle pilot() { return nullptr; }                  // [0x0060861c]
  virtual Handle view() { return nullptr; }                   // [0x005f4c8c]
  virtual Handle interp() { return nullptr; }                 // [0x006040b0]
  virtual uint32_t field32(Handle obj, uint32_t off) { (void)obj, (void)off; return 0; }
  virtual void set_field32(Handle obj, uint32_t off, uint32_t v) { (void)obj, (void)off, (void)v; }
  virtual void set_field8(Handle obj, uint32_t off, uint8_t v) { (void)obj, (void)off, (void)v; }
  virtual Config* config_of(uint32_t handle) { return reinterpret_cast<Config*>(h(handle)); }  // a slot's +0x08
  virtual Handle rec_ref(const ai::AivRecord* rec) { return const_cast<ai::AivRecord*>(rec); }
  virtual void clear_60b570() {}                              // [0x0060b570] = 0

  virtual void fn_491d20(Handle missions) { unported("0x00491d20", {low(missions)}); }
  virtual ai::AivRecord* aiv_read_named(ai::AivRecord& rec, const char* name);  // 0x004e0680
  // The options (mech3/options) and the pilot's game.
  virtual int32_t use_joystick() { return static_cast<int32_t>(unported("0x0040d1b0", {})); }
  virtual int32_t camera_mode() { return static_cast<int32_t>(unported("0x0040ce70", {})); }
  virtual void set_camera_mode(int32_t m) { unported("0x0040ce10", {m}); }
  virtual Handle game_slot(Handle game) { return h(unported("game.vt+0x34", {low(game), 0})); }
  virtual int32_t game_type(Handle game) { return static_cast<int32_t>(unported("game.vt+0x2c", {low(game)})); }
  // The mech's view, HUD and AI target (not ported).
  virtual void mech_attach_view(Mech& m);                     // vt+0x108, 0x0041ce30
  virtual void view_set_camera(Handle view, uint32_t mode) { unported("0x0040ae70", {low(view), mode}); }
  virtual void camera_update(Handle camera) { unported("0x004f3430", {low(camera), 1}); }
  virtual void pilot_set_mech(Handle pilot, Mech& m);         // 0x004782c0
  virtual void pilot_place(Handle pilot, const float pos[3], uint32_t yaw) {
    unported("0x004782f0", {low(pilot), zgame::bits(pos[0]), zgame::bits(pos[1]), zgame::bits(pos[2]), yaw});
  }
  virtual void interp_set_mech(Handle interp, Mech& m);       // 0x004108a0
  virtual Handle mech_screen(Pilot* data);                    // 0x00430aa0
  virtual void hud_reset(Handle screen) { unported("hud.vt+0x64", {low(screen) + 0x17964}); }
  virtual void hud_zoom(Handle screen, float v) { unported("hud.vt+0x60", {low(screen) + 0x17964, zgame::bits(v)}); }
  virtual void controls_set_record(MechControls* c, const ai::AivRecord* rec);  // IControls vt+0x88
  virtual void interp_set_cycle_node(Handle interp, Handle node) { set_field32(interp, 0xb8, low(node)); }
  virtual uint8_t interp_source(Handle interp, const char* path) {
    return static_cast<uint8_t>(unported("0x00595990", {low(interp), path}));
  }
  virtual void textures_load() { unported("0x005190d0", {}); }
  // The rest of the line-up, by game type.
  virtual void lancemates() { unported("0x0049e5e0", {}); }
  virtual void pilot_478f10(Handle pilot) { unported("0x00478f10", {low(pilot), 0}); }
  virtual void instant_action() { unported("0x0049dc80", {1}); }
  virtual void objectives(Handle a, Handle b, Handle c, Handle d) {
    unported("0x0045c4a0", {low(a), low(b), low(c), low(d)});
  }
  virtual void multiplayer(Handle pilot) { unported("0x0046da80", {low(pilot)}); }
  virtual void pilot_478ce0(Handle pilot) { unported("0x00478ce0", {low(pilot)}); }
  virtual uint8_t pilot_4794f0(Handle pilot) { return static_cast<uint8_t>(unported("0x004794f0", {low(pilot)})); }
  virtual void pilot_vt4(Handle pilot) { unported("pilot.vt+0x4", {low(pilot)}); }
  virtual void close_handle(uint32_t handle) { unported("CloseHandle", {handle}); }
  virtual void target_4d6d20(Mech& m, uint32_t v);            // 0x004d6d20 (AiTarget)
  virtual void hud_43d920(Handle hud) { unported("0x0043d920", {low(hud)}); }
  virtual void free(Handle p) { unported("delete", {low(p)}); }
  virtual void fn_40f0a0() { unported("0x0040f0a0", {}); }
  virtual void fn_537520() { unported("0x00537520", {}); }
};

void set_launch_hooks(LaunchHooks* h);
LaunchHooks& launch_hooks();

// Mech3.exe 1.2 @ 0x00491150, on the missions object: false (and no mech)
// without a `player` record in aiv.zrd or when the mech is not made.
bool launch_player();
Mech* launched_mech();  // the mech the last launch made (the pilot's +0x14 in the exe)

}  // namespace mech3
