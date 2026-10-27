// Probes for test_fmv.py: zFMV's script (gamez/zfmv) with every call outside
// recorded, and zRender's blur passes on given buffers.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "gamez/zfmv/fmv_script.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zrender/zrndr_blur.h"
#include "gamez/zrender/zrndr_ripple.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "probe_app.h"

namespace {

struct Recording : zfmv::Host {
  int32_t mode = 1;
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return probe_app::call(key, args);
  }
  int32_t video_mode() override { return mode; }  // a read in the original, not a call
};

Recording g_host;
zfmv::Script* g_script = nullptr;

int32_t action_index(const zfmv::Script& s) {
  if (!s.current) return -1;
  int32_t i = 0;
  for (const zfmv::Action* a = s.head; a; a = a->next, ++i)
    if (a == s.current) return i;
  return -2;
}

}  // namespace

extern "C" {

// A new script, the old one deleted (unrecorded); the display's pixel format.
void probe_fmv_reset(int32_t mode, int32_t r_bits, int32_t g_bits, int32_t b_bits) {
  zfmv::set_host(&g_host);
  probe_app::Mute quiet;
  delete g_script;
  g_host.mode = mode;
  zvid::set_pixel_format(r_bits, g_bits, b_bits);
  g_script = new zfmv::Script(nullptr, nullptr, 0);
}

void probe_fmv_mode(int32_t mode) { g_host.mode = mode; }

// 0x00598d80 on the binary tree in `data` (as zrdr::load reads one from an
// archive), the block `name`.
int32_t probe_fmv_load(const uint8_t* data, int32_t size, const char* name) {
  std::FILE* f = std::tmpfile();
  if (!f) return -100;
  std::fwrite(data, 1, static_cast<size_t>(size), f);
  std::rewind(f);
  zrdr::Node* root = zrdr::new_nodes(zrdr::List, 1);
  zrdr::read_node(f, root);
  std::fclose(f);
  const int32_t n = g_script->load_tree(root, name);
  zrdr::free_node(root);
  return n;
}

int32_t probe_fmv_play(double t) { return g_script->play_at(t); }
int32_t probe_fmv_playing(double t) { return g_script->playing_at(t); }
void probe_fmv_stop(int32_t all) { g_script->stop(all != 0); }
// The current action's index (-1 none) and the number of actions.
int32_t probe_fmv_current() { return action_index(*g_script); }
int32_t probe_fmv_count() {
  int32_t n = 0;
  for (const zfmv::Action* a = g_script->head; a; a = a->next) ++n;
  return n;
}
// Each action as the original's object holds it: its vtable, then words.
int32_t probe_fmv_actions(uint32_t* out, int32_t cap) {
  int32_t n = 0;
  auto put = [&](uint32_t v) {
    if (n < cap) out[n] = v;
    ++n;
  };
  for (const zfmv::Action* a = g_script->head; a; a = a->next) {
    if (auto* i = dynamic_cast<const zfmv::ImageAction*>(a)) {
      put(0x5cf850), put(i->flip), put(i->placed);
      put(static_cast<uint32_t>(i->rect.left)), put(static_cast<uint32_t>(i->rect.top));
      put(static_cast<uint32_t>(i->rect.right)), put(static_cast<uint32_t>(i->rect.bottom));
    } else if (auto* w = dynamic_cast<const zfmv::WaitAction*>(a)) {
      put(0x5cf820), put(zgame::bits(w->seconds));
    } else if (auto* f = dynamic_cast<const zfmv::FadeAction*>(a)) {
      put(0x5cf868), put(static_cast<uint32_t>(f->out)), put(f->colour), put(zgame::bits(f->seconds));
      put(zgame::bits(f->alpha));
    } else if (auto* v = dynamic_cast<const zfmv::AviAction*>(a)) {
      put(0x5cf880), put(v->flag), put(!v->path.empty());
    } else if (auto* b = dynamic_cast<const zfmv::BlurAction*>(a)) {
      put(b->kind == 1 ? 0x5cf808 : b->kind == 2 ? 0x5cf7f0 : 0x5cf898);
      put(static_cast<uint32_t>(b->steps)), put(static_cast<uint32_t>(b->passes));
    } else if (dynamic_cast<const zfmv::SoundAction*>(a)) {
      put(0x5cf7d8);
    } else {
      put(0);
    }
  }
  return n;
}

void probe_fmv_delete() {
  delete g_script;
  g_script = nullptr;
}

// The pause screen's blur: 0x00599f70(steps, passes) run as 0x00599a70.
void probe_fmv_blur_run(int32_t steps, int32_t passes) {
  zfmv::BlurAction b(steps, passes, 3);
  b.run();
}

// 0x0055a540 on `screen` (h rows of pitch_px pixels) and `scratch` (w * h),
// both returned; the display's channel masks; rect 4 ints or null.
void probe_blur(uint16_t* screen, int32_t w, int32_t h, int32_t pitch_px, uint16_t* scratch, const int32_t* rect,
                int32_t kind, uint32_t mr, uint32_t mg, uint32_t mb) {
  zrndr::set_finish_target(screen, w, h, pitch_px * 2);
  zvid::channel_masks() = {mr, mg, mb};
  auto& s = zrndr::ripple_state().scratch;
  s.assign(scratch, scratch + static_cast<size_t>(w) * h);
  zrndr::blur(rect, kind);
  std::memcpy(scratch, s.data(), sizeof(uint16_t) * static_cast<size_t>(w) * h);
}
}
