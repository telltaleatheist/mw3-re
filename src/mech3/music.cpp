#include "mech3/music.h"

#include "gamez/zsound/zsnd_cd.h"

namespace mech3_music {

namespace {
bool g_enabled = true;  // 0x005e855c
int g_track = 0;        // 0x0060b104
int g_mode = 0;         // 0x005e8560
}  // namespace

bool enabled() { return g_enabled; }

// Mech3.exe 1.2 @ 0x004ac290. Remembers the request so turning music back on
// can restart it.
void play(int track, int mode) {
  g_mode = mode;
  g_track = track;
  if (g_enabled) {
    zsnd_cd::play_track(track, mode);
    return;
  }
  zsnd_cd::stop();
}

// Mech3.exe 1.2 @ 0x004ac230. The music slider: below 0x800 on both channels
// counts as off.
void set_volume(uint16_t left, uint16_t right) {
  const bool on = !(left < 0x800 && right < 0x800);
  const bool changed = g_enabled != on;
  g_enabled = on;
  if (changed) play(g_track, g_mode);
  zsnd_cd::set_volume(left, right);
}

}  // namespace mech3_music
