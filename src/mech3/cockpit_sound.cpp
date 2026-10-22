#include "mech3/cockpit_sound.h"

#include "gamez/zsound/zsnd.h"
#include "mech3/sound_extra.h"

// Original file: mech3\mech.cpp (0x004141a0 and 0x0041e770..0x0041e800 sit
// among the mech's functions).
namespace mech3 {

namespace {
CockpitSounds g_sounds;
CockpitSoundCalls g_calls;

// 0x004141a0's order (the strings 0x005e228c down to 0x005e20a8).
const char* const kNames[kCockpitSounds] = {
    "mech_missilelock", "damaged", "destroyed", "mech_chit_main", "mech_chit_head", "mech_chit_ltorso",
    "mech_chit_torso", "mech_chit_rtorso", "mech_chit_larm", "mech_chit_rarm", "mech_chit_lleg",
    "mech_chit_rleg", "mech_acquirelock", "mech_low_ammo", "mech_no_ammo", "ammo_explosion", "ammo_jettison",
    "mech_flush_coolant", "coolant_depleted", "ecm_engaged", "single_fire_engaged", "group_fire_engaged",
    "linked_fire_engaged", "mech_friendly_fire", "snd_gyro_overload", "snd_gyro_ok", "mech_heat_critical",
    "masc_engaged", "tc_online", "mech_traitor"};
}  // namespace

CockpitSounds& cockpit_sounds() { return g_sounds; }
CockpitSoundCalls& cockpit_sound_calls() { return g_calls; }

const char* cockpit_sound_name(int index) { return index >= 0 && index < kCockpitSounds ? kNames[index] : ""; }

// Mech3.exe 1.2 @ 0x004141a0. The voices are left as they are.
void cockpit_sounds_load() {
  for (int i = 0; i < kCockpitSounds; ++i) g_sounds.sound[i] = g_calls.find ? g_calls.find(kNames[i]) : zsnd::find(kNames[i]);
}

// Mech3.exe 1.2 @ 0x0041e770. Played where the listener is (no position), the
// voice kept for the stop.
void cockpit_sound_play(int index, float volume) {
  zsnd::Object* s = g_sounds.sound[index];
  g_sounds.voice[index] = g_calls.play ? g_calls.play(s, volume) : sound_play(s, nullptr, volume, nullptr, -1.0f);
}

// Mech3.exe 1.2 @ 0x0041e7a0. A slot with no sound does nothing.
void cockpit_sound_stop(int index, int fade) {
  if (!g_sounds.sound[index]) return;
  if (fade) {
    cockpit_sound_fade(index, 1, 0.0f);
    return;
  }
  if (g_calls.stop) g_calls.stop(g_sounds.voice[index]);
  else zsnd::stop(g_sounds.voice[index]);
  g_sounds.voice[index] = nullptr;
}

// Mech3.exe 1.2 @ 0x0041e7e0
void cockpit_sound_fade(int index, int stop, float level) {
  zsnd::VoiceBase* v = g_sounds.voice[index];
  g_sounds.voice[index] = g_calls.fade ? g_calls.fade(v, stop, level) : zsnd::fade(v, stop, level);
}

// Mech3.exe 1.2 @ 0x0041e800
zsnd::VoiceBase* cockpit_sound_voice(int index) { return g_sounds.voice[index]; }

}  // namespace mech3
