#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd_cd.h"
#include "gamez/zutil/zutl_zar.h"

namespace {
// Builds binary reader nodes and archives by hand, in the on-disk layout.
struct Bytes {
  std::string b;
  Bytes& u32(uint32_t v) { b.append(reinterpret_cast<const char*>(&v), 4); return *this; }
  Bytes& i(int32_t v) { return u32(1).u32(static_cast<uint32_t>(v)); }
  Bytes& f(float v) { uint32_t x; std::memcpy(&x, &v, 4); return u32(2).u32(x); }
  Bytes& s(const std::string& v) { u32(3).u32(static_cast<uint32_t>(v.size())); b += v; return *this; }
  Bytes& list(uint32_t items) { return u32(4).u32(items + 1); }
};

std::string archive(const std::vector<std::pair<std::string, std::string>>& files) {
  std::string data;
  std::vector<zutl_zar::Entry> table;
  for (const auto& [name, body] : files) {
    zutl_zar::Entry e{};
    e.start = static_cast<uint32_t>(data.size());
    e.length = static_cast<uint32_t>(body.size());
    std::strncpy(e.name, name.c_str(), sizeof e.name - 1);
    table.push_back(e);
    data += body;
  }
  data.append(reinterpret_cast<const char*>(table.data()), table.size() * sizeof(zutl_zar::Entry));
  return data + Bytes().u32(1).u32(static_cast<uint32_t>(table.size())).b;
}

std::string write_temp(const std::string& name, const std::string& data) {
  std::string path = std::string(MW3_TEST_TMP) + "/test_" + name;  // inside build/
  std::FILE* f = std::fopen(path.c_str(), "wb");
  std::fwrite(data.data(), 1, data.size(), f);
  std::fclose(f);
  return path;
}

// ("NAME" "atlas" "SPEED" 1.5 "TONS" 75 "POS" (1 2.5 3) "WRAP" (7) "LAST")
std::string sample() {
  Bytes b;
  b.list(11).s("NAME").s("atlas").s("SPEED").f(1.5f).s("TONS").i(75);
  b.s("POS").list(3).i(1).f(2.5f).i(3);
  b.s("WRAP").list(1).i(7);
  b.s("LAST");
  return b.b;
}

struct Archives {
  Archives() { zrdr::init(); }
  ~Archives() { zrdr::close_archives(true); }
};
}  // namespace

TEST(zreader_reads_nodes_and_typed_values) {
  Archives guard;
  std::string path = write_temp("a.zbd", archive({{"other.zrd", Bytes().list(0).b}, {"mech.zrd", sample()}}));
  EXPECT(zrdr::open_archive(path.c_str(), true));
  // The directory is dropped and the name matched without case, as in the original.
  zrdr::Node* root = zrdr::load("..\\data\\common\\zrdr\\MECH.zrd");
  EXPECT(root != nullptr);
  if (!root) return;
  EXPECT(root->type == zrdr::List && zrdr::list_count(root) == 12);
  EXPECT(std::string(zrdr::get_string(root, "NAME")) == "atlas");
  float speed = 0;
  EXPECT(zrdr::get_float(root, "SPEED", &speed) && speed == 1.5f);
  float tons = 0;
  EXPECT(zrdr::get_float(root, "TONS", &tons) && tons == 75.0f);  // Int converted
  int32_t n = 0;
  EXPECT(!zrdr::get_int(root, "SPEED", &n));  // a Float is not an Int
  EXPECT(zrdr::get_int(root, "WRAP", &n) && n == 7);  // one-item list
  int32_t keep = 42;
  zrdr::get_int_if(root, "MISSING", &keep);
  EXPECT(keep == 42);
  float pos[3] = {0, 0, 0};
  EXPECT(zrdr::get_vec3(zrdr::find(root, "POS"), pos) && pos[0] == 1 && pos[1] == 2.5f && pos[2] == 3);
  EXPECT(zrdr::free_node(root) == 0);
  EXPECT(zrdr::load("absent.zrd") == nullptr);
}

TEST(zreader_find_keeps_original_quirks) {
  Archives guard;
  std::string path = write_temp("b.zbd", archive({{"q.zrd", sample()}}));
  zrdr::open_archive(path.c_str(), true);
  zrdr::Node* root = zrdr::load("q.zrd");
  if (!root) { EXPECT(false); return; }
  // A string value matches like a key: "atlas" finds the node after it.
  EXPECT(zrdr::find(root, "atlas") == &root->list[3]);
  // A key in the last slot returns one past the end of the list.
  EXPECT(zrdr::find(root, "LAST") == &root->list[12]);
  // ... and reading that slot finds no value (FIX (A): the spare zeroed slot).
  int32_t last = -1;
  EXPECT(!zrdr::get_int(root, "LAST", &last) && last == -1);
  // find searches nested lists; find_flat does not.
  zrdr::Node* outer = zrdr::new_nodes(zrdr::List, 1);
  outer->list = zrdr::alloc_list(2);
  outer->list[0].type = zrdr::Int;
  outer->list[0].i = 2;
  outer->list[1] = *root;
  EXPECT(zrdr::find(outer, "TONS") == &root->list[6]);
  EXPECT(zrdr::find_flat(outer, "TONS", 1) == nullptr);
  std::free(outer->list);
  std::free(outer);
  zrdr::free_node(root);
}

TEST(zreader_archives_searched_in_open_order) {
  Archives guard;
  std::string p1 = write_temp("c1.zbd", archive({{"x.zrd", Bytes().list(1).i(1).b}}));
  std::string p2 = write_temp("c2.zbd", archive({{"x.zrd", Bytes().list(1).i(2).b}, {"y.zrd", Bytes().list(1).i(3).b}}));
  zrdr::open_archive(p1.c_str(), false);
  zrdr::open_archive(p2.c_str(), true);
  zrdr::Node* x = zrdr::load("x.zrd");
  EXPECT(x && x->list[1].i == 1);  // first opened wins
  zrdr::free_node(x);
  zrdr::close_archives(false);  // keeps the current one (c2)
  EXPECT(zrdr::state().archives.size() == 1);
  x = zrdr::load("x.zrd");
  EXPECT(x && x->list[1].i == 2);
  zrdr::free_node(x);
  EXPECT(!zrdr::open_archive("/nonexistent/none.zbd", false));
}

TEST(zreader_cd_tracks_from_the_real_reader_zbd) {
  std::string path = std::string(MW3_GAME_DIR) + "/zbd/reader.zbd";
  if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
    std::fclose(f);
  } else {
    std::printf("  (skipped: no %s)\n", path.c_str());
    return;
  }
  Archives guard;
  EXPECT(zrdr::open_archive(path.c_str(), true));
  EXPECT(zrdr::state().current && zrdr::state().current->count == 178);
  zrdr::Node* sounds = zrdr::load("sounds.zrd");
  EXPECT(sounds != nullptr);
  if (!sounds) return;
  std::vector<zsnd_cd::Cue> cues = zsnd_cd::read_cues(zrdr::find(sounds, "CD_TRACKS"));
  // The table the CD tests have used, written down from the trace by hand.
  const std::vector<zsnd_cd::Cue> expected = {{"menu", {2}}, {"c1", {3}}, {"c2", {3}}, {"c3", {3}},
                                              {"c4", {3}},   {"dev1", {2}}, {"dev2", {2}}};
  EXPECT(cues.size() == expected.size());
  for (size_t i = 0; i < cues.size() && i < expected.size(); ++i)
    EXPECT(cues[i].name == expected[i].name && cues[i].tracks == expected[i].tracks);
  zrdr::free_node(sounds);
}
