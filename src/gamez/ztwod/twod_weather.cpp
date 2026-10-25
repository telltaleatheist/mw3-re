#include "gamez/ztwod/twod_weather.h"

#include <cmath>
#include <cstring>

#include "crt/msvc_math.h"
#include "crt/msvc_rand.h"
#include "gamez/zclass/class.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zmodel/gmod_clip2d.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zvideo/zvid_dd.h"
#include "gamez/zvideo/zvid_ddd3d.h"

#pragma STDC FP_CONTRACT OFF

namespace ztwod {

namespace {
WeatherLayer g_layer;
WeatherHooks g_hooks;
WeatherState g_state;
// [0x0072f348]: a Direct3D device (the draw functions' own test).
bool hardware() { return zrndr::device_fog().active != 0; }
int32_t rnd() { return g_hooks.rand ? g_hooks.rand() : crt::msvc_rand(); }
const Rect* screen_rect() {
  static Rect r;
  if (g_hooks.screen_rect) return g_hooks.screen_rect();
  return &r;
}
}  // namespace

WeatherLayer& weather_layer() { return g_layer; }
WeatherHooks& weather_hooks() { return g_hooks; }
WeatherState& weather_state() { return g_state; }

// Mech3.exe 1.2 @ 0x005a5470. With a device, the "SnowFX" ramp: 16 x 8,
// 16-bit colours and an alpha plane (type 0xb, +9 |= 0x20), its record clamped
// both ways. FIX (A): the planes are zeroed (malloc'd in the exe, and only the
// first row is ever written, by the draw).
Weather::Weather(int32_t particles) : Object2D(0, 0) {
  capacity = count = particles;
  streaks.assign(static_cast<size_t>(particles > 0 ? particles : 0), WeatherStreak{});
  pos[0].assign(static_cast<size_t>(particles > 0 ? 3 * particles : 0), 0.0f);
  pos[1].assign(pos[0].size(), 0.0f);
  for (int32_t i = 0; i < particles; ++i) reset(i);
  if (hardware()) {
    texture_name = "SnowFX";  // 0x005f4a54
    image = new zimg::Texture{};
    image->flags = 0xb;            // 0x00517c60
    ramp_pixels.assign(0x80, 0);       // 0x100 bytes
    ramp_alpha.assign(0x80, 0);
    image->pixels = ramp_pixels.data();  // 0x00517c70
    image->alpha = ramp_alpha.data();
    image->flags |= 2;
    image->flags |= 0x20;
    image->width = 0x10, image->height = 8, image->pixel_count = 0x80;  // 0x00517c90
    texture = zvid::create_texture(texture_name, *image, image->flags & 2, 1, 1);  // [0x0072f364]
  }
}

// Mech3.exe 1.2 @ 0x005a5640
Weather::~Weather() {
  if (hardware()) {
    if (texture) zvid::release_texture(texture);  // [0x0072f378]
    texture = nullptr;
  }
  delete image;  // 0x00517b10
  image = nullptr;
}

// Mech3.exe 1.2 @ 0x005a56e0. Depth 0.5..1; x and y -1..0.5, either one below
// -depth folded up by 1.5 with the depth turned to 1.5 - depth.
void Weather::reset(int32_t i) {
  float* p = &pos[cur][static_cast<size_t>(3 * i)];
  float* q = &pos[next][static_cast<size_t>(3 * i)];
  p[2] = 0.5f - static_cast<float>(rnd()) * -1.5259255e-05f;  // 0x005d05ac, 0x005d05a8
  float v = -1.0f - static_cast<float>(rnd()) * -4.5777764e-05f;  // 0x005d05b4, 0x005d05b0
  p[0] = v;
  if (v < -p[2]) {
    p[0] = v - -1.5f;  // 0x005d05b8
    p[2] = 1.5f - p[2];  // 0x005d05bc
  }
  v = -1.0f - static_cast<float>(rnd()) * -4.5777764e-05f;
  p[1] = v;
  if (v < -p[2]) {
    p[1] = v - -1.5f;
    p[2] = 1.5f - p[2];
  }
  q[0] = p[0], q[1] = p[1], q[2] = p[2];
}

// Mech3.exe 1.2 @ 0x005a5360. On the manager, with its 2D target set the
// software finish is pointed at it (0x00558290), then the particles drawn for
// each of its two rects; the first is kept.
void Weather::draw() {
  Layer* o = owner;
  erase();
  if (!o) return draw_particles();
  auto* m = static_cast<WeatherLayer*>(o);
  const zvid::TextTarget& t = zvid::text_target();
  if (t.pixels) zrndr::set_finish_target(t.pixels, t.width, t.height, t.pitch);
  for (const Rect* r : m->rects)
    if (r) rect = r, draw_particles();
  rect = m->rects[0];
}

namespace {
// Mech3.exe 1.2 @ 0x005a5a60 (ECX the count, EDX the rect): every vertex
// inside it, edges included; no rect, all of them.
bool inside(int32_t n, const Rect* r) {
  if (!r || n < 1) return true;
  const zmod::TLVertex* v = zmod::clip2d().tl;
  for (int32_t i = 0; i < n; ++i) {
    if (v[i].sx < static_cast<float>(r->left) || static_cast<float>(r->right) < v[i].sx) return false;
    if (v[i].sy < static_cast<float>(r->top)) return false;
    if (static_cast<float>(r->bottom) < v[i].sy) return false;
  }
  return true;
}
}  // namespace

// Mech3.exe 1.2 @ 0x005a57d0. Direct3D only (the software path 0x0055a950 is
// not ported): the 2D lock let go, the ramp re-tinted when the colour changed
// (16 texels of the first row, alpha i x 255 / 16), then each streak as a
// quad its second edge a pixel along x (wider than tall) or y, white-textured, priority 1;
// those reaching outside the rect are skipped.
void Weather::draw_particles() {
  if (!hardware()) return;
  const bool locked = zvid::surfaces().back.locked != 0;  // 0x00576d70
  if (locked) zvid::unlock_3d();
  if (image && ramp_pixels[0] != color) {
    for (int32_t i = 0, k = 0; k < 0xff0; ++i, k += 0xff) {
      ramp_pixels[static_cast<size_t>(i)] = color;
      ramp_alpha[static_cast<size_t>(i)] = static_cast<uint8_t>(k / 16);
    }
    if (g_hooks.reload) g_hooks.reload(texture, image);
    else zvid::reload_texture(texture, image);  // [0x0072f374]
  }
  zvid::scene_begin();
  zmod::TLVertex* v = zmod::clip2d().tl;
  for (int32_t i = 0; i < count; ++i) {
    const WeatherStreak& s = streaks[static_cast<size_t>(i)];
    float dx, dy;  // the second edge's offset: along x when wider than tall
    if (s.h < s.w) dx = static_cast<float>(s.shown), dy = 0.0f;
    else dx = 0.0f, dy = static_cast<float>(s.shown);
    const float z = pos[cur][static_cast<size_t>(3 * i + 2)];
    v[3].sx = static_cast<float>(s.x), v[3].sy = static_cast<float>(s.y);
    v[2].sx = static_cast<float>(s.x) + dx, v[2].sy = static_cast<float>(s.y) + dy;
    v[1].sx = static_cast<float>(s.w + s.x) + dx, v[1].sy = static_cast<float>(s.h + s.y) + dy;
    v[0].sx = static_cast<float>(s.w + s.x), v[0].sy = static_cast<float>(s.h + s.y);
    v[3].tu = v[2].tu = s.u_tail;
    v[1].tu = v[0].tu = s.u_head;
    for (int k = 0; k < 4; ++k) v[k].sz = z, v[k].tv = 0.0f;  // 0x005d05c0
    if (!inside(4, rect)) continue;
    if (g_hooks.draw) g_hooks.draw(4, texture, 1, 1.0f, 0);
    else zvid::draw_textured_white(4, texture, 1, 1.0f, 0);
  }
  if (g_hooks.flush) g_hooks.flush();
  else zvid::flush_transparent();  // [0x0072f3e4]
  zvid::scene_end();
  if (locked) zvid::lock_3d();
}

// Mech3.exe 1.2 @ 0x005a60a0
Rain::Rain(int32_t particles) : Weather(particles) {
  x8c = 1;
  x90 = 20.0f;
  x94 = 400.0f;
}

// The start both updates share (0x005a6110 to 0x005a6404, 0x005a5b40 to
// 0x005a5e34): the time summed, the rect, and the motion this frame in
// camera space: gravity along `down`, the wind at its heading and the
// camera's move since the last frame against them, a tenth each. d is cut to
// length 1; the streak s is d made 0.1 long, only when |d|^2 > 0.01 (a short
// move is its own streak). FIX (bug): without a rect (the first frame, before
// the draw sets one) the exe reads its width and height unset; the screen's
// are used.
bool Weather::motion(float dt, WeatherClock& st, WeatherMotion& m) {
  st.time = st.time + dt;
  if (!camera) return false;
  int32_t width, height;
  if (rect) {
    width = rect->right - rect->left, height = rect->bottom - rect->top;
    m.left = rect->left, m.top = rect->top;
  } else {
    const Rect* r = screen_rect();
    m.left = r->left, m.top = r->top;
    width = r->right - r->left, height = r->bottom - r->top;
  }
  m.width = static_cast<float>(width), m.height = static_cast<float>(height);
  float place[3], angles[3];
  if (g_hooks.camera_place) g_hooks.camera_place(camera, place);
  else zcls::node_world_position(camera, place);
  if (g_hooks.camera_angles) g_hooks.camera_angles(camera, angles);
  else zcls::node_world_euler(camera, angles);
  zmth::Vec3 moved{place[0] - st.last_camera[0], place[1] - st.last_camera[1], place[2] - st.last_camera[2]};
  moved.x = moved.x * 0.1f, moved.y = moved.y * 0.1f, moved.z = moved.z * 0.1f;  // 0x005d05c4
  st.last_camera[0] = place[0], st.last_camera[1] = place[1], st.last_camera[2] = place[2];
  // FIX (B), behind per_second: the fall and the wind are a 30 fps frame's,
  // the camera's move a real frame's; scaled, the streaks keep their slant.
  const float steps = zfix::options().per_second ? zfix::frame_steps(dt) : 1.0f;
  const float k = gravity * -0.1f * steps;  // 0x005d05c8
  zmth::Vec3 fall{down[0] * k, down[1] * k, down[2] * k};
  const float wv = wind_vel * 0.1f * steps;
  zmth::Vec3 wind{static_cast<float>(static_cast<double>(wv) * std::sin(static_cast<double>(wind_dir))), 0.0f,
                  static_cast<float>(std::cos(static_cast<double>(wind_dir)) * static_cast<double>(wv))};
  // Into camera space: the camera's matrix (its angles and place), its view
  // (0x00520b00, the frame's view set again), the three turned by it.
  zmth::Mat34 mat;
  zmth::push(&mat);
  zmth::load_identity();
  const float one[3] = {1.0f, 1.0f, 1.0f};
  zmth::euler(angles, place, one);
  zmth::pop();
  zmth::set_view(mat);
  zmth::push(&mat);
  zmth::load_view();
  zmth::rotate_points(&moved, 1);
  zmth::rotate_points(&fall, 1);
  zmth::rotate_points(&wind, 1);
  zmth::pop();
  zmth::Vec3 d{fall.x + wind.x, fall.y + wind.y, fall.z + wind.z};
  d = {d.x - moved.x, d.y - moved.y, d.z - moved.z};
  if (d.x * d.x + d.y * d.y + d.z * d.z > 1.0f) zmth::normalize(d);  // 0x005d05cc
  zmth::Vec3 s = d;
  if (s.x * s.x + s.y * s.y + s.z * s.z > 0.01f) {  // 0x005d05d0
    zmth::normalize(s);
    s = {s.x * 0.1f, s.y * 0.1f, s.z * 0.1f};
  }
  m.d[0] = d.x, m.d[1] = d.y, m.d[2] = d.z;
  m.s[0] = s.x, m.s[1] = s.y, m.s[2] = s.z;
  return true;
}

// Both loops' body up to the reset: the next place = place + d, the tail =
// place + s; both ends projected into the rect as (k p + 0.5) x size with
// k = 1.5 - depth; u along the ramp is k x the end's alpha.
void Weather::project(int32_t i, const WeatherMotion& m, float& k1) {
  float* n = &pos[next][static_cast<size_t>(3 * i)];
  const float* c = &pos[cur][static_cast<size_t>(3 * i)];
  n[0] = c[0] + m.d[0], n[1] = c[1] + m.d[1], n[2] = c[2] + m.d[2];
  const float tail[3] = {c[0] + m.s[0], c[1] + m.s[1], c[2] + m.s[2]};
  k1 = 1.5f - c[2];  // 0x005d05bc
  const float k2 = 1.5f - tail[2];
  const float w = m.width, h = m.height;
  WeatherStreak& r = streaks[static_cast<size_t>(i)];
  r.x = crt::msvc_ftol((k2 * tail[0] - -0.5f) * w) + m.left;  // 0x005d05d4
  r.y = crt::msvc_ftol((k2 * tail[1] - -0.5f) * h) + m.top;
  r.w = crt::msvc_ftol((c[0] * k1 - -0.5f) * w) + m.left - r.x;
  r.h = crt::msvc_ftol((c[1] * k1 - -0.5f) * h) - r.y + m.top;
  r.color = color;
  r.u_tail = k2 * alpha_tail;
  r.u_head = k1 * alpha_head;
}

// Mech3.exe 1.2 @ 0x005a6110. Every particle's streak this frame, then a
// fresh place for it: the rain keeps nothing from one frame to the next.
void Rain::update(float dt) {
  if (flags & kHidden) return;
  WeatherMotion m;
  if (!motion(dt, g_state.rain, m)) return;
  for (int32_t i = 0; i < count; ++i) {
    float k1;
    project(i, m, k1);
    streaks[static_cast<size_t>(i)].shown = 1;
    reset(i);
  }
  Object2D::update(dt);
  std::swap(cur, next);
}

// Mech3.exe 1.2 @ 0x005a5ad0
Snow::Snow(int32_t particles) : Weather(particles) {
  x8c = 1;
  x90 = 20.0f;
  x94 = 400.0f;
}

// Mech3.exe 1.2 @ 0x005a5b40 (with its tail 0x005a5e10). As the rain, but a
// flake keeps its place, moved by d each frame, and starts again only once
// its next place leaves the view: off the sides (|x| or |y| past the depth)
// or the depth out of 0.5..1 (NaN included). A flake is 3.5 x k pixels thick
// (+0x54 is always 0), thicker the nearer.
void Snow::update(float dt) {
  if (flags & kHidden) return;
  WeatherMotion m;
  if (!motion(dt, g_state.snow, m)) return;
  for (int32_t i = 0; i < count; ++i) {
    float k1;
    project(i, m, k1);
    WeatherStreak& r = streaks[static_cast<size_t>(i)];
    r.shown = crt::msvc_ftol(static_cast<float>(x54 + 1) * k1 * 3.5f);  // 0x005d05d8
    const float* n = &pos[next][static_cast<size_t>(3 * i)];
    const float az = std::fabs(n[2]);
    if (std::fabs(n[1]) > az || std::fabs(n[0]) > az || n[2] > 1.0f || !(n[2] >= 0.5f)) reset(i);
  }
  Object2D::update(dt);
  std::swap(cur, next);
}

// Mech3.exe 1.2 @ 0x005a6790 -> 0x005a66b0
void weather_rect(int32_t i, const Rect* r) {
  if (i < 2) g_layer.rects[i] = r;
}

// Mech3.exe 1.2 @ 0x005a67c0 -> 0x005a6600: the layer's update (0x0059fd20);
// its +0x1ec (the timed texts' count) cleared, not ported here.
void weather_tick(float dt) { g_layer.update(dt); }

}  // namespace ztwod
