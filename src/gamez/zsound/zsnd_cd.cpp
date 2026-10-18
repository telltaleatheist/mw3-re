#include "gamez/zsound/zsnd_cd.h"

#include <cstdio>

#include "crt/msvc_rand.h"
#include "gamez/zerror/zerr.h"
#include "platform/platform.h"

namespace zsnd_cd {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zSound\\zsnd_cd.cpp";
constexpr uint32_t kOpen = 2;
constexpr int kNotifySuccessful = 1;  // MCI_NOTIFY_SUCCESSFUL

State g_state;
platform::Files* g_files = nullptr;
platform::MusicOutput* g_output = nullptr;
std::string g_dir = "music";

bool is_open() { return (g_state.flags & kOpen) != 0; }

// 0x005728b0: forget the position. (The original also clears the paused
// minute/second, which the rebuild leaves to MusicOutput.)
void clear_position() {
  g_state.last_track = 0;
  g_state.track = 0;
  g_state.end_track = 0;
}

// Every MCI failure in the original goes through 0x00574660 with its line number.
bool fail(int line, const char* what) {
  zerr::report(0x100, kFile, line, what);
  return false;
}

bool start(int track) {
  const std::string path = track_path(track);
  if (path.empty() || !g_output || !g_output->play(path)) return fail(0x117, "cannot play track");
  return true;
}
}  // namespace

State& state() { return g_state; }

void set_backend(platform::Files* files, platform::MusicOutput* output, std::string music_dir) {
  g_files = files;
  g_output = output;
  g_dir = std::move(music_dir);
}

std::string track_path(int track) {
  if (!g_files || track < 1 || track > 99) return "";
  static const char* const kExts[] = {"ogg", "flac", "wav", "mp3"};
  char name[32];
  for (const char* ext : kExts) {
    std::snprintf(name, sizeof name, "/track%02d.%s", track, ext);
    std::string path = g_dir + name;
    if (g_files->exists(path)) return path;
  }
  return "";
}

// Mech3.exe 1.2 @ 0x00572450. MCI open of "cdaudio", a media-present check, the
// track count and the CD_TRACKS cues. FIX (A): there is no drive; the "disc" is
// the music folder, present when it holds any track, and its track count is the
// highest track number found. The aux-device volume probe goes to MusicOutput.
bool open(std::vector<Cue> cues) {
  if (is_open()) return true;
  g_state.flags |= kOpen;
  g_state.enabled = true;
  int highest = 0;
  for (int t = 1; t <= 99; ++t)
    if (!track_path(t).empty()) highest = t;
  if (highest == 0) {  // media not present: the original closes and returns 0
    close();
    return false;
  }
  g_state.num_tracks = highest;
  clear_position();
  g_state.cues = std::move(cues);
  return true;
}

// Mech3.exe 1.2 @ 0x005728f0
void close() {
  stop();
  g_state.flags &= ~kOpen;
  g_state.cues.clear();
}

// Mech3.exe 1.2 @ 0x00572af0. Mode 6 restarts even when the track is already the
// one asked for; every other mode leaves a playing request alone.
bool play_track(int track, int mode) {
  if (!is_open()) return false;
  if (!g_state.enabled) return false;
  if (mode != Restart && track == g_state.last_track) return true;
  if (!seek(track)) return false;
  return play(mode);
}

// Mech3.exe 1.2 @ 0x00572e40
bool seek(int track) {
  if (!is_open()) return false;
  if (g_output) g_output->stop();
  clear_position();
  g_state.track = track;
  g_state.end_track = track;
  g_state.last_track = track;
  return true;
}

// Mech3.exe 1.2 @ 0x00572b40. Plays from the current track to the end of it
// (modes 2, 5, 6) or to the end of the disc.
bool play(int mode) {
  if (!is_open()) return false;
  const bool one_track = mode == Once || mode == Loop || mode == Restart;
  g_state.end_track = (one_track ? g_state.track : g_state.num_tracks) + 1;
  if (!start(g_state.track)) return false;
  g_state.mode = mode;
  return true;
}

// Mech3.exe 1.2 @ 0x00572c30
bool stop() {
  if (!is_open()) return false;
  if (g_output) g_output->stop();
  g_state.mode = Stopped;
  clear_position();
  return true;
}

// Mech3.exe 1.2 @ 0x00572c90. Only a playing CD (mode 2 and up) pauses.
bool pause() {
  if (!is_open()) return false;
  if (g_state.mode < Once) return false;
  if (g_output) g_output->pause();
  g_state.mode = Paused;
  return true;
}

// Mech3.exe 1.2 @ 0x00572d70. The caller passes back the mode it paused from.
bool resume(int mode) {
  if (!is_open()) return false;
  if (g_state.mode != Paused) return false;
  if (g_output) g_output->resume();
  g_state.mode = mode;
  return true;
}

// Mech3.exe 1.2 @ 0x00572bf0. When a looping track's range finishes, play it again.
// (The original also checks the notifying device is the CD.)
void notify(int status) {
  if (!is_open()) return;
  if (g_state.mode != Loop && g_state.mode != Restart) return;
  if (status != kNotifySuccessful) return;
  play_track(g_state.track, Restart);
}

void on_output_finished() {
  if (!is_open() || g_state.mode < Once) return;
  // A CD plays straight on into the next track; files need starting one by one.
  if (g_state.track + 1 < g_state.end_track && !track_path(g_state.track + 1).empty()) {
    g_state.track += 1;
    start(g_state.track);
    return;
  }
  notify(kNotifySuccessful);
}

// Mech3.exe 1.2 @ 0x00572a30. Several tracks: pick one with MSVCRT rand().
int cue_track(const std::string& name) {
  for (const Cue& cue : g_state.cues) {
    if (cue.name != name) continue;
    const int count = static_cast<int>(cue.tracks.size());
    if (count == 1) return cue.tracks[0];
    if (count == 0) return 0;
    return cue.tracks[(crt::msvc_rand() * count) / 32768];
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00572f80 (auxSetVolume on the CD's aux device).
void set_volume(uint16_t left, uint16_t right) {
  if (!is_open()) return;
  if (g_output) g_output->set_volume(left, right);
}

}  // namespace zsnd_cd
