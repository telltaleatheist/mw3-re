// The vehicle editor's pages for test_vehicle_ddx.py: each page's
// DoDataExchange (p1 0x004b8bc0, p2 0x004b8eb0, Attack 0x004b92f0, ...) on the MFC stand-in (ddx.h), one
// pass on a state given as the exe lays it out, and the MFC conversions alone.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "mech3/game_editor_vehicles.h"
#include "upgrades/editor/ddx.h"

namespace {
struct Ctl {
  int32_t id, check, limit;
  char text[64];
};
// test_vehicle_ddx.py's State, field for field.
struct State {
  uint8_t params[0xd0];  // the settings' +0x00..+0xcf (+0x04 on used)
  int32_t show, show_inout, team_side, keep[5], objective, shutdown;
  int32_t pilot_null;
  char pilot[64];
  char p2_pilot[64];
  int32_t combo_cur;  // p1's combo 1229, Attack's 1233
  float entry_f[5];     // the Valve page's entry: +0x00..+0x10
  uint8_t entry_flags[2];  // +0x18, +0x19
  Ctl ctl[40];
  int32_t n_ctl;
  int32_t ids[64];
  int32_t n_ids;
  uint32_t calls[16];
  int32_t n_calls;
  int32_t ok, fail_id;
  char message[96];
};
}  // namespace

extern "C" void probe_vehicle_ddx(int32_t page, int32_t save, State* s) {
  using namespace mech3;
  VehicleEditor* e = vehicle_editor_create();
  e->selected = -1;  // no vehicle: the list fills do nothing but say they ran
  e->strategy_page = -1;  // Attack's own tab: none shown (its pages are run alone)
  std::memcpy(e->params.raw + ai::AiParams::kBegin, s->params + ai::AiParams::kBegin,
              ai::AiParams::kEnd - ai::AiParams::kBegin);
  e->params.pilot = s->pilot_null ? nullptr : strdup(s->pilot);
  e->show = s->show, e->show_inout = static_cast<uint8_t>(s->show_inout), e->team_side = s->team_side;
  for (int k = 0; k < 5; ++k) e->keep[k] = s->keep[k];
  e->p2_objective = s->objective, e->p2_shutdown = s->shutdown;
  e->p2_pilot = s->p2_pilot;
  Dialog& d = e->pages[page];
  for (int32_t i = 0; i < s->n_ctl; ++i) {
    DlgControl& c = d.at(s->ctl[i].id);
    c.check = s->ctl[i].check, c.text = s->ctl[i].text, c.limit = s->ctl[i].limit;
  }
  const int32_t combo_id = page == kPageAttack ? 1233 : 1229;
  d.at(combo_id).combo.cur = s->combo_cur;
  ai::VListEntry& v = e->spare;  // no valve named: the page shows the editor's own entry (+0x3fc)
  v.f00 = s->entry_f[0], v.f04 = s->entry_f[1], v.f08 = s->entry_f[2], v.f0c = s->entry_f[3], v.f10 = s->entry_f[4];
  v.flags = s->entry_flags[0], v.flags2 = s->entry_flags[1];
  std::vector<uint32_t> calls;
  std::vector<int32_t> ids;
  vehicle_editor_trace() = &calls;
  vehicle_editor_ids() = &ids;
  s->ok = vehicle_page_update(page, save != 0) ? 1 : 0;
  vehicle_editor_trace() = nullptr;
  vehicle_editor_ids() = nullptr;
  s->fail_id = ddx_message().control;
  std::snprintf(s->message, sizeof s->message, "%s", ddx_message().text.c_str());
  std::memcpy(s->params + ai::AiParams::kBegin, e->params.raw + ai::AiParams::kBegin,
              ai::AiParams::kEnd - ai::AiParams::kBegin);
  s->pilot_null = e->params.pilot ? 0 : 1;
  std::snprintf(s->pilot, sizeof s->pilot, "%s", e->params.pilot ? e->params.pilot : "");
  s->show = e->show, s->show_inout = e->show_inout, s->team_side = e->team_side;
  for (int k = 0; k < 5; ++k) s->keep[k] = e->keep[k];
  s->objective = e->p2_objective, s->shutdown = e->p2_shutdown;
  std::snprintf(s->p2_pilot, sizeof s->p2_pilot, "%s", e->p2_pilot.c_str());
  for (int32_t i = 0; i < s->n_ctl; ++i) {
    const DlgControl& c = d.at(s->ctl[i].id);
    s->ctl[i].check = c.check, s->ctl[i].limit = c.limit;
    std::snprintf(s->ctl[i].text, sizeof s->ctl[i].text, "%s", c.text.c_str());
  }
  s->combo_cur = d.at(combo_id).combo.cur;
  s->entry_f[0] = v.f00, s->entry_f[1] = v.f04, s->entry_f[2] = v.f08, s->entry_f[3] = v.f0c, s->entry_f[4] = v.f10;
  s->entry_flags[0] = v.flags, s->entry_flags[1] = v.flags2;
  s->n_ids = static_cast<int32_t>(ids.size() < 64 ? ids.size() : 64);
  for (int32_t i = 0; i < s->n_ids; ++i) s->ids[i] = ids[size_t(i)];
  s->n_calls = static_cast<int32_t>(calls.size() < 16 ? calls.size() : 16);
  for (int32_t i = 0; i < s->n_calls; ++i) s->calls[i] = calls[size_t(i)];
  std::free(e->params.pilot);
  e->params.pilot = nullptr;
  vehicle_editor_close();
}

// The conversions: the text a float shows as; a text read as a float, an
// int or an unsigned (0 when it fails).
extern "C" void probe_ddx_float_text(float v, char* out) {
  std::snprintf(out, 64, "%s", mech3::ddx_float_text(v).c_str());
}
extern "C" int probe_ddx_parse_float(const char* s, double* d) { return mech3::ddx_parse_float(s, *d) ? 1 : 0; }
extern "C" int probe_ddx_parse_int(const char* s, int is_unsigned, int32_t* v) {
  return mech3::ddx_parse_int(s, is_unsigned != 0, *v) ? 1 : 0;
}
