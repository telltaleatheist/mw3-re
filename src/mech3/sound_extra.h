#pragma once
#include <cstdint>

namespace zrdr { struct Node; }
namespace zsnd { struct Object; struct VoiceBase; }

// What a sound's sounds.zrd EXTRA list gives it (the source file is
// unknown; named for the list): a repeat delay and a subtitle, kept as the
// zSound object's user data. The pilots' cockpit sounds and the radio play
// through it.
namespace mech3 {

struct SoundExtra {                // 0x0c bytes (0x004e6a60)
  char* subtitle = nullptr;        // +0x00  the message's text (0x0049d1c0 strdup)
  float min_delay = 0;             // +0x04  MINDELAY, seconds
  float next = 0;                  // +0x08  the game time it may play again
};

// What the records call outside; unset: zSound's 0x00571cc0 and the
// message panel's 0x0040fe50 (the tests stand in for them).
struct SoundExtraHooks {
  zsnd::VoiceBase* (*play_fading)(zsnd::Object* obj, const float* pos, float volume, const float* vel,
                                  float fade_to, int32_t stop) = nullptr;
  zsnd::VoiceBase* (*play)(zsnd::Object* obj, float volume) = nullptr;  // 0x0056e3d0
  void (*subtitle)(const char* text) = nullptr;
};
SoundExtraHooks& sound_extra_hooks();

// The record for sounds without one (0x0060b800, made from the zero
// template 0x0060b868 at start-up).
SoundExtra& default_extra();
// [0x0060b878]: the subtitles' switch. Nothing in the exe sets it, so they
// never show (docs/sound.md).
bool& subtitles_on();

// zSound's load callback (0x004e6b00, ECX the object, EDX its EXTRA list):
// a record when MINDELAY or SUBTITLE is there, else none.
void* sound_extra_load(zsnd::Object* obj, zrdr::Node* extra);
void sound_extra_free(zsnd::Object* obj, void* user);   // 0x004e6be0
void sound_extras_clear();                              // 0x004e69a0: every record freed

// The sound, unless it played less than its delay ago; its subtitle to the
// message panel when subtitles are on.
zsnd::VoiceBase* sound_extra_play(SoundExtra& r, zsnd::Object* obj, float volume, const float* pos,
                                  const float* vel, float fade_to);  // 0x004e6cd0 ECX record
zsnd::VoiceBase* sound_play(zsnd::Object* obj, const float* pos, float volume, const float* vel,
                            float fade_to);                           // 0x004e6d80 ECX object, EDX pos
// The same with zSound's plain play (0x0056e3d0, no place): the radio's
// lines (docs/ai.md 17.6).
zsnd::VoiceBase* sound_extra_say(SoundExtra& r, zsnd::Object* obj, float volume);  // 0x004e6c60 ECX record
zsnd::VoiceBase* sound_say(zsnd::Object* obj, float volume);                       // 0x004e6d50 ECX object
// A mech type's sound by slot (`sounds` is mech.h's MechData::sounds,
// mech_data.cpp; pilot.h's career MechData shares the name, so it is not
// named here): no fade (0x00432990 on +0x684: the fifth argument is ignored).
zsnd::VoiceBase* pilot_sound(void* const* sounds, int32_t id, float volume, const float* pos, const float* vel);

}  // namespace mech3
