// mw3indeo: the movies' Video for Windows calls in a 32-bit process (ours,
// not original code; docs/fmv.md, The Indeo helper). FIX (A): the install's
// Indeo 5 codec, Ir50_32.dll, is a 32-bit Video for Windows driver that
// 64-bit mw3 cannot load and that modern Windows no longer registers. This
// helper loads it, installs it for itself alone (ICInstall with
// ICINSTALL_FUNCTION: no registry, no administrator) and answers the movie
// player's AVIFile and ICDecompress calls over its stdin and stdout
// (vfw_protocol.h), with Windows' own AVIFIL32 and MSVFW32, so the frames are
// the codec's own as the original showed them.
//
//   mw3indeo --serve DLL                  answer mw3 (it starts the helper)
//   mw3indeo --check DLL AVI [N [OUT [REF]]]   the PC's check: the codec
//       loaded and opened, every frame decoded and timed, frame N (default
//       30) written as OUT.bmp (24-bit) and OUT.565 (the raw 16 bits), and
//       compared with REF (raw RGB565, FFmpeg's decode of that frame); the
//       DLLs loaded.
//
// Built 32-bit with clang-cl as a GUI program (tools/windows/build_indeo.cmd),
// so mw3 starting it opens no console; Windows only.
#if !defined(_WIN32)
#error "mw3indeo is Windows' (32-bit): the codec is an x86 Win32 driver"
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vfw.h>
#include <fcntl.h>
#include <io.h>
#include <psapi.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "platform/vfw_protocol.h"  // -I src

namespace {

constexpr DWORD kIV50 = mmioFOURCC('I', 'V', '5', '0'), kiv50 = mmioFOURCC('i', 'v', '5', '0');

// The codec loaded and installed for this process; the error text when not.
bool install_codec(const char* dll, std::string* why) {
  HMODULE m = LoadLibraryA(dll);
  if (!m) {
    *why = std::string("LoadLibrary ") + dll + " failed, error " + std::to_string(GetLastError());
    return false;
  }
  FARPROC proc = GetProcAddress(m, "DriverProc");  // the driver function ICInstall takes as its lParam
  if (!proc) {
    *why = "no DriverProc in the codec";
    return false;
  }
  const BOOL upper = ICInstall(ICTYPE_VIDEO, kIV50, reinterpret_cast<LPARAM>(proc), nullptr, ICINSTALL_FUNCTION);
  const BOOL lower = ICInstall(ICTYPE_VIDEO, kiv50, reinterpret_cast<LPARAM>(proc), nullptr, ICINSTALL_FUNCTION);
  if (!upper && !lower) {
    *why = "ICInstall failed";
    return false;
  }
  return true;
}

// The image's bytes for a 16-bit BITMAPINFOHEADER: rows of DWORDs.
size_t image_bytes(const BITMAPINFOHEADER& b) {
  const LONG h = b.biHeight < 0 ? -b.biHeight : b.biHeight;
  return static_cast<size_t>(h) * ((static_cast<size_t>(b.biWidth) * b.biBitCount / 8 + 3) & ~size_t(3));
}

struct Stream {
  PAVISTREAM avi = nullptr;
  AVISTREAMINFOA info{};
  std::vector<uint8_t> format;  // AVIStreamReadFormat's
  std::vector<uint8_t> chunk;   // the picture's chunk read last
  LONG chunk_bytes = 0;
  HIC hic = nullptr;
  std::vector<uint8_t> out;     // the output format ICDecompressBegin was given
  std::vector<uint8_t> image;
  ~Stream() {
    if (hic) ICDecompressEnd(hic), ICClose(hic);
    if (avi) AVIStreamRelease(avi);
  }
};

// ICDecompress of the chunk read last, as the movie player asks it.
LRESULT decompress(Stream& s, DWORD flags, const std::vector<uint8_t>& out) {
  if (s.format.size() < sizeof(BITMAPINFOHEADER) || out.size() < sizeof(BITMAPINFOHEADER)) return ICERR_BADFORMAT;
  auto* in = reinterpret_cast<BITMAPINFOHEADER*>(s.format.data());
  const auto* o = reinterpret_cast<const BITMAPINFOHEADER*>(out.data());
  if (!s.hic || s.out != out) {
    if (s.hic) ICDecompressEnd(s.hic), ICClose(s.hic);
    s.hic = ICLocate(ICTYPE_VIDEO, s.info.fccHandler, in, const_cast<BITMAPINFOHEADER*>(o), ICMODE_DECOMPRESS);
    if (!s.hic) return ICERR_UNSUPPORTED;  // no driver takes this format
    const LRESULT begun = ICDecompressBegin(s.hic, in, o);
    if (begun != ICERR_OK) return begun;
    s.out = out;
  }
  s.image.assign(image_bytes(*o), 0);
  in->biSizeImage = static_cast<DWORD>(s.chunk_bytes);
  return ICDecompress(s.hic, flags, in, s.chunk.data(), const_cast<BITMAPINFOHEADER*>(o), s.image.data());
}

bool open_stream(Stream& s, const char* path, DWORD type) {
  if (AVIStreamOpenFromFileA(&s.avi, path, type, 0, OF_READ, nullptr) != 0) return false;
  AVIStreamInfoA(s.avi, &s.info, sizeof s.info);
  LONG size = 0;
  AVIStreamReadFormat(s.avi, 0, nullptr, &size);
  s.format.assign(size > 0 ? static_cast<size_t>(size) : 0, 0);
  if (size > 0) AVIStreamReadFormat(s.avi, 0, s.format.data(), &size);
  return true;
}

int serve(const char* dll) {
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
  std::string why;
  const bool codec = install_codec(dll, &why);
  AVIFileInit();
  std::map<uint32_t, Stream> streams;
  uint32_t next = 1;
  vfwpipe::Request r;
  while (vfwpipe::read_request(stdin, &r)) {
    auto it = streams.find(r.stream);
    Stream* s = it == streams.end() ? nullptr : &it->second;
    switch (r.op) {
      case vfwpipe::kHello: {
        const std::string text = codec ? "Indeo 5 from " + std::string(dll) : why;
        vfwpipe::write_answer(stdout, codec ? static_cast<int32_t>(vfwpipe::kVersion) : -1, text.data(),
                              static_cast<uint32_t>(text.size()));
        break;
      }
      case vfwpipe::kOpen: {
        const std::string path(r.data.begin(), r.data.end());
        Stream& n = streams[next];
        if (!open_stream(n, path.c_str(), r.a)) {
          streams.erase(next);
          vfwpipe::write_answer(stdout, static_cast<int32_t>(AVIERR_FILEOPEN), nullptr, 0);
        } else {
          const uint32_t id = next++;
          vfwpipe::write_answer(stdout, 0, &id, 4);
        }
        break;
      }
      case vfwpipe::kFormat:
        if (s) vfwpipe::write_answer(stdout, 0, s->format.data(), static_cast<uint32_t>(s->format.size()));
        else vfwpipe::write_answer(stdout, -1, nullptr, 0);
        break;
      case vfwpipe::kInfo:
        if (s) vfwpipe::write_answer(stdout, 0, &s->info, sizeof s->info);
        else vfwpipe::write_answer(stdout, -1, nullptr, 0);
        break;
      case vfwpipe::kLength:
        vfwpipe::write_answer(stdout, s ? AVIStreamLength(s->avi) : 0, nullptr, 0);
        break;
      case vfwpipe::kRead: {
        if (!s) {
          vfwpipe::write_answer(stdout, -1, nullptr, 0);
          break;
        }
        std::vector<uint8_t> buf(r.c);
        LONG got_bytes = 0, got_samples = 0;
        const HRESULT hr = AVIStreamRead(s->avi, static_cast<LONG>(r.a), static_cast<LONG>(r.b), buf.empty() ? nullptr : buf.data(),
                                         static_cast<LONG>(r.c), &got_bytes, &got_samples);
        if (s->info.fccType == streamtypeVIDEO) {
          s->chunk = std::move(buf);
          s->chunk_bytes = got_bytes;
          vfwpipe::write_answer(stdout, hr, nullptr, 0);
        } else {
          vfwpipe::write_answer(stdout, hr, buf.data(), static_cast<uint32_t>(buf.size()));
        }
        break;
      }
      case vfwpipe::kFind:
        vfwpipe::write_answer(stdout, s ? AVIStreamFindSample(s->avi, static_cast<LONG>(r.a), static_cast<LONG>(r.b)) : -1, nullptr,
                              0);
        break;
      case vfwpipe::kDecompress: {
        const LRESULT e = s ? decompress(*s, r.a, r.data) : ICERR_BADHANDLE;
        if (s && e == ICERR_OK) vfwpipe::write_answer(stdout, 0, s->image.data(), static_cast<uint32_t>(s->image.size()));
        else vfwpipe::write_answer(stdout, static_cast<int32_t>(e), nullptr, 0);
        break;
      }
      case vfwpipe::kRelease:
        if (s) streams.erase(it);
        vfwpipe::write_answer(stdout, 0, nullptr, 0);
        break;
      case vfwpipe::kQuit:
        vfwpipe::write_answer(stdout, 0, nullptr, 0);
        streams.clear();
        AVIFileExit();
        return 0;
      default:
        vfwpipe::write_answer(stdout, -1, nullptr, 0);
    }
  }
  streams.clear();
  AVIFileExit();
  return 0;
}

void write_bmp(const std::string& path, const std::vector<uint8_t>& image, int w, int h) {
  const int row = (w * 3 + 3) & ~3;
  BITMAPFILEHEADER f{};
  BITMAPINFOHEADER b{};
  f.bfType = 0x4d42;
  f.bfOffBits = sizeof f + sizeof b;
  f.bfSize = f.bfOffBits + row * h;
  b.biSize = sizeof b, b.biWidth = w, b.biHeight = -h, b.biPlanes = 1, b.biBitCount = 24;
  std::FILE* out = std::fopen(path.c_str(), "wb");
  if (!out) return;
  std::fwrite(&f, sizeof f, 1, out);
  std::fwrite(&b, sizeof b, 1, out);
  const int pitch = (w * 2 + 3) & ~3;
  std::vector<uint8_t> line(row, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      uint16_t p;
      std::memcpy(&p, &image[y * pitch + x * 2], 2);
      const int r = p >> 11, g = (p >> 5) & 63, bl = p & 31;
      line[x * 3 + 0] = static_cast<uint8_t>(bl << 3 | bl >> 2);
      line[x * 3 + 1] = static_cast<uint8_t>(g << 2 | g >> 4);
      line[x * 3 + 2] = static_cast<uint8_t>(r << 3 | r >> 2);
    }
    std::fwrite(line.data(), 1, line.size(), out);
  }
  std::fclose(out);
}

void list_modules() {
  HMODULE mods[512];
  DWORD bytes = 0;
  if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &bytes)) return;
  for (DWORD i = 0; i < bytes / sizeof(HMODULE); ++i) {
    char name[MAX_PATH];
    if (GetModuleFileNameA(mods[i], name, MAX_PATH)) {
      std::string n = name;
      for (char& c : n) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
      if (n.find("ir50") != std::string::npos || n.find("indeo") != std::string::npos || n.find("avifil") != std::string::npos ||
          n.find("msvfw") != std::string::npos)
        std::printf("  loaded: %s\n", name);
    }
  }
}

// Frame against a reference (both 16-bit 565, `pitch` bytes a row): the PSNR
// over the three channels widened to 8 bits, and the largest difference.
void compare(const std::vector<uint8_t>& a, const std::string& ref_path, int w, int h) {
  std::vector<uint8_t> ref;
  if (std::FILE* f = std::fopen(ref_path.c_str(), "rb")) {
    uint8_t buf[1 << 16];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) ref.insert(ref.end(), buf, buf + n);
    std::fclose(f);
  }
  const int pitch = (w * 2 + 3) & ~3;
  if (ref.size() < static_cast<size_t>(w) * h * 2) return (void)std::printf("reference %s: too short\n", ref_path.c_str());
  double sq = 0;
  int worst = 0;
  auto wide = [](uint16_t p, int c) {
    const int v = c == 0 ? p >> 11 : c == 1 ? (p >> 5) & 63 : p & 31;
    return c == 1 ? (v << 2 | v >> 4) : (v << 3 | v >> 2);
  };
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      uint16_t p, q;
      std::memcpy(&p, &a[y * pitch + x * 2], 2);
      std::memcpy(&q, &ref[(y * w + x) * 2], 2);  // FFmpeg's rows are unpadded
      for (int c = 0; c < 3; ++c) {
        const int d = wide(p, c) - wide(q, c);
        sq += d * d;
        worst = d < 0 ? (-d > worst ? -d : worst) : (d > worst ? d : worst);
      }
    }
  const double mse = sq / (3.0 * w * h);
  std::printf("against %s: PSNR %.1f dB, largest difference %d of 255\n", ref_path.c_str(),
              mse > 0 ? 10.0 * std::log10(255.0 * 255.0 / mse) : 99.0, worst);
}

int check(const char* dll, const char* avi, int want, const std::string& out, const char* ref) {
  std::string why;
  if (!install_codec(dll, &why)) return std::printf("codec: %s\n", why.c_str()), 1;
  std::printf("codec: %s loaded, DriverProc installed (ICINSTALL_FUNCTION)\n", dll);
  // DRV_OPEN through ICM: the driver's own name and description.
  if (HIC h = ICOpen(ICTYPE_VIDEO, kIV50, ICMODE_DECOMPRESS)) {
    ICINFO info{};
    info.dwSize = sizeof info;
    ICGetInfo(h, &info, sizeof info);
    std::printf("ICOpen: %p, \"%ls\", \"%ls\", version %lu\n", static_cast<void*>(h), info.szName, info.szDescription,
                static_cast<unsigned long>(info.dwVersion));
    ICClose(h);
  } else {
    return std::printf("ICOpen(vidc, IV50) failed\n"), 1;
  }
  AVIFileInit();
  int status = 1;
  {
    Stream s;
    if (!open_stream(s, avi, streamtypeVIDEO)) {
      std::printf("AVIStreamOpenFromFile %s failed\n", avi);
    } else {
      const auto* in = reinterpret_cast<const BITMAPINFOHEADER*>(s.format.data());
      const LONG frames = AVIStreamLength(s.avi);
      std::printf("%s: %ldx%ld, %ld frames, handler %.4s\n", avi, static_cast<long>(in->biWidth), static_cast<long>(in->biHeight),
                  static_cast<long>(frames), reinterpret_cast<const char*>(&s.info.fccHandler));
      // The movie player's output: 16 bits, BI_BITFIELDS 565, top down.
      std::vector<uint8_t> o(sizeof(BITMAPINFOHEADER) + 12, 0);
      auto* b = reinterpret_cast<BITMAPINFOHEADER*>(o.data());
      b->biSize = sizeof(BITMAPINFOHEADER), b->biWidth = in->biWidth, b->biHeight = -in->biHeight, b->biPlanes = 1;
      b->biBitCount = 16, b->biCompression = BI_BITFIELDS;
      const DWORD masks[3] = {0xf800, 0x07e0, 0x001f};
      std::memcpy(o.data() + sizeof(BITMAPINFOHEADER), masks, sizeof masks);
      b->biSizeImage = static_cast<DWORD>(image_bytes(*b));
      LARGE_INTEGER f, t0, t1;
      QueryPerformanceFrequency(&f);
      QueryPerformanceCounter(&t0);
      int failed = 0;
      std::vector<uint8_t> kept;
      for (LONG n = 0; n < frames; ++n) {
        LONG need = 0;
        AVIStreamRead(s.avi, n, 1, nullptr, 0, &need, nullptr);
        s.chunk.assign(need > 0 ? static_cast<size_t>(need) : 1, 0);
        AVIStreamRead(s.avi, n, 1, s.chunk.data(), need, &s.chunk_bytes, nullptr);
        const DWORD flags = AVIStreamIsKeyFrame(s.avi, n) ? 0 : ICDECOMPRESS_NOTKEYFRAME;
        if (decompress(s, flags, o) != ICERR_OK) ++failed;
        if (n == want) kept = s.image;
      }
      QueryPerformanceCounter(&t1);
      const double ms = 1000.0 * static_cast<double>(t1.QuadPart - t0.QuadPart) / static_cast<double>(f.QuadPart);
      std::printf("decoded %ld frames in %.0f ms (%.2f ms a frame), %d failed\n", static_cast<long>(frames), ms,
                  frames ? ms / frames : 0.0, failed);
      if (!kept.empty()) {
        write_bmp(out + ".bmp", kept, in->biWidth, in->biHeight);
        if (std::FILE* raw = std::fopen((out + ".565").c_str(), "wb")) std::fwrite(kept.data(), 1, kept.size(), raw), std::fclose(raw);
        std::printf("frame %d written to %s.bmp and %s.565\n", want, out.c_str(), out.c_str());
        if (ref) compare(kept, ref, in->biWidth, in->biHeight);
      }
      status = failed ? 1 : 0;
    }
  }
  list_modules();
  AVIFileExit();
  return status;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 3 && std::strcmp(argv[1], "--serve") == 0) return serve(argv[2]);
  // A GUI program (no console window when mw3 starts it): --check, run from
  // a console, writes to that console.
  if (AttachConsole(ATTACH_PARENT_PROCESS) && _fileno(stdout) < 0) {
    std::freopen("CONOUT$", "w", stdout);
    std::freopen("CONOUT$", "w", stderr);
  }
  if (argc >= 4 && std::strcmp(argv[1], "--check") == 0)
    return check(argv[2], argv[3], argc >= 5 ? std::atoi(argv[4]) : 30, argc >= 6 ? argv[5] : "mw3indeo-frame",
                 argc >= 7 ? argv[6] : nullptr);
  std::fprintf(stderr, "mw3indeo --serve Ir50_32.dll | --check Ir50_32.dll MOVIE.AVI [N [OUT [REF.565]]] (docs/fmv.md, The Indeo helper)\n");
  return 2;
}
#endif
