#pragma once
#include <cstdint>
#include <list>
#include <vector>

#include "gamez/zgame/unported.h"
#include "mech3/zai_target.h"

// C:\zipper\Mech3\turret.cpp: the turrets (docs/vehicles.md, The turrets).
// aiTurrets.zrd's TURRET block names node prefixes; each world node that
// starts with one and has an active `healthy` child becomes a turret, an
// AiTarget with its own frame (0x004a6be0; 4c2), damage and death. The
// constructor and its helpers sit just before the named functions
// (0x004a4ac0..0x004a6230), the limits classes further back (0x004a4000..).
namespace zrdr {
struct Node;
}

namespace mech3 {
struct Ams;
}

namespace mech3::tur {

using zgame::Handle;

// A turret's turn limits (+0x210, built in place by 0x004a4b30; +0x20c
// points at it): the gun's pitch as sines and cosines (0x004a4000), and by
// the yaw's range one of four kinds. The kind is the exe's vtable. A new
// kind is built over the old one: fields it does not set keep their bytes.
// The aim asks within (vt+0x08); nothing in the turret asks clamp (vt+0x04).
enum class LimitKind : uint32_t {
  Fixed = 0x005c8310,   // 0x004a42f0: a range of 0.001 or less (or NaN): the low yaw alone
  Narrow = 0x005c8320,  // 0x004a4580 over 0x004a4270: under pi
  Wide = 0x005c8330,    // 0x004a4800 over 0x004a4270: under 2 pi
  Full = 0x005c8340,    // 0x004a4a70: a full turn (veh_controls' TurretLimits)
};
struct TurnLimits {
  LimitKind kind = LimitKind::Full;
  float lo_sin = 0, lo_cos = 0;  // +0x04
  float hi_sin = 0, hi_cos = 0;  // +0x0c
  // Fixed: (cos yaw, 0, -sin yaw). Narrow and Wide: the low edge
  // (-cos lo, 0, sin lo), then the high one (cos hi, 0, -sin hi).
  float lo_edge[3] = {};  // +0x14
  float hi_edge[3] = {};  // +0x20
  // Mech3.exe 1.2 @ 0x004a4b30: the pitch from lo to hi, the yaw from ylo to yhi.
  void make(float lo, float hi, float ylo, float yhi);
  void make_full(float lo, float hi);  // 0x004a4a70
  // vt+0x08: the direction's pitch inside the limits (0x004a4180, NaN
  // within), then its heading: Fixed 0x004a4330 (along the one heading,
  // either way), Narrow 0x004a45b0 (inside both edges), Wide 0x004a4830
  // (inside either).
  bool within(const float d[3]) const;
};

// The pick's turn by game time (FIX (B), behind per_second): one turn in `n`
// 1/30 s steps, the one leaving `id`; `seen` the last step asked at (-1 none).
bool turn_due(int64_t& seen, float clock, uint32_t n, int32_t id);

// Mech3.exe 1.2 @ 0x004abea0 (between turret.cpp and veh_basic.cpp): a
// direction given the pitch's sine `slope`, its heading kept: the flat part
// scaled to the rest of the length by the same bit-trick square root; with
// no heading, (f, slope, f), or (0, 0, 1) at slope 0.
void lead_dir(float d[3], float slope);

// The weapons callback 0x004a7940 installs on the node (0x005833b0): a hit
// goes to the turret's damage 0x004a7530.
constexpr uint32_t kHitCallback = 0x004a7940;

// 0x284 bytes, vtable 0x005c8770 (55 slots; the shared ones are zEntity3d's
// and AiTarget's). The ZEntity3d part is the port's few fields, as in
// CZAiTarget. Offsets in the comments are the exe's.
class Turret : public ai::AiTarget {
 public:
  Handle self = 0;           // its handle as the hooks see it (the original's block)
  Handle node = 0;           // +0x004  zEntity3d's node
  uint8_t active = 0;        // +0x008  set by the setup when the healthy node is there
  uint8_t sensed = 1;        // +0x009  (ZEntity) the sensors' "update me": the frame runs while set
  Handle target_node = 0;    // +0x010  zEntity's (0x004d6c90 sets them)
  Handle centroid_node = 0;  // +0x014
  uint8_t zones[4] = {};     // +0x064  (zEntity3d) its zone record: a count, then ids (0x005a80f0)
  // +0x0d4: the object of the last hit's attacker, credited with the kill
  // (vt+0x80) when the turret goes; the player's vehicle when it is killed
  // outright (vt+0x70). (In the other vehicles +0xd4 is the weapons.)
  Handle attacker = 0;
  int32_t id = 0;            // +0x0d8  the count at [0x00609a48] when made: which frame it picks targets
  Handle healthy = 0;        // +0x0dc  the node's `healthy` child
  float pos[3] = {};         // +0x0e0  the node's world position (0x004ed650)
  float aim_from[3] = {};    // +0x0ec  pos raised by the turret's, gun's and firepoints' heights
  Handle off_node = 0;       // +0x0f8  DEACTIVATE: a node, then a path of children under it
  Handle turret = 0;         // +0x0fc  PARTS' turret (the yaw)
  Handle turret_matrix = 0;  // +0x100  its matrix (0x004fc3f0)
  Handle gun = 0;            // +0x104  PARTS' gun (the pitch)
  float muzzle[2][3] = {};   // +0x108  the firepoints' translations (0x004fc020)
  Handle gun_matrix = 0;     // +0x120
  float facing[3] = {0, 0, -1};  // +0x124  the node's rotation (0x004fbe90), normalised
  Handle effect = 0;         // +0x130  EFFECT's node, shown while firing
  float effect_time = 0;     // +0x134  EFFECT's second value
  uint8_t ams_on = 0;        // +0x138  AMS_RANGE given
  uint8_t ams_found = 0;     // +0x139
  Ams* ams = nullptr;        // +0x13c  owned
  Handle weapon = 0;         // +0x140  WEAPON's NAME (zWeapon's definition, 0x00581ed0)
  int32_t ammo = 50;         // +0x144  AMMO
  float detect_range = 200.0f;  // +0x148  DETECTION_RANGE, else the weapon's +0x1c
  uint8_t aimed = 0;         // +0x14c
  int32_t firepoint = 0;     // +0x150  the one that fired last
  int32_t firepoints = 0;    // +0x154  PARTS: 4 names 2, 3 or 2 names 1
  Handle fire_node[2] = {};  // +0x158
  float fire_dir[3] = {0, 0, -1};  // +0x160
  float fire_from[3] = {};   // +0x16c
  float x178[3] = {};        // +0x178
  float rate = 1.0f;         // +0x184  FIRE_RATE: seconds between shots
  float next_fire = 1.0f;    // +0x188
  float burst_end = 0;       // +0x18c
  float burst[2] = {};       // +0x190  FIRE_LIMITS: a burst's length, then the pause after it
  Handle beam = 0;           // +0x198  a beam weapon's beam (0x00584a70)
  uint8_t beam_on = 0;       // +0x19c
  // +0x1a0 / +0x1a4: what the shot is fired as (0x00581f60 is handed
  // both): 1 and the player's vehicle, set by each pick.
  int32_t x1a0 = 0;          // +0x1a0
  Handle shooter = 0;        // +0x1a4
  Handle target_at = 0;      // +0x1a8  the picked target's position (3 floats; read through the hooks)
  int32_t x1ac = 0;          // +0x1ac
  int64_t turn_seen = -1;    // ours: the last 1/30 s step the pick asked at (the turn by time, FIX (B))
  float yaw_lo = -6.2831855f;  // +0x204  LIMITS' YAW in radians
  float yaw_hi = 6.2831855f;   // +0x208
  TurnLimits limits;         // +0x210 (+0x20c points at it)
  Handle destroy_anim = 0;   // +0x23c  DESTROY_ANIM, else GENERIC_DESTROY_ANIM's
  float health = 1.0f;       // +0x240  a fraction
  float per_damage = 1.0f;   // +0x244  1 / HEALTH
  uint8_t picked = 0;        // +0x248
  std::list<Handle> wakeup;  // +0x250  WAKEUP_ANIM's
  std::list<Handle> onfire;  // +0x25c  ONFIRE_ANIM's, run at each shot
  std::list<Handle> aux;     // +0x268  AUX_ANIM's, claimed (0x004d4f20) and reset at load
  std::list<Handle> sleep;   // +0x274  SLEEP_ANIM's
  uint8_t idle = 0;          // +0x27c  IS_IDLE: holds fire (the messages 0x32 / 0x33 clear and set it)
  Handle fire_sound = 0;     // +0x280  SOUNDS' FIRE

  // 0x004a5c60: on `node` (none: not placed), the definition `def`.
  Turret(Handle self, Handle node, Handle generic_anim, zrdr::Node* def);
  ~Turret();  // 0x004a5ff0 (0x004a5fd0)
  Turret(const Turret&) = delete;
  Turret& operator=(const Turret&) = delete;

  void set_node(Handle n);                     // 0x004a6230 (n, 0, 0, 0)
  void setup(Handle generic_anim, zrdr::Node* def);  // 0x004a6270
  void aux_reset();                            // 0x004a7af0
  void message(uint32_t from, int32_t msg);    // 0x004a74f0 (vt+0x74)
  uint32_t hit(Handle weapon, Handle x, float damage);  // 0x004a7530
  void kill();                                 // 0x004a7600 (vt+0x70)
  void deactivate(bool destroyed);             // 0x004a7620
  void tint(bool on);                          // 0x004a75b0 (vt+0x68)
  bool idle_now() const { return idle == 1; }  // 0x004a7a60 (vt+0xa0)
  // 0x004a7a70 (vt+0xcc, the sensors' "alive"): active, not idle, and its
  // zones meet the player's (0x0053cc90).
  bool awake() const;
  // 0x004a7ab0 (vt+0xd0, the level of detail's): active and the player's
  // contact inside the detection range.
  bool in_range() const;
  float alive() const { return active ? 1.0f : 0.0f; }  // 0x0046b780 (vt+0x98)

  // 0x004a6be0 (vt+0x34, its argument unused): the frame (docs/vehicles.md,
  // The turrets' frame).
  void frame();
  bool pick(Handle player);           // 0x004a70d0
  void pick_enemy();                  // 0x004d72b0 (AiTarget's): the nearest enemy contact in sight
  bool target_player(Handle player);  // 0x004a7b40 with 0x004a7ba0: the player's contact the target, else none
  bool aim(Handle at);                // 0x004a71c0
};

// Mech3.exe 1.2 @ 0x004a76b0: aiTurrets.zrd's turrets made on the world's
// nodes and placed (vt+0x38 with `at`); the three HUD markers first.
// False when the file is not there.
bool turrets_load(const char* file, Handle at);
void turrets_end();                  // 0x004a7970: the markers and the file freed
Handle turret_marker(int32_t side);  // 0x004a5fc0 (vt+0x3c): ally, enemy, other
// Ours: the turrets made, in order (the exe keeps them in zEntity's list
// only, which deletes them through vt+0x00); each deleted.
std::vector<Turret*>& turret_list();
void turrets_delete_all();

// The calls out, by the original's address.
class TurretHooks : public zgame::Unported {
 public:
  // A new turret's handle (operator new 0x284).
  virtual Handle turret_handle(const void* mem) { (void)mem; return 0; }
  virtual void turret_free(Turret* t) { delete t; }
  // The HUD's markers (zTwoD images 0x0059b830 with the vtable 0x005bce78, +0x48 1).
  virtual Handle marker_create(const char* name) { return unported("0x0059b830", {name, 0, 0, 1}); }
  virtual void marker_delete(Handle m) { unported("marker.vt+0x0", {m, 1}); }
  // zReader: the file (0x00556740) and its tree freed (0x00556820).
  virtual zrdr::Node* read(const char* file);
  virtual void free_tree(zrdr::Node* root);
  virtual void report(int32_t line, const char* fmt, const char* name) {
    unported("0x0049bfd0", {0x200, "C:\\zipper\\Mech3\\turret.cpp", line, fmt, name});
  }
  // zClass: list 6 by prefix (0x004f8b80), lookups, flags, matrices.
  virtual Handle nodes_first(const char* prefix) { return unported("0x004f8b80", {prefix, 6}); }
  virtual Handle nodes_next() { return unported("0x004f8b80", {0u, 6}); }
  virtual const char* node_name(Handle node) { (void)node; return ""; }       // +0x00
  virtual uint32_t node_flags(Handle node) { (void)node; return 0; }          // +0x24: 4 active
  virtual Handle node_model(Handle node) { (void)node; return 0; }            // +0x3c
  // Two searches of a node's tree by name, the node itself first: children
  // last to first (0x004f9d60), first to last (0x004eb0d0, which reports a
  // null node).
  virtual Handle node_find(Handle root, const char* name) { return unported("0x004f9d60", {root, name}); }
  virtual Handle node_child(Handle node, const char* name) { return unported("0x004eb0d0", {node, name}); }
  virtual Handle node_named(const char* name) { return unported("0x004f7fe0", {6, name}); }
  virtual void node_set_active(Handle node, int32_t on) { unported("0x004eb170", {node, on}); }
  virtual void node_set_box_hit(Handle node, int32_t on) { unported("0x004eb750", {node, on}); }  // flag 0x20
  virtual void node_world_position(Handle node, float out[3]);                    // 0x004ed650
  virtual void node_rotation(Handle node, float out[3]);                          // 0x004fbe90
  virtual void node_translation(Handle node, float out[3]);                       // 0x004fc020
  virtual Handle node_matrix(Handle node) { return unported("0x004fc3f0", {node}); }
  virtual float matrix_y(Handle m) { (void)m; return 0; }                        // its +0x28
  virtual void node_set_translation(Handle node, float x, float y, float z) {
    unported("0x004fc0b0", {node, zgame::bits(x), zgame::bits(y), zgame::bits(z)});
  }
  virtual int32_t node_box(Handle node, float box[6]);                            // 0x004ebd50: 0 when it has one
  virtual void nodes_pending() { unported("0x004f7e50", {}); }
  virtual void node_tint(Handle node, float amount) {                             // 0x004fb8c0 with [0x005e8398]
    unported("0x004fb8c0", {node, 0x5e8398u, zgame::bits(amount)});
  }
  virtual void node_tinted(Handle node, int32_t on) { unported("0x004fb840", {node, on}); }
  virtual void model_scroll(Handle model) {                                       // 0x00526ae0(1, 10, 0)
    unported("0x00526ae0", {model, 1, zgame::bits(10.0f), 0});
  }
  // zEffect's animations.
  virtual Handle anim_find(const char* name) { return unported("0x0050b450", {name}); }
  virtual void anim_reset(Handle anim, Handle node) { unported("0x00508460", {anim, node}); }
  virtual void anim_run(Handle anim, Handle node) { unported("0x00508c10", {anim, node, 0, 0, 0}); }
  virtual uint8_t anim_claim(Handle self, Handle anim) { return static_cast<uint8_t>(unported("0x004d4f20", {self, anim})); }
  // zSound and zWeapon.
  virtual Handle sound_find(const char* name) { return unported("0x0056f6f0", {name}); }
  virtual Handle weapon_def(const char* name) { return unported("0x00581ed0", {name}); }
  virtual float weapon_range(Handle def) { (void)def; return 0; }              // +0x1c
  virtual uint32_t weapon_flags(Handle def) { (void)def; return 0; }           // +0x6c: 2 a beam
  // The pointers are into the turret (its +0xe0, +0x160, +0x16c, +0x178).
  virtual Handle beam_make(Handle def, Handle node, Handle gun, const float* from, const float* dir) {
    (void)from, (void)dir;
    return unported("0x00584a70", {def, node, 0, gun, 0, 0, 2});
  }
  virtual Handle beam_free(Handle beam) { return unported("0x00584b40", {beam}); }
  virtual void beam_stop(Handle beam) { unported("0x00581400", {beam}); }
  virtual int32_t set_hit_callback(Handle self, Handle node) {
    return static_cast<int32_t>(unported("0x005833b0", {self, node, 1, kHitCallback}));
  }
  virtual void remove_handlers(Handle node) { unported("0x00583400", {node}); }
  virtual Handle hit_attacker() { return unported("0x00581e40", {}); }
  virtual Handle node_object(Handle node) { return unported("0x0040eee0", {node}); }
  virtual void hit_killed() { unported("0x00583710", {1, 0}); }
  virtual void hit_health(float fraction) { unported("0x00583730", {zgame::bits(fraction)}); }
  virtual void credit_kill(Handle object, Handle self) { unported("obj.vt+0x80", {object, self}); }
  virtual Handle player_vehicle() { return unported("[0x0060861c]+0x14", {}); }
  // 0x0053cc90 on the two zone lists (+0x64 of each).
  virtual int32_t zones_meet(Handle player, const Turret& t) {
    return static_cast<int32_t>(unported("0x0053cc90", {player + 0x64, t.self + 0x64}));
  }
  // zEntity3d and AiTarget.
  virtual void entity_init(Handle self) { unported("0x005a7e10", {self, 0}); }
  virtual void entity_end(Handle self) { unported("0x005a7f90", {self}); }
  virtual void entity_list(Handle self) { unported("0x005a7f50", {self}); }
  virtual void entity_unlist(Handle self) { unported("0x005a7f70", {self}); }
  virtual void entity_set_node(Handle self, Handle node, Handle* target_node, Handle* centroid_node) {
    unported("0x004d6c90", {self, node, 0, 0, 0});
    *target_node = *centroid_node = 0;
  }
  virtual void entity_place(Handle self, Handle world, Handle at) { unported("0x005a80f0", {self, world, at}); }
  virtual Handle world_node() { return unported("[0x00608c84]+4", {}); }
  virtual void entity_markers(Handle self) { unported("0x005a8500", {self}); }
  virtual void entity_refresh(Handle self) { unported("0x005a85c0", {self}); }
  virtual void target_message(Handle self, uint32_t from, int32_t msg) { unported("0x004d7270", {self, from, msg}); }
  virtual void sorted_clear() {}  // [0x0060b56c] = 0

  // The frame's (4c2). The clocks: zClass's frame count [0x0072f338], the
  // game clock [0x0072c530] and the frame's dt [0x0072c52c].
  virtual uint32_t frame_number();
  virtual float clock();
  virtual float frame_dt();
  virtual void node_world_cached(Handle node, float out[3]);  // 0x004ed530
  virtual void read_vec(Handle at, float out[3]) { (void)at; out[0] = out[1] = out[2] = 0; }
  // The matrix stack (zMath): push, identity, a node's world matrix (mode
  // 3), a point through it, a direction through its inverse, pop.
  virtual void matrix_push() { unported("0x0051e9d0", {}); }
  virtual void matrix_identity() { unported("0x0051fde0", {}); }
  virtual void node_to_world(Handle node) { unported("0x004ed280", {node, 3}); }
  virtual void point_through(float v[3]) { unported("0x005227a0", {1}); (void)v; }
  virtual void dir_through_inverse(float v[3]) { unported("0x00522640", {1}); (void)v; }
  virtual void matrix_pop() { unported("0x0051ea00", {}); }
  virtual float matrix_at(Handle m, int32_t i) { (void)m, (void)i; return 0; }  // the node's 3x4, +4 i
  virtual void matrix_put(Handle m, int32_t i, float v) { (void)m, (void)i, (void)v; }
  virtual void node_set_matrix(Handle node, Handle m) { unported("0x004fc300", {node, m}); }
  virtual void effect_animate(Handle model) { unported("0x00541ad0", {model}); }
  // The weapon's: its definition's +0x4c (a lead), +0x84 bit 1 (fired from
  // the firepoint, 0x00581fb0), +0x6c bit 0x4000 (it goes off where it stands).
  virtual float weapon_lead(Handle def) { (void)def; return 0; }
  virtual uint32_t weapon_fire_flags(Handle def) { (void)def; return 0; }
  virtual void detonate(Handle def, const float* at) { unported("0x005820c0", {def, 0, 0}); (void)at; }
  // 0x00581ff0: the arc's slope from `from` to the target, -1 for none.
  virtual float aim_slope(Handle def, const float* from, Handle to) {
    (void)from;
    return zgame::from_bits(unported("0x00581ff0", {def, 0, 0, to}));
  }
  // 0x00581f60: the next shot's lock, the turret's +0x1a0 and its lock
  // (+0x1a4: the player, the picked place, no velocity); off: none.
  virtual void shooter_set(Turret& t, bool on) { unported("0x00581f60", {on ? t.self + 0x1a0 : 0u, on ? t.self + 0x1a4 : 0u}); }
  virtual void attach_fire(Handle def, Handle node) { unported("0x00581fb0", {def, node}); }
  virtual void beam_start(Handle beam) { unported("0x00581230", {beam, 0}); }
  virtual void weapon_fire(Handle def, Handle node, const float* from, const float* dir, const float* velocity,
                           int32_t friendly) {
    (void)from, (void)dir, (void)velocity;
    unported("0x004b4a40", {def, node, 0, 0, 0, 0, 0, 0, friendly});
  }
  virtual void sound_play(Handle sound, const float* at) {
    (void)at;
    unported("0x0056e7e0", {sound, 0, zgame::bits(1.0f), 0});
  }
  // The player's team: its vehicle's vt+0x24, that one's vt+0x28, +0xc0.
  virtual uint32_t player_team(Handle player);
  // The AMS (0x004b3ae0 at the turret's place, 0x004b3ba0, 0x004b3bb0(1)).
  virtual bool ams_find(Ams& a, int32_t side, const float at[3]);
  virtual Handle ams_target(Ams& a);
  virtual void ams_fire(Ams& a);
  // The pick's: a contact's vehicle (0x004e6fe0), its AiTarget (vt+0x28)
  // and team (+0xc0), the line of sight (0x004d5980), vt+0xc0 0x004d5e40,
  // an entity's position (0x005a8630).
  virtual Handle contact_vehicle(const ai::Contact* c) { (void)c; return 0; }
  virtual Handle vehicle_target(Handle v) { return unported("veh.vt+0x28", {v}); }
  virtual uint32_t target_team(Handle t) { (void)t; return 0; }
  virtual bool line_clear(Handle self, Handle v) { return unported("0x004d5980", {self, v, 0}) != 0; }
  virtual void set_target(Turret& t, ai::Contact* c) { t.ai::AiTarget::set_target(c, false); }
  virtual Handle entity_position(Handle e) { return unported("0x005a8630", {e}); }
};
void set_turret_hooks(TurretHooks* h);
TurretHooks& turret_hooks();

// The count at [0x00609a48] (the next turret's id) and the pick's turn
// [0x00609a4c]; the mission's end does not reset them.
int32_t& turret_count();
int32_t& turret_turn();

}  // namespace mech3::tur
