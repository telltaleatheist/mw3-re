// PM's AI readers, for test_pm_ai_nodes.py: every node_NN of a shipped net file through the
// node reader, as text, valves and sounds by name.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_node.h"
#include "mech3/zrdrainet.h"
#include "mech3/zrdraiv.h"
#include "mech3/zrdrvalve.h"

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

// A record's reset (0x004e9d60) from every byte set to `fill`: PM's words +0x04..+0xef in PM's order (the port's
// numbers, its target block, then its flags, held at MW3's +0xcc).
extern "C" void probe_pm_ai_reset(uint32_t fill, uint32_t* out) {
  AiParams p;
  std::memset(p.raw, static_cast<int>(fill & 0xff), sizeof p.raw);
  for (uint32_t& w : p.target_words) w = fill * 0x01010101u;
  p.reset();
  int k = 0;
  for (int off = 4; off < 0xcc; off += 4) out[k++] = p.get<uint32_t>(off);
  for (uint32_t w : p.target_words) out[k++] = w;
  out[k++] = p.get<uint32_t>(0xcc);
}

// aiv.zrd's records (0x004f0830): the numbers, the target block, the flags as PM holds them.
extern "C" int probe_pm_ai_aiv(const char* entry, char* out, int size) {
  rdr_hooks().localize = same;
  zrdr::Node* root = zrdr::load(entry);
  if (!root || root->type != zrdr::List) return -1;
  std::string o;
  zrdr::Node* top = root->list[0].i > 1 && root->list[1].type == zrdr::List ? root->list[1].list : root->list;
  const int32_t n = top[0].i;
  auto* r = new AivRecord;
  for (int32_t i = 1; i + 1 < n; i += 2) {
    if (top[i].type != zrdr::String || top[i + 1].type != zrdr::List) continue;
    r->parse(&top[i + 1], top[i].s);
    o += top[i].s;
    char b[32];
    for (int off = 4; off < 0xcc; off += 4) {
      std::snprintf(b, sizeof b, " %x", r->get<uint32_t>(off));
      o += b;
    }
    for (uint32_t w : r->target_words) std::snprintf(b, sizeof b, " %x", w), o += b;
    std::snprintf(b, sizeof b, " %x %x %x ", r->raw[0xcc], r->raw[0xcd], r->raw[0xce]);
    o += b;
    o += std::string(sn(r->pilot)) + '|' + sn(r->sound_tag) + '|';
    if (r->vlist)
      for (const VListEntry& e : *r->vlist) {
        std::snprintf(b, sizeof b, "%x,%x,", fb(e.f00), fb(e.f04));
        o += b;
        std::snprintf(b, sizeof b, "%x,%x,%x,", fb(e.f08), fb(e.f0c), fb(e.f10));
        o += b;
        std::snprintf(b, sizeof b, "%x,%x,", e.flags, e.flags2);
        o += b;
        o += std::string(vn(e.valve)) + ';';
      }
    o += '\n';
  }
  delete r;
  zrdr::free_node(root);
  rdr_hooks() = RdrHooks{};
  if (static_cast<int>(o.size()) >= size) return -2;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}

// valves.zrd's records (0x004f38e0), one record read over again, as the file's is.
extern "C" int probe_pm_ai_valves(const char* entry, char* out, int size) {
  rdr_hooks().localize = same;
  zrdr::Node* root = zrdr::load(entry);
  if (!root || root->type != zrdr::List) return -1;
  std::string o;
  zrdr::Node* top = root->list[0].i > 1 && root->list[1].type == zrdr::List ? root->list[1].list : root->list;
  const int32_t n = top[0].i;
  auto* d = new ValveDef;
  char b[64];
  auto named = [&](const ValveDef::Named& v) {
    std::snprintf(b, sizeof b, "|%s %d %x", sn(v.name), v.value, v.flag);
    o += b;
  };
  auto flag = [&](const ValveDef::Flag& v) {
    std::snprintf(b, sizeof b, "|%x %d", v.on, v.value);
    o += b;
  };
  auto text = [&](const char* s, int32_t v, uint32_t w) {
    std::snprintf(b, sizeof b, "|%s %d %x", sn(s), v, w);
    o += b;
  };
  for (int32_t i = 1; i + 1 < n; i += 2) {
    if (top[i].type != zrdr::String || top[i + 1].type != zrdr::List) continue;
    d->parse(&top[i + 1], top[i].s);
    o += top[i].s;
    flag(d->init);
    named(d->sound), named(d->anim);
    for (const auto& v : d->scan) named(v);
    named(d->shutdown);
    flag(d->complete), flag(d->abort), flag(d->reset);
    named(d->destroy);
    text(d->update, 0, 0);
    text(d->delay_valve, 0, d->delay);
    text(d->team_entity, d->team_value, d->team);
    text(d->attack_entity, d->attack_value, d->strategy);
    text(d->teleport, d->teleport_value, fb(d->yaw));
    std::snprintf(b, sizeof b, "|%x %x %x", fb(d->pos[0]), fb(d->pos[1]), fb(d->pos[2]));
    o += b;
    for (const auto* r : {&d->leave_net, &d->attack_radius, &d->approach, &d->min_range, &d->scan_delay[0],
                          &d->scan_delay[1], &d->scan_duration[0], &d->scan_duration[1]})
      text(r->name, r->value, r->number);
    for (const auto& v : d->retreat) named(v);
    std::snprintf(b, sizeof b, "|%x|", d->flags & 0x1f);
    o += b;
    for (const auto& s : d->names.names) o += (s ? *s : std::string("-")) + ',';
    o += '\n';
  }
  delete d;
  zrdr::free_node(root);
  if (static_cast<int>(o.size()) >= size) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}

// A net file's header (0x004eb960) as text, with its reports. `entry` is open already.
namespace {
std::string g_reports;
void report_sink(int, const char*, int line, const char*) {
  char b[16];
  std::snprintf(b, sizeof b, "%x,", line);
  g_reports += b;
}
}  // namespace
extern "C" int probe_pm_ai_header(const char* entry, const char* base, int net, char* out, int size) {
  zrdr::Node* root = zrdr::load(entry);
  if (!root || root->type != zrdr::List) return -1;
  zrdr::Node* top = root->list[0].i > 1 && root->list[1].type == zrdr::List ? &root->list[1] : root;
  g_reports.clear();
  zerr::set_sink(report_sink);
  NetHeader h;
  h.net = net;
  h.parse(top, base);
  zerr::set_sink(nullptr);
  char b[128];
  std::snprintf(b, sizeof b, "%d %d %x %x %x %d %s ", h.version, h.has_pos, fb(h.pos[0]), fb(h.pos[1]), fb(h.pos[2]), h.first,
                h.name ? h.name : "-");
  const std::string o = b + g_reports;
  zrdr::free_node(root);
  if (static_cast<int>(o.size()) >= size) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
