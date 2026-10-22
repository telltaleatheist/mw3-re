#include "mech3/mech_pocket.h"

#include "mech3/lance_orders.h"
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
    note("0x0046b0a0");                  // the mode's own keys and camera (L4c2c)
  }
  if (!(x154 & 1)) {
    set_jump(0);  // IControls vt+0x34
    jj_strafe = 0, jj_forward = 0;  // vt+0x44, vt+0x3c
    fire = 0;     // +0x34
  }
  return 1;
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

// Mech3.exe 1.2 @ 0x00469cf0: the dispatcher on the mode's order (+0x160:
// 2, or the one an order's line put there): the order to the picked, and
// the mode left either way.
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
