#include "upgrades/valve_map/valve_map.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <list>
#include <set>

#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zreader/zreader.h"
#include "mech3/mech_fieldbase.h"
#include "mech3/zai_anim_owner.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_node.h"
#include "mech3/zainet_region.h"
#include "mech3/zrdrainet.h"
#include "mech3/zrdraiv.h"
#include "mech3/zrdrvalve.h"

namespace mech3 {
namespace {

std::string num(float f) {
  char b[32];
  std::snprintf(b, sizeof b, "%g", static_cast<double>(f));
  return b;
}

std::string metres(float squared) {  // the file's number is kept squared (AiParams::finish)
  char b[32];
  std::snprintf(b, sizeof b, "%g", static_cast<double>(std::sqrt(squared > 0 ? squared : 0)));
  return b;
}

std::string code(const std::string& s) { return "`" + s + "`"; }

struct Builder {
  ValveMap& m;
  const std::function<const void*(uint32_t)>& object_of;
  ValveMapEntry& at(const std::string& name) {
    ValveMapEntry& e = m.valves[name];
    e.name = name;
    return e;
  }
  std::string link(const std::string& name) { return "[" + name + "](#" + valve_map_anchor(name) + ")"; }
  std::string link(const ai::Valve* v) { return v && v->name ? link(std::string(v->name)) : std::string("(no valve)"); }
};

const char* kCondition[4] = {"any member nonzero", "any member zero", "all members nonzero", "all members zero"};
const char* kScans[4] = {"scan_aim", "scan_hit", "scan_time", "canleave"};

// valves.zrd: each record's start value, condition and actions (ai.md section 13).
void from_valves_file(Builder& b) {
  ai::ValveFile& f = ai::valve_file();
  f.add_ref();
  const int32_t n = f.record_count();
  for (int32_t i = 0; i < n; ++i) {
    const char* key = nullptr;
    ai::ValveDef* d = f.read(i, &key);
    if (!d || !key) continue;
    ++b.m.valve_records;
    const std::string me = key;
    ValveMapEntry& e = b.at(me);
    const std::string where = "valves.zrd record " + std::to_string(i + 1) + " (" + code(me) + ")";
    if (d->init.on) e.has_start = true, e.start = d->init.value;
    for (int k = 0; k < 4; ++k)
      if (d->flags & (1u << k)) {
        std::string members;
        for (const auto& nm : d->names.names)
          if (nm) {
            members += (members.empty() ? "" : ", ") + b.link(*nm);
            b.at(*nm).reads.push_back("valves.zrd: " + b.link(me) + "'s condition (" + kCondition[k] + ")");
          }
        e.sets.push_back(std::string(where) + ": its value is computed, " + kCondition[k] + " of " + members +
                         " (a set is ignored; a member's change fires its reactions)");
      }
    auto when = [&](int32_t v) { return "to " + std::to_string(v); };
    if (d->sound.name)
      e.effects.push_back(when(d->sound.value) + ": plays sound " + code(d->sound.name) + (d->sound.flag ? " (once)" : ""));
    if (d->anim.name)
      e.effects.push_back(when(d->anim.value) + ": runs animation " + code(d->anim.name) + (d->anim.flag ? " (once)" : ""));
    for (int k = 0; k < 4; ++k)
      if (d->scan[k].name)
        e.effects.push_back(when(d->scan[k].value) + ": " + kScans[k] + " " + (d->scan[k].flag ? "on" : "off") +
                            " for " + code(d->scan[k].name));
    if (d->shutdown.name)
      e.effects.push_back(when(d->shutdown.value) + ": " + (d->shutdown.flag ? "powers down " : "wakes (powers up) ") +
                          code(d->shutdown.name));
    if (d->destroy.name) e.effects.push_back(when(d->destroy.value) + ": destroys " + code(d->destroy.name));
    if (d->team_entity)
      e.effects.push_back(when(d->team_value) + ": puts " + code(d->team_entity) + " on team " + std::to_string(d->team));
    if (d->attack_entity)
      e.effects.push_back(when(d->attack_value) + ": attack strategy " + std::to_string(d->strategy) + " for " +
                          code(d->attack_entity));
    if (d->teleport)
      e.effects.push_back(when(d->teleport_value) + ": teleports " + code(d->teleport) + " to (" + num(d->pos[0]) + " " +
                          num(d->pos[1]) + " " + num(d->pos[2]) + "), yaw " + num(d->yaw));
    if (d->reset.on) e.effects.push_back(when(d->reset.value) + ": missionreset (never made: the game only reports it)");
    if (d->abort.on) e.effects.push_back(when(d->abort.value) + ": the mission is lost (missionabort)");
    if (d->complete.on) e.effects.push_back(when(d->complete.value) + ": missioncomplete (does nothing in the game)");
    if (d->update) {
      e.effects.push_back("any change: fires " + b.link(d->update) + "'s reactions (update), whatever its value");
      b.at(d->update).sets.push_back(where + ": fired by " + b.link(me) +
                                     "'s update on any change (its reactions run; its value is not changed)");
    }
    if (d->delay_valve) {
      float s;
      std::memcpy(&s, &d->delay, sizeof s);
      e.effects.push_back("any change: sets " + b.link(d->delay_valve) + " to " + me + "'s value " + num(s) +
                          " s later (delayupdate)");
      b.at(d->delay_valve).sets.push_back(where + ": " + num(s) + " s after " + b.link(me) + " changes, to " + me +
                                          "'s value (delayupdate)");
    }
  }
  f.release();
}

// aiv.zrd: every record's vlist (ai.md 17.5), the player's included.
void from_aiv(Builder& b) {
  ai::AivRecord rec;  // holds a reference on aiv.zrd
  const int32_t n = ai::aiv_file().record_count();
  for (int32_t i = 0; i < n; ++i) {
    const char* name = nullptr;
    ai::AivRecord* r = rec.read(i, &name);
    if (!r || !name || !r->vlist) continue;
    const bool player = std::strcmp(name, "player") == 0;
    int k = 0;
    for (const ai::VListEntry& v : *r->vlist) {
      ++k;
      ++b.m.vlist_entries;
      if (!v.valve || !v.valve->name) continue;
      const int want = v.flags >> 1 & 1;
      std::vector<std::string> checks;
      const uint8_t f = v.flags;
      if (f & 0x08) checks.push_back("hit within the last second (item 4)");
      if (f & 0x20) checks.push_back("heat >= " + num(v.f08) + " (items 7, 8)");
      if (f & 0x10) checks.push_back("health <= " + num(v.f04) + " (items 5, 6)");
      if (f & 0x04) checks.push_back("ammo <= " + num(v.f00) + " (items 2, 3)");
      if (f & 0x80) checks.push_back("nearest enemy closer than " + metres(v.f10) + " m (items 13, 14)");
      if (f & 0x40) checks.push_back("nearest enemy closer than " + metres(v.f0c) + " m with a clear line (items 11, 12)");
      const char* rn = v.region ? v.region->region_name : nullptr;
      if (rn && (v.flags2 & 3) == 3) checks.push_back(std::string("anywhere (region ") + code(rn) + ", items 9 and 17)");
      else if (rn && (v.flags2 & 1)) checks.push_back(std::string("inside ") + code(rn) + " (items 9, 10)");
      else if (rn && (v.flags2 & 2)) checks.push_back(std::string("outside ") + code(rn) + " (items 17, 10)");
      else if (rn) checks.push_back(std::string("region ") + code(rn) + " named, but neither inside nor outside asked: never");
      std::string text = "aiv.zrd " + code(name) + " vlist #" + std::to_string(k) + ": to " + std::to_string(want) + " when ";
      if (checks.empty()) text += "nothing (no check on: never)";
      for (size_t c = 0; c < checks.size(); ++c) text += (c ? ", or " : "") + checks[c];
      if (player) text += " (the player's own list)";
      else if (rn && r->team() == 1)
        text += "; this record is on the player's side: when its pilot dies its region checks pass to the player (ai.md 17.6)";
      b.at(v.valve->name).sets.push_back(text);
    }
  }
}

// targets.zrd: health_below's valves (ai.md section 3; zai_target.cpp 0x004d2460).
void from_targets(Builder& b) {
  ai::RecordFile& f = ai::targets_file();
  f.add_ref();
  const int32_t n = f.record_count();
  for (int32_t i = 0; i < n; ++i) {
    const char* key = nullptr;
    zrdr::Node* value = f.record(i, &key);
    if (!value || !key) continue;
    ai::TargetRec t;
    if (!t.parse(value, key) || !t.health_below) continue;
    for (const ai::HealthBelow& h : *t.health_below) {
      if (!h.valve || !h.valve->name) continue;
      ++b.m.target_links;
      b.at(h.valve->name).sets.push_back("targets.zrd " + code(key) + ": to " + std::to_string(h.a) + " when its health falls below " +
                                         num(h.health) + (h.health <= 0 ? " (destroyed)" : "") + " (health_below)");
    }
  }
  f.release();
}

// The AI nets (net_NN.zrd): the valve steps on their links (ai.md sections 9 and 17).
void from_nets(Builder& b) {
  for (int net = 1; net < 100; ++net) {
    const char* nm = ai::net_name(net);
    std::string file = std::string(nm) + ".zrd";
    zrdr::Node* probe = zrdr::load(file.c_str());  // the mission's archive has it (never fclose an archive's stream)
    if (!probe) continue;
    zrdr::free_node(probe);
    ai::NetRef ref(net, nullptr);
    b.m.sources.push_back(file);
    const int32_t n = ref.count();
    for (int32_t i = 0; i < n; ++i) {
      const char* key = nullptr;
      ai::NetNodeRec* r = ref.node(i, &key);
      if (!r) continue;
      const std::string where = code(std::string(nm) + " " + (key ? key : "?")) +
                                (r->auxname ? " (" + code(r->auxname) + ")" : std::string());
      auto add_read = [&](const ai::Valve* v, const std::string& how) {
        if (!v || !v->name) return;
        ++b.m.net_links;
        b.at(v->name).reads.push_back(where + ": " + how);
      };
      if (r->valve)
        add_read(r->valve, std::string("its links let a pilot by only while the valve holds ") +
                               (r->valve_has_value ? std::to_string(r->valve_value)
                                                   : "1 on the node's last link and 0 on the others") +
                               (r->valve_on ? "" : " (the step is off: its first item 0)"));
      if (r->deploy_set) {  // ai.md 17.4a, the deploy node's state (0x4d00a0)
        const std::string second = r->deploy_valve_b && r->deploy_valve_b->name ? b.link(r->deploy_valve_b) : std::string("(none)");
        if (r->deploy_valve_a && r->deploy_valve_a->name) {
          ++b.m.net_links;
          b.at(r->deploy_valve_a->name)
              .sets.push_back(where + ": to " + std::to_string(r->deploy_a) + " by a pilot holding on this deploy node (while " +
                              b.link("mcbdeploy") + " is 1 and " + second + " is not " + std::to_string(r->deploy_b) + ")");
        }
        add_read(r->deploy_valve_b, "deploy node: a pilot holds here while " + b.link("mcbdeploy") + " is 1 and this valve is not " +
                                        std::to_string(r->deploy_b));
        b.at("mcbdeploy").reads.push_back(where + ": deploy node (the pilot holds here only while it is 1)");
      }
      if (r->mcbwait_set)
        add_read(r->mcbwait_valve, "mcbwait: the pilot waits here until " + num(r->mcbwait_time) +
                                       " s of game time or until the valve holds " + std::to_string(r->mcbwait_b));
      if (r->valve_union)
        for (const ai::ValveCount& c : r->valve_union->valves)
          add_read(c.valve, "valve union (count " + std::to_string(c.count) + "): the way finder's choice of link");
      if (r->valve_assign && r->valve_assign->valve && r->valve_assign->valve->name) {
        ++b.m.net_links;
        b.at(r->valve_assign->valve->name)
            .sets.push_back(where + ": to " + std::to_string(r->valve_assign->value) + " when a pilot takes its links (valve_assign)");
      }
    }
  }
}

// objectives.zrd's block m<N> (mech_objective.cpp's parse_condition): valve_change reads, set_valve sets.
void from_objectives(Builder& b) {
  zrdr::Node* root = zrdr::load("objectives.zrd");
  if (!root) return;
  const std::string block = "m" + std::to_string(b.m.number);
  zrdr::Node* blk = zrdr::find(root, block.c_str());
  auto str = [](zrdr::Node* n, const char* key) -> std::string {
    const char* s = n ? zrdr::get_string(n, key) : nullptr;
    return s ? s : "";
  };
  std::function<void(zrdr::Node*, const std::string&)> walk = [&](zrdr::Node* list, const std::string& path) {
    if (!list || list->type != zrdr::List) return;
    const int32_t n = (list->list[0].i - 1) / 2;
    for (int32_t i = 0; i < n; ++i) {
      zrdr::Node* slot = &list->list[1 + 2 * i];
      if (slot->type != zrdr::String) continue;
      zrdr::Node* args = slot + 1;
      const std::string type = slot->s;
      if (type == "desc" || type == "sound" || type == "visible" || type == "silent") continue;
      const std::string desc = str(args, "desc"), nav = str(args, "nav");
      std::string here = path + " > " + code(type);
      if (!desc.empty() || !nav.empty())
        here += " (" + (desc.empty() ? std::string() : code(desc)) + (desc.empty() || nav.empty() ? "" : ", ") +
                (nav.empty() ? std::string() : "nav " + code(nav)) + ")";
      if (type == "valve_change") {
        const std::string v = str(args, "valve");
        int32_t value = 1;
        zrdr::get_int(args, "value", &value);
        if (!v.empty()) {
          ++b.m.objective_links;
          b.at(v).reads.push_back("objectives.zrd " + here + ": done when the valve is " + std::to_string(value));
        }
      }
      zrdr::Node* sv = zrdr::find_flat(args, "set_valve", 1);
      if (sv && sv->type == zrdr::List && sv->list[0].i > 2 && sv->list[1].type == zrdr::String && sv->list[2].type == zrdr::Int) {
        ++b.m.objective_links;
        b.at(sv->list[1].s).sets.push_back("objectives.zrd " + here + ": to " + std::to_string(sv->list[2].i) +
                                           " when the objective is done (set_valve)");
      }
      if (type == "obj_list") walk(zrdr::find(args, "list"), here);
    }
  };
  if (blk)
    for (const char* group : {"primary", "secondary", "final", "fail"}) walk(zrdr::find(blk, group), code(block) + " " + group);
  zrdr::free_node(root);
}

// The animations bound to valves (zai_anim_owner.cpp, 0x004d4b30): a def whose callback events carry
// 0x2a..0x31 sets the valve named after its node (or a parent up to three up) to the value's low bit.
void from_anims(Builder& b) {
  const std::list<ai::AnimOwnerRec*>& recs = ai::anim_owner_records();
  for (zeff::AnimDef* d = zeff::anim_next_with_callbacks(nullptr); d; d = zeff::anim_next_with_callbacks(d)) {
    ai::AnimOwnerRec* rec = static_cast<ai::AnimOwnerRec*>(d->callback_arg);
    bool ours = false;
    for (const ai::AnimOwnerRec* r : recs) ours = ours || r == rec;
    if (!ours || !rec->valve) continue;
    const auto* v = static_cast<const ai::Valve*>(b.object_of(rec->valve));
    if (!v || !v->name) continue;
    std::set<int32_t> values;
    zeff::anim_each_callback(d, [](zeff::AnimDef*, void* ctx, int32_t x) { static_cast<std::set<int32_t>*>(ctx)->insert(x); }, &values);
    for (int32_t x : values) {
      if (x < 0x2a || x > 0x31) continue;
      ++b.m.anim_links;
      const char* node = d->node ? d->node->name() : nullptr;
      char hex[8];
      std::snprintf(hex, sizeof hex, "0x%02x", x);
      b.at(v->name).sets.push_back("anim.zbd " + code(d->name()) + (node ? " at node " + code(node) : std::string()) +
                                   ": to " + std::to_string(x & 1) + " by its callback event (value " + hex + ": the valve named after " +
                                   (x < 0x2c ? std::string("its node") : "the node " + std::to_string((x - 0x2a) >> 1) + " up from it") + ")");
    }
  }
}

// The field bases' own valves (mech_fieldbase.cpp, mech_fieldbase_dock.cpp).
void from_field_bases(Builder& b) {
  FieldBases& r = field_bases();
  if (r.deploy_valve && r.deploy_valve->name)
    b.at(r.deploy_valve->name)
        .sets.push_back("the field base code: 0 at the mission start, 1 when the mobile field base is called to deploy (mech_fieldbase_dock.cpp)");
  for (ai::Valve* v : r.valves)
    if (v && v->name)
      b.at(v->name).sets.push_back("the field base code: this base's own valve (named after its node): 0 at the start and when the "
                                   "bases are called, 1 when it arrives at its place (mech_fieldbase_dock.cpp)");
}

std::string list_md(const std::vector<std::string>& v) {
  std::string s;
  for (const std::string& x : v) s += "- " + x + "\n";
  return s;
}

}  // namespace

std::string valve_map_anchor(const std::string& name) {
  std::string a = "v-";
  for (char c : name) a += std::isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : '-';
  return a;
}

ValveMap valve_map_collect(const std::string& mission, const std::string& dir, int number,
                           const std::function<const void*(uint32_t)>& object_of) {
  ValveMap m;
  m.mission = mission, m.dir = dir, m.number = number;
  Builder b{m, object_of};
  m.sources = {"valves.zrd", "aiv.zrd", "targets.zrd"};
  from_valves_file(b);
  from_aiv(b);
  from_targets(b);
  from_nets(b);
  m.sources.push_back("objectives.zrd (block m" + std::to_string(number) + ")");
  from_objectives(b);
  m.sources.push_back("anim.zbd (the defs bound to valves)");
  from_anims(b);
  from_field_bases(b);
  for (ai::Valve* v : ai::valves()) {
    if (!v || !v->name) continue;
    ValveMapEntry& e = b.at(v->name);
    e.registered = true;
    e.now = v->get();
  }
  return m;
}

std::string valve_map_markdown(const ValveMap& m) {
  std::string s;
  s += "# Valve map: " + m.mission + "\n\n";
  s += "Generated by `mw3 --mission " + m.mission + " --valve-map DIR` from the game's own files (see [README.md](README.md): how to "
       "read it and how to make the others). Do not edit by hand.\n\n";
  s += "Read: ";
  for (size_t i = 0; i < m.sources.size(); ++i) s += (i ? ", " : "") + m.sources[i];
  s += ".\n\n";
  s += std::to_string(m.valves.size()) + " valves; " + std::to_string(m.valve_records) + " valves.zrd records, " +
       std::to_string(m.vlist_entries) + " vlist entries, " + std::to_string(m.target_links) + " target triggers, " +
       std::to_string(m.net_links) + " net links, " + std::to_string(m.objective_links) + " objective links, " +
       std::to_string(m.anim_links) + " animation links.\n\n";
  s += "## Index\n\n| Valve | Start | Set by | Read by | Does |\n|---|---|---|---|---|\n";
  for (const auto& [name, e] : m.valves)
    s += "| [" + name + "](#" + valve_map_anchor(name) + ") | " + std::to_string(e.has_start ? e.start : 0) + " | " +
         std::to_string(e.sets.size()) + " | " + std::to_string(e.reads.size()) + " | " + std::to_string(e.effects.size()) + " |\n";
  s += "\n## Valves\n";
  for (const auto& [name, e] : m.valves) {
    s += "\n### <a id=\"" + valve_map_anchor(name) + "\"></a>" + name + "\n\n";
    s += "Start: " + std::to_string(e.has_start ? e.start : 0) + (e.has_start ? " (valves.zrd `value`)" : " (no `value`: 0)");
    if (e.registered && e.now != (e.has_start ? e.start : 0)) s += "; at the first frame " + std::to_string(e.now);
    if (!e.registered) s += "; not in the registry at the first frame";
    s += ".\n\n";
    s += "**Set by**\n\n" + (e.sets.empty() ? std::string("- nothing in the mission's files\n") : list_md(e.sets)) + "\n";
    s += "**Read by**\n\n" + (e.reads.empty() ? std::string("- nothing in the mission's files\n") : list_md(e.reads));
    if (!e.effects.empty()) s += "\n**When it changes** (valves.zrd)\n\n" + list_md(e.effects);
  }
  return s;
}

}  // namespace mech3
