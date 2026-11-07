#include "mech3/lance_orders.h"

#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_control.h"
#include "mech3/mech_pocket.h"

#include "mech3/zai_brain.h"
#include "mech3/zai_target.h"

namespace mech3 {

void player_clear_60b56c();  // player.h ([0x0060b56c] = 0), not includable with mech.h

namespace {
LanceCalls g_calls;

int32_t give(ai::Brain& b, int32_t order, ai::Contact* c) {
  return g_calls.order ? g_calls.order(b, order, c) : b.lance_order(order, c);
}

// 0x00469400 / 0x00469650's own path (not in the command mode): the
// player's saved target or lancemate (0x004e76e0) when its mech (0x004e6ff0)
// is a lancemate under orders (+0xbc bit 4), its controls (vt+0x140) and
// their brain (vt+0x54).
ai::Brain* selected_brain(Mech& player, Mech** mate) {
  // Pirate's Moon 1.0 @ 0x004e76e0: the saved target, else the target when it's a lancemate under orders.
  ai::Contact* sel = player.saved_target;
  if (!sel && player.target) {
    ai::BrainHooks& h = ai::brain_hooks();
    const zgame::Handle v = h.contact_vehicle(player.target);
    const ai::AiTarget* t = v ? h.targetable(v) : nullptr;
    if (t && (t->xbc & 0x10)) sel = player.target;
  }
  Mech* m = sel && collide_calls().contact_mech ? collide_calls().contact_mech(sel) : nullptr;
  if (!m || !(m->xbc & 0x10)) return nullptr;
  *mate = m;
  if (g_calls.brain) return g_calls.brain(*m);
  auto* ac = dynamic_cast<AiControls*>(m->controls);
  return ac ? ac->brain.get() : nullptr;
}

// 0x004e6ee0: a contact from `from` on `to` (the template 0x005e9d10: no
// distance yet), its squared distance and the way to it.
ai::Contact contact_on(Mech& from, zgame::Handle to) {
  ai::BrainHooks& h = ai::brain_hooks();
  ai::Contact c;
  c.owner = h.vehicle_handle(&from);
  c.entity = to;
  c.seen = c.entity;  // vt+0x24: a mech answers itself
  float a[3], b[3];
  h.position(c.owner, a);  // 0x005a85e0
  h.position(c.entity, b);
  const float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
  c.dist2 = static_cast<float>((double{dx} * dx + double{dy} * dy) + double{dz} * dz);  // held in the register, then stored
  c.delta[0] = dx, c.delta[1] = dy, c.delta[2] = dz;
  return c;
}

// Pirate's Moon 1.0 @ 0x0046a750 (MW3's 0x00468ca0): the object of an order. On the command marker
// ([0x00629a2c]), "only an enemy" (10) becomes "only an ally" (11), and with either the lancemate's own
// marker (the brain's +0x2cc) takes team bit 30, else bit 0; it goes to the marker's place, is shown, and
// is the order's object. On anything else it's hidden and the order keeps its object.
uint32_t order_object(ai::Brain& b, uint32_t target, int32_t* order) {
  const uint32_t own = g_calls.own_marker ? g_calls.own_marker(b) : b.own_marker;
  ai::AiTarget* t = own ? ai::brain_hooks().targetable(own) : nullptr;
  const bool on = target == pocket_globals().marker;
  if (on && *order == 10) *order = 11;
  if (t) {
    t->team = on && *order == 11 ? 1u << 30 : 1u;
    t->allies = t->team & 0xfffffffeu;
    t->enemies = ~t->allies & 0xfffffffeu;
    player_clear_60b56c();
  }
  if (!own) return target;
  if (on && own != target && g_calls.own_marker_place) g_calls.own_marker_place(own, target);
  if (g_calls.own_marker_show) g_calls.own_marker_show(own, on);
  return on ? own : target;
}

// Mech3.exe 1.2 @ 0x00469400: the order on the lancemate's contact on the
// player (+0x98), when it has one.
void to_follow(Mech& player, int32_t order) {
  Mech* mate = nullptr;
  ai::Brain* b = selected_brain(player, &mate);
  if (!b || !mate->player_contact) return;
  order_object(*b, ai::brain_hooks().contact_vehicle(mate->player_contact), &order);  // Pirate's Moon 0x0046af20
  give(*b, order, mate->player_contact);
}

// Mech3.exe 1.2 @ 0x00469650: the order on a contact from the lancemate on
// the player's target's vehicle (+0x6c, 0x004e6fe0: what the contact sees,
// any vehicle, a mech or not); 0x00468ca0 (a network game's team change for
// orders 10 and 11) does nothing outside one. Until 2026-10-30 (playtest
// W11) the port took the target's mech (0x004e6ff0), so an order on a
// vehicle, a turret or a building was dropped.
void to_target(Mech& player, int32_t order) {
  Mech* mate = nullptr;
  ai::Brain* b = selected_brain(player, &mate);
  const zgame::Handle t = player.target ? ai::brain_hooks().contact_vehicle(player.target) : 0;
  if (!b || !t) return;
  ai::Contact c = contact_on(*mate, order_object(*b, t, &order));  // Pirate's Moon 0x0046b1b0
  give(*b, order, &c);
}

}  // namespace

LanceCalls& lance_calls() { return g_calls; }

namespace {

}  // namespace

// Mech3.exe 1.2 @ 0x00468f00 / 0x00468fa0 (with 0x00468bd0 / 0x00468df0's
// record and each unit's 0x00468c00 / 0x00468e20)
int32_t pocket_order(PocketControls& p, int32_t order, uint32_t target, bool on_target) {
  PocketCalls& pc = pocket_calls();
  PocketUnit units[32];
  const int32_t n = pc.panel_units ? pc.panel_units(p, units, 32) : pocket_panel_units(p, units, 32);
  if (n <= 0) return 2;
  bool taken = false, refused = false;
  for (int32_t i = 0; i < n; ++i) {
    const PocketUnit& u = units[i];
    if (!u.picked || !u.mech) continue;
    ai::Brain* b = g_calls.brain ? g_calls.brain(*u.mech) : nullptr;
    if (!g_calls.brain)
      if (auto* ac = dynamic_cast<AiControls*>(u.mech->controls)) b = ac->brain.get();
    if (!b) continue;
    int32_t r;
    if (on_target) {
      ai::Contact c = contact_on(*u.mech, order_object(*b, target, &order));  // Pirate's Moon 0x0046a9c0
      r = give(*b, order, &c);
    } else {
      if (!u.mech->player_contact) continue;
      order_object(*b, ai::brain_hooks().contact_vehicle(u.mech->player_contact), &order);  // 0x0046a8e0
      r = give(*b, order, u.mech->player_contact);
    }
    if (r) taken = true;
    else refused = true;
  }
  return taken ? 1 : refused ? 0 : 2;
}

namespace {

// 0x00469400 / 0x00469650 in the pocket commander: the order to the
// lancemates picked (0x00468f00, or 0x00468fa0 on the player's target's
// entity (0x004e6fe0), else the panel's cursor's (its vt+0x2c)), then the
// mode left (none without a mech: the mode kept).
MechControls* in_pocket(PocketControls& p, int32_t order, bool on_target) {
  Mech* player = p.owner;
  if (player) {
    uint32_t target = 0;
    if (on_target) {
      ai::BrainHooks& h = ai::brain_hooks();
      target = player->target ? h.contact_entity(player->target) : pocket_globals().cursor;
    }
    if (!on_target || target) pocket_order(p, order, target, on_target);
  }
  return p.leave(true);
}

}  // namespace

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
