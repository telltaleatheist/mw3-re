// The program's zFMV (gamez/zfmv, docs/fmv.md): the movie scripts the states
// play through the Engine's movie hooks, and the pause screen's blur, on
// GameEngine's 16-bit buffer and Render3D. Our addition, not original code:
// the zfmv::Host the exe's modules stood behind.
#include <memory>

#include "gamez/zfmv/fmv_script.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zrender/zrndr_blur.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/app_engine.h"

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
  // Until the movie player is ported (frontend-plan.md item 11) no movie is
  // found, so PLAYAVI ends at once.
  int32_t file_stat(const char*) override { return -1; }
  // timeGetTime() x 0.001f, the x87 at 24 bits (a float).
  double now() override {
    const mech3::Platform& p = e_->platform_parts();
    const platform::Clock* c = p.movie_clock ? p.movie_clock : p.clock;
    const uint32_t ms = c ? const_cast<platform::Clock*>(c)->ticks_ms() : 0;
    return static_cast<double>(static_cast<float>(ms) * 0.001f);
  }

 private:
  zimg::Texture* texture(Handle h) const { return static_cast<zimg::Texture*>(e_->object_of(h)); }
  GameEngine* e_;
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
