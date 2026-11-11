// M3 (docs/modding.md): the readers a broken mod file could crash, bounded
// by their archive entry. The shipped files read whole; truncated and
// garbled copies are refused or read, never a crash.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "check.h"
#include "zip_write.h"
#include "gamez/zclass/cls_mechlib.h"
#include "gamez/zclass/cls_mechlib_json.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zsiscript/si_script.h"
#include "gamez/zsiscript/si_script_json.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/mech_anim.h"
#include "mech3/mods.h"
#include "gamez/zutil/zutl_path.h"

namespace {
// The animations' reader refuses a path of 0x80 characters or more (0x005090f0, as the exe does). A checkout that
// sits deep (a worktree under a long folder) puts the install's and the test's own files past that, and the
// reads below then fail for the path, not for what they test: such a path is named from the current directory.
std::string anim_path(const std::string& p) {
  if (p.size() <= 0x7f) return p;
  std::error_code ec;
  const std::string r = std::filesystem::path(p).lexically_relative(std::filesystem::current_path(ec)).string();
  return !ec && !r.empty() && r.size() < p.size() ? r : p;
}

namespace fs = std::filesystem;
struct Entry {
  std::string name;
  std::vector<uint8_t> data;
};
std::vector<Entry> read_all(const std::string& path) {
  std::vector<Entry> out;
  zutl_zar::Archive a;
  zutl_zar::init(a);
  if (!zutl_zar::open(a, path.c_str())) return out;
  for (int i = 0; i < a.count; ++i) {
    Entry e;
    e.name = a.entries[i].name;
    uint32_t size = a.entries[i].length;
    e.data.resize(size);
    if (zutl_zar::read(a, i, e.data.data(), &size) == 0) out.push_back(std::move(e));
  }
  zutl_zar::destroy(a);
  return out;
}
void write_all(const std::string& path, const std::vector<Entry>& entries) {
  zutl_zar::Archive a;
  zutl_zar::init(a);
  zutl_zar::create(a, path.c_str());
  for (const Entry& e : entries) zutl_zar::add(a, e.name.c_str(), e.data.data(), static_cast<uint32_t>(e.data.size()));
  zutl_zar::close(a);
  zutl_zar::destroy(a);
}
bool is_tree(const std::string& n) { return n != "version" && n != "format" && n != "materials"; }
// A copy: cut short, bytes flipped, or words made huge.
void mutate(std::vector<uint8_t>& d, std::mt19937& r, int kind) {
  if (d.empty()) return;
  if (kind == 0) {
    d.resize(r() % d.size());
  } else if (kind == 1) {
    for (int n = 1 + r() % 16; n > 0; --n) d[r() % d.size()] ^= static_cast<uint8_t>(1 + r() % 255);
  } else {
    for (int n = 1 + r() % 4; n > 0 && d.size() >= 4; --n) {
      const size_t at = (r() % (d.size() / 4)) * 4;
      const uint32_t v = r() % 2 ? 0xffffffffu - r() % 16 : 0x10000u + r() % 0x7fffffffu;
      std::memcpy(&d[at], &v, 4);
    }
  }
}
}  // namespace

TEST(bounded_mechlib_reads_the_shipped_trees_whole) {
  const std::string path = std::string(MW3_GAME_DIR) + "/zbd/mechlib.zbd";
  std::error_code ec;
  if (!fs::exists(path, ec)) return;
  zmod::models() = zmod::ModelPool{};
  zmod::reset_pool();
  zcls::MechLib lib;
  zutl_zar::init(lib.archive);
  EXPECT(zcls::mechlib_open(lib, path.c_str()));
  EXPECT(lib.materials.size() == 442);
  int trees = 0, loaded = 0;
  for (const Entry& e : read_all(path)) {
    if (!is_tree(e.name)) continue;
    ++trees;
    loaded += zcls::mechlib_load(lib, e.name.c_str()) != nullptr;
  }
  std::printf("  mechlib: %d of %d trees read whole\n", loaded, trees);
  EXPECT(trees == 57 && loaded == 57);
  zcls::mechlib_close(lib);
}

TEST(bounded_mechlib_fuzz_never_crashes) {
  const std::string path = std::string(MW3_GAME_DIR) + "/zbd/mechlib.zbd";
  std::error_code ec;
  if (!fs::exists(path, ec)) return;
  const std::vector<Entry> all = read_all(path);
  std::vector<size_t> trees;
  for (size_t i = 0; i < all.size(); ++i)
    if (is_tree(all[i].name)) trees.push_back(i);
  const std::string dir = std::string(MW3_TEST_TMP) + "/bounded_mechlib";
  fs::create_directories(dir, ec);
  const std::string copy = dir + "/mechlib.zbd";
  std::mt19937 r(1234);
  int runs = 0, refused = 0, read = 0, cut = 0, cut_refused = 0, materials_refused = 0;
  for (int seed = 0; seed < 240; ++seed) {
    std::vector<Entry> m = all;
    const int kind = seed % 3;
    const bool materials = seed % 8 == 7;
    const std::string want = materials ? std::string("materials") : all[trees[r() % trees.size()]].name;
    Entry* target = nullptr;
    for (Entry& e : m)
      if (e.name == want) target = &e;
    if (!target) continue;
    const size_t before = target->data.size();
    mutate(target->data, r, kind);
    write_all(copy, m);
    zmod::models() = zmod::ModelPool{};
    zmod::reset_pool();
    zcls::MechLib lib;
    zutl_zar::init(lib.archive);
    const bool open = zcls::mechlib_open(lib, copy.c_str());
    if (materials) materials_refused += !open || lib.materials.empty();
    zcls::Node* t = open && !materials ? zcls::mechlib_load(lib, target->name.c_str()) : nullptr;
    if (!materials) {
      ++runs;
      t ? ++read : ++refused;
      if (kind == 0 && target->data.size() < before) ++cut, cut_refused += t == nullptr;
    }
    zcls::mechlib_close(lib);
  }
  std::printf("  mechlib fuzz: %d trees, %d refused, %d read; cut short %d, refused %d; materials refused %d\n", runs,
              refused, read, cut, cut_refused, materials_refused);
  EXPECT(runs > 150 && refused > 0 && cut_refused == cut);
}

namespace {
// Every motion in the archive at `path` loaded: how many read, how many refused.
void load_motions(const std::string& path, const std::vector<std::string>& names, int* read, int* refused) {
  *read = *refused = 0;
  if (!mech3::open_motions(path.c_str())) return;
  for (const std::string& n : names) {
    zsis::Script s;
    if (mech3::load_motion(s, n.c_str())) ++*read;
    else ++*refused;
    mech3::script_free_parts(s);
  }
  mech3::close_motions();
}
}  // namespace

TEST(bounded_motion_reads_the_shipped_motions_whole) {
  const std::string path = std::string(MW3_GAME_DIR) + "/zbd/motion.zbd";
  std::error_code ec;
  if (!fs::exists(path, ec)) return;
  std::vector<std::string> names;
  for (const Entry& e : read_all(path)) names.push_back(e.name);
  int read = 0, refused = 0;
  load_motions(path, names, &read, &refused);
  std::printf("  motion: %d of %zu read whole\n", read, names.size());
  EXPECT(names.size() == 258 && read == 258 && refused == 0);
}

TEST(bounded_motion_fuzz_never_crashes) {
  const std::string path = std::string(MW3_GAME_DIR) + "/zbd/motion.zbd";
  std::error_code ec;
  if (!fs::exists(path, ec)) return;
  const std::vector<Entry> all = read_all(path);
  const std::string dir = std::string(MW3_TEST_TMP) + "/bounded_motion";
  fs::create_directories(dir, ec);
  const std::string copy = dir + "/motion.zbd";
  std::mt19937 r(4321);
  int runs = 0, refused = 0, cut = 0, cut_refused = 0;
  for (int seed = 0; seed < 300; ++seed) {
    std::vector<Entry> m = all;
    Entry& e = m[r() % m.size()];
    const size_t before = e.data.size();
    const int kind = seed % 3;
    mutate(e.data, r, kind);
    write_all(copy, m);
    int ok = 0, bad = 0;
    load_motions(copy, {e.name}, &ok, &bad);
    ++runs, refused += bad;
    if (kind == 0 && e.data.size() < before) ++cut, cut_refused += bad;
  }
  std::printf("  motion fuzz: %d motions, %d refused; cut short %d, refused %d\n", runs, refused, cut, cut_refused);
  EXPECT(runs == 300 && refused > 0 && cut_refused == cut);
}

// anim.zbd (a whole file, not an archive): copies of c1's cut short, bytes
// flipped or words made huge, read with no world. Each read refuses the
// file (no defs, its counts 0) or reads every def and fails at the first
// lookup, the original's way; none crash or allocate what a count says.
TEST(bounded_anim_fuzz_never_crashes) {
  const std::string src = std::string(MW3_GAME_DIR) + "/zbd/c1/anim.zbd";
  std::error_code ec;
  if (!fs::exists(src, ec)) return;
  std::vector<uint8_t> all(fs::file_size(src, ec));
  if (std::FILE* f = std::fopen(src.c_str(), "rb")) {
    const size_t n = std::fread(all.data(), 1, all.size(), f);
    all.resize(n);
    std::fclose(f);
  }
  const std::string dir = std::string(MW3_TEST_TMP) + "/bounded_anim";
  fs::create_directories(dir, ec);
  const std::string copy = dir + "/anim.zbd";
  std::mt19937 r(777);
  int runs = 0, refused = 0, cut = 0, cut_refused = 0;
  for (int seed = 0; seed < 150; ++seed) {
    std::vector<uint8_t> d = all;
    const int kind = seed % 3;
    // Past the 12-byte header and the source list, so most changes land in the defs.
    std::vector<uint8_t> head(d.begin(), d.begin() + 12), body(d.begin() + 12, d.end());
    mutate(body, r, kind);
    d = head;
    d.insert(d.end(), body.begin(), body.end());
    if (std::FILE* f = std::fopen(copy.c_str(), "wb")) {
      std::fwrite(d.data(), 1, d.size(), f);
      std::fclose(f);
    }
    zeff::anim() = zeff::AnimState{};
    zeff::anim_allow_loading(1);
    zeff::anim_set_zbd_path(anim_path(copy).c_str());
    zeff::anim_read_zbd();
    const bool was_refused = zeff::anim().defs == nullptr && zeff::anim().def_count() == 0;
    ++runs, refused += was_refused;
    if (kind == 0 && d.size() < all.size()) ++cut, cut_refused += was_refused;
    zeff::anim_free_all();
  }
  std::printf("  anim fuzz: %d reads, %d refused; cut short %d, refused %d\n", runs, refused, cut, cut_refused);
  EXPECT(runs == 150 && refused > 0 && cut_refused == cut);
  // A source count of 0xfffffff0 in the header: refused before anything is
  // allocated (unbounded, the list alone would be some 360 GB).
  std::vector<uint8_t> d = all;
  const uint32_t huge = 0xfffffff0u;
  std::memcpy(&d[8], &huge, 4);
  if (std::FILE* f = std::fopen(copy.c_str(), "wb")) {
    std::fwrite(d.data(), 1, d.size(), f);
    std::fclose(f);
  }
  zeff::anim() = zeff::AnimState{};
  zeff::anim_allow_loading(1);
  zeff::anim_set_zbd_path(anim_path(copy).c_str());
  EXPECT(zeff::anim_read_zbd() == -1 && zeff::anim().defs == nullptr);
  zeff::anim_free_all();
}

// A mod's motion.zbd with one motion cut short (docs/modding.md, M3): the
// motion comes from the next copy (the install's), read whole; with no
// other copy it is refused. Every other motion is the mod's own.
TEST(bounded_motion_falls_back_to_the_next_copy) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/motion.zbd", ec)) return;
  const zutl::Mounts saved = zutl::mounts();
  const std::string wd = std::string(MW3_TEST_TMP) + "/motion_fallback";
  fs::remove_all(wd, ec);
  fs::create_directories(wd + "/mods/brokenwalk/zbd", ec);
  std::ofstream(wd + "/mods/brokenwalk/mod.txt") << "name=Broken walk\n";
  std::vector<Entry> all = read_all(install + "/zbd/motion.zbd");
  size_t walk_frames = 0;
  for (Entry& e : all)
    if (e.name == "madcat_walk") e.data.resize(e.data.size() / 2);  // cut short: refused
  write_all(wd + "/mods/brokenwalk/zbd/motion.zbd", all);
  auto load = [&](bool with_install, const char* name, uint32_t* frames) {
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd);
    mech3::mods_mount(wd, {"brokenwalk"}, {install});
    if (with_install) zutl::mount(install);
    bool ok = false;
    if (mech3::open_motions("zbd\\motion.zbd")) {
      zsis::Script s;
      ok = mech3::load_motion(s, name);
      *frames = ok ? s.frame_count : 0;
      mech3::script_free_parts(s);
      mech3::close_motions();
    }
    return ok;
  };
  uint32_t frames = 0, whole = 0;
  {  // the install's own, for the frame count
    zutl::mounts() = zutl::Mounts{};
    zutl::mount(install);
    if (mech3::open_motions("zbd\\motion.zbd")) {
      zsis::Script s;
      if (mech3::load_motion(s, "madcat_walk")) whole = s.frame_count;
      mech3::script_free_parts(s);
      mech3::close_motions();
    }
  }
  (void)walk_frames;
  EXPECT(whole > 0);
  EXPECT(load(true, "madcat_walk", &frames) && frames == whole);   // the install's copy
  EXPECT(!mech3::motion_broken("madcat_walk"));
  EXPECT(!load(false, "madcat_walk", &frames));                    // no other copy: refused
  EXPECT(mech3::motion_broken("madcat_walk"));                     // broken, so the mech is refused
  EXPECT(load(false, "madcat_run", &frames) && frames > 0);        // the mod's own others
  // Missing from every copy (the shipped data has no elemental_limp): not
  // broken, so the Elemental keeps the exe's null limp and is not refused.
  EXPECT(!load(true, "elemental_limp", &frames) && !mech3::motion_broken("elemental_limp"));
  zutl::mounts() = saved;
}

namespace {
std::vector<uint8_t> slurp_file(const std::string& path) {
  std::vector<uint8_t> d;
  if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
    std::fseek(f, 0, SEEK_END);
    d.resize(static_cast<size_t>(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    d.resize(std::fread(d.data(), 1, d.size(), f));
    std::fclose(f);
  }
  return d;
}
void spit_file(const std::string& path, const std::vector<uint8_t>& d) {
  if (std::FILE* f = std::fopen(path.c_str(), "wb")) {
    std::fwrite(d.data(), 1, d.size(), f);
    std::fclose(f);
  }
}
uint32_t get32(const std::vector<uint8_t>& d, size_t at) {
  uint32_t v;
  std::memcpy(&v, &d[at], 4);
  return v;
}
void set32(std::vector<uint8_t>& d, size_t at, uint32_t v) { std::memcpy(&d[at], &v, 4); }
// A reader entry of lists `depth` deep around an int (each list holds one node).
std::vector<uint8_t> nested(int depth) {
  std::vector<uint8_t> d;
  auto u32 = [&](uint32_t v) { d.insert(d.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + 4); };
  for (int i = 0; i < depth; ++i) u32(4), u32(2);
  u32(1), u32(7);
  return d;
}
}  // namespace

// Modding item 4 (docs/modding.md): archives larger than the shipped ones, as
// a repack that adds to them makes (our own repacks here: no mech3ax and no
// real mods). mechlib.zbd with four times its trees, motion.zbd with four
// times its motions, reader.zbd with four times its entries, c1's anim.zbd
// with twice its sources and defs, c1's gamez.zbd with 2,500 more node slots
// (a second block): each read whole, nothing capped below what the file
// holds. Then one past each reader's limit, refused and named, no crash: a
// tree or motion one byte short, a reader list 65 deep (64 reads), an anim
// def count of 0x7fff (the format's int16) with too few bytes, a node array
// larger than its file.
TEST(bounded_readers_take_more_than_shipped) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/mechlib.zbd", ec)) return;
  const std::string dir = std::string(MW3_TEST_TMP) + "/bounded_more";
  fs::create_directories(dir, ec);
  // mechlib: the trees four times over (each copy under a new name).
  {
    std::vector<Entry> m = read_all(install + "/zbd/mechlib.zbd"), extra;
    for (int k = 1; k <= 3; ++k)
      for (const Entry& e : m)
        if (is_tree(e.name)) extra.push_back({"x" + std::to_string(k) + "_" + e.name, e.data});
    std::string cut_name = extra.front().name;
    m.insert(m.end(), extra.begin(), extra.end());
    m.push_back({"cut_" + cut_name, extra.front().data});
    m.back().data.pop_back();
    write_all(dir + "/mechlib.zbd", m);
    zmod::models() = zmod::ModelPool{};
    zmod::models().size = 8000;  // the reader's test: room for the 3,080 models (the default pool holds 1,750)
    zmod::reset_pool();
    zcls::MechLib lib;
    zutl_zar::init(lib.archive);
    EXPECT(zcls::mechlib_open(lib, (dir + "/mechlib.zbd").c_str()) && lib.materials.size() == 442);
    int trees = 0, loaded = 0;
    for (const Entry& e : m) {
      if (!is_tree(e.name) || e.name.rfind("cut_", 0) == 0) continue;
      ++trees;
      loaded += zcls::mechlib_load(lib, e.name.c_str()) != nullptr;
    }
    const bool cut_refused = zcls::mechlib_load(lib, ("cut_" + cut_name).c_str()) == nullptr;
    std::printf("  mechlib: %d of %d trees read whole; one a byte short refused %d\n", loaded, trees, cut_refused);
    EXPECT(trees == 57 * 4 && loaded == trees && cut_refused);
    zcls::mechlib_close(lib);
    zmod::models() = zmod::ModelPool{};
  }
  // motion.zbd: the motions four times over, and one a byte short.
  {
    std::vector<Entry> m = read_all(install + "/zbd/motion.zbd");
    const size_t shipped = m.size();
    std::vector<std::string> names;
    for (const Entry& e : m) names.push_back(e.name);
    for (int k = 1; k <= 3; ++k)
      for (size_t i = 0; i < shipped; ++i) m.push_back({"x" + std::to_string(k) + "_" + m[i].name, m[i].data}), names.push_back(m.back().name);
    m.push_back({"cut_motion", m[0].data});
    m.back().data.pop_back();
    write_all(dir + "/motion.zbd", m);
    int read = 0, refused = 0, cut_read = 0, cut_refused = 0;
    load_motions(dir + "/motion.zbd", names, &read, &refused);
    load_motions(dir + "/motion.zbd", {"cut_motion"}, &cut_read, &cut_refused);
    std::printf("  motion: %d of %zu read whole; one a byte short refused %d\n", read, names.size(), cut_refused);
    EXPECT(names.size() == 258 * 4 && read == 258 * 4 && refused == 0 && cut_refused == 1);
  }
  // reader.zbd: every entry four times over; lists 64 deep read, 65 refused.
  {
    std::vector<Entry> m = read_all(install + "/zbd/reader.zbd");
    const size_t shipped = m.size();
    for (int k = 1; k <= 3; ++k)
      for (size_t i = 0; i < shipped; ++i) m.push_back({"x" + std::to_string(k) + "_" + m[i].name, m[i].data});
    m.push_back({"deep64.zrd", nested(64)});  // the root's list and 63 inside it: 64 deep
    m.push_back({"deep65.zrd", nested(65)});
    write_all(dir + "/reader.zbd", m);
    zrdr::init();
    EXPECT(zrdr::open_archive((dir + "/reader.zbd").c_str(), true));
    int base = 0, copies = 0;
    for (size_t i = 0; i < m.size(); ++i) {
      if (m[i].name.rfind("deep", 0) == 0) continue;
      zrdr::Node* n = zrdr::load(m[i].name.c_str());
      (i < shipped ? base : copies) += n != nullptr;
      if (n) zrdr::free_node(n);
    }
    zrdr::Node* d64 = zrdr::load("deep64.zrd");
    zrdr::Node* d65 = zrdr::load("deep65.zrd");
    // 65 deep: the 64 the reader allows are kept and it stops there, saying so (the graceful read, zreader.cpp).
    int kept = 0;
    for (zrdr::Node* n = d65; n && n->type == zrdr::List; n = zrdr::list_count(n) > 1 ? &n->list[1] : nullptr) ++kept;
    const bool said = !zrdr::load_notes().empty() && zrdr::load_notes()[0].wrong.find("lists nested more than 64 deep") == 0;
    std::printf("  reader: %zu entries, %d read, copies %d of %d; 64 deep read %d, 65 deep read to %d lists and said %d\n", m.size(),
                base + copies, copies, base * 3, d64 != nullptr, kept, said);
    EXPECT(base > 100 && copies == base * 3 && d64 != nullptr && d65 != nullptr && kept == 64 && said);
    if (d64) zrdr::free_node(d64);
    if (d65) zrdr::free_node(d65);
    zrdr::close_archives(true);
  }
  // c1's anim.zbd: its sources and defs twice (each block repeated), the
  // messages after; read with no world it reads every def and stops at the
  // first lookup, as the shipped file does. A def count of 0x7fff refused.
  {
    const std::vector<uint8_t> a = slurp_file(install + "/zbd/c1/anim.zbd");
    const uint32_t sources = get32(a, 8);
    const size_t block_at = 12 + sources * 0x54, defs_at = block_at + 0x44;
    std::vector<uint8_t> block(a.begin() + static_cast<long>(block_at), a.begin() + static_cast<long>(defs_at));
    int16_t defs = 0;
    std::memcpy(&defs, &block[0x0a], 2);
    const size_t messages = get32(block, 0x10) * 0x24, defs_end = a.size() - messages;
    std::vector<uint8_t> d(a.begin(), a.begin() + 12);
    set32(d, 8, sources * 2);
    for (int k = 0; k < 2; ++k) d.insert(d.end(), a.begin() + 12, a.begin() + static_cast<long>(block_at));
    const int16_t twice = static_cast<int16_t>(defs * 2);
    std::memcpy(&block[0x0a], &twice, 2);
    d.insert(d.end(), block.begin(), block.end());
    for (int k = 0; k < 2; ++k) d.insert(d.end(), a.begin() + static_cast<long>(defs_at), a.begin() + static_cast<long>(defs_end));
    d.insert(d.end(), a.begin() + static_cast<long>(defs_end), a.end());
    spit_file(dir + "/anim.zbd", d);
    auto read = [&](const std::string& path) {
      zeff::anim() = zeff::AnimState{};
      zeff::anim_allow_loading(1);
      zeff::anim_set_zbd_path(anim_path(path).c_str());
      zeff::anim_read_zbd();
      const int n = zeff::anim().defs ? zeff::anim().def_count() : -1;
      zeff::anim_free_all();
      return n;
    };
    const int shipped = read(install + "/zbd/c1/anim.zbd"), doubled = read(dir + "/anim.zbd");
    std::vector<uint8_t> over = a;
    const int16_t most = 0x7fff;
    std::memcpy(&over[block_at + 0x0a], &most, 2);
    spit_file(dir + "/anim_over.zbd", over);
    const int too_many = read(dir + "/anim_over.zbd");
    std::printf("  anim: c1's %d defs, doubled %d (sources %u to %u); 0x7fff defs read as %d\n", shipped, doubled, sources,
                sources * 2, too_many);
    EXPECT(shipped == defs && doubled == defs * 2 && too_many == -1);
  }
  // c1's gamez.zbd: 2,500 more node slots, free (chained into the file's free
  // list), after the shipped ones and before the nodes' data: the world reads,
  // the same nodes live, the pool a block larger. A node array past the file refused.
  {
    const std::vector<uint8_t> g = slurp_file(install + "/zbd/c1/gamez.zbd");
    const int32_t size = static_cast<int32_t>(get32(g, 0x18)), last_free = static_cast<int32_t>(get32(g, 0x1c));
    const size_t nodes_at = get32(g, 0x20), slots_end = nodes_at + static_cast<size_t>(size) * 0xd4;
    constexpr int32_t kMore = 2500;
    std::vector<uint8_t> more(static_cast<size_t>(kMore) * 0xd4, 0);
    for (int32_t k = 0; k < kMore; ++k)
      set32(more, static_cast<size_t>(k) * 0xd4 + 0xd0,
            k + 1 < kMore ? static_cast<uint32_t>(size + k + 1) : static_cast<uint32_t>(last_free) & 0xffffffu);
    std::vector<uint8_t> d(g.begin(), g.begin() + static_cast<long>(slots_end));
    d.insert(d.end(), more.begin(), more.end());
    d.insert(d.end(), g.begin() + static_cast<long>(slots_end), g.end());
    set32(d, 0x18, static_cast<uint32_t>(size + kMore));
    set32(d, 0x1c, static_cast<uint32_t>(size));
    spit_file(dir + "/gamez.zbd", d);
    auto read = [&](const std::string& path, int32_t* live, int32_t* capacity) {
      zmod::models() = zmod::ModelPool{};
      zmod::reset_pool();
      const int r = zcls::read_zbd(path.c_str());
      *live = zcls::nodes().live, *capacity = zcls::nodes().capacity;
      return r;
    };
    int32_t live0 = 0, cap0 = 0, live1 = 0, cap1 = 0, live2 = 0, cap2 = 0;
    const int r0 = read(install + "/zbd/c1/gamez.zbd", &live0, &cap0);
    const int r1 = read(dir + "/gamez.zbd", &live1, &cap1);
    std::vector<uint8_t> over = g;
    set32(over, 0x18, static_cast<uint32_t>(size + 1000000));
    spit_file(dir + "/gamez_over.zbd", over);
    const int r2 = read(dir + "/gamez_over.zbd", &live2, &cap2);
    std::printf("  gamez: c1 %d slots, %d live (pool %d); with %d more: read %d, %d live (pool %d); array past the file: %d\n", size,
                live0, cap0, kMore, r1, live1, cap1, r2);
    EXPECT(r0 == 0 && r1 == 0 && live1 == live0 && cap1 >= size + kMore && r2 == -1);
  }
}

// A mod's motion as unzbd's JSON (docs/modding.md): the folder's zbd/motion/<name>.json, then unzbd's zbd/motion.zip,
// then the archives; a later mod's not reached past a mod with its own motion.zbd. One that does not read is refused
// and the archive's motion read, as a broken copy is.
TEST(bounded_motion_json_in_a_folder_or_a_zip) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/motion.zbd", ec)) return;
  const zutl::Mounts saved = zutl::mounts();
  const auto put = [](const std::string& path, const std::string& text) {
    std::error_code e;
    fs::create_directories(fs::path(path).parent_path(), e);
    std::ofstream(path, std::ios::binary) << text;
  };
  const char* kName = "vulture_walk";
  std::string json;
  {
    zutl_zar::Archive a;
    zutl_zar::init(a);
    EXPECT(zutl_zar::open(a, (install + "/zbd/motion.zbd").c_str()));
    uint32_t length = 0;
    std::FILE* f = zutl_zar::seek(a, zutl_zar::find(a, kName), &length);
    std::vector<uint8_t> entry(length);
    EXPECT(f && std::fread(entry.data(), 1, length, f) == length && zsis::motion_to_json(entry.data(), entry.size(), &json));
    zutl_zar::destroy(a);
  }
  const auto with_loop = [&](const char* t) {
    std::string j = json;
    const size_t at = j.find("\"loop_time\": ");
    const size_t end = j.find(',', at);
    return j.replace(at, end - at, std::string("\"loop_time\": ") + t);
  };
  const std::string wd = std::string(MW3_TEST_TMP) + "/mods_motion_json";
  fs::remove_all(wd, ec);
  const std::string mod = wd + "/mods/motionmod", later = wd + "/mods/later";
  put(mod + "/mod.txt", "name=Motion\n");
  put(later + "/mod.txt", "name=Later\n");
  put(later + "/zbd/motion/vulture_walk.json", with_loop("9.0"));
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"motionmod", "later"}, {install});
  zutl::mount(install);
  const auto loop = [&]() {
    EXPECT(mech3::open_motions("zbd/motion.zbd"));
    zsis::Script sc;
    const bool ok = mech3::load_motion(sc, kName);
    const float t = ok ? sc.loop_time : -1.0f;
    mech3::script_free_parts(sc);
    mech3::close_motions();
    return t;
  };
  const float shipped = loop();
  EXPECT(shipped == 9.0f);  // only the later mod has one
  write_stored_zip(mod + "/zbd/motion.zip", {{"VULTURE_WALK.JSON", with_loop("3.5")}});
  EXPECT(loop() == 3.5f);
  put(mod + "/zbd/motion/vulture_walk.json", with_loop("4.25"));
  EXPECT(loop() == 4.25f);  // the folder before the zip
  put(mod + "/zbd/motion/vulture_walk.json", with_loop("-1.0"));
  const float refused = loop();
  put(mod + "/zbd/motion/vulture_walk.json", "{\"loop_time\": 2.0,\n \"frame_count\": 3, \"parts\": [{\"name\": \"hip\", \"frames\": []}]}");
  const float cut = loop();
  fs::remove_all(mod + "/zbd/motion", ec);
  fs::remove(mod + "/zbd/motion.zip", ec);
  fs::create_directories(mod + "/zbd", ec);
  fs::copy_file(install + "/zbd/motion.zbd", mod + "/zbd/motion.zbd", ec);
  const float own = loop();  // the mod's own archive stops the search: the later mod's JSON not read
  std::printf("  later mod 9 -> %.2f; zip 3.5, folder 4.25; refused -> %.3f, %.3f; own archive -> %.3f\n", shipped, refused, cut, own);
  EXPECT(refused != -1.0f && refused != 9.0f && refused > 0.0f);  // the archive's
  EXPECT(cut == refused && own == refused);
  zutl::mounts() = saved;
}

// One node, Initial, one model of three vertices (the second's y given) and one polygon: a mechlib model as
// unzbd writes one, the least it holds.
std::string tiny_model_json(const char* y) {
  const std::string box = "{\"a\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"b\": {\"x\": 1, \"y\": 1, \"z\": 1}}";
  const std::string v = "{\"x\": 0, \"y\": 0, \"z\": 0}";
  return "{\"nodes\": [{\"name\": \"tiny.flt\", \"flags\": {\"active\": true, \"bbox_node\": true, \"tree_valid\": true}, \"update_flags\": 1,"
         " \"zone_id\": -1, \"model_index\": 0, \"area_partition\": null, \"virtual_partition\": null, \"parent_indices\": [],"
         " \"child_indices\": [], \"active_bbox\": \"Node\", \"node_bbox\": " + box + ", \"model_bbox\": " + box + ", \"child_bbox\": " + box +
         ", \"field192\": 0, \"field196\": 160, \"field200\": 0, \"field204\": 0, \"data\": {\"Object3d\": {\"opacity\": null, \"color\": null,"
         " \"unk\": 0.0, \"transform\": \"Initial\", \"signs\": 0}}, \"data_ptr\": 1, \"parent_array_ptr\": 0, \"child_array_ptr\": 0, \"index\": 1}],"
         " \"models\": [{\"model_type\": \"Default\", \"facade_mode\": \"CylindricalY\", \"flags\": {\"lighting\": true}, \"parent_count\": 1,"
         " \"vertices\": [" + v + ", {\"x\": 1, \"y\": " + y + ", \"z\": 0}, {\"x\": 0, \"y\": 1, \"z\": 1}], \"normals\": [], \"morphs\": [], \"lights\": [],"
         " \"polygons\": [{\"flags\": {}, \"priority\": 0, \"zone_set\": [], \"vertex_indices\": [0, 1, 2], \"normal_indices\": null,"
         " \"vertex_colors\": [{\"r\": 255, \"g\": 255, \"b\": 255}, {\"r\": 255, \"g\": 255, \"b\": 255}, {\"r\": 255, \"g\": 255, \"b\": 255}],"
         " \"materials\": [{\"material_index\": 0, \"uv_coords\": null}], \"vertex_indices_ptr\": 1, \"normal_indices_ptr\": 0, \"uvs_ptr\": 0,"
         " \"vertex_colors_ptr\": 1, \"matl_refs_ptr\": 0, \"materials_ptr\": 0}], \"texture_scroll\": {\"u\": 0, \"v\": 0},"
         " \"bbox_mid\": " + v + ", \"bbox_diag\": 1.5, \"polygons_ptr\": 1, \"vertices_ptr\": 1, \"normals_ptr\": 0, \"lights_ptr\": 0,"
         " \"morphs_ptr\": 0, \"material_refs_ptr\": 0}]}";
}

// A mod's mechlib model as unzbd's JSON (docs/modding.md): one the archive lacks, as a new mech's would be, from the
// folder's zbd/mechlib/<model>.flt.json, then unzbd's zbd/mechlib.zip, read by the archive's bounded reader; one that
// does not read refused; a mod's own mechlib.zbd stopping the search.
TEST(bounded_mechlib_json_in_a_folder_or_a_zip) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/mechlib.zbd", ec)) return;
  const zutl::Mounts saved = zutl::mounts();
  const auto put = [](const std::string& path, const std::string& text) {
    std::error_code e;
    fs::create_directories(fs::path(path).parent_path(), e);
    std::ofstream(path, std::ios::binary) << text;
  };
  const std::string wd = std::string(MW3_TEST_TMP) + "/mods_mechlib_json";
  fs::remove_all(wd, ec);
  const std::string mod = wd + "/mods/mechmod", later = wd + "/mods/later";
  put(mod + "/mod.txt", "name=Mech\n");
  put(later + "/mod.txt", "name=Later\n");
  put(later + "/zbd/mechlib/tiny.flt.json", tiny_model_json("9.0"));
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"mechmod", "later"}, {install});
  zutl::mount(install);
  const auto second_y = [&]() {
    zmod::models() = zmod::ModelPool{};
    zmod::reset_pool();
    zcls::MechLib lib;
    zutl_zar::init(lib.archive);
    float y = -1;
    if (zcls::mechlib_open(lib, "zbd\\mechlib.zbd"))
      if (zcls::Node* n = zcls::mechlib_load(lib, "tiny.flt"); n && n->model && n->model->count(0x14) == 3) y = n->model->vertices[4];
    zcls::mechlib_close(lib);
    return y;
  };
  EXPECT(second_y() == 9.0f);  // only the later mod has it
  write_stored_zip(mod + "/zbd/mechlib.zip", {{"TINY.FLT.JSON", tiny_model_json("3.5")}});
  EXPECT(second_y() == 3.5f);
  put(mod + "/zbd/mechlib/tiny.flt.json", tiny_model_json("4.25"));
  EXPECT(second_y() == 4.25f);  // the folder before the zip
  put(mod + "/zbd/mechlib/tiny.flt.json", tiny_model_json("\"high\""));
  EXPECT(second_y() == -1.0f);  // refused, and the archive has none: no model
  fs::remove_all(mod + "/zbd/mechlib", ec);
  fs::remove(mod + "/zbd/mechlib.zip", ec);
  fs::copy_file(install + "/zbd/mechlib.zbd", mod + "/zbd/mechlib.zbd", ec);
  EXPECT(second_y() == -1.0f);  // the mod's own archive stops the search: the later mod's not read
  zutl::mounts() = saved;
}

// What a mechlib model's JSON refuses, with its line and place (docs/modding.md).
TEST(bounded_mechlib_json_errors_name_the_place) {
  std::vector<uint8_t> bytes;
  std::string why;
  const std::string good = tiny_model_json("1.0");
  EXPECT(zcls::mechlib_model_from_json(good, &bytes, &why) && bytes.size() == 208 + 144 + 92 + 3 * 12 + 36 + 3 * 4 + 3 * 12);
  const auto with = [&](const char* from, const char* to) {
    std::string t = good;
    const size_t at = t.find(from);
    return at == std::string::npos ? std::string("missing ") + from : t.replace(at, std::strlen(from), to);
  };
  const std::pair<std::string, const char*> cases[] = {
      {with("\"model_index\": 0", "\"model_index\": 3"), "(at $.nodes[0].model_index): no model 3"},
      {with("\"child_indices\": []", "\"child_indices\": [0]"), "(at $.nodes[0]): reached twice"},
      {with("\"Object3d\"", "\"Camera\""), "(at $.nodes[0].data): not an Object3d"},
      {with("\"transform\": \"Initial\"", "\"transform\": \"Spin\""), "(at $.nodes[0].data.Object3d.transform): not \"Initial\""},
      {with("\"bbox_node\": true", "\"bbox_nod\": true"), "(at $.nodes[0].flags.bbox_nod): not a flag this format has"},
      {with("\"facade_mode\": \"CylindricalY\"", "\"facade_mode\": \"Round\""), "(at $.models[0].facade_mode): not one of"},
      {with("\"priority\": 0", "\"priority\": 51"), "(at $.models[0].polygons[0].priority): 51: past -50..50"},
      {with("\"materials\": [{\"material_index\": 0, \"uv_coords\": null}]", "\"materials\": []"), "(at $.models[0].polygons[0].materials): no material"},
      {with("\"name\": \"tiny.flt\"", "\"name\": \"a_name_far_longer_than_the_record_holds\""), "(at $.nodes[0].name): longer than 35"},
      {with("\"lights\": []", "\"lights\": [{}]"), "(at $.models[0].lights): a mechlib model's lights are not read"},
      {with("{\"x\": 1, \"y\": 1.0", "{\"x\": 1,\n \"y\": true"), "line 2 (at $.models[0].vertices[1].y): not a number"},
  };
  for (const auto& [text, want] : cases) {
    why.clear();
    const bool ok = zcls::mechlib_model_from_json(text, &bytes, &why);
    std::printf("  -> %s\n", why.c_str());
    EXPECT(!ok && why.find(want) != std::string::npos);
  }
}
