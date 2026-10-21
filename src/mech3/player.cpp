#include "mech3/player.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zutil/zfix.h"
#include "mech3/zrdraiv.h"

// The mission start's vehicles and the player's tick; docs/mission-start.md.
namespace mech3 {

namespace {

// What a front end leaves unwired: the silent reads give nothing.
class DefaultPlayerHooks : public PlayerHooks {
 public:
  float reader_item_float(Handle, int32_t) override { return 0; }
  Handle reader_item(Handle, int32_t) override { return 0; }
  int32_t dfn_config_count(Handle) override { return 0; }
  Handle dfn_config(Handle, int32_t) override { return 0; }
  int32_t dfn_skin_count(Handle) override { return 0; }
  Handle dfn_skin(Handle, int32_t) override { return 0; }
  void set_field32(Handle, uint32_t, uint32_t) override {}
  uint32_t field32(Handle, uint32_t) override { return 0; }
  Handle controls_interface(Handle) override { return 0; }
  float frame_dt() override { return 0; }
  Handle pilot_vehicle() override { return 0; }
  Handle view() override { return 0; }
  Handle view_target(Handle) override { return 0; }
  Handle stealth() override { return 0; }
  void clear_6099c0() override {}
};

PlayerState g_state;
DefaultPlayerHooks g_default_hooks;
PlayerHooks* g_hooks = &g_default_hooks;

void report(int line, const char* fmt, ...) {
  char text[512];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(text, sizeof text, fmt, ap);
  va_end(ap);
  g_hooks->report(0x200, line, text);
}

// A vehicle's net by its record's first item: znet's numbered object, then
// the compiled net it holds (ai.md section 9).
Handle mech_net(int32_t id) {
  PlayerHooks& h = *g_hooks;
  const Handle o = h.znet_find(id);
  if (!o) return 0;
  const Handle x = h.net_object(o);
  return x ? h.net_compiled(x) : 0;
}

// A controller's IControls virtual base deleted (scalar deleting dtor, 1).
void delete_controls(Handle ctl) {
  if (!ctl) return;
  if (const Handle i = g_hooks->controls_interface(ctl)) g_hooks->controls_delete(i);
}

}  // namespace

PlayerState& player_state() { return g_state; }
void player_clear_60b56c() { g_state.x60b56c = 0; }
void player_set_60b56c(uint8_t v) { g_state.x60b56c = v; }
uint8_t player_get_60b56c() { return g_state.x60b56c; }
void set_player_hooks(PlayerHooks* h) { g_hooks = h ? h : &g_default_hooks; }
PlayerHooks& player_hooks() { return *g_hooks; }

ai::AivRecord* PlayerHooks::aiv_read_named(ai::AivRecord& rec, const char* name) { return rec.read_named(name); }
ai::AivRecord* PlayerHooks::aiv_read(ai::AivRecord& rec, int32_t i, const char** name) { return rec.read(i, name); }
int32_t PlayerHooks::aiv_count() { return ai::aiv_file().record_count(); }
Handle PlayerHooks::rec_handle(const ai::AivRecord*) { return 0; }
uint32_t PlayerHooks::rec_yaw(const ai::AivRecord* rec) { return rec->get<uint32_t>(0x14); }

// Mech3.exe 1.2 @ 0x0049f924 (inline, also 0x0049fbe1 and the launch's
// 0x00491262): the AiTarget masks from aiv.zrd's team, a 5-bit team number
// in each 5 bits of it; allies are the teams but bit 0, enemies the rest.
void set_team_masks(Handle vehicle, uint32_t team) {
  uint32_t mask = 1u << (team & 31);
  for (uint32_t t = team >> 5; t; t >>= 5) mask |= 1u << (t & 31);
  g_hooks->set_field32(vehicle, 0xc0, mask);
  const uint32_t allies = mask & ~1u;
  g_hooks->set_field32(vehicle, 0xc4, allies);
  g_hooks->set_field32(vehicle, 0xc8, ~allies & ~1u);
  g_state.x60b56c = 0;
}

// Mech3.exe 1.2 @ 0x0049e810: as the mission comes up (0x00490ab0), every
// aiv.zrd record but the player and the stealth entity becomes a vehicle, or
// for a campaign a lancemate slot (made at launch, 0x0049e5e0). `full` is
// the app's +0x1b2.
void player_setup(bool full) {
  PlayerHooks& h = *g_hooks;
  PlayerState& s = g_state;
  zmod::zones().mode = 1;  // [0x005ea1d0]
  if (full) h.nets_build_all();
  h.nets_begin();
  s.tick = h.object3d_create();
  h.set_action_priority(s.tick, 2);
  h.set_action(s.tick, kPlayerTick);
  const Handle player_zrd = h.reader_load("player.zrd");
  Handle n = h.reader_find(player_zrd, "nom_gravity");
  s.gravity = n ? h.reader_item_float(n, 1) : 60.0f;
  n = h.reader_find(player_zrd, "wat_gravity");
  s.water_gravity = n ? h.reader_item_float(n, 1) : s.gravity * 0.33333334f;
  n = h.reader_find(player_zrd, "qsd_gravity");
  s.quicksand_gravity = n ? h.reader_item_float(n, 1) : s.gravity * 0.16666667f;
  h.reader_free(player_zrd);
  if (!full) return;
  const int32_t type = h.game_type();
  if (type == 3 || type == 2) return;  // multiplayer and instant action place their own (0x0049dc80)

  const Handle vehicles = h.reader_load("vehicle.zrd");
  ai::AivRecord rec;
  ai::AivRecord* player = h.aiv_read_named(rec, "player");
  const int32_t count = h.aiv_count();
  int32_t player_team = 0;
  const Handle campaign = h.campaign();
  const Handle training = h.training();
  if (player) {
    player_team = player->team();
    // Training: the player's mech takes the record's configuration (p11).
    if (training) {
      if (const Handle dfn = h.mech_dfn(player->get<int32_t>(0xc4))) {
        const uint32_t i = player->get<uint32_t>(0xbc);
        const uint32_t configs = static_cast<uint32_t>(h.dfn_config_count(dfn));
        const Handle cfg = i < configs && static_cast<int32_t>(i) >= 0 ? h.dfn_config(dfn, static_cast<int32_t>(i)) : 0;
        if (cfg) h.slot_set_config(h.training_slot(training), cfg);
      }
    }
  }
  int32_t mates = 0;
  for (int32_t i = 0; i < count; ++i) {
    const char* name = nullptr;
    ai::AivRecord* r = h.aiv_read(rec, i, &name);
    if (!r || std::strcmp(name, "player") == 0 || std::strcmp(name, "stealth") == 0) continue;
    // The type is the name up to its first '_', the rest the suffix.
    char type_buf[0x100], name_buf[0x100];
    uint8_t frame[0x14 + 0x1c + 0xd0];
    char* type = type_buf;
    char* full_name = name_buf;
    const bool whole = zfix::options().bugs;
    if (whole) {
      // FIX (bug): whole names. The exe's buffers are 20 and 28 bytes, then
      // this record (docs/mission-start.md, Quirks).
      std::snprintf(type_buf, sizeof type_buf, "%s", name);
      std::snprintf(name_buf, sizeof name_buf, "%s", name);
    } else {
      // The exe's frame: the type's 20 bytes, the name's 28, the record (its
      // vtable, then AiParams +0x04..). A name of 20 or more spills from the
      // type into the name, of 28 or more into the record's first fields.
      std::memcpy(frame + 0x30, r->raw, 0xd0);
      full_name = reinterpret_cast<char*>(frame + 0x14);
      std::snprintf(full_name, sizeof frame - 0x14, "%s", name);  // FIX (bug): as text, not as the format
      type = reinterpret_cast<char*>(frame);
      std::memmove(type, full_name, std::strlen(full_name) + 1);
    }
    char* suffix = std::strchr(type, '_');
    if (suffix) *suffix++ = '\0';
    if (!whole) std::memcpy(r->raw + 4, frame + 0x34, 0xcc);
    int32_t mech = r->get<int32_t>(0xc4);
    mech = mech > 0x12 ? 0x12 : (mech < -1 ? -1 : mech);
    // A campaign's lancemate the player configures (pilot can_configure,
    // +0xcc bit 7) on the player's team is a slot of the campaign, kept
    // from mission to mission; the mech is made at launch.
    if (campaign && mech != -1 && player_team != 0 && player_team == r->team() && (r->get<uint8_t>(0xcc) & 0x80)) {
      const Handle mem = h.alloc(0x18);
      const Handle slot = mem ? h.lancemate_init(mem, mech) : 0;
      if (!slot) continue;  // FIX (bug): the exe writes through the null slot
      h.set_field32(slot, 0x14, static_cast<uint32_t>(i));
      h.campaign_set_lancemate(campaign, mates++, slot);
      continue;
    }
    create_vehicle(h.reader_find(vehicles, type), r, type, full_name, suffix);
  }
  if (campaign) h.campaign_set_lancemates(campaign, mates);
  h.reader_free(vehicles);
  h.nets_end();
}

// Mech3.exe 1.2 @ 0x0049fce0: a record with a mech (p12, else the type as a
// mech name) is an AI mech; otherwise a vehicle of vehicle.zrd's `type`,
// AI-driven when its net exists or its suffix starts "mech".
bool create_vehicle(Handle def, ai::AivRecord* rec, const char* type, const char* name, const char* suffix) {
  PlayerHooks& h = *g_hooks;
  if (!rec || std::strcmp(name, "player") == 0 || std::strcmp(name, "stealth") == 0) return false;
  int32_t mech = rec->get<int32_t>(0xc4);
  if (mech == -1) mech = h.mech_index(type);
  if (mech != -1) return create_ai_mech(type, name, mech, rec, 0) != 0;
  if (!mech_net(rec->kind()) && (!suffix || std::strncmp(suffix, "mech", 4) != 0)) {
    if (!def) return false;
    Handle out = 0;
    return create_basic_vehicle(def, rec, type, name, 0, &out) != 0;
  }
  return create_controlled_vehicle(def, type, name, rec) != 0;
}

// Mech3.exe 1.2 @ 0x0049f800 (the exe's second argument, -1, is not read).
int32_t create_controlled_vehicle(Handle def, const char* type, const char* name, ai::AivRecord* rec) {
  if (!def || !rec) return 0;
  const Handle net = mech_net(rec->kind());
  // FIX (bug): without a net the exe passes the record as the controller
  // (the vehicle then takes rec+0x214 as its animation slot, and a failed
  // build deletes the record as one). No controller is meant.
  Handle ctl = 0;
  if (net)
    ctl = make_vehicle_controls(net, rec);
  else if (!zfix::options().bugs)
    ctl = g_hooks->rec_handle(rec);
  Handle out = 0;
  const int32_t r = create_basic_vehicle(def, rec, type, name, ctl, &out);
  if (!r) delete_controls(ctl);
  return r;
}

// Mech3.exe 1.2 @ 0x0049f8a0: the vehicle, named, on its team, placed, set up
// from vehicle.zrd's `common_mode` and its mode list (the def's item 4).
int32_t create_basic_vehicle(Handle def, ai::AivRecord* rec, const char* type, const char* name, Handle ctl,
                             Handle* out) {
  PlayerHooks& h = *g_hooks;
  const Handle v = std::strncmp(type, "mobilehq", 9) == 0 ? h.mobilehq_create(ctl, type, name)
                                                           : h.vehicle_create(ctl, type, name);
  *out = v;
  if (!v) {
    report(0x423, "failed to create basic vehicle: type \"%s\", name \"%s\"", type, name);
    return 0;
  }
  h.entity_name(v, name);
  set_team_masks(v, static_cast<uint32_t>(rec->team()));
  if (!ctl) ctl = h.field32(v, 0xe4);  // the vehicle's own controls
  const Handle common = h.reader_find(def, "common_mode");
  h.vehicle_place(v, rec);
  h.vehicle_setup(v, common, name, rec->kind());
  // 0x0049ec90: the slot is in the controller (+0x214), else a scratch word
  // of this frame (0 here).
  const Handle controls = h.field32(v, 0xe4);
  h.vehicle_anims(controls ? controls + 0x764 : 0, h.field32(v, 4), h.reader_item(def, 4), name, type,
                  ctl ? ctl + 0x214 : 0);
  h.vehicle_49f660(v);
  h.vehicle_touch(v, h.field32(v, 4));
  h.vehicle_5a85c0(v);
  return 1;
}

// Mech3.exe 1.2 @ 0x0049fa20: an AI mech: its controller over the net (the
// record's, if none given), the mech, its skin (+0xc8, 1..7), its team, on
// the ground at the record's place. p12's second flag (+0xcc bit 5) starts
// it powered down, after one 0.1 s update with its controller held.
Handle create_ai_mech(const char* type, const char* name, int32_t mech, ai::AivRecord* rec, Handle net) {
  PlayerHooks& h = *g_hooks;
  if (!rec) return 0;
  if (!net) net = mech_net(rec->kind());
  Handle ctl = 0;
  if (!net) {
    report(0x459, "no mech net %d found for %s (%d)", rec->kind(), type, mech);
  } else {
    ctl = make_mech_controls(net, rec);
    if (!ctl) report(0x460, "failed to create controller for %s (%d)", type, mech);
  }
  Handle m = 0;
  if (ctl) {
    m = h.mech_create(mech, ctl, rec->get<int32_t>(0xbc), name);
    if (!m) {
      report(0x468, "failed to create mech for %s (%d)", type, mech);
    } else {
      const uint32_t skin = rec->get<uint32_t>(0xc8);
      if (skin != 0 && skin < 8) {
        const Handle dfn = h.mech_dfn_of(m);
        const int32_t skins = h.dfn_skin_count(dfn);
        const Handle sk = static_cast<int32_t>(skin) < skins ? h.dfn_skin(dfn, static_cast<int32_t>(skin)) : 0;
        if (sk) h.mech_set_skin(m, sk);
      }
    }
  }
  if (!m) {
    report(0x475, "ignoring %s (%d)", type, mech);
    delete_controls(ctl);
    return 0;
  }
  set_team_masks(m, static_cast<uint32_t>(rec->team()));
  if (!h.mech_place(m, rec)) {
    delete_controls(ctl);
    h.mech_delete(m);
    return 0;
  }
  if (rec->get<uint8_t>(0xcc) & 0x20) {
    if (const Handle o = h.controls_vt90(h.controls_interface(ctl))) {
      const uint32_t held = h.field32(o, 0x88);
      h.set_field32(o, 0x88, 1);
      h.mech_update(m, 0.1f);
      h.set_field32(o, 0x88, held);
    }
    h.mech_power(m, 1);
  }
  return m;
}

// Mech3.exe 1.2 @ 0x0049fe90: the AI mech controls (0x320 bytes, 0x004d8750).
Handle make_mech_controls(Handle net, ai::AivRecord* rec) {
  const Handle mem = g_hooks->alloc(800);
  return mem ? g_hooks->ai_mech_controls(mem, net, rec) : 0;
}

// Mech3.exe 1.2 @ 0x0049ff10: the AI vehicle controls (0xcd8 bytes, 0x004e71c0).
Handle make_vehicle_controls(Handle net, ai::AivRecord* rec) {
  const Handle mem = g_hooks->alloc_4ac860(0xcd8);
  return mem ? g_hooks->ai_vehicle_controls(mem, net, rec) : 0;
}

// Mech3.exe 1.2 @ 0x004a1160: the tick object's action (priority 2), once a
// frame: the player's vehicle, the stealth entity when the view looks at it,
// every other entity's frame (0x004a1140), the HUD's view. A frame time of 0
// (or NaN, the x87 compare says equal) gives an inverse of 10000.
namespace {
uint32_t g_ticks = 0;
}  // namespace
uint32_t player_tick_count() { return g_ticks; }

void player_tick() {
  ++g_ticks;
  PlayerHooks& h = *g_hooks;
  PlayerState& s = g_state;
  s.dt = h.frame_dt();
  s.inv_dt = !(s.dt < 0 || s.dt > 0) ? 10000.0f : 1.0f / s.dt;
  h.vehicle_update(h.pilot_vehicle(), s.dt);
  const Handle st = h.stealth();
  if (h.view_target(h.view()) == st) h.stealth_update(st, s.dt);
  h.clear_6099c0();
  h.entities_update(s.dt);
  h.tick_41e3f0(s.dt);
  const int32_t type = h.game_type();
  if (type == 2 || type == 3) h.tick_537f20(s.x5e8118);
  h.vehicle_41aba0(h.pilot_vehicle(), s.dt);
  h.view_tick(h.view(), s.dt);
}

}  // namespace mech3
