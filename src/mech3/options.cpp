#include "mech3/options.h"

#include <cstring>

#include "gamez/zreader/zreader.h"

namespace mech3 {

namespace {
Options g_options;
OptionsHooks g_default_hooks;
OptionsHooks* g_hooks = &g_default_hooks;

uint32_t& word() { return g_options.game_options; }
void set_bit(uint32_t bit, bool on) { word() = on ? (word() | bit) : (word() & ~bit); }

// 0x00556fe0: a node's number as a float; a list stands for its first item.
bool node_float(const zrdr::Node* n, float* out) {
  if (!n) return false;
  if (n->type == zrdr::List && n->list[0].i > 1) n = &n->list[1];
  if (n->type == zrdr::Int) {
    *out = static_cast<float>(n->i);
    return true;
  }
  if (n->type == zrdr::Float) {
    *out = n->f;
    return true;
  }
  return false;
}
}  // namespace

Options& options() { return g_options; }
void set_options_hooks(OptionsHooks* hooks) { g_hooks = hooks ? hooks : &g_default_hooks; }
OptionsHooks& options_hooks() { return *g_hooks; }

// Mech3.exe 1.2 @ 0x0040d4c0
void select_hw(int32_t hw) {
  g_options.hw_card_flag = hw;
  g_options.hw = hw;
}
// Mech3.exe 1.2 @ 0x0040d4d0
int32_t hw_card_flag() { return g_options.hw_card_flag; }

// Mech3.exe 1.2 @ 0x0040cbf0
void set_hw_card_dev(int32_t dev) {
  g_options.hw_card_dev = dev;
  select_hw(dev >= 0 ? 1 : 0);
}
// Mech3.exe 1.2 @ 0x0040cc10
int32_t hw_card_dev() { return g_options.hw_card_dev; }

zrdr::Node* OptionsHooks::reader_load(const char* name) { return zrdr::load(name); }

// Mech3.exe 1.2 @ 0x0040cc20. The level is stored as given and clamped only to
// pick the preset; a complete (name f f f b b) entry in intensity.zrd overrides it.
void set_game_intensity(int32_t level) {
  g_options.game_intensity = level;
  struct Preset {
    const char* name;
    float f[3];
    int32_t b[2];
  };
  static const Preset kPresets[3] = {{"low", {0.5f, 1.2f, 0.7f}, {1, 1}},
                                     {"medium", {1.0f, 1.0f, 1.0f}, {0, 0}},
                                     {"high", {1.2f, 0.6f, 1.3f}, {0, 0}}};
  if (level < 0) level = 0;
  if (level > 2) level = 2;
  Preset cur = kPresets[level];
  if (zrdr::Node* root = g_hooks->reader_load("intensity.zrd")) {
    if (zrdr::Node* n = zrdr::find(root, cur.name)) {
      Preset tmp = kPresets[level];
      if (n->type == zrdr::List && n->list[0].i == 6 && node_float(&n->list[1], &tmp.f[0]) &&
          node_float(&n->list[2], &tmp.f[1]) && node_float(&n->list[3], &tmp.f[2]) &&
          zrdr::get_bool_int(&n->list[4], &tmp.b[0]) && zrdr::get_bool_int(&n->list[5], &tmp.b[1]))
        cur = tmp;
    }
    zrdr::free_node(root);
  }
  g_options.intensity[0] = cur.f[0];
  g_options.intensity[2] = cur.f[2];
  g_options.intensity[1] = cur.f[1];
  g_options.intensity_flag[0] = cur.b[0] != 0;
  g_options.intensity_flag[1] = cur.b[1] != 0;
}
// Mech3.exe 1.2 @ 0x0040cdf0
int32_t game_intensity() { return g_options.game_intensity; }

// Mech3.exe 1.2 @ 0x0040ce00
void set_game_options(uint32_t bits) { word() = bits; }

// Mech3.exe 1.2 @ 0x0040ce10: 0 and 1 use bit 0, 2 is bit 8; other values only
// reach the view.
void set_camera_mode(int32_t mode) {
  if (mode == 0) {
    word() &= ~kCamera1;
    word() &= ~kCamera2;
  } else if (mode == 1) {
    word() |= kCamera1;
  } else if (mode == 2) {
    word() &= ~kCamera1;
    word() |= kCamera2;
  }
  if (Handle view = g_hooks->player_view()) g_hooks->view_set_camera_mode(view, mode);
}
// Mech3.exe 1.2 @ 0x0040ce70
int32_t camera_mode() {
  if (word() & kCamera1) return 1;
  return static_cast<int32_t>((word() >> 7) & 2);
}

// Mech3.exe 1.2 @ 0x0040ce90; it reads the old value (0x0040cec0) and ignores it.
void set_fast_menu(int32_t on) {
  fast_menu();
  set_bit(kFastMenu, on != 0);
}
// Mech3.exe 1.2 @ 0x0040cec0
int32_t fast_menu() { return static_cast<int32_t>((word() >> 9) & 1); }

// Mech3.exe 1.2 @ 0x0040ced0
void set_invincible(int32_t on) {
  set_bit(kInvincible, on != 0);
  if (on) g_options.cheated = 1;
  if (Handle mech = g_hooks->player_mech())
    g_hooks->mech_set_invincible(mech, on != 0 && !g_options.multiplayer ? 1 : 0);
}
// Mech3.exe 1.2 @ 0x0040cf20
int32_t invincible() { return static_cast<int32_t>((word() >> 1) & 1); }

// Mech3.exe 1.2 @ 0x0040cf30
void set_infinite_ammo(int32_t on) {
  set_bit(kInfiniteAmmo, on != 0);
  if (on) g_options.cheated = 1;
  if (g_options.multiplayer) return;
  if (Handle mech = g_hooks->player_mech())
    g_hooks->weapons_set_infinite_ammo(g_hooks->mech_weapons(mech), on == 1 ? 1 : 0);
}
// Mech3.exe 1.2 @ 0x0040cf80
int32_t infinite_ammo() { return static_cast<int32_t>((word() >> 2) & 1); }

// Mech3.exe 1.2 @ 0x0040cf90: turning heat off is the cheat.
void set_heat(int32_t on) {
  set_bit(kHeat, on != 0);
  if (!on) g_options.cheated = 1;
}
// Mech3.exe 1.2 @ 0x0040cfc0
int32_t heat() { return static_cast<int32_t>((word() >> 3) & 1); }

// Mech3.exe 1.2 @ 0x0040cfd0
void set_torso_auto_return(int32_t on) {
  set_bit(kTorsoAutoReturn, on != 0);
  if (Handle mech = g_hooks->player_mech())
    g_hooks->mech_set_torso_auto_return(mech, on == 1 ? 1 : 0);
}
// Mech3.exe 1.2 @ 0x0040d010
int32_t torso_auto_return() { return static_cast<int32_t>((word() >> 4) & 1); }

// Mech3.exe 1.2 @ 0x0040d020: the mech's flag is the inverse (throttle decays).
void set_throttle_const(int32_t on) {
  set_bit(kThrottleConst, on != 0);
  if (Handle mech = g_hooks->player_mech()) g_hooks->mech_set_throttle_decay(mech, on == 0 ? 1 : 0);
}
// Mech3.exe 1.2 @ 0x0040d060
int32_t throttle_const() { return static_cast<int32_t>((word() >> 5) & 1); }

namespace {
// 0x0040daf0 / 0x0040db30 / 0x0040db10 on the axis table at 0x006040d0; the
// table's +0x40 byte (0x00604110) is cleared by each write.
void axis3(int i, uint32_t a, uint32_t b, uint32_t c) {
  g_options.axis_map[i][0] = a;
  g_options.axis_map[i][1] = b;
  g_options.axis_map[i][2] = c;
}
void axis4(int i, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
  g_options.axis_map[i][0] = a;
  g_options.axis_map[i][1] = b;
  g_options.axis_map[i][2] = c;
  g_options.axis_map[i][3] = d;
  g_options.axis_map_ready = 0;
}
void axis1(int i, uint32_t a) {
  g_options.axis_map[i][0] = a;
  g_options.axis_map_ready = 0;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0040d070: when the axis table is ready, switching rewrites it
// for joystick or mouse, then the mech's control mode follows (6 joystick, 3 not).
void set_use_joystick(int32_t on) {
  Options& o = g_options;
  if (on) {
    word() |= kUseJoystick;
    if (o.axis_map_ready) {
      axis3(0, 3, zgame::bits(-0.5f), zgame::bits(0.5f));
      o.axis_map[0][3] = zgame::bits(0.05f);
      o.axis_map_ready = 0;
      axis3(1, 6, zgame::bits(-1.0f), 0);
      o.axis_map[1][3] = 0;
      o.axis_map_ready = 0;
      axis4(2, 2, zgame::bits(1.0f), 0, 0);
      axis4(3, 1, zgame::bits(-1.0f), 0, 0);
      o.axis_map_ready = 1;
    }
  } else {
    word() &= ~kUseJoystick;
    if (o.axis_map_ready) {
      axis3(2, 10, zgame::bits(1.0f), 0);
      o.axis_map[2][3] = 0;
      o.axis_map_ready = 0;
      axis3(1, 9, zgame::bits(1.0f), 0);
      o.axis_map[1][3] = 0;
      o.axis_map_ready = 0;
      axis1(0, 0);
      axis1(3, 0);
      o.axis_map_ready = 1;
    }
  }
  if (Handle mech = g_hooks->player_mech())
    g_hooks->mech_set_jump_jet_mode(mech, use_joystick() == 1 ? 6 : 3);
}
// Mech3.exe 1.2 @ 0x0040d1b0
int32_t use_joystick() { return static_cast<int32_t>((word() >> 6) & 1); }

// Mech3.exe 1.2 @ 0x0040d1c0
void set_mw2_jump_jets(int32_t on) { set_bit(kMw2JumpJets, on != 0); }
// Mech3.exe 1.2 @ 0x0040d1e0
int32_t mw2_jump_jets() { return static_cast<int32_t>((word() >> 7) & 1); }

// Mech3.exe 1.2 @ 0x0040d1f0
void set_shadow_parts(int32_t v) {
  g_options.shadow_parts[g_options.hw ? 1 : 0] = v;
  g_hooks->shadows_set(v != 0 ? 1 : 0);
}
// Mech3.exe 1.2 @ 0x0040d220
int32_t shadow_parts() { return g_options.shadow_parts[g_options.hw ? 1 : 0]; }

// Mech3.exe 1.2 @ 0x0040d240: hardware only; forced off on a 3D card without
// capability 3.
void set_shadow(int32_t v) {
  if (!g_options.hw) return;
  g_options.shadow = v;
  if (g_hooks->video_mode() == 0) return;
  if (g_hooks->video_is_3d() != 0 && g_hooks->video_has_cap(3) == 0) v = 0;
  g_hooks->model_set_global_light(v);
}
// Mech3.exe 1.2 @ 0x0040d280
int32_t shadow() { return g_options.hw ? g_options.shadow : 0; }

// Mech3.exe 1.2 @ 0x0040d2a0: the effects module counts the other way.
void set_effects_level(int32_t v) {
  g_options.effects_level[g_options.hw ? 1 : 0] = v;
  if (v == 0) g_hooks->effects_set_level(2);
  else if (v == 1) g_hooks->effects_set_level(1);
  else if (v == 2) g_hooks->effects_set_level(0);
}
// Mech3.exe 1.2 @ 0x0040d2f0
int32_t effects_level() { return g_options.effects_level[g_options.hw ? 1 : 0]; }

// Mech3.exe 1.2 @ 0x0040d310: the camera's and the mechs' level-of-detail scales.
void set_object_lod(int32_t v) {
  float cam_scale = 1.0f, mech_scale = 1.0f;
  const Handle cam = camera();
  g_options.object_lod[g_options.hw ? 1 : 0] = v;
  if (v == 0) {
    cam_scale = 1.0f;
    mech_scale = 1.0f;
  } else if (v == 1) {
    cam_scale = 0.75f;
    mech_scale = 0.75f;
  } else if (v == 2) {
    cam_scale = 0.4f;
    mech_scale = 0.0f;
  }
  if (cam) g_hooks->camera_set_lod_scale(cam, cam_scale);
  g_hooks->mechs_set_lod_scale(mech_scale);
}
// Mech3.exe 1.2 @ 0x0040d3b0
int32_t object_lod() { return g_options.object_lod[g_options.hw ? 1 : 0]; }

// Mech3.exe 1.2 @ 0x0040d3d0 / 0x0040d3e0
void set_sound_lod(int32_t v) { g_options.sound_lod = v; }
int32_t sound_lod() { return g_options.sound_lod; }

// Mech3.exe 1.2 @ 0x0040d3f0 / 0x0040d410
void set_texture_memory(int32_t v) { g_options.texture_memory[g_options.hw ? 1 : 0] = v; }
int32_t texture_memory() { return g_options.texture_memory[g_options.hw ? 1 : 0]; }

// Mech3.exe 1.2 @ 0x0040d430: bit 4 turns the world's "sunlight" node on.
void set_gfx_flags(int32_t v) {
  g_options.gfx_flags[g_options.hw ? 1 : 0] = v;
  if (Handle sun = g_hooks->find_node(6, "sunlight")) g_hooks->node_set_active(sun, (v & 0x10) ? 1 : 0);
}
// Mech3.exe 1.2 @ 0x0040d480
int32_t gfx_flags() { return g_options.gfx_flags[g_options.hw ? 1 : 0]; }

// Mech3.exe 1.2 @ 0x0040d4a0 / 0x0040d4b0
void set_pixel_double(int32_t on) { g_options.pixel_double = on; }
int32_t pixel_double() { return g_options.pixel_double; }

// Mech3.exe 1.2 @ 0x0040d4e0
void rect_set_origin(Rect& r, int32_t x, int32_t y) {
  r.x = x;
  r.y = y;
  r.x_end = r.w + x;
  r.y_end = r.h + y;
  r.x_last = r.x_end - 1;
  r.y_last = r.y_end - 1;
}
// Mech3.exe 1.2 @ 0x0040d510
void rect_set_size(Rect& r, int32_t w, int32_t h) {
  r.w = w;
  r.h = h;
  r.x_end = r.x + w;
  r.y_end = r.y + h;
  r.x_last = r.x_end - 1;
  r.y_last = r.y_end - 1;
}

// Mech3.exe 1.2 @ 0x0040d590: the horizontal field of view follows the vertical
// one and the render size's aspect.
void set_camera(Handle cam) {
  g_options.camera.camera = cam;
  if (!cam) return;
  float fx = 0, fy = 0;
  g_hooks->camera_get_fov(cam, &fx, &fy);
  const Rect& r = g_options.render;
  const float x = static_cast<float>(static_cast<double>(r.w) * fy / static_cast<double>(r.h));
  g_hooks->camera_set_fov(g_options.camera.camera, x, fy);
}
// Mech3.exe 1.2 @ 0x0040d5e0
Handle camera() { return g_options.camera.camera; }

// Mech3.exe 1.2 @ 0x0040d600
void set_render_size(int32_t w, int32_t h) {
  Rect& r = g_options.render;
  rect_set_size(r, w, h);
  if (r.target) g_hooks->window_set_size(r.target, r.w, r.h);
}
// Mech3.exe 1.2 @ 0x0040d630
void set_render_origin(int32_t x, int32_t y) {
  Rect& r = g_options.render;
  rect_set_origin(r, x, y);
  if (r.target) {
    g_hooks->window_set_size(r.target, r.w, r.h);
    g_hooks->window_set_origin(r.target, r.x, r.y);
  }
}
// Mech3.exe 1.2 @ 0x0040d670
void set_render_target(Handle window) {
  Rect& r = g_options.render;
  r.target = window;
  if (window) {
    g_hooks->window_set_size(r.target, r.w, r.h);
    g_hooks->window_set_origin(r.target, r.x, r.y);
  }
}
// Mech3.exe 1.2 @ 0x0040d6a0
Rect& render() { return g_options.render; }

// Mech3.exe 1.2 @ 0x0040d6b0
void set_display_target(Handle d) {
  Rect& r = g_options.display;
  r.target = d;
  if (d) {
    g_hooks->display_set_size(r.target, r.w, r.h);
    g_hooks->display_set_origin(r.target, r.x, r.y);
  }
}
// Mech3.exe 1.2 @ 0x0040d6e0
void set_display_origin(int32_t x, int32_t y) {
  Rect& r = g_options.display;
  rect_set_origin(r, x, y);
  if (r.target) {
    g_hooks->display_set_size(r.target, r.w, r.h);
    g_hooks->display_set_origin(r.target, r.x, r.y);
  }
}
// Mech3.exe 1.2 @ 0x0040d720
void set_display_size(int32_t w, int32_t h) {
  Rect& r = g_options.display;
  rect_set_size(r, w, h);
  if (r.target) g_hooks->display_set_size(r.target, r.w, r.h);
}
// Mech3.exe 1.2 @ 0x0040d750 .. 0x0040d790
Rect& display() { return g_options.display; }
int32_t display_w() { return g_options.display.w; }
int32_t display_h() { return g_options.display.h; }
void set_display_depth(int32_t bpp) { g_options.display.depth = bpp; }
int32_t display_depth() { return g_options.display.depth; }
// Mech3.exe 1.2 @ 0x0040d7a0 .. 0x0040d7f0
int32_t vstride() { return g_options.vstride; }
Rect& window() { return g_options.window; }
int32_t window_h() { return g_options.window.h; }
void set_window_size(int32_t w, int32_t h) { rect_set_size(g_options.window, w, h); }
void set_window_origin(int32_t x, int32_t y) { rect_set_origin(g_options.window, x, y); }

// Mech3.exe 1.2 @ 0x0040d810: the screen modes. Mode 3 draws 320x240 and doubles
// it to 640x480; mode 4 keeps the sizes it had; 5..10 are 640x480 to 1600x1200.
void set_vmode(int32_t mode) {
  g_options.vmode = mode;
  set_render_origin(0, 0);
  set_window_origin(0, 0);
  set_display_origin(0, 0);
  set_display_depth(16);
  static const int32_t kSizes[][2] = {{640, 480},  {800, 600},   {1024, 768},
                                      {1152, 864}, {1280, 1024}, {1600, 1200}};
  if (mode == 3) {
    set_render_size(320, 240);
    set_window_size(640, 480);
    set_display_size(640, 480);
    set_pixel_double(1);
  } else if (mode >= 5 && mode <= 10) {
    const int32_t w = kSizes[mode - 5][0], h = kSizes[mode - 5][1];
    set_render_size(w, h);
    set_window_size(w, h);
    set_display_size(w, h);
    set_pixel_double(0);
  }
  const int32_t h = display_h();
  g_hooks->input_set_screen(display_w(), h);
}
// Mech3.exe 1.2 @ 0x0040d9d0
int32_t vmode() { return g_options.vmode; }

// Mech3.exe 1.2 @ 0x0040da80
void set_sound_volume(float v) {
  g_options.sound_volume = v;
  g_hooks->sound_set_volume(v);
}
// Mech3.exe 1.2 @ 0x0040daa0
float sound_volume() { return g_options.sound_volume; }

// Mech3.exe 1.2 @ 0x0040dab0: 0..1 to the 16-bit volume of both channels.
void set_cd_volume(float v) {
  const int32_t n = static_cast<int32_t>(static_cast<double>(v) * 65535.0);  // x87, truncated
  g_options.cd_volume = v;
  g_hooks->music_set_volume(n, n);
}
// Mech3.exe 1.2 @ 0x0040dae0
float cd_volume() { return g_options.cd_volume; }

// Mech3.exe 1.2 @ 0x0040cb30
void apply_all() {
  set_camera_mode(camera_mode());
  set_invincible(invincible());
  set_infinite_ammo(infinite_ammo());
  set_torso_auto_return(torso_auto_return());
  set_throttle_const(throttle_const());
  set_shadow_parts(shadow_parts());
  set_effects_level(effects_level());
  set_object_lod(object_lod());
  set_gfx_flags(gfx_flags());
  set_vmode(vmode());
  set_use_joystick(use_joystick());
  set_game_intensity(game_intensity());
  set_shadow(shadow());
  set_sound_volume(sound_volume());
  set_cd_volume(cd_volume());
}

}  // namespace mech3
