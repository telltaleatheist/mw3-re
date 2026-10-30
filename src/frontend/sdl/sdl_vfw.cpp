#include "frontend/sdl/sdl_vfw.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "platform/vfw_protocol.h"

namespace sdlfe {
namespace {

// The whole of n bytes, waiting on a pipe that is not ready yet.
bool read_full(SDL_IOStream* io, void* p, size_t n) {
  auto* b = static_cast<uint8_t*>(p);
  while (n > 0) {
    const size_t got = SDL_ReadIO(io, b, n);
    if (got == 0) {
      if (SDL_GetIOStatus(io) != SDL_IO_STATUS_NOT_READY) return false;
      SDL_Delay(1);
      continue;
    }
    b += got, n -= got;
  }
  return true;
}

bool write_full(SDL_IOStream* io, const void* p, size_t n) {
  if (n == 0) return true;
  if (SDL_WriteIO(io, p, n) != n) return false;
  SDL_FlushIO(io);  // a pipe writes through; its flush may say it has nothing to do
  return true;
}

}  // namespace

SdlVfwHelper::~SdlVfwHelper() {
  if (ok_) {
    int32_t r = 0;
    call(vfwpipe::kQuit, 0, 0, 0, 0, nullptr, 0, &r, nullptr);
  }
  stop();
}

void SdlVfwHelper::stop() {
  ok_ = false;
  if (proc_) {
    SDL_KillProcess(proc_, true);
    SDL_WaitProcess(proc_, true, nullptr);
    SDL_DestroyProcess(proc_);
  }
  proc_ = nullptr, in_ = out_ = nullptr;
}

bool SdlVfwHelper::ready() {
  if (tried_) return ok_;
  tried_ = true;
  const char* args[] = {helper_.c_str(), "--serve", codec_.c_str(), nullptr};
  proc_ = SDL_CreateProcess(args, true);
  if (!proc_) {
    hello_ = std::string("the helper did not start: ") + SDL_GetError();
    return false;
  }
  in_ = SDL_GetProcessInput(proc_);
  out_ = SDL_GetProcessOutput(proc_);
  ok_ = in_ && out_;
  int32_t r = -1;
  std::vector<uint8_t> text;
  if (ok_ && call(vfwpipe::kHello, 0, 0, 0, 0, nullptr, 0, &r, &text)) hello_.assign(text.begin(), text.end());
  if (r != static_cast<int32_t>(vfwpipe::kVersion)) {
    if (hello_.empty()) hello_ = "the helper did not answer";
    stop();
  }
  return ok_;
}

bool SdlVfwHelper::call(uint32_t op, uint32_t stream, uint32_t a, uint32_t b, uint32_t c, const void* data,
                        size_t size, int32_t* result, std::vector<uint8_t>* answer) {
  if (!ok_) return false;
  const uint32_t w[7] = {op, stream, a, b, c, 0, static_cast<uint32_t>(size)};
  uint32_t head[2] = {};
  if (!write_full(in_, w, sizeof w) || !write_full(in_, data, size) || !read_full(out_, head, sizeof head)) {
    std::fprintf(stderr, "mw3: the Indeo helper stopped answering; the movies fall back to the imported files\n");
    stop();
    return false;
  }
  std::vector<uint8_t> scratch;
  std::vector<uint8_t>* into = answer ? answer : &scratch;
  into->resize(head[1]);
  if (!read_full(out_, into->data(), into->size())) {
    stop();
    return false;
  }
  *result = static_cast<int32_t>(head[0]);
  return true;
}

int32_t SdlVfwHelper::open(const std::string& path, uint32_t type, uint32_t* stream) {
  *stream = 0;
  int32_t r = -1;
  std::vector<uint8_t> id;
  if (!call(vfwpipe::kOpen, 0, type, 0, 0, path.data(), path.size(), &r, &id)) return -1;
  if (r == 0 && id.size() == 4) std::memcpy(stream, id.data(), 4);
  return r;
}

int32_t SdlVfwHelper::format(uint32_t stream, std::vector<uint8_t>* out) {
  int32_t r = -1;
  return call(vfwpipe::kFormat, stream, 0, 0, 0, nullptr, 0, &r, out) ? r : -1;
}

int32_t SdlVfwHelper::info(uint32_t stream, void* info_0x8c) {
  int32_t r = -1;
  std::vector<uint8_t> d;
  if (!call(vfwpipe::kInfo, stream, 0, 0, 0, nullptr, 0, &r, &d)) return -1;
  std::memcpy(info_0x8c, d.data(), std::min<size_t>(d.size(), 0x8c));
  return r;
}

int32_t SdlVfwHelper::length(uint32_t stream) {
  int32_t r = 0;
  return call(vfwpipe::kLength, stream, 0, 0, 0, nullptr, 0, &r, nullptr) ? r : 0;
}

int32_t SdlVfwHelper::read(uint32_t stream, int32_t start, int32_t samples, void* buffer, int32_t bytes) {
  int32_t r = -1;
  std::vector<uint8_t> d;
  if (!call(vfwpipe::kRead, stream, static_cast<uint32_t>(start), static_cast<uint32_t>(samples),
            static_cast<uint32_t>(bytes < 0 ? 0 : bytes), nullptr, 0, &r, &d))
    return -1;
  if (buffer && !d.empty()) std::memcpy(buffer, d.data(), std::min(d.size(), static_cast<size_t>(bytes)));
  return r;
}

int32_t SdlVfwHelper::find_sample(uint32_t stream, int32_t pos, uint32_t flags) {
  int32_t r = -1;
  return call(vfwpipe::kFind, stream, static_cast<uint32_t>(pos), flags, 0, nullptr, 0, &r, nullptr) ? r : -1;
}

int32_t SdlVfwHelper::decompress(uint32_t stream, uint32_t flags, const void* out_format, size_t out_bytes, void* image,
                                 size_t image_bytes) {
  int32_t r = -1;
  std::vector<uint8_t> d;
  if (!call(vfwpipe::kDecompress, stream, flags, 0, 0, out_format, out_bytes, &r, &d)) return -1;
  if (r == 0) std::memcpy(image, d.data(), std::min(d.size(), image_bytes));
  return r;
}

void SdlVfwHelper::release(uint32_t stream) {
  int32_t r = 0;
  call(vfwpipe::kRelease, stream, 0, 0, 0, nullptr, 0, &r, nullptr);
}

}  // namespace sdlfe
