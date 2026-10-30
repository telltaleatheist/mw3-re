#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Video for Windows as the movie player calls it (AVIFIL32, MSVFW32), for a
// platform that has it: on Windows, the 32-bit helper mw3indeo with the
// install's Indeo 5 codec (docs/fmv.md, The Indeo helper). FIX (A): the
// codec is a 32-bit driver; without one the movies come from the importer's
// files (mech3/app_movie.h). Results are the calls' own (HRESULTs, ICERR_*).
namespace platform {

class VideoForWindows {
 public:
  virtual ~VideoForWindows() = default;
  virtual bool ready() = 0;  // started and the codec installed (started on the first ask)
  // AVIStreamOpenFromFileA(host path, 'vids' or 'auds'): *stream an id, or 0.
  virtual int32_t open(const std::string& path, uint32_t type, uint32_t* stream) = 0;
  virtual int32_t format(uint32_t stream, std::vector<uint8_t>* out) = 0;   // AVIStreamReadFormat
  virtual int32_t info(uint32_t stream, void* info_0x8c) = 0;              // AVIStreamInfoA
  virtual int32_t length(uint32_t stream) = 0;                              // AVIStreamLength
  // AVIStreamRead: a sound's bytes into `buffer`; a picture's chunk stays with the codec.
  virtual int32_t read(uint32_t stream, int32_t start, int32_t samples, void* buffer, int32_t bytes) = 0;
  virtual int32_t find_sample(uint32_t stream, int32_t pos, uint32_t flags) = 0;  // AVIStreamFindSample
  // ICDecompress of the picture read last into `image` (`out_format` the BITMAPINFOHEADER with its masks).
  virtual int32_t decompress(uint32_t stream, uint32_t flags, const void* out_format, size_t out_bytes, void* image,
                             size_t image_bytes) = 0;
  virtual void release(uint32_t stream) = 0;  // AVIStreamRelease
};

}  // namespace platform
