// Pirate's Moon's model reader under the shared bound (FIX (A) memory safety, zmod::ReadBound, docs/fixes.md): a count
// from the file past what it holds is refused fast and in little memory, the counts not read zeroed.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#if !defined(_WIN32)
#include <sys/resource.h>
#endif

#include "check.h"
#include "gamez/zmodel/gmod_const.h"
#include "pm/gamez/zmodel/gmod_const.h"

namespace {
double peak_mb() {
#if defined(_WIN32)
  return 0;
#else
  rusage u{};
  getrusage(RUSAGE_SELF, &u);
#if defined(__APPLE__)
  return static_cast<double>(u.ru_maxrss) / (1024.0 * 1024.0);
#else
  return static_cast<double>(u.ru_maxrss) / 1024.0;
#endif
#endif
}

// One model's table (size 1) and its 0x68 record, the arrays at the record's offset (+0x64): what reread_model reads.
pm::zmod::Model* reread(std::vector<uint8_t> rec, const std::vector<uint8_t>& data) {
  std::FILE* f = std::tmpfile();
  const int32_t head[3] = {1, 1, -1};
  const uint32_t at = 12 + 0x68;
  std::memcpy(rec.data() + 0x64, &at, 4);
  std::fwrite(head, 4, 3, f);
  std::fwrite(rec.data(), 1, rec.size(), f);
  if (!data.empty()) std::fwrite(data.data(), 1, data.size(), f);
  std::rewind(f);
  pm::zmod::Model* m = pm::zmod::reread_model(f, 0, 0);
  std::fclose(f);
  return m;
}
void put(std::vector<uint8_t>& b, size_t at, uint32_t v) { std::memcpy(b.data() + at, &v, 4); }
}  // namespace

TEST(pm_model_reader_refuses_a_count_past_the_file) {
  const double before = peak_mb();
  const auto t0 = std::chrono::steady_clock::now();
  std::vector<uint8_t> rec(0x68, 0);
  put(rec, 0x14, 0x10000000u), put(rec, 0x10, 3), put(rec, 0x5c, 2);  // 3 GB of vertices
  pm::zmod::Model* m = reread(rec, {});
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  EXPECT(m != nullptr);
  if (m) {
    EXPECT(m->vertices == nullptr && m->polygons == nullptr && m->refs == nullptr);
    EXPECT(m->count(0x14) == 0 && m->count(0x10) == 0 && m->count(0x5c) == 0);
    pm::zmod::model_release(m);
  }
  EXPECT(::zmod::read_bound().refused);
  EXPECT(secs < 1.0);
  EXPECT(peak_mb() - before < 256.0);
  std::printf("  refused in %.3f s, peak grew %.0f MB\n", secs, peak_mb() - before);
}

// A polygon whose material list asks for 1 GB: the polygons before it are the model's, the rest are not read.
TEST(pm_model_reader_cuts_at_a_polygon_past_the_file) {
  const double before = peak_mb();
  std::vector<uint8_t> rec(0x68, 0);
  put(rec, 0x10, 2), put(rec, 0x5c, 1);
  std::vector<uint8_t> data(0x28 * 2 + 64, 0);
  put(data, 0x00, 3), put(data, 0x10, 1);                // polygon 0: 3 points, 1 material
  put(data, 0x28, 3), put(data, 0x28 + 0x10, 0x10000000u);  // polygon 1: 268 M materials
  put(data, 0x50 + 12, 0xffffffffu);                     // polygon 0's material: none (-1)
  // polygon 0's arrays: 3 point indices, its 1 material index, 3 colours (12 + 4 + 36 = 52 bytes)
  pm::zmod::Model* m = reread(rec, data);
  EXPECT(m != nullptr);
  if (m) {
    EXPECT(m->count(0x10) == 1 && m->polygons != nullptr);
    EXPECT(m->polygons && m->polygons[0].colors != nullptr && m->polygons[0].materials != nullptr);
    EXPECT(m->polygons && m->polygons[1].vertex_indices == nullptr && m->polygons[1].colors == nullptr);
    EXPECT(m->count(0x5c) == 0 && m->refs == nullptr);
    pm::zmod::model_release(m);
  }
  EXPECT(::zmod::read_bound().refused);
  EXPECT(peak_mb() - before < 256.0);
}
