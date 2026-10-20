#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\Sound.c
namespace zcls {

Node* sound_create();                               // 0x004fd280
int sound_delete(Node* node);                       // 0x004fd370
int sound_set_active(Node* node, int on);           // 0x004fd520
int sound_set_name(Node* node, const char* name);   // 0x004fd480

int sound_set_translation(Node* node, float x, float y, float z);     // 0x004fd5c0
int sound_get_translation(Node* node, float* x, float* y, float* z);  // 0x004fd620
int sound_update(Node* node);                        // 0x004fd680, each frame
void sound_world_position(Node* node, struct Sound* data);  // 0x004fd780
int world_sounds(Node* world);                       // 0x004f3170, the scene walk's hook

// zSound's lookup by name (0x0056f6f0): zsnd::find unless a test sets another.
using FindSound = void* (*)(const char* name);
void set_find_sound(FindSound fn);

}  // namespace zcls
