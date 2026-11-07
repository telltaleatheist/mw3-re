// test_pm_refit.py: PM's dock state for a static MFB, DockRefit, every call out logged.
#include <cstdint>
#include <cstring>

#include "mech3/mech_fieldbase.h"
#include "mech3/veh_basic_object.h"

using namespace mech3;

namespace {
struct Setup {
  int powered, mp, enemy, remote, node, def, viewed, far_off;
  float enemy_d2;
};
Setup g;
uint32_t* g_log;
int g_n;
uint32_t g_frame;
float g_time;
const float kPoint[3] = {10, 2, 30};
float g_at[3] = {10, 2, 30};
int g_mech, g_point, g_ctc, g_veh, g_anim, g_node, g_defn;
void note(uint32_t a, uint32_t b = 0) { g_log[g_n++] = a, g_log[g_n++] = b; }

struct Hooks : FieldBaseHooks {
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  uint32_t frame() override { return g_frame; }
  float game_time() override { return g_time; }
  bool powered_down(Handle) override { return g.powered != 0; }
  const float* entity_position(Handle) override { return g_at; }
  const float* dock_point_position(Handle) override { return kPoint; }
  const char* dock_point_name(Handle) override { return "pointname"; }
  bool mp_repairs() override { return g.mp != 0; }
  Handle nearest_enemy(Handle) override { return g.enemy ? &g_ctc : nullptr; }
  Handle contact_vehicle(Handle) override { return &g_veh; }
  float contact_distance(Handle) override { return g.enemy_d2; }
  const char* entity_name(Handle) override { return "them"; }
  void hud_message(const char*) override { note(1); }
  bool entity_remote(Handle) override { return g.remote != 0; }
  Handle view_vehicle() override { return g.viewed ? static_cast<Handle>(&g_mech) : static_cast<Handle>(&g_veh); }
  int32_t view_camera() override { return 7; }
  void view_set_camera(int32_t mode) override { note(2, static_cast<uint32_t>(mode)); }
  void mech_rest_at(Handle, const float*) override { note(3); }
  Handle effects_start(Handle, Handle n) override { note(4, n ? 1 : 0); return &g_anim; }
  void anim_on_end(Handle, Handle) override { note(5); }
  void entity_set_shutdown(Handle, bool on) override { note(6, on ? 1 : 0); }
  void mech_repair(Handle, float) override { note(7); }
  void mech_power_up(Handle) override { note(8); }
  void marker_clear() override { note(9); }
  void say(Handle s) override { note(10, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s))); }
};
Hooks g_hooks;
struct Veh : VehBasicHooks {
  Handle node_named(const char*) override { return g.node ? &g_node : nullptr; }
  Handle anim_find(const char* name) override {
    note(11, std::strcmp(name, "mfb_station_01") == 0 ? 1 : 0);
    return g.def ? &g_defn : nullptr;
  }
};
Veh g_veh_hooks;
FieldBases* g_r;
}  // namespace

extern "C" {
void probe_pm_refit_reset(const int* s, float enemy_d2, float x) {
  g_at[0] = x;
  g = Setup{s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], enemy_d2};
  set_field_base_hooks(&g_hooks);
  set_veh_basic_hooks(&g_veh_hooks);
  delete g_r;
  g_r = new FieldBases;
  g_r->requester = s[8] ? &g_mech : nullptr;
  g_r->dock_point = s[9] ? &g_point : nullptr;
  for (int i = 0; i < 11; ++i) g_r->sounds[i] = reinterpret_cast<Handle>(uintptr_t(0x100 + i));
  field_bases_quiet_until() = 0;
  g_r->refit.anim = nullptr, g_r->refit.line_at = 0, g_r->refit.finished = false;
  field_bases_drive() = field_bases_contact() = false;
}
// op: 0 begin, 1 step, 2 end, 3 abort, 4 the animation's end (arg stopped). Out: the flags.
int probe_pm_refit_op(int op, int arg, uint32_t frame, float time, uint32_t* log, int* out) {
  g_log = log, g_n = 0, g_frame = frame, g_time = time;
  DockRefit& d = g_r->refit;
  if (op == 0) d.begin();
  if (op == 1) out[0] = d.step(0.0f) ? 1 : 0;
  if (op == 2) d.end();
  if (op == 3) d.abort();
  if (op == 4) d.anim_ended(arg);
  out[1] = d.finished, out[2] = d.anim != nullptr, out[3] = field_bases_drive(), out[4] = field_bases_contact();
  out[5] = static_cast<int>(reinterpret_cast<uintptr_t>(d.sound)), out[6] = static_cast<int>(g_r->states.size());
  return g_n / 2;
}
void probe_pm_refit_done() {
  set_field_base_hooks(nullptr);
  set_veh_basic_hooks(nullptr);
}
}
