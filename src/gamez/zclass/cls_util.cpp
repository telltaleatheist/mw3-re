#include "gamez/zclass/cls_util.h"

#include <cstdint>
#include <cstring>

#pragma STDC FP_CONTRACT OFF

namespace zcls {

float approx_sqrt(float x) {
  int32_t b;
  std::memcpy(&b, &x, 4);
  b = (b >> 1) + 0x1fc00000;  // sar: a negative input stays negative
  float r;
  std::memcpy(&r, &b, 4);
  return r;
}

// Mech3.exe 1.2 @ 0x004f9c40
void corners_sphere(const float corners[8][3], float centre[3], float* radius) {
  float lo[3] = {corners[0][0], corners[0][1], corners[0][2]};
  float hi[3] = {lo[0], lo[1], lo[2]};
  for (int i = 1; i < 8; ++i) {
    for (int k = 0; k < 3; ++k) {
      const float v = corners[i][k];
      if (v < lo[k]) lo[k] = v;
      else if (hi[k] < v) hi[k] = v;
    }
  }
  const float hx = (hi[0] - lo[0]) * 0.5f, hy = (hi[1] - lo[1]) * 0.5f, hz = (hi[2] - lo[2]) * 0.5f;
  centre[0] = hx + lo[0], centre[1] = hy + lo[1], centre[2] = hz + lo[2];
  const float xy = hx * hx + hy * hy;
  const float sum = xy + hz * hz;
  *radius = approx_sqrt(sum);
}

// Mech3.exe 1.2 @ 0x004f9bc0
void box_sphere(const float box[6], float centre[3], float* radius) {
  const float hx = (box[3] - box[0]) * 0.5f, hy = (box[4] - box[1]) * 0.5f, hz = (box[5] - box[2]) * 0.5f;
  centre[0] = box[0] + hx, centre[1] = hy + box[1], centre[2] = hz + box[2];
  *radius = approx_sqrt((hx * hx + hy * hy) + hz * hz);
}

}  // namespace zcls
