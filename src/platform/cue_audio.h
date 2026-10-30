#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// A disc image's CD audio as the music files the program plays (ours, not
// original code; docs/fmv.md section 6, Music). The game asks MCI for CD
// track N; the audio layer plays music/trackNN.wav, so a cue sheet's AUDIO
// track N is written as trackNN.wav: 44100 Hz, 16-bit stereo, from its
// INDEX 01 (the pregap, INDEX 00 to 01, left out) to the next track's start
// in the same file, or the file's end.
namespace platform {

struct CueTrack {
  int32_t number = 0;
  bool audio = false;
  std::string mode;          // the TRACK line's type, upper case: AUDIO, MODE1/2352, MODE1/2048, MODE2/2352
  std::string file;          // as the cue names it, relative to the cue's folder
  int64_t index0 = -1;       // frames (1/75 s, 2352 bytes) into the file; -1 none
  int64_t index1 = -1;
};

// The cue's tracks in order; false (and why) when it cannot be read or has none.
bool read_cue(const std::string& cue_path, std::vector<CueTrack>* tracks, std::string* err);

// Each AUDIO track into `out_dir`/trackNN.wav; `done` hears each one (its
// number and the bytes of sound). The tracks written, -1 on a bad cue.
int32_t write_cue_audio(const std::string& cue_path, const std::string& out_dir,
                        const std::function<void(int32_t track, uint64_t bytes)>& done, std::string* err);

}  // namespace platform
