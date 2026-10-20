#pragma once
#include <map>
#include <vector>

#include "platform/sound_output.h"

// A platform::SoundOutput with no device: buffers keep their bytes and
// settings, Play marks them playing until Stop (nothing ever reaches its end).
// For tests and for running without audio (mw3 --nosound).
namespace platform {

class SilentSoundOutput : public SoundOutput {
 public:
  int32_t create() override { return 0; }
  void release_device() override { buffers_.clear(); }
  int32_t set_cooperative_level(uint32_t, uint32_t) override { return 0; }
  int32_t get_caps(uint32_t* flags) override { return *flags = kCapsPrimary16Bit, 0; }
  int32_t create_buffer(const BufferDesc& desc, SoundBuffer* out) override {
    Buffer& b = buffers_[*out = next_++];
    b.flags = desc.flags;
    b.data.resize(desc.bytes);
    if (desc.format) b.format = *desc.format;
    return 0;
  }
  int32_t duplicate_buffer(SoundBuffer original, SoundBuffer* out) override {
    Buffer* b = get(original);
    if (!b) return kInvalidParam;
    Buffer copy = *b;
    copy.playing = false;
    buffers_[*out = next_++] = copy;
    return 0;
  }
  void release(SoundBuffer b) override { buffers_.erase(b); }
  int32_t get_format(SoundBuffer b, WaveFormat* out) override {
    Buffer* x = get(b);
    return x ? (*out = x->format, 0) : kInvalidParam;
  }
  int32_t set_format(SoundBuffer b, const WaveFormat& f) override {
    Buffer* x = get(b);
    return x ? (x->format = f, 0) : kInvalidParam;
  }
  int32_t get_status(SoundBuffer b, uint32_t* status) override {
    Buffer* x = get(b);
    *status = x && x->playing ? kStatusPlaying | (x->looping ? kStatusLooping : 0) : 0;
    return x ? 0 : kInvalidParam;
  }
  int32_t restore(SoundBuffer) override { return 0; }
  int32_t lock(SoundBuffer b, uint32_t offset, uint32_t bytes, void** p1, uint32_t* n1, void** p2,
               uint32_t* n2) override {
    Buffer* x = get(b);
    if (!x || offset > x->data.size()) return kInvalidParam;
    if (bytes > x->data.size() - offset) bytes = static_cast<uint32_t>(x->data.size() - offset);
    *p1 = x->data.data() + offset, *n1 = bytes;
    if (p2) *p2 = nullptr;
    if (n2) *n2 = 0;
    return 0;
  }
  int32_t unlock(SoundBuffer, void*, uint32_t, void*, uint32_t) override { return 0; }
  int32_t play(SoundBuffer b, uint32_t flags) override {
    Buffer* x = get(b);
    if (!x) return kInvalidParam;
    x->playing = true, x->looping = (flags & kPlayLooping) != 0;
    ++plays;
    return 0;
  }
  int32_t stop(SoundBuffer b) override {
    Buffer* x = get(b);
    return x ? (x->playing = false, 0) : kInvalidParam;
  }
  int32_t set_current_position(SoundBuffer, uint32_t) override { return 0; }
  int32_t get_current_position(SoundBuffer, uint32_t* play, uint32_t* write) override {
    *play = 0, *write = 0;
    return 0;
  }
  int32_t get_volume(SoundBuffer b, int32_t* v) override {
    Buffer* x = get(b);
    return x ? (*v = x->volume, 0) : kInvalidParam;
  }
  int32_t set_volume(SoundBuffer b, int32_t v) override {
    Buffer* x = get(b);
    if (!x) return kInvalidParam;
    if (!(x->flags & kCtrlVolume) && !(x->flags & kPrimary)) return kControlUnavailable;
    return x->volume = v, 0;
  }
  int32_t set_pan(SoundBuffer b, int32_t) override {
    Buffer* x = get(b);
    if (!x) return kInvalidParam;
    return (x->flags & kCtrlPan) ? 0 : kControlUnavailable;
  }
  int32_t get_frequency(SoundBuffer b, uint32_t* hz) override {
    Buffer* x = get(b);
    if (!x) return kInvalidParam;
    if (!(x->flags & kCtrlFrequency)) return kControlUnavailable;
    return *hz = x->freq ? x->freq : x->format.samples_per_sec, 0;
  }
  int32_t set_frequency(SoundBuffer b, uint32_t hz) override {
    Buffer* x = get(b);
    if (!x) return kInvalidParam;
    if (!(x->flags & kCtrlFrequency)) return kControlUnavailable;
    return x->freq = hz, 0;
  }

  int plays = 0;  // Play calls, for tests

 private:
  struct Buffer {
    uint32_t flags = 0;
    std::vector<uint8_t> data;
    WaveFormat format;
    bool playing = false, looping = false;
    int32_t volume = 0;
    uint32_t freq = 0;
  };
  Buffer* get(SoundBuffer b) {
    auto it = buffers_.find(b);
    return it == buffers_.end() ? nullptr : &it->second;
  }
  std::map<SoundBuffer, Buffer> buffers_;
  SoundBuffer next_ = 1;
};

}  // namespace platform
