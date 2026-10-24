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

struct FieldBase;
struct FieldBases;

// A step of the repair dock (docs/vehicles.md, The repair dock): the base
// of its states and of a field base's drive (vtable 0x005c2600). A list runs
// its front one: begun, stepped (true marks it finished) until done, then
// ended and dropped and the next begun.
struct DockStep {
  virtual ~DockStep() = default;
  virtual void begin() { finished = false; }            // vt+0x00 0x0045b980
  virtual bool step(float dt) { (void)dt; return finished; }  // vt+0x04 0x0045b990
  virtual void after(float dt) { (void)dt; }            // vt+0x08 0x004a5bc0
  virtual void end() {}                                 // vt+0x0c 0x0049bfd0 (empty in this build)
  virtual Handle message(Handle m);                     // vt+0x10 0x0045b7a0: abort, then m
  virtual void abort();                                 // vt+0x14 0x0045b7b0: 0x0047a720, finished
  virtual bool done() { return finished; }              // vt+0x18 0x0045b9b0
  bool finished = false;                                // +0x04
};

// A field base's drive to or from the dock (0x18 bytes, vtable 0x005c2540,
// 0x004598a0): its arms opened or shut, and backed or driven at a quarter
// throttle until its travel (+0x14) crosses 0.2.
struct FieldBaseDrive : DockStep {
  FieldBase* base = nullptr;  // +0x08
  bool open = false;          // +0x0c  out (arms open, backing) or in
  bool reached = false;       // +0x0d
  float way = 0;              // +0x10  -1 out, 1 in
  float travel = 0;           // +0x14
  // Ours: deleted by its state while still on its base's steps (the exe
  // frees it there and goes on reading it until the base drops it); the
  // base frees it when it drops it.
  bool orphan = false;
  FieldBaseDrive(FieldBase* b, bool o) : base(b), open(o) {}
  void begin() override;              // 0x004598d0
  bool step(float dt) override;       // 0x00459990 (dt unread)
  void after(float dt) override;      // 0x00459ac0
  void end() override {}              // 0x005ac5c0
  Handle message(Handle m) override { return m; }  // 0x0045b9a0
  void abort() override;              // 0x00459af0
  bool done() override;               // 0x0045b9c0: finished, or no base or an inactive one
  bool there(float d) const;          // 0x00459a80: past 0.2 its way
};

// The dock's states (each 0x00459b80 makes, in the registry): +0x08 the
// registry, +0x0c the radio line it says.
struct DockState : DockStep {
  FieldBases* reg = nullptr;  // +0x08
  Handle sound = nullptr;     // +0x0c
  void say_soon();            // 0x0045b7d0: its line once the last has had its time
  void cancel(DockStep* to);  // the aborts' unwind: the list emptied to `to`, the front ended, `to` begun
};
// +0xbc (vtable 0x005c25e0): waiting for a mech to ask (its message is the
// request, 0x0045a780, step 4b2).
struct DockIdle : DockState {
  bool asked = false;            // +0x10
  void begin() override;         // 0x0045a740
  bool step(float) override { return asked; }  // 0x0045a760
  void end() override;           // 0x0045a770
  Handle message(Handle m) override;  // 0x0045a780 (FieldBaseHooks::dock_request)
};
// +0xd0 (0x005c25c0): the mech comes within 12 m of the dock point, powered down.
struct DockContact : DockState {
  void begin() override;         // 0x0045acb0
  bool step(float) override;     // 0x0045ace0
  void end() override;           // 0x0045acd0
  Handle message(Handle m) override { return m; }  // 0x0045add0
  void abort() override;         // 0x0045ade0: to the reset
};
// +0xe0 (0x005c2620): the bases reach their places (their valves).
struct DockPlaces : DockState {
  uint32_t until = 0;            // +0x10  a frame count
  void begin() override;         // 0x0045ab50
  bool step(float) override;     // 0x0045ac20
  void end() override;           // 0x0045ac70
  void abort() override;         // 0x0045ade0
  bool done() override;          // 0x0045ab70
};
// +0xf4 (0x005c2640, 0x0045aeb0): the bases back out and open.
struct DockDeploy : DockState {
  FieldBaseDrive* drives[3] = {};  // +0x10
  void begin() override;         // 0x0045af00
  bool step(float) override;     // 0x0045b0e0
  void end() override;           // 0x0045b150
  Handle message(Handle m) override;  // 0x0045b770: another mech hears "occupied"
  void abort() override;         // 0x0045b1b0: to the close
};
// +0x110 (0x005c25a0): twelve seconds, then the mech repaired.
struct DockRepair : DockState {
  float timer = 0;               // +0x10
  void begin() override;         // 0x0045b2f0
  bool step(float dt) override;  // 0x0045b320
  void end() override;           // 0x0045b380
  Handle message(Handle m) override;  // 0x0045b770
  void abort() override;         // 0x0045b3b0: to the close
};
// +0x124 (0x005c2580): the bases drive back in and shut.
struct DockClose : DockState {
  FieldBaseDrive* drives[3] = {};  // +0x10
  void begin() override;         // 0x0045b460
  bool step(float) override;     // 0x0045b620
  void end() override;           // 0x0045b660
  Handle message(Handle m) override;  // 0x0045b770
  void abort() override {}       // 0x005ac5c0
};
// +0x140 (0x005c2560): the dock free again, the idle state queued.
struct DockReset : DockState {
  void begin() override;         // 0x0045b6c0
  bool step(float) override { return true; }  // 0x0045b6e0
  void end() override;           // 0x0045b6f0
  void abort() override {}       // 0x005ac5c0
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
  std::list<DockStep*> actions;         // +0xfc..+0x107  (the allocator byte, the head, the count)
  bool begin_next = false;              // +0x108  the front one to begin
  // The dock's places for it (0x0045a560, step 4b2): the registry's points
  // out (+0x2c..) and in (+0x50..), the dock point.
  const float* dock_out = nullptr;      // +0x10c
  const float* dock_in = nullptr;       // +0x110
  float dock_at[3] = {};                // +0x114
  Handle nodes[7] = {};                 // +0x120  arm01..04, cover_left, cover_right, cover_middle
  FieldBaseArms* arms = nullptr;        // +0x13c
  zsis::Part* parts[7] = {};            // +0x140  the script's parts by the same names
  Handle anim = nullptr;                // +0x15c  the registry's animation (0x00458cf0, 4b)
};

// The field bases' registry (the static at 0x00605ae0, made by 0x00459b80;
// the repair dock's states around it are item 4b): up to three bases, each
// with its valve, by the node's name, set to 0; the dock's radio lines.
struct FieldBases {
  std::list<DockStep*> states;         // +0x00..+0x0b  the dock's, run by its frame (0x0045a270, 4b2)
  bool begin_next = false;             // +0x0c
  int32_t count = 0;                   // +0x10
  FieldBase* bases[3] = {};            // +0x14
  ai::Valve* valves[3] = {};           // +0x20
  // +0x8c..+0xb4: mcb_friendly_fire, mcb_under_attack, mcb_lost_vehicle,
  // mcb_occupied, mcb_pos_wait, mcb_pos_cancel, mcb_contact, mcb_waiting,
  // mcb_deploy, mcb_abort, mcb_complete.
  Handle sounds[11] = {};
  Handle effects = nullptr;            // [0x00605c44]  the animation "mfbeffects"
  float points[6][3] = {};             // +0x2c  out (3), in (3): 0x0045a560, 4b2
  Handle requester = nullptr;          // +0x74  the mech that asked
  bool busy = false;                   // +0x78  deploying or repairing
  Handle dock_point = nullptr;         // +0x7c  its vt+0x18 the place (0x0045a780, 4b2)
  int32_t camera = -1;                 // +0x80  the view's camera mode before the dock's
  ai::Valve* deploy_valve = nullptr;   // +0x84  "mcbdeploy"
  ai::Valve* point_valve = nullptr;    // +0x88  (0x0045a780, 4b2)
  bool idle_on = false;                // +0xb8
  DockIdle idle;                       // +0xbc
  DockContact contact;                 // +0xd0
  DockPlaces places;                   // +0xe0
  DockDeploy deploy;                   // +0xf4
  DockRepair repair;                   // +0x110
  DockClose close;                     // +0x124
  DockReset reset;                     // +0x140
  FieldBases();                        // 0x00459b80 (the states' sounds: none, before zSound is up)
  FieldBases(const FieldBases&) = delete;
  FieldBases& operator=(const FieldBases&) = delete;
  bool requester_ok() const;           // 0x0045a720: one, powered down (vt+0xa0)
  void set_requester(Handle m);        // 0x0045b740: when idle and the bases may drive
  void message(Handle m);              // 0x0045a500: to the front state, its answer the requester
  void abort_front();                  // 0x0045a540
  void effects_on();                   // 0x0045b290
  void effects_off();                  // 0x0045b2c0
  int32_t alive() const;               // 0x0045a000: those still active
  bool add(FieldBase& b);              // 0x0045a030
  void clear();                        // 0x00459e50
};
FieldBases& field_bases();
// [0x0060b4d0]: the dock lets the bases drive (set 1 by 0x0045a780; 0 at the
// mission start). Clear, a base's frame holds its throttle at 0.
bool& field_bases_drive();
// [0x0060b4cc]: the dock is waiting for the mech (the contact state on; the
// AI's deploy state 0x004d0ce0 reads it).
bool& field_bases_contact();
// [0x00605c4c]: the game time before which no radio line is said again.
float& field_bases_quiet_until();
// [0x00736584], the scripts' loose-file path: made by the mechs' set-up
// (0x00413e60) or the first base and never trimmed, so each base adds the
// path again. The port's motions come from motion.zbd only.
bool& field_bases_path_made();
// Mech3.exe 1.2 @ 0x00459e80: the registry emptied, the radio lines and
// the effects found, the states' list back to the idle one (to begin),
// "mcbdeploy" made and 0, the bases held.
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
  virtual float stopping_now(BasicVehicle& v) { return vehicle_stopping_now(v); }  // vt+0x124 0x004ad050
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
  // 0x0047a720 on [0x0060b544] (unmapped: the last base lost, and each
  // dock state's end of its line).
  virtual void marker_clear() { unported("0x0047a720", {0x60b544}); }
  virtual uint32_t entity_team(Handle entity);
  virtual void controls_hit(BasicVehicle& v, Handle weapon, uint32_t x, float amount, Handle entity);
  // The death's (0x004590b0 after the basic vehicle's): the campaign's
  // field bases one fewer and what the player carries cut (docs/vehicles.md,
  // A mobile field base lost).
  // The dock's (step 4b1): the game time ([0x0060991c]) and frame count
  // ([0x0072f338]); a valve's value (its holder's vt+0x04); an entity's
  // place (0x005a85e0) and powered down (vt+0xa0); the dock point's place
  // (+0x7c vt+0x18); the view's vehicle ([0x005f4c8c]+0x64) and camera mode
  // (+0x58's +0x04, -1 without one) and its switch (0x0040ae70); the mech
  // put at rest at a place (0x00418e90) and repaired (0x004154e0); the
  // effects animation started on a node (0x00508c10(def, node, 0, 0, 0));
  // the dock request (the idle state's message 0x0045a780, step 4b2).
  virtual float game_time();
  virtual uint32_t frame();
  virtual int32_t valve_value(ai::Valve* v);
  virtual const float* entity_position(Handle e) { return reinterpret_cast<const float*>(h(unported("0x005a85e0", {low(e)}))); }
  virtual bool powered_down(Handle e) { return unported("entity.vt+0xa0", {low(e)}) != 0; }
  virtual const float* dock_point_position(Handle p) { return reinterpret_cast<const float*>(h(unported("point.vt+0x18", {low(p)}))); }
  virtual Handle view_vehicle() { return h(unported("[0x005f4c8c]+0x64", {})); }
  virtual int32_t view_camera() { return static_cast<int32_t>(unported("[0x005f4c8c]+0x58+0x4", {})); }
  virtual void view_set_camera(int32_t mode) { unported("0x0040ae70", {0x5f4c8c, mode}); }
  virtual void mech_rest_at(Handle mech, const float* pos) { unported("0x00418e90", {low(mech), low(pos)}); }
  virtual void mech_repair(Handle mech, float dt) { unported("0x004154e0", {low(mech), zgame::bits(dt)}); }
  virtual Handle effects_start(Handle def, Handle node) { return h(unported("0x00508c10", {low(def), low(node), 0, 0, 0})); }
  virtual Handle dock_request(DockIdle& s, Handle m) { (void)s; return h(unported("0x0045a780", {low(m)})); }
  virtual FieldBaseDrive* drive_make(FieldBase* b, bool open) { return new FieldBaseDrive(b, open); }  // new 0x18
  virtual void drive_free(FieldBaseDrive* d) { delete d; }
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
void field_base_effects_start(FieldBase& b);      // 0x00458cf0: the registry's effects on it, once
void field_base_effects_stop(FieldBase& b);       // 0x00458d20
void field_base_drop_front(FieldBase& b);         // its front step off the list (an orphaned drive freed)
void field_base_delete(FieldBase* b);             // 0x00458b30(1)

}  // namespace mech3
