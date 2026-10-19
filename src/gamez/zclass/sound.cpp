#include "gamez/zclass/sound.h"

#include <cstring>

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zsound/zsnd.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\Sound.c";
void* find_default(const char* name) { return zsnd::find(name); }
FindSound g_find_sound = find_default;

// The voice's stop in 0x004fd370 / 0x004fd520: stopped, unlocked if kept.
void stop_voice(Sound* s) {
  auto* v = static_cast<zsnd::VoiceBase*>(s->voice);
  zsnd::stop(v);
  if (s->get<uint32_t>(0x2c) & 8) {
    zsnd::unlock(v);
    s->put<uint32_t>(0x2c, s->get<uint32_t>(0x2c) & ~8u);
  }
  s->voice = nullptr;
  s->put<uint32_t>(4, 0);
}

void clear_flag(Sound* s, uint32_t f) { s->put<uint32_t>(0x2c, s->get<uint32_t>(0x2c) & ~f); }
void set_flag(Sound* s, uint32_t f) { s->put<uint32_t>(0x2c, s->get<uint32_t>(0x2c) | f); }
}  // namespace

void set_find_sound(FindSound fn) { g_find_sound = fn; }

// Mech3.exe 1.2 @ 0x004fd280. Active, on list 10.
Node* sound_create() {
  Node* node = node_alloc();
  if (!node) return zerr::report(0x400, kFile, 0x76, "Null node pointer."), nullptr;
  node->put(0x74, 1.0f), node->put(0x78, 1.0f), node->put(0x80, 2.0f);
  node->put(0x84, 2.0f), node->put(0x7c, -2.0f), node->put(0x88, -1.0f);
  node->set_flags(node->flags() | 0x100);
  node->put<int32_t>(0x34, 10);
  auto* s = new Sound{};
  node->data = s;
  s->put<uint32_t>(0, 0), s->put<uint32_t>(4, 0);
  s->put<int32_t>(0x78, 1);
  s->put(0x7c, 32.0f), s->put(0x80, 64.0f), s->put(0x84, 4096.0f), s->put(0x88, 0.03125f);
  s->put<uint32_t>(0x2c, s->get<uint32_t>(0x2c) | 1);
  node_set_active(node, 1);
  s->put<int32_t>(0x8c, 0);
  s->nodes = nullptr;
  push_front(kSounds, node);
  return node;
}

// Mech3.exe 1.2 @ 0x004fd520. Turning off a playing sound stops it.
int sound_set_active(Node* node, int on) {
  if (!node) return zerr::report(0x400, kFile, 0x149, "Null node pointer."), 5;
  auto* s = static_cast<Sound*>(node->data);
  if (!s) return zerr::report(0x400, kFile, 0x14a, "Null class data pointer"), 5;
  if (s->voice && on == 0) stop_voice(s);
  if (on == 1) node->set_flags(node->flags() | 4);
  else if (on == 0) node->set_flags(node->flags() & ~4u);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fd480. The name is kept (34 characters at most) and
// the sound looked up by it.
int sound_set_name(Node* node, const char* name) {
  if (!node) return zerr::report(0x400, kFile, 0x11e, "Null node pointer."), 5;
  auto* s = static_cast<Sound*>(node->data);
  if (!s) return zerr::report(0x400, kFile, 0x11f, "Null class data pointer"), 5;
  char* dst = reinterpret_cast<char*>(s->raw + 8);
  if (std::strlen(name) < 0x24) {
    std::strcpy(dst, name);
  } else {
    std::strncpy(dst, name, 0x22);
    s->raw[0x2b] = 0;
  }
  void* found = g_find_sound ? g_find_sound(name) : nullptr;
  s->put<uint32_t>(0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(found)));
  s->sound = found;
  s->put<uint32_t>(4, 0);
  s->voice = nullptr;
  s->put<uint32_t>(0x2c, s->get<uint32_t>(0x2c) | 1);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fd5c0: the position (+0x30), flags 1 and 2 set.
int sound_set_translation(Node* node, float x, float y, float z) {
  if (!node) return zerr::report(0x400, kFile, 0x17e, "Null node pointer."), 5;
  auto* s = static_cast<Sound*>(node->data);
  if (!s) return zerr::report(0x400, kFile, 0x17f, "Null class data pointer"), 5;
  s->put(0x30, x), s->put(0x34, y), s->put(0x38, z);
  set_flag(s, 3);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fd620
int sound_get_translation(Node* node, float* x, float* y, float* z) {
  if (!node) return zerr::report(0x400, kFile, 0x1d0, "Null node pointer."), 5;
  auto* s = static_cast<Sound*>(node->data);
  if (!s) return zerr::report(0x400, kFile, 0x1d1, "Null class data pointer"), 5;
  *x = s->get<float>(0x30), *y = s->get<float>(0x34), *z = s->get<float>(0x38);
  return 0;
}

// Mech3.exe 1.2 @ 0x004fd780 (ECX node, EDX data): the node's origin in the
// world, into +0x3c.
void sound_world_position(Node* node, Sound* data) {
  zmth::Mat34 buf;
  zmth::push(&buf);
  zmth::load_identity();
  node_to_world(node, 1);
  zmth::MatrixStack& ms = zmth::matrix_stack();
  float p[3] = {0.0f, 0.0f, 0.0f};
  if (!ms.is_identity()) {
    const float* m = ms.matrix().m;
    const float o = 0.0f;  // the origin, multiplied out as the exe does
    p[0] = ((o * m[0] + o * m[3]) + o * m[6]) + m[9];
    p[1] = ((o * m[1] + o * m[4]) + o * m[7]) + m[10];
    p[2] = ((o * m[2] + o * m[5]) + o * m[8]) + m[11];
  }
  data->put(0x3c, p[0]), data->put(0x40, p[1]), data->put(0x44, p[2]);
  zmth::pop();
}

// Mech3.exe 1.2 @ 0x004fd680. An active sound node starts its sound once and
// keeps the voice (locked). A node under a parent, or given a position, plays
// in 3D at its world position, moved each frame.
int sound_update(Node* node) {
  if (!node) return zerr::report(0x400, kFile, 0x224, "Null node pointer."), 5;
  if (!(node->flags() & 4)) return 0;
  auto* s = static_cast<Sound*>(node->data);
  if (!s) return zerr::report(0x400, kFile, 0x22a, "Null class data pointer"), 5;
  if (!s->voice && (node_root(node) != node || (s->get<uint32_t>(0x2c) & 2))) set_flag(s, 4);
  auto* snd = static_cast<zsnd::Object*>(s->sound);
  if (s->get<uint32_t>(0x2c) & 4) {
    sound_world_position(node, s);
    if (s->voice) {
      zsnd::update_3d(static_cast<zsnd::Voice*>(s->voice), s->floats(0x3c), nullptr, 0);
      clear_flag(s, 1);
      return 0;
    }
    if (!snd) return clear_flag(s, 1), 0;
    s->voice = zsnd::play_at(snd, s->floats(0x3c), 1.0f, nullptr);
  } else {
    if (s->voice || !snd) return clear_flag(s, 1), 0;
    s->voice = zsnd::play(snd, 1.0f);
  }
  s->put<uint32_t>(4, s->voice ? 1u : 0u);
  if (zsnd::lock(static_cast<zsnd::VoiceBase*>(s->voice))) set_flag(s, 8);
  clear_flag(s, 1);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f3170 (ECX world): each of the world's sound nodes.
int world_sounds(Node* world) {
  auto* w = static_cast<World*>(world->data);
  const int32_t n = w->get<int32_t>(0xac);
  for (int32_t i = 0; i < n; ++i) sound_update(w->sounds[i]);
  return 0;
}

}  // namespace zcls
