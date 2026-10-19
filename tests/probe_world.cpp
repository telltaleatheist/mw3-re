// The mission's world parameters reader (mech3/mission_world, 0x491de0) for
// tests/differential/test_world_params.py: a small scene (sunlight, a horizon
// with three skies, a world and a camera), the reader run once or twice, and
// a text trace of every hooked call and the state after.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/mission_world.h"

namespace {

std::string g_out;
int32_t g_cfg[16];
int g_weathers = 0;
mech3::Weather* g_made[4];

enum Cfg { kHardware, kGameType, kMission, kHwCard, kHasWorld, kHasView, kHasSun, kHasHorizon, kNoMemory,
           kRotX, kRotY, kRotZ, kDiffuse, kAmbient };

void line(const char* fmt, ...) {
  char buf[400];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  g_out += buf;
  g_out += '\n';
}

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
float flt(int32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

// The scene: list 6 holds a decoy, the skies, the horizon, the sunlight, the
// world and the camera, in that order.
enum { kDecoy, kScroll, kMain, kBlack, kHorizon, kSun, kWorld, kCamera, kNodeCount };
const char* const kNames[] = {"decoy", "horscroll", "hormain", "horblack", "horizon", "sunlight", "world1", "camera1"};
zcls::Node g_nodes[kNodeCount];
zcls::Node* g_children[3];
zcls::Light g_light;
zcls::World g_world;
zcls::Camera g_camera;

const char* name_of(const zcls::Node* n) {
  for (int i = 0; i < kNodeCount; ++i)
    if (n == &g_nodes[i]) return kNames[i];
  return n ? "?" : "0";
}
const char* weather_of(const mech3::Weather* w) {
  static char buf[8];
  for (int i = 0; i < g_weathers; ++i)
    if (w == g_made[i]) return std::snprintf(buf, sizeof buf, "W%d", i + 1), buf;
  return w ? "W?" : "0";
}

void build_scene() {
  zcls::reset_lists();
  for (int i = 0; i < kNodeCount; ++i) {
    g_nodes[i] = zcls::Node{};
    std::strcpy(reinterpret_cast<char*>(g_nodes[i].raw), kNames[i]);
    g_nodes[i].put<int32_t>(0x34, i == kSun ? 9 : i == kWorld ? 13 : i == kCamera ? 8 : 5);
    g_nodes[i].set_flags(i == kScroll || i == kSun ? 4 : 0x10);
    if (i == kSun && !g_cfg[kHasSun]) continue;
    zcls::push_back(zcls::kAllNodes, &g_nodes[i]);
  }
  g_light = zcls::Light{};
  g_light.put(0x9c, g_cfg[kDiffuse]), g_light.put(0xa0, g_cfg[kAmbient]);
  g_nodes[kSun].data = &g_light;
  g_world = zcls::World{};
  for (int o = 0; o < 0x34; o += 4) g_world.put<uint32_t>(o, 0x3f800000u + o * 0x1000);
  g_world.put<uint32_t>(0, 0);
  g_nodes[kWorld].data = &g_world;
  g_camera = zcls::Camera{};
  g_camera.nodes[2] = g_cfg[kHasHorizon] ? &g_nodes[kHorizon] : nullptr;
  g_nodes[kCamera].data = &g_camera;
  g_children[0] = &g_nodes[kScroll], g_children[1] = &g_nodes[kMain], g_children[2] = &g_nodes[kBlack];
  g_nodes[kHorizon].put<int32_t>(0x5c, 3);
  g_nodes[kHorizon].children = g_children;
}

void install_hooks() {
  mech3::WorldHooks& h = mech3::world_hooks();
  h.game_type = [] { return line("game_type"), g_cfg[kGameType]; };
  h.mission_number = [] { return line("mission_number"), g_cfg[kMission]; };
  h.hw_card_flag = [] { return line("hw_card_flag"), g_cfg[kHwCard]; };
  h.world = g_cfg[kHasWorld] ? &g_nodes[kWorld] : nullptr;
  h.camera = g_cfg[kHasView] ? &g_nodes[kCamera] : nullptr;
  h.puffer_tick_reset = [] { line("puffer"); };
  h.wind_velocity = [](const float v[3]) { line("wind_vel %08x %08x %08x", bits(v[0]), bits(v[1]), bits(v[2])); };
  h.wind_max_speed = [](float v) { line("wind_max %08x", bits(v)); };
  h.wind_random_accel = [](float v) { line("wind_accel %08x", bits(v)); };
  h.wind_random_angvel = [](float v) { line("wind_angvel %08x", bits(v)); };
  h.light_get_orientation = [](zcls::Node* n, float* x, float* y, float* z) {
    line("light_get %s", name_of(n));
    *x = flt(g_cfg[kRotX]), *y = flt(g_cfg[kRotY]), *z = flt(g_cfg[kRotZ]);
  };
  h.light_set_orientation = [](zcls::Node* n, float x, float y, float z) {
    line("light_rot %s %08x %08x %08x", name_of(n), bits(x), bits(y), bits(z));
  };
  h.light_set_ambient = [](zcls::Node* n, float v) { line("light_amb %s %08x", name_of(n), bits(v)); };
  h.light_set_diffuse = [](zcls::Node* n, float v) { line("light_dif %s %08x", name_of(n), bits(v)); };
  static auto make = [](const char* kind, int32_t n) -> mech3::Weather* {
    if (g_cfg[kNoMemory]) return nullptr;  // the original's new fails before the ctor
    line("%s %d", kind, n);
    mech3::Weather* w = new mech3::Weather{};
    g_made[g_weathers++] = w;
    return w;
  };
  h.create_snow = [](int32_t n) { return make("snow", n); };
  h.create_rain = [](int32_t n) { return make("rain", n); };
  h.weather_add = [](mech3::Weather* w) { line("weather_add %s", weather_of(w)); };
  h.weather_remove = [](mech3::Weather* w) { line("weather_remove %s", weather_of(w)); };
  h.weather_destroy = [](mech3::Weather* w) { line("weather_destroy %s", weather_of(w)); };
  h.weather_start = [](mech3::Weather* w, float v) { line("weather_start %s %08x", weather_of(w), bits(v)); };
  h.weather_stop = [](mech3::Weather* w) { line("weather_stop %s", weather_of(w)); };
  h.fluctuate_on = [](int32_t on) { line("fl_on %d", on); };
  h.fluctuate_period = [](float v) { line("fl_period %08x", bits(v)); };
  h.fluctuate_wavelength = [](float v) { line("fl_wave %08x", bits(v)); };
  h.fluctuate_base_amp = [](float b, float a) { line("fl_baseamp %08x %08x", bits(b), bits(a)); };
  h.fluctuate_heading = [](float v) { line("fl_heading %08x", bits(v)); };
  h.software_color = [](const float c[3]) { line("swcolor %08x %08x %08x", bits(c[0]), bits(c[1]), bits(c[2])); };
}

// The 4-byte fields of the original's +0x0c..+0x4c, by offset (+0x28, the sky, apart).
uint8_t* field(mech3::MissionWorld& m, int o) {
  if (o < 0x14) return reinterpret_cast<uint8_t*>(m.fog_range) + (o - 0x0c);
  if (o < 0x1c) return reinterpret_cast<uint8_t*>(m.fog_altitude) + (o - 0x14);
  if (o < 0x28) return reinterpret_cast<uint8_t*>(m.fog_color) + (o - 0x1c);
  if (o == 0x2c) return reinterpret_cast<uint8_t*>(&m.fog_state);
  if (o < 0x38) return reinterpret_cast<uint8_t*>(m.water_range) + (o - 0x30);
  if (o < 0x40) return reinterpret_cast<uint8_t*>(m.water_altitude) + (o - 0x38);
  return reinterpret_cast<uint8_t*>(m.water_color) + (o - 0x40);
}

void dump(mech3::MissionWorld& m) {
  uint8_t raw[0x4c];
  raw[8] = m.fluctuate, raw[9] = m.fog;
  for (int o = 0x0c; o < 0x4c; o += 4)
    if (o != 0x28) std::memcpy(raw + o, field(m, o), 4);
  std::string s = "m";
  char b[16];
  for (int o = 0x0c; o < 0x4c; o += 4) {
    if (o == 0x28) continue;
    uint32_t u;
    std::memcpy(&u, raw + o, 4);
    std::snprintf(b, sizeof b, " %08x", u);
    s += b;
  }
  line("%s fl=%d fog=%d weather=%s sun=%s sky=%s", s.c_str(), raw[8], raw[9], weather_of(m.weather),
       m.sun ? name_of(m.sun->node) : "0", name_of(m.sky));
  const mech3::Environment& e = mech3::environment();
  line("env %08x %08x %08x", bits(e.ambient_temperature), bits(e.inv_temperature), bits(e.jet_ceiling));
  for (int i = 0; i < g_weathers; ++i) {
    std::string w = "weather";
    for (int o = 4; o < 0x98; o += 4) {
      if (o == 0x50) continue;
      std::snprintf(b, sizeof b, " %08x", g_made[i]->get<uint32_t>(o));
      w += b;
    }
    line("%s cam=%s", w.c_str(), name_of(g_made[i]->camera));
  }
  std::string w = "world";
  for (int o = 0; o < 0x34; o += 4) {
    std::snprintf(b, sizeof b, " %08x", g_world.get<uint32_t>(o));
    w += b;
  }
  line("%s", w.c_str());
  for (int i = 0; i < kNodeCount; ++i) line("node %s %08x", kNames[i], g_nodes[i].flags());
}

}  // namespace

extern "C" {

// cfg: see Cfg. Returns the trace length, or -1 if the archive does not open.
int probe_world(const char* archive, const char* file, const char* key, const int32_t* cfg, int reads, char* out,
                int cap) {
  std::memcpy(g_cfg, cfg, sizeof g_cfg);
  g_out.clear();
  for (int i = 0; i < g_weathers; ++i) delete g_made[i];
  g_weathers = 0;
  zrdr::close_archives(true);
  zrdr::init();
  if (!zrdr::open_archive(archive, true)) return -1;
  zrndr::device_fog().active = cfg[kHardware];
  zvid::set_pixel_format(5, 6, 5);
  zerr::set_sink([](int level, const char*, int at, const char* msg) { line("zerr %x %d %s", level, at, msg); });
  build_scene();
  install_hooks();
  mech3::environment() = mech3::Environment{1.5f, 2.5f, 3.5f};

  mech3::MissionWorld m;
  for (int o = 0x0c; o < 0x4c; o += 4) {
    const uint32_t u = 0x40000000u + o * 0x1000;
    if (o != 0x28) std::memcpy(field(m, o), &u, 4);
  }
  m.sky = nullptr;
  mech3::world_init(m);
  for (int r = 0; r < reads; ++r) {
    line("read %d -> %d", r, mech3::world_read_params(m, file, key) ? 1 : 0);
    dump(m);
  }
  delete m.sun;  // the leaked first handle of a second read is not tracked
  zerr::set_sink(nullptr);
  zrdr::close_archives(true);
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), n);
  return n;
}

}  // extern "C"
