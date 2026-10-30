#pragma once
#include <string>

#include "platform/vfw.h"

struct SDL_Process;
struct SDL_IOStream;

// platform::VideoForWindows through the helper mw3indeo (ours; docs/fmv.md,
// The Indeo helper): started with the codec's path on the first movie, each
// call one message over its pipes (tools/indeo/vfw_protocol.h). A helper
// that does not start, lacks the codec or stops answering makes ready()
// false from then on, and the movies fall back to the importer's files.
namespace sdlfe {

class SdlVfwHelper : public platform::VideoForWindows {
 public:
  SdlVfwHelper(std::string helper, std::string codec) : helper_(std::move(helper)), codec_(std::move(codec)) {}
  ~SdlVfwHelper() override;
  bool ready() override;
  int32_t open(const std::string& path, uint32_t type, uint32_t* stream) override;
  int32_t format(uint32_t stream, std::vector<uint8_t>* out) override;
  int32_t info(uint32_t stream, void* info_0x8c) override;
  int32_t length(uint32_t stream) override;
  int32_t read(uint32_t stream, int32_t start, int32_t samples, void* buffer, int32_t bytes) override;
  int32_t find_sample(uint32_t stream, int32_t pos, uint32_t flags) override;
  int32_t decompress(uint32_t stream, uint32_t flags, const void* out_format, size_t out_bytes, void* image,
                     size_t image_bytes) override;
  void release(uint32_t stream) override;
  const std::string& hello() const { return hello_; }  // the helper's first answer (the codec, or why not)

 private:
  // One request and its answer; false (and the helper given up) when the pipe fails.
  bool call(uint32_t op, uint32_t stream, uint32_t a, uint32_t b, uint32_t c, const void* data, size_t size,
            int32_t* result, std::vector<uint8_t>* answer);
  void stop();
  std::string helper_, codec_, hello_;
  SDL_Process* proc_ = nullptr;
  SDL_IOStream* in_ = nullptr;   // the helper's stdin
  SDL_IOStream* out_ = nullptr;  // its stdout
  bool tried_ = false, ok_ = false;
};

}  // namespace sdlfe
