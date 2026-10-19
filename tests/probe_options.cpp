// Probes for test_app_options.py: the options' getters and setters (mech3/options).
#include <cstdint>
#include <cstring>

#include "mech3/options.h"
#include "probe_app.h"

namespace {
using probe_app::call;

struct RecordingOptionsHooks : mech3::OptionsHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return call(key, args);
  }
  zrdr::Node* reader_load(const char* name) override {
    call("0x00556740", {name});
    return nullptr;
  }
  void camera_get_fov(zgame::Handle cam, float* x, float* y) override {
    call("0x004f3ee0", {cam});
    *x = zgame::from_bits(call("@fovx", {}));
    *y = zgame::from_bits(call("@fovy", {}));
  }
};
RecordingOptionsHooks g_hooks;

}  // namespace

extern "C" {

// The options as a flat list of 32-bit words, in the order test_app_options.py
// reads the original's memory.
int32_t probe_options_words() { return 85; }

void probe_options_io(uint32_t* w, int32_t write) {
  mech3::Options& o = mech3::options();
  uint32_t* p = w;
  auto io = [&](auto& field) {
    static_assert(sizeof(field) == 4, "a 32-bit field");
    if (write) std::memcpy(&field, p, 4);
    else std::memcpy(p, &field, 4);
    ++p;
  };
  auto io8 = [&](uint8_t& field) {
    if (write) field = static_cast<uint8_t>(*p);
    else *p = field;
    ++p;
  };
  io(o.hw); io(o.hw_card_flag); io(o.hw_card_dev);
  for (int32_t* a : {o.texture_memory, o.gfx_flags, o.object_lod, o.effects_level, o.shadow_parts}) {
    io(a[0]);
    io(a[1]);
  }
  io(o.shadow); io(o.sound_lod); io(o.game_options); io(o.game_intensity); io(o.vmode);
  io(o.in_game_vmode); io(o.vstride);
  io(o.camera.unknown0); io(o.camera.unknown4); io(o.camera.camera);
  for (mech3::Rect* r : {&o.render, &o.display, &o.window}) {
    io(r->x); io(r->y); io(r->x_end); io(r->y_end); io(r->w); io(r->h);
    io(r->x_last); io(r->y_last); io(r->depth); io(r->target);
  }
  io(o.mouse_accel[0]); io(o.mouse_accel[1]); io(o.joy_accel[0]); io(o.joy_accel[1]);
  io(o.sound_volume); io(o.mouse_mode); io(o.cd_volume); io(o.pixel_double);
  io(o.intensity[0]); io(o.intensity[1]); io(o.intensity[2]);
  io8(o.intensity_flag[0]); io8(o.intensity_flag[1]);
  for (auto& e : o.axis_map)
    for (auto& x : e) io(x);
  io8(o.axis_map_ready); io8(o.cheated); io8(o.multiplayer);
}

// Runs one setter or getter by its original address with an int or float value
// (and a second int for the two-argument ones); returns a getter's result.
uint32_t probe_options_op(uint32_t addr, uint32_t value, uint32_t value2) {
  using namespace mech3;
  set_options_hooks(&g_hooks);
  const int32_t v = static_cast<int32_t>(value), v2 = static_cast<int32_t>(value2);
  const float f = zgame::from_bits(value);
  uint32_t r = 0;
  switch (addr) {
    case 0x40d4c0: select_hw(v); break;
    case 0x40d4d0: r = hw_card_flag(); break;
    case 0x40cbf0: set_hw_card_dev(v); break;
    case 0x40cc10: r = hw_card_dev(); break;
    case 0x40cc20: set_game_intensity(v); break;
    case 0x40cdf0: r = game_intensity(); break;
    case 0x40ce00: set_game_options(value); break;
    case 0x40ce10: set_camera_mode(v); break;
    case 0x40ce70: r = camera_mode(); break;
    case 0x40ce90: set_fast_menu(v); break;
    case 0x40cec0: r = fast_menu(); break;
    case 0x40ced0: set_invincible(v); break;
    case 0x40cf20: r = invincible(); break;
    case 0x40cf30: set_infinite_ammo(v); break;
    case 0x40cf80: r = infinite_ammo(); break;
    case 0x40cf90: set_heat(v); break;
    case 0x40cfc0: r = heat(); break;
    case 0x40cfd0: set_torso_auto_return(v); break;
    case 0x40d010: r = torso_auto_return(); break;
    case 0x40d020: set_throttle_const(v); break;
    case 0x40d060: r = throttle_const(); break;
    case 0x40d070: set_use_joystick(v); break;
    case 0x40d1b0: r = use_joystick(); break;
    case 0x40d1c0: set_mw2_jump_jets(v); break;
    case 0x40d1e0: r = mw2_jump_jets(); break;
    case 0x40d1f0: set_shadow_parts(v); break;
    case 0x40d220: r = shadow_parts(); break;
    case 0x40d240: set_shadow(v); break;
    case 0x40d280: r = shadow(); break;
    case 0x40d2a0: set_effects_level(v); break;
    case 0x40d2f0: r = effects_level(); break;
    case 0x40d310: set_object_lod(v); break;
    case 0x40d3b0: r = object_lod(); break;
    case 0x40d3d0: set_sound_lod(v); break;
    case 0x40d3e0: r = sound_lod(); break;
    case 0x40d3f0: set_texture_memory(v); break;
    case 0x40d410: r = texture_memory(); break;
    case 0x40d430: set_gfx_flags(v); break;
    case 0x40d480: r = gfx_flags(); break;
    case 0x40d4a0: set_pixel_double(v); break;
    case 0x40d4b0: r = pixel_double(); break;
    case 0x40d590: set_camera(value); break;
    case 0x40d5e0: r = camera(); break;
    case 0x40d600: set_render_size(v, v2); break;
    case 0x40d630: set_render_origin(v, v2); break;
    case 0x40d670: set_render_target(value); break;
    case 0x40d6b0: set_display_target(value); break;
    case 0x40d6e0: set_display_origin(v, v2); break;
    case 0x40d720: set_display_size(v, v2); break;
    case 0x40d760: r = display_w(); break;
    case 0x40d770: r = display_h(); break;
    case 0x40d780: set_display_depth(v); break;
    case 0x40d790: r = display_depth(); break;
    case 0x40d7a0: r = vstride(); break;
    case 0x40d7c0: r = window_h(); break;
    case 0x40d7d0: set_window_size(v, v2); break;
    case 0x40d7f0: set_window_origin(v, v2); break;
    case 0x40d810: set_vmode(v); break;
    case 0x40d9d0: r = vmode(); break;
    case 0x40da80: set_sound_volume(f); break;
    case 0x40daa0: r = zgame::bits(sound_volume()); break;
    case 0x40dab0: set_cd_volume(f); break;
    case 0x40dae0: r = zgame::bits(cd_volume()); break;
    case 0x40cb30: apply_all(); break;
    default: r = 0xdeadbeef;
  }
  set_options_hooks(nullptr);
  return r;
}

}  // extern "C"
