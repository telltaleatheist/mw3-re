#include "mech3/mech.h"

#include <cmath>

#include "crt/msvc_rand.h"

namespace mech3 {

namespace {
World g_world;
HeatCalls g_calls;

constexpr float kHeatMax = 2.0f;        // 0x005bcd78
constexpr float kCritical = 2.0f;       // 0x005e1fd8
constexpr float kOverheat = 1.5f;       // 0x005e1fdc
constexpr float kShutdownWarn = 0.95f;  // 0x005e1fe0
constexpr float kShutdownSafe = 0.9f;   // 0x005e1fe4
constexpr float kBeepOn = 0.7f;         // 0x005e1fec
constexpr float kBeepOff = 0.7f;        // 0x005e1ff0
constexpr float kRecover = 0.4f;        // 0x005e1ff4
constexpr float kHazeStart = 0.8f;      // 0x005bcd90
constexpr int kLava = 8;
constexpr int kMultiplayer = 3;  // docs/gametypes.md

constexpr int kSoundBeep = 5, kSoundAlarm = 6, kSoundShutdownWarning = 0x14, kSoundShutdown = 0x15;
constexpr int kMechSoundAmmo = 15, kMechSoundCritical = 26;
constexpr int kMessageHeatDeathDisabled = 0x20c;  // Mech3Msg.dll

float rand_unit() { return static_cast<float>(crt::msvc_rand()) * 3.051851e-05f; }  // 0x005bcd94

void stop(Sound& s) {
  if (s) {
    if (g_calls.stop_sound) g_calls.stop_sound(s);
    s = nullptr;
  }
}

Sound play(Mech& m, int id) { return g_calls.play_sound ? g_calls.play_sound(m, id) : nullptr; }
int game_type() { return g_calls.game_type ? g_calls.game_type() : 0; }

uint32_t beep_state(uint32_t s) { return s & 7; }
uint32_t next_beep(uint32_t s) { return (s & ~7u) | ((s + 1) & 7); }
uint32_t next_shutdown(uint32_t s, int step) { return (s & ~0x38u) | ((s + 8 * step) & 0x38); }
}  // namespace

World& world() { return g_world; }
HeatCalls& heat_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x0045fb90
float HeatSinks::rate() const {
  return static_cast<float>(count * 0.1 * (doubles ? 2.5f : 1.25f));  // 0x005e4ee8, 0x005e4eec
}

// Mech3.exe 1.2 @ 0x0041cc10
bool Mech::is_powered_down() const { return power_down_pending || powered_down; }

// Mech3.exe 1.2 @ 0x0041d4f0
Foot* Mech::foot(int leg) const { return legs[leg] ? legs[leg]->foot : nullptr; }

// Mech3.exe 1.2 @ 0x004192f0. Walkthrough and thresholds: docs/heat.md.
void update_heat(Mech& m, float dt) {
  World& w = g_world;
  const bool player = &m == w.player;
  const bool full = w.full_heat_all_mechs || player;

  float rate = 1.0f;  // 0x005e1fd4
  if (m.powered_down) rate *= 3.0f;
  if (m.foot(0)->in_water) {
    // Quirk: max(depth / height, 1) makes (3f + 1)^2 at least 16, so the cap
    // always wins and wading is a flat x8.
    float f = m.foot(0)->water_depth / m.height;
    if (f < 1.0f) f = 1.0f;
    f = (f * 3.0f + 1.0f) * (f * 3.0f + 1.0f);
    if (!(f <= 8.0f)) f = 8.0f;
    rate *= f;
  }
  m.heat_dissipation = rate * 0.05f;  // 0x005e5d40

  if (full) {
    const float t = 1.6f - std::fabs(g_calls.throttle ? g_calls.throttle(m) : 0.0f);
    if (t < 1.0f) m.heat_dissipation *= t;
    if (m.jump_jets_firing) m.heat_added += dt * 0.05f;
    if (m.masc_time != 0.0f) m.heat_added += dt * 0.05f;
  }
  // Per frame, not per second: lava heats faster at higher frame rates.
  if (m.foot(0)->soil == kLava || m.foot(1)->soil == kLava) m.heat_added += 0.05f;
  m.heat_dissipation *= m.heat_sinks.rate();

  if ((g_calls.heat_option && g_calls.heat_option()) || game_type() == kMultiplayer) {
    m.heat -= dt * m.heat_dissipation;
    if (!(m.heat <= kHeatMax)) m.heat = kHeatMax;
    else if (m.heat < 0.0f) m.heat = 0.0f;
    m.heat += m.heat_added;
    if (!(m.heat <= kHeatMax)) m.heat = kHeatMax;
    else if (m.heat < 0.0f) m.heat = 0.0f;
  } else {
    m.heat_added = 0.0f;
    m.heat = 0.0f;
  }

  void* screen = player && g_calls.screen ? g_calls.screen(m) : nullptr;
  if (screen && m.heat >= kHazeStart && !w.no_heat_haze && g_calls.heat_haze)
    g_calls.heat_haze(screen, (m.heat - kHazeStart) * 5.0f, 5, w.dt);

  if (!full) {
    if (m.heat >= kCritical && game_type() != kMultiplayer && g_calls.torso_hit) g_calls.torso_hit(m, 1);
    m.heat_added = 0.0f;
    return;
  }

  if (m.is_powered_down()) m.heat_state = (m.heat_state & ~kOverride) | kPoweredDown;
  else m.heat_state &= ~kPoweredDown;
  if (m.heat < kRecover) m.heat_state &= ~kOverride;

  // The warning beep: 0 idle, 1 just started, 2 waiting, 3 sounding; 4-7 cancel.
  for (bool again = true; again;) {
    again = false;
    if (m.heat_state & 0x7e000000) m.heat_state |= 4;
    uint32_t& s = m.heat_state;
    switch (beep_state(s)) {
      case 0:
        if (m.heat > kBeepOn && w.time > m.beep_time) {
          if (player) m.heat_beep = play(m, kSoundBeep);
          again = true;
          s = next_beep(s);
        }
        break;
      case 1:
        m.beep_time = w.time + 2.0f;
        s = next_beep(s);
        break;
      case 2:
        if (w.time > m.beep_time) s = next_beep(s);
        break;
      case 3:
        if (m.heat < kBeepOff) {
          m.beep_time = w.time + 2.0f;
          if (player) stop(m.heat_beep);
          s &= ~7u;
        }
        break;
      case 4:
        s &= ~7u;
        break;
      default:
        m.beep_time = w.time + 2.0f;
        stop(m.heat_beep);
        s &= ~7u;
        break;
    }
  }

  // Shutdown, in bits 3-5: 0 normal, 1 warned, 2 shutting down, 3 shut down,
  // 4 critical. The top bits change what each state does.
  for (bool again = true; again;) {
    again = false;
    bool critical = false, over = false;
    uint32_t& s = m.heat_state;
    if (!(m.heat < kCritical)) {
      stop(m.heat_beep);
      stop(m.heat_alarm);
      if (player && g_calls.play_mech_sound) g_calls.play_mech_sound(kMechSoundCritical);
      m.shutdown_time = 0.0f;
      s &= ~kPoweredDown;
      s = (s & ~0x18u) | 0x20;
      critical = true;
    } else if (!m.is_powered_down() && !(m.heat < kOverheat)) {
      stop(m.heat_beep);
      stop(m.heat_alarm);
      over = true;
      s = (s & ~0x28u) | 0x10;
    }

    const uint32_t key = (s & 0x7f000000) | ((s >> 3) & 7);
    auto warn = [&] {
      if (m.heat > kShutdownWarn && w.time > m.voice_time) {
        m.voice_time = w.time + 4.0f;
        m.shutdown_time = w.time + 4.0f;
        if (player) m.shutdown_warning = play(m, kSoundShutdownWarning);
        s = next_shutdown(s, 1);
      }
    };
    auto reset = [&] {
      stop(m.heat_alarm);
      s &= ~0x38u;
    };
    switch (key) {
      case 0:
      case kOverride:
        warn();
        break;
      case 1:
      case kOverride | 1:
        if (m.heat <= kShutdownSafe) {
          m.heat_shutdown = false;
          s &= ~kOverride;
          s &= ~0x38u;
        } else if (w.time > m.shutdown_time) {
          m.shutdown_time = w.time + 3.0f;
          again = true;
          s = next_shutdown(s, 1);
        }
        break;
      case kOverride | 2:
        if (!over) {
          s = next_shutdown(s, 1);
          break;
        }
        [[fallthrough]];
      case 2:
        m.voice_time = w.time + 4.0f;
        if (player) m.shutdown_voice = play(m, kSoundShutdown);
        if (g_calls.power) g_calls.power(m, 1);
        m.heat_shutdown = true;
        s = next_shutdown(s, 1);
        break;
      case 3:
      case kOverride | 3:
        if (m.heat <= kShutdownSafe) {
          m.heat_shutdown = false;
          s &= ~kPoweredDown;
          reset();
        } else if (w.time > m.shutdown_time) {
          m.shutdown_time = w.time + (0.1f + rand_unit() * 2.9f);
          if (player && !m.heat_alarm) m.heat_alarm = play(m, kSoundAlarm);
          s = next_shutdown(s, 1);
        }
        break;
      case 4:
      case kOverride | 4:
        if (!(w.time > m.shutdown_time)) break;
        if (!critical && rand_unit() < 0.8f) {
          // Ride it out: maybe an ammo bin cooks off, then back to state 3.
          if (g_calls.ammo_explosion && g_calls.ammo_explosion(m) && player && g_calls.play_mech_sound)
            g_calls.play_mech_sound(kMechSoundAmmo);
          m.shutdown_time = w.time + 3.0f;
          s = next_shutdown(s, -1);
          break;
        }
        m.heat_shutdown = false;
        if (!m.heat_death_disabled) {
          if (critical && game_type() != kMultiplayer) {
            if (g_calls.torso_hit) g_calls.torso_hit(m, 1);
          } else if (g_calls.torso_hit) {
            g_calls.torso_hit(m, 0);  // 0x0041cfa0(1)
          }
          m.heat = 0.0f;
          s = next_shutdown(s, -1);
        } else {
          if (g_calls.hud_message) g_calls.hud_message(2.0f, kMessageHeatDeathDisabled);
          m.voice_time = w.time + 8.0f;
          if (player) {
            stop(m.heat_beep);
            reset();
          } else {
            s &= ~0x38u;
          }
        }
        break;
      case kPoweredDown | 1:
      case kPoweredDown | kOverride | 1:
        s &= ~0x38u;
        break;
      case kPoweredDown | 2:
      case kPoweredDown | kOverride | 2:
        s &= ~0x38u;
        warn();
        break;
      case kPoweredDown | 3:
      case kPoweredDown | kOverride | 3:
        if (m.heat <= kRecover && w.time > m.shutdown_time) {
          m.heat_shutdown = false;
          if (g_calls.power) g_calls.power(m, 0);
          s &= ~0x38u;
        }
        break;
      case kPoweredDown | 4:
      case kPoweredDown | kOverride | 4:
        m.heat_shutdown = false;
        reset();
        break;
      default:
        break;
    }
  }
  m.heat_added = 0.0f;
}

}  // namespace mech3
