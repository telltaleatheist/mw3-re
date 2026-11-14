// Probes for test_movie2d.py (docs/twod.md, Movies in a dialog; Lines in a
// dialog): Movie2D's start, colour key, update, draw and erase on a stream
// whose fields the test sets (its frame step 0x0059aaf0 and the blit
// 0x0055b1f0 recorded), and Line2D's reader 0x005a6a10 on a .zrd entry.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "gamez/zfmv/fmv_stream.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zvideo/zvid_format.h"

namespace {
uint32_t fb(float f) {
  uint32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bf(uint32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}

std::vector<uint32_t> g_calls;  // 1 frame n; 2 blit: image (1 the stream's, 2 the background), x, y, key, has rect, l t r b
zimg::Texture* g_stream_image = nullptr;
zimg::Texture* g_background = nullptr;

struct MovieHost : ztwod::Host {
  int32_t movie_frame(zfmv::Stream*, int32_t f) override {
    g_calls.insert(g_calls.end(), {1, static_cast<uint32_t>(f)});
    return 0;
  }
  void blit(zimg::Texture* image, int32_t x, int32_t y, int32_t flags, const ztwod::Rect* src) override {
    const uint32_t which = image == g_stream_image ? 1 : image == g_background ? 2 : 0;
    g_calls.insert(g_calls.end(), {2, which, static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                                   static_cast<uint32_t>(flags) & 0xffff, src ? 1u : 0u});
    for (int32_t v : {src ? src->left : 0, src ? src->top : 0, src ? src->right : 0, src ? src->bottom : 0})
      g_calls.push_back(static_cast<uint32_t>(v));
  }
} g_host;
}  // namespace

extern "C" {

// One movie: st = x, y, flags, timer, time, key, has stream, stream w, h, flags byte, length, fps, has
// background, background w, h, bg_rect l t r b (19 words); ops = (op, a) pairs: 1 start, 2 colour key a,
// 3 update dt a (bits), 4 draw, 5 erase. out per op: the calls (count first), then x, y, flags, timer, time,
// key, bg_rect, the stream image's flags byte.
int32_t probe_movie2d(const uint32_t* st, const uint32_t* ops, int32_t nops, uint32_t* out, int32_t cap) {
  zvid::set_pixel_format(5, 6, 5);
  ztwod::Host* host_was = &ztwod::host();
  ztwod::set_host(&g_host);
  int32_t k = 0;
  {
    ztwod::Movie2D m;
    m.x = static_cast<int32_t>(st[0]), m.y = static_cast<int32_t>(st[1]);
    m.flags = st[2], m.timer = bf(st[3]), m.time = bf(st[4]), m.color_key = static_cast<uint16_t>(st[5]);
    zimg::Texture bg{};
    if (st[6]) {
      m.stream = new zfmv::Stream("", false);  // no file: only the fields the movie reads
      m.stream->image.width = static_cast<int16_t>(st[7]), m.stream->image.height = static_cast<int16_t>(st[8]);
      m.stream->image.flags = static_cast<uint8_t>(st[9]);
      m.stream->length = static_cast<int32_t>(st[10]), m.stream->fps = st[11];
    }
    if (st[12]) {
      bg.width = static_cast<int16_t>(st[13]), bg.height = static_cast<int16_t>(st[14]);
      m.background = &bg;
    }
    m.bg_rect = {static_cast<int32_t>(st[15]), static_cast<int32_t>(st[16]), static_cast<int32_t>(st[17]),
                 static_cast<int32_t>(st[18])};
    g_stream_image = m.stream ? &m.stream->image : nullptr;
    g_background = m.background;
    for (int32_t i = 0; i < nops; ++i) {
      g_calls.clear();
      const uint32_t op = ops[2 * i], a = ops[2 * i + 1];
      if (op == 1) m.start();
      if (op == 2) m.set_color_key(static_cast<uint16_t>(a));
      if (op == 3) m.update(bf(a));
      if (op == 4) m.draw();
      if (op == 5) m.erase();
      if (k + static_cast<int32_t>(g_calls.size()) + 16 > cap) break;
      out[k++] = static_cast<uint32_t>(g_calls.size());
      for (uint32_t v : g_calls) out[k++] = v;
      for (uint32_t v : {static_cast<uint32_t>(m.x), static_cast<uint32_t>(m.y), m.flags, fb(m.timer), fb(m.time),
                         static_cast<uint32_t>(m.color_key), static_cast<uint32_t>(m.bg_rect.left),
                         static_cast<uint32_t>(m.bg_rect.top), static_cast<uint32_t>(m.bg_rect.right),
                         static_cast<uint32_t>(m.bg_rect.bottom),
                         m.stream ? static_cast<uint32_t>(m.stream->image.flags) : 0u})
        out[k++] = v;
    }
    m.background = nullptr;
  }
  ztwod::set_host(host_was);
  return k;
}

// Line2D's reader on a serialized .zrd entry, the line made at (x0 y0)-(x1 y1). out: the result, x, y,
// end x, end y, colour.
void probe_line2d_read(const uint8_t* zrd, int32_t len, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       uint32_t* out) {
  zvid::set_pixel_format(5, 6, 5);
  std::FILE* f = std::tmpfile();
  std::fwrite(zrd, 1, static_cast<size_t>(len), f);
  std::rewind(f);
  zrdr::Node tree{};
  zrdr::read_node(f, &tree);
  std::fclose(f);
  ztwod::Line2D l(x0, y0, x1, y1, 0);
  out[0] = l.read(&tree, nullptr) ? 1 : 0;
  out[1] = static_cast<uint32_t>(l.x), out[2] = static_cast<uint32_t>(l.y);
  out[3] = static_cast<uint32_t>(l.end_x), out[4] = static_cast<uint32_t>(l.end_y), out[5] = l.color;
}

}  // extern "C"
