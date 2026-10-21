#pragma once

// zGeometry's Weiler–Atherton polygon clipping (zGeometry\zgeo_weiler.cpp,
// 0x0050f950..0x005158c0; docs/craters.md, section 5). Ported so far: the
// leaf the model side uses.
namespace zgeo {

// a and b within `eps` in x and in y.
int points_near(const float a[2], const float b[2], float eps);  // 0x00515130 ECX a, EDX b

}  // namespace zgeo
