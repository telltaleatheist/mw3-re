// The weapon definitions (gamez/zweapon/zwep_init, mech3/weapon) for
// tests/differential/test_weapons_zrd.py. The reader runs on a one-entry
// archive the test writes; every call it makes into unported systems is
// recorded and answered with a value made from the name it was given, the same
// way the test's emulator side does. Each def, its impacts and its WeaponType
// are then written out in the original's byte layout, pointers replaced by
// tokens, for a byte-exact compare.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech_names.h"
#include "mech3/weapon.h"

namespace {

std::vector<int32_t> g_log;
std::vector<std::string> g_strings;  // keeps message() results alive
uint32_t g_crater[10];
int g_tick_ok = 1;

uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  if (!s) return 0;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}
// The handle a lookup of `kind` answers for a name; 0 for the empty name.
uint32_t handle(int kind, const char* s) { return s && *s ? (uint32_t(kind) << 24) | (fnv(s) & 0xffffff) : 0; }
void* as_ptr(uint32_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); }
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}

void set_hooks() {
  zwep::InitHooks& h = zwep::init_hooks();
  h = zwep::InitHooks{};
  h.lights_init = [] { log({1}); };
  h.sound = [](const char* s) { log({2, int32_t(fnv(s))}); return as_ptr(handle(1, s)); };
  h.message = [](const char* s) -> const char* {
    log({3, int32_t(fnv(s))});
    g_strings.push_back(std::string("M:") + s);
    return g_strings.back().c_str();
  };
  h.anim = [](const char* s) { log({4, int32_t(fnv(s))}); return as_ptr(handle(2, s)); };
  h.effect = [](const char* s) { log({5, int32_t(fnv(s))}); return int32_t(handle(3, s)); };
  h.find_node = [](int32_t list, const char* s) { log({6, list, int32_t(fnv(s))}); return as_ptr(handle(4, s)); };
  h.crater_defaults = [](zwep::Crater* c) {
    log({7});
    std::memcpy(c, g_crater, sizeof g_crater);
  };
  h.set_intersect_surface = [](void* n, int32_t on) { log({8, int32_t(u(n)), on}); };
  h.set_active = [](void* n, int32_t on) { log({9, int32_t(u(n)), on}); };
  h.tree_unlit = [](void* n) {
    log({10, int32_t(u(n))});
    return int32_t((u(n) >> 3) & 1);
  };
  h.pool_objects = [](int32_t c) { log({11, c}); };
  h.object3d_create = []() -> void* {
    log({12});
    return g_tick_ok ? as_ptr(0x7700) : nullptr;
  };
  h.set_action_priority = [](void* n, int32_t p) { log({13, int32_t(u(n)), p}); };
  h.set_tick_action = [](void* n) { log({14, int32_t(u(n))}); };
  h.error = [](int32_t level, int32_t line, const char*) { log({15, level, line}); };
}

template <class T>
void put(uint8_t* b, int off, T v) { std::memcpy(b + off, &v, sizeof v); }

void put_effect(uint8_t* b, const zwep::Effect& e) {
  put(b, 0x00, e.flags);
  put(b, 0x04, u(e.animation));
  put(b, 0x08, u(e.animation2));
  put(b, 0x0c, u(e.animation_attached));
  put(b, 0x10, u(e.model_animation));
  put(b, 0x14, u(e.surface_animation));
  put(b, 0x18, u(e.animation_min_range));
  put(b, 0x1c, e.effect);
  put(b, 0x20, u(e.model));
  put(b, 0x24, e.sound_count);
  for (int i = 0; i < 4; ++i) put(b, 0x28 + 4 * i, u(e.sounds[i]));
  put(b, 0x38, e.bounce_sound_count);
  for (int i = 0; i < 4; ++i) put(b, 0x3c + 4 * i, u(e.bounce_sounds[i]));
  put(b, 0x4c, e.sound_choice);
  put(b, 0x50, e.animation_choice);
  put(b, 0x54, u(e.attach_node));
  put(b, 0x58, u(e.attach_offset));
}

void put_crater(uint8_t* b, const zwep::Crater& c) { std::memcpy(b, &c, 40); }

uint32_t def_token(const zwep::WeaponDef* d) {
  return d ? static_cast<uint32_t>(d - zwep::state().defs.data()) + 1 : 0;
}

uint32_t callback_addr(zwep::DefCallback f) {
  if (!f) return 0;
  if (f == mech3::salvo_spread) return 0x4afe70;
  if (f == mech3::airstrike) return 0x4afea0;
  if (f == mech3::burst) return 0x4b44d0;
  return 0xffffffff;
}

}  // namespace

extern "C" {

// Runs zwep::init on the archive's weapons.zrd with mech3's WeaponType
// callback. Returns init's result; the call log goes to `out`.
int probe_zwep_run(const char* archive, const uint32_t* crater, int hardware, int tick_ok, int32_t* out, int cap,
                   int* n) {
  g_log.clear();
  g_strings.clear();
  g_strings.reserve(4096);
  std::memcpy(g_crater, crater, sizeof g_crater);
  g_tick_ok = tick_ok;
  zwep::state() = zwep::State{};
  set_hooks();
  zwep::init_hooks().hardware = hardware != 0;
  zrdr::init();
  zrdr::close_archives(true);
  if (!zrdr::open_archive(archive, true)) return -100;
  const int rc = zwep::init(as_ptr(0x6600), "weapons.zrd", 0x1234, mech3::make_weapon_type);
  if (static_cast<int>(g_log.size()) > cap) return -101;
  std::memcpy(out, g_log.data(), g_log.size() * 4);
  *n = static_cast<int>(g_log.size());
  return rc;
}

// Globals after the run: def count, pool size, the four sounds, crater radius
// bits, world, arg, tick, whether the free list runs through the whole pool.
void probe_zwep_globals(uint32_t* out) {
  const zwep::State& s = zwep::state();
  out[0] = static_cast<uint32_t>(s.defs.size());
  out[1] = static_cast<uint32_t>(s.pool.size());
  out[2] = u(s.lock_on_warning);
  out[3] = u(s.no_ammo_warning);
  out[4] = u(s.trigger_inactive);
  out[5] = u(s.weapon_inactive);
  std::memcpy(&out[6], &s.max_crater_radius, 4);
  out[7] = u(s.world);
  out[8] = static_cast<uint32_t>(s.init_arg);
  out[9] = u(s.tick);
  // The free list: last slot first, each pointing at the one before.
  uint32_t ok = 1, k = static_cast<uint32_t>(s.pool.size());
  for (const zwep::Projectile* p = s.free_list; p; p = p->next) {
    if (k == 0 || p != &s.pool[k - 1]) ok = 0;
    --k;
  }
  out[10] = ok && k == 0;
}

// Def i in the original layout (0x1f4, strings and pointers zeroed), its
// impacts (6 x 0x5c, zeros when none), its WeaponType (0xc8, zeros when none)
// and its four strings, NUL-separated into `text`.
int probe_zwep_def(int i, uint8_t* def, uint8_t* impacts, uint8_t* type, char* text, int cap) {
  const zwep::WeaponDef& d = zwep::state().defs[static_cast<size_t>(i)];
  std::memset(def, 0, 0x1f4);
  put(def, 0x010, d.index);
  put(def, 0x014, d.period);
  put(def, 0x018, d.ammo_limit);
  put(def, 0x01c, d.range);
  put(def, 0x020, d.range_sq);
  put(def, 0x024, d.range_min[0]);
  put(def, 0x028, d.range_min[1]);
  put(def, 0x02c, d.velocity);
  put(def, 0x030, d.turn_rate);
  put(def, 0x034, d.pitch_rate);
  put(def, 0x038, d.acceleration);
  put(def, 0x03c, d.impact_proximity);
  put(def, 0x040, d.impact_proximity_sq);
  put(def, 0x044, d.detonation_distance_sq);
  put(def, 0x048, d.damage);
  put(def, 0x04c, d.gravity);
  put(def, 0x050, d.lock_on);
  put(def, 0x054, d.turn_suspend_time);
  put(def, 0x058, d.beam_energy);
  put(def, 0x05c, d.pulse[0]);
  put(def, 0x060, d.pulse[1]);
  put(def, 0x064, d.pulse_rate);
  put(def, 0x068, d.damage_at_max_range);
  put(def, 0x06c, d.flags);
  put(def, 0x070, d.lock_on_lead_min);
  put(def, 0x074, d.lock_on_lead_max);
  put(def, 0x078, d.lock_on_lead_span);
  put(def, 0x07c, u(d.active));
  put(def, 0x080, d.flyout_health);
  put_effect(def + 0x84, d.fire);
  put_effect(def + 0xe0, d.flyout);
  put(def, 0x140, u(d.model_pool));
  put(def, 0x144, d.damage_anim_count);
  for (int k = 0; k < 4; ++k) {
    put(def, 0x148 + 8 * k, d.damage_anims[k].health);
    put(def, 0x14c + 8 * k, u(d.damage_anims[k].anim));
  }
  put(def, 0x168, u(d.destroy_animation));
  put(def, 0x16c, u(d.kill_animation));
  put(def, 0x170, d.impact_type);
  put_crater(def + 0x174, d.crater);
  put_crater(def + 0x19c, d.crater_span);
  for (int k = 0; k < 6; ++k) put(def, 0x1c8 + 4 * k, d.target_heat[k]);
  put(def, 0x1e0, d.apex);
  put(def, 0x1e4, d.x1e4);
  put(def, 0x1e8, d.terrain_height);
  put(def, 0x1ec, callback_addr(d.callback));
  std::memset(impacts, 0, 6 * 0x5c);
  if (d.impacts)
    for (int k = 0; k < zwep::kSoilCount; ++k) put_effect(impacts + 0x5c * k, d.impacts[k]);
  std::memset(type, 0, 0xc8);
  if (const mech3::WeaponType* t = d.type) {
    put(type, 0x00, t->id);
    put(type, 0x04, def_token(t->def));
    put(type, 0x08, t->heat);
    put(type, 0x0c, t->heat_2);
    put(type, 0x10, static_cast<uint8_t>(t->show_on_radar));
    put(type, 0x14, t->zaps_hud);
    put(type, 0x18, t->zaps_hud_2);
    for (int k = 0; k < 2; ++k) {
      put(type, 0x1c + 0xc * k, t->lab[k].tons);
      put(type, 0x20 + 0xc * k, t->lab[k].slots);
      put(type, 0x24 + 0xc * k, static_cast<uint8_t>(t->lab[k].present));
    }
    put(type, 0x34, t->tons_per_round);
    put(type, 0x38, t->rounds_per_ton);
    put(type, 0x3c, t->salvo_count);
    put(type, 0x40, t->salvo_delta);
    put(type, 0x44, t->salvo_truecount);
    put(type, 0x48, t->impact_force);
    put(type, 0x4c, static_cast<uint8_t>(t->is_narc));
    put(type, 0x50, t->burst_radius);
    put(type, 0x54, t->burst_count);
    put(type, 0x58, def_token(t->burst));
    put(type, 0x5c, u(t->looped_sound));
    put(type, 0x60, u(t->recharge_sound));
    put(type, 0x64, u(t->name_sound));
    put(type, 0x68, u(t->part_damage_sound));
    put(type, 0x6c, u(t->part_destroy_sound));
    put(type, 0x70, t->range);
    put(type, 0x74, t->range_sq);
    put(type, 0x78, t->half_range);
    put(type, 0x7c, t->half_range_sq);
    put(type, 0x84, static_cast<uint32_t>(t->salvo_marks.empty()));  // the emulator side writes "head is empty"
    put(type, 0x88, static_cast<uint32_t>(t->salvo_marks.size()));
    put(type, 0x8c, t->salvo_radius_sq);
    put(type, 0x90, t->salvo_delay);
    put(type, 0x94, t->recoil);
    put(type, 0x98, t->recharge_rate);
    put(type, 0x9c, t->fire_time);
    put(type, 0xa0, def_token(t->strike_weapon));
    put(type, 0xb0, t->strike_flag);
    put(type, 0xb8, static_cast<uint32_t>(t->strike_lock.at == &t->strike_point));
    put(type, 0xbc, u(t->strike_lock.velocity));
    put(type, 0xc0, t->lrm_flight);
    put(type, 0xc4, t->ready_at);
  }
  std::string s;
  for (const char* p : {d.key, d.name, d.desc, d.military_name}) {
    s += p ? p : "<null>";
    s.push_back('\0');
  }
  if (static_cast<int>(s.size()) > cap) return -1;
  std::memcpy(text, s.data(), s.size());
  return static_cast<int>(s.size());
}

// The mech lab's view (0x00484b70, 0x00484bc0, 0x00484c00) of weapon index w.
void probe_zwep_lab(int w, float* out_f, int32_t* out_i) {
  out_f[0] = mech3::weapon_weight(w, 1);
  out_f[1] = mech3::weapon_weight(w, 2);
  out_f[2] = mech3::ammo_weight(w);
  out_i[0] = mech3::weapon_slots(w, 1, false);
  out_i[1] = mech3::weapon_slots(w, 2, false);
}

void probe_zwep_shutdown() { zwep::shutdown(mech3::free_weapon_type); }

}  // extern "C"
