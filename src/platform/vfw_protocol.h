#pragma once
// The pipe between mw3 and mw3indeo (ours, not original code; docs/fmv.md,
// The Indeo helper): the movie player's Video for Windows calls, one message
// each, answered by a 32-bit process that has Windows' own AVIFile and the
// install's Indeo 5 codec (Ir50_32.dll, a 32-bit driver a 64-bit mw3 cannot
// load). Plain C++ with no Windows types, shared by both ends and the test
// helper (src/platform, as tools/indeo/mw3indeo.cpp includes it).
//
// A request is seven little-endian words, then `size` bytes:
//   op, stream, a, b, c, d, size
// An answer is two words, then `size` bytes:
//   result (an HRESULT or the call's own value), size
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vfwpipe {

constexpr uint32_t kVersion = 1;

enum Op : uint32_t {
  kHello = 1,       // result: kVersion when the codec is installed, else negative; payload: a line of text
  kOpen = 2,        // a: the stream type ('vids', 'auds'); payload: the AVI's host path. Result HRESULT, payload: the stream id (word)
  kFormat = 3,      // AVIStreamReadFormat at 0: payload the format
  kInfo = 4,        // AVIStreamInfoA: payload the 0x8c bytes
  kLength = 5,      // AVIStreamLength
  kRead = 6,        // AVIStreamRead(a start, b samples, c bytes): the sound's bytes; a picture's chunk stays in the helper
  kFind = 7,        // AVIStreamFindSample(a pos, b flags)
  kDecompress = 8,  // ICDecompress of the picture's chunk read last (a: flags); payload in: the output BITMAPINFOHEADER
                    // (with the masks); out: the picture, (width rounded to 4) x 2 bytes a row, top down
  kRelease = 9,     // AVIStreamRelease (and the codec, for a picture)
  kQuit = 10,
};

struct Request {
  uint32_t op = 0, stream = 0, a = 0, b = 0, c = 0, d = 0;
  std::vector<uint8_t> data;
};

inline bool read_all(std::FILE* f, void* p, size_t n) { return n == 0 || std::fread(p, 1, n, f) == n; }
inline bool write_all(std::FILE* f, const void* p, size_t n) { return n == 0 || std::fwrite(p, 1, n, f) == n; }

inline bool read_request(std::FILE* f, Request* r) {
  uint32_t w[7];
  if (!read_all(f, w, sizeof w)) return false;
  r->op = w[0], r->stream = w[1], r->a = w[2], r->b = w[3], r->c = w[4], r->d = w[5];
  if (w[6] > (64u << 20)) return false;
  r->data.resize(w[6]);
  return read_all(f, r->data.data(), r->data.size());
}

inline bool write_answer(std::FILE* f, int32_t result, const void* data, uint32_t size) {
  uint32_t w[2] = {static_cast<uint32_t>(result), size};
  const bool ok = write_all(f, w, sizeof w) && write_all(f, data, size);
  std::fflush(f);
  return ok;
}

}  // namespace vfwpipe
