#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"

// Ours: zrd's binary form (docs/zreader.md) read and written in place, for
// the editor's saves (Add vehicle's record, the vehicle editor's records).
namespace mech3::zrdbytes {

inline uint32_t get32(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  return v;
}
inline void put32(std::vector<uint8_t>& o, uint32_t v) {
  const uint8_t* b = reinterpret_cast<const uint8_t*>(&v);
  o.insert(o.end(), b, b + 4);
}
inline void put_int(std::vector<uint8_t>& o, int32_t v) { put32(o, zrdr::Int), put32(o, static_cast<uint32_t>(v)); }
inline void put_float(std::vector<uint8_t>& o, float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  put32(o, zrdr::Float), put32(o, u);
}
inline void put_string(std::vector<uint8_t>& o, const std::string& s) {
  put32(o, zrdr::String), put32(o, static_cast<uint32_t>(s.size()));
  o.insert(o.end(), s.begin(), s.end());
}
// The end of the node at `at`, 0 when it runs past `size` or does not read.
inline size_t skip(const uint8_t* p, size_t size, size_t at, int depth = 0) {
  if (at + 8 > size || depth > 64) return 0;
  const uint32_t t = get32(p + at), v = get32(p + at + 4);
  if (t == zrdr::Int || t == zrdr::Float) return at + 8;
  if (t == zrdr::String) return v <= size - at - 8 ? at + 8 + v : 0;
  if (t != zrdr::List || v == 0) return 0;
  size_t q = at + 8;
  for (uint32_t i = 1; i < v; ++i)
    if (!(q = skip(p, size, q, depth + 1))) return 0;
  return q;
}
inline std::string string_at(const uint8_t* p, size_t size, size_t at) {
  if (at + 8 > size || get32(p + at) != zrdr::String) return {};
  const uint32_t n = get32(p + at + 4);
  return n <= size - at - 8 ? std::string(reinterpret_cast<const char*>(p + at + 8), n) : std::string();
}
// The pairs list's items: each key's start and its value's start and end.
struct Pair {
  std::string key;
  size_t value = 0, end = 0;
};
inline bool pairs_of(const uint8_t* p, size_t size, std::vector<Pair>* out, size_t* list_end) {
  if (size < 16 || get32(p) != zrdr::List || get32(p + 8) != zrdr::List) return false;
  const uint32_t n = get32(p + 12);
  size_t q = 16;
  for (uint32_t i = 1; i < n; i += 2) {
    const size_t k_end = skip(p, size, q);
    if (!k_end) return false;
    Pair pr;
    pr.key = string_at(p, size, q);
    pr.value = k_end;
    if (i + 1 >= n) {  // a key without a value
      q = k_end;
      break;
    }
    if (!(pr.end = skip(p, size, k_end))) return false;
    out->push_back(pr);
    q = pr.end;
  }
  *list_end = q;
  return true;
}
// aiv.zrd's records: a name followed by its list. The game finds a record by its name anywhere in the file
// (0x004e0680), so loose words between records (PM's readermp files have ":spawn points for one base", a comment
// the file's compiler kept) are stepped over one by one, not taken as pairs; they stay where they are.
inline bool record_pairs_of(const uint8_t* p, size_t size, std::vector<Pair>* out, size_t* list_end) {
  if (size < 16 || get32(p) != zrdr::List || get32(p + 8) != zrdr::List) return false;
  const uint32_t n = get32(p + 12);
  size_t q = 16;
  for (uint32_t i = 1; i < n;) {
    const size_t k_end = skip(p, size, q);
    if (!k_end) return false;
    if (get32(p + q) == zrdr::String && i + 1 < n && k_end + 8 <= size && get32(p + k_end) == zrdr::List) {
      Pair pr;
      pr.key = string_at(p, size, q);
      pr.value = k_end;
      if (!(pr.end = skip(p, size, k_end))) return false;
      out->push_back(pr);
      q = pr.end, i += 2;
    } else {
      q = k_end, ++i;  // a loose item
    }
  }
  *list_end = q;
  return true;
}
}  // namespace mech3::zrdbytes
