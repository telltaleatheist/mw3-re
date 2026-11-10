// The AI net file's text writers (mech3/zrdrainet: NetHeader::write 0x004dad10,
// NetFile::write_record 0x004db8d0) for tests/differential/test_ai_netwrite.py.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/net_save.h"
#include "mech3/zai_valve.h"
#include "mech3/zrdrainet.h"

using namespace mech3::ai;

namespace {
template <class T>
T at(const uint8_t* img, int off) {
  T v;
  std::memcpy(&v, img + off, sizeof v);
  return v;
}

// What the writer put in a temporary file, into out (at most size bytes).
template <class F>
int32_t written(char* out, int32_t size, F&& write) {
  std::FILE* f = std::tmpfile();
  if (!f) return -1;
  write(f);
  const long n = std::ftell(f);
  std::rewind(f);
  const size_t got = std::fread(out, 1, static_cast<size_t>(n < size ? n : size), f);
  std::fclose(f);
  return static_cast<int32_t>(got);
}
}  // namespace

extern "C" {

// A record as the exe lays it out (0xb4 bytes: the scalars at their offsets),
// with its links, its strings (name, auxname, gate) and valves by name (valve,
// deploy's two, mcbwait's, valve_assign's; null for none), the valveunion's
// items (n < 0: none; a null name an empty slot) and valve_assign's value.
int32_t probe_netwrite_record(const uint8_t* img, const int32_t* links, int32_t nlinks, const char* const* strs,
                              const char* const* union_names, const int32_t* union_counts, int32_t nunion,
                              int32_t assign_value, int32_t version, int32_t net, const char* key, char* out,
                              int32_t size) {
  std::vector<std::unique_ptr<Valve>> valves;
  const auto valve = [&](const char* name) -> Valve* {
    if (!name) return nullptr;
    valves.push_back(std::make_unique<Valve>(name));
    return valves.back().get();
  };
  NetNodeRec r;
  r.kind = at<int32_t>(img, 0x04), r.index = at<int32_t>(img, 0x08);
  for (int i = 0; i < 3; ++i) r.pos[i] = at<float>(img, 0x0c + 4 * i);
  r.speed_on = at<int32_t>(img, 0x18), r.speed = at<float>(img, 0x1c);
  r.wait_interval_on = at<int32_t>(img, 0x20), r.wait_interval = at<float>(img, 0x24);
  r.wait_time_on = at<int32_t>(img, 0x28), r.wait_time = at<float>(img, 0x2c);
  r.valve_on = at<int32_t>(img, 0x30), r.valve_has_value = img[0x38], r.valve_value = at<int32_t>(img, 0x3c);
  r.canleave[0] = at<int32_t>(img, 0x40), r.canleave[1] = at<int32_t>(img, 0x44);
  r.jump_on = at<int32_t>(img, 0x48), r.jump = at<float>(img, 0x4c);
  for (int i = 0; i < 2; ++i) {
    r.scan_aim[i] = at<int32_t>(img, 0x50 + 4 * i);
    r.scan_hit[i] = at<int32_t>(img, 0x58 + 4 * i);
    r.scan_time[i] = at<int32_t>(img, 0x60 + 4 * i);
  }
  r.hidden_set = img[0x68], r.hidden = img[0x69], r.fixed_set = img[0x6a], r.fixed = img[0x6b];
  r.autonext_set = img[0x6c], r.autonext = img[0x6d], r.visible_set = img[0x6e], r.visible = img[0x6f];
  r.deploy_set = img[0x70], r.deploy_a = img[0x71], r.deploy_b = img[0x72], r.deploy_c = at<int16_t>(img, 0x74);
  r.arrange_set = img[0x80], r.arrange = at<float>(img, 0x84);
  r.mcbwait_set = img[0x88], r.mcbwait_b = img[0x89], r.mcbwait_time = at<float>(img, 0x8c);
  r.backup = img[0x98], r.shunt = img[0x99];
  r.name = strs[0] ? strdup(strs[0]) : nullptr;
  r.auxname = strs[1] ? strdup(strs[1]) : nullptr;
  r.gate = strs[2] ? strdup(strs[2]) : nullptr;
  r.valve = valve(strs[3]);
  r.deploy_valve_a = valve(strs[4]), r.deploy_valve_b = valve(strs[5]);
  r.mcbwait_valve = valve(strs[6]);
  if (strs[7]) r.valve_assign = new ValveAssign{valve(strs[7]), assign_value};
  r.links.assign(links, links + nlinks);
  if (nunion >= 0) {
    r.valve_union = new ValveUnion;
    for (int32_t i = 0; i < nunion; ++i) r.valve_union->valves.push_back({valve(union_names[i]), union_counts[i]});
  }
  NetFile file(net, "net_00.zrd");
  file.header.version = version;
  return written(out, size, [&](std::FILE* f) { file.write_record(r, f, key); });
}

// The header of a file named `file` (its base the name up to its '.').
int32_t probe_netwrite_header(int32_t net, int32_t version, int32_t has_pos, const float* pos, const char* name,
                              const char* file, char* out, int32_t size) {
  NetFile f(net, file);
  f.header.version = version, f.header.has_pos = has_pos;
  for (int i = 0; i < 3; ++i) f.header.pos[i] = pos[i];
  f.header.name = name ? strdup(name) : nullptr;
  return written(out, size, [&](std::FILE* t) { f.write_header(t); });
}

// An archive saved as the editor saves it (net_save.h), for test_net_save_mech3ax.py: mode 0 as it is (each
// entry its own bytes), 1 every net, region and waynet file's nodes re-encoded from their records, 2 net_01.zrd
// with test_net_save.cpp's edits (node_00 moved, node_01's speed, node_99 added). The entries written, -1 on failure.
int32_t probe_net_resave(const char* src, const char* dst, int32_t mode) {
  static std::set<std::string> sounds;
  RdrHooks saved = rdr_hooks();
  rdr_hooks().localize = [](const char* s) { return s; };
  rdr_hooks().sound_find = [](const char* s) -> void* { return const_cast<std::string*>(&*sounds.insert(s).first); };
  std::ifstream in(src, std::ios::binary);
  std::vector<uint8_t> a((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (a.size() < 8) return -1;
  uint32_t count;
  std::memcpy(&count, a.data() + a.size() - 4, 4);
  const size_t table = a.size() - 8 - count * sizeof(zutl_zar::Entry);
  int32_t written = 0;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry e;
    std::memcpy(&e, a.data() + table + i * sizeof e, sizeof e);
    e.name[sizeof e.name - 1] = 0;
    entries.emplace_back(e.name, std::vector<uint8_t>(a.begin() + e.start, a.begin() + e.start + e.length));
  }
  for (auto& [name, data] : entries) {
    const std::string n = name;
    const bool net = n.size() > 4 && n.compare(n.size() - 4, 4, ".zrd") == 0 &&
                     (n.rfind("net_", 0) == 0 || n.rfind("reg_", 0) == 0 || n.find("waynet") != std::string::npos);
    if (mode == 0 || !net || (mode == 2 && n != "net_01.zrd")) {
      a = archive_with_entry(a, name.c_str(), data);
      ++written;
      continue;
    }
    std::FILE* f = std::tmpfile();
    std::fwrite(data.data(), 1, data.size(), f);
    std::rewind(f);
    zrdr::Node tree{};
    zrdr::read_node(f, &tree);
    std::fclose(f);
    NetFile file(0, "net_00.zrd");
    std::map<std::string, NetNodeRec> recs;
    if (tree.type == zrdr::List && tree.list[0].i >= 2 && tree.list[1].type == zrdr::List) {
      zrdr::Node& kv = tree.list[1];
      for (int32_t i = 1; i + 1 < kv.list[0].i; i += 2)
        if (kv.list[i].type == zrdr::String && std::strncmp(kv.list[i].s, "node_", 5) == 0 && !recs.count(kv.list[i].s)) {
          NetNodeRec r;
          file.read_node(&r, &kv.list[i + 1], kv.list[i].s);
          recs.emplace(kv.list[i].s, r);
        }
    }
    std::vector<std::pair<std::string, const NetNodeRec*>> edits;
    NetNodeRec added;
    if (mode == 1) {
      for (const auto& [k, r] : recs) edits.emplace_back(k, &r);
    } else {
      NetNodeRec& moved = recs["node_00"];
      moved.pos[0] = 1234.5678f, moved.pos[2] = -0.04999f;
      NetNodeRec& fast = recs["node_01"];
      fast.speed_on = 1, fast.speed = 0.3333333f;
      added.kind = 1, added.index = 99, added.links = {0, 1, 3};
      added.valve = valve_get("door_a"), added.valve_on = 1, added.valve_has_value = 1, added.valve_value = 7;
      added.hidden_set = 1, added.hidden = 1, added.shunt = 1;
      added.valve_union = new ValveUnion;
      added.valve_union->valves = {{valve_get("u1"), 2}};
      added.auxname = strdup("Added");
      edits = {{"node_00", &moved}, {"node_01", &fast}, {"node_99", &added}};
    }
    const std::vector<uint8_t> out = net_with_edits(data.data(), data.size(), &tree, edits);
    zrdr::free_contents(&tree);
    if (out.empty()) return rdr_hooks() = saved, -1;
    a = archive_with_entry(a, name.c_str(), out);
    ++written;
  }
  rdr_hooks() = saved;
  valves_clear();
  std::ofstream(dst, std::ios::binary).write(reinterpret_cast<const char*>(a.data()), static_cast<std::streamsize>(a.size()));
  return written;
}

}  // extern "C"
