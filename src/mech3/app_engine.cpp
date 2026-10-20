#include "mech3/app_engine.h"
#include "mech3/app_mech.h"

#include <cstdio>
#include <cstring>

#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zsound/zsnd_cd.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zvideo/zvid_dd.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/mech3_interp.h"
#include "mech3/app_view.h"
#include "mech3/mech_control.h"
#include "mech3/music.h"
#include "mech3/player.h"
#include "platform/platform.h"
#include "platform/render3d.h"
#include "platform/sound_output.h"

namespace mech3 {

namespace {

GameEngine* g_engine = nullptr;

// The options' calls into modules (mech3/options), wired where those are ported.
struct WiredOptionsHooks : OptionsHooks {
  void sound_set_volume(float v) override { zsnd::set_volume(v); }  // 0x0056f260
  void music_set_volume(int32_t l, int32_t r) override {           // 0x004ac230
    mech3_music::set_volume(static_cast<uint16_t>(l), static_cast<uint16_t>(r));
  }
  void model_set_global_light(int32_t on) override { zmod::set_shadows(on); }  // 0x00526a70
  void input_set_screen(int32_t w, int32_t h) override { zin::mouse_set_screen(w, h); }  // 0x0051b590
  int32_t video_mode() override { return g_engine ? static_cast<int32_t>(g_engine->peek32(0x72f348)) : 0; }
  const char* message(int32_t id) override { return g_engine ? g_engine->message(id) : ""; }  // 0x00576040
  void controls_set_defaults() override { mech3::controls_set_defaults(); }        // 0x00411150
  void controls_load(const char* name) override { mech3::controls_load(name); }    // 0x004120a0
  void controls_free() override { mech3::controls_free_groups(); }                 // 0x00410a80
  Handle player_view() override { return g_engine ? g_engine->player_view() : 0; }  // [0x005f4c8c]
  void view_set_camera_mode(Handle, int32_t mode) override { program_view_set_camera(mode); }  // 0x0040ae70
  uint32_t unported(const char* key, std::initializer_list<Arg> args) override {
    return g_engine ? g_engine->unported(key, args) : 0;
  }
};
WiredOptionsHooks g_options_hooks;

// zSound's engine variables (0x00597d10): the options that hold them.
void* find_var(const char* name) {
  if (std::strcmp(name, "SoundLOD") == 0) return &options().sound_lod;
  if (std::strcmp(name, "SoundVolume") == 0) return &options().sound_volume;
  return nullptr;  // MuteSound is not an option of MW3's
}

void zerr_to_log(int level, const char* file, int line, const char* message) {
  std::fprintf(stderr, "mw3: report %#x %s:%d %s\n", level, file ? file : "", line, message ? message : "");
}

}  // namespace

int32_t program_camera_option() { return camera_mode(); }  // 0x0040ce70
void program_camera_set(int32_t mode) { set_camera_mode(mode); }  // 0x0040ce10
int32_t program_mouse_mode() { return options().mouse_mode; }
void program_set_mouse_mode(int32_t v) { options().mouse_mode = v; }

bool MountFiles::exists(const std::string& path) { return zutl::exists(path.c_str()); }

GameEngine::GameEngine(const Platform& p, std::string command_line)
    : p_(p), command_line_(std::move(command_line)), buffer_(640 * 480) {
  g_engine = this;
  ztimer::set_clock(p_.clock);
  zsnd_cd::set_backend(p_.files, p_.music, "music");
  set_options_hooks(&g_options_hooks);
  wire_mission_hooks(this);
  wire_frame_hooks(this);
  static ViewProgram vp;
  vp.ctx = this;
  vp.unported = [](void* c, const char* key) { return static_cast<GameEngine*>(c)->unported(key, {}); };
  vp.hardware = [](void* c) { return static_cast<GameEngine*>(c)->video_mode(); };
  vp.sync = [](void* c) { static_cast<GameEngine*>(c)->sync_view(); };
  wire_view(&vp);
}

GameEngine::~GameEngine() {
  delete_screens();
  if (g_engine == this) {
    g_engine = nullptr;
    set_options_hooks(nullptr);
    wire_mission_hooks(nullptr);
    wire_frame_hooks(nullptr);
    wire_view(nullptr);
  }
  zutl::install_messages(nullptr);
}

uint32_t GameEngine::unported(const char* key, std::initializer_list<Arg> args) {
  // The base's records of calls it makes itself (for the call-trace tests).
  if (std::strcmp(key, "CString::Format") == 0 || std::strcmp(key, "sprintf") == 0) return 0;
  uint32_t& n = counts_[key];
  if (n++ == 0 && !quiet) {
    std::string a;
    for (const Arg& x : args) {
      char buf[48];
      if (x.s) std::snprintf(buf, sizeof buf, " \"%.32s\"", x.s);
      else std::snprintf(buf, sizeof buf, " %#x", x.v);
      a += buf;
    }
    std::fprintf(stderr, "mw3: not ported: %s(%s ) -> 0 (logged once)\n", key, a.c_str());
  }
  return 0;
}

void GameEngine::print_unported_summary() const {
  std::fprintf(stderr, "mw3: %zu original functions reached that are not ported:", counts_.size());
  int i = 0;
  for (const auto& [k, n] : counts_) std::fprintf(stderr, "%s %s x%u", i++ % 6 ? "" : "\n ", k.c_str(), n);
  std::fprintf(stderr, "\n");
}

uint32_t GameEngine::peek32(uint32_t addr) {
  return static_cast<uint32_t>(peek8(addr)) | static_cast<uint32_t>(peek8(addr + 1)) << 8 |
         static_cast<uint32_t>(peek8(addr + 2)) << 16 | static_cast<uint32_t>(peek8(addr + 3)) << 24;
}
uint8_t GameEngine::peek8(uint32_t addr) {
  if (addr == 0x60b56c) return player_get_60b56c();  // the ports' copy (mech3/player)
  auto it = memory_.find(addr);
  return it == memory_.end() ? 0 : it->second;
}
void GameEngine::poke32(uint32_t addr, uint32_t v) {
  for (int i = 0; i < 4; ++i) poke8(addr + i, static_cast<uint8_t>(v >> (8 * i)));
}
void GameEngine::poke8(uint32_t addr, uint8_t v) { memory_[addr] = v; }

// UpdateWindow: the window is shown once the engine has started.
void GameEngine::update_window(uint32_t) {
  if (p_.window) p_.window->show();
}

uint32_t GameEngine::message_box_text(uint32_t, const char* text, const char* caption, uint32_t type) {
  if (!p_.alert) return 1;
  return static_cast<uint32_t>(p_.alert->message_box(text ? text : "", caption ? caption : "", type));
}

void GameEngine::sleep(uint32_t ms) {
  if (p_.clock) p_.clock->sleep(ms);
}

// 0x00575f30: Mech3Msg.dll, found through the mounts. Missing, the original
// shows the system's error text and exits; so does this.
uint32_t GameEngine::message_dll_load(const char* name) {
  const std::string path = zutl::find(name);
  if (path.empty() || !zutl::read_message_dll(path, dll_)) {
    // Case differs between the exe's "mech3Msg.dll" and the file's name.
    const std::string alt = zutl::find("Mech3Msg.dll");
    if (alt.empty() || !zutl::read_message_dll(alt, dll_)) {
      if (p_.alert) p_.alert->abort_message(std::string("Cannot read ") + name + " from the game folder.");
      exit_process(1);
      return 0;
    }
  }
  zutl::install_messages(&dll_);
  return 1;
}

const char* GameEngine::message_format(int32_t id, const char* arg) {
  formatted_ = zutl::format_message(id, arg);
  return formatted_.c_str();
}

// 0x004ac150, the game's message box (zTwoD's dlgMessage.zrd, not ported): the
// platform's box with the same texts. OK answers 1 as the game's does.
uint32_t GameEngine::message_box(int32_t a, int32_t caption, int32_t text, int32_t arg) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%d", arg);
  const std::string body = zutl::format_message(text, buf);
  const std::string title = message(caption);
  unported("0x004ac150", {a, caption, text});
  if (!p_.alert) return 0;
  return p_.alert->message_box(body, title, static_cast<uint32_t>(a) & 0xf) == 1 ? 1 : 0;
}

// 0x004ac150 with three strings (the controls screen's reassignment questions).
uint32_t GameEngine::message_box3(int32_t a, int32_t caption, int32_t text, const char* s1, const char* s2,
                                  const char* s3) {
  const std::string body = zutl::format_message(text, {s1, s2, s3});
  const std::string title = message(caption);
  unported("0x004ac150", {a, caption, text});
  if (!p_.alert) return 0;
  return p_.alert->message_box(body, title, static_cast<uint32_t>(a) & 0xf) == 1 ? 1 : 0;
}

Handle GameEngine::joy_state() { return handle_of(zin::joy_state()); }
float GameEngine::source_value(int32_t src, Handle joy) {
  return mech3::source_value(src, static_cast<const platform::JoyState*>(object_of(joy)), nullptr);
}

// 0x00410700 on a new interp object: "interp.zbd" with "DamageScale".
Handle GameEngine::script_new() {
  auto* in = new zint::Interp;
  interp_create(*in);
  interp() = in;
  return handle_of(in);
}
void GameEngine::script_delete(Handle s) {
  auto* in = static_cast<zint::Interp*>(object_of(s));
  if (interp() == in) interp() = nullptr;
  delete in;
}

// CWnd::CreateEx for the 640x480 frame, shown larger (the game draws at its
// own resolution; the platform scales the frame to the window).
Handle GameEngine::frame_create(Handle) {
  hwnd_ = p_.window ? p_.window->create("", p_.window_width, p_.window_height) : 0x20000;
  return hwnd_;
}
void GameEngine::frame_set_title(Handle, const char* title) {
  if (p_.window) p_.window->set_title(title ? title : "");
}

// 0x0050da50 opens mech3.err; the rebuild's error reports go to stderr.
void GameEngine::error_log_open(uint32_t, uint32_t, const char*) { zerr::set_sink(zerr_to_log); }

void GameEngine::reader_init(const char*) { zrdr::init(); }
void GameEngine::reader_open_archive(const char* path, int32_t current) { zrdr::open_archive(path, current != 0); }
void GameEngine::reader_close_archives(int32_t all) { zrdr::close_archives(all != 0); }

// 0x00598210: the store's key, and the machine the detail settings are
// picked for. FIX (A): the platform describes a modern machine instead of the
// CPUID timing loop, GlobalMemoryStatus and DirectDraw's caps (settings.md).
void GameEngine::settings_init(const char* company, const char* product, const char* version) {
  zreg::Machine m;
  std::memcpy(m.vendor, "GenuineIntel", 13);
  m.cpu_class = 6;     // CPU_CLASS_PENTIUM_PRO: a P6-family CPU, as every modern x86 is
  m.cpu_mhz = 2000;
  m.flags = 0x41;      // MMX, a 3D card
  const uint64_t kb = static_cast<uint64_t>(p_.ram_mb) * 1024;
  m.ram_kb = static_cast<uint32_t>(kb > 0x3fffff ? 0x3fffff : kb);  // dwTotalPhys stops at 4 GB
  m.video_kb = 256 * 1024;
  zreg::init(company, product, version, m, install_options_);
  zreg::set_settings(p_.settings);
}

// 0x0040c420, the detail.zrd loader with the saved settings (mech3/options).
int32_t GameEngine::options_load(const char* name) { return load_options(name); }

int32_t GameEngine::sound_init(uint32_t window) {
  zsnd::Hooks& h = zsnd::hooks();
  h.output = p_.sound;
  h.find_var = find_var;
  h.cd_open = [](zrdr::Node* cd_tracks) { zsnd_cd::open(zsnd_cd::read_cues(cd_tracks)); };
  h.cd_close = [] { zsnd_cd::close(); };
  return zsnd::init(window);
}
void GameEngine::sound_set_mode(int32_t v) { zsnd::set_archives(v); }
// 0x004e6b00, the callback that gives each sound its zClass object, is not
// ported: the sounds load without it.
void GameEngine::sound_load(uint32_t window, const char* name, uint32_t callback, uint32_t a) {
  if (callback) unported("0x004e6b00", {});
  zsnd::load(window, name, nullptr, static_cast<int32_t>(a));
}
void GameEngine::sound_shutdown_groups(uint32_t) { zsnd::close_device(nullptr); }
void GameEngine::sound_shutdown() { zsnd::shutdown(); }
void GameEngine::light_init() { Engine::light_init(); }

// zInput, ported (gamez/zinput, docs/input.md) on the platform's input.
void GameEngine::input_init(int32_t commands, const char* map) { zin::init(commands, map); }
void GameEngine::input_start(uint32_t window, uint32_t cmd_show) {
  zin::set_input(p_.input);
  zin::start(window, cmd_show);
}
void GameEngine::input_devices_shutdown() { zin::stop(); }
void GameEngine::input_acquire() { zin::resume(); }
void GameEngine::input_unacquire() { zin::suspend(); }
uint32_t GameEngine::joystick_present() { return zin::joy_present() ? 1 : 0; }
uint32_t GameEngine::force_feedback_present() { return zin::force_feedback(); }
void GameEngine::input_set_mode(uint32_t mode) { zin::set_mode(mode); }
void GameEngine::input_poll(uint8_t callbacks) { zin::poll(callbacks != 0); }
uint32_t GameEngine::key_state(int32_t key) { return zin::key_state(key); }
void GameEngine::input_clear() { zin::kbd_reset(); }
void GameEngine::input_reset() { zin::reset(); }
void GameEngine::input_capture() { zin::mouse_use(); }
void GameEngine::input_release() { zin::mouse_unuse(); }
void GameEngine::input_center_mouse() { zin::mouse_centre(); }
void GameEngine::mouse_set_pos(int32_t x, int32_t y) { zin::mouse_from_cursor(x, y); }
void GameEngine::mouse_set_accel(float x, float y) { zin::mouse_set_accel(x, y); }

void GameEngine::clock_init() { ztimer::init(); }

// 0x00579c60: the screen mode. Direct3D (video mode 1) when a card was
// chosen; the software renderer's modes are not offered (render.md).
int32_t GameEngine::video_open(uint32_t, int32_t hw, int32_t, int32_t) {
  poke32(0x72f348, hw ? 1u : 0u);
  if (!hw) unported("0x00579c60 software", {});
  const Rect& d = mech3::display();
  width_ = d.w > 0 ? d.w : 640;
  height_ = d.h > 0 ? d.h : 480;
  buffer_.assign(static_cast<size_t>(width_) * height_, 0);
  if (p_.render) p_.render->set_viewport(0, 0, width_, height_);
  return 0;
}

// [0x0072f38c]: the card's memory. A modern GPU, reported as 256 MB.
uint32_t GameEngine::video_card_memory(int32_t, uint32_t* total) {
  *total = 256u << 20;
  return 1;
}

// 0x00576de0: the 16-bit back buffer locked for the 2D blits.
void GameEngine::video_lock() {
  zrndr::Target2D& t = zrndr::target2d();
  t.base = reinterpret_cast<uint8_t*>(buffer_.data());
  t.width = static_cast<uint32_t>(width_);
  t.height = static_cast<uint32_t>(height_);
  t.pitch = static_cast<uint32_t>(width_ * 2);
  t.format = 6;
  zrndr::set_finish_target(buffer_.data(), width_, height_, width_ * 2);  // 0x00558290, as 0x00576de0 sets it
}
void GameEngine::video_clear(Handle) { std::fill(buffer_.begin(), buffer_.end(), uint16_t{0}); }
// 0x00576ea0: the back buffer to the screen.
// A frame whose 3D scene was drawn (the Core state's) shows that instead.
void GameEngine::video_flip(int32_t, int32_t, int32_t, int32_t) {
  if ((scene_drawn_ || layer_drawn_) && p_.render) p_.render->present();
  else if (p_.screen) p_.screen->present_16(buffer_.data(), width_, height_, width_ * 2, true);
  last_drew_scene_ = scene_drawn_;
  scene_drawn_ = layer_drawn_ = false;
  ++frames_presented;
  if (on_present) on_present(frames_presented);
}
// 0x004f7d80: the zClass action lists, each followed by the frame update
// (docs/action-lists.md): the player's tick (2), weapon_tick (3), the
// mission's animations and the scripts' ticks.
void GameEngine::lists_update() { zcls::run_action_lists(); }
void GameEngine::classes_shutdown() { zcls::classes_reset(); }
void GameEngine::missions_shutdown() { mech3::mech_types_close(); }
// 0x00576cf0: the 3D window cleared before the Core state draws (the sky
// colour of the debug viewer: the window's clear colour is not traced).
void GameEngine::video_clear_b(Handle) {
  if (p_.render) p_.render->clear(true, 89, 115, 140);
}
// 0x00579840: the cursor's on/off, the old state back. The menus draw their own.
uint32_t GameEngine::video_show_cursor(uint32_t on) {
  const uint32_t old = cursor_;
  cursor_ = on;
  return old;
}

// FIX (A): the fade's DirectDraw and Direct3D calls on the 16-bit buffer
// (fixes.md, Running on modern machines).
// 0x00579500: the screen to a new image. The port has one buffer, whichever
// the original asked for. Handles of freed stills are reused. FIX (A): the
// screen is the GPU's back buffer, read back and scaled to the game's 16-bit
// size (the exe's Lock read the DirectDraw surface); without a readback, the
// 16-bit buffer (the menus' screens, drawn there in full).
Handle GameEngine::video_grab(int32_t) {
  Handle h = kGrabs;
  while (grabs_.count(h)) h += 0x10;
  std::vector<uint32_t> px;
  int w = 0, ht = 0;
  if (video_mode() != 0 && p_.render && p_.render->read_back(&px, &w, &ht) && w > 0 && ht > 0) {
    std::vector<uint16_t> still(static_cast<size_t>(width_) * height_);
    for (int y = 0; y < height_; ++y) {
      const uint32_t* row = px.data() + static_cast<size_t>(y * ht / height_) * w;
      for (int x = 0; x < width_; ++x) {
        const uint32_t c = row[x * w / width_];
        still[static_cast<size_t>(y) * width_ + x] = zvid::pack_bytes(c >> 16 & 0xff, c >> 8 & 0xff, c & 0xff);
      }
    }
    grabs_[h] = std::move(still);
  } else {
    grabs_[h] = buffer_;
  }
  return h;
}
// 0x0055b1f0: the fade's still back over the buffer, whole, at 0, 0.
void GameEngine::image_draw(Handle image, int32_t x, int32_t y, int32_t key, Handle rect) {
  auto it = grabs_.find(image);
  if (it == grabs_.end() || x || y || rect || it->second.size() != buffer_.size())
    return (void)Engine::image_draw(image, x, y, key, rect);
  buffer_ = it->second;
}
// 0x00558540, ported (zrndr::tint): kept for the software finish, or the
// Direct3D quad. A rect is four ints in the modules' memory.
void GameEngine::screen_tint(uint16_t colour, Handle rect, double alpha) {
  int32_t r[4] = {};
  if (rect)
    for (int i = 0; i < 4; ++i) r[i] = static_cast<int32_t>(peek32(rect + 4 * i));
  zrndr::tint(colour, rect ? r : nullptr, alpha, video_mode() != 0);
}
void GameEngine::image_free(Handle image) {
  if (!grabs_.erase(image)) Engine::image_free(image);
}

// The Direct3D frame (render-frame.md): the ported lock, scene nesting, quad
// and finish. FIX (A): the back buffer's Lock gives the 16-bit layer, filled
// with kLayerKey; its Unlock lays what the 2D code drew over the 3D frame.
void GameEngine::video_begin_3d() { zvid::lock_3d(); }
void GameEngine::video_end_3d() { zvid::unlock_3d(); }
void GameEngine::video_begin_scene() { zvid::scene_begin(); }
void GameEngine::video_draw_3d() { zvid::draw_tint_quad(); }
void GameEngine::video_end_scene() { zvid::scene_end(); }
void GameEngine::render_finish() { zrndr::finish(); }

int32_t GameEngine::lock_back(zvid::Surface* s) {
  std::fill(buffer_.begin(), buffer_.end(), kLayerKey);
  s->width = width_, s->height = height_, s->pitch = width_ * 2, s->pixels = buffer_.data();
  return 0;
}
int32_t GameEngine::unlock_back(zvid::Surface*) {
  if (p_.screen) p_.screen->overlay_16(buffer_.data(), width_, height_, width_ * 2, true, kLayerKey);
  layer_drawn_ = true;
  return 0;
}

void wire_frame_hooks(GameEngine* e) {
  zvid::surface_hooks() = e ? zvid::SurfaceHooks{[](void* c, zvid::Surface* s) {
                                                    return static_cast<GameEngine*>(c)->lock_back(s);
                                                  },
                                                  [](void* c, zvid::Surface* s) {
                                                    return static_cast<GameEngine*>(c)->unlock_back(s);
                                                  },
                                                  e}
                            : zvid::SurfaceHooks{};
  zrndr::tint_hooks().quad = e ? zvid::set_tint_quad : nullptr;
  zvid::scene_depth() = 0;
}

Handle GameEngine::handle_of(void* p) {
  if (!p) return 0;
  auto it = handles_.find(p);
  if (it != handles_.end()) return it->second;
  objects_.push_back(p);
  const Handle h = 0x00100000u + static_cast<Handle>(objects_.size());
  handles_[p] = h;
  return h;
}
void* GameEngine::object_of(Handle h) const {
  if (h <= 0x00100000u || h - 0x00100000u > objects_.size()) return nullptr;
  return objects_[h - 0x00100001u];
}

void GameEngine::sound_update(int32_t a, int32_t b) { zsnd::frame(a, b); }
Handle GameEngine::sound_find(const char* name) { return handle_of(zsnd::find(name)); }
void GameEngine::sound_play(Handle s, float v) {
  if (auto* o = static_cast<zsnd::Object*>(object_of(s))) zsnd::play(o, v);
}
Handle GameEngine::sound_listener() { return handle_of(zsnd::snapshot()); }
void GameEngine::sound_stop_all(Handle s) {
  if (auto* o = static_cast<zsnd::Snapshot*>(object_of(s))) zsnd::stop_snapshot(o);
}
void GameEngine::sound_release_all(Handle s) {
  if (auto* o = static_cast<zsnd::Snapshot*>(object_of(s))) {
    zsnd::free_snapshot(o);
    handles_.erase(o);
    objects_[s - 0x00100001u] = nullptr;
  }
}
void GameEngine::sound_reset() { zsnd::reset(); }
void GameEngine::voice_start(const char* name) { zsnd::load_set(name); }
void GameEngine::voice_stop(Handle s) {
  if (auto* v = static_cast<zsnd::VoiceBase*>(object_of(s))) zsnd::stop(v);
}
void GameEngine::voice_release(const char* name) { zsnd::unload_set(name); }

int32_t GameEngine::model_global_light() { return zmod::shadows_on(); }
void GameEngine::model_set_global_light(int32_t on) { zmod::set_shadows(on); }

}  // namespace mech3
