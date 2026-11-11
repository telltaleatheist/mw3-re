// The AI net editor's Save Changes (mech3/net_save, docs/editor.md): every
// shipped net, region and waynet file saved unedited is the same bytes, and
// with every node re-encoded from its record reads back as the same records;
// every shipped archive rewritten with an entry's own bytes is the same file;
// the editor's own mod: made with its marker, reused, a folder not ours passed
// over, an earlier save kept as .bak, turned on just before its source.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/mod_export.h"
#include "mech3/mod_install.h"
#include "upgrades/editor/form.h"
#include "upgrades/editor/save_as.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/mods.h"
#include "mech3/net_save.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet.h"
#include "mech3/zainetedit.h"
#include "mech3/zrdrainet.h"

namespace {
namespace fs = std::filesystem;
using mech3::ai::NetNodeRec;

std::vector<uint8_t> file_bytes(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
std::string text_of(const fs::path& p) {
  const std::vector<uint8_t> b = file_bytes(p);
  return std::string(b.begin(), b.end());
}
void put(const fs::path& p, const std::string& text) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p, std::ios::binary) << text;
}

// An entry's tree as the game reads it.
zrdr::Node* tree_of(const std::vector<uint8_t>& b) {
  std::FILE* f = std::tmpfile();
  if (!f) return nullptr;
  std::fwrite(b.data(), 1, b.size(), f);
  std::rewind(f);
  auto* n = new zrdr::Node{};
  zrdr::read_node(f, n);
  std::fclose(f);
  return n;
}
void free_tree(zrdr::Node* n) {
  if (!n) return;
  zrdr::free_contents(n);
  delete n;
}

// The node_ keys of a net tree with their records (as NetFile reads them).
std::map<std::string, NetNodeRec> records(zrdr::Node* tree) {
  std::map<std::string, NetNodeRec> out;
  if (!tree || tree->type != zrdr::List || tree->list[0].i < 2 || tree->list[1].type != zrdr::List) return out;
  mech3::ai::NetFile file(0, "net_00.zrd");
  zrdr::Node& kv = tree->list[1];
  for (int32_t i = 1; i + 1 < kv.list[0].i; i += 2) {
    if (kv.list[i].type != zrdr::String || std::strncmp(kv.list[i].s, "node_", 5) != 0) continue;
    NetNodeRec r;
    file.read_node(&r, &kv.list[i + 1], kv.list[i].s);
    out.emplace(kv.list[i].s, r);
  }
  return out;
}

bool same_str(const char* a, const char* b) { return a == b || (a && b && std::strcmp(a, b) == 0); }
bool same_f(float a, float b) { return std::memcmp(&a, &b, 4) == 0; }
// Every field the reader fills; the first that differs, or "".
std::string differs(const NetNodeRec& a, const NetNodeRec& b) {
  if (a.kind != b.kind || a.index != b.index) return "kind/index";
  for (int i = 0; i < 3; ++i)
    if (!same_f(a.pos[i], b.pos[i])) return "pos";
  if (a.speed_on != b.speed_on || !same_f(a.speed, b.speed)) return "speed";
  if (a.wait_interval_on != b.wait_interval_on || !same_f(a.wait_interval, b.wait_interval)) return "wait_interval";
  if (a.wait_time_on != b.wait_time_on || !same_f(a.wait_time, b.wait_time)) return "wait_time";
  if (a.valve_on != b.valve_on || a.valve != b.valve || a.valve_has_value != b.valve_has_value ||
      a.valve_value != b.valve_value)
    return "valve";
  if (a.canleave[0] != b.canleave[0] || a.canleave[1] != b.canleave[1]) return "canleave";
  if (a.jump_on != b.jump_on || !same_f(a.jump, b.jump)) return "jump";
  for (int i = 0; i < 2; ++i)
    if (a.scan_aim[i] != b.scan_aim[i] || a.scan_hit[i] != b.scan_hit[i] || a.scan_time[i] != b.scan_time[i])
      return "scan";
  if (std::memcmp(&a.hidden_set, &b.hidden_set, 8) != 0) return "flags";
  if (a.deploy_set != b.deploy_set || a.deploy_a != b.deploy_a || a.deploy_b != b.deploy_b ||
      a.deploy_c != b.deploy_c || a.deploy_valve_a != b.deploy_valve_a || a.deploy_valve_b != b.deploy_valve_b)
    return "deploy";
  if (a.arrange_set != b.arrange_set || !same_f(a.arrange, b.arrange)) return "arrangenodes";
  if (a.mcbwait_set != b.mcbwait_set || a.mcbwait_b != b.mcbwait_b || !same_f(a.mcbwait_time, b.mcbwait_time) ||
      a.mcbwait_valve != b.mcbwait_valve)
    return "mcbwait";
  if (a.sound != b.sound) return "sound";
  if (a.backup != b.backup || a.shunt != b.shunt) return "backup/shunt";
  if (!same_str(a.name, b.name)) return "name";
  if (!same_str(a.auxname, b.auxname)) return "auxname";
  if (a.links != b.links) return "links";
  if (!same_str(a.gate, b.gate)) return "gate";
  if (!a.valve_union != !b.valve_union) return "valveunion";
  if (a.valve_union) {
    if (a.valve_union->valves.size() != b.valve_union->valves.size()) return "valveunion size";
    for (size_t i = 0; i < a.valve_union->valves.size(); ++i)
      if (a.valve_union->valves[i].valve != b.valve_union->valves[i].valve ||
          a.valve_union->valves[i].count != b.valve_union->valves[i].count)
        return "valveunion item";
  }
  if (!a.valve_assign != !b.valve_assign) return "valve_assign";
  if (a.valve_assign && (a.valve_assign->valve != b.valve_assign->valve || a.valve_assign->value != b.valve_assign->value))
    return "valve_assign item";
  return "";
}

// The reader's hooks: every name shown as "L:<name>" (so a record's name is
// not its written one) and a sound for each name.
const char* fake_localize(const char* s) {
  static std::set<std::string> names;
  return names.insert(std::string("L:") + s).first->c_str();
}
void* fake_sound(const char* s) {
  static std::set<std::string> names;
  return const_cast<std::string*>(&*names.insert(s).first);
}
struct Hooks {
  mech3::ai::RdrHooks saved = mech3::ai::rdr_hooks();
  Hooks() { mech3::ai::rdr_hooks().localize = fake_localize, mech3::ai::rdr_hooks().sound_find = fake_sound; }
  ~Hooks() { mech3::ai::rdr_hooks() = saved; }
};

struct Entry {
  std::string name;
  std::vector<uint8_t> data;
};
std::vector<Entry> entries(const std::vector<uint8_t>& a) {
  std::vector<Entry> out;
  if (a.size() < 8) return out;
  uint32_t version, count;
  std::memcpy(&version, a.data() + a.size() - 8, 4), std::memcpy(&count, a.data() + a.size() - 4, 4);
  if (version != 1 || count * sizeof(zutl_zar::Entry) > a.size() - 8) return out;
  const size_t table = a.size() - 8 - count * sizeof(zutl_zar::Entry);
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry e;
    std::memcpy(&e, a.data() + table + i * sizeof e, sizeof e);
    e.name[sizeof e.name - 1] = 0;
    if (e.start <= table && e.length <= table - e.start)
      out.push_back({e.name, std::vector<uint8_t>(a.begin() + e.start, a.begin() + e.start + e.length)});
  }
  return out;
}
bool is_net(const std::string& n) {
  return n.size() > 4 && n.compare(n.size() - 4, 4, ".zrd") == 0 &&
         (n.rfind("net_", 0) == 0 || n.rfind("reg_", 0) == 0 || n.find("waynet") != std::string::npos);
}
}  // namespace

TEST(net_save_every_shipped_net_round_trips) {
  if (!fs::exists(MW3_GAME_DIR)) return;
  Hooks hooks;
  int archives = 0, archives_same = 0, nets = 0, nets_same = 0, nodes = 0, nodes_same = 0, bytes_same = 0;
  std::string first_bad;
  for (const auto& f : fs::recursive_directory_iterator(fs::path(MW3_GAME_DIR) / "zbd")) {
    if (f.path().extension() != ".zbd") continue;
    const std::vector<uint8_t> a = file_bytes(f.path());
    const std::vector<Entry> es = entries(a);
    if (es.empty()) continue;
    ++archives;
    if (mech3::ai::archive_with_entry(a, es[0].name.c_str(), es[0].data) == a) ++archives_same;
    else if (first_bad.empty()) first_bad = f.path().string() + ": rewritten as it is, not the same file";
    for (const Entry& e : es) {
      if (!is_net(e.name)) continue;
      ++nets;
      zrdr::Node* tree = tree_of(e.data);
      if (mech3::ai::net_with_edits(e.data.data(), e.data.size(), tree, {}) == e.data) ++nets_same;
      else if (first_bad.empty()) first_bad = f.path().string() + " " + e.name + ": saved unedited, not the same bytes";
      const std::map<std::string, NetNodeRec> was = records(tree);
      std::vector<std::pair<std::string, const NetNodeRec*>> edits;
      for (const auto& [k, r] : was) edits.emplace_back(k, &r);
      const std::vector<uint8_t> out = mech3::ai::net_with_edits(e.data.data(), e.data.size(), tree, edits);
      if (out == e.data) ++bytes_same;
      zrdr::Node* back = tree_of(out);
      const std::map<std::string, NetNodeRec> now = records(back);
      for (const auto& [k, r] : was) {
        ++nodes;
        auto it = now.find(k);
        const std::string d = it == now.end() ? std::string("missing") : differs(r, it->second);
        if (d.empty()) ++nodes_same;
        else if (first_bad.empty()) first_bad = f.path().string() + " " + e.name + " " + k + ": " + d;
      }
      free_tree(back);
      free_tree(tree);
    }
  }
  mech3::ai::valves_clear();
  std::printf("  %d archives (%d the same rewritten), %d net files (%d the same unedited, %d the same re-encoded), "
              "%d nodes (%d read back the same)\n", archives, archives_same, nets, nets_same, bytes_same, nodes, nodes_same);
  if (!first_bad.empty()) std::printf("  first: %s\n", first_bad.c_str());
  EXPECT(archives > 50 && archives_same == archives);
  EXPECT(nets > 700 && nets_same == nets);
  EXPECT(nodes > 19000 && nodes_same == nodes);
}

// The editor's own mod: the install's archive saved twice (its folder made
// with the marker and mod.txt, last among the run's entry's ticks; the second save over it, the
// first kept as .bak), a folder of that name not ours passed over for " (2)",
// a mod's archive saved into "<its name> edits" just before it, and an editor
// mod's own copy saved over in place.
TEST(net_save_into_the_editors_own_mod) {
  const fs::path root = fs::path(MW3_TEST_TMP_ROOT) / "net_save";
  std::error_code ec;
  fs::remove_all(root, ec);
  const fs::path inst = root / "install", wd = root / "write";
  const zutl::Mounts saved = zutl::mounts();
  const auto archive = [](const fs::path& p, const std::string& net) {
    fs::create_directories(p.parent_path());
    zutl_zar::Archive a;
    zutl_zar::init(a);
    zutl_zar::create(a, p.string().c_str());
    zutl_zar::add(a, "other.zrd", "x", 1);
    zutl_zar::add(a, "net_01.zrd", net.data(), static_cast<uint32_t>(net.size()));
    zutl_zar::close(a);
    zutl_zar::destroy(a);
  };
  archive(inst / "zbd" / "c1" / "readerm1.zbd", "install");
  put(wd / "mw3.cfg", "width=640\nmod=zzz\n");  // never read or written: the run's entry's ticks are (campaign_entries.h)
  EXPECT(mech3::entry_mods_save(wd.string(), {"a", "foo", "b"}));
  const auto mount = [&](const std::vector<std::string>& mods) {
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd.string());
    for (const std::string& m : mods) zutl::mount_mod((wd / "mods" / m).string());
    zutl::mount(inst.string());
  };
  const auto entry = [&](const fs::path& p) {
    for (const Entry& e : entries(file_bytes(p)))
      if (e.name == "net_01.zrd") return std::string(e.data.begin(), e.data.end());
    return std::string("(none)");
  };
  const std::vector<uint8_t> one = {'o', 'n', 'e'}, two = {'t', 'w', 'o'};
  const char* rel = "zbd\\c1\\readerm1.zbd";
  // A folder "editor edits" that is not ours (no marker): passed over.
  put(wd / "mods" / "editor edits" / "mod.txt", "name=Someone's\n");
  mount({});
  mech3::ai::NetSave s = mech3::ai::save_archive_edit(rel, "net_01.zrd", one);
  EXPECT(s.ok && s.folder == "editor edits (2)");
  EXPECT(text_of(wd / "mods" / "editor edits" / "mod.txt") == "name=Someone's\n");
  const fs::path ours = wd / "mods" / "editor edits (2)";
  EXPECT(fs::is_regular_file(ours / mech3::ai::kEditorMarker));
  EXPECT(text_of(ours / "mod.txt") == "name=editor edits\nfor=mw3 1.2\n");
  EXPECT(entry(ours / "zbd" / "c1" / "readerm1.zbd") == "one");
  EXPECT(entry(inst / "zbd" / "c1" / "readerm1.zbd") == "install");  // the install untouched
  EXPECT((mech3::entry_mods_on(wd.string()) == std::vector<std::string>{"a", "foo", "b", "editor edits (2)"}));
  EXPECT(s.message.find("editor edits (2)") != std::string::npos && s.message.find("on for MechWarrior 3: play it from MODS") != std::string::npos);
  // Saved again from the install: the same folder, the first save kept as .bak.
  s = mech3::ai::save_archive_edit(rel, "net_01.zrd", two);
  EXPECT(s.ok && s.folder == "editor edits (2)");
  EXPECT(entry(ours / "zbd" / "c1" / "readerm1.zbd") == "two");
  EXPECT(entry(ours / "zbd" / "c1" / "readerm1.zbd.bak") == "one");
  EXPECT((mech3::entry_mods_on(wd.string()) == std::vector<std::string>{"a", "foo", "b", "editor edits (2)"}));
  // A mod's archive: "<its name> edits", just before it; the mod untouched.
  put(wd / "mods" / "foo" / "mod.txt", "name=Foo\nfor=mw3 1.2\n");
  archive(wd / "mods" / "foo" / "zbd" / "c1" / "readerm1.zbd", "foo's");
  mount({"a", "foo", "b"});
  s = mech3::ai::save_archive_edit(rel, "net_01.zrd", one);
  EXPECT(s.ok && s.folder == "Foo edits");
  EXPECT(entry(wd / "mods" / "Foo edits" / "zbd" / "c1" / "readerm1.zbd") == "one");
  EXPECT(entry(wd / "mods" / "foo" / "zbd" / "c1" / "readerm1.zbd") == "foo's");
  EXPECT(text_of(wd / "mods" / "Foo edits" / "mod.txt") == "name=Foo edits\nfor=mw3 1.2\n");
  EXPECT((mech3::entry_mods_on(wd.string()) ==
          std::vector<std::string>{"a", "Foo edits", "foo", "b", "editor edits (2)"}));
  // Its own copy found first: saved over in place, the order kept.
  mount({"a", "Foo edits", "foo", "b"});
  s = mech3::ai::save_archive_edit(rel, "net_01.zrd", two);
  EXPECT(s.ok && s.folder == "Foo edits");
  EXPECT(entry(wd / "mods" / "Foo edits" / "zbd" / "c1" / "readerm1.zbd") == "two");
  EXPECT(entry(wd / "mods" / "Foo edits" / "zbd" / "c1" / "readerm1.zbd.bak") == "one");
  EXPECT((mech3::entry_mods_on(wd.string()) ==
          std::vector<std::string>{"a", "Foo edits", "foo", "b", "editor edits (2)"}));
  EXPECT(text_of(wd / "mw3.cfg") == "width=640\nmod=zzz\n");
  // No such entry, or no archive: refused, nothing written.
  s = mech3::ai::save_archive_edit(rel, "net_99.zrd", one);
  EXPECT(!s.ok && !s.error.empty());
  s = mech3::ai::save_archive_edit("zbd\\c9\\readerm1.zbd", "net_01.zrd", one);
  EXPECT(!s.ok && !fs::exists(wd / "mods" / "editor edits (3)"));
  zutl::mounts() = saved;
  fs::remove_all(root, ec);
}

// Edits through the game's own reader: c1m1's net_01.zrd with node_00 moved
// to a place %6.1f would round, node_01's speed changed, and a new node_99
// (no position; a valve with a value, deploy, valveunion, flags, a name),
// saved into a copy of readerm1.zbd, which the reader opens as the game does
// (zrdr::open_archive, NetFile::load): the edited records read back as they
// were saved, every other node and entry as it was.
TEST(net_save_edits_read_back_through_the_reader) {
  const fs::path src = fs::path(MW3_GAME_DIR) / "zbd" / "c1" / "readerm1.zbd";
  if (!fs::exists(src)) return;
  Hooks hooks;
  const std::vector<uint8_t> a = file_bytes(src);
  std::vector<uint8_t> net;
  for (const Entry& e : entries(a))
    if (e.name == "net_01.zrd") net = e.data;
  EXPECT(!net.empty());
  zrdr::Node* tree = tree_of(net);
  std::map<std::string, NetNodeRec> was = records(tree);
  EXPECT(was.count("node_00") && was.count("node_01") && !was.count("node_99"));
  NetNodeRec moved = was["node_00"], fast = was["node_01"], added;
  moved.pos[0] = 1234.5678f, moved.pos[2] = -0.04999f;
  fast.speed_on = 1, fast.speed = 0.3333333f;
  added.kind = 1, added.index = 99, added.links = {0, 1, 3};
  added.valve = mech3::ai::valve_get("door_a"), added.valve_on = 1, added.valve_has_value = 1, added.valve_value = 7;
  added.deploy_set = 1, added.deploy_valve_a = mech3::ai::valve_get("d_a"), added.deploy_valve_b = mech3::ai::valve_get("d_b");
  added.deploy_a = 2, added.deploy_b = 3, added.deploy_c = -5;
  added.hidden_set = 1, added.hidden = 1, added.visible_set = 1, added.visible = 0, added.shunt = 1;
  added.valve_union = new mech3::ai::ValveUnion;
  added.valve_union->valves = {{mech3::ai::valve_get("u1"), 2}, {mech3::ai::valve_get("u2"), 1}};
  added.auxname = strdup("Added");
  const std::vector<uint8_t> edited = mech3::ai::net_with_edits(
      net.data(), net.size(), tree, {{"node_00", &moved}, {"node_01", &fast}, {"node_99", &added}});
  const std::vector<uint8_t> out = mech3::ai::archive_with_entry(a, "net_01.zrd", edited);
  EXPECT(!out.empty());
  const fs::path saved = fs::path(MW3_TEST_TMP_ROOT) / "net_save_readerm1.zbd";
  { std::ofstream(saved, std::ios::binary).write(reinterpret_cast<const char*>(out.data()), out.size()); }
  const zutl::Mounts keep = zutl::mounts();
  zrdr::close_archives(true);
  zrdr::init();
  EXPECT(zrdr::open_archive(saved.string().c_str(), true));
  mech3::ai::NetFile f(1, "net_01.zrd");
  EXPECT(f.load() != 0);
  int checked = 0;
  for (const auto& [k, r] : was) {
    const NetNodeRec* now = f.node_named(k.c_str());
    const NetNodeRec& want = k == "node_00" ? moved : k == "node_01" ? fast : r;
    const std::string d = now ? differs(want, *now) : std::string("missing");
    if (!d.empty()) std::printf("  %s: %s\n", k.c_str(), d.c_str());
    checked += d.empty();
  }
  const NetNodeRec* n99 = f.node_named("node_99");
  const std::string d99 = n99 ? differs(added, *n99) : std::string("missing");
  if (!d99.empty()) std::printf("  node_99: %s\n", d99.c_str());
  EXPECT(checked == static_cast<int>(was.size()) && d99.empty());
  std::printf("  %d nodes read back as saved (2 edited), node_99 added; header version %d\n", checked, f.header.version);
  // The other entries as they were.
  const std::vector<Entry> before = entries(a), after = entries(out);
  EXPECT(before.size() == after.size());
  for (size_t i = 0; i < before.size() && i < after.size(); ++i)
    if (before[i].name != "net_01.zrd") EXPECT(before[i].name == after[i].name && before[i].data == after[i].data);
  f.unload();
  zrdr::close_archives(true);
  zutl::mounts() = keep;
  free_tree(tree);
  mech3::ai::valves_clear();
  std::error_code ec;
  fs::remove(saved, ec);
}

// The record cache (0x004dc2b0, NetFile::edit; S5 sitting 4) on c1's net_01:
// the file's own record is not kept, a changed one is (a copy, its auxname
// too), changed back it goes, a new key is kept, none marks a key gone (and
// the save drops it), and the file reads as edited while anything is kept.
TEST(net_record_cache_keeps_edits_until_they_are_the_files_again) {
  const fs::path src = fs::path(MW3_GAME_DIR) / "zbd" / "c1" / "readerm1.zbd";
  if (!fs::exists(src)) return;
  Hooks hooks;
  const zutl::Mounts keep = zutl::mounts();
  zrdr::close_archives(true);
  zrdr::init();
  EXPECT(zrdr::open_archive(src.string().c_str(), true));
  {
    mech3::ai::NetFile f(1, "net_01.zrd");
    EXPECT(f.load() != 0);
    const NetNodeRec* own = f.node_named("node_00");
    EXPECT(own != nullptr);
    if (own) {
      NetNodeRec r(*own);
      EXPECT(!f.edited());
      f.edit("node_00", &r);
      EXPECT(f.cache.empty() && !f.edited());
      r.pos[0] += 10.0f;
      std::free(r.auxname);
      r.auxname = strdup("Kept");  // the exe's copy (assigned, 0x004c63a0) would drop it
      f.edit("node_00", &r);
      EXPECT(f.cache.size() == 1 && f.cache["node_00"] && f.cache["node_00"]->pos[0] == r.pos[0] && f.edited());
      EXPECT(same_str(f.cache["node_00"]->auxname, "Kept"));
      const NetNodeRec* via = f.node_named("node_00");
      EXPECT(via && via->pos[0] == r.pos[0]);  // the reader gives the cached one
      r.pos[0] -= 10.0f;
      std::free(r.auxname);
      r.auxname = own->auxname ? strdup(own->auxname) : nullptr;
      f.edit("node_00", &r);
      EXPECT(f.cache.empty() && !f.edited());
      NetNodeRec added;
      added.kind = 1, added.index = 99;
      f.edit("node_99", &added);
      EXPECT(f.cache.size() == 1 && f.cache["node_99"] && f.cache["node_99"]->index == 99);
      f.edit("node_02", nullptr);
      EXPECT(f.cache.count("node_02") && !f.cache["node_02"] && f.node_named("node_02") == nullptr);
      // The save: node_02 dropped, node_99 added, the rest as it was.
      std::vector<uint8_t> a = file_bytes(src), net;
      for (const Entry& e : entries(a))
        if (e.name == "net_01.zrd") net = e.data;
      std::vector<std::pair<std::string, const NetNodeRec*>> edits(f.cache.begin(), f.cache.end());
      const std::vector<uint8_t> out = mech3::ai::net_with_edits(net.data(), net.size(), f.tree, edits);
      zrdr::Node* was = tree_of(net);
      zrdr::Node* now = tree_of(out);
      const std::map<std::string, NetNodeRec> before = records(was), after = records(now);
      EXPECT(!out.empty() && before.count("node_02") && !after.count("node_02") && after.count("node_99"));
      EXPECT(after.size() == before.size());  // one gone, one added
      std::printf("  %zu nodes before, %zu after (node_02 gone, node_99 added)\n", before.size(), after.size());
      free_tree(was), free_tree(now);
    }
    f.unload();
  }
  // Through the editing side: a node's set_pos folds it (0x004c71a0 ->
  // 0x004befc0), back to its place it goes; the save's fold (0x004bd100)
  // marks a node gone.
  {
    mech3::ai::net_files_clear();
    auto* reg = new mech3::ai::AiNet(1);  // the registry's net_01, as the dialog opens it
    {
      mech3::ai::CZAiNetEdit e(1, nullptr, 0);
      mech3::ai::NetFile* f = e.ref ? e.ref->file : nullptr;
      mech3::ai::NetNode* n5 = e.node(5);
      EXPECT(f && n5 && f->cache.empty() && e.editing);
      if (f && n5) {
        const float was[3] = {n5->pos()[0], n5->pos()[1], n5->pos()[2]};
        const float to[3] = {was[0] + 3.0f, was[1], was[2]};
        n5->set_pos(to);
        EXPECT(f->cache.count("node_05") == 1 && e.dirty == 1);
        n5->set_pos(was);
        EXPECT(f->cache.empty());
        delete e.elems[2];
        e.elems[2] = e.placeholder;
        e.fold_edits();
        EXPECT(f->cache.size() == 1 && f->cache.count("node_02") && !f->cache["node_02"] && e.dirty == 1);
      }
    }
    delete reg;
    mech3::ai::net_files_clear();
  }
  zrdr::close_archives(true);
  zutl::mounts() = keep;
  mech3::ai::valves_clear();
}

// Mods by campaign, step 4b (upgrades/editor/save_as.h): the editor only
// saves as. A copy of a campaign is a mod folder with the editor's marker
// and a mod.txt naming what it was saved from; its entry starts with that
// campaign's ticks. A name whose folder exists is refused and never written
// over, and the next suggestion counts on. A save's target without the
// marker is refused whatever it is: an imported campaign mod, an imported
// asset mod, a folder made by hand. Inside the game's own campaign the first
// save asks the name once (cancelled: nothing saved, nothing made), goes into
// the copy, and later saves follow it; the install and the campaign's own
// folders are as they were. A copy edited is saved into itself.
TEST(editor_saves_only_as_a_copy_of_the_campaign) {
  const fs::path root = fs::path(MW3_TEST_TMP_ROOT) / "save_as";
  std::error_code ec;
  fs::remove_all(root, ec);
  const fs::path inst = root / "install", wd = root / "write";
  const zutl::Mounts saved = zutl::mounts();
  const auto archive = [](const fs::path& p, const std::string& net) {
    fs::create_directories(p.parent_path());
    zutl_zar::Archive a;
    zutl_zar::init(a);
    zutl_zar::create(a, p.string().c_str());
    zutl_zar::add(a, "other.zrd", "x", 1);
    zutl_zar::add(a, "net_01.zrd", net.data(), static_cast<uint32_t>(net.size()));
    zutl_zar::close(a);
    zutl_zar::destroy(a);
  };
  const auto entry = [&](const fs::path& p) {
    for (const Entry& e : entries(file_bytes(p)))
      if (e.name == "net_01.zrd") return std::string(e.data.begin(), e.data.end());
    return std::string("(none)");
  };
  auto snapshot = [&](const fs::path& dir) {
    std::string t;
    std::vector<fs::path> all;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) all.push_back(it->path());
    std::sort(all.begin(), all.end());
    for (const fs::path& f : all)
      if (fs::is_regular_file(f, ec)) t += f.string() + ":" + text_of(f) + ":" + std::to_string(static_cast<long long>(fs::last_write_time(f, ec).time_since_epoch().count())) + "\n";
    return t;
  };
  const char* rel = "zbd\\c1\\readerm1.zbd";
  archive(inst / "zbd" / "c1" / "readerm1.zbd", "install");
  put(wd / "mods" / "camp" / "mod.txt", "name=Camp\ncampaign=night.txt\n");  // imported: a campaign
  put(wd / "mods" / "camp" / "night.txt", "name=Long Night\nmission=c1 1\n");
  put(wd / "mods" / "camp" / "mw3-mod-added.txt", "added by the game\n");
  archive(wd / "mods" / "camp" / "zbd" / "c1" / "readerm1.zbd", "camp's");
  put(wd / "mods" / "weps" / "mod.txt", "name=Weps\n");                         // imported: an asset mod
  put(wd / "mods" / "weps" / "mw3-mod-added.txt", "added by the game\n");
  put(wd / "mods" / "byhand" / "mod.txt", "name=By Hand\n");                    // put there by hand: no marker at all
  const auto mount = [&](const std::vector<std::string>& mods) {
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd.string());
    for (const std::string& m : mods) zutl::mount_mod((wd / "mods" / m).string());
    zutl::mount(inst.string());
  };
  const std::vector<uint8_t> one = {'o', 'n', 'e'}, two = {'t', 'w', 'o'};
  const std::string w = wd.string();
  // The hard rule: an unmarked folder is never a save's target.
  mount({"camp"});
  for (const char* target : {"camp", "weps", "byhand"}) {
    const std::string before = snapshot(wd / "mods");
    mech3::ai::net_save_into_mod() = target;
    const mech3::ai::NetSave r = mech3::ai::save_archive_edit(rel, "net_01.zrd", one);
    EXPECT(!r.ok && r.error.find("was not made by the editor, so it is never written") != std::string::npos);
    EXPECT(snapshot(wd / "mods") == before && !mech3::editor_owns_mod(w, target));
  }
  mech3::ai::net_save_into_mod().clear();
  // The copy's making and its refusals.
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(w);
  const mech3::CampaignEntry* own = mech3::find_entry(all, "mw3");
  EXPECT(own && mech3::entry_save_ticks(w, *own, {"weps"}));
  EXPECT(mech3::save_as_default_name(w, "mw3") == "MechWarrior 3, my edit" && mech3::save_as_default_name(w, "camp~night") == "Long Night, my edit");
  std::string folder, why;
  const std::string hand_before = snapshot(wd / "mods" / "byhand");
  EXPECT(!mech3::save_as_make_copy(w, "mw3", "byhand", &folder, &why) && why.find("there is a mod folder byhand already") == 0);
  EXPECT(snapshot(wd / "mods" / "byhand") == hand_before);  // never written over
  EXPECT(!mech3::save_as_make_copy(w, "mw3", "  ", &folder, &why) && why == "it needs a name");
  EXPECT(!mech3::save_as_make_copy(w, "nowhere", "X", &folder, &why) && why.find("not among the campaigns") != std::string::npos);
  // Inside the game's own campaign: the first save asks once.
  mount({"weps"});
  mech3::save_as_begin(w, "mw3");
  int asked = 0, ran = 0;
  mech3::save_as_ask_hook() = [&](std::string*) { return ++asked, false; };  // Cancel
  const std::string mods_before = snapshot(wd / "mods");
  EXPECT(!mech3::editor_save_ready([&] { ++ran; }) && asked == 1 && ran == 0 && snapshot(wd / "mods") == mods_before);
  mech3::save_as_ask_hook() = [&](std::string* name) {
    ++asked;
    EXPECT(*name == "MechWarrior 3, my edit");  // the default offered
    return true;
  };
  EXPECT(mech3::editor_save_ready([&] { ++ran; }) && asked == 2);
  const std::string copy = mech3::save_as_session().copy;
  EXPECT(copy == "MechWarrior 3, my edit" && mech3::save_as_session().made && mech3::editor_owns_mod(w, copy));
  EXPECT(text_of(wd / "mods" / copy / "mod.txt") == "name=MechWarrior 3, my edit\nkind=campaign\nbase=mw3\nfor=mw3 1.2\n");
  EXPECT((mech3::entry_ticks(w, copy) == std::vector<std::string>{"weps"}));  // the base's ticks
  mech3::ai::NetSave r = mech3::ai::save_archive_edit(rel, "net_01.zrd", one);
  EXPECT(r.ok && r.folder == copy && entry(wd / "mods" / copy / "zbd" / "c1" / "readerm1.zbd") == "one");
  EXPECT(entry(inst / "zbd" / "c1" / "readerm1.zbd") == "install" && !fs::exists(wd / "mods" / "editor edits"));
  // A later save: no question, the same copy.
  EXPECT(mech3::editor_save_ready([&] { ++ran; }) && asked == 2);
  r = mech3::ai::save_archive_edit(rel, "net_01.zrd", two);
  EXPECT(r.ok && r.folder == copy && entry(wd / "mods" / copy / "zbd" / "c1" / "readerm1.zbd") == "two");
  mech3::save_as_end();
  // It is a campaign of its own now, built on the game's; the next default name counts on.
  const std::vector<mech3::CampaignEntry> now = mech3::campaign_entries(w);
  const mech3::CampaignEntry* made = mech3::find_entry(now, copy);
  EXPECT(made && made->kind == mech3::CampaignEntry::kCopy && made->base == "mw3" && mech3::entry_plays_own_campaign(w, *made));
  EXPECT(mech3::save_as_default_name(w, "mw3") == "MechWarrior 3, my edit 2");
  // An imported campaign edited: its own folder is not touched, the copy holds the file changed.
  mount({"camp"});
  const std::string camp_before = snapshot(wd / "mods" / "camp");
  mech3::save_as_begin(w, "camp~night");
  EXPECT(mech3::save_as_session().copy.empty());
  mech3::save_as_ask_hook() = [&](std::string* name) { return *name = "Night, mine", true; };
  EXPECT(mech3::editor_save_ready(nullptr));
  r = mech3::ai::save_archive_edit(rel, "net_01.zrd", one);
  EXPECT(r.ok && r.folder == "Night, mine" && entry(wd / "mods" / "Night, mine" / "zbd" / "c1" / "readerm1.zbd") == "one");
  EXPECT(snapshot(wd / "mods" / "camp") == camp_before);
  EXPECT(text_of(wd / "mods" / "Night, mine" / "mod.txt").find("base=camp~night\n") != std::string::npos);
  mech3::save_as_end();
  // A copy edited: saved into itself, nothing asked.
  mount({"Night, mine", "camp"});
  asked = 0;
  mech3::save_as_ask_hook() = [&](std::string*) { return ++asked, true; };
  mech3::save_as_begin(w, "Night, mine");
  EXPECT(mech3::save_as_session().copy == "Night, mine" && mech3::editor_save_ready(nullptr) && asked == 0);
  r = mech3::ai::save_archive_edit(rel, "net_01.zrd", two);
  EXPECT(r.ok && r.folder == "Night, mine" && entry(wd / "mods" / "Night, mine" / "zbd" / "c1" / "readerm1.zbd") == "two");
  EXPECT(!fs::exists(wd / "mods" / "Night, mine, my edit"));
  mech3::save_as_end();
  mech3::save_as_ask_hook() = nullptr;
  // The box itself (no hook): shown at the first save with the default name; a name whose folder exists keeps
  // the box with the reason and a new suggestion; Cancel closes it and nothing is made; Save makes the copy and
  // runs the command again.
  mount({"weps"});
  mech3::save_as_begin(w, "mw3");
  ran = 0;
  EXPECT(!mech3::editor_save_ready([&] { ++ran; }) && !mech3::editor_forms().empty());
  mech3::EditorForm* box = mech3::editor_forms().empty() ? nullptr : mech3::editor_forms().back();
  EXPECT(box && box->title == "Save as a new campaign" && box->modal && box->field("Name") && *box->field("Name")->text == "MechWarrior 3, my edit 2");
  if (box) {
    *box->field("Name")->text = "byhand";
    EXPECT(box->press("Save") && mech3::form_shown(box) && ran == 0 && mech3::save_as_session().copy.empty());
    EXPECT(box->fields[0].label.find("Not saved: there is a mod folder byhand already") == 0 && *box->field("Name")->text == "MechWarrior 3, my edit 2");
    EXPECT(box->press("Cancel") && !mech3::form_shown(box) && ran == 0 && !fs::exists(wd / "mods" / "MechWarrior 3, my edit 2"));
    EXPECT(!mech3::editor_save_ready([&] { ++ran; }));
    box = mech3::editor_forms().empty() ? nullptr : mech3::editor_forms().back();
    EXPECT(box && box->press("Save") && !mech3::form_shown(box) && ran == 1 && mech3::save_as_session().copy == "MechWarrior 3, my edit 2");
  }
  mech3::save_as_end();
  mech3::ai::net_save_into_mod().clear();
  mech3::current_entry() = mech3::official_entries().front().id;
  zutl::mounts() = saved;
}

// A copy exported and imported elsewhere is an imported mod there (EXPORT
// leaves the editor's marker out): the editor never writes it, an edit makes
// that player's own copy of it, built on the imported one.
TEST(editor_never_writes_an_imported_copy) {
  const fs::path root = fs::path(MW3_TEST_TMP_ROOT) / "save_as_export";
  std::error_code ec;
  fs::remove_all(root, ec);
  const fs::path inst = root / "install", wd = root / "write", wd2 = root / "other write";
  const zutl::Mounts saved = zutl::mounts();
  const auto archive = [](const fs::path& p, const std::string& net) {
    fs::create_directories(p.parent_path());
    zutl_zar::Archive a;
    zutl_zar::init(a);
    zutl_zar::create(a, p.string().c_str());
    zutl_zar::add(a, "net_01.zrd", net.data(), static_cast<uint32_t>(net.size()));
    zutl_zar::close(a);
    zutl_zar::destroy(a);
  };
  auto tree = [&](const fs::path& dir) {
    std::string t;
    std::vector<fs::path> all;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) all.push_back(it->path());
    std::sort(all.begin(), all.end());
    for (const fs::path& f : all)
      if (fs::is_regular_file(f, ec)) t += f.generic_string() + ":" + text_of(f) + "\n";
    return t;
  };
  archive(inst / "zbd" / "c1" / "readerm1.zbd", "install");
  put(wd / "mods" / "Mine" / "mod.txt", "name=Mine\nkind=campaign\nbase=mw3\n");
  put(wd / "mods" / "Mine" / mech3::ai::kEditorMarker, "the editor's\n");
  put(wd / "mods" / "Mine" / "zbd" / "notes.bin", "mine");  // (IMPORT checks a reader archive's entries: a plain file here)
  const std::string zip = (root / "Mine.zip").string();
  EXPECT(mech3::mod_export((wd / "mods" / "Mine").string(), zip).ok);
  fs::create_directories(wd2, ec);
  const mech3::ModAddDone d = mech3::mod_add_run(wd2.string(), zip, inst.string(), nullptr, nullptr);
  std::printf("  import: %s\n", d.message.c_str());
  EXPECT(d.ok && d.folder == "Mine");
  const fs::path theirs = wd2 / "mods" / "Mine";
  EXPECT(!fs::exists(theirs / mech3::ai::kEditorMarker) && fs::is_regular_file(theirs / "mw3-mod-added.txt") && !mech3::editor_owns_mod(wd2.string(), "Mine"));
  // Edited there: asked for a name, a new copy built on the imported one; the imported folder as it was.
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd2.string());
  zutl::mount_mod(theirs.string());
  zutl::mount(inst.string());
  const std::string before = tree(theirs);
  mech3::save_as_begin(wd2.string(), "Mine");
  EXPECT(mech3::save_as_session().copy.empty());
  int asked = 0;
  mech3::save_as_ask_hook() = [&](std::string*) { return ++asked, true; };
  EXPECT(mech3::editor_save_ready(nullptr) && asked == 1 && mech3::save_as_session().copy == "Mine, my edit");
  const mech3::ai::NetSave r = mech3::ai::save_archive_edit("zbd\\c1\\readerm1.zbd", "net_01.zrd", {'n', 'e', 'w'});
  EXPECT(r.ok && r.folder == "Mine, my edit" && tree(theirs) == before);
  EXPECT(text_of(wd2 / "mods" / "Mine, my edit" / "mod.txt").find("base=Mine\n") != std::string::npos);
  mech3::save_as_end();
  mech3::save_as_ask_hook() = nullptr;
  mech3::ai::net_save_into_mod().clear();
  mech3::current_entry() = mech3::official_entries().front().id;
  zutl::mounts() = saved;
}
