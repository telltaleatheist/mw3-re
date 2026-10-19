#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\Camera.c
namespace zcls {

int camera_set_clip(Node* node, float near_clip, float far_clip);  // 0x004f3a70
int camera_set_fov(Node* node, float h, float v);                  // 0x004f3b90

}  // namespace zcls
