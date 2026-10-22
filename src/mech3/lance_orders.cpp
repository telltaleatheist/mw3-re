#include "mech3/lance_orders.h"

#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_control.h"
#include "mech3/mech_pocket.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_target.h"

namespace mech3 {
namespace {
LanceCalls g_calls;

int32_t give(ai::Brain& b, int32_t order, ai::Contact* c) {
  return g_calls.order ? g_calls.order(b, order, c) : b.lance_order(order, c);
}

// 0x00469400 / 0x00469650's own path (not in the command mode): the
// player's saved target or last ally (0x004d7240) when its mech (0x004e6ff0)
// is a lancemate under orders (+0xbc bit 4), its controls (vt+0x140) and
// their brain (vt+0x54).
ai::Brain* selected_brain(Mech& player, Mech** mate) {
  ai::Contact* sel = player.saved_or_ally();
  Mech* m = sel && collide_calls().contact_mech ? collide_calls().contact_mech(sel) : nullptr;
  if (!m || !(m->xbc & 0x10)) return nullptr;
  *mate = m;
  if (g_calls.brain) return g_calls.brain(*m);
  auto* ac = dynamic_cast<AiControls*>(m->controls);
  return ac ? ac->brain.get() : nullptr;
}

// 0x004e6ee0: a contact from `from` on `to` (the template 0x005e9d10: no
// distance yet), its squared distance and the way to it.
ai::Contact contact_on(Mech& from, Mech& to) {
  ai::BrainHooks& h = ai::brain_hooks();
  ai::Contact c;
  c.owner = h.vehicle_handle(&from);
  c.entity = h.vehicle_handle(&to);
  c.seen = c.entity;  // vt+0x24: a mech answers itself
  float a[3], b[3];
  h.position(c.owner, a);  // 0x005a85e0
  h.position(c.entity, b);
  const float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
  c.dist2 = (dx * dx + dy * dy) + dz * dz;  // the exe's order of the sums
  c.delta[0] = dx, c.delta[1] = dy, c.delta[2] = dz;
  return c;
}

// Mech3.exe 1.2 @ 0x00469400: the order on the lancemate's contact on the
// player (+0x98), when it has one.
void to_follow(Mech& player, int32_t order) {
  Mech* mate = nullptr;
  ai::Brain* b = selected_brain(player, &mate);
  if (b && mate->player_contact) give(*b, order, mate->player_contact);
}

// Mech3.exe 1.2 @ 0x00469650: the order on a contact from the lancemate on
// the player's target's mech (+0x6c, 0x004e6fe0); 0x00468ca0 (a network
// game's team change for orders 10 and 11) does nothing outside one.
void to_target(Mech& player, int32_t order) {
  Mech* mate = nullptr;
  ai::Brain* b = selected_brain(player, &mate);
  Mech* t = player.target && collide_calls().contact_mech ? collide_calls().contact_mech(player.target) : nullptr;
  if (!b || !t) return;
  ai::Contact c = contact_on(*mate, *t);
  give(*b, order, &c);
}

}  // namespace

LanceCalls& lance_calls() { return g_calls; }

// 0x00469400 / 0x00469650 in the pocket commander: the order to the
// lancemates picked (0x00468f00, or 0x00468fa0 on the player's target's
// entity (0x004e6fe0), else the panel's cursor's (its vt+0x2c)), then the
// mode left (none without a mech: the mode kept).
MechControls* in_pocket(PocketControls& p, int32_t order, bool on_target) {
  PocketCalls& pc = pocket_calls();
  Mech* player = p.owner;
  if (player) {
    uint32_t target = 0;
    if (on_target) {
      ai::BrainHooks& h = ai::brain_hooks();
      target = player->target ? h.contact_entity(player->target) : pocket_globals().cursor;
    }
    if (!on_target || target) {
      if (pc.panel_order) pc.panel_order(p, order, target);
      else if (pc.unported) pc.unported(on_target ? "0x00468fa0" : "0x00468f00", static_cast<uint32_t>(order));
    }
  }
  return p.leave(true);
}

// Mech3.exe 1.2 @ 0x00469940
MechControls* lance_dispatch(MechControls* c, int32_t order) {
  if (!c) return nullptr;
  Mech* player = c->owner;  // IControls +0x10, vt+0x50
  if (!player) return c;
  PocketControls* p = c->pocket();  // primary vt+0x08
  switch (order) {
    case 0: case 1: case 7: case 8: case 9: case 12:
      if (p) return in_pocket(*p, order, false);
      to_follow(*player, order);
      break;
    case 3: case 4: case 5: case 6: case 10: case 11:
      if (p) return in_pocket(*p, order, true);
      to_target(*player, order);
      break;
    case 2: return p ? p->leave(true) : pocket_enter(c, 1);  // 0x00469930's mode, 1
    default: break;
  }
  return c;
}

}  // namespace mech3
