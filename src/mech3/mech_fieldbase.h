#pragma once
#include <cstdint>
#include <list>

#include "gamez/zgame/unported.h"
#include "gamez/zsiscript/si_script.h"
#include "mech3/veh_basic_object.h"

namespace zrdr {
struct Node;
}
namespace mech3::ai {
struct Valve;
class AiParams;
}

// The mobile field base (aiv kind 2, vehicle.zrd's `mobilehq`; docs/vehicles.md,
// The mobile field base): a basic vehicle with arms and covers opened by a
// motion script, a list of queued steps (the repair dock's, item 4b), and the
// field bases' registry at 0x00605ae0. Original file unknown: these sit between
// mech3\mech_map.cpp and mech3\mech_objective.cpp (0x004587b0..0x0045a270).
namespace mech3 {

// A queued step (vtable 0x005c2540 is the dock's drive, 0x004598a0; item 4b).
struct FieldBaseAction {
  virtual ~FieldBaseAction() = default;
  virtual void begin() = 0;           // vt+0x00
  virtual bool step(float dt) = 0;    // vt+0x04: true marks it running
  virtual void after(float dt) = 0;   // vt+0x08
  virtual void end() = 0;             // vt+0x0c
  virtual bool done() = 0;            // vt+0x18
  bool running = false;               // +0x04
};

// The arms' motion script (0x90 bytes, vtable 0x005c2388; 0x00458890): the
// script "mobilehq", its time running from `now` toward `target` (0 shut,
// 1 open) at 0.2 a second.
struct FieldBaseArms : zsis::Script {
  float target = 0;  // +0x88  (0x004589a0)
  float now = 0;     // +0x8c
  ~FieldBaseArms();  // 0x005a9dc0
  void aim(bool open) { target = open ? 1.0f : 0.0f; }  // 0x004589a0
  bool step(float dt);  // 0x00458900: false when there (or not a number)
};

// 0x160 bytes, vtable 0x005c23d8: the basic vehicle's but for vt+0x00 (the
// destructor 0x00458b30), vt+0x34 (the frame 0x00459640), vt+0x70 (the death
// 0x004590b0), vt+0xf0 / vt+0xf4 (itself, 0x004d2a70: a field base; the
// basic vehicle's 0), vt+0x158 (the setup 0x00458d40), vt+0x160 (the damage
// 0x00458f50).
struct FieldBase : BasicVehicle {
  std::list<FieldBaseAction*> actions;  // +0xfc..+0x107  (the allocator byte, the head, the count)
  bool begin_next = false;              // +0x108  the front one to begin
  Handle nodes[7] = {};                 // +0x120  arm01..04, cover_left, cover_right, cover_middle
  FieldBaseArms* arms = nullptr;        // +0x13c
  zsis::Part* parts[7] = {};            // +0x140  the script's parts by the same names
  Handle anim = nullptr;                // +0x15c  the registry's animation (0x00458cf0, 4b)
};

// The field bases' registry (the static at 0x00605ae0, made by 0x00459b80;
// the repair dock's states around it are item 4b): up to three bases, each
// with its valve, by the node's name, set to 0; the dock's radio lines.
struct FieldBases {
  int32_t count = 0;                   // +0x10
  FieldBase* bases[3] = {};            // +0x14
  ai::Valve* valves[3] = {};           // +0x20
  // +0x8c..+0xb4: mcb_friendly_fire, mcb_under_attack, mcb_lost_vehicle,
  // mcb_occupied, mcb_pos_wait, mcb_pos_cancel, mcb_contact, mcb_waiting,
  // mcb_deploy, mcb_abort, mcb_complete.
  Handle sounds[11] = {};
  Handle effects = nullptr;            // [0x00605c44]  the animation "mfbeffects"
  int32_t alive() const;               // 0x0045a000: those still active
  bool add(FieldBase& b);              // 0x0045a030
  void clear();                        // 0x00459e50
};
FieldBases& field_bases();
// [0x0060b4d0]: the dock lets the bases drive (set 1 by 0x0045a780; 0 at the
// mission start). Clear, a base's frame holds its throttle at 0.
bool& field_bases_drive();
// [0x00736584], the scripts' loose-file path: made by the mechs' set-up
// (0x00413e60) or the first base and never trimmed, so each base adds the
// path again. The port's motions come from motion.zbd only.
bool& field_bases_path_made();
// Mech3.exe 1.2 @ 0x00459e80, its registry part: emptied, the radio lines
// and the effects found, the bases held. The dock's states and valve
// (+0x04, +0x84, +0xbc..) are step 4b.
void field_bases_mission_start();

// The calls out the basic vehicle's hooks do not cover.
class FieldBaseHooks : public zgame::Unported {
 public:
  static uint32_t low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  static Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
  virtual uint32_t ref(const void* p) { return low(p); }
  virtual void on_new(uint32_t size, const void* p) { (void)size, (void)p; }
  // The basic vehicle's own, called directly in the exe (their ports by
  // default; tests stub them): the constructor 0x004ac530, the setup
  // 0x0049ec00, the damage 0x004ad3f0, the death 0x004ad2d0, the destructor
  // 0x004ac650, the motion animation's step 0x004addc0; a script part's
  // pose (0x005aafe0, 0x005ab1e0) and lookup (0x005aa270, exact).
  virtual void base_construct(BasicVehicle& v, Handle ctl, Handle node) { vehicle_construct(v, ctl, node); }
  virtual void base_setup(BasicVehicle& v, zrdr::Node* def, const char* name, int32_t kind) {
    vehicle_setup(v, def, name, kind);
  }
  virtual int32_t base_damage(BasicVehicle& v, Handle weapon, uint32_t x, float amount) {
    return vehicle_damage(v, weapon, x, amount);
  }
  virtual void base_die(BasicVehicle& v) { vehicle_die(v); }
  virtual void base_destroy(BasicVehicle& v) { vehicle_destroy(v); }
  virtual void motion_step(BasicVehicle& v, float throttle);
  virtual void part_translation(zsis::Part* p, float t, float out[3]) { p->translation(t, out); }
  virtual void part_rotation(zsis::Part* p, float t, float out[4]) { p->rotation(t, out); }
  virtual zsis::Part* arms_part(FieldBaseArms& a, const char* name) { return a.find_part(name, true); }
  // The game type (game.vt+0x2c: 1 the campaign, 3 multiplayer) and the
  // campaign's field bases left (0x00479440, +0xa8).
  virtual int32_t game_type() { return static_cast<int32_t>(unported("game.vt+0x2c", {})); }
  virtual int32_t campaign_bases() { return static_cast<int32_t>(unported("0x00479440+0xa8", {})); }
  virtual ai::Valve* valve_get(const char* name);  // 0x004d3c40 (zai_valve)
  virtual void valve_set(ai::Valve* v, int32_t value);  // 0x004d3c00
  // The setup's: zClass's tree search (0x004f9d60), the scripts' file path
  // (0x00576100 the first, 0x00576140 one more; [0x00736584]), the script
  // by name (0x005a9990: 30 frames a second), the AI controls' brain
  // (IControls vt+0x140, vt+0x54) and its record's fields.
  virtual Handle node_find(Handle root, const char* name);
  virtual void script_path_add(const char* path, bool first) { unported(first ? "0x00576100" : "0x00576140", {path}); }
  virtual FieldBaseArms* arms_make();  // operator new 0x90, 0x00458890
  virtual void arms_free(FieldBaseArms* a) { delete a; }  // 0x005a9dc0 and the delete
  virtual ai::AiParams* brain_params(BasicVehicle& v);
  // The arms' nodes: the local matrix (0x004fc3f0) written and set (0x004fc300).
  virtual float* node_matrix(Handle node);
  virtual void node_set_matrix(Handle node, const float* m);
  // The destructor's: the node's first parent (+0x58) losing it (0x004ebb40),
  // zEffect's animations on it stopped (0x00506bc0(node, 1, 1)), the
  // registry's animation stopped (0x00506ca0).
  virtual bool node_in_world(Handle node);
  virtual void node_unparent(Handle node);
  virtual void anims_stop_on(Handle node);
  // The damage's: the player's vehicle's allies ([0x0060861c]+0x14, +0xc4),
  // the difficulty's scale for the player's side ([0x00608e5c]), a radio
  // line (0x004e6d50(1.0)), the last base lost (0x0047a720 on
  // [0x0060b544]), an entity's team (vt+0x28, +0xc0), the controls'
  // IControls vt+0x94 (a hit, with the attacker's entity).
  virtual uint32_t player_allies();
  virtual float difficulty();
  virtual void say(Handle sound) { unported("0x004e6d50", {low(sound), 0x3f800000}); }
  virtual void all_lost() { unported("0x0047a720", {0x60b544}); }
  virtual uint32_t entity_team(Handle entity);
  virtual void controls_hit(BasicVehicle& v, Handle weapon, uint32_t x, float amount, Handle entity);
  // The death's (0x004590b0 after the basic vehicle's): the campaign's
  // field bases one fewer and what the player carries cut (docs/vehicles.md,
  // A mobile field base lost).
  virtual Handle campaign() { return h(unported("0x00479440", {})); }
  virtual void base_lost(Handle campaign) { unported("0x004590b0 cut", {low(campaign)}); }
};
void set_field_base_hooks(FieldBaseHooks* h);  // null: the defaults
FieldBaseHooks& field_base_hooks();

bool is_field_base(const BasicVehicle& v);
// Mech3.exe 1.2 @ 0x004589d0 (ECX the controls, EDX the type): the base
// named `name`, a copy of the world's `type` put in the world; null without it.
FieldBase* field_base_create(Handle ctl, const char* type, const char* name);
void field_base_construct(FieldBase& b, Handle ctl, Handle node);  // 0x00458a70
void field_base_setup(FieldBase& b, zrdr::Node* def, const char* name, int32_t kind);  // vt+0x158 0x00458d40
void field_base_frame(FieldBase& b, float dt);   // vt+0x34 0x00459640
void field_base_arms_step(FieldBase& b, float dt);  // 0x004597d0
int32_t field_base_damage(FieldBase& b, Handle weapon, uint32_t x, float amount);  // vt+0x160 0x00458f50
void field_base_die(FieldBase& b);                // vt+0x70 0x004590b0
void field_base_destroy(FieldBase& b);            // 0x00458bc0
void field_base_delete(FieldBase* b);             // 0x00458b30(1)

}  // namespace mech3
