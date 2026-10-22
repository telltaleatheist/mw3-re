// The mech's command handler (mech3/mech_command.cpp, 0x0041be80) and the
// heat keys' 0x00419190 / 0x00419e60 for tests/differential/test_command.py.
// Every call the handler makes out is logged as (code, a, b) with pointers
// as the emulator's addresses (tok[]); the mech's own calls come back through
// CommandCalls::own with the scenario's answers.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_command.h"
#include "mech3/mech_control.h"
#include "mech3/mech_move.h"
#include "mech3/weapon.h"

using namespace mech3;

namespace {

// Codes for the calls that are hooks in the port (the rest log their address).
enum : int32_t {
  kHud = 0x1000, kPlay = 0x1001, kMechSound = 0x1002, kCamera = 0x1003, kOther = 0x1004, kTeam = 0x2001,
  kRefuses = 0x2002
};

enum In {
  kCmd, kActive, kViewed, kScreen, kPowered, kCrouched, kAms, kXbc, kRange, kEq6, kEq12, kMasc, kTarget, kSaved,
  kView, kMode, kGroup, kCurrent, kSelect, kJettison, kHudWeapon, kHudWeaponGroup, kLanceBusy, kAllies, kAim,
  kEntity, kTeamIn, kRefuse, kContacts, kSeen0, kStations = kSeen0 + 4, kInCount
};
enum Tok {
  tWm, tWeapon, tGroup0, tScreen = tGroup0 + 5, tCurrent, tMech, tSound, tControls, tNode, tContact0,
  tEntity0 = tContact0 + 4, tTokCount = tEntity0 + 3
};
constexpr uint32_t kElse = 0x7f00;  // test_command.py's ELSE

std::vector<int32_t> g_log;
const int32_t* g_in;
const uint32_t* g_tok;
WeaponManager* g_wm;
Weapon* g_weapon;
ai::Contact g_contacts[4];
MechControls* g_controls;

void log(int32_t code, uintptr_t a, uintptr_t b) {
  g_log.insert(g_log.end(), {3, code, static_cast<int32_t>(a), static_cast<int32_t>(b)});
}

// The port's pointers as the emulator's addresses.
uintptr_t tok(uintptr_t v) {
  if (v == reinterpret_cast<uintptr_t>(g_wm)) return g_tok[tWm];
  if (v == reinterpret_cast<uintptr_t>(g_weapon)) return g_tok[tWeapon];
  for (int k = 0; k < 5; ++k)
    if (v == reinterpret_cast<uintptr_t>(g_wm->groups[k])) return g_tok[tGroup0 + k];
  for (int k = 0; k < 4; ++k)
    if (v == reinterpret_cast<uintptr_t>(&g_contacts[k])) return g_tok[tContact0 + k];
  return v;
}

uintptr_t own(uint32_t addr, uintptr_t a, uintptr_t b) {
  log(static_cast<int32_t>(addr), tok(a), tok(b));
  switch (addr) {
    case 0x41cc10: return static_cast<uintptr_t>(g_in[kPowered]);
    case 0x4b0730: return static_cast<uintptr_t>(g_in[kSelect]);
    case 0x4b04c0: return g_in[kCurrent] ? g_tok[tCurrent] : 0;
    case 0x4b0ae0: return static_cast<uintptr_t>(g_in[kJettison]);
    default: return 0;
  }
}

template <typename T> T* ptr(uint32_t v) { return reinterpret_cast<T*>(static_cast<uintptr_t>(v)); }

}  // namespace

extern "C" {

// in: the In fields; tok: the emulator's addresses (Tok). out: xbc,
// saved_target, the AMS's on, the manager's mode, the view option, the
// controls, sounds[23] (+0x614). Returns the log's length.
int32_t probe_command(const int32_t* in, const uint32_t* t, int32_t* out, int32_t* log_out, int32_t cap) {
  g_in = in, g_tok = t, g_log.clear();
  const CommandCalls saved_calls = command_calls();
  const HeatCalls saved_heat = heat_calls();
  auto saved_view = update_calls().controls_view;
  Mech* saved_player = world().player;
  static int32_t option;
  option = -1;

  Mech m;
  WeaponManager wm(nullptr);
  Weapon weapon(static_cast<WeaponDef*>(nullptr), nullptr, &wm, true);  // a beam: no ammo bin
  Ams ams(0.0f, 0.0f, 0);
  MechControls controls;
  g_wm = &wm, g_weapon = &weapon, g_controls = &controls;
  m.active = in[kActive] != 0;
  m.crouched = in[kCrouched] != 0;
  m.xbc = static_cast<uint8_t>(in[kXbc]);
  std::memcpy(&m.sensor_range, &in[kRange], 4);
  m.equipment[6] = in[kEq6], m.equipment[12] = in[kEq12];
  std::memcpy(&m.masc_time, &in[kMasc], 4);
  m.target = in[kTarget] >= 0 ? &g_contacts[in[kTarget]] : nullptr;
  m.allies = static_cast<uint32_t>(in[kAllies]);
  for (int k = 0; k < 4; ++k) {
    g_contacts[k].seen = in[kSeen0 + k] ? t[tEntity0 + in[kSeen0 + k] - 1] : kElse;
    if (k < in[kContacts]) m.contacts.push_back(&g_contacts[k]);
  }
  m.saved_target = ptr<ai::Contact>(static_cast<uint32_t>(in[kSaved]));
  m.ammo = &wm;
  ams.on = in[kAms] == 2;
  m.ams = in[kAms] ? &ams : nullptr;
  controls.view = in[kView];
  m.controls = &controls;
  wm.mode = in[kMode], wm.group = in[kGroup];
  weapon.group = in[kHudWeaponGroup];
  world().player = in[kViewed] ? &m : nullptr;

  CommandCalls& c = command_calls();
  c = CommandCalls{};
  c.own = own;
  c.handle = [](Mech&) { return 0x1234u; };
  c.torso_center = [](Mech&, int32_t how) { log(0x465a00, static_cast<uint32_t>(how), 0); };
  c.view_camera = [](int32_t mode) { log(kCamera, static_cast<uint32_t>(mode), 0); };
  c.set_view_option = [](int32_t v) { option = v; };
  c.hud = [](void* s, int32_t cmd) { log(kHud, reinterpret_cast<uintptr_t>(s), static_cast<uint32_t>(cmd)); };
  c.hud_weapon = [](void*) -> void* { return g_in[kHudWeapon] ? g_weapon : nullptr; };
  c.hud_aimed_node = [](void* s) -> void* {
    log(kHud, reinterpret_cast<uintptr_t>(s), g_in[kCmd] == 0x48 ? 0x48u : 0x14u);
    return g_in[kAim] ? ptr<void>(g_tok[tNode]) : nullptr;
  };
  c.node_entity = [](void* node) -> uint32_t {
    log(0x40eee0, reinterpret_cast<uintptr_t>(node), 0);
    return node && g_in[kEntity] ? g_tok[tEntity0 + g_in[kEntity] - 1] : 0;
  };
  c.entity_team = [](uint32_t e) {
    log(kTeam, e, 0);
    return static_cast<uint32_t>(g_in[kTeamIn]);
  };
  c.entity_refuses = [](uint32_t e, bool zoom) {
    log(kRefuses, e, zoom ? 1 : 0);
    return ((zoom ? g_in[kRefuse] : g_in[kRefuse] >> 1) & 1) != 0;
  };
  c.lance_order = [](Mech&, int32_t order) -> MechControls* {
    if (g_in[kLanceBusy]) return nullptr;
    log(0x469930, static_cast<uint32_t>(order), 0);
    return ptr<MechControls>(g_tok[tControls]);
  };
  c.target_part = [](Mech&, int32_t part) { log(0x41d810, static_cast<uint32_t>(part), 0); };
  c.other = [](Mech&, int32_t cmd) { log(kOther, static_cast<uint32_t>(cmd), 0); };
  c.stations = []() -> void* { return g_in[kStations] ? ptr<void>(0x5150) : nullptr; };
  HeatCalls& h = heat_calls();
  h = HeatCalls{};
  h.screen = [](Mech&) -> void* { return g_in[kScreen] ? ptr<void>(g_tok[tScreen]) : nullptr; };
  h.play_sound = [](Mech&, int id) -> Sound {
    log(kPlay, static_cast<uint32_t>(id), 0);
    return ptr<void>(g_tok[tSound]);
  };
  h.play_mech_sound = [](int id) { log(kMechSound, static_cast<uint32_t>(id), 0); };
  update_calls().controls_view = [](MechControls&, int mode) { log(0x426780, static_cast<uint32_t>(mode), 0); };

  m.command(in[kCmd]);

  out[0] = m.xbc;
  out[1] = static_cast<int32_t>(tok(reinterpret_cast<uintptr_t>(m.saved_target)));
  out[2] = ams.on;
  out[3] = wm.mode;
  out[4] = option;
  out[5] = m.controls == &controls ? 0 : static_cast<int32_t>(reinterpret_cast<uintptr_t>(m.controls));
  out[6] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(m.sounds[23]));

  // Nothing here is the mech's or the manager's to free.
  m.target = m.saved_target = nullptr;
  m.contacts.clear();
  m.ams = nullptr, m.ammo = nullptr, m.controls = nullptr;
  m.sounds[23] = nullptr;
  command_calls() = saved_calls;
  heat_calls() = saved_heat;
  update_calls().controls_view = saved_view;
  world().player = saved_player;
  const int32_t n = static_cast<int32_t>(g_log.size()) < cap ? static_cast<int32_t>(g_log.size()) : cap;
  std::memcpy(log_out, g_log.data(), sizeof(int32_t) * n);
  return n;
}

// 0x00419190 (which 0, with on) or 0x00419e60 (which 1). state: heat_state;
// io: heat, coolant as bits, in and out. log: the sounds as above.
int32_t probe_heat_key(int32_t which, int32_t on, int32_t viewed, int32_t pilot, uint32_t* state, float* io,
                       uint32_t sound, int32_t* log_out, int32_t cap) {
  g_log.clear();
  static uint32_t s_sound;
  s_sound = sound;
  const HeatCalls saved_heat = heat_calls();
  Mech* saved_player = world().player;
  Mech* saved_pilot = world().mission_player;
  Mech m;
  m.heat_state = *state;
  m.heat = io[0], m.coolant = io[1];
  world().player = viewed ? &m : nullptr;
  world().mission_player = pilot ? &m : nullptr;
  HeatCalls& h = heat_calls();
  h = HeatCalls{};
  h.play_sound = [](Mech&, int id) -> Sound {
    log(kPlay, static_cast<uint32_t>(id), 0);
    return ptr<void>(s_sound);
  };
  h.play_mech_sound = [](int id) { log(kMechSound, static_cast<uint32_t>(id), 0); };
  if (which == 0) m.heat_override(on != 0);
  else m.flush_coolant();
  *state = m.heat_state;
  io[0] = m.heat, io[1] = m.coolant;
  log(0x610, reinterpret_cast<uintptr_t>(m.sounds[22]), 0);
  m.sounds[22] = nullptr;
  heat_calls() = saved_heat;
  world().player = saved_player;
  world().mission_player = saved_pilot;
  const int32_t n = static_cast<int32_t>(g_log.size()) < cap ? static_cast<int32_t>(g_log.size()) : cap;
  std::memcpy(log_out, g_log.data(), sizeof(int32_t) * n);
  return n;
}

}  // extern "C"
