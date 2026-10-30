// mw3indeo_fake: the Indeo helper's pipe answered from the importer's files
// (ours, for the tests; docs/fmv.md, The Indeo helper). It stands for the
// 32-bit mw3indeo where there is no Windows: the same messages, the AVI's
// path mapped to `<video dir>/<name>.fmv` and answered by the imported
// streams (mech3/app_movie.h), so mw3's side of the pipe and the movie host's
// routing are checked on any platform. The real helper's answers come from
// AVIFile and the codec; only the PC checks those.
//
//   mw3indeo_fake --serve CODEC    (as mw3 starts the helper; the video dir
//                                   from MW3_FAKE_INDEO_DIR, CODEC unused)
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>

#include "mech3/app_movie.h"
#include "platform/vfw_protocol.h"

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv) {
  const char* video = std::getenv("MW3_FAKE_INDEO_DIR");
  if (argc < 3 || std::strcmp(argv[1], "--serve") != 0 || !video) {
    std::fprintf(stderr, "MW3_FAKE_INDEO_DIR=VIDEO_DIR mw3indeo_fake --serve CODEC\n");
    return 2;
  }
#if defined(_WIN32)
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  const std::string dir = video;
  struct Open {
    std::unique_ptr<mech3::MovieStream> m;
    std::vector<uint8_t> image;
  };
  std::map<uint32_t, Open> streams;
  uint32_t next = 1;
  vfwpipe::Request r;
  while (vfwpipe::read_request(stdin, &r)) {
    auto it = streams.find(r.stream);
    mech3::MovieStream* s = it == streams.end() ? nullptr : it->second.m.get();
    switch (r.op) {
      case vfwpipe::kHello: {
        const std::string t = "the test helper on " + dir;
        vfwpipe::write_answer(stdout, static_cast<int32_t>(vfwpipe::kVersion), t.data(), static_cast<uint32_t>(t.size()));
        break;
      }
      case vfwpipe::kOpen: {
        std::string name(r.data.begin(), r.data.end());
        const size_t slash = name.find_last_of("/\\");
        name = name.substr(slash == std::string::npos ? 0 : slash + 1);
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name.size() > 4) name.erase(name.size() - 4);
        auto m = mech3::MovieStream::open(dir + "/" + name, r.a);
        if (!m) {
          vfwpipe::write_answer(stdout, -1, nullptr, 0);
          break;
        }
        streams[next].m = std::move(m);
        const uint32_t id = next++;
        vfwpipe::write_answer(stdout, 0, &id, 4);
        break;
      }
      case vfwpipe::kFormat: {
        if (!s) {
          vfwpipe::write_answer(stdout, -1, nullptr, 0);
          break;
        }
        const std::vector<uint8_t>& f = s->video() ? s->info().video_format : s->info().audio_format;
        vfwpipe::write_answer(stdout, 0, f.data(), static_cast<uint32_t>(f.size()));
        break;
      }
      case vfwpipe::kInfo:
        if (s) vfwpipe::write_answer(stdout, 0, s->video() ? &s->info().video : &s->info().audio, 0x8c);
        else vfwpipe::write_answer(stdout, -1, nullptr, 0);
        break;
      case vfwpipe::kLength:
        vfwpipe::write_answer(stdout, s ? static_cast<int32_t>((s->video() ? s->info().video : s->info().audio).length) : 0,
                              nullptr, 0);
        break;
      case vfwpipe::kRead: {
        std::vector<uint8_t> buf(s && !s->video() ? r.c : 0);
        const int32_t res = s ? s->read(static_cast<int32_t>(r.a), static_cast<int32_t>(r.b), buf.empty() ? nullptr : buf.data(),
                                        static_cast<int32_t>(buf.size()))
                              : -1;
        vfwpipe::write_answer(stdout, res, buf.data(), static_cast<uint32_t>(buf.size()));
        break;
      }
      case vfwpipe::kFind:
        vfwpipe::write_answer(stdout, s ? s->find_sample(static_cast<int32_t>(r.a), r.b) : -1, nullptr, 0);
        break;
      case vfwpipe::kDecompress: {
        if (!s || r.data.size() < 40) {
          vfwpipe::write_answer(stdout, -1, nullptr, 0);
          break;
        }
        int32_t w = 0, h = 0;
        uint32_t masks[3] = {0xf800, 0x07e0, 0x001f};
        std::memcpy(&w, &r.data[4], 4);
        std::memcpy(&h, &r.data[8], 4);
        if (r.data.size() >= 52) std::memcpy(masks, &r.data[40], 12);
        const int32_t rows = h < 0 ? -h : h, pitch = ((w + 3) & ~3) * 2;
        std::vector<uint8_t>& img = it->second.image;
        img.assign(static_cast<size_t>(rows) * pitch, 0);
        const int32_t res = s->decompress(reinterpret_cast<uint16_t*>(img.data()), w, rows, pitch, masks[0], masks[1], masks[2]);
        vfwpipe::write_answer(stdout, res, res == 0 ? img.data() : nullptr, res == 0 ? static_cast<uint32_t>(img.size()) : 0);
        break;
      }
      case vfwpipe::kRelease:
        if (it != streams.end()) streams.erase(it);
        vfwpipe::write_answer(stdout, 0, nullptr, 0);
        break;
      case vfwpipe::kQuit:
        vfwpipe::write_answer(stdout, 0, nullptr, 0);
        return 0;
      default:
        vfwpipe::write_answer(stdout, -1, nullptr, 0);
    }
  }
  return 0;
}
