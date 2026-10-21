// Any screen size and 16:9 (docs/resolution.md section 5): the modes' sizes,
// the device's range and RESOLUTION's list, and the camera's horizontal angle,
// with zfix's any_resolution on and off (off: the original's).
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zutil/zfix.h"
#include "mech3/app_hooks.h"
#include "mech3/options.h"
#include "mech3/options_menu.h"

namespace {

struct FixOn {
  bool was = zfix::options().any_resolution;
  explicit FixOn(bool on) { zfix::options().any_resolution = on; }
  ~FixOn() { zfix::options().any_resolution = was; }
};

// A camera with plain angles in place of the engine's: what 0x004f3ee0 reads
// and 0x004f3d90 writes.
struct FovHooks : mech3::OptionsHooks {
  float h = 1.3962634f, v = 1.0471976f;
  int sets = 0;
  void camera_get_fov(mech3::Handle, float* x, float* y) override { *x = h, *y = v; }
  void camera_set_fov(mech3::Handle, float x, float y) override { h = x, v = y, ++sets; }
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
};

float camera_h_at(int32_t w, int32_t h, float vfov = 1.0471976f) {
  FovHooks hooks;
  hooks.v = vfov;
  mech3::OptionsHooks& old = mech3::options_hooks();
  mech3::set_options_hooks(&hooks);
  mech3::set_render_size(w, h);
  mech3::set_camera(1);
  mech3::set_camera(0);
  mech3::set_options_hooks(&old);
  return hooks.v == vfov ? hooks.h : -1.0f;
}

// The original's 0x0040d590, as test_app_options.py checks it.
float linear(int32_t w, int32_t h, float vfov) {
  return static_cast<float>(static_cast<double>(w) * vfov / static_cast<double>(h));
}

// Horizontal over vertical scale of the projection (render.md section 0).
double squeeze(float hfov, float vfov, int32_t w, int32_t h) {
  return (w / std::tan(hfov * 0.5)) / (h / std::tan(vfov * 0.5));
}

struct RangeEngine : mech3::Engine {
  int32_t total = 4 << 20, used = 0;
  int32_t video_memory_total() override { return total; }
  int32_t video_memory_used() override { return used; }
  const char* message(int32_t id) override {
    static std::string s;
    s = "msg" + std::to_string(id);
    return s.c_str();
  }
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
};

}  // namespace

TEST(resolution_original_modes_keep_their_sizes) {
  for (bool fix : {false, true}) {
    FixOn f(fix);
    const int32_t sizes[][3] = {{5, 640, 480},   {6, 800, 600},   {7, 1024, 768},
                                {8, 1152, 864},  {9, 1280, 1024}, {10, 1600, 1200}};
    for (const auto& s : sizes) {
      mech3::set_vmode(s[0]);
      EXPECT(mech3::render().w == s[1] && mech3::render().h == s[2]);
      EXPECT(mech3::display().w == s[1] && mech3::display().h == s[2]);
      EXPECT(mech3::window().w == s[1] && mech3::window().h == s[2]);
    }
    mech3::set_vmode(3);  // 320x240 doubled
    EXPECT(mech3::render().w == 320 && mech3::display().w == 640);
    mech3::set_vmode(5);
    mech3::set_vmode(4);  // keeps the sizes
    EXPECT(mech3::display().w == 640 && mech3::display().h == 480);
  }
}

TEST(resolution_new_modes_only_with_the_fix) {
  {
    FixOn f(false);
    mech3::set_vmode(5);
    for (int32_t m = 11; m <= mech3::kLastVMode; ++m) {
      mech3::set_vmode(m);  // the exe: an unknown mode keeps the sizes
      EXPECT(mech3::display().w == 640 && mech3::display().h == 480);
    }
  }
  FixOn f(true);
  const int32_t sizes[][3] = {{11, 1280, 720}, {12, 1600, 900}, {13, 1920, 1080}, {14, 2560, 1440}, {15, 3840, 2160}};
  for (const auto& s : sizes) {
    mech3::set_vmode(s[0]);
    EXPECT(mech3::render().w == s[1] && mech3::render().h == s[2]);
    EXPECT(mech3::display().w == s[1] && mech3::display().h == s[2]);
  }
  mech3::Options& o = mech3::options();
  const int32_t keep_w = o.screen_w, keep_h = o.screen_h;
  o.screen_w = 2560, o.screen_h = 1080;  // 21:9
  mech3::set_vmode(mech3::kCustomVMode);
  EXPECT(mech3::display().w == 2560 && mech3::display().h == 1080);
  o.screen_w = 10000, o.screen_h = 100;  // clamped to 640x480 .. the cap
  mech3::set_vmode(mech3::kCustomVMode);
  EXPECT(mech3::display().w == mech3::kMaxScreen[0] && mech3::display().h == 480);
  o.screen_w = keep_w, o.screen_h = keep_h;
  mech3::set_vmode(5);
}

TEST(resolution_range_and_list) {
  RangeEngine e;
  int32_t lo = 0, hi = 0;
  {
    FixOn f(false);
    e.video_mode_range(0, &lo, &hi);  // 4 MB free: 1024x768 cut to 800x600
    EXPECT(lo == 5 && hi == 6);
    e.video_mode_range(-1, &lo, &hi);
    EXPECT(lo == 3 && hi == 5);
  }
  FixOn f(true);
  e.video_mode_range(0, &lo, &hi);
  EXPECT(lo == 5 && hi == mech3::kLastVMode);
  e.video_mode_range(-1, &lo, &hi);  // software stays as it was
  EXPECT(lo == 3 && hi == 5);

  mech3::ResolutionCycle list(e);
  list.rebuild(0);
  std::vector<std::string> labels;
  for (auto* it : list.items) labels.push_back(it->text ? it->text : "");
  EXPECT(list.count() == 12);  // 5..16
  EXPECT(list.count() == 12 && labels[3] == "msg4058" && labels[5] == "msg4060");  // 8..10 by name
  EXPECT(list.count() == 12 && labels[8] == "1920x1080" && list.value_at(8) == 13);
  EXPECT(list.count() == 12 && list.value_at(11) == mech3::kCustomVMode);
  list.rebuild(-1);
  EXPECT(list.count() == 2);  // software: 3 and 5
}

TEST(resolution_camera_4_3_is_the_original) {
  for (bool fix : {false, true}) {
    FixOn f(fix);
    const int32_t shapes[][2] = {{640, 480}, {800, 600}, {1024, 768}, {1600, 1200}, {320, 240}};
    for (const auto& s : shapes)
      for (float v : {1.0471976f, 0.7f, 1.2f}) EXPECT(camera_h_at(s[0], s[1], v) == linear(s[0], s[1], v));
  }
}

TEST(resolution_camera_wide_keeps_the_4_3_look) {
  const float v = 1.0471976f;
  const double look = squeeze(linear(640, 480, v), v, 640, 480);  // about 0.9175
  EXPECT(std::fabs(look - 0.9175) < 1e-3);
  {
    FixOn f(false);  // the original's linear rule squeezes 16:9 further
    EXPECT(camera_h_at(1920, 1080) == linear(1920, 1080, v));
    EXPECT(squeeze(camera_h_at(1920, 1080), v, 1920, 1080) < 0.8);
  }
  FixOn f(true);
  const int32_t shapes[][2] = {{1920, 1080}, {1280, 720}, {2560, 1080}, {1280, 1024}, {3840, 2160}};
  for (const auto& s : shapes) {
    const float hf = camera_h_at(s[0], s[1]);
    EXPECT(std::fabs(squeeze(hf, v, s[0], s[1]) - look) < 1e-5);
  }
  EXPECT(std::fabs(camera_h_at(1920, 1080) * 57.29578 - 96.4) < 0.1);  // degrees across
  mech3::set_render_size(640, 480);
}

// The wired hooks' targets: 0x004f3ee0 reads what 0x004f3d90 set.
TEST(resolution_camera_angles_on_a_node) {
  zcls::Node node{};
  zcls::Camera cam{};
  node.data = &cam;
  node.put<int32_t>(0x34, 1);
  cam.put(0xd8, 1.0f), cam.put(0xdc, 1.0f);
  EXPECT(zcls::camera_set_fov_angles(&node, 1.6825f, 1.0471976f) == 0);
  float h = 0, v = 0;
  EXPECT(zcls::camera_get_fov_angles(&node, &h, &v) == 0);
  EXPECT(h == 1.6825f && v == 1.0471976f);
  EXPECT(cam.get<float>(0xe8) == 1.6825f);  // the angles setter does not cap at 80 degrees
  node.put<int32_t>(0x34, 5);
  EXPECT(zcls::camera_get_fov_angles(&node, &h, &v) == 3);  // not a camera
}
