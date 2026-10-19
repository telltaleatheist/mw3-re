#include "gamez/zclass/camera.h"

#include <cmath>

#include "gamez/zclass/cls_data.h"

namespace zcls {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zClass\\Camera.c";
}  // namespace

// Mech3.exe 1.2 @ 0x004f3a70
int camera_set_clip(Node* node, float near_clip, float far_clip) {
  if (int r = check_class(node, 1, kFile, 0x5ca)) return r;
  auto* c = static_cast<Camera*>(node->data);
  c->put(0xb0, near_clip), c->put(0xb4, far_clip);
  c->put<int32_t>(0xf8, 1);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f3b90. The angle is clamped to 1.396 rad (80 degrees).
int camera_set_fov(Node* node, float h, float v) {
  if (int r = check_class(node, 1, kFile, 0x614)) return r;
  auto* c = static_cast<Camera*>(node->data);
  const float ah = c->get<float>(0xe0) / h, av = c->get<float>(0xe4) / v;
  c->put(0xd8, h), c->put(0xdc, v);
  c->put(0xe8, ah > 1.396f ? 1.396f : ah);  // 0x005cdb4c
  c->put(0xec, av > 1.396f ? 1.396f : av);
  const double hh = c->get<float>(0xe8) * 0.5, hv = c->get<float>(0xec) * 0.5;
  c->put<int32_t>(0x138, 1), c->put<int32_t>(0xf8, 1);
  c->put(0xf0, static_cast<float>(hh)), c->put(0xf4, static_cast<float>(hv));
  c->put(0x1d4, static_cast<float>(1.0 / std::tan(hh)));
  c->put(0x1d8, static_cast<float>(1.0 / std::tan(hv)));
  return 0;
}

}  // namespace zcls
