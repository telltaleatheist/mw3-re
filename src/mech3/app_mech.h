#pragma once
#include <cstdint>
#include <initializer_list>

#include <string>
#include <vector>

#include "gamez/zgame/unported.h"

// The mech build's and the launch's hooks wired to the program (app_mech.cpp,
// docs/mech-build.md section 9). Not original code. The engine's side comes
// as plain functions, so this header has neither of the two Handle types
// (app_hooks.h's numbers, mech.h's pointers).
namespace mech3 {
namespace ai { struct AiTarget; }

struct MechWiring {
  void* engine = nullptr;
  uint32_t (*unported)(void* e, const char* key, std::initializer_list<zgame::Arg> args) = nullptr;
  uint32_t (*peek32)(void* e, uint32_t a) = nullptr;
  void (*poke32)(void* e, uint32_t a, uint32_t v) = nullptr;
  void (*poke8)(void* e, uint32_t a, uint8_t v) = nullptr;
  void* (*object_of)(void* e, uint32_t h) = nullptr;
  uint32_t (*handle_of)(void* e, const void* p) = nullptr;
  uint32_t (*vcall)(void* e, uint32_t obj, uint32_t slot, std::initializer_list<zgame::Arg> args) = nullptr;
  uint32_t (*pilot)(void* e) = nullptr;   // [0x0060861c]
  uint32_t (*view)(void* e) = nullptr;    // [0x005f4c8c]
};
void wire_mech_build(const MechWiring* w);  // null: the hooks' defaults again
// The weapons' hooks (app_weapons.cpp): the manager's, zWeapon's, the
// queries; and a mech's damage handler (0x005833b0 with 0x004161a0).
void wire_weapons(uint32_t (*unported)(const char* key));  // what is not ported, logged once
void unwire_weapons();
void sync_frame_step(float dt);
// 0x00478f10 on the pilot the pilot screen chose (app_menus.cpp); false when none is.
bool program_record_mission(bool next);
int32_t mech_select_hot_group(void* mech);
bool mech_select_group(void* mech, int32_t g);
std::string mech_group_names(void* mech, int32_t g);
std::vector<std::string>& weapon_fx_log();
// Ours: what the mechs' parts played ("step <sound>", "print <effect>", "wake <level>").
std::vector<std::string>& mech_parts_fx_log();  // the first 4,096
// For the program's tests: hits the mechs' damage handler took (and the last
// shooter node), a mech's node, whether a group's weapons are firing or reloading.
int32_t mech_hits_taken(void** attacker_node);
void* mech_node(void* mech);
bool mech_group_reloading(void* mech, int32_t group);
void mech_after_tick(void* mech, uint32_t (*unported)(const char*));  // 0x0041aba0  // a group whose weapons heat (as F1..F5 would), -1 none  // [0x0072c52c] into the mech, weapon and zWeapon modules
void mech_set_damage_handler(void* mech);

// The mech types (mech_lib.h, mech_data.h) for the engine and the mission:
// mechlib opened with its voices (0x00484270), the cached types' sounds and
// effects made again for a new mission (0x004319a0), mechlib closed and the
// definitions freed (0x004844b0).
void mech_types_open();
void mech_types_refresh();
void mech_types_close();
// As a new process starts: no mech type cached (the exe clears them only at
// a mission's unload, 0x004318e0 in 0x00490480, which the program has not).
void mech_types_forget();
void mech_canned_nodes_free();
void mission_lose();
void mission_controls_free(void* controls);  // app_mission.cpp: an AI mech's controls (and brain) deleted
// A mech (one on zEntity's list) deleted (vtable +0 with 1); false for anything else.
bool mech_free(void* obj);
void mech_dead_tick(float dt);  // 0x0041e3f0, the player tick's: the dead mechs' frame
void mech_dead_clear();         // 0x00414000's list: emptied at the mission's cleanup
// 0x004782c0(0): the pilot's mech let go: its fire mode out (false without
// weapons), the mech globals cleared.
bool mech_player_release(void* mech, int32_t* fire_mode);
void* mission_player_mech();  // for the tests: world().mission_player ([0x0060861c] +0x14's mech)
int32_t mechs_alive();  // mech_build.cpp: made and not yet deleted
int mission_game_type();  // app_mission.cpp: the pilot's game's vt+0x2c (gametypes.md)  // app_mission.cpp: Missions::lose (0x0048f9e0), the player's mech destroyed  // 0x00431800: the canned effects' node copies

// What the last launch built (0x00491150), for the program's log and its tests.
struct LaunchedMech {
  int32_t roster = -1;     // the type
  int32_t parts = 0;       // hip, torso, head, arms, hands, legs, calves, feet with their nodes
  int32_t weapons = 0;     // mounted (the manager's count)
  int32_t sinks = 0;       // the heat sinks
  int32_t toes = 0;        // both feet's
  float height = 0;        // from the model's box
  float max_speed = 0;     // m/s, from the engine
  float torso_armor_scale = 0;  // the centre torso's 100 / armor points
  uint32_t team = 0;       // the AiTarget mask
  bool skis = false, ams = false;
  bool motions = false;    // the walk and its blends copied from the type's motions
  bool powering_down = false;  // asked to power down (0x0041cc30), or down
  bool powered_down = false;   // +0xde
  float pos[3] = {};       // the node's translation
  float yaw_sin = 0;       // the orientation's y (sin of half the yaw)
  bool in_world = false;   // the node's parent is the world node
  bool feet_down = false;  // both skis on the ground
  float armor = 0;         // armor and structure left, summed over the parts (fractions)
  float heat = 0;          // +0x554
  int32_t group = -1;      // the manager's selected group (+0x28), -1 without one
  const void* weapon = nullptr;  // that group's current weapon (+0x10)
  int32_t jets = 0;        // jump jets installed (equipment[11])
  int32_t jets_firing = 0; // +0x580
  bool jet_flame = false;  // +0x620: the jets' flame effect started (0x00432830)
  bool crouched = false;   // +0x586
  float alive = 0;         // +0x414  seconds of full updates
  int32_t anim_state = 0;  // +0x578
  bool blocked[4] = {};    // +0x374  the last collide: vehicle, mech or bump, ceiling, wall or touch
  const void* attacker = nullptr;  // the last mech to hit it
  int32_t action = 0;      // the action queue's front (0 none; 7, 8 the slow death falls)
  bool deadmech = false;   // the node renamed "deadmech" (the death)
  bool aim_torso = false;  // zEntity's target node (+0x10) is the torso's (0x0041d130)
  uint32_t kills[6] = {};  // the kill credit (+0x3f4): mechs, vehicles, buildings, then their points
  int32_t debris = 0;      // nodes of parts that flew off (+0x6d0)
  bool debris_in_world = false;  // each of them under the world node
  uint32_t detail = 0;     // +0x688  the level of detail's bits (mech_lod.cpp)
  uint32_t lod = 0;        // +0x690  0 the full model, 1 the low-detail one
  bool dead_listed = false;  // on the dead mechs' list (0x00605510)
};
bool launched_mech(LaunchedMech* out);  // false when the launch made none
bool describe_mech(const void* mech, LaunchedMech* out);  // the same of any mech (a Mech*)
void mech_kill(void* mech, void* killer = nullptr);  // for the tests: its centre torso destroyed (Destroy(0, 1), 0x00465f40), `killer` its last attacker
int32_t mission_salvage_items();
void mech_destroy_part(void* mech, int32_t id);  // for the tests: Mech::DestroyPart (0x0041cfa0)  // for the tests: the items in the mission's salvage store
int32_t canned_nodes_count();  // for the tests: the canned effects' node copies (the eject seats)

// player.cpp's AI mechs (mission-start.md section 4): 0x00414150 on a
// controller the program made (a MechControls), the definition's skin
// (0x00414390), the mech's definition (0x0041e9a0). Pointers as void*.
void* build_ai_mech(int32_t mech, void* controls, int32_t config, const char* name);
bool set_mech_skin(void* mech, void* skin);
const void* mech_definition(void* mech);
void mech_power(void* mech, int32_t down);       // 0x0041cc30 (Mech::power)
int32_t place_mech(void* mech, const float pos[3], uint32_t yaw);  // 0x004161c0 (Mech::place)
void move_mech(void* mech, const float pos[3], float yaw);         // vtable +0xdc, 0x00418f30 (Mech::set_place)
int32_t spawn_updates();                                           // the spawn's update steps run so far
const float* spawn_position();                                     // the placed mech's position before its last step
bool is_launched_mech(const void* mech);                           // the pilot's mech, the port's
void update_mech(void* mech, float dt);
void update_entities(float dt);                                   // 0x004a1140: every other entity's frame
void* node_owner(void* node);                                     // 0x0040e9d0 (app_weapons.cpp)
void* node_entity(void* node);                                    // 0x0040e920 (app_weapons.cpp)
bool entity_is_mech(const void* o);  // a program mech (zEntity's entry answering vt+0x50 with itself); others and freed ones not
void mech_entities_forget();         // the mechs' registry emptied with zEntity's list
// 0x004814e0 on the program's mechs: each one's projected shadow kept, drawn
// along the mission's sun (app_states' update_shadows is the handle version).
void update_mech_shadows();
void* mission_sun();                 // the mission's "sunlight" node ([0x006098d4] +4), or null
const void* mech_shadow(const void* mech);  // its caster (+0xcc, a zmod::ShadowCaster), for the tests
bool mech_aim_at(void* mech, const float at[3]);                   // 0x0041bc20 (Mech::aim_at), as the HUD's reticle would (0x0041cd40)                            // vtable +0x34, 0x00419fc0 (Mech::update)
void mech_set_hud_fall(void* mech, int32_t v);                     // 0x0041ca30
uint32_t mech_allies(const void* mech);          // AiTarget +0xc4
// The AiTarget masks player.cpp writes as words (+0xc0 team, +0xc4 allies,
// +0xc8 enemies); false for any other offset.
bool mech_set_mask(void* mech, uint32_t off, uint32_t v);
void* mech_controls_of(void* mech);              // +0x37c (a MechControls*)
float mech_game_time();                          // [0x0060991c]
bool mech_world_position(void* mech, float out[3]);  // 0x005a85e0; false (0, 0, 0) without a node
float mech_health(void* mech);                   // vt+0x98, 0x0041ba40
float mech_speed_at(void* mech, float share);    // vt+0x118, 0x0041d1c0
float mech_stopping_distance(void* mech, float speed);  // vt+0x120, 0x0041d210
void mech_forward(void* mech, float out[3]);     // +0x24, the flat forward
int32_t mech_jump_jets(void* mech);              // +0x3d8, the jump jets installed
bool mech_set_heat(void* mech, float heat);   // for the tests: +0x554
// For the tests: a hit of `damage` on the part with that id (0x00460970, no weapon).
bool mech_hit_part(void* mech, int32_t id, float damage);  // 6: the head
// For the tests: the throttle target set and held, as a throttle key held to v would.
bool mech_hold_throttle(void* mech, float v);
int32_t mech_controls_view(void* mech);
// For the tests: the world y of the mech's node, target node (+0x10) and
// centroid, and its height (0x005a8500 keeps the two markers' matrices).
bool mech_marker_heights(void* mech, float out[4]);
// For the tests: the textures on the mech's model (+0x694) and its low-detail
// copy (+0x698), by name (each textured polygon's), and its skin's (+0x3e8).
struct MechTextures {
  std::vector<std::string> names;  // the model's
  std::vector<std::string> lod;    // the low-detail copy's
  std::string skin;                // the mech's skin's name
  int loaded = 0;                  // polygons whose texture has an image
};
bool mech_textures(void* mech, MechTextures* out);
// For the tests: the mech's ceiling response (0x00423600) to `node` as the
// ceiling crossed; the head's and torso's structure after it (6 floats: head,
// torso sections 0..2, the hip's offset y, 1 when it had a box, the crouch depth).
bool mech_ceiling_hit(void* mech, void* node, float out[7]);  // for the tests: the controls' view state (+0x0c, 0x00426780)
float mech_throttle_target(void* mech);
// The cockpit frame's nodes (app_states' vehicle_node, cockpit_node, arm_node): the
// mech's node, its type's cockpit model (0x00430ad0) and an arm's (0 left, 1 right).
void* mech_node_of(void* mech);
void* mech_cockpit_node(void* mech);
void* mech_arm_node(void* mech, int side);  // for the tests: the controls' +0x44, 0 without
bool mech_powered_down(void* mech);              // vt+0xa0, 0x0041cc10
ai::AiTarget* mech_ai_target(void* mech);        // the Mech's AiTarget base
// 0x004b4ed0 on the mech's weapon manager (+0xd4): the ammo's value against
// its most, 1 before it has any; false without a manager.
bool mech_ammo_value(void* mech, float* out);
void mech_set_game_time(float t);
bool mech_get_mask(const void* mech, uint32_t off, uint32_t* v);
// The debriefing's lance (0x0047baf0's loop over zEntity's list): every
// mech, the mission's player's or not, its team, type and +0x3f4..+0x408.
class Store;
struct DebriefMech {
  void* mech = nullptr;
  bool player = false;
  uint32_t team = 0;
  int32_t type = -1;
  int32_t stats[6] = {};
};
std::vector<DebriefMech> mech_debrief_lance();
uint32_t mech_player_allies();
// 0x004aee70: an AI mech's name, its brain's vehicle record's pilot (AiParams
// +0xd4); none for the player's or a mech without a brain.
const char* mech_brain_name(void* mech);
// The mission's player mech (0x0060861c +0x14), for 0x0041df30's test.
void* mech_mission_player();
// A lance card's armor display's read (0x0047eba0, 0x0047ef10): the mech's
// type, its loadout's armor and its parts' armor left in the bars' order.
bool mech_lance_armor(void* mech, int32_t* type, int32_t armor[11], float left[11]);
// 0x0047ae10's won path on the ported mechs: the player's mech into `own`
// (the campaign's +0x64), its allies' into the mission's salvage store
// (mech_salvage.h).
void mech_lance_salvage(Store& own, bool team);  // the mission's player mech's +0xc4, 0 without one

// The AI mechs player.cpp built this mission (app_mission.cpp, our record of
// them for the log and the tests): each mech (a Mech*) and its controller (an
// AiControls*), in the order they were made.
struct AiMechMade {
  void* mech = nullptr;
  void* controls = nullptr;
  const char* name = nullptr;  // the aiv.zrd record's
};
const AiMechMade* mission_ai_mechs(int32_t* count);
// The mission's objectives as the program read them (mech_objective.h), and
// the hooks it gives them.
struct Objectives;
class ObjectiveHooks;
Objectives& mission_objectives();
void* mech_shade_of(void* mech);  // its sun shade (+0x68c, mech_shade.h), for the tests
ObjectiveHooks& mission_objective_hooks();

// The debug launch's slot (our addition, mission-load.md section 4): the
// mech by roster index, its first configuration, the definition's skin 0.
uint32_t debug_slot_config(int32_t mech, void* e, uint32_t (*handle_of)(void* e, const void* p));

}  // namespace mech3
