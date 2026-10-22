#include "mech3/mech_pocket.h"

#include <cmath>

#include "mech3/lance_orders.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech.h"
#include "mech3/pocket_panel.h"

namespace mech3 {
namespace {
PocketCalls g_calls;
PocketGlobals g_globals;

void note(const char* key, uint32_t arg = 0) {
  if (g_calls.unported) g_calls.unported(key, arg);
}
}  // namespace

PocketCalls& pocket_calls() { return g_calls; }
PocketGlobals& pocket_globals() { return g_globals; }

// The inputs the mode takes over and gives back: the throttle's state, the
// smoothed axes, the trigger (0x004690d0's copy, the way out's the other way).
void PocketControls::copy_inputs(const MechControls& f) {
  throttle_returns = f.throttle_returns;  // +0x3f
  throttle_held = f.throttle_held;        // +0x3e
  throttle_target = f.throttle_target;    // +0x44
  x5a = f.x5a;                            // +0x5a
  joy_throttle = f.joy_throttle;          // +0x40
  reverse = f.reverse;                    // +0x50
  throttle = f.throttle;                  // +0x10
  jump = f.jump;                          // +0x20
  jj_forward = f.jj_forward;              // +0x28
  jj_strafe = f.jj_strafe;                // +0x24
  turn = f.turn;                          // +0x14
  pitch = f.pitch;                        // +0x18
  twist = f.twist;                        // +0x1c
  fire = f.fire;                          // +0x34
  joy_throttle_set = f.joy_throttle_set;  // +0x3c
}

// Mech3.exe 1.2 @ 0x004690d0 (the dispatcher's 0x004690d0(controls, the
// instance, 2, mode, 500, 0x5f, 1); its 500 is replaced at once).
PocketControls::PocketControls(MechControls* p, PlayerControls* pi, int32_t o, uint32_t mode)
    : prev(p), prev_instance(pi), order(o) {
  construct(mode);  // 0x00427320(mode, 0): no commands registered
  panel_x = (g_calls.display_width ? g_calls.display_width() : 0) - 0x8c;
  panel_y = 0x5f;
  net_element = g_calls.net_element ? g_calls.net_element() : 0;
  x1b8 = 0;
  g_globals.x60 = 0, g_globals.x50 = 0;
  if (prev) copy_inputs(*prev);
}

// Mech3.exe 1.2 @ 0x00469310: the HUD's hold given back (0x004484b0(1)) when
// a panel is up and the mech is the view's, the panel freed; then the player
// controls' 0x00427410: [0x006056ec] cleared when it is this.
PocketControls::~PocketControls() {
  if (panel && owner && g_calls.hud_hold) g_calls.hud_hold(*owner, true);
  if (panel) {
    if (g_calls.panel_free) g_calls.panel_free(panel);
    else delete static_cast<PocketPanel*>(panel);  // 0x00468720
  }
  panel = nullptr;
  if (player_controls_instance() == this) player_controls_instance() = nullptr;
}

// Mech3.exe 1.2 @ 0x0046a440: the owner set, the old panel freed, the cursor's
// contact reset (the template 0x005e9d10), a new panel for a mech (0x00468250)
// and the HUD's hold let go (0x004484b0(0)).
void PocketControls::set_owner(Mech* m) {
  owner = m;
  if (panel) {
    if (g_calls.panel_free) g_calls.panel_free(panel);
    else delete static_cast<PocketPanel*>(panel);  // 0x00468720
  }
  panel = nullptr;
  place[0] = place[1] = place[2] = 0;
  net_element = g_calls.net_element ? g_calls.net_element() : 0;
  aim = ai::Contact{};
  x1c0 = 0, x1c1 = 0;
  if (!m) return;
  panel = g_calls.panel_make ? g_calls.panel_make(*this) : new PocketPanel(this, panel_x, panel_y, 0x0c);
  if (g_calls.hud_hold) g_calls.hud_hold(*m, false);
}

// Mech3.exe 1.2 @ 0x0046a360: the panel's frame (0x00469d00, 0x0046b0a0);
// unless +0x154 bit 0, no jump, no jump-jet axes, the trigger off.
int PocketControls::update(float dt) {
  if (g_calls.panel_frame) {
    g_calls.panel_frame(*this, dt);
  } else {
    if (panel) pocket_frame(*this, dt);  // 0x00469d00
    keys(dt);                            // 0x0046b0a0
  }
  if (!(x154 & 1)) {
    set_jump(0);  // IControls vt+0x34
    jj_strafe = 0, jj_forward = 0;  // vt+0x44, vt+0x3c
    fire = 0;     // +0x34
  }
  return 1;
}

namespace {
constexpr float kRate = 10.0f;                   // 0x005e50b0: the mode's pitch and twist, per second
constexpr float kStep = 0.025f;                  // 0x3ccccccd, a frame
constexpr float kReturn = -0.7f;                 // 0x005e50ac
constexpr double kZero = 0.05;                   // 0x005c2da0
constexpr double kSnap = 0.05000000074505806;    // 0x005c2da8
constexpr float kYaw = -0.26f, kPitch = -0.16f;  // 0x005e50a0, 0x005e50a4: per pixel a frame
constexpr float kLook = 1.5f;                    // 0x005c2d8c

bool held(int cmd) {
  ControlsCalls& k = controls_calls();
  return ((k.command ? k.command(cmd) : zin::command(cmd)) & 3) != 0;
}
// The exe's clamps: above 1, else not at or above the low end (a NaN too).
float clamp_to(float v, float lo) { return v > 1.0f ? 1.0f : !(v >= lo) ? lo : v; }
void ease(float& v, float target, float dt) {
  const float n = (target - v) * kRate * dt + v;
  v = n;
  if (std::fabs(target - n) < kSnap) v = target;
}
}  // namespace

void PocketControls::mouse_look(float dt, float* p, float* tw) {
  if (!(flags & 1)) return;
  ControlsCalls& k = controls_calls();
  const auto* m = static_cast<const zin::MouseInfo*>(k.mouse_info ? k.mouse_info() : zin::mouse_info());  // 0x0051b7b0
  const float inv = 1.0f / dt;
  const float fx = inv * kYaw, fy = inv * kPitch;
  switch (view) {
    case 0:
      look_y = look_x = look_dy = look_dx = 0.0f;
      *tw = m->fdx * fx;
      *p = m->fdy * fy;
      break;
    case 1:  // the player's own puts this on the free look; here the torso
      look_y = look_x = look_dy = look_dx = 0.0f;
      *tw = m->fdx * fx * kLook;
      *p = m->fdy * fy * kLook;
      break;
    case 2:
      look_dx = look_x - m->fx;
      look_dy = look_y - m->fy;
      look_x = m->fx, look_y = m->fy;
      *tw = 0.0f, *p = 0.0f;
      break;
    case 3:
      *tw = m->fdx * fx;
      *p = m->fdy * fy;
      if (k.mouse_set) k.mouse_set(look_x, look_y);  // 0x0051b440
      else zin::mouse_set(look_x, look_y);
      break;
    default:
      break;
  }
}

// The player's update (0x00427d60) without the pilot's axis table: the
// mouse look, the throttle step, the turn, the jets, the zoom box, twist
// at 1 (not 1.5), pitch, the trigger; the throttle, turn and jump set as
// there; pitch and twist eased at 10/s (6/s there); no free look. Then the
// smoothing (0x00426960).
void PocketControls::keys(float dt) {
  Mech& m = *owner;
  if (!m.is_powered_down()) {  // vt+0xa0
    ControlsCalls& k = controls_calls();
    if (k.pilot_tick) k.pilot_tick(*this);
    else tick();               // IControls vt+0xa4: the pilot's (0x004d8990)
    float d = 0, tn = 0, jump_in = 0, p = 0, tw = 0, strafe = 0, ahead = 0;
    void* screen = g_calls.view_screen ? g_calls.view_screen(m) : nullptr;
    mouse_look(dt, &p, &tw);
    // FIX (B): the throttle keys' step is per 1/30 s, as the player's own (fixes.md, Frame rate).
    const float step = kStep * zfix::frame_steps(dt);
    if (held(0x18)) {
      d = step;
      joy_throttle_set = throttle_held = false;
    } else if (held(0x19)) {
      d = -step;
      joy_throttle_set = throttle_held = false;
    }
    if (held(2)) tn = 1.0f;
    else if (held(3)) tn = -1.0f;
    if (m.equipment[11] > 0) {  // +0x3d8: jump jets
      if (held(0xc)) jump_in = 1.0f;
      if (k.mw2_jets && k.mw2_jets()) {  // 0x0040d1e0
        if (held(0xd)) jump_in = 1.0f, ahead = 1.0f;
        else if (held(0xe)) jump_in = 1.0f, ahead = -1.0f;
        if (held(0xf)) jump_in = 1.0f, strafe = -1.0f;
        else if (held(0x10)) jump_in = 1.0f, strafe = 1.0f;
        if (m.airborne()) {  // vt+0x144
          if (held(0x11)) tn = 1.0f;
          else if (held(0x12)) tn = -1.0f;
        }
      }
    }
    if (screen) {  // the zoom box (+0x1508) held open or shut, unless locked (+0x167c)
      const bool zoom = held(0x14);
      if (!(k.screen_busy ? k.screen_busy(screen) : uint8_t{0}) && k.zoom) k.zoom(screen, zoom);
    }
    if (held(8)) tw = 1.0f;
    else if (held(9)) tw = -1.0f;
    if (held(4)) p = 1.0f;
    else if (held(5)) p = -1.0f;
    fire = held(0x3d) ? 1 : 0;  // 0x00467480
    d = clamp_to(d, -1.0f);
    tn = clamp_to(tn, -1.0f);
    jump_in = clamp_to(jump_in, 0.0f);
    if (!joy_throttle_set) {
      if (!(d < 0.0f || d > 0.0f) && throttle_returns && !throttle_held) {
        throttle_target = fast_exp(kReturn * dt) * throttle_target;
        if (std::fabs(throttle_target) < kZero) throttle_target = 0.0f;
      } else {
        set_throttle(throttle_target + d);  // IControls +0x20
      }
    } else {
      set_throttle(d);
    }
    set_turn(tn);  // +0x28
    if (!jump_held || jump_in != 0.0f) set_jump(jump_in);  // +0x34
    ease(pitch, p, dt);
    ease(twist, tw, dt);
    jj_strafe = strafe;
    jj_forward = ahead;
  }
  MechControls::update(dt);
}

// Mech3.exe 1.2 @ 0x0046a860: not powered down (vt+0xa0), 0x1b..0x1e pick
// lancemates 1..4 (0x00469060), 0x1f..0x23 nothing, the rest the player's.
void PocketControls::command(int32_t cmd) {
  Mech* m = owner;
  if (!m || m->is_powered_down()) return;
  if (cmd >= 0x1b && cmd <= 0x1e) {
    if (g_calls.panel_pick) g_calls.panel_pick(*this, cmd - 0x1a);
    else if (panel) static_cast<PocketPanel*>(panel)->pick(cmd - 0x1a);
    return;
  }
  if (cmd >= 0x1f && cmd <= 0x23) return;
  if (g_calls.player_keys) g_calls.player_keys(*this, cmd);
  else PlayerControls::command(cmd);  // 0x004274d0
}

// The way out (0x00469940's and 0x0046ad80's block): the instance
// [0x006056ec] given back, the old controls' owner the mech (their
// IControls vt+0x08) and their control mode set again (0x00426650 on their
// +0x08), the view's pocket off (0x004b5290(0)), the camera mode given back
// (0x0040ae70), the HUD's flag off, the inputs copied back.
MechControls* PocketControls::leave(bool free_self) {
  Mech* m = owner;
  MechControls* p = prev;
  if (!m) return this;
  player_controls_instance() = prev_instance;
  g_globals.active = nullptr;
  if (p) {
    p->set_owner(m);
    p->set_mode(p->flags);
  }
  if (g_calls.view_pocket) g_calls.view_pocket(false);
  else note("0x004b5290", 0);
  if (g_calls.set_camera_mode) g_calls.set_camera_mode(saved_camera);
  if (g_calls.hud_mode) g_calls.hud_mode(*m, false);
  if (p) {
    p->throttle_returns = throttle_returns, p->throttle_held = throttle_held;
    p->throttle_target = throttle_target, p->x5a = x5a, p->joy_throttle = joy_throttle;
    p->reverse = reverse, p->throttle = throttle, p->jump = jump, p->jj_forward = jj_forward;
    p->jj_strafe = jj_strafe, p->turn = turn, p->pitch = pitch, p->twist = twist, p->fire = fire;
    p->joy_throttle_set = joy_throttle_set;
  }
  if (free_self) delete this;  // vt+0x00(1)
  return p;
}

// Mech3.exe 1.2 @ 0x00469cf0: 0x00469930 (the dispatcher with its stack
// argument 1, the mode order 2 makes; lance_dispatch's) on the mode's order
// (+0x160: 2, or the one an order's line put there): the order to the
// picked, and the mode left either way.
MechControls* PocketControls::unwrap() { return lance_dispatch(this, order); }

// Mech3.exe 1.2 @ 0x0046ad80 (the mech going, 0x0041520b): the mode left,
// then the old controls asked the same (and deleted when they say so).
// FIX (bug): the exe deletes itself here and answers yes, so the mech, whose
// +0x37c still holds it (the old controls' vt+0x08 sets only their owner),
// deletes it again; here it answers yes and stays for the mech to delete.
bool PocketControls::release() {
  MechControls* p = owner ? leave(false) : prev;
  if (p && p != this && p->release()) delete p;
  prev = nullptr;
  return true;
}

// Mech3.exe 1.2 @ 0x0046abb0 (IControls vt+0xa8, the mech's death): the
// mode left and deleted, the old controls the mech's (0x00491120).
void PocketControls::on_death() {
  Mech* m = owner;
  MechControls* p = leave(false);
  if (m && p && p != this) {
    m->controls = p;
    p->set_owner(m);
    delete this;
  }
}

// Mech3.exe 1.2 @ 0x00469940 case 2 (not in the mode): none without a mech;
// the instance [0x006056ec] taken and the mode made over the controls (none
// made: the instance back); it the instance and the mech's owner (its
// IControls vt+0x08), the view's pocket on (0x004b5290(1)), the camera mode
// kept (+0x150) and set to 5 (0x0040ae70), the view state 2 (0x00426780),
// the HUD's flag on, [0x00605f64] the mode.
MechControls* pocket_enter(MechControls* c, uint32_t mode) {
  Mech* m = c ? c->owner : nullptr;
  if (!m) return c;
  PlayerControls* inst = player_controls_instance();
  player_controls_instance() = nullptr;
  auto* p = new PocketControls(c, inst, 2, mode);
  player_controls_instance() = p;
  p->set_owner(m);
  if (g_calls.view_pocket) g_calls.view_pocket(true);
  else note("0x004b5290", 1);
  p->saved_camera = g_calls.camera_mode ? g_calls.camera_mode() : -1;
  if (g_calls.set_camera_mode) g_calls.set_camera_mode(5);
  if (g_calls.view_state) g_calls.view_state(*p, 2);
  else p->set_view(2);  // 0x00426780
  if (g_calls.hud_mode) g_calls.hud_mode(*m, true);
  g_globals.active = p;
  return p;
}

}  // namespace mech3
