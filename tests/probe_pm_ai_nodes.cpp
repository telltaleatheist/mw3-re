// PM's AI readers, for test_pm_ai_nodes.py: every node_NN of a shipped net file through the
// node reader, as text, valves and sounds by name.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "gamez/zreader/zreader.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_node.h"
#include "mech3/zrdrainet.h"
#include "mech3/zrdraiv.h"

namespace {
using namespace mech3::ai;
const char* same(const char* s) { return s; }
void* sound_named(const char* s) { return const_cast<char*>(s); }
uint32_t fb(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
const char* vn(const Valve* v) { return v && v->name ? v->name : "-"; }
const char* sn(const char* s) { return s ? s : "-"; }
void dump(std::string& o, const char* key, const NetNodeRec& r) {
  char b[1024];
  std::snprintf(b, sizeof b,
                "%s %d %x %d %x %d %x %d %s %d %d %d %d %d %x %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %s %s %d %x %d %d %x %s %s %d %d %x %d %d %d",
                key, r.speed_on, fb(r.speed), r.wait_interval_on, fb(r.wait_interval), r.wait_time_on, fb(r.wait_time),
                r.valve_on, vn(r.valve), r.valve_has_value, r.valve_value, r.canleave[0], r.canleave[1], r.jump_on,
                fb(r.jump), r.scan_aim[0], r.scan_aim[1], r.scan_hit[0], r.scan_hit[1], r.scan_time[0], r.scan_time[1],
                r.hidden_set, r.hidden, r.fixed_set, r.fixed, r.autonext_set, r.autonext, r.visible_set, r.visible,
                r.deploy_set, r.deploy_a, r.deploy_b, r.deploy_c, vn(r.deploy_valve_a), vn(r.deploy_valve_b),
                r.arrange_set, fb(r.arrange), r.mcbwait_set, r.mcbwait_b, fb(r.mcbwait_time), vn(r.mcbwait_valve),
                r.sound ? static_cast<const char*>(r.sound) : "-", r.backup, r.shunt, fb(r.via_radius2), r.via_number,
                r.terminus, r.static_mfb_set);
  o += b;
  o += ' ' + std::to_string(r.static_mfb) + ' ' + sn(r.name) + '|' + sn(r.auxname) + '|' + sn(r.gate) + '|' + sn(r.via) + '|';
  if (r.valve_union)
    for (const ValveCount& c : r.valve_union->valves) o += std::string(vn(c.valve)) + ':' + std::to_string(c.count) + ',';
  o += '|';
  if (r.valve_assign) o += std::string(vn(r.valve_assign->valve)) + ':' + std::to_string(r.valve_assign->value);
  o += '\n';
}
}  // namespace

extern "C" int probe_pm_ai_nodes(const char* entry, char* out, int size) {
  rdr_hooks().localize = same;
  rdr_hooks().sound_find = sound_named;
  zrdr::Node* root = zrdr::load(entry);
  if (!root || root->type != zrdr::List) return -1;
  std::string o;
  zrdr::Node* top = root->list[0].i > 1 && root->list[1].type == zrdr::List ? root->list[1].list : nullptr;
  const int32_t n = top ? top[0].i : 0;
  for (int32_t i = 1; i + 1 < n; ++i) {
    const zrdr::Node& k = top[i];
    if (k.type != zrdr::String || std::strncmp(k.s, "node_", 5) != 0) continue;
    NetNodeRec r;
    net_read_props(r, &top[i + 1]);
    dump(o, k.s, r);
  }
  zrdr::free_node(root);
  rdr_hooks() = RdrHooks{};
  if (static_cast<int>(o.size()) >= size) return -2;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}

// targets.zrd's records (0x004f1c20) as text, flags as the exe holds them, valves by name.
extern "C" int probe_pm_ai_targets(const char* entry, char* out, int size) {
  rdr_hooks().localize = same;
  zrdr::Node* root = zrdr::load(entry);
  if (!root || root->type != zrdr::List) return -1;
  std::string o;
  zrdr::Node* top = root->list[0].i > 1 && root->list[1].type == zrdr::List ? root->list[1].list : root->list;
  const int32_t n = top[0].i;
  for (int32_t i = 1; i + 1 < n; i += 2) {
    if (top[i].type != zrdr::String) continue;
    TargetRec r;
    r.parse(&top[i + 1], top[i].s);
    char b[512];
    std::snprintf(b, sizeof b, "%s %d %x %x %x %x %x %x %x %x %d %x %x ", top[i].s, r.kind, fb(r.health), fb(r.target_offset[0]),
                  fb(r.target_offset[1]), fb(r.target_offset[2]), fb(r.centroid_offset[0]), fb(r.centroid_offset[1]),
                  fb(r.centroid_offset[2]), fb(r.height), r.mpoption, r.flags, fb(r.load_limit));
    o += b;
    o += std::string(sn(r.name)) + '|' + sn(r.destroy_anim) + '|' + sn(r.target) + '|' + sn(r.centroid) + '|' + sn(r.footfall_anim) + '|';
    if (r.health_below)
      for (const HealthBelow& h : *r.health_below) o += std::string(vn(h.valve)) + ':' + std::to_string(h.a) + ':' + std::to_string(fb(h.health)) + ',';
    o += '\n';
  }
  zrdr::free_node(root);
  rdr_hooks() = RdrHooks{};
  if (static_cast<int>(o.size()) >= size) return -2;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
