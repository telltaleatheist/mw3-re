#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\Sound.c
namespace zcls {

Node* sound_create();                               // 0x004fd280
int sound_set_active(Node* node, int on);           // 0x004fd520
int sound_set_name(Node* node, const char* name);   // 0x004fd480

// zSound's lookup by name (0x0056f6f0), not ported yet.
using FindSound = void* (*)(const char* name);
void set_find_sound(FindSound fn);

}  // namespace zcls
