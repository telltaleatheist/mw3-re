// The model readers' bound (FIX (A) memory safety, zmod::ReadBound, docs/fixes.md): a count from the file that asks
// for more than the file holds is refused fast and in little memory; a cut file's arrays still read as zeros.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>
#if !defined(_WIN32)
#include <sys/resource.h>
#endif

#include "check.h"
#include "gamez/zmodel/gmod_const.h"

namespace {
// The process's peak resident size in MB (0 where it isn't known).
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

void put(std::vector<uint8_t>& b, size_t at, uint32_t v) { std::memcpy(b.data() + at, &v, 4); }

// MW3's model section: size, count, free head, then size slots of 0x60 (header 0x5c, link), then the arrays.
std::FILE* models_file(const std::vector<uint8_t>& slots, int32_t size, size_t tail = 0) {
  std::FILE* f = std::tmpfile();
  const int32_t head[3] = {size, size, -1};
  std::fwrite(head, 4, 3, f);
  std::fwrite(slots.data(), 1, slots.size(), f);
  const std::vector<uint8_t> zeros(tail, 0);
  if (tail) std::fwrite(zeros.data(), 1, tail, f);
  std::rewind(f);
  return f;
}
}  // namespace

TEST(model_reader_refuses_a_count_past_the_file) {
  zmod::ModelPool saved = std::move(zmod::models());
  zmod::models() = zmod::ModelPool{};
  const double before = peak_mb();
  const auto t0 = std::chrono::steady_clock::now();
  std::vector<uint8_t> slots(0x60 * 2, 0);
  put(slots, 0x0c, 1), put(slots, 0x14, 0x10000000u), put(slots, 0x10, 5);  // 3 GB of vertices
  put(slots, 0x60 + 0x0c, 1), put(slots, 0x60 + 0x14, 4);                  // and a good one after
  std::FILE* f = models_file(slots, 2, 48);
  EXPECT(zmod::read_models(f) == 2);
  std::fclose(f);
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  zmod::Model& bad = zmod::models().slots[0];
  EXPECT(bad.vertices == nullptr && bad.polygons == nullptr);
  EXPECT(bad.count(0x14) == 0 && bad.count(0x10) == 0);
  EXPECT(zmod::read_bound().refused);
  // The read stops there: the model after it is left empty too (its place in the file is not known).
  EXPECT(zmod::models().slots[1].vertices == nullptr && zmod::models().slots[1].count(0x14) == 0);
  EXPECT(secs < 1.0);
  EXPECT(peak_mb() - before < 256.0);
  std::printf("  refused in %.3f s, peak grew %.0f MB\n", secs, peak_mb() - before);
  zmod::models() = std::move(saved);
}

TEST(model_reader_refuses_a_huge_table) {
  zmod::ModelPool saved = std::move(zmod::models());
  zmod::models() = zmod::ModelPool{};
  const double before = peak_mb();
  std::FILE* f = models_file(std::vector<uint8_t>(0x60, 0), 0x7fffffff);
  EXPECT(zmod::read_models(f) == -1);
  std::fclose(f);
  EXPECT(zmod::models().slots.empty());
  EXPECT(peak_mb() - before < 256.0);
  zmod::models() = std::move(saved);
}

// A cut file: the last model's arrays run past the end and read as zeros, the exe's way (kept within the bound).
TEST(model_reader_reads_a_cut_files_arrays_as_zeros) {
  zmod::ModelPool saved = std::move(zmod::models());
  zmod::models() = zmod::ModelPool{};
  std::vector<uint8_t> slots(0x60, 0);
  put(slots, 0x0c, 1), put(slots, 0x14, 4), put(slots, 0x18, 4);  // 96 bytes asked, 20 there
  std::FILE* f = models_file(slots, 1, 20);
  zmod::read_models(f);
  std::fclose(f);
  const zmod::Model& m = zmod::models().slots[0];
  EXPECT(!zmod::read_bound().refused);
  EXPECT(m.vertices != nullptr && m.count(0x14) == 4 && m.count(0x18) == 4);
  zmod::models() = std::move(saved);
}
