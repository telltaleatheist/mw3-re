#pragma once
// The weather: rain and snow as screen-space particles (docs/weather.md). A
// zTwoD object on the weather manager [0x00736328] (the mission's world
// parameters add it, world-params.md section 7), updated and drawn by the
// Direct3D frame through weather_tick. Original file unknown: between
// zTwoD\twod_wintext.cpp and zEntity\zent_init.cpp (0x005a5310..0x005a6860).
#include <cstdint>
#include <vector>

#include "gamez/ztwod/twod.h"

namespace zcls { struct Node; }
namespace zimg { struct Texture; }
namespace zvid { struct D3DTexture; }

namespace ztwod {

// The frame time summed by an update and the camera's last place: the
// rain's [0x0073653c] / [0x00736528], the snow's [0x00736538] / [0x00736518].
struct WeatherClock {
  float time = 0.0f;
  float last_camera[3] = {};
};

// What both updates work out before their particle loops (the exe has the
// code twice, 0x005a5b40..0x005a5e34 and 0x005a6110..0x005a6404).
struct WeatherMotion {
  int32_t left = 0, top = 0;
  float width = 0.0f, height = 0.0f;
  float d[3] = {};  // a place's move this frame
  float s[3] = {};  // the streak, tail to head
};

// One particle's streak on the screen (0x20 bytes, +0x38's array).
struct WeatherStreak {
  int32_t x = -1, y = -1, w = -1, h = -1;  // +0x00  from the tail to the head
  uint16_t color = 0;                      // +0x10
  float u_tail = 0, u_head = 0;            // +0x14, +0x18  along the ramp texture
  int32_t shown = 0;                       // +0x1c  1 once set (the draw's 1-pixel offset)
};

// The base (vtable 0x005d0528, 0x98 bytes).
class Weather : public Object2D {
 public:
  explicit Weather(int32_t particles);  // 0x005a5470
  ~Weather() override;                  // 0x005a5640
  void draw() override;                 // +0x04  0x005a5360: the particles once per manager rect
  virtual void draw_particles();        // +0x7c  0x005a57d0
  void reset(int32_t i);                // 0x005a56e0: a fresh random place, both buffers
  // The shared start of the updates: false without a camera.
  bool motion(float dt, WeatherClock& clock, WeatherMotion& out);
  // A particle's streak from its current place (the head) and the tail.
  void project(int32_t i, const WeatherMotion& m, float& k_head);

  const Rect* rect = nullptr;            // +0x34  the rect drawn into (the manager's)
  std::vector<WeatherStreak> streaks;    // +0x38
  int32_t capacity = 0;                  // +0x3c
  int32_t count = 0;                     // +0x40
  uint16_t color = 0x7fff;               // +0x44  display-packed
  float alpha_tail = 1.0f;               // +0x48  alpha_gradient [0]
  float alpha_head = 0.05f;              // +0x4c  alpha_gradient [1]
  zcls::Node* camera = nullptr;          // +0x50  the player view's camera node
  int32_t x54 = 0;                       // +0x54
  std::vector<float> pos[2];             // +0x58, +0x5c  x y z each, in screen space
  int32_t cur = 0, next = 1;             // +0x60, +0x64  which buffer is which
  float wind_dir = 0.0f;                 // +0x68  radians
  float wind_vel = 1.0f;                 // +0x6c
  float gravity = 1.0f;                  // +0x70
  float down[3] = {0.0f, 1.0f, 0.0f};    // +0x74
  const char* texture_name = nullptr;    // +0x80  "SnowFX"
  zimg::Texture* image = nullptr;        // +0x84  16 x 8, only the first row filled
  zvid::D3DTexture* texture = nullptr;   // +0x88
  int32_t x8c = 1;                       // +0x8c  (rain and snow: 1)
  float x90 = 20.0f, x94 = 400.0f;       // +0x90, +0x94

  std::vector<uint16_t> ramp_pixels;     // the image's planes (FIX (A): zeroed)
  std::vector<uint8_t> ramp_alpha;
};

// Rain (vtable 0x005d0660): 0x005a60a0.
class Rain : public Weather {
 public:
  explicit Rain(int32_t particles);  // 0x005a60a0
  void update(float dt) override;    // +0x24  0x005a6110
};

// Snow (vtable 0x005d05e0): 0x005a5ad0.
class Snow : public Weather {
 public:
  explicit Snow(int32_t particles);  // 0x005a5ad0
  void update(float dt) override;    // +0x24  0x005a5b40 (0x005a5e10 is its tail)
};

// The weather manager [0x00736328]: a Layer with two rects (+0x10, +0x14),
// set each frame (0x005a6790), and the 2D text target (+0x18, zvid's).
struct WeatherLayer : Layer {
  WeatherLayer() { set_active(1); }  // 0x005a4590's vt+0x04(1), at static init
  const Rect* rects[2] = {};
};
WeatherLayer& weather_layer();
void weather_rect(int32_t i, const Rect* r);  // 0x005a6790 -> 0x005a66b0: i < 2 only
void weather_tick(float dt);                  // 0x005a67c0 -> 0x005a6600: the layer's update

// What the update asks of the engine. Unset: zClass's (the camera node's
// world place and angles, 0x004ed530 / 0x004ed5d0) and the display's rect
// (0x00579880).
struct WeatherHooks {
  int32_t (*rand)() = nullptr;                       // the CRT's rand ([0x005bb668])
  void (*camera_place)(zcls::Node*, float out[3]) = nullptr;
  void (*camera_angles)(zcls::Node*, float out[3]) = nullptr;
  const Rect* (*screen_rect)() = nullptr;            // 0x00579880
  // The draw's device calls (unset: zvid's): [0x0072f3bc] = 0x0057caa0,
  // [0x0072f374] = 0x0057baa0, [0x0072f3e4] = 0x0057e110.
  void (*draw)(int32_t count, const zvid::D3DTexture*, int32_t priority, float opacity, int32_t overlay) = nullptr;
  void (*reload)(zvid::D3DTexture*, const zimg::Texture*) = nullptr;
  void (*flush)() = nullptr;
};
WeatherHooks& weather_hooks();
struct WeatherState {
  WeatherClock rain, snow;
};
WeatherState& weather_state();

}  // namespace ztwod
