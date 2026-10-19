#include "gamez/zclass/sound.h"

#include <cstring>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\Sound.c";
FindSound g_find_sound = nullptr;
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

// Mech3.exe 1.2 @ 0x004fd520. Turning off a playing sound stops it in zSound,
// which is not ported yet.
int sound_set_active(Node* node, int on) {
  if (!node) return zerr::report(0x400, kFile, 0x149, "Null node pointer."), 5;
  auto* s = static_cast<Sound*>(node->data);
  if (!s) return zerr::report(0x400, kFile, 0x14a, "Null class data pointer"), 5;
  if (s->get<uint32_t>(4) != 0 && on == 0) gamez_abort("sound_set_active: stopping a sound is not ported yet");
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
  s->put<uint32_t>(0x2c, s->get<uint32_t>(0x2c) | 1);
  return 0;
}

}  // namespace zcls
