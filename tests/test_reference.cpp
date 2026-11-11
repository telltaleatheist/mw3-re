// The modding reference (docs/modding-reference.md; upgrades/reference): the
// document is what `mw3 --reference` writes now, and each file kind's
// registry holds exactly the keys its reader looks up, recorded through the
// readers' observer while they read the shipped files.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <string>

#include "check.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech3_interp.h"
#include "mech3/mech_names.h"
#include "mech3/mod_campaign.h"
#include "mech3/mods.h"
#include "upgrades/reference/reference.h"

namespace mech3 {
void make_weapon_type(zrdr::Node* node, zwep::WeaponDef* def);  // 0x004b0020 (weapon.cpp)
}

namespace {
namespace ref = upgrades::reference;
namespace fs = std::filesystem;

std::set<std::string>& recorded() {
  static std::set<std::string> s;
  return s;
}
// Records every key looked up under `context` while `run` runs; the observer
// and context put back after.
std::set<std::string> record(const char* context, const std::function<void()>& run) {
  recorded().clear();
  ref::observer() = [](const char*, const char* key, const void*) { recorded().insert(key); };
  const char* was = ref::context();
  ref::context() = context;
  run();
  ref::context() = was;
  ref::observer() = nullptr;
  return recorded();
}
std::set<std::string> registry(const char* id) {
  std::set<std::string> s;
  for (const ref::File& f : ref::files())
    if (std::string(f.id) == id)
      for (const ref::Key& k : f.keys) s.insert(k.key);
  return s;
}
std::string listed(const std::set<std::string>& s) {
  std::string out;
  for (const std::string& k : s) out += (out.empty() ? "" : ", ") + k;
  return out.empty() ? "none" : out;
}
// Read and not listed, listed and not read: both empty when they agree.
bool agree(const char* id, const std::set<std::string>& read) {
  const std::set<std::string> reg = registry(id);
  std::set<std::string> missing, dead;
  for (const std::string& k : read)
    if (!reg.count(k)) missing.insert(k);
  for (const std::string& k : reg)
    if (!read.count(k)) dead.insert(k);
  std::printf("  %s: %zu keys read; not in the registry: %s; in it but never read: %s\n", id, read.size(), listed(missing).c_str(),
              listed(dead).c_str());
  return missing.empty() && dead.empty();
}
}  // namespace

// The document in the repo is the generator's output now; when it is not,
// `build/mw3 --reference docs/modding-reference.md` brings it up to date.
TEST(reference_doc_is_current) {
  const fs::path doc = fs::path(MW3_SOURCE_DIR) / "docs" / "modding-reference.md";
  std::ifstream f(doc, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  const bool same = ss.str() == ref::markdown();
  if (!same) std::printf("  stale: run build/mw3 --reference docs/modding-reference.md\n");
  EXPECT(same);
  // Every key once in its file, every cell filled.
  for (const ref::File& f2 : ref::files()) {
    std::set<std::string> seen;
    for (const ref::Key& k : f2.keys) {
      const std::string at = std::string(k.path) + "|" + k.key;
      EXPECT(seen.insert(at).second && *k.type && *k.fallback && *k.meaning);
    }
  }
}

// Our parsers: an unknown key makes each compare every key it knows.
TEST(reference_mod_txt_and_campaign_keys_are_the_parsers) {
  const std::set<std::string> mod = record("mod.txt", [] {
    mech3::ModInfo info;
    mech3::mod_parse("not_a_key=1\n", &info);
    mech3::ModMech m;
    mech3::ModWeapon w;
    std::string why;
    mech3::mech_row_parse("x not_a_key=1", &m, &why);
    mech3::weapon_row_parse("x not_a_key=1", &w, &why);
  });
  EXPECT(agree("mod.txt", mod));
  const std::set<std::string> camp = record("campaign", [] {
    mech3::ModCampaign c;
    mech3::campaign_parse("not_a_key=1\n", &c);
  });
  EXPECT(agree("campaign", camp));
}

// weapons.zrd as the game reads it (zWeapon's reader and MW3's weapon type),
// over the shipped file: every key it looks up. The weapons' own names
// (BALLISTICS' records) and the soils' are names, not keys.
TEST(reference_weapons_zrd_keys_are_the_readers) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(fs::path(install) / "zbd" / "reader.zbd", ec)) return;
  zutl::Mounts keep = zutl::mounts();
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(install);
  zrdr::init();
  EXPECT(zrdr::open_archive("zbd\\reader.zbd", true));
  std::set<std::string> names;
  const bool hardware = zwep::init_hooks().hardware;
  zwep::init_hooks().hardware = true;  // DAMAGE_ANIM_ON_HEALTH is read on hardware rendering only (the port's)
  std::set<std::string> read = record("weapons.zrd", [&] {
    EXPECT(zwep::init(nullptr, "weapons.zrd", 0, mech3::make_weapon_type) == 0);
    for (const zwep::WeaponDef& d : zwep::state().defs)
      if (d.key) names.insert(d.key);
  });
  zwep::shutdown(nullptr);
  zwep::init_hooks().hardware = hardware;
  zrdr::close_archives(true);
  zutl::mounts() = keep;
  for (const std::string& s : zmod::soil_names()) names.insert(s);
  for (const std::string& n : names) read.erase(n);
  EXPECT(names.size() > 30);
  EXPECT(agree("weapons.zrd", read));
}

// The observers the whole-run check relies on: a reader file's load reports
// its root, every lookup its node, and freeing a tree each of its nodes (a
// freed node's memory may then hold another file's, which the check's map
// must not credit to the first).
TEST(reference_observers_see_load_lookup_and_free) {
  struct Seen {
    std::set<const void*> freed;
    int lookups = 0;
  };
  static Seen seen;
  seen = Seen{};
  std::string why;
  zrdr::Node* root = zrdr::parse_json("[\"KEY\", [\"INNER\", 1], \"OTHER\", \"x\"]", &why);
  EXPECT(root != nullptr);
  if (!root) return;
  std::set<const void*> nodes;
  std::function<void(const zrdr::Node*)> walk = [&](const zrdr::Node* n) {
    nodes.insert(n);
    if (n->type == zrdr::List)
      for (int32_t i = 1; i < n->list[0].i; ++i) walk(&n->list[i]);
  };
  walk(root);
  ref::observer() = [](const char*, const char*, const void*) { ++seen.lookups; };
  ref::free_observer() = [](const void* node) { seen.freed.insert(node); };
  EXPECT(zrdr::find(root, "INNER") != nullptr && seen.lookups == 1);
  zrdr::free_node(root);
  ref::observer() = nullptr, ref::free_observer() = nullptr;
  std::printf("  %zu nodes, %zu reported freed, %d lookups\n", nodes.size(), seen.freed.size(), seen.lookups);
  // Every node of the tree (free_contents visits each list's count slot too).
  EXPECT(std::includes(seen.freed.begin(), seen.freed.end(), nodes.begin(), nodes.end()));
}

// The interp script commands (interp.zbd's scripts; docs/interp.md): every
// name the preprocessor, the dispatcher and MW3's pre hook test (an unknown
// word for each first letter makes each letter test all of its own), and
// each dispatcher command's arguments in the order it reads them, run once
// with numbers for arguments and no engine (every engine call null).
namespace {
std::vector<std::string>& interp_names() {
  static std::vector<std::string> v;
  return v;
}
std::string& interp_args() {
  static std::string s;
  return s;
}
bool& interp_typed() {
  static bool t = false;
  return t;
}
void interp_line(zint::Interp& in, const std::string& text) {
  std::vector<char> buf(text.begin(), text.end());
  buf.push_back('\n');
  buf.resize(buf.size() + 0x400, 0);
  zint::exec_line(in, buf.data());
}
// The names, then each command's arguments: name -> "sfi..." ("-" none).
std::map<std::string, std::string> interp_record() {
  const zint::Engine keep = zint::engine();
  zint::engine() = zint::Engine{};  // no engine: every call skipped
  const int32_t hardware = zrndr::device_fog().active;
  zrndr::device_fog().active = 1;  // ModelDoShadows: hardware only
  zint::Interp in;
  mech3::interp_create(in);
  // With no engine nothing sets the interp's model pointers: cleared, or the Object3D commands write through
  // whatever an earlier test's mission left there (freed: the whole run's crash here, 2026-11-10).
  zint::globals().object_model = zint::globals().cycle_model = zint::globals().model_out = nullptr;
  interp_names().clear();
  ref::observer() = [](const char*, const char* key, const void* node) {
    if (!node) interp_names().push_back(key);
  };
  std::set<std::string> names;
  for (char c = 'A'; c <= 'z'; ++c) {
    if (!std::isalpha(static_cast<unsigned char>(c))) continue;
    interp_line(in, std::string(1, c) + "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
  }
  interp_line(in, "SEQzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
  for (const std::string& n : interp_names()) names.insert(n);
  ref::observer() = nullptr;
  std::map<std::string, std::string> out;
  static const std::set<std::string> kPre = {"endif", "ifdef", "ifndef", "mkdir", "Quit", "set", "source", "who", "var"};
  ref::arg_observer() = [](char k) {
    if (k == 's' && interp_typed()) {  // the string a typed read fetched
      interp_typed() = false;
      return;
    }
    interp_typed() = k != 's';
    interp_args() += k;
  };
  for (const std::string& n : names) {
    if (kPre.count(n)) {
      out[n] = "";
      continue;
    }
    // A current node (zeroed stand-in: the engine is null, so nothing
    // reaches into it but the commands' own null checks), and the command
    // run with 0 to 12 arguments (some take an exact count): the longest
    // read is its arguments.
    alignas(16) static uint8_t node[0x4000];
    std::string best;
    // check_args wants the node's class (+0x34): the others tried only when
    // class 0 reads nothing, never for the commands that walk the node
    // itself (dumps, prints, counts: none takes an argument).
    const bool walks = n.rfind("Print", 0) == 0 || n.rfind("Count", 0) == 0 || n == "NodeDump";
    for (int32_t cls : {0, 1, 2, 3, 4, 5, 6})
      for (int count = 0; count <= 12 && (cls == 0 || (best.empty() && !walks)); ++count) {  // some take an exact count only
        std::string line = n;
        for (int k = 1; k <= count; ++k) line += " " + std::to_string(k);
        interp_args().clear(), interp_typed() = false;
        std::memset(node, 0, sizeof node);
        std::memcpy(node + 0x34, &cls, 4);
        in.current = reinterpret_cast<zcls::Node*>(node);
        interp_line(in, line);
        if (interp_args().size() > best.size()) best = interp_args();
      }
    in.current = nullptr;
    out[n] = best.empty() ? "-" : best;
  }
  ref::arg_observer() = nullptr;
  zint::engine() = keep;
  zrndr::device_fog().active = hardware;
  return out;
}
}  // namespace

TEST(reference_interp_commands_are_the_dispatchers) {
  const std::map<std::string, std::string> got = interp_record();
  std::map<std::string, std::string> reg;
  std::set<std::string> pre;
  for (const ref::File& f : ref::files())
    if (std::string(f.id) == "interp")
      for (const ref::Key& k : f.keys) {
        reg[k.key] = k.type;
        if (std::string(k.path) == "preprocessor") pre.insert(k.key);
      }
  std::string missing, dead, typed;
  for (const auto& [n, a] : got) {
    const auto it = reg.find(n);
    if (it == reg.end()) missing += n + " ";
    else if (!pre.count(n) && it->second != a) typed += n + " (reads " + a + ", listed " + it->second + ") ";
  }
  for (const auto& [n, a] : reg)
    if (!got.count(n)) dead += n + " ";
  std::printf("  %zu names; not in the registry: %s; never tested: %s; arguments differ: %s\n", got.size(),
              missing.empty() ? "none" : missing.c_str(), dead.empty() ? "none" : dead.c_str(), typed.empty() ? "none" : typed.c_str());
  EXPECT(got.size() > 150 && missing.empty() && dead.empty() && typed.empty());
}
