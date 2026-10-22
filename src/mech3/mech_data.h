#pragma once
#include <cstdint>
#include <list>

#include "gamez/zgame/unported.h"
#include "mech3/mech.h"

namespace zrdr {
struct Node;
}

// A mech type's animations, loaded once and copied to each mech of it
// (docs/movement.md, The gait table). Original file: mech3\mech_data.cpp
// (0x004308a0..0x00431a70), its getters just after. The object is the
// mech's +0x684, here `MechData` (mech.h).
namespace mech3 {

struct MechDfn;

// The calls the animation load makes (a test's stand-ins). Unset: dfn is
// mech_dfn(roster); canned is the port's load_canned.
struct MechDataCalls {
  const MechDfn* (*dfn)(MechData&) = nullptr;               // 0x004846a0 on +0x0c
  bool (*canned)(MechData&, zrdr::Node* canned, int i) = nullptr;  // 0x00431750: the canned effects (+0x14c + 0x14 i)
};
MechDataCalls& mech_data_calls();

// The mech data's calls out of mech_data.cpp (docs/mech-build.md section 14),
// each named by the original's address; the defaults run the ports where
// there are ports.
class MechDataHooks : public zgame::Unported {
 public:
  static uint32_t low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  static Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
  virtual uint32_t ref(const void* p) { return low(p); }
  virtual void on_new(uint32_t size, const void* p) { (void)size, (void)p; }  // the exe's operator new
  virtual Handle mech_model(int32_t roster, int32_t variant);  // 0x00484760: mech3::mech_model
  virtual void node_set_intersect(Handle node, int32_t on);    // 0x004eb6d0: zcls
  virtual void message_box(int32_t a, int32_t b, int32_t id, const char* arg) {  // 0x004ac150
    unported("0x004ac150", {a, b, id, arg});
  }
  virtual void zerr(int32_t level, const char* file, int32_t line, const char* fmt, const char* arg) {
    unported("0x0049bfd0", {level, file, line, fmt, arg});
  }
  virtual const MechDfn* dfn(int32_t roster);                 // 0x004846a0: mech_dfn
  virtual void skins_apply(const void* skins, Handle node);  // 0x00475780: SkinSet::apply_first
  // 0x00430e70: the camera kinds (tether, attach, area, mcbtether, zippercam,
  // godseye) read from the definition into a list, not ported.
  virtual Handle cameras(MechData& p) { return h(unported("0x00430e70", {ref(&p)})); }
  virtual void cameras_free(Handle list) { unported("0x0040abf0", {low(list)}); }
  virtual void hud_free(Handle hud) { unported("0x0043b860", {low(hud)}); }
  virtual Handle sound_find(const char* name);                // 0x0056f6f0: zsnd::find
  virtual Handle anim_find(const char* name) { return h(unported("0x0050b450", {name})); }
  virtual Handle node_named(int32_t list, const char* name);  // 0x004f7fe0: zcls::list_find_by_name
  // The pilot's game ([0x0060861c] +0x6c, vt+0x2c): 3 (0x00479480) or 2 (0x00479460).
  virtual uint32_t game_multiplayer() { return unported("0x00479480", {}); }
  virtual uint32_t game_instant_action() { return unported("0x00479460", {}); }
  virtual void animations(MechData& p);                          // 0x00431a70: load_animations
  // zReader on the definition's tree.
  virtual Handle reader_find(Handle node, const char* key);   // 0x005569e0
  virtual bool reader_vec3(Handle node, const char* key, float out[3]);  // 0x005570b0
  virtual const char* reader_string(Handle node, const char* key);       // 0x005569f0
  virtual const char* reader_value(Handle node);  // a found list's first item, read inline
  // The canned effects (0x00432830, 0x00431800); the defaults run the ports.
  virtual Handle node_copy(Handle node);                          // 0x004f9af0(node, 0, 0): zcls::tree_copy
  virtual void node_place(Handle node, float x, float y, float z);  // 0x004fc0b0
  virtual Handle effect_start(Handle effect, Handle node, const float args[9]);  // 0x00508aa0: zeff::anim_start_at
  virtual void node_free(Handle node);  // 0x00431800 on each: off its parents, its animations stopped, destroyed
  // The cockpit (0x00430ad0's second half, make_cockpit); the defaults run the ports.
  virtual Handle hud_model(int32_t roster);                       // 0x004847e0: mech3::hud_model
  virtual Handle object3d_create();                               // 0x004fb740
  virtual void node_set_name(Handle node, const char* name);      // 0x004eb300
  virtual void node_set_flag8(Handle node, int32_t on);           // 0x004eb620
  virtual void anim_restart(Handle anim, Handle node);            // 0x00508460 (EDX node): zeff::anim_restart
};
void set_mech_data_hooks(MechDataHooks* h);  // null: the defaults
MechDataHooks& mech_data_hooks();

// Mech3.exe 1.2 @ 0x00430ad0, after the HUD: the cockpit block at +0x228, the
// type's cockpit model (mech_<name>_hud.flt) or an empty object3d named
// "null_hud_cage". A model with a "hud" block in the definition is put at the
// origin, left out of the queries and flag 8 cleared, and the cockpit's
// explosion (+0x200) restarted.
void make_cockpit(MechData& p);

// The mech types' data (0x00605704, a std::list, count 0x00605708).
std::list<MechData*>& mech_data_cache();
// 0x00430d10 (ECX roster, EDX create): the cached data, else (with create) a
// new one loaded; its count of holders goes up either way. 0 when it failed
// to load, though it stays cached (and is not tried again).
MechData* mech_data(int32_t roster, int32_t create);
void mech_data_init(MechData& p);                 // 0x00430490
MechData* mech_data_new(int32_t roster);          // 0x004303d0: init, load, onto the cache
bool mech_data_load(MechData& p);                 // 0x004308a0
void mech_data_free(MechData* p);                 // 0x00430580 and the delete
void mech_data_clear();                        // 0x004318e0, at the mission's unload

// A canned effect (MechData::canned: 0 eject, 1 jump jets, 2 narc) started at
// `node` at its offset plus `at`; with no node, at a copy of the slot's node
// (kept for canned_nodes_free), none when the slot has no node. `at` and
// `vel` null stand for the exe's zero vector 0x0063d870, and a null `at`
// leaves the copy where it is. 0x00432830 (ECX +0x684).
Handle canned_effect(MechData& p, int32_t slot, Handle node, const float* at, const float* vel);
void canned_nodes_free();  // 0x00431800: the copies, at the mission's unload
int32_t canned_nodes_count();  // 0x00605718
void mech_data_refresh();                      // 0x004319a0, at each mission's load: 0x004319f0 on each
void load_ranges(MechData& p);                    // 0x00430840
// [0x005e2928], 1 at start: the options' mech detail scale on the ranges.
float& lod_scale();
void set_lod_scale(float s);                     // 0x00430440: stored, each cached type's ranges again
void load_sounds(MechData& p);                    // 0x004306d0
void load_effects(MechData& p);                   // 0x004320b0
void load_soils(MechData& p);                     // 0x00432670 with 0x004326d0
bool load_canned(MechData& p, Handle canned, int i);  // 0x00431750: eject, jumpjets, narc
const char* soil_name(int32_t i);              // 0x0047f380, "unknown" past 10

const char* gait_name(int i);     // 0x00431560: limp, limp, stand, step, walk, trot, run, ..., 12 stand
const char* action_name(int i);   // 0x004316e0: 1 fallf .. 8 collapser

// The engine's top speed (0x00433030 over 0x00433050 with the XL flag 1):
// rating / mass x 1.5 x 10.8 km/h, x 0.2778 to m/s.
float engine_speed(int32_t engine, float mass);
// The base speed of 0x00431350 (config 0's engine and mech, 0x00432950) and
// of 0x004329c0 (config 0's engine, the roster's mass).
float config_speed(MechData& p, bool roster_mass);

bool load_gait(MechData& p, zrdr::Node* motion, int i);   // 0x00431350
bool load_action_motion(MechData& p, int i);              // 0x004315d0
void load_animations(MechData& p);                        // 0x00431a70
MechAnim* gait_copy(const MechData& p, int i);            // 0x00431fb0
MechAction* action_copy(const MechData& p, int i);        // 0x00432030
// 0x004329c0: the gait table for a top speed, the file's pairs x base / max.
void scale_gaits(MechData& p, float gait[13][2], float max_speed);

}  // namespace mech3
