#pragma once
#include <cstdint>
#include <deque>
#include <vector>
#include <memory>

#include "gamez/zgame/unported.h"
#include "mech3/zai_params.h"
#include "mech3/zai_firing.h"
#include "mech3/zai_path.h"
#include "mech3/zai_steps.h"
#include "mech3/veh_controls.h"

namespace znet { class Object; }

// The pilot's brain (docs/ai.md, The brain's data): what an AI vehicle's
// controls are built around. Its constructor 0x004cf330 copies the vehicle's
// aiv.zrd settings, makes the firing state, the order stack with its first
// order on the vehicle's net, a steering block and, for an allied pilot with
// a radio voice, the lancemate radio. What the brain decides each frame
// (0x004e9710), the behaviours and the firing are not ported: only the data
// they start from. Original files unknown: the brain sits in the AI's big
// unnamed range (0x004cf330..0x004cf810), its orders, radio and net hook with
// the AI vehicle controls (0x004e7c40..0x004ea910), the firing state after
// weapon.cpp (0x004b6390..0x004b7060).
namespace mech3::ai {

class AiNet;
class Brain;
struct Order;

class NetPart;
class BehaviourState;
struct Behaviour;
struct AiTarget;
struct Contact;
class Region;

// What the brain calls outside it. The defaults run the ports.
class BrainHooks : public zgame::Unported {
 public:
  static uint32_t low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  // The original's operator new and delete for blocks the port owns (silent
  // here; the tests give the port's objects the original's addresses).
  virtual void on_new(uint32_t size, const void* p) { (void)size, (void)p; }
  virtual void on_delete(const void* p) { (void)p; }
  virtual uint32_t ref(const void* p) { return low(p); }
  // A net by number: znet's object (0x0058b160), then the AI's casts.
  virtual znet::Object* net_find(int32_t id);
  // The built-in net at 0x00604008 (made at start-up by 0x004c0310, not
  // ported): the pilot's net when its own is missing.
  virtual AiNet* builtin_net() { return nullptr; }
  // The default behaviour at 0x00604068 (zai_steps): an order's without one.
  virtual Behaviour* default_behaviour();
  // The handle the hooks know a vehicle by (the program's object table).
  virtual zgame::Handle vehicle_handle(void* vehicle) { return low(vehicle); }
  // The hit handlers' questions (docs/ai.md 17.8b): a vehicle's mech
  // (vt+0x50; a mech is its own); the attacker's controls told it hit an ally
  // (vt+0x40, vt+0x140, IControls vt+0x98: the AI's 0x004ea500, nothing for
  // the rest); everyone turning on the player (0x004d5dc0).
  virtual zgame::Handle vehicle_mech(zgame::Handle v) { return unported("vehicle.vt+0x50", {v}); }
  virtual void ally_hit(zgame::Handle attacker, zgame::Handle victim) {
    unported("attacker.ictl.vt+0x98", {attacker, victim});
  }
  virtual void allies_turn() { unported("0x004d5dc0", {}); }
  // 0x004ea260 (the refit order, zai_repair.cpp): the repair behaviour
  // pushed when a field base is up and the dock takes the pilot.
  virtual bool refit(Brain& b);
  // The refit's and the repair state's: the field bases still up
  // (0x0045a000), the dock asked for `v` (0x0047a780: the mcbwaynet
  // [0x0060b544] there and 0x0047a730), the mcbwaynet there, its selected
  // point's place (0x0058b060 by its +0x20, then vt+0x18; false without),
  // the dock's flags (the bases driving [0x0060b4d0], in contact
  // [0x0060b4cc]) and a mech powered down or up (0x0041cc30(1 / 0)).
  virtual int32_t bases_alive() { return static_cast<int32_t>(unported("0x0045a000", {0x605ae0})); }
  virtual bool mfb_ask(zgame::Handle v) { return unported("0x0047a780", {v}) & 0xff; }
  virtual bool has_mcb() { return unported("[0x0060b544]", {}) != 0; }
  virtual bool dock_point(float out[3]);
  virtual bool dock_drive() { return unported("[0x0060b4d0]", {}) & 0xff; }
  virtual bool dock_contact() { return unported("[0x0060b4cc]", {}) & 0xff; }
  virtual void mech_power(zgame::Handle mech, int32_t down) { unported("0x0041cc30", {mech, down}); }
  // The game time [0x0060991c].
  virtual float now();
  // The pilot's vehicle (the exe's IControls +0x10): its health, vt+0x98
  // (the Mech's 0x0041ba40, its parts' product); its position (0x005a85e0);
  // its AiTarget part (+0x68..), whose contacts the brain reads.
  virtual float health(zgame::Handle v);
  virtual void position(zgame::Handle v, float out[3]);
  virtual AiTarget* targetable(zgame::Handle v) { (void)v; return nullptr; }
  // The plain states' questions (docs/ai.md 17.2): a net's nearest step to a
  // point (0x004d51d0 then vt+0x74; the port's nearest_step), a vehicle's
  // AiTarget +0xbc, the player's pilot's lance state ([0x0060861c] +0x6c:
  // vt+0x48, then vt+0x2c; -1 without a pilot), the formation net
  // 0x00603fb8, the player's vehicle (the pilot's +0x14) and a vehicle's
  // own net's follow behaviour (0x004c0de0, item 10).
  virtual Behaviour* nearest(AiNet* n, const float p[3]);
  virtual uint8_t vehicle_flags(zgame::Handle v);
  virtual int32_t player_lance_state() { return static_cast<int32_t>(unported("[0x0060861c]+0x6c", {})) - 1; }
  virtual AiNet* formation_net();  // 0x00603fb8: the port's FormationNet, made on first ask
  virtual zgame::Handle player_vehicle() { return unported("[0x0060861c]+0x14", {}); }
  virtual Behaviour* follow_behaviour(AiNet* vehicle_net);  // 0x004c0de0 (on the vehicle's net)
  // The manoeuvres at 0x00604098 (0) and 0x00604088 (1), class 0x004ca440.
  virtual Behaviour* manoeuvre_behaviour(int32_t which);

  // The brain's frame (0x004e9710, docs/ai.md 17.1). The AI switch
  // [0x0060b938] (0x004ea740 on, 0x004ea750 off).
  virtual bool ai_on();
  virtual int32_t powered_down(zgame::Handle v);               // vt+0xa0 == 1 (the Mech's 0x0041cc10)
  virtual Contact* pick(zgame::Handle v, int32_t side);        // 0x004d6b90 on the vehicle
  // A contact's vehicle: 0x004e6fe0 (its +0x04's vt+0x2c), then vt+0x40.
  virtual zgame::Handle contact_vehicle(const Contact* c);
  virtual float contact_range2(zgame::Handle v);               // a vehicle's AiTarget +0xa4
  virtual void count_awake();                                  // [0x006099c0] += 1
  virtual float intensity();                                   // GameIntensity [0x00608e54]
  virtual int32_t rand();                                      // MSVC's rand (0x005bb668)
  // The vehicle's weapon manager's ammo value 0x004b4ed0 (+0xd4); false
  // without a manager.
  virtual bool ammo_value(zgame::Handle v, float* out);
  // Not ported yet, each an item of the porting order (ai.md 17.9).
  // Steering's questions: a vehicle's contacts (+0xac..+0xb0, nearest
  // first); what a contact sees as an entity (0x004e6fe0: its +0x04's
  // vt+0x2c; 0 for none) and an entity's flat forward (+0x24, +0x2c).
  virtual const std::vector<Contact*>* contacts(zgame::Handle v);
  virtual zgame::Handle contact_entity(const Contact* c) { return unported("0x004e6fe0", {low(c)}); }
  virtual void forward(zgame::Handle e, float* x, float* z);
  virtual void radio_line(Brain& b, int32_t event);            // 0x004e6d50(1.0) on the radio's line (item 14)
  // The vlist triggers' questions (0x004d8e70): the vehicle's heat (vt+0x9c,
  // the Mech's +0x554), a clear line to what a contact sees (0x004d5980 on
  // the vehicle, not ported), a region's inside test (0x004c57d0).
  virtual float heat(zgame::Handle v);
  virtual bool clear_line(zgame::Handle v, zgame::Handle seen);
  virtual bool region_inside(Region* r, const float* p);
  virtual void firing_reset(Brain& b);                         // 0x004e9590 -> Firing::stand_down
  virtual void firing_run(Brain& b, float dt);                 // 0x004e9fb0 -> Firing::run
  // The firing's questions (zai_firing, docs/ai.md 17.7): the vehicle's
  // weapon manager (+0xd4, 0 for none), its current weapon (0x004b04c0), the
  // aim cleared (0x004b0bc0(0)), the alpha strike (0x004b0a70); a weapon's
  // type wait (0x004b4ba0), reload (+0x30), ammo (vt+0x14) and its type's
  // flight hold (+0x2c's +0x1f0, 0x004b4c90); the torso recentred (the
  // mech's vt+0x50, its torso's 0x00465a00(1)).
  virtual zgame::Handle weapons(zgame::Handle v) { return unported("vehicle+0xd4", {v}); }
  virtual zgame::Handle wm_current(zgame::Handle wm) { return unported("0x004b04c0", {wm}); }
  virtual void wm_clear_aim(zgame::Handle wm) { unported("0x004b0bc0", {wm, 0u}); }
  virtual void wm_alpha(zgame::Handle wm, float seconds) { unported("0x004b0a70", {wm, zgame::bits(seconds)}); }
  virtual bool weapon_type_ready(zgame::Handle w) { return unported("0x004b4ba0", {w}) & 0xff; }
  virtual float weapon_reload(zgame::Handle w) { return zgame::from_bits(unported("weapon+0x30", {w})); }
  virtual int32_t weapon_ammo(zgame::Handle w) { return static_cast<int32_t>(unported("weapon.vt+0x14", {w})); }
  virtual void weapon_hold_for_flight(zgame::Handle w) { unported("0x004b4c90", {w}); }
  virtual void torso_centre(zgame::Handle v) { unported("0x00465a00", {v, 1u}); }
  virtual FirePlan* fire_plan_pick(Brain& b);  // 0x004b6790: volley_plan_pick
  // The volley pick's questions (zai_firing): an entity's active flag (+0x08),
  // a vehicle's weapon bearer (vt+0x40, a mech itself), whether it can face a
  // direction (vt+0x8c, the Mech's 0x0041dc70: the torso's 0x00465c70),
  // whether its bearer has the plan in reach (vt+0x150, 0x0041dba0: the
  // torso's 0x00465bb0), its target taken (vt+0xc0(c, 0): AiTarget's
  // 0x004d5e40), the volley the weapon manager picks at a contact
  // (0x004b1860, item 13), a plan tracked (0x004b6e30, item 12b2), a clear
  // line to an entity's position (0x004d5980 with 0x005a8630's point).
  virtual bool entity_active(zgame::Handle e) { return unported("entity+0x08", {e}) & 0xff; }
  virtual zgame::Handle bearer(zgame::Handle v) { return v; }
  virtual bool can_face(zgame::Handle v, const float dir[3]) {
    (void)dir;
    return unported("vehicle.vt+0x8c", {v}) & 0xff;
  }
  virtual uint8_t bearer_in_reach(zgame::Handle b, float arg) {
    return unported("vehicle.vt+0x150", {b, zgame::bits(arg)}) & 0xff;
  }
  virtual bool select_target(zgame::Handle v, Contact* c);
  virtual int32_t volley_pick(zgame::Handle wm, Contact* c) {
    return static_cast<int32_t>(unported("0x004b1860", {wm, low(c)}));
  }
  // The track's and the lead point's (0x004b6e30, 0x004d6de0): the target
  // mech aiming others at its last hit part (vt+0x50's 0x0041d130), the
  // selected group's weapon (+0x28, +0x2c[], its +0x10; 0 for none), a
  // weapon's node position (0x004ed530 on +0x20), speed (def +0x2c) and
  // flags (def +0x6c), an entity's position (0x005a8630), a vehicle's
  // velocity (vt+0x0c) and matrix (+0x0c), the bearer's clamp (vt+0x90) and
  // torso aim (vt+0x154).
  virtual void mech_aim_at_last_hit(zgame::Handle e) { unported("0x0041d130", {e}); }
  virtual zgame::Handle wm_group_weapon(zgame::Handle wm) { return unported("wm+0x28", {wm}); }
  virtual void weapon_node_position(zgame::Handle w, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    unported("0x004ed530", {w});
  }
  virtual float weapon_speed(zgame::Handle w) { return zgame::from_bits(unported("def+0x2c", {w})); }
  virtual uint32_t weapon_flags(zgame::Handle w) { return unported("def+0x6c", {w}); }
  virtual void entity_lock_position(zgame::Handle e, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    unported("0x005a8630", {e});
  }
  virtual void velocity(zgame::Handle v, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    unported("vehicle.vt+0xc", {v});
  }
  virtual void vehicle_matrix(zgame::Handle v, float m[9]) {
    for (int i = 0; i < 9; ++i) m[i] = (i % 4 == 0) ? 1.0f : 0.0f;
    unported("vehicle+0xc", {v});
  }
  virtual bool bearer_aim_clamp(zgame::Handle b, const float dir[3], float out[3]) {
    out[0] = dir[0], out[1] = dir[1], out[2] = dir[2];
    return unported("vehicle.vt+0x90", {b}) & 0xff;
  }
  virtual void bearer_set_aim(zgame::Handle b, const float dir[3]) {
    unported("vehicle.vt+0x154", {b, zgame::bits(dir[0]), zgame::bits(dir[1]), zgame::bits(dir[2])});
  }
  virtual bool clear_line_at(zgame::Handle v, zgame::Handle e);  // clear_line to e's lock position (0x005a8630)
  // The clear line's (0x004d5980, zai_target): an entity's zone list (+0x64:
  // a count, then ids, 0xff all; zEntity3d's 0x005a80f0 that keeps it is not
  // ported, so count 0: every zone), its node (+0x04) and centroid (+0x14), a
  // node's world position (its data +0x84, redone first when stale:
  // 0x004ed280(1)), its INTERSECT flag (0x004eb710 / 0x004eb6d0) and the
  // world's segment query (0x004eed90 on [0x00608c84] +4) with the first-hit
  // mode and the exclude flags set for it (0x004ed9c0, 0x004eda00): its
  // answer, and the crossings in *count.
  virtual void zone_list(zgame::Handle e, uint8_t out[4]) {
    (void)e;
    out[0] = 0, out[1] = out[2] = out[3] = 0xff;
  }
  virtual zgame::Handle entity_node(zgame::Handle e) { return unported("entity+0x04", {e}); }
  virtual zgame::Handle entity_centroid(zgame::Handle e) { return unported("entity+0x14", {e}); }
  virtual void node_world_cached(zgame::Handle n, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    unported("node+0x84", {n});
  }
  virtual bool node_intersect(zgame::Handle n) { return unported("0x004eb710", {n}) & 1; }
  virtual void node_set_intersect(zgame::Handle n, bool on) { unported("0x004eb6d0", {n, on ? 1u : 0u}); }
  virtual int32_t segment_first(const float from[3], const float to[3], uint32_t exclude, uint8_t* count) {
    *count = 0;
    return static_cast<int32_t>(unported("0x004eed90", {zgame::bits(from[0]), zgame::bits(from[1]), zgame::bits(from[2]),
                                                       zgame::bits(to[0]), zgame::bits(to[1]), zgame::bits(to[2]), exclude}));
  }
  // The firing's aim (0x004b72a0, 0x004b77e0: the firing's Aim) and its
  // questions: the difficulty's aim error ([0x00608e58]: 1.2, 1.0, 0.6),
  // whether the vehicle's mech (vt+0x50) has a targeting computer (+0x3e4)
  // or a fire control system (+0x3b8), the manager aimed at a point (+0x88
  // set, then 0x004b0bc0).
  virtual void aim_track(Brain& b, Contact* target, zgame::Handle w);
  virtual void aim_lead(Brain& b, float dt, Contact* target, zgame::Handle w, zgame::Handle wm);
  virtual float aim_intensity() { return zgame::from_bits(unported("[0x00608e58]", {})); }
  virtual bool targeting_aid(zgame::Handle v) { return unported("mech+0x3e4/+0x3b8", {v}) & 0xff; }
  virtual void wm_aim_at(zgame::Handle wm, const float at[3]) {
    unported("0x004b0bc0", {wm, zgame::bits(at[0]), zgame::bits(at[1]), zgame::bits(at[2])});
  }
  virtual AiNet* vehicle_net(zgame::Handle v);                 // vt+0xd8: its own net (0x004d70f0)
  virtual Behaviour* attack_behaviour(AiNet* vehicle_net);     // 0x004c0e10
  virtual bool attack(Brain& b, AiNet* current_net);           // 0x004e9ca0 (item 10)
  virtual void own_order_run(Brain& b, float dt);              // the brain's own order's vt+0x04: Brain::run_own_order
  virtual void* sound_find(const char* name);  // 0x0056f6f0: zsnd::find
  // 0x0056e7e0 (ECX the sound, EDX the place, the volume, 0): a sound step's
  // sound at the pilot's place.
  virtual void sound_at(void* sound, const float pos[3], float volume) {
    unported("0x0056e7e0", {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sound)), zgame::bits(pos[0]),
                            zgame::bits(pos[1]), zgame::bits(pos[2]), zgame::bits(volume)});
  }
  // The player's vehicle ([0x0060861c] +0x14) and its allies (+0xc4, the
  // AiTarget masks); false when there is no pilot or no vehicle. Silent.
  virtual bool player_allies(uint32_t* allies) {
    (void)allies;
    return false;
  }
  // The walking states' questions (docs/ai.md 17.4a): the brain's way
  // finder's next step after `at` (+0x238, 0x004c7cb0); the vehicle's speed at
  // a share of its top speed (vt+0x118, the Mech's 0x0041d1c0) and its
  // stopping distance from a speed (vt+0x120, 0x0041d210); the mission
  // boundary's pull on a point (0x004c3000 with ECX the vehicle, EDX `in`:
  // true with the place `out` is pulled to, at `in`'s height; false, `out`
  // untouched, as when the world has no boundary); what the steps' passable tests
  // see of the pilot (its vehicle's +0x04 name and vt+0x50's +0x3d8 jump jets).
  virtual Behaviour* path_next(Brain& b, Behaviour* at);
  virtual float speed_at(zgame::Handle v, float share);
  virtual float stopping_distance(zgame::Handle v, float speed);
  virtual float stop_share(zgame::Handle v, float d);  // vt+0x12c, the Mech's 0x0041d240
  // The station keeping's (zai_attack): a vehicle's vt+0xf4 (0 for the
  // mechs, 0x0040bae0) and its speed (vt+0x114; its velocity is velocity()).
  virtual bool vehicle_xf4(zgame::Handle v) { return unported("vehicle.vt+0xf4", {v}) & 0xff; }
  virtual float move_speed(zgame::Handle v) { return zgame::from_bits(unported("vehicle.vt+0x114", {v})); }  // the Mech's 0x0041d1d0
  // The deploy state's: the vehicle's local vertical speed (+0x40) and its
  // placement (vt+0xdc, the Mech's set_place 0x00418f30).
  virtual float vertical_speed(zgame::Handle v) { return zgame::from_bits(unported("vehicle+0x40", {v})); }
  // The jump state's: the vehicle's (vt+0x50's) local forward speed (+0x44),
  // its local velocity set (+0x3c..+0x44), whether it is airborne (vt+0x144,
  // the Mech's +0x584), and the world's gravity ([0x005e5d34], WorldSetGravity).
  virtual float forward_speed(zgame::Handle v) { return zgame::from_bits(unported("vehicle+0x44", {v})); }
  virtual void set_local_velocity(zgame::Handle v, float x, float y, float z) {
    unported("vehicle+0x3c=", {v, zgame::bits(x), zgame::bits(y), zgame::bits(z)});
  }
  virtual bool airborne(zgame::Handle v) { return unported("vehicle.vt+0x144", {v}) & 0xff; }
  // The manoeuvre's: the jets' fuel (vt+0x148, 0..1), and where to aim at
  // `target` (vt+0x50's 0x0041d940, the selected weapon's lead point) when
  // the vehicle's own target (+0x6c, a contact) sees it; false otherwise.
  virtual float jump_fuel(zgame::Handle v) { return zgame::from_bits(unported("vehicle.vt+0x148", {v})); }
  virtual bool lead_point(zgame::Handle v, zgame::Handle target, float out[3]) {
    (void)out;
    return unported("0x0041d940", {v, target}) & 0xff;
  }
  virtual float gravity() { return zgame::from_bits(unported("[0x005e5d34]", {})); }
  virtual void place(zgame::Handle v, const float p[3], float yaw) {
    unported("vehicle.vt+0xdc", {v, zgame::bits(p[0]), zgame::bits(p[1]), zgame::bits(p[2]), zgame::bits(yaw)});
  }
  virtual bool inout_clamp(zgame::Handle v, const float in[3], float out[3]) {
    (void)in, (void)out;
    return unported("0x004c3000", {v}) & 0xff;
  }
  virtual void pilot_of(zgame::Handle v, Pilot* p) {
    (void)p;
    unported("vehicle.vt+0x50", {v});
  }
  // The basic vehicle controls' destructor's calls (0x004ac800): the owner's
  // vt+0xec (a vehicle's itself, 0x004d2a70), then 0x00402510 on its answer
  // (veh_basic.cpp: the basic path's temporary entries dropped).
  virtual void* owner_vehicle(void* owner) { return reinterpret_cast<void*>(unported("owner.vt+0xec", {low(owner)})); }
  virtual void path_drop_temporary(void* vehicle) { unported("0x00402510", {low(vehicle)}); }
};
BrainHooks& brain_hooks();
// Mech3.exe 1.2 @ 0x004ac800 (the basic vehicle controls' destructor, from
// the AI's 0x004e7870): the vtables back, then the owner's vehicle's path
// told (0x00402510).
void vehicle_controls_gone(void* owner);
// Ours, for the tests: the AiVehicleControls alive.
int32_t ai_vehicle_controls_alive();
// The AI switch [0x0060b938]: on at the end of the mission start
// (0x004ea740), off in the cleanup (0x004ea750).
bool& ai_switch();
inline void ai_switch_on() { ai_switch() = true; }    // 0x004ea740
inline void ai_switch_off() { ai_switch() = false; }  // 0x004ea750
void set_brain_hooks(BrainHooks* h);

// What a net builds in an order (its vt+0x5c, in place at the order's
// +0x30; docs/ai.md 17.2). Slot 0 destroys it in place (no free).
class NetPart {
 public:
  explicit NetPart(Order* o) : order(o) {}
  virtual ~NetPart() = default;           // +0x00
  virtual void run(float dt) { (void)dt; }  // +0x04: 0x004bf620 for the shipped kinds
  virtual AiNet* net() const = 0;         // +0x08

  Order* order = nullptr;                 // +0x04
};

// The part a compiled net and the built-in one make: 0x0c bytes over
// 0x004bffe0 (0x004c0070 -> 0x004c0010, vtable 0x005ca748; the built-in's
// 0x004c0520 -> 0x004c0540, 0x005cab50): the order and the net.
class NetLink : public NetPart {
 public:
  NetLink(Order* o, AiNet* n, bool built_in) : NetPart(o), target(n), builtin(built_in) {}
  AiNet* net() const override { return target; }  // 0x004c0030

  AiNet* target = nullptr;  // +0x08
  bool builtin = false;     // the vtable
};

// A vehicle's own net's part (0x004c0ba0, 0x24 bytes, vtable 0x005cac40;
// docs/ai.md 17.2): a home point for the leash, the pilot's position when
// it is made (or at its first run when the pilot had no vehicle yet),
// stacked on the brain's +0xe4 and taken off again when it goes; and the
// behaviour for the pilot's mode (+0x40: 1 to 4 the net's slots 2, 3, 4,
// 6, by their +0x90 test).
class VehiclePart : public NetPart {
 public:
  VehiclePart(Order* o, VehicleNet* n);  // 0x004c0ba0
  ~VehiclePart() override;               // 0x004c0dc0: the brain's +0xe4 back
  void run(float dt) override;           // +0x04 0x004c0d60
  AiNet* net() const override;           // +0x08 0x0040a890

  float home[3] = {};             // +0x08
  const float* prev = nullptr;    // +0x14  the brain's +0xe4 before
  VehicleNet* target = nullptr;   // +0x18
  Behaviour* behaviour = nullptr; // +0x1c  (its reader not found yet)
  bool pending = false;           // +0x20  no vehicle when made: the home point at the first run
  bool pushed = false;            // its home on the brain's stack

 private:
  void push_home();
};

// What a behaviour builds in an order (its vt+0x8c, in place at +0x54),
// over 0x004c8660 (vtable 0x005cb238): the order. The slots' defaults are the
// base's; each behaviour's own state class overrides them (ai.md 17.2).
class BehaviourState {
 public:
  BehaviourState(Order* o, Behaviour* b) : order(o), from(b) {}
  virtual ~BehaviourState() = default;                  // +0x00
  virtual bool may_break_in() const { return true; }    // +0x04 0x00499270
  virtual bool may_fire() const { return true; }        // +0x08 0x004d7210
  virtual void suspend() {}                             // +0x0c 0x0049bfd0
  virtual void resume() {}                              // +0x10 0x0049bfd0
  virtual void run(float dt);                           // +0x14 0x004c8690 (not ported)
  virtual Behaviour* behaviour() const { return nullptr; }  // +0x18 0x0040bae0 for the base

  Order* order = nullptr;  // +0x04
  Behaviour* from = nullptr;
};

// The state of the shared behaviour at 0x0060b460 (class 0x004cbe10, vtable
// 0x005cbe18) every first order runs: 0x004cbe30 -> 0x004cbe50 (vtable
// 0x005cbed0), 8 bytes. Its run 0x004cbeb0 is not ported (ai.md 17.2).
class SharedState : public BehaviourState {
 public:
  explicit SharedState(Order* o);
  bool may_break_in() const override { return false; }  // 0x004d72a0
  bool may_fire() const override { return false; }      // 0x004d7290
  void run(float dt) override;                          // 0x004cbeb0
  Behaviour* behaviour() const override;                // 0x004cbea0: 0x0060b460
};

// The two plain behaviours made at start-up: 0x00604058 (0x00410610, vtable
// 0x005bc988; the formation net 0x00603fb8 and a vehicle's own net are made
// of it) and 0x00604028 (0x00410630, 0x005bca40).
Behaviour* stand_behaviour();
Behaviour* hold_behaviour();

// The shared behaviour at 0x0060b460, made at start-up.
Behaviour* shared_behaviour();

// The built-in net at 0x00604008 (0x004c0310 over the seven-behaviour net,
// not ported): what an order without a net goes on. Its part is the
// built-in kind (0x004c0520).
class BuiltinNet : public CompiledNet {
 public:
  using CompiledNet::CompiledNet;
  NetPart* make_part(Order& o) override;
};

// One order (0x98 bytes, vtables 0x005cd8d4 / 0x005cd8d8, ctor 0x004e7f80,
// 0x004e8030 for the template at 0x0060b898). The deque's orders are
// 0x004e8070's (the template argument unused); the brain's own at +0x194 is
// 0x004e8180's (vtable 0x005cd8e8). Its parts live in place in the exe
// (+0x30, +0x54); here they are owned.
struct Order {
  Order() = default;
  ~Order();                                        // 0x004e7ff0: the state, then the net's part, then the owner cleared
  Order(const Order&) = delete;
  Order& operator=(const Order&) = delete;

  void run(float dt);                              // vt+0x04 0x004e8150: the part, then the state
  void pop();                                      // vt+0x08 0x004e8170: its owner's pop (the brain's own: 0x004e8270)
  bool reset(AiNet* n, Behaviour* b);              // 0x004e80b0: both parts remade
  bool set_behaviour(Behaviour* b);                // 0x004e8120: the state remade

  uint8_t x04 = 0;                  // +0x04  marks the orders a stop takes back (0x004e8c60)
  Brain* owner = nullptr;           // +0x08  0x004e8090 sets it once
  bool own = false;                 // the brain's own order (+0x194, vtable 0x005cd8e8)
  std::unique_ptr<NetPart> net;     // +0x0c  (in place at +0x30)
  std::unique_ptr<BehaviourState> state;  // +0x10  (in place at +0x54)
  int32_t x14 = 0;                  // +0x14  0x004ce200: a word and a point
  float x18[3] = {};                // +0x18
  float x24[3] = {};                // +0x24  0x0063d870 (the zero vector)

  bool set_owner(Brain* b) {                       // 0x004e8090
    if (!owner) owner = b;
    return owner == b;
  }
};

// The brain's steering (+0x250, 0x48 bytes, 0x004ce620; docs/ai.md 17.4):
// a path segment from `start` along `dir` (x, z), and a carrot on it
// `along_at` ahead of the start and `across` to its side, which the
// vehicle's position is measured from (`along`, `side`). The states set the
// segment (0x004cea30) and steer the controls by it; each frame
// 0x004cec80 eases the carrot and keeps it near, and the contacts in the
// way push it aside.
struct Steer {
  float start[3] = {};   // +0x00  0x0063d870
  float dir[2] = {};     // +0x0c  x, z; [0x0060b4b8] / [0x0060b4bc] at first: never written, 0
  float along_min = 0;   // +0x14  how far back the carrot may fall
  float length = 0;      // +0x18  the segment's
  float along_at = 0;    // +0x1c  the carrot's distance along
  float across = 0;      // +0x20  its offset to the side, eased toward
  float across_to = 0;   // +0x24
  float rate = 0;        // +0x28  how fast the carrot falls back, a second
  float along = 0;       // +0x2c  the vehicle from the carrot, along the segment
  float side = 0;        // +0x30  and across
  float near_dist = 20.0f;  // +0x34  0x005e8bb4: near the segment within this
  float x38;             // +0x38  [0x0060b4a0]: cos(pi/4), set at start-up (0x004ce5b0)
  uint32_t x3c = 0, x40 = 0, x44 = 0;  // +0x3c..+0x44  (0x004ce690 clears +0x3c and +0x44)
  Steer();

  // 0x004cea30: a new segment from `from` toward `to`, the carrot falling
  // back at `rate`; the vehicle measured and the contacts let push.
  void set(const float to[3], const float from[3], float rate, zgame::Handle owner);
  void update(float dt, zgame::Handle owner);         // 0x004cec80
  void measure(zgame::Handle owner, float dt);        // 0x004ceb50
  bool near() const;                                   // 0x004ce6a0
  void carrot(float out[3]) const;                    // 0x004ce6f0
  void project(const float p[3], float out[2]) const;  // 0x004ceb00
  void advance(float d) { along_at += d, along -= d; }  // 0x004ce800
  void shift(float da, float ds) { along -= da, side -= ds, along_at += da, across += ds; }  // 0x004ce820

 private:
  float distance_off() const;  // the measure 0x004ce6a0 and 0x004ceb50 compare with near_dist
  void push_by_contacts(zgame::Handle owner, float dt);
};

// 0x004e7c40 (0x40 bytes): the lancemate radio, sixteen zSound lookups of
// "<voice>_<event>" (0x004e7dc0) in this order.
struct Radio {
  static const char* const kEvents[16];
  void load(const char* voice);
  void* sounds[16] = {};
};

// 0x004c9d00 then 0x004cd770 (0x10 bytes at the brain's +0x240, vtable
// 0x005cc290): a step of the brain's own, pointing at the null step.
struct BrainStep {
  uint8_t flags = 0;    // +0x04  bit 0 cleared (the rest is the heap's)
  int32_t x08 = 0;      // +0x08
  const void* next;     // +0x0c  0x00603fa0, the null step
  BrainStep();
};

// IControls +0x04 (0x004aec00 zeroes it): the last hit's game time, when the
// run of hits from one attacker began (a new attacker or a 3 s gap starts
// another) and that attacker.
struct HitRecord {
  float last = 0;            // +0x00
  float run_start = 0;       // +0x04
  zgame::Handle by = 0;      // +0x08
  void note(zgame::Handle attacker, float now);        // 0x004aec10
  bool within(float secs, float now) const;            // 0x004aec40 (0x004aed40: 1 s, "hit lately")
  float run() const { return last - run_start; }       // 0x004aec70
};
// Mech3.exe 1.2 @ 0x004aec90 (IControls vt+0x94, the base): the hit noted;
// from an attacker that counts the vehicle an ally, the attacker's controls
// are told (IControls vt+0x98).
void controls_hit(HitRecord& r, zgame::Handle owner, zgame::Handle attacker);

// What the brain asks of the controls it is a base of (the exe's through
// its virtual base +0xe0): the AI mech controls' 0x004d8750 family.
struct BrainControls {
  virtual ~BrainControls() = default;
  virtual zgame::Handle vehicle() = 0;    // IControls +0x10: the vehicle
  virtual void stop() = 0;                // IControls vt+0x14 (0x00428490)
  virtual void smooth(float dt) = 0;      // the brain's vt+0x04 (the mech's 0x004d89f0 -> 0x00426960)
  virtual bool hit_lately() = 0;          // IControls vt+0xa0 (0x004aed40)
  virtual void fire(bool on) = 0;         // IControls vt+0x10 (0x00428580: +0x34)
  virtual bool reversing() = 0;           // IControls vt+0x18 (0x00428500: +0x50, IsReverse)
  virtual float throttle_now() = 0;       // IControls vt+0x1c (0x00428510: +0x10, smoothed)
  virtual void set_throttle(float v) = 0; // IControls vt+0x20 (0x00426b60)
  virtual void set_turn(float v) = 0;     // IControls vt+0x28 (0x00426b10)
  virtual void set_jump(float v) = 0;     // IControls vt+0x34 (0x00426b40: the jets, no hold)
  virtual void set_jet_forward(float v) = 0;  // IControls vt+0x3c (0x00428560: +0x28, the jets' forward thrust)
  virtual float turn_goal() = 0;          // IControls vt+0x30 (0x00426b00: +0x48)
  virtual void aim_started() = 0;         // IControls vt+0x80 (the AI's 0x004d8930: Brain::scan_aim)
  virtual zgame::Handle attacker() { return 0; }  // IControls +0x0c: the hit record's last attacker
};

// The brain (0x004cf330, 0x2ac bytes and, when it is the most derived
// object, an IControls base at +0x2b0; it never is: the AI controls derive
// from it). Offsets in the comments are the brain's.
class Brain {
 public:
  // 0x004cf330(net, record, 0): the net is the vehicle's, else the record's
  // first item's (znet), else the built-in one.
  Brain(AiNet* net, const AiParams& rec);
  ~Brain();  // 0x004cf630
  Brain(const Brain&) = delete;
  Brain& operator=(const Brain&) = delete;

  // IControls vt+0x04 (0x004e9380): drop every order, then one order on the
  // net. False (nothing changed) when the net is not a compiled one.
  bool set_net(AiNet* net);

  // The order stack (docs/ai.md 17.2). A net or behaviour of none is the
  // built-in net (0x00604008) or the default behaviour (0x00604068).
  Order& current() { return orders.back(); }       // 0x004ea760
  bool remake(AiNet* n);                           // 0x004e82e0: the current order on n
  bool push_net(AiNet* n);                         // 0x004e86a0
  bool push(AiNet* n, Behaviour* b);               // 0x004e87c0
  bool push_behaviour(Behaviour* b);               // 0x004e88c0: on the current order's net
  void pop();                                      // 0x004e8380
  bool pop_if(AiNet* n);                           // 0x004e84a0: when the current order is on n
  // 0x004c84b0: on the current order's net `n`, a way (+0x238) from the
  // current state's behaviour to `target`; the current order dropped first
  // when that behaviour is +0x23c's (none). False when the net is another.
  bool go_to(AiNet* n, Behaviour* target);
  Order* top_unmarked();                           // 0x004e8c60
  void unwind_to(Order* o);                        // 0x004e8990
  bool stop();                                     // vt+0x40 0x004ea4e0
  // The player's lance orders (the brain's vt+0x20..+0x48, docs/ai.md 17.6),
  // each on the contact on the player's target: follow (and so fight) its
  // vehicle (vt+0x20 0x004ea450; +0x24 / +0x34 0x004ea4a0 the same); only
  // when it is no ally (+0x2c / +0x44 0x004ea390) or only when it is one
  // (+0x30 / +0x48 0x004ea3f0); form on the player (vt+0x28 0x004ea4b0: the
  // formation net); refit (vt+0x3c 0x004ea260, a hook); +0x38 (0x0058b380)
  // nothing. Each answers whether it was taken.
  bool attack_target(Contact* c);
  bool form_on_me();
  bool enemy_only(Contact* c);
  bool ally_only(Contact* c);
  // 0x004ea000..0x004ea230 (the order's number as the dispatcher 0x00469940
  // gives it, 0..12 but 2), then 0x004e9fd0: with a radio, disregard_order
  // when refused, else the order's line (follow, stop, confirm_order,
  // confirm_refit, disengages_order). The answer.
  int32_t lance_order(int32_t order, Contact* c);
  // Mech3.exe 1.2 @ 0x004ea520 (the AI's IControls vt+0x94): the base's,
  // then the radio: an enemy's hit with the player over sqrt(500) m off
  // (call_help), the player's own (friendly_fire), and past 3 s of his hits
  // in a run the lance turns on him (0x004d5dc0).
  void on_hit(HitRecord& r, zgame::Handle owner, zgame::Handle attacker);
  // 0x004ea630 (IControls vt+0xb4): it killed `victim` (last_shot unless an
  // ally). 0x004ea6b0's line (vt+0xa8, after the controls' tick): ejecting.
  void killed(zgame::Handle owner, zgame::Handle victim);
  void died() {
    if (radio) brain_hooks().radio_line(*this, 6);  // ejecting (radio +0x18)
  }
  // 0x004ea500 -> 0x004b78d0 (IControls vt+0x98): it hit an ally: the firing
  // passes over targets behind that one and stands down.
  void hit_an_ally(zgame::Handle victim) {
    firing.obstacle = victim;
    firing.stand_down();
  }
  bool may_fire();                                 // 0x004e9070
  // The vlist triggers (docs/ai.md 17.5): 0x004e9550 -> 0x004d9290 over the
  // settings' list, the nearest enemy picked once between them and the frame.
  void vlist(BrainControls& c, zgame::Handle owner, bool* picked, Contact** target);
  void pilot_tick(BrainControls& c);               // IControls vt+0xa4 0x004e94f0: the same, nothing picked yet
  // From the IControls side, given a znet object (its vt+0x34 cast).
  bool pop_if_object(znet::Object* o);             // 0x004e8660
  bool push_object(znet::Object* o);               // 0x004e8960
  bool clear_if_object(znet::Object* o);           // 0x004e9100
  void push_order();  // 0x004e9d80 -> 0x004e9d90: a new order without parts (the template's)
  // 0x004e81a0 (the own order's vt+0x04, with params +0x80 at 1): with no
  // state yet, the nearest contact of any side (0x004d6b90) within +0x74
  // (squared) gives its vehicle's own net a part in the order and that net's
  // own-order behaviour (slot 5) its state (both dropped when there is no
  // state); then the part's and the state's runs.
  void run_own_order(float dt);

  // The controls it is a base of in the exe (set by the frame and by the AI
  // controls): what its orders' states steer.
  BrainControls* controls = nullptr;
  // A scan or canleave step's test on this brain (0x004d9c20..0x004d9ca0).
  void scan_test(ScanTest t);
  // 0x004d8930 (the AI controls' IControls vt+0x80, as the firing starts to
  // aim): not attacking, with scan_aim on, a manoeuvre each +0xa8 seconds.
  bool scan_aim();

  // Mech3.exe 1.2 @ 0x004e9710: the frame (0x004e96f0 answers it): the
  // targets set and the controls smoothed (ai.md 17.1).
  bool frame(BrainControls& c, float dt);

  // The gates (docs/ai.md 17.3), given the pilot's vehicle.
  bool may_engage(zgame::Handle owner, AiNet* net, BehaviourState* state);  // 0x004e8f10
  bool must_break_off(zgame::Handle owner);                                 // 0x004e8dd0
  bool p6_clear() const;            // vt+0x0c 0x004e9640: +0x54 is 0
  bool sees_target(zgame::Handle owner);  // vt+0x10 0x004e9650
  void manoeuvre();                 // 0x004e96b0

  AiParams params;          // +0x00  0x004d9bf0 -> 0x004d9940 (vtable 0x005cd360, the brain's 0x005cc698)
  float xdc = 3.4028235e38f;  // +0xdc  FLT_MAX (0x004d9bf0)
  // +0xe0: the virtual base pointer.
  const float* xe4 = nullptr;  // +0xe4  the home point the leash measures from (0x004e8f10)
  // The home points the net parts stack on +0xe4 (the vehicle net's and the
  // attack's): each live part's home and its "under" (the +0xe4 it found).
  // FIX (bug): the exe keeps only the under pointers, so a part that goes
  // before one stacked on it leaves that one's under pointing at freed
  // memory (the own order's part, made during a fight, outliving the
  // fight's); here a part going from the middle hands its under to the one
  // above it. In stack order it is the exe's, word for word (docs/fixes.md).
  struct HomeEntry {
    const float* home;
    const float** under;
  };
  std::vector<HomeEntry> homes;
  void home_push(const float* home, const float** under);  // *under = +0xe4, +0xe4 = home
  void home_pop(const float* home);                        // its entry gone: +0xe4 = *under if on top
  float xe8 = 3.4028235e38f;  // +0xe8
  float xec = 1.0f;           // +0xec
  int32_t xf0 = 0;            // +0xf0
  float xf4 = 3.4028235e38f;  // +0xf4
  uint8_t xf8 = 0;            // +0xf8
  float xfc = 0;              // +0xfc  when scan_aim may start a manoeuvre again (0x004d8930)
  float x100 = 0;             // +0x100  when a hit may start the firing again (0x004e9710)
  uint8_t x104 = 0;           // +0x104  the order asks to run again
  Firing firing;              // +0x108
  // +0x160: a std::deque<Order> (0x004e82a0: allocator byte, two
  // iterators, map, map size, size; blocks of 26 orders, 0xf70 bytes) and
  // at +0x190 its owner, this brain. The last is the current order (+0x17c).
  std::deque<Order> orders;
  const void* order_block = nullptr;  // the deque's first block and map, for the tests
  const void* order_map = nullptr;
  Order order;                                     // +0x194  0x004e8180, its owner this brain
  uint8_t x22c = 2;           // +0x22c  hurt: 2 not decided this frame, 0 / 1 (0x004e8f10)
  float x230 = 0;             // +0x230  the health an attack gives up at (0x004e9710)
  char* x234 = nullptr;       // +0x234  0x0049d180: a string holder, empty
  PathHolder path;            // +0x238  the way finder (0x004c7c10), deleted by the dtor
  Pilot path_pilot;           // ours: what the finder asks of the pilot (the exe passes the brain)
  void* x23c = nullptr;       // +0x23c  the same
  BrainStep step;             // +0x240
  Steer steer;                // +0x250
  float x298[3] = {};         // +0x298  0x0063d870
  uint8_t x2a4 = 0;           // +0x2a4
  Radio* radio = nullptr;     // +0x2a8  0x004e7e20

 private:
  void pop_order();                                // the pop_back of 0x004e9380 (no resume)
  void pop_resume();                               // the pop of 0x004e8380: then the new current state's resume
  void make_radio();                               // 0x004e7e20
};

// The AI vehicle controls (0x004e71c0 over 0x004e7090, 0xcd8 bytes from
// 0x004ac860, which zeroes them): the basic vehicle's controls (0x004ac750,
// veh_basic.cpp: their +0x764 block holds at +0x28 a pointer to the turret
// limits at +0x2c), the brain at +0xa04, the IControls virtual base at +0xcc4
// (its owner +0xcd4).
struct AiVehicleControls : VehicleControls, BrainControls {
  AiVehicleControls(void* owner, AiNet* net, const AiParams& rec);  // 0x004e71c0(owner, net, rec, 1)
  ~AiVehicleControls();  // IControls vt+0x00: 0x004e7820 -> 0x004e7870
  AiVehicleControls(const AiVehicleControls&) = delete;
  AiVehicleControls& operator=(const AiVehicleControls&) = delete;

  std::unique_ptr<Brain> brain;  // +0xa04 (a base in the exe)
  TurretAim aim;            // +0xcb0 (0x0063d870), +0xcbc
  HitRecord hit;            // +0xcc8 (IControls +0x04)
  bool can_aim(float cos_limit) override { return turret_can_aim(*this, aim, cos_limit); }  // vt+0x24 0x004e7490
  void aim_at(const float dir[3]) override { turret_aim_at(*this, aim, dir); }             // vt+0x28 0x004e7570
  bool destroyed() override;                                    // vt+0x1c 0x004e7470: the owner no longer active
  void turn_step(const VehicleMotion&, float) override { x50.y = top_turn * turn; }  // vt+0x14 0x004a2ae0
  bool on_path() override { return true; }                      // IControls vt+0x70 0x004e7790 -> 0x004a5270
  void died() override;                                         // IControls vt+0xa8 0x004e7800 -> 0x004ea6b0
  int update(float dt) override;                                // IControls vt+0x0c 0x004e7670 -> 0x004e71a0
  void on_hit(void* attacker);                                  // IControls vt+0x94 0x004ea520
  void hit_an_ally(void* victim) { brain->hit_an_ally(brain_hooks().vehicle_handle(victim)); }  // vt+0x98 0x004ea500
  void tick() { brain->pilot_tick(*this); }                     // IControls vt+0xa4 0x004e94f0
  void on_kill(void* victim);                                   // IControls vt+0xb4 0x004ea630

  // What the brain asks of them (BrainControls): the vehicle controls' own
  // IControls slots (veh_controls), the smoothing the turret's turn.
  zgame::Handle vehicle() override { return brain_hooks().vehicle_handle(owner); }
  void stop() override { clear_commands(); }                    // IControls vt+0x14 0x004ad640
  void smooth(float dt) override { turret_turn(*this, aim, dt); }  // +0xa04 vt+0x04 0x004e72f0
  bool hit_lately() override;                                   // IControls vt+0xa0 0x004aed40
  zgame::Handle attacker() override { return hit.by; }
  void fire(bool on) override { fire_on = on ? 1 : 0; }         // IControls vt+0x10 0x004ad720
  bool reversing() override { return throttle < 0.0f; }         // IControls vt+0x18 0x004a5b20
  float throttle_now() override { return throttle; }            // IControls vt+0x1c 0x004a5b50
  void set_throttle(float v) override { throttle = v; }         // IControls vt+0x20 0x004a5b70
  void set_turn(float v) override { turn = v; }                 // IControls vt+0x28 0x004a5bb0
  void set_jump(float v) override { (void)v; }                  // IControls vt+0x34 0x004a5bc0
  void set_jet_forward(float v) override { (void)v; }           // IControls vt+0x3c 0x004a5bc0
  float turn_goal() override { return turn; }                   // IControls vt+0x30 0x004aed90 -> vt+0x24 0x004a5b90
  void aim_started() override {}                                // IControls vt+0x80 0x0040bae0
  // owner: the base's (IControls +0xcd4 here)
};

// Mech3.exe 1.2 @ 0x004d9290: each trigger of a settings' vlist, in order
// (0x004d8e70); the player's controls run their own (0x004d8990, not ported).
void vlist_run(const AiParams& p, BrainControls& c, zgame::Handle owner, bool* picked, Contact** target);

// The default net for a vehicle without its own: znet's object of the
// record's first item, as a compiled net (vt+0x28, vt+0x34), else the
// built-in one.
AiNet* brain_net_for(const AiParams& rec);

}  // namespace mech3::ai
