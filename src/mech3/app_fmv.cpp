// The program's zFMV (gamez/zfmv, docs/fmv.md): the movie scripts the states
// play through the Engine's movie hooks, and the pause screen's blur, on
// GameEngine's 16-bit buffer and Render3D. Our addition, not original code:
// the zfmv::Host the exe's modules stood behind.
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>

#include "gamez/zfmv/fmv_script.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zrender/zrndr_blur.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/app_engine.h"
#include "mech3/app_movie.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "platform/vfw.h"

namespace mech3 {

namespace {

class GameFmvHost : public zfmv::Host {
 public:
  explicit GameFmvHost(GameEngine* e) : e_(e) {}
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return e_->unported(key, args);
  }

  Handle target_base() override { return e_->video_target(); }
  Handle target_width() override { return static_cast<Handle>(e_->buffer_width()); }
  Handle target_height() override { return static_cast<Handle>(e_->buffer_height()); }
  Handle target_pitch() override { return static_cast<Handle>(e_->buffer_width() * 2); }
  Handle render_base() override { return e_->video_target(); }
  Handle render_pitch() override { return static_cast<Handle>(e_->buffer_width() * 2); }
  Handle screen_width() override { return static_cast<Handle>(e_->buffer_width()); }
  Handle screen_height() override { return static_cast<Handle>(e_->buffer_height()); }
  int32_t video_mode() override { return e_->video_mode(); }
  void video_lock() override { e_->video_lock(); }
  void video_unlock() override { e_->video_unlock(); }
  void lock_3d() override { e_->video_begin_3d(); }
  void unlock_3d() override { e_->video_end_3d(); }
  void video_flip(Handle a, Handle b, int32_t c, int32_t d) override {
    e_->video_flip(static_cast<int32_t>(a), static_cast<int32_t>(b), c, d);
  }
  Handle video_grab(int32_t buffer) override { return e_->video_grab(buffer); }
  // FIX (A): one 2D buffer stands for the back buffer and the second surface,
  // so the blur's copies between them are nothing to do.
  void copy_to_second(const zfmv::Rect*, const zfmv::Rect*) override {}
  void copy_from_second(const zfmv::Rect*, const zfmv::Rect*) override {}
  void image_stretch(Handle image, int32_t, const zfmv::Rect*, const zfmv::Rect* dst) override {
    if (auto* t = texture(image); t && dst) e_->fmv_stretch(t, dst->left, dst->top, dst->right, dst->bottom);
  }
  void scene_begin() override { e_->video_begin_scene(); }
  void draw_tint_quad() override { e_->video_draw_3d(); }
  void scene_end() override { e_->video_end_scene(); }
  // The 2D lock points the finish target at the buffer (GameEngine::video_lock).
  void set_finish_target(Handle, Handle, Handle, Handle) override {}
  void render_target(int32_t* w, int32_t* h, int32_t*, int32_t*) override {
    *w = e_->buffer_width();
    *h = e_->buffer_height();
  }
  void image_draw(Handle image, int32_t x, int32_t y, int32_t key, const zfmv::Rect* src) override {
    if (e_->is_grab(image)) return e_->image_draw(image, x, y, key, 0);
    ztwod::host().blit(texture(image), x, y, key, reinterpret_cast<const ztwod::Rect*>(src));
  }
  void tint(uint16_t colour, const zfmv::Rect*, double alpha) override { e_->screen_tint(colour, 0, alpha); }
  void finish() override { e_->render_finish(); }
  void blur(const zfmv::Rect* r, int32_t kind) override {
    zrndr::blur(r ? &r->left : nullptr, kind);
  }
  Handle image_get(const char* name) override { return e_->handle_of(ztwod::host().image_get(name)); }
  void image_release(Handle image) override {
    if (e_->is_grab(image)) return e_->image_free(image);
    ztwod::host().image_release(texture(image));
  }
  void image_path(const char* path) override { ztwod::host().image_path(path); }
  void sound_load_set(const char* name) override { e_->voice_start(name); }
  Handle sound_snapshot() override { return e_->sound_listener(); }
  void sound_stop_snapshot(Handle s) override { e_->sound_stop_all(s); }
  Handle sound_find(const char* name) override { return e_->sound_find(name); }
  Handle sound_play(Handle sound, float volume) override {
    auto* o = static_cast<zsnd::Object*>(e_->object_of(sound));
    return o ? e_->handle_of(zsnd::play(o, volume)) : 0;
  }
  void sound_stop(Handle voice) override { e_->voice_stop(voice); }
  void input_clear() override { e_->input_clear(); }
  uint32_t key_state(int32_t dik) override { return e_->key_state(dik); }
  // The movies are the importer's files (app_movie.h): "found" when the
  // The AVI through the helper's Video for Windows, else the imported .fmv.
  // FIX (A): no AVIFile or Indeo 5 here (docs/fmv.md, The Indeo helper).
  int32_t file_stat(const char* path) override { return remote_path(path).empty() && movie_base(path).empty() ? -1 : 0; }

  // Video for Windows: Windows' own and the install's codec in the 32-bit
  // helper when there is one (platform::VideoForWindows), else the imported
  // files (FIX (A), app_movie.h).
  void avi_file_init() override {}
  void avi_file_exit() override {}
  int32_t avi_open(Handle* stream, const char* path, uint32_t type) override {
    *stream = 0;
    auto o = std::make_unique<Open>();
    o->video = type == 0x73646976;  // 'vids'
    const std::string host = remote_path(path);
    if (!host.empty() && vfw()->open(host, type, &o->remote) == 0 && o->remote) {
      vfw()->format(o->remote, &o->format);
      vfw()->info(o->remote, &o->info);
    } else {
      o->remote = 0;
      o->imported = MovieStream::open(movie_base(path), type);
      if (!o->imported) return -1;  // AVIERR_FILEOPEN
      o->format = o->video ? o->imported->info().video_format : o->imported->info().audio_format;
      o->info = o->video ? o->imported->info().video : o->imported->info().audio;
    }
    *stream = e_->handle_of(o.get());
    streams_[*stream] = std::move(o);
    return 0;
  }
  int32_t avi_read_format(Handle stream, void* format, int32_t* size) override {
    Open* m = movie(stream);
    if (!m) return -1;
    const std::vector<uint8_t>& f = m->format;
    if (!format) return *size = static_cast<int32_t>(f.size()), 0;
    std::memcpy(format, f.data(), std::min(f.size(), static_cast<size_t>(*size < 0 ? 0 : *size)));
    return 0;
  }
  int32_t avi_length(Handle stream) override {
    Open* m = movie(stream);
    if (!m) return 0;
    return m->remote ? vfw()->length(m->remote) : static_cast<int32_t>(m->info.length);
  }
  int32_t avi_info(Handle stream, zfmv::AviStreamInfo* info) override {
    Open* m = movie(stream);
    if (!m) return -1;
    *info = m->info;
    return 0;
  }
  int32_t avi_read(Handle stream, int32_t start, int32_t samples, void* buffer, int32_t bytes) override {
    Open* m = movie(stream);
    if (!m) return -1;
    return m->remote ? vfw()->read(m->remote, start, samples, buffer, bytes) : m->imported->read(start, samples, buffer, bytes);
  }
  int32_t avi_find_sample(Handle stream, int32_t pos, uint32_t flags) override {
    Open* m = movie(stream);
    if (!m) return -1;
    return m->remote ? vfw()->find_sample(m->remote, pos, flags) : m->imported->find_sample(pos, flags);
  }
  void avi_release(Handle stream) override {
    auto it = streams_.find(stream);
    if (it == streams_.end()) return;
    if (decoding_ == it->second.get()) decoding_ = nullptr;
    if (it->second->remote && vfw()) vfw()->release(it->second->remote);
    e_->forget_object(it->second.get());
    streams_.erase(it);
  }
  // The Indeo 5 codec: the picture stream opened last decodes.
  Handle ic_locate(uint32_t, uint32_t, const void*, const void*, uint32_t) override {
    for (auto& [h, m] : streams_)
      if (m->video) decoding_ = m.get();
    return decoding_ ? 1 : 0;
  }
  int32_t ic_send(Handle, uint32_t, const void*, const void*) override { return 0; }
  int32_t ic_decompress(Handle, uint32_t flags, const void*, const void*, const void* out, void* image) override {
    if (!decoding_ || !out) return -1;
    int32_t w = 0, h = 0;
    uint16_t bits = 0;
    uint32_t size = 0, compression = 0;
    std::memcpy(&size, out, 4);
    std::memcpy(&w, static_cast<const uint8_t*>(out) + 4, 4);
    std::memcpy(&h, static_cast<const uint8_t*>(out) + 8, 4);
    std::memcpy(&bits, static_cast<const uint8_t*>(out) + 14, 2);
    std::memcpy(&compression, static_cast<const uint8_t*>(out) + 16, 4);
    if (bits != 16) return -1;
    const int32_t rows = h < 0 ? -h : h, pitch = ((w + 3) & ~3) * 2;
    if (decoding_->remote)  // the header and, for BI_BITFIELDS, its three masks
      return vfw()->decompress(decoding_->remote, flags, out, size + (compression == 3 ? 12 : 0), image,
                               static_cast<size_t>(rows) * pitch);
    const zvid::ChannelMasks& m = zvid::channel_masks();
    return decoding_->imported->decompress(static_cast<uint16_t*>(image), w, rows, pitch, m.r, m.g, m.b);
  }
  void ic_close(Handle) override { decoding_ = nullptr; }
  uint32_t display_bits() override { return 16; }
  // Each frame drawn passes here: counted for --movie (ours, for checking).
  Handle stream_image(zfmv::Stream* stream) override {
    if (!stream) return 0;
    GameEngine::MovieStats& st = e_->movie_stats;
    ++st.drawn;
    st.last = stream->shown, st.length = stream->length;
    return e_->handle_of(&stream->image);
  }
  // zSound's streaming buffer (zsnd_create.cpp, ported).
  Handle sound_stream_create(const void* format, const void* data, uint32_t bytes) override {
    platform::WaveFormat wf;
    std::memcpy(&wf, format, sizeof wf);
    return e_->handle_of(zsnd::stream_create(&wf, data, bytes));
  }
  int32_t sound_stream_lock(Handle sound, uint32_t offset, uint32_t bytes, void** p1, uint32_t* n1, void** p2,
                            uint32_t* n2) override {
    *p1 = *p2 = nullptr, *n1 = *n2 = 0;
    auto* s = static_cast<zsnd::Sound*>(e_->object_of(sound));
    return s ? zsnd::stream_lock(s, offset, bytes, p1, n1, p2, n2) : 0;
  }
  void sound_stream_unlock(Handle sound, void* p1, uint32_t n1, void* p2, uint32_t n2) override {
    if (auto* s = static_cast<zsnd::Sound*>(e_->object_of(sound))) zsnd::stream_unlock(s, p1, n1, p2, n2);
  }
  uint32_t sound_stream_position(Handle sound) override {
    auto* s = static_cast<zsnd::Sound*>(e_->object_of(sound));
    return s ? zsnd::stream_position(s) : 0;
  }
  void sound_stream_free(Handle sound) override {
    if (auto* s = static_cast<zsnd::Sound*>(e_->object_of(sound))) {
      e_->forget_object(s);
      zsnd::stream_free(s);
    }
  }
  // timeGetTime() x 0.001f, the x87 at 24 bits (a float).
  double now() override {
    const mech3::Platform& p = e_->platform_parts();
    const platform::Clock* c = p.movie_clock ? p.movie_clock : p.clock;
    const uint32_t ms = c ? const_cast<platform::Clock*>(c)->ticks_ms() : 0;
    return static_cast<double>(static_cast<float>(ms) * 0.001f);
  }

 private:
  zimg::Texture* texture(Handle h) const { return static_cast<zimg::Texture*>(e_->object_of(h)); }
  // An opened stream: the helper's (remote, its id) or the imported files'.
  struct Open {
    bool video = false;
    uint32_t remote = 0;
    std::unique_ptr<MovieStream> imported;
    std::vector<uint8_t> format;
    zfmv::AviStreamInfo info;
  };
  Open* movie(Handle h) const {
    auto it = streams_.find(h);
    return it == streams_.end() ? nullptr : it->second.get();
  }
  platform::VideoForWindows* vfw() const { return e_->platform_parts().vfw; }
  // The AVI's host path when the helper is up and the mounts have the file.
  std::string remote_path(const char* path) const {
    platform::VideoForWindows* v = vfw();
    if (!v || !path || !v->ready()) return {};
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    while (p.rfind("./", 0) == 0) p.erase(0, 2);
    return zutl::find(p.c_str());
  }
  GameEngine* e_;
  std::map<Handle, std::unique_ptr<Open>> streams_;
  Open* decoding_ = nullptr;
};

std::unique_ptr<GameFmvHost> g_host;

}  // namespace

void wire_fmv(GameEngine* e) {
  if (e) {
    g_host = std::make_unique<GameFmvHost>(e);
    zfmv::set_host(g_host.get());
  } else {
    zfmv::set_host(nullptr);
    g_host.reset();
  }
}

// The scripts (new(0x20) + 0x00598cc0(0, 0, 0)) by handle.
Handle GameEngine::movie_new() { return handle_of(new zfmv::Script(nullptr, nullptr, 0)); }
void GameEngine::movie_set_window(Handle m, uint32_t hwnd) {
  if (auto* s = static_cast<zfmv::Script*>(object_of(m))) s->hwnd = hwnd;
}
int32_t GameEngine::movie_load(Handle m, const char* file, const char* name) {
  auto* s = static_cast<zfmv::Script*>(object_of(m));
  return s ? s->load(file, name) : -1;
}
void GameEngine::movie_play(Handle m) {
  if (auto* s = static_cast<zfmv::Script*>(object_of(m))) s->play();
}
uint32_t GameEngine::movie_playing(Handle m) {
  auto* s = static_cast<zfmv::Script*>(object_of(m));
  return s && s->playing();
}
void GameEngine::movie_stop(Handle m, int32_t all) {
  if (auto* s = static_cast<zfmv::Script*>(object_of(m))) s->stop(all != 0);
}
void GameEngine::movie_delete(Handle m) {
  auto* s = static_cast<zfmv::Script*>(object_of(m));
  forget_object(s);
  delete s;
}

// The pause screen's blur: 0x00599f70(4, 1) on the stack, its vtable the
// BLUR action's (0x005cf898), run as 0x00599a70 does.
Handle GameEngine::wait_new(int32_t steps, int32_t passes) {
  pause_blur_ = std::make_unique<zfmv::BlurAction>(steps, passes, 3);
  return 0;
}
void GameEngine::wait_start() {
  if (pause_blur_) pause_blur_->start(0.0);
}
uint32_t GameEngine::wait_poll() { return pause_blur_ && pause_blur_->step(0.0); }
void GameEngine::wait_end() {
  if (pause_blur_) pause_blur_->end();
  pause_blur_.reset();
}

// [0x0072f360] (0x005773e0): the whole image Blt-stretched over the rect of
// the 2D target. FIX (A): onto the 16-bit buffer, nearest pixel, as
// DirectDraw's stretch without filtering.
void GameEngine::fmv_stretch(zimg::Texture* image, int32_t left, int32_t top, int32_t right, int32_t bottom) {
  if (!image || !image->pixels || image->width <= 0 || image->height <= 0) return;
  if (last_via_render_ && !buffer_is_screen_) buffer_is_screen_ = true;  // drawn over whatever was shown
  const int32_t x0 = left < 0 ? 0 : left, y0 = top < 0 ? 0 : top;
  const int32_t x1 = right > width_ ? width_ : right, y1 = bottom > height_ ? height_ : bottom;
  const int32_t w = right - left, h = bottom - top;
  if (w <= 0 || h <= 0) return;
  const int32_t iw = image->width, ih = image->height;
  for (int32_t y = y0; y < y1; ++y) {
    const int32_t sy = static_cast<int32_t>(static_cast<int64_t>(y - top) * ih / h);
    for (int32_t x = x0; x < x1; ++x) {
      const int32_t sx = static_cast<int32_t>(static_cast<int64_t>(x - left) * iw / w);
      const size_t i = static_cast<size_t>(sy) * iw + sx;
      const uint16_t c = image->palette_count ? image->palette[static_cast<const uint8_t*>(image->pixels)[i]]
                                              : static_cast<const uint16_t*>(image->pixels)[i];
      buffer_[static_cast<size_t>(y) * width_ + x] = c;
    }
  }
}

}  // namespace mech3
