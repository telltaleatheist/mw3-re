#pragma once
#include <cstdint>

#include "gamez/zeffect/zeff_anim_init.h"

// zEffect\zeff_anim_run.c: running the mission animations (docs/anim.md,
// docs/anim-postload.md). A started def's own object3d (+0x68) carries its
// tick on an action list; the tick runs each sequence's events through
// anim_run_events, one handler per event type (zeff_anim_events.cpp).
// 0x00508310..0x005090e0 sit between this file and zeff_anim_init.c and are
// kept here.
namespace zeff {

// A sequence (the reset one at def +0xc8, the others at +0xc4): +0x20 state
// (0 next event, 1 inside one, 2 stopped, 3 waiting to be called), +0x21
// the state a reset puts back, +0x24 time since it started, +0x28 time in
// the current event, +0x2c loop time, +0x30 loop count (u16), +0x34 the
// current event, +0x38 / +0x3c the events.
inline SeqDef* reset_sequence(AnimDef& def) { return &def.reset_seq; }

// The world's ground under a point (0x004edab0, cls_di.c, not ported): up to
// 36 hits, each a height and the surface it is on.
struct GroundHit {
  float y = 0;                     // +0x10 of the exe's 0x28-byte entry
  int32_t surface = 0;             // +0x20 -> +0x20: 1 and 4 have their own sequences/sounds
  bool has_surface = false;        // the +0x20 pointer is set
};
struct GroundHits {
  int32_t count = 0;               // the byte at +0
  GroundHit hit[36];
};

// What the animations call that is not ported yet, or that a test records.
// Null hooks are skipped (the handler acts as if nothing happened).
struct AnimRunHooks {
  void* ctx = nullptr;
  void (*play_sound)(void* ctx, void* sound, float volume) = nullptr;  // 0x0056e3d0
  void (*play_sound_at)(void* ctx, void* sound, const float pos[3], float volume) = nullptr;  // 0x0056e7e0
  void (*ground)(void* ctx, zcls::Node* world, float x, float y, float z, GroundHits* out) = nullptr;  // 0x004edab0
  // 0x00507220 (ECX node or null, EDX position or null, stack &height,
  // &hit): the "if" event's line-of-sight test down from a point (0x004eed90).
  int32_t (*ray_down)(void* ctx, zcls::Node* node, const float* pos, float height, int32_t* hit) = nullptr;
  void (*effect)(void* ctx, AnimDef* def, uint8_t* ev) = nullptr;          // event 3, 0x005031b0
  void (*camera_state)(void* ctx, AnimDef* def, uint8_t* ev) = nullptr;    // event 20, 0x00503c10
  int32_t (*camera_from_to)(void* ctx, AnimDef* def, SeqDef* seq, uint8_t* ev) = nullptr;  // event 21, 0x00503d50
  void (*remove_child)(void* ctx, AnimDef* def, uint8_t* ev) = nullptr;    // event 16, 0x004ebb40
  void (*detonate)(void* ctx, uint8_t* ev, const float pos[3]) = nullptr;  // event 41, 0x00581ed0 / 0x005822c0
  int32_t (*fbfx_color)(void* ctx, AnimDef* def, SeqDef* seq, uint8_t* ev) = nullptr;  // event 36, 0x005074b0
  int32_t (*fbfx_wave)(void* ctx, AnimDef* def, SeqDef* seq, uint8_t* ev) = nullptr;   // event 37, 0x005076c0
  void (*message)(void* ctx, uint8_t* ev) = nullptr;                       // event 38, 0x00507970
};
AnimRunHooks& anim_run_hooks();

// The runtime.
int32_t anim_run_events(AnimDef* def, SeqDef* seq);   // 0x005079b0, 0 or -1
void anim_tick(zcls::Node* object);                    // 0x00507de0, the running def's action
void anim_reset_tick(zcls::Node* object);              // 0x00508260, a reset sequence still running
void anim_wait_tick(zcls::Node* object);               // 0x005085b0, waiting out the reset time
int32_t anim_save_states(AnimDef* def);                // 0x00507fc0
int32_t anim_restore_states(AnimDef* def);             // 0x00508090, AUTO_RESET defs only
int32_t anim_reset_finish(AnimDef* def);               // 0x00508170
int32_t anim_reset(AnimDef* def, zcls::Node* node, int32_t keep);  // 0x00508310
int32_t anim_stop(AnimDef* def);                       // 0x00506ca0, the end of a run
int32_t anim_lights_off(AnimDef* def);                 // 0x005069b0
int32_t anim_puffers_off(AnimDef* def);                // 0x00506a20
int32_t anim_sounds_off(AnimDef* def);                 // 0x00506a70
int32_t anim_seq_restart(SeqDef* seq);                 // 0x00506f40
int32_t anim_restart(AnimDef* def);                    // 0x00508460, from status 6/4
int32_t anim_reset_logged(AnimDef* def);               // 0x00508490
void anim_invalidate(AnimDef* def);                    // 0x005084c0
int32_t anim_prepare(AnimDef* def);                    // 0x005084f0
int32_t anim_prerequisites(AnimDef* def);              // 0x00508950, 1 when met
AnimDef* anim_call(AnimDef* def, zcls::Node* node);    // 0x00508760, the def (or instance) started
AnimDef* anim_start_at(AnimDef* def, zcls::Node* node, const float pos[3], const float rot[3],
                       const float vec[3]);            // 0x005085e0 (0x00508aa0 calls it)
AnimDef* anim_start(AnimDef* def, zcls::Node* node, float x, float y, float z);  // 0x00508ae0 (0x00508c10)
AnimDef* anim_start_connector(AnimDef* def, zcls::Node* node, zcls::Node* from, const float* from_pos,
                              const float* vec);       // 0x00508c30 (0x00508da0)
AnimDef* anim_start_connector_at(AnimDef* def, zcls::Node* node, zcls::Node* from, const float* from_pos,
                                 zcls::Node* to, const float* to_pos);  // 0x00508dc0 (0x00508ee0)
void anim_set_callback(AnimDef* def, AnimDef::Callback cb, void* arg);  // 0x00508f00
void anim_set_level(int32_t level);                    // 0x00508fb0
void anim_set_player(const float pos[3]);              // 0x00502d40
void anim_set_zone(const uint8_t zone[4]);             // 0x00502d70, ignored if an id is 0xff
void anim_keep_stopped_elapsed(int32_t on);            // 0x00502d20

// The damage callbacks registered at load (zwep_damage). ECX def.
float anim_on_collide(AnimDef* def, float damage);                       // 0x00502df0
int32_t anim_on_weapon_hit(AnimDef* def, const void* hit, int32_t unknown, float damage);  // 0x00502e60

// Per-def lookups that add a ref when the name is missing (zeff_anim_init.c).
int32_t anim_light_ref(AnimDef& def, const char* name);  // 0x00509400
int32_t anim_sound_ref(AnimDef& def, const char* name);  // 0x005092e0

// Instances (zeff_anim_init.c): a copy of a def to run at another node.
AnimDef* anim_instance(AnimDef* def, zcls::Node* node);  // 0x005097c0
AnimDef* anim_bind(AnimDef* def, zcls::Node* node);      // 0x00509eb0

// The post-load pass (zeff_anim_init.c).
int32_t anim_load();                                     // 0x0050ae60

// Helpers the handlers share.
zcls::Node* event_node(AnimDef& def, int32_t index);     // refs 1.., -200 the def's at_node
float fast_sqrt(float v);                                // (bits >> 1) + 0x1fc00000

}  // namespace zeff
