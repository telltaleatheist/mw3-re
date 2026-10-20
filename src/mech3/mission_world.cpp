#include "mech3/mission_world.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zvideo/zvid_format.h"

namespace mech3 {

namespace {
const char kFile[] = "C:\\zipper\\Mech3\\mission.cpp";
constexpr double kDegToRad = 0.01745329251994;  // 0x005c60c0: pi cut to 3.1415926535892, / 180
Environment g_env;
WorldHooks g_hooks{};

bool hardware() { return zrndr::device_fog().active != 0; }  // 0x0072f348

using zcls::world_get_fog_altitude;
using zcls::world_get_fog_color;
using zcls::world_get_fog_range;
}  // namespace

Environment& environment() { return g_env; }
WorldHooks& world_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x00491d70
void world_init(MissionWorld& m) {
  m.weather = nullptr;
  m.sun = nullptr;
  m.fluctuate = 0;
  m.fog = 0;
}

// Mech3.exe 1.2 @ 0x00491d80
bool world_clear(MissionWorld& m) {
  const WorldHooks& h = g_hooks;
  if (m.weather) {
    h.weather_remove(m.weather);
    h.weather_destroy(m.weather);
  }
  m.weather = nullptr;
  if (m.sun) {
    delete m.sun;
    m.sun = nullptr;
  }
  if (m.fluctuate) {
    h.fluctuate_on(0);
    m.fluctuate = 0;
  }
  m.fog = 0;
  return true;
}

// Mech3.exe 1.2 @ 0x004926d0, with zMath's 0x00522170 (pitch and yaw to the
// direction they face) and 0x004020d0 on its level part.
void world_update(const MissionWorld& m) {
  const WorldHooks& h = g_hooks;
  if (!m.fluctuate) return;
  const float* rot = h.camera_angles();
  const float pitch = rot[0], yaw = rot[1];
  const float cp = static_cast<float>(std::cos(double{pitch}));
  zmth::Vec3 dir{-cp * static_cast<float>(std::sin(double{yaw})), 0.0f,
                 -cp * static_cast<float>(std::cos(double{yaw}))};
  zmth::normalize(dir);
  const float d[2] = {dir.z, -dir.x};
  h.clouds_advance(d, h.frame_dt());
}

// Mech3.exe 1.2 @ 0x00492660. The sun is lit only on the Direct3D renderer.
void world_set_enabled(MissionWorld& m, bool on) {
  const WorldHooks& h = g_hooks;
  if (m.weather) {
    if (on) h.weather_start(m.weather, -1.0f);
    else h.weather_stop(m.weather);
  }
  if (m.sun) zcls::node_set_active(m.sun->node, on && hardware() ? 1 : 0);
  if (m.fluctuate) h.fluctuate_on(on ? 1 : 0);
}

// Mech3.exe 1.2 @ 0x00491de0. Layout, defaults and per-mission values:
// docs/drafts/world-params.md.
bool world_read_params(MissionWorld& m, const char* file, const char* key) {
  const WorldHooks& h = g_hooks;
  // A second read leaks the first handle, as the original does.
  if (Node* sunlight = zcls::list_find_by_name(zcls::kAllNodes, "sunlight"))
    m.sun = new NodeHandle{sunlight};

  m.water_range[0] = 0.0f, m.water_range[1] = 250.0f;
  m.water_altitude[0] = 0.0f, m.water_altitude[1] = 100.0f;
  m.water_color[0] = 50.0f, m.water_color[1] = 50.0f, m.water_color[2] = 80.0f;

  if (h.game_type() == 3 && h.mission_number() > 2) file = "mpinfo.zrd", key = "world_params";
  zrdr::Node* root = zrdr::load(file);
  if (!root) {
    char msg[300];
    std::snprintf(msg, sizeof msg, "Failed to read %s", file);
    zerr::report(0x200, kFile, 0x73c, msg);
  } else if (zrdr::Node* params = zrdr::find(root, key)) {
    if (zrdr::Node* fog = zrdr::find(params, "fog")) {
      m.sky = nullptr;
      m.fog = 1;
      zrdr::get_vec2(fog, "range", m.fog_range);
      if (hardware()) zrdr::get_vec2(fog, "hwrange", m.fog_range);
      zrdr::get_vec3(fog, "color", m.fog_color);
      zrdr::get_vec2(fog, "altitude", m.fog_altitude);
      zrdr::get_int(fog, "state", &m.fog_state);
      // Without a "node" the name is null; the original's compare then faults.
      m.sky = zcls::list_find_by_name(zcls::kAllNodes, zrdr::get_string(fog, "node"));
    } else {
      m.fog = 0;
    }

    if (zrdr::Node* water = zrdr::find(params, "underwater")) {
      zrdr::get_vec2(water, "range", m.water_range);
      if (hardware()) zrdr::get_vec2(water, "hwrange", m.water_range);
      zrdr::get_vec3(water, "color", m.water_color);
      zrdr::get_vec2(water, "altitude", m.water_altitude);
    }

    float ceiling = 150.0f;
    zrdr::get_float(params, "jet_ceiling", &ceiling);
    g_env.jet_ceiling = ceiling;
    h.puffer_tick_reset();

    if (zrdr::Node* wind = zrdr::find(params, "global_wind")) {
      float v;
      if (zrdr::get_float(wind, "max_speed", &v)) h.wind_max_speed(v);
      if (zrdr::get_float(wind, "random_accel", &v)) h.wind_random_accel(v);
      if (zrdr::get_float(wind, "random_angvel", &v))
        h.wind_random_angvel(static_cast<float>(v * kDegToRad));
      float vel[3];
      if (zrdr::get_vec3(wind, "velocity", vel)) h.wind_velocity(vel);
    }

    if (m.sun) {
      zrdr::Node* sun = zrdr::find(params, "sun");
      if (m.sun && sun) {
        Node* light = m.sun->node;
        float rot[3];
        h.light_get_orientation(light, &rot[0], &rot[1], &rot[2]);
        float ambient = static_cast<zcls::Light*>(light->data)->get<float>(0xa0);
        float diffuse = static_cast<zcls::Light*>(light->data)->get<float>(0x9c);
        if (zrdr::get_vec3(sun, "orientation", rot)) {
          for (float& r : rot) r = static_cast<float>(r * kDegToRad);
        }
        zrdr::get_float(sun, "diffuse", &diffuse);
        zrdr::get_float(sun, "ambient", &ambient);
        h.light_set_orientation(light, rot[0], rot[1], rot[2]);
        h.light_set_ambient(light, ambient);
        h.light_set_diffuse(light, diffuse);
      }
    }

    // Zero or below (or NaN) falls back to 20: the heat code divides by it.
    float temperature = 20.0f;
    zrdr::get_float(params, "ambient_temperature", &temperature);
    if (!(temperature > 0.0f)) temperature = 20.0f;
    g_env.ambient_temperature = temperature;
    g_env.inv_temperature = 1.0f / temperature;

    if (zrdr::Node* weather = zrdr::find(params, "weather")) {
      int32_t particles = 100;
      zrdr::get_int(weather, "particles", &particles);
      // Any other type, or none, keeps the weather from an earlier read.
      if (const char* type = zrdr::get_string(weather, "type")) {
        if (std::strcmp(type, "SNOW") == 0) m.weather = h.create_snow(particles);
        else if (std::strcmp(type, "RAIN") == 0) m.weather = h.create_rain(particles);
      }
      if (m.weather) {
        Weather& w = *m.weather;
        int32_t c[3] = {0, 0, 0};
        if (zrdr::get_ivec3(weather, "color", c))
          w.put<uint16_t>(0x44, zvid::pack_bytes(static_cast<uint8_t>(c[0]), static_cast<uint8_t>(c[1]),
                                                 static_cast<uint8_t>(c[2])));
        if (zrdr::Node* wind = zrdr::find(weather, "wind")) {
          float dir = 0.0f, vel = 0.0f;
          zrdr::get_float(wind, "dir", &dir);
          zrdr::get_float(wind, "vel", &vel);
          w.put(0x68, dir), w.put(0x6c, vel);
        }
        float gravity = 0.0f;
        zrdr::get_float(weather, "gravity", &gravity);
        w.put(0x70, gravity);
        float gradient[2] = {0.0f, 0.0f};
        if (zrdr::get_vec2(weather, "alpha_gradient", gradient)) w.put(0x4c, gradient[1]), w.put(0x48, gradient[0]);
        h.weather_add(m.weather);
      }
    }

    if (zrdr::Node* fl = zrdr::find(params, "fluctuate")) {
      m.fluctuate = 1;
      float v;
      if (!zrdr::get_float(fl, "period", &v)) v = 5.0f;
      h.fluctuate_period(v);
      if (!zrdr::get_float(fl, "wavelength", &v)) v = 500.0f;
      h.fluctuate_wavelength(v);
      float amplitude;
      if (!zrdr::get_float(fl, "base", &v)) v = 0.7f;
      if (!zrdr::get_float(fl, "amplitude", &amplitude)) amplitude = 0.3f;
      h.fluctuate_base_amp(v, amplitude);
      if (!zrdr::get_float(fl, "heading", &v)) v = 0.0f;
      h.fluctuate_heading(static_cast<float>(v * kDegToRad));
    }
  }

  world_set_enabled(m, true);
  if (m.sun && !h.hw_card_flag()) zcls::node_set_active(m.sun->node, 0);
  if (m.weather) m.weather->camera = h.camera;

  if (m.fog && h.world) {
    zcls::world_set_fog_type(h.world, 1);
    zcls::world_set_fog_range(h.world, m.fog_range[0], m.fog_range[1]);
    zcls::world_set_fog_color(h.world, m.fog_color[0], m.fog_color[1], m.fog_color[2]);
    zcls::world_set_fog_altitude(h.world, m.fog_altitude[0], m.fog_altitude[1]);
    // The named sky is shown and the horizon's other children hidden.
    if (m.sky && h.camera) {
      if (Node* horizon = static_cast<zcls::Camera*>(h.camera->data)->nodes[2]) {
        for (int i = 0; i < horizon->child_count(); ++i) {
          Node* child = horizon->children[i];
          zcls::node_set_active(child, child == m.sky ? 1 : 0);
        }
      }
    }
  } else if (!m.fog && h.world) {
    world_get_fog_range(h.world, &m.fog_range[0], &m.fog_range[1]);
    world_get_fog_color(h.world, &m.fog_color[0], &m.fog_color[1], &m.fog_color[2]);
    world_get_fog_altitude(h.world, &m.fog_altitude[0], &m.fog_altitude[1]);
  }
  zrdr::free_node(root);
  if (!hardware()) {
    h.software_color(m.fog_color);
    h.software_color(m.water_color);
  }
  return true;
}

}  // namespace mech3
