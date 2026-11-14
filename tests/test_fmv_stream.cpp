// A movie's frame from its streamed sound (gamez/zfmv/fmv_stream, docs/fmv.md
// section 5): the play cursor at exactly 0 after a wrap, as a sound device's
// 10 ms steps leave it when Video for Windows gives the sound an exact second
// a half (INTRO through the Indeo helper).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zfmv/fmv_script.h"
#include "gamez/zfmv/fmv_stream.h"
#include "gamez/ztwod/twod.h"
#include "gamez/zutil/zfix.h"

namespace {
using zfmv::Handle;
constexpr uint32_t kAuds = 0x73647561;

// INTRO as the helper reports it: 15 frames a second, 22050 Hz 16-bit
// stereo, 88200 bytes a half.
struct FakeHost : zfmv::Host {
  uint32_t cursor = 0;
  std::vector<uint8_t> scratch = std::vector<uint8_t>(1 << 20);
  void avi_file_init() override {}
  void avi_file_exit() override {}
  int32_t avi_open(Handle* stream, const char*, uint32_t type) override {
    *stream = type == kAuds ? 1 : 2;
    return 0;
  }
  int32_t avi_read_format(Handle stream, void* format, int32_t* size) override {
    uint8_t f[40] = {};
    const uint32_t audio[] = {0x00020001, 22050, 88200, 0x00100004};  // PCM, 2 channels; rate; bytes/s; align 4, 16 bits
    const uint32_t video[] = {40, 16, 16, 0x00180001};                 // 16x16, 24 bits
    std::memcpy(f, stream == 1 ? audio : video, 16);
    const int32_t n = stream == 1 ? 16 : 40;
    if (format) std::memcpy(format, f, static_cast<size_t>(n));
    *size = n;
    return 0;
  }
  int32_t avi_length(Handle stream) override { return stream == 1 ? 22050 * 200 : 3000; }
  int32_t avi_info(Handle stream, zfmv::AviStreamInfo* info) override {
    *info = {};
    if (stream == 1) info->scale = 4, info->rate = 88200, info->suggested_buffer = 88200, info->sample_size = 4;
    else info->scale = 1, info->rate = 15, info->length = 3000;
    return 0;
  }
  int32_t avi_read(Handle, int32_t, int32_t, void*, int32_t) override { return 0; }
  int32_t avi_find_sample(Handle, int32_t pos, uint32_t) override { return pos; }
  void avi_release(Handle) override {}
  Handle ic_locate(uint32_t, uint32_t, const void*, const void*, uint32_t) override { return 3; }
  int32_t ic_send(Handle, uint32_t, const void*, const void*) override { return 0; }
  int32_t ic_decompress(Handle, uint32_t, const void*, const void*, const void*, void*) override { return 0; }
  void ic_close(Handle) override {}
  uint32_t display_bits() override { return 16; }
  Handle sound_stream_create(const void*, const void*, uint32_t) override { return 4; }
  int32_t sound_stream_lock(Handle, uint32_t, uint32_t bytes, void** p1, uint32_t* n1, void** p2, uint32_t* n2) override {
    *p1 = scratch.data(), *n1 = bytes, *p2 = nullptr, *n2 = 0;
    return 1;
  }
  void sound_stream_unlock(Handle, void*, uint32_t, void*, uint32_t) override {}
  uint32_t sound_stream_position(Handle) override { return cursor; }
  void sound_stream_free(Handle) override {}
  Handle sound_play(Handle, float) override { return 5; }
};

// The frames the sound gives over 20 s of 10 ms device steps (220.5 samples,
// 882 bytes each; 200 steps a lap of the 176400-byte buffer, so every wrap
// lands on 0), and how far the frame ever went back.
int worst_drop(bool bugs) {
  const bool keep = zfix::all();
  zfix::set_all(bugs);
  FakeHost host;
  zfmv::set_host(&host);
  int drop = 0;
  {
    zfmv::Stream s("intro.avi", true);
    EXPECT(s.audio_ok && s.video_ok && s.half == 88200 && s.fps == 15);
    s.frame(0);  // the sound starts
    int last = 0;
    uint64_t played = 0;
    for (int step = 0; step < 2000; ++step) {
      played += 882;
      host.cursor = static_cast<uint32_t>(played % (2 * 88200));
      const int f = s.sound_frame();
      if (f < last && last - f > drop) drop = last - f;
      if (f > last) last = f;
    }
    EXPECT(last >= 15 * 19);  // it kept up with 20 s of sound
  }
  zfmv::set_host(nullptr);
  zfix::set_all(keep);
  return drop;
}
}  // namespace

TEST(fmv_sound_cursor_at_zero_after_a_wrap_keeps_the_frame) {
  const int fixed = worst_drop(true), original = worst_drop(false);
  std::printf("  the frame went back at most %d (the exe's reading: %d, a whole buffer)\n", fixed, original);
  EXPECT(fixed == 0);
  EXPECT(original >= 29);  // the exe's: back to the buffer's start, 2 s at 15 frames a second
}

// A movie in a dialog (ztwod::Movie2D, 0x005a7b50, docs/twod.md, Movies in a
// dialog): a file not found leaves no stream and draws nothing; a found one is
// opened without sound of its own, started at its place (its background rect
// the movie's size), keyed when asked.
TEST(dialog_movie_opens_found_files_only) {
  struct Host : FakeHost {
    int32_t file_stat(const char* path) override { return std::strstr(path, "none") ? -1 : 0; }
  } host;
  zfmv::set_host(&host);
  {
    ztwod::Movie2D missing;
    missing.open("video\\none.avi");
    EXPECT(missing.stream == nullptr && std::strcmp(missing.name, "video\\none.avi") == 0);
    missing.set_color_key(0x1234);
    EXPECT(missing.color_key == 0x1234);
    ztwod::Movie2D m;
    m.set_pos(10, -5);
    m.open("video\\loop.avi");
    EXPECT(m.stream && m.stream->video_ok && !m.stream->streamed && m.stream->image.width == 16);
    EXPECT(m.bg_rect.left == 10 && m.bg_rect.top == 0 && m.bg_rect.right == 26 && m.bg_rect.bottom == 16);
    m.set_color_key(0x07e0);
    EXPECT(m.stream && (m.stream->image.flags & 2) && m.color_key == 0x07e0);
    std::string longer(300, 'a');
    ztwod::Movie2D far;
    far.open(longer.c_str());  // cut and terminated (the exe's strncpy left it open)
    EXPECT(std::strlen(far.name) == sizeof far.name - 1);
  }
  zfmv::set_host(nullptr);
}
