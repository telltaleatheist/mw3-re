#pragma once
#include <cstdint>

// What zSound (docs/sound.md) needs from the machine: stands in for DirectSound
// (DSOUND.dll's DirectSoundCreate, IDirectSound and IDirectSoundBuffer as the
// game uses them). FIX (A): DirectSound does not exist on macOS and is legacy on
// Windows; the SDL3 front end implements this (src/frontend/sdl/sdl_sound),
// a recorder does for tests.
//
// One method per COM call the game makes, in DirectSound's own units, and each
// returns the HRESULT (0 = DS_OK) so the game's error paths stay: volumes in
// hundredths of a decibel (0 full, -10000 silent), pan -10000 (left) to 10000
// (right), frequencies in Hz, positions in bytes. Buffers are handles, 0 none.
// An implementation behaves as DirectSound does where the game can tell: a
// control the buffer was not created with fails with kControlUnavailable (the
// game's doppler depends on this), a buffer that is not looping stops at its
// end with its position back at 0, Lock hands out the buffer's own memory.
namespace platform {

using SoundBuffer = int32_t;

struct WaveFormat {               // WAVEFORMATEX without cbSize, as the game fills it
  uint16_t format_tag = 1;        // 1 PCM
  uint16_t channels = 1;
  uint32_t samples_per_sec = 22050;
  uint32_t avg_bytes_per_sec = 22050;
  uint16_t block_align = 1;
  uint16_t bits_per_sample = 8;
};

struct BufferDesc {               // DSBUFFERDESC
  uint32_t flags = 0;             // kPrimary .. kGetPosition2
  uint32_t bytes = 0;             // 0 for the primary buffer
  const WaveFormat* format = nullptr;  // null for the primary buffer
};

class SoundOutput {
 public:
  // DSBCAPS_* the game asks for.
  static constexpr uint32_t kPrimary = 0x1, kStatic = 0x2, kLocSoftware = 0x8, kCtrlFrequency = 0x20,
                            kCtrlPan = 0x40, kCtrlVolume = 0x80, kGetPosition2 = 0x10000;
  static constexpr uint32_t kStatusPlaying = 0x1, kStatusLost = 0x2, kStatusLooping = 0x4;  // DSBSTATUS_*
  static constexpr uint32_t kPlayLooping = 0x1;                                            // DSBPLAY_LOOPING
  static constexpr uint32_t kCapsPrimary16Bit = 0x8;                                        // DSCAPS_*
  static constexpr uint32_t kNormal = 1, kExclusive = 3;                                    // DSSCL_*
  static constexpr int32_t kControlUnavailable = static_cast<int32_t>(0x8878001e);          // DSERR_CONTROLUNAVAIL
  static constexpr int32_t kInvalidParam = static_cast<int32_t>(0x80070057);                // DSERR_INVALIDPARAM

  virtual ~SoundOutput() = default;

  // DirectSoundCreate(NULL, &ds, NULL), IDirectSound::Release.
  virtual int32_t create() = 0;
  virtual void release_device() = 0;
  // IDirectSound
  virtual int32_t set_cooperative_level(uint32_t window, uint32_t level) = 0;
  virtual int32_t get_caps(uint32_t* flags) = 0;  // DSCAPS.dwFlags, the field the game reads
  virtual int32_t create_buffer(const BufferDesc& desc, SoundBuffer* out) = 0;
  virtual int32_t duplicate_buffer(SoundBuffer original, SoundBuffer* out) = 0;
  // IDirectSoundBuffer
  virtual void release(SoundBuffer b) = 0;
  virtual int32_t get_format(SoundBuffer b, WaveFormat* out) = 0;
  virtual int32_t set_format(SoundBuffer b, const WaveFormat& format) = 0;
  virtual int32_t get_status(SoundBuffer b, uint32_t* status) = 0;
  virtual int32_t restore(SoundBuffer b) = 0;
  virtual int32_t lock(SoundBuffer b, uint32_t offset, uint32_t bytes, void** p1, uint32_t* n1, void** p2,
                       uint32_t* n2) = 0;
  virtual int32_t unlock(SoundBuffer b, void* p1, uint32_t n1, void* p2, uint32_t n2) = 0;
  virtual int32_t play(SoundBuffer b, uint32_t flags) = 0;
  virtual int32_t stop(SoundBuffer b) = 0;
  virtual int32_t set_current_position(SoundBuffer b, uint32_t position) = 0;
  virtual int32_t get_current_position(SoundBuffer b, uint32_t* play, uint32_t* write) = 0;
  virtual int32_t get_volume(SoundBuffer b, int32_t* volume) = 0;
  virtual int32_t set_volume(SoundBuffer b, int32_t volume) = 0;
  virtual int32_t set_pan(SoundBuffer b, int32_t pan) = 0;
  virtual int32_t get_frequency(SoundBuffer b, uint32_t* hz) = 0;
  virtual int32_t set_frequency(SoundBuffer b, uint32_t hz) = 0;
};

}  // namespace platform
