#include "upgrades/mods/bz2_read.h"

#include <cstring>

namespace mech3 {
namespace {

// Bits from the most significant end, as bzip2 writes them. Reading past the
// end sets `bad` and gives zeros.
struct Bits {
  const uint8_t* p;
  size_t n, at = 0;
  uint64_t hold = 0;
  int have = 0;
  bool bad = false;
  uint32_t get(int count) {  // count <= 32
    while (have < count) {
      if (at >= n) {
        bad = true;
        return 0;
      }
      hold = hold << 8 | p[at++];
      have += 8;
    }
    have -= count;
    return static_cast<uint32_t>(hold >> have) & (count == 32 ? 0xffffffffu : ((1u << count) - 1));
  }
};

const uint32_t* crc_table() {
  static uint32_t t[256];
  static bool built = false;
  if (!built) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t v = i << 24;
      for (int b = 0; b < 8; ++b) v = (v & 0x80000000u) ? (v << 1) ^ 0x04c11db7u : v << 1;
      t[i] = v;
    }
    built = true;
  }
  return t;
}

constexpr int kMaxGroups = 6, kMaxSymbols = 258, kMaxLen = 20, kGroupRun = 50;

struct Table {
  int min_len = 0, max_len = 0;
  int32_t limit[kMaxLen + 2] = {}, base[kMaxLen + 2] = {};
  uint16_t perm[kMaxSymbols] = {};
};

// One coding table from its code lengths (canonical, shortest first).
bool make_table(const uint8_t* len, int symbols, Table* t) {
  t->min_len = kMaxLen + 1, t->max_len = 0;
  for (int i = 0; i < symbols; ++i) {
    if (len[i] < 1 || len[i] > kMaxLen) return false;
    if (len[i] < t->min_len) t->min_len = len[i];
    if (len[i] > t->max_len) t->max_len = len[i];
  }
  int pp = 0;
  for (int l = t->min_len; l <= t->max_len; ++l)
    for (int i = 0; i < symbols; ++i)
      if (len[i] == l) t->perm[pp++] = static_cast<uint16_t>(i);
  int count[kMaxLen + 2] = {};
  for (int i = 0; i < symbols; ++i) ++count[len[i]];
  int32_t code = 0, index = 0;
  for (int l = t->min_len; l <= t->max_len; ++l) {
    t->base[l] = index - code;  // perm index = code + base
    code += count[l];
    index += count[l];
    t->limit[l] = code - 1;  // the largest code of this length
    code <<= 1;
  }
  return true;
}

}  // namespace

bool bz2_decompress(const uint8_t* data, size_t size, size_t max_out, std::vector<uint8_t>* out, std::string* why, size_t* used) {
  auto fail = [&](const char* w) {
    if (why) *why = w;
    out->clear();
    return false;
  };
  out->clear();
  if (size < 4 || data[0] != 'B' || data[1] != 'Z' || data[2] != 'h' || data[3] < '1' || data[3] > '9')
    return fail("not a bzip2 stream");
  const uint32_t block_max = static_cast<uint32_t>(data[3] - '0') * 100000u;
  Bits in{data + 4, size - 4};
  const uint32_t* crc_t = crc_table();
  std::vector<uint32_t> tt;       // the block's symbols, then the inverse transform's links
  std::vector<uint8_t> selectors;
  uint32_t combined = 0;
  for (;;) {
    const uint32_t m1 = in.get(24), m2 = in.get(24);
    if (in.bad) return fail("the bzip2 stream ends early");
    if (m1 == 0x177245 && m2 == 0x385090) {  // the stream's end
      const uint32_t want = in.get(32);
      if (in.bad || want != combined) return fail("the bzip2 stream's checksum is wrong");
      if (used) *used = 4 + in.at;  // whole bytes read (the end is padded to one)
      return true;
    }
    if (m1 != 0x314159 || m2 != 0x265359) return fail("a bzip2 block does not start as one");
    const uint32_t block_crc = in.get(32);
    if (in.get(1)) return fail("a randomised bzip2 block (not read)");
    const uint32_t orig = in.get(24);
    // The bytes in use.
    uint8_t seq[256];
    int in_use = 0;
    const uint32_t groups16 = in.get(16);
    for (int g = 0; g < 16; ++g)
      if (groups16 >> (15 - g) & 1) {
        const uint32_t bits = in.get(16);
        for (int k = 0; k < 16; ++k)
          if (bits >> (15 - k) & 1) seq[in_use++] = static_cast<uint8_t>(g * 16 + k);
      }
    if (in.bad || in_use == 0) return fail("a bzip2 block uses no bytes");
    const int symbols = in_use + 2;
    const int groups = static_cast<int>(in.get(3));
    const int n_sel = static_cast<int>(in.get(15));
    if (in.bad || groups < 2 || groups > kMaxGroups || n_sel < 1) return fail("a bzip2 block's tables are wrong");
    selectors.assign(static_cast<size_t>(n_sel), 0);
    {
      uint8_t order[kMaxGroups];
      for (int i = 0; i < groups; ++i) order[i] = static_cast<uint8_t>(i);
      for (int i = 0; i < n_sel; ++i) {
        int j = 0;
        while (in.get(1)) {
          if (++j >= groups || in.bad) return fail("a bzip2 block's selectors are wrong");
        }
        const uint8_t v = order[j];
        for (; j > 0; --j) order[j] = order[j - 1];
        order[0] = v;
        selectors[static_cast<size_t>(i)] = v;
      }
    }
    Table tables[kMaxGroups];
    for (int t = 0; t < groups; ++t) {
      uint8_t len[kMaxSymbols];
      int l = static_cast<int>(in.get(5));
      for (int i = 0; i < symbols; ++i) {
        for (;;) {
          if (l < 1 || l > kMaxLen || in.bad) return fail("a bzip2 block's code lengths are wrong");
          if (!in.get(1)) break;
          l += in.get(1) ? -1 : 1;
        }
        len[i] = static_cast<uint8_t>(l);
      }
      if (!make_table(len, symbols, &tables[t])) return fail("a bzip2 block's code lengths are wrong");
    }
    // The symbols: runs of the front byte (RUNA, RUNB), moved-to-front places, the end.
    uint8_t mtf[256];
    for (int i = 0; i < in_use; ++i) mtf[i] = seq[i];
    uint32_t counts[256] = {};
    tt.clear();
    const int end_of_block = in_use + 1;
    int sel = 0, left_in_group = 0;
    const Table* tb = nullptr;
    uint32_t run = 0, run_bit = 1;
    for (;;) {
      if (left_in_group == 0) {
        if (sel >= n_sel) return fail("a bzip2 block runs past its selectors");
        tb = &tables[selectors[static_cast<size_t>(sel++)]];
        left_in_group = kGroupRun;
      }
      --left_in_group;
      int l = tb->min_len;
      int32_t code = static_cast<int32_t>(in.get(l));
      while (l <= tb->max_len && code > tb->limit[l]) code = code << 1 | static_cast<int32_t>(in.get(1)), ++l;
      if (in.bad || l > tb->max_len) return fail("a bzip2 block's data is wrong");
      const int32_t index = code + tb->base[l];
      if (index < 0 || index >= symbols) return fail("a bzip2 block's data is wrong");
      const int s = tb->perm[index];
      if (s <= 1) {  // RUNA (0) and RUNB (1): the run's length in bijective base 2
        if (run_bit > (1u << 24)) return fail("a bzip2 block's run is too long");
        run += run_bit << s;
        run_bit <<= 1;
        continue;
      }
      if (run) {
        if (tt.size() + run > block_max) return fail("a bzip2 block is larger than its stream allows");
        const uint8_t b = mtf[0];
        counts[b] += run;
        tt.insert(tt.end(), run, b);
        run = 0, run_bit = 1;
      }
      if (s == end_of_block) break;
      if (tt.size() >= block_max) return fail("a bzip2 block is larger than its stream allows");
      const int place = s - 1;  // 1..in_use-1
      if (place >= in_use) return fail("a bzip2 block's data is wrong");
      const uint8_t b = mtf[place];
      std::memmove(mtf + 1, mtf, static_cast<size_t>(place));
      mtf[0] = b;
      ++counts[b];
      tt.push_back(b);
    }
    const uint32_t n = static_cast<uint32_t>(tt.size());
    if (n == 0 || orig >= n) return fail("a bzip2 block's start is outside it");
    // The inverse transform: each byte's place in the sorted column links to the next.
    uint32_t start[256], sum = 0;
    for (int i = 0; i < 256; ++i) start[i] = sum, sum += counts[i];
    for (uint32_t i = 0; i < n; ++i) {
      const uint8_t b = static_cast<uint8_t>(tt[i] & 0xff);
      tt[start[b]++] |= i << 8;
    }
    // Walked from the start, with the first run-length layer undone: four equal bytes, then a count of more.
    uint32_t pos = tt[orig] >> 8, crc = 0xffffffffu;
    int same = 0, last = -1;
    for (uint32_t i = 0; i < n; ++i) {
      const uint32_t e = tt[pos];
      const uint8_t b = static_cast<uint8_t>(e & 0xff);
      pos = e >> 8;
      if (same == 4) {  // b is a count of repeats of `last`
        if (out->size() + b > max_out) return fail("the bzip2 stream unpacks to more than its declared size");
        for (int k = 0; k < b; ++k) crc = crc << 8 ^ crc_t[(crc >> 24) ^ static_cast<uint8_t>(last)];
        out->insert(out->end(), b, static_cast<uint8_t>(last));
        same = 0, last = -1;
        continue;
      }
      if (out->size() >= max_out) return fail("the bzip2 stream unpacks to more than its declared size");
      out->push_back(b);
      crc = crc << 8 ^ crc_t[(crc >> 24) ^ b];
      if (b == last) ++same;
      else same = 1, last = b;
    }
    crc = ~crc;
    if (crc != block_crc) return fail("a bzip2 block's checksum is wrong");
    combined = (combined << 1 | combined >> 31) ^ crc;
  }
}

}  // namespace mech3
