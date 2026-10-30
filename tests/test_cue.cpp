// The disc image's CD audio as music files (platform/cue_audio.h, ours).
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "platform/cue_audio.h"

namespace {
namespace fs = std::filesystem;

// `frames` raw CD frames, frame i's bytes all (i + seed) & 0xff.
void write_bin(const fs::path& p, int frames, int seed) {
  std::ofstream o(p, std::ios::binary);
  std::vector<char> f(2352);
  for (int i = 0; i < frames; ++i) {
    std::fill(f.begin(), f.end(), static_cast<char>((i + seed) & 0xff));
    o.write(f.data(), static_cast<std::streamsize>(f.size()));
  }
}

std::vector<unsigned char> read_all(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

// A Redump-style image: a data track, then two audio tracks in one bin (the
// first with a 2 s pregap, the second's INDEX 00 ending the first), and a
// third audio track in a bin of its own with a pregap.
TEST(cue_audio_writes_each_audio_track_from_index_01) {
  const fs::path d = fs::path(MW3_TEST_TMP) / "cue_audio";
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  write_bin(d / "data.bin", 300, 0);
  write_bin(d / "music.bin", 150 + 400 + 75 + 200, 7);  // pregap 150, track 2 400, gap 75, track 3 200
  write_bin(d / "t4.bin", 150 + 100, 3);
  {
    std::ofstream c(d / "disc.cue");
    c << "FILE \"data.bin\" BINARY\r\n  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n"
      << "FILE \"music.bin\" BINARY\r\n  TRACK 02 AUDIO\r\n    INDEX 00 00:00:00\r\n    INDEX 01 00:02:00\r\n"
      << "  TRACK 03 AUDIO\r\n    INDEX 00 00:07:25\r\n    INDEX 01 00:08:25\r\n"
      << "FILE \"t4.bin\" BINARY\r\n  TRACK 04 AUDIO\r\n    INDEX 00 00:00:00\r\n    INDEX 01 00:02:00\r\n";
  }
  std::vector<platform::CueTrack> tracks;
  std::string err;
  EXPECT(platform::read_cue((d / "disc.cue").string(), &tracks, &err));
  EXPECT(tracks.size() == 4 && !tracks[0].audio && tracks[1].audio && tracks[2].file == "music.bin");
  EXPECT(tracks[2].index0 == 550 && tracks[2].index1 == 625);
  std::vector<int32_t> done;
  const int32_t n = platform::write_cue_audio((d / "disc.cue").string(), (d / "music").string(),
                                              [&](int32_t t, uint64_t) { done.push_back(t); }, &err);
  std::printf("  cue: %d tracks written (%s)\n", n, err.c_str());
  EXPECT(n == 3 && done == std::vector<int32_t>({2, 3, 4}) && err.empty());
  EXPECT(!fs::exists(d / "music" / "track01.wav"));
  struct Want { const char* file; int frames, first; };
  for (const Want& w : {Want{"track02.wav", 400, (150 + 7) & 0xff}, Want{"track03.wav", 200, (625 + 7) & 0xff},
                        Want{"track04.wav", 100, (150 + 3) & 0xff}}) {
    const std::vector<unsigned char> b = read_all(d / "music" / w.file);
    const size_t data = static_cast<size_t>(w.frames) * 2352;
    EXPECT(b.size() == 44 + data);
    if (b.size() != 44 + data) continue;
    EXPECT(std::string(b.begin(), b.begin() + 4) == "RIFF" && std::string(b.begin() + 36, b.begin() + 40) == "data");
    const uint32_t rate = b[24] | b[25] << 8 | b[26] << 16 | static_cast<uint32_t>(b[27]) << 24;
    EXPECT(rate == 44100 && b[22] == 2 && b[34] == 16);
    EXPECT(b[44] == w.first && b[44 + data - 1] == static_cast<unsigned char>(w.first + w.frames - 1));
  }
  // A cue without tracks, and one missing its INDEX 01.
  { std::ofstream(d / "empty.cue") << "REM nothing\n"; }
  EXPECT(platform::write_cue_audio((d / "empty.cue").string(), (d / "m2").string(), nullptr, &err) == -1);
  { std::ofstream(d / "noindex.cue") << "FILE \"t4.bin\" BINARY\n  TRACK 01 AUDIO\n"; }
  EXPECT(platform::write_cue_audio((d / "noindex.cue").string(), (d / "m2").string(), nullptr, &err) == -1);
}
