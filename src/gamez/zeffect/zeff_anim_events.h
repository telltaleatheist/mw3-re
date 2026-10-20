#pragma once
#include <cstdint>
#include <cstring>

#include "gamez/zeffect/zeff_anim_run.h"

// The animation events' handlers, one per type (0x00503100..0x00507970).
// They sit between zdec_qsand.cpp and zeff_anim_run.c's first named function,
// and only the runner (0x005079b0) calls them: most likely zeff_anim_run.c.
// Each takes the def (ECX), the event or the sequence (EDX) and the event
// (stack) as the runner passes them, and returns the sequence's new state:
// 2 done (next event), 1 still running, 0 for the loop event's restart.
// Offsets are from the event's start (the 12-byte header first; mech3ax's
// layouts start after it).
namespace zeff {

namespace ev {
template <typename T>
inline T get(const uint8_t* p, int off) {
  T v;
  std::memcpy(&v, p + off, sizeof v);
  return v;
}
template <typename T>
inline void put(uint8_t* p, int off, T v) {
  std::memcpy(p + off, &v, sizeof v);
}
inline float* fp(uint8_t* p, int off) { return reinterpret_cast<float*>(p + off); }
}  // namespace ev

int32_t ev_sound(AnimDef& def, uint8_t* e);                      // 1   0x00503100
int32_t ev_sound_node(AnimDef& def, uint8_t* e);                 // 2   0x00503270
int32_t ev_light_state(AnimDef& def, uint8_t* e);                // 4   0x00503390
int32_t ev_light_animation(AnimDef& def, SeqDef& s, uint8_t* e); // 5   0x00503910
int32_t ev_object_active(AnimDef& def, uint8_t* e);              // 6   0x005044d0
int32_t ev_object_translate(AnimDef& def, uint8_t* e);           // 7   0x00504380
int32_t ev_object_scale(AnimDef& def, uint8_t* e);               // 8   0x00504350
int32_t ev_object_rotate(AnimDef& def, uint8_t* e);              // 9   0x00504180
int32_t ev_object_motion(AnimDef& def, SeqDef& s, uint8_t* e);   // 10  0x00504520
int32_t ev_object_motion_from_to(AnimDef& def, SeqDef& s, uint8_t* e);  // 11  0x00505200
int32_t ev_object_motion_script(AnimDef& def, SeqDef& s, uint8_t* e);   // 12  0x00505b00
int32_t ev_object_opacity(AnimDef& def, uint8_t* e);             // 13  0x00505bf0
int32_t ev_object_opacity_from_to(AnimDef& def, SeqDef& s, uint8_t* e);  // 14  0x00505c60
int32_t ev_object_add_child(AnimDef& def, uint8_t* e);           // 15  0x00505d90
int32_t ev_object_cycle_texture(AnimDef& def, SeqDef& s, uint8_t* e);    // 17  0x00505e20
int32_t ev_object_connector(AnimDef& def, SeqDef& s, uint8_t* e);       // 18  0x00505e80
int32_t ev_call_connector(AnimDef& def, uint8_t* e);             // 19  0x00506290
int32_t ev_call_sequence(AnimDef& def, uint8_t* e);              // 22  0x005064e0
int32_t ev_stop_sequence(AnimDef& def, uint8_t* e);              // 23  0x00506590
int32_t ev_call_animation(AnimDef& def, SeqDef& s, uint8_t* e);  // 24  0x00506640
int32_t ev_stop_animation(uint8_t* e);                           // 25  0x00506d60
int32_t ev_reset_animation(uint8_t* e);                          // 26  0x00506e00
int32_t ev_invalidate_animation(uint8_t* e);                     // 27  0x00506ea0
int32_t ev_fog_state(uint8_t* e);                                // 28  0x00503ba0
int32_t ev_loop(AnimDef& def, SeqDef& s, uint8_t* e);            // 30  0x00506f60, the new state
int32_t ev_if(AnimDef& def, SeqDef& s, uint8_t* e);              // 31  0x00507010
int32_t ev_else(SeqDef& s);                                      // 32  0x005073b0 (33 0x005073e0)
int32_t ev_callback(AnimDef& def, uint8_t* e);                   // 35  0x00507410
int32_t ev_detonate_weapon(AnimDef& def, uint8_t* e);            // 41  0x00507430

// Pieces the handlers share (0x00502f00..0x00505710).
float connector_set(zcls::Node* node, const float from[3], const float to[3]);  // 0x00502f00
float connector_part(zcls::Node* node, const float from[3], float t0, const float to[3], float t1);  // 0x00502fd0
// ECX position, EDX the event's ground table (count byte, then 0x28 entries),
// stack how far up to look, &hit: the ground nearest the point.
int32_t ground_below(const float pos[3], float reach, GroundHits& hits, int32_t* best);  // 0x00505160
float distance2_to_player(zcls::Node* node);                       // 0x00507340

}  // namespace zeff
