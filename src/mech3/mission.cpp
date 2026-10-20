#include "mech3/mission.h"

#include <cstdarg>
#include <cstdio>
#include <string>

namespace mech3 {

namespace {
Missions g_missions;
MissionHooks g_default_hooks;
MissionHooks* g_hooks = &g_default_hooks;

// MFC's CString::Format (and sprintf) for the formats mission.cpp uses.
std::string format(const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  return buf;
}
}  // namespace

Missions& missions() { return g_missions; }
void set_mission_hooks(MissionHooks* h) { g_hooks = h ? h : &g_default_hooks; }
MissionHooks& mission_hooks() { return *g_hooks; }

void MissionHooks::render_rect(int32_t out[4]) {
  unported("0x0040d6a0", {});
  out[0] = out[1] = out[2] = out[3] = 0;
}
uint32_t MissionHooks::video_texture_memory(int32_t dev, uint32_t* total, uint32_t* free_) {
  *total = *free_ = 0;
  return unported("[0x0072f38c]", {dev});
}

// Mech3.exe 1.2 @ 0x00484c50
const char* mission_dir(int32_t index) {
  static const char* const kDirs[] = {"c1", "c2", "c3", "c4", "c4b", "t1", "lab", "dev1", "dev2"};
  return index >= 0 && index < 9 ? kDirs[index] : "none";
}

// Mech3.exe 1.2 @ 0x004902a0: the megabytes of textures to load, 0 in
// software. A 3D card's own memory, capped by the TextureMemory option
// (TEXMEM_MAX no cap; 12, 10 and 8 MB all cap at 8; 6, 4, 2). Always true.
bool Missions::texture_budget(uint32_t* mb) {
  MissionHooks& h = *g_hooks;
  if (h.hw_card_flag() != 0 && h.video_fn_390(-1) == 0) {
    uint32_t free_ = 0;
    *mb = h.video_texture_memory(-1, mb, &free_) ? (*mb >> 20) : 1;
  } else {
    *mb = 0;
  }
  auto cap = [&] {
    switch (h.texture_memory()) {
      case 1: case 2: case 3: return 8;
      case 4: return 6;
      case 5: return 4;
      case 6: return 2;
      default: return -1;  // 0 (TEXMEM_MAX) and anything else
    }
  };
  int32_t c = cap();
  if (c == -1) return true;
  if (*mb == 0) {
    *mb = static_cast<uint32_t>(cap());
    return true;
  }
  c = cap();
  if (static_cast<uint32_t>(c) < *mb) *mb = static_cast<uint32_t>(cap());
  return true;
}

// Mech3.exe 1.2 @ 0x00490600 with 0x00490590: the reader block of the
// game: "ia%d" for type 2, "mp%d" for 3, else "m%d", by mission number.
void Missions::block_name(char* out) {
  MissionHooks& h = *g_hooks;
  const int32_t n = h.game_mission_number();
  const int32_t type = h.game_type();
  std::snprintf(out, 0x20, type == 2 ? "ia%d" : type == 3 ? "mp%d" : "m%d", n);
}

// Mech3.exe 1.2 @ 0x00490630: the mission's reader archives and loose paths.
void Missions::open_readers(const char* name) {
  MissionHooks& h = *g_hooks;
  h.reader_open(format("zbd\\%s\\reader.zbd", name).c_str(), 0);
  h.reader_set_path(format("..\\data\\common\\zrdr;..\\data\\common\\zrdr\\mechs;..\\data\\%s\\zrdr;..\\data\\%s\\zrdr\\aipath",
                           name, name).c_str());
  char block[0x20];
  block_name(block);
  h.reader_add_path(format("..\\data\\%s\\zrdr\\aipath\\%s", name, block).c_str());
  h.reader_open(format("zbd\\%s\\reader%s.zbd", name, block).c_str(), 0);
}

// Mech3.exe 1.2 @ 0x0048faf0: the world. A mission already up is kept, another
// one unloaded first. The texture levels: `level` for "texture" (2 under 4 MB
// of card memory, 3 under 8, else 0), `flags` 0x11 for the mech textures under
// 4 MB or when the install has no 16-bit set (InstallOptions 2 or 4); the
// mech set is "mechtex" on a 3D card, "mechtexS" in software.
bool Missions::load_world(int32_t index) {
  MissionHooks& h = *g_hooks;
  h.progress(0.1f);
  h.fn_518f50(0);
  if (current != -1) {
    if (current == index) return true;
    h.unload();
    return load_world(index);
  }
  const std::string name = mission_dir(index);
  h.interp_define("USEZBD");
  h.fn_484270(1);
  h.set_use_archive();
  h.anim_allow_loading(1);
  const bool hw = h.hw_card_flag() != 0;
  uint32_t mb = 0;
  texture_budget(&mb);
  const uint8_t rgb_option = h.app_rgb();
  uint8_t rgb = rgb_option;
  int32_t flags = 0, level = -1;
  auto check_16bit = [&] {
    if (h.install_options(2) == 0 && h.install_options(4) == 0) flags = 0x11, rgb = 0;
  };
  if (mb == 0) {
    rgb = hw;
    check_16bit();
    level = 0;
  } else if (mb < 4) {
    flags = 0x11;
    level = 2;
  } else if (mb < 8) {
    check_16bit();
    level = 3;
  } else {
    rgb = hw;
    check_16bit();
    level = 0;
  }
  if (hw) {
    h.interp_define("HARDWARE");
    h.light_5269e0(0);
  } else {
    rgb = 0;
  }
  const uint8_t holes = (rgb || rgb_option) ? 1 : 0;
  h.set_holes(holes);
  h.fn_576270(0);
  uint32_t total = 0, free_ = 0;
  if (h.video_texture_memory(-1, &total, &free_)) {
    const double k = 9.5367431640625e-07;  // 2^-20
    h.progress_text(format("Texture Memory: %.2f (%.2f)", total * k, free_ * k).c_str());
  }
  if (abort) return false;
  h.textures_begin(0);
  h.texture_library("texture", level, hw ? 1 : 0);
  h.texture_library(hw ? "mechtex" : "mechtexS", flags, 1);
  h.progress_text(format("MechWarrior3 (%s) (%s)", "PRODUCTION", h.version_number()).c_str());
  h.progress_text(h.video_driver_name());
  h.progress_text(h.video_mode_name());
  h.interp_source(format("support\\%ssize.gw", name.c_str()).c_str());  // its result is ignored
  h.classes_reset();
  h.models_reset();
  if (abort) return false;
  h.progress_text(format("Loading [ %s ]", name.c_str()).c_str());
  h.fn_535ed0(0.5f);
  const std::string script = format("%s.gs", name.c_str());
  if (!h.interp_source(script.c_str())) {
    h.zerr(0x800, "C:\\zipper\\Mech3\\mission.cpp", 0x261, "Failed to Interpret: %s -- Mission Load Aborted",
           script.c_str());
    h.cleanup(1);
    return false;
  }
  if (abort) return false;
  h.progress(0.3f);
  if (abort) return false;
  h.images_path("..\\data\\common\\images\\hud;zbd");
  if (abort) return false;
  h.progress_text(format("Loading Textures (%d/%d) %s %s", level, flags, hw ? "RGB" : "PAL",
                         holes ? "HOLES" : "NOHOLES").c_str());
  h.textures_load();
  h.progress(0.4f);
  if (abort) return false;
  h.progress_text(h.message(0x104));
  const Handle world = h.find_node(0xd, "world1");
  const Handle camera = h.find_node(8, "camera1");
  const Handle window = h.find_node(0xe, "window1");
  const Handle display = h.find_node(0xf, "display");
  Handle mem = h.alloc(8);
  h.set_608c84(mem ? h.make_608c84(mem, world) : 0);
  mem = h.alloc(0x3c);
  const Handle vw = mem ? h.make_view_window(mem, window) : 0;
  mem = h.alloc(0x7c);
  h.set_view(mem ? h.make_view(mem, camera, vw, h.get_608c84()) : 0);
  h.lists_4f7f70();
  h.lists_4f7bd0();
  h.set_render_target(window);
  h.set_display_target(display);
  h.set_camera(camera);
  // The view window's two rectangles (+0x8, +0x18) both from the render
  // rectangle, so they never differ and the scale is never computed.
  int32_t r[4];
  h.render_rect(r);
  for (int i = 0; i < 4; ++i) {
    h.set_field32(vw, 0x8 + 4 * i, static_cast<uint32_t>(r[i]));
    h.set_field32(vw, 0x18 + 4 * i, static_cast<uint32_t>(r[i]));
  }
  const bool changed = false;  // the compare of the two copies (0x00490008)
  h.set_field8(vw, 0x28, changed ? 1 : 0);
  h.window_set_size(h.field32(vw, 0x4), r[2] - r[0], r[3] - r[1]);
  h.window_set_origin(h.field32(vw, 0x4), r[0], r[1]);
  h.clear_9a17c0();
  if (abort) return false;
  h.fn_4e7b00();
  h.progress_text("Loading Effects");
  h.effects_load(world, camera, "effects.zrd");
  h.progress(0.5f);
  if (abort) return false;
  h.progress_text("Loading Animations");
  h.anim_set_world(world);
  h.anim_load("anim.zrd");
  h.progress(0.6f);
  if (abort) return false;
  h.progress_text("Loading DEClient");
  h.decals_load(world, 0);
  if (abort) return false;
  h.progress_text("Loading Weapons");
  h.weapons_load(world, "weapons.zrd", 0, 0x4b0020);
  h.progress(0.7f);
  if (abort) return false;
  h.progress_text("Loading HUD");
  const Handle w = h.window_rect();
  h.hud_load(h.display_rect(), w);
  h.fn_40db60();
  current = index;
  h.progress_text("Creating Stealth");
  mem = h.alloc(0x104);
  if (mem) {
    const Handle obj = h.stealth_4ac860(0xa1c);
    h.set_stealth(h.stealth_4a4de0(mem, obj ? h.stealth_4a51b0(obj, 1) : 0));
  } else {
    h.set_stealth(0);
  }
  return true;
}

// Mech3.exe 1.2 @ 0x00490ab0: the mission behind the world. The game type
// picks the objectives and the start animation (`start_single_player_m<n>`,
// `switch_ia_m<n>`, `start_multiplayer_m<n>`, `start_training_m<n>`, each
// falling back to the name without the number).
bool Missions::setup() {
  MissionHooks& h = *g_hooks;
  const int32_t index = h.game_mission_index();
  const std::string name = mission_dir(index);
  h.fn_576270(0);
  h.fn_48f400();
  h.progress(0.0f);
  const int32_t type = h.game_type();
  const int32_t number = h.game_mission_number();
  open_readers(name.c_str());
  if (!load_world(index)) return false;
  open_readers(name.c_str());
  h.mechs_init();
  h.fn_4319a0();
  h.fn_459e80(0x605ae0);
  std::string block = format("m%d", number);
  char reader_block[0x20];
  block_name(reader_block);
  h.world_params("world.zrd", reader_block);
  h.fn_4e7b40();
  h.entity_named(h.stealth(), "stealth");
  h.progress(0.8f);
  h.fn_459e80(0x605ae0);
  h.progress_text("Loading InOut data");
  h.inout_load();
  h.landmines_load(0x604e48, "landmines.zrd", block.c_str());
  h.fn_4b4ab0(0x60b228);
  h.player_setup(h.app_turrets());
  if (h.game_saved()) h.progress_text("Saved Game Loaded");
  if (abort) return false;
  for (int i = 1; i < 5; ++i) objectives[i] = 0;  // +0x7c..+0x88; +0x78 is left
  const Handle out[5] = {0x6098d0 + 0x78, 0x6098d0 + 0x7c, 0x6098d0 + 0x80, 0x6098d0 + 0x84, 0x6098d0 + 0x88};
  auto turrets = [&] {
    if (h.app_turrets()) h.turrets_load("aiTurrets.zrd", h.field32(h.field32(h.view(), 0x4), 0x38) + 0x1e4);
  };
  Handle anim = 0;
  auto start_anim = [&](const char* fmt, const char* fallback) {
    anim = h.anim_start(format(fmt, number).c_str());
    if (!anim) anim = h.anim_start(fallback);
  };
  switch (type + 1) {
    case 0: case 1: case 2:  // -1, 0, 1: single player
      turrets();
      block = format("m%d", number);
      h.progress_text("Creating Objectives");
      h.objectives_load("objectives.zrd", block.c_str(), out[0], out[1], out[2], out[3], out[4]);
      start_anim("start_single_player_m%d", "start_single_player");
      break;
    case 3:  // 2: instant action
      block = format("ia%d", number);
      h.progress_text("Creating Objectives");
      h.objectives_load("objectives.zrd", block.c_str(), out[0], out[1], out[2], out[3], out[4]);
      start_anim("switch_ia_m%d", "switch_ia");
      break;
    case 4:  // 3: multiplayer
      block = format("mp%d", number);
      h.progress_text("Creating Objectives");
      h.objectives_load(number > 2 ? "mpinfo.zrd" : "objectives.zrd", number > 2 ? "objectives" : block.c_str(),
                        out[0], out[1], out[2], out[3], out[4]);
      start_anim("start_multiplayer_m%d", "start_multiplayer");
      if (h.mp_flag()) h.mp_stations(stations);
      break;
    case 5:  // 4: training
      turrets();
      block = format("m%d", number);
      h.progress_text("Creating Objectives");
      h.game_objectives("objectives.zrd", block.c_str(), out[0], out[1], out[2], out[3], out[4]);
      start_anim("start_training_m%d", "start_training");
      break;
    default:
      break;
  }
  sounds[0] = h.sound_find("com_success");
  sounds[1] = h.sound_find("com_failed");
  sounds[2] = h.sound_find("obj_primary");
  sounds[3] = h.sound_find("obj_secondary");
  h.progress_text("Post Vehicle Fixup");
  h.fn_4d2e20();
  h.fn_4d31d0();
  h.fn_4e2120();
  if (abort) return false;
  h.ai_list_reset(0x604048);
  h.ai_list_reset(0x604078);
  h.keys_reset();
  for (int i = 0; i < 25; ++i) h.hud_line_clear(i);  // the HUD's 25 text lines (0x005fc848, 0x4b0 apart)
  h.fn_40f040();
  h.fn_40f890();
  loaded = 1;
  if (abort) return false;
  h.progress(0.9f);
  h.ai_list_reset(0x603f90);
  f55 = f56 = 0;
  f58 = 0;
  h.fn_4d4b30();
  if (abort) return false;
  h.progress_text("Ready To Launch");
  if (anim) h.anim_run(anim);
  h.interp_5964c0(0);
  h.progress(1.0f);
  return true;
}

// Mech3.exe 1.2 @ 0x00490880 and its thread 0x00490900. FIX (A): the load
// runs here, not on a thread at idle priority beside the briefing: the
// world's textures go to the GPU, which wants the main thread. The briefing
// sees it finished at once (0x00490930).
void Missions::start() {
  g_hooks->game_mission_index();
  done = 0;
  abort = 0;
  failed = setup() ? 0 : 1;
  done = 1;
}

}  // namespace mech3
