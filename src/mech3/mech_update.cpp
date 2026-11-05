#include <cstring>
#include "gamez/zutil/zfix.h"
// The mech's frame, Mech::Update (docs/movement.md, The frame). Original
// file: mech3\mech.cpp (0x00419fc0 sits between its heat code and
// mech_anim.cpp).
#include <cmath>

#include "gamez/zclass/object3d.h"
#include "gamez/zentity/zent_manager.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "mech3/mech_headlight.h"
#include "mech3/mech.h"
#include "mech3/mech_move.h"
#include "mech3/mech_reader.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
UpdateCalls g_calls;

constexpr int kSuspended = 14, kDead = 10, kFallenBack = 12, kFallenFront = 13;
constexpr int kViewKey = 0x17;          // CMD_ id of the camera view toggle
constexpr float kYawDecay = 3.0f;       // 0x005e1ffc, per second
constexpr float kPushDecay = 4.5f;      // 0x005e2000
constexpr float kKnockDecay = 6.0f;     // 0x005bce40
constexpr float kKnockMin = 0.2f;       // 0x005bce44
constexpr float kWadeSound = 0.35f;     // 0x005bcd48, of the height
constexpr int kWalkSound = 0, kWadeSoundId = 1, kLimpSound = 2;

zmth::Quat quat(const float q[4]) { return {q[0], q[1], q[2], q[3]}; }
}  // namespace

UpdateCalls& update_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x0041d190 (vtable +0x14c): a second after the last hit the
// "last part hit" falls back to the torso, and stays refreshed from then on.
void Mech::hit_timeout() {
  if (last_hit_time - -1.0f < world().time) note_hit();  // 0x005bce0c
}

// Mech3.exe 1.2 @ 0x00419fc0 (vtable +0x34). dt carries over the frames the
// entity budget skipped. Each step's order is the original's; the hooks
// stand in for what is not ported (docs/movement.md, The frame).
void Mech::update(float dt) {
  // Pirate's Moon's vt+0x34 and vt+0x3c around it (0x0041b900, 0x0041b930): the headlight out of the tree for
  // the update, back after, with its fade.
  Headlight* head = torso ? torso->headlight : nullptr;
  struct Back {
    Headlight* h;
    float dt;
    ~Back() { if (h) h->attach(dt); }
  } back{head, dt};
  if (head) head->detach();
  World& w = world();
  dt = dt + frame_dt;
  frame_dt = dt;
  impulse = {0, 0, 0};
  if (!zent::budget().allows()) return;
  if (g_calls.finish_power_down) g_calls.finish_power_down(*this);
  else finish_power_down();
  alive_time = alive_time + frame_dt;
  bool acting = !actions.empty();
  if (anim_state == kSuspended) return;
  if (!(narc_time < 0.0f)) narc_time = narc_time - frame_dt;
  const bool dying = anim_state == kDead && acting;  // the death fall still plays
  if (anim_state == kDead && !dying) {
    active = false;
    for (Sound& s : sounds) {
      if (!s) continue;
      if (heat_calls().stop_sound) heat_calls().stop_sound(s);
      s = nullptr;
    }
    if (this == w.mission_player && g_calls.stop_cockpit_sound)
      for (int i = 0; i < 30; ++i) g_calls.stop_cockpit_sound(i);
    if (g_calls.add_dead) g_calls.add_dead(*this);
    return;
  }
  const bool viewed = this == w.player;
  if (viewed && g_calls.key && (g_calls.key(kViewKey) == 1 || (g_calls.key(kViewKey) & 4))) {
    const int mode = controls->view == 2 ? 3 : controls->view == 3 ? 2 : 0;
    if (mode) {
      if (g_calls.controls_view) g_calls.controls_view(*controls, mode);
      else controls->set_view(mode);  // 0x00426780
    }
  }
  if (!dying) {
    const int go = g_calls.controls_update ? g_calls.controls_update(*this, frame_dt) : controls->update(frame_dt);
    if (!go) goto tail;
  }
  {
    zent::budget().spend();
    if (dying) ++w.dead_updates;
    if (viewed && g_calls.player_frame) g_calls.player_frame(*this, frame_dt);

    // The yaw rate decays; only yaw is ever set.
    vec_58.y = -vec_58.y * kYawDecay * dt + vec_58.y;
    if (std::fabs(-vec_58.y) < 0.05000000074505806) vec_58.y = 0.0f;  // 0x005bce30
    vec_58.z = 0.0f;
    vec_58.x = 0.0f;
    if (jump_jets_firing == 0 && !airborne()) vec_3c.x = 0.0f;
    if (controls->moves[0] || controls->moves[1]) {
      if (acting) {
        if (g_calls.update_action) g_calls.update_action(*this, frame_dt);
        else update_action(frame_dt);
      } else if (g_calls.select_gait) {
        g_calls.select_gait(*this, frame_dt);
      } else {
        select_gait(frame_dt);
      }
    }
    // Lying down and powered: no turning or part updates, and a local mech
    // gets up when the timer runs out.
    if (!acting && (anim_state == kFallenBack || anim_state == kFallenFront) && !is_powered_down()) {
      acting = true;
      lie_time = lie_time - dt;
      if (!remote && lie_time < 0.0f) {
        if (g_calls.get_up) g_calls.get_up(*this);
        else get_up();
      }
    }
    jet_fuel_step(dt);
    bool stopped = false, held = false;  // held: Collide reports the mech blocked
    if (!acting) {
      if (g_calls.turn) g_calls.turn(*this, frame_dt);
      else turn(frame_dt);
      torso->set_twist(controls->twist, controls->axis_30);
      torso->set_pitch(controls->pitch, controls->axis_2c);
      torso->update(frame_dt);
      hip->update(frame_dt);
      legs[0]->update(frame_dt);
      legs[1]->update(frame_dt);
      arms[0]->update(frame_dt);
      arms[1]->update(frame_dt);
    }
    if (powered_down) {
      vec_58 = {0, 0, 0};
      vec_3c = {0, 0, 0};
    }
    // Pirate's Moon: the collision checks the move predict worked out; it is committed unless the collider held the mech.
    if (collides) {
      held = g_calls.collide ? g_calls.collide(*this) : collide();
      stopped = blocked[0] || blocked[1] || blocked[2] || blocked[3];
    }
    const bool slid = skis->slid;
    if (!stopped || !held) {
      std::memcpy(orientation, pending_q, sizeof orientation);
      velocity = pending_velocity;
      std::memcpy(transform, pending_m, sizeof pending_m);
    }
    if (g_calls.moved) g_calls.moved(*this);
    if (slid && velocity.x * push.x + velocity.z * push.z < 0.0f) {
      // Pushed this frame, moving against the push: the throttle falls by a factor a frame. FIX (B): per 1/30 s.
      controls->throttle = zfix::options().per_second ? controls->throttle * std::pow(0.6f, zfix::frame_steps(dt))
                                                      : controls->throttle * 0.6f;  // 0x005dfaa8
    }
    if (push.x != 0.0f || push.z != 0.0f) {
      const float k = kPushDecay * dt;
      const Vec3 t{k * (0.0f - push.x), k * (0.0f - push.y), k * (0.0f - push.z)};
      push = {push.x + t.x, push.y + t.y, push.z + t.z};
      if (std::fabs(push.x) < 0.01) push.x = 0.0f;
      if (std::fabs(push.z) < 0.01) push.z = 0.0f;
    }
    const Vec3* at = skis_calls().position ? skis_calls().position(*this) : nullptr;
    if (g_calls.landmines) g_calls.landmines(at);
    hit_timeout();
    if (g_calls.weapons_changed && g_calls.weapons_changed(*this)) {
      void* screen = viewed && heat_calls().screen ? heat_calls().screen(*this) : nullptr;
      if (screen) {
        if (viewed) screen = heat_calls().screen(*this);
        if (g_calls.hud_weapons) g_calls.hud_weapons(screen);
      }
    }
    if (g_calls.weapons_fire) g_calls.weapons_fire(*this, controls->fire);
    if (g_calls.weapons_update) g_calls.weapons_update(*this, frame_dt);
    if (g_calls.equipment_update) g_calls.equipment_update(*this, frame_dt);
    else update_ams(*this);  // 0x00419200 (unwired until W18: no mech's AMS ever fired)
    if (g_calls.update_heat) g_calls.update_heat(*this, frame_dt);
    else update_heat(*this, frame_dt);
    if (!powered_down) movement_sounds();
    if (controls->wrapped) {
      controls->wrapped = 0;
      if (g_calls.unwrap) controls = g_calls.unwrap(*controls);
      else controls = controls->unwrap();  // primary vt+0x04
    }
    if (g_calls.x68c_update) g_calls.x68c_update(x68c, frame_dt);
    frame_dt = 0.0f;
  }
tail:
  if (g_calls.net_update) g_calls.net_update(*this, dt);
  if (knock > 0.0f) {
    const float k = knock - dt * kKnockDecay;
    if (k < kKnockMin) {
      knock = 0.0f;
      knock_vec = {0, 0, 0};
    } else {
      const float r = k / knock;
      knock = k;
      knock_vec = {knock_vec.x * r, knock_vec.y * r, knock_vec.z * r};
    }
  }
  if (g_calls.purge) g_calls.purge(*this, frame_dt);  // 0 after a full update
}

// Pirate's Moon 1.0 @ 0x004186f0: the lean rolls the display matrix into turns (detail bit 5).
bool Mech::lean_step(float dt) {
  if (!(detail & 0x20)) {
    lean = 0.0f;
    return false;
  }
  const MechDfn* def = g_calls.dfn ? g_calls.dfn(*this) : mech_dfn(pilot->roster);
  float target = 0.0f;
  if (vec_58.y != 0.0f && limp != 0 && limp != 1) {
    target = static_cast<float>(controls->turn * double{std::fabs(controls->throttle)});
    target = target * speed_factor * def->get<float>(0x154);  // lean_angle
  }
  const float l = (target - lean) * def->get<float>(0x158) * dt + lean;  // lean_factor
  lean = l;
  if (this == world().player && damage_calls().view_mode && damage_calls().view_mode() == 1) lean = l * 0.5f;
  if (std::fabs(lean) < 0.01) lean = 0.0f;  // 0x005dfca8
  if (lean == 0.0f) return false;
  zmth::Quat roll{};
  zmth::quat_roll(lean, roll);
  lean_q[0] = roll.w, lean_q[1] = roll.x, lean_q[2] = roll.y, lean_q[3] = roll.z;
  return true;
}

// Pirate's Moon 1.0 @ 0x00423590, called first in the ground step: the orientation, world velocity and matrix this frame
// would give, the vertical speed left out and put back, as velocity's too.
void Mech::predict(float dt) {
  const float y = vec_3c.y;
  const bool leaning = lean_step(dt);
  velocity.y = 0.0f, vec_3c.y = 0.0f;
  const float h = dt * 0.5f;
  zmth::Quat e, p, n;
  zmth::quat_exp({h * vec_58.x, h * vec_58.y, h * vec_58.z}, e);
  zmth::quat_mul(e, quat(orientation), p);
  zmth::quat_normalize(p, n);
  pending_q[0] = n.w, pending_q[1] = n.x, pending_q[2] = n.y, pending_q[3] = n.z;
  zmth::Vec3 wv;
  zmth::quat_rotate(n, {vec_3c.x, vec_3c.y, vec_3c.z}, wv);
  pending_velocity = {wv.x + push.x, wv.y + push.y, wv.z + push.z};
  const Vec3* at = skis_calls().position ? skis_calls().position(*this) : nullptr;
  const Vec3 from = at ? *at : Vec3{};
  zmth::Quat q = n;
  if (leaning) zmth::quat_mul(q, quat(lean_q), q);
  const float qa[4] = {q.w, q.x, q.y, q.z};
  zmth::quat_matrix(qa, pending_m);
  pending_m[9] = from.x + dt * pending_velocity.x;
  pending_m[10] = from.y + dt * pending_velocity.y;
  pending_m[11] = from.z + dt * pending_velocity.z;
  velocity.y = y, vec_3c.y = y;
}

// Mech3.exe 1.2 @ 0x0041a843..0x0041aa61: the walk, wade and limp loops,
// louder with the throttle. Wading compares the water's absolute height with
// 0.35 of the mech's (docs/movement-ground.md, Quirks).
void Mech::movement_sounds() {
  const HeatCalls& hc = heat_calls();
  const MoveCalls& mc = move_calls();
  auto play = [&](int id, float vol) -> Sound {
    return mc.pilot_sound ? mc.pilot_sound(*this, id, vol, -1.0f) : nullptr;
  };
  auto fade = [&](Sound s) -> Sound { return mc.fade_sound ? mc.fade_sound(s) : nullptr; };
  auto stop = [&](Sound& s) {
    if (!s) return;
    if (hc.stop_sound) hc.stop_sound(s);
    s = nullptr;
  };
  auto place = [&](Sound s) {
    const Vec3* at = skis_calls().position ? skis_calls().position(*this) : nullptr;
    if (g_calls.set_position) g_calls.set_position(s, at);
  };
  Sound& walk = sounds[kWalkSound];
  Sound& wade = sounds[kWadeSoundId];
  Sound& limping = sounds[kLimpSound];
  if (!(detail & 1)) {
    stop(limping);
    stop(wade);
    stop(walk);
    return;
  }
  const float floor = world().move_volume_floor;
  const float vol = (1.0f - floor) * std::fabs(controls->throttle) + floor;
  const float h = height * kWadeSound;
  const bool wading = skis->surface[1] > h || skis->surface[4] > h;
  int which = -1;
  if (wading && walk) {
    walk = fade(walk);
  } else if (wading && !wade) {
    which = kWadeSoundId;
    wade = play(kWadeSoundId, vol);
  } else if (!wading && !walk) {
    which = kWalkSound;
    walk = play(kWalkSound, vol);
  } else if (!wading && wade) {
    wade = fade(wade);
  } else if (walk) {
    which = kWalkSound;
    if (g_calls.set_volume) g_calls.set_volume(walk, vol);
  } else if (wade) {
    which = kWadeSoundId;
    if (g_calls.set_volume) g_calls.set_volume(wade, vol);
  }
  if (which != -1 && sounds[which]) place(sounds[which]);
  if (limp == 0 || limp == 1) {
    if (!limping) {
      limping = play(kLimpSound, vol);
    } else {
      if (g_calls.set_volume) g_calls.set_volume(limping, vol);
      place(limping);
    }
  } else {
    stop(limping);
  }
}

}  // namespace mech3
