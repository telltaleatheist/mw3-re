// The sounds' EXTRA records and the foot's wake (c1m1 item 1) for
// tests/differential/test_sound_extra.py.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "mech3/mech.h"
#include "mech3/mech_wake.h"
#include "mech3/sound_extra.h"

namespace {
std::vector<uint8_t> g_out;
void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void f32(float v) {
  uint32_t b;
  std::memcpy(&b, &v, 4);
  u32(b);
}
void str(const char* s) {
  if (!s) return u32(0xffffffffu);
  u32(static_cast<uint32_t>(std::strlen(s)));
  g_out.insert(g_out.end(), s, s + std::strlen(s));
}
int finish(uint8_t* out, int cap) {
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n;
}

// A list from the test's tokens: 1 int, 2 float bits, 3 string index, 4 a
// list (its item count, then the items).
const int32_t* g_tok;
const char* const* g_strs;
std::vector<std::unique_ptr<zrdr::Node[]>> g_lists;
void build(zrdr::Node& n) {
  const int32_t kind = *g_tok++;
  n.type = kind;
  if (kind == 1) n.i = *g_tok++;
  else if (kind == 2) std::memcpy(&n.f, g_tok++, 4);
  else if (kind == 3) n.s = const_cast<char*>(g_strs[*g_tok++]);
  else {
    const int32_t count = *g_tok++;
    g_lists.emplace_back(new zrdr::Node[static_cast<size_t>(count) + 1]{});
    zrdr::Node* l = g_lists.back().get();
    l[0].type = 1, l[0].i = count + 1;
    for (int32_t i = 1; i <= count; ++i) build(l[i]);
    n.list = l;
  }
}
}  // namespace

extern "C" {

// 0x004e6b00 on a list: none, or the record (subtitle, delay, next).
int probe_sx_load(const int32_t* tokens, const char* const* strings, uint8_t* out, int cap) {
  g_out.clear();
  g_tok = tokens, g_strs = strings;
  zrdr::Node root{};
  build(root);
  auto* r = static_cast<mech3::SoundExtra*>(mech3::sound_extra_load(nullptr, &root));
  u32(r != nullptr);
  if (r) str(r->subtitle), f32(r->min_delay), f32(r->next);
  mech3::sound_extras_clear();
  g_lists.clear();
  return finish(out, cap);
}

// 0x004e6cd0 on a record (subtitle 0 or "SUB"), at game time `now`, with
// the subtitle switch; the play and the subtitle stood in (logged).
int probe_sx_play(float next, float delay, int subtitle, int on, float now, float volume, int placed, float fade,
                  uint8_t* out, int cap) {
  g_out.clear();
  mech3::SoundExtra r;
  static char sub[] = "SUB";
  r.subtitle = subtitle ? sub : nullptr, r.min_delay = delay, r.next = next;
  mech3::subtitles_on() = on != 0;
  mech3::world().time = now;
  mech3::SoundExtraHooks& h = mech3::sound_extra_hooks();
  h.play_fading = [](zsnd::Object* obj, const float* pos, float vol, const float* vel, float f, int32_t stop) {
    u32(0x51), u32(obj != nullptr), u32(pos ? 1 : 0);
    if (pos) f32(pos[0]), f32(pos[1]), f32(pos[2]);
    f32(vol), u32(vel != nullptr), f32(f), u32(static_cast<uint32_t>(stop));
    return reinterpret_cast<zsnd::VoiceBase*>(0x7777);
  };
  h.subtitle = [](const char* t) { u32(0x52), str(t); };
  static int obj_dummy;
  const float at[3] = {1.5f, -2.0f, 30.25f};
  zsnd::VoiceBase* v = mech3::sound_extra_play(r, reinterpret_cast<zsnd::Object*>(&obj_dummy), volume,
                                               placed ? at : nullptr, nullptr, fade);
  u32(v ? 1u : 0u), f32(r.next);
  h = mech3::SoundExtraHooks{};
  return finish(out, cap);
}

// 0x004e6c60, the same without a place: zSound's 2D play logged.
int probe_sx_say(float next, float delay, int subtitle, int on, float now, float volume, uint8_t* out, int cap) {
  g_out.clear();
  mech3::SoundExtra r;
  static char sub[] = "SUB";
  r.subtitle = subtitle ? sub : nullptr, r.min_delay = delay, r.next = next;
  mech3::subtitles_on() = on != 0;
  mech3::world().time = now;
  mech3::SoundExtraHooks& h = mech3::sound_extra_hooks();
  h.play = [](zsnd::Object* obj, float vol) {
    u32(0x53), u32(obj != nullptr), f32(vol);
    return reinterpret_cast<zsnd::VoiceBase*>(0x7777);
  };
  h.subtitle = [](const char* t) { u32(0x52), str(t); };
  static int obj_dummy;
  zsnd::VoiceBase* v = mech3::sound_extra_say(r, reinterpret_cast<zsnd::Object*>(&obj_dummy), volume);
  u32(v ? 1u : 0u), f32(r.next);
  h = mech3::SoundExtraHooks{};
  return finish(out, cap);
}

// The wake: each op a throttle to start at (float bits) or 0x7fffffff to
// stop; the calls logged, then the level and whether it plays.
int probe_wake(const int32_t* ops, int n, uint8_t* out, int cap) {
  g_out.clear();
  mech3::WakeHooks& h = mech3::wake_hooks();
  static int counter = 0;
  counter = 0;
  h.start = [](zeff::AnimDef* def, zcls::Node* node) {
    u32(0x61), u32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(def))), u32(node != nullptr);
    return reinterpret_cast<zeff::AnimDef*>(static_cast<uintptr_t>(0x100 + ++counter));
  };
  h.stop = [](zeff::AnimDef* p) { u32(0x62), u32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); };
  h.set_active = [](zcls::Node*, int on) { u32(0x63), u32(static_cast<uint32_t>(on)); };
  mech3::FootWake w;
  static int node_dummy;
  w.node = reinterpret_cast<zcls::Node*>(&node_dummy);
  for (int i = 0; i < 3; ++i) w.anims[i] = reinterpret_cast<zeff::AnimDef*>(static_cast<uintptr_t>(0x10 + i));
  for (int i = 0; i < n; ++i) {
    if (ops[i] == 0x7fffffff) {
      mech3::foot_wake_stop(&w);
    } else {
      float t;
      std::memcpy(&t, ops + i, 4);
      mech3::foot_wake_start(&w, t);
    }
    u32(static_cast<uint32_t>(w.level)), u32(mech3::foot_wake_playing(&w));
  }
  h = mech3::WakeHooks{};
  return finish(out, cap);
}

}  // extern "C"
