#include "gamez/zmodel/gmod_strip.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <utility>

#include "gamez/zmodel/gmod_matl.h"

#pragma STDC FP_CONTRACT OFF

// docs/mechlib.md, The triangle stripper. The exe keeps the model and the limit in globals (0x0072be60,
// 0x0072be5c); here they travel in a Strip context. Its counters 0x0072be64..0x0072be70 are never read.
namespace zmod {
namespace {

struct Edge {  // Mech3.exe 1.2 @ 0x005545d0: the two vertex indices, smaller first
  uint32_t a, b;
  bool operator<(const Edge& o) const { return a != o.a ? a < o.a : b < o.b; }
  bool operator==(const Edge& o) const { return a == o.a && b == o.b; }
};

struct Dir {  // 0x005534b0 / 0x00553700 / 0x00553760: an edge walked from p to q, dir = q < p
  Edge e;
  bool dir;
  bool operator==(const Dir& o) const { return e == o.e && dir == o.dir; }  // Mech3.exe 1.2 @ 0x00553510
};

// Mech3.exe 1.2 @ 0x005534b0
Dir walk(uint32_t p, uint32_t q) { return {{p < q ? p : q, p < q ? q : p}, q < p}; }

struct Item {  // a polygon on an edge's list and the way it walks the edge
  uint32_t poly;
  bool dir;
};

struct Info {  // 0x14 bytes in the exe: the key and the list
  Edge key;
  std::list<Item> list;
};

struct Rec {  // 0x005535f0: a vertex's record
  uint32_t v, n;
  float uv[2];
  float col[3];
};

// x87 fcomp then `test ah, 0x40`: equal, or unordered.
bool fp_eq(float a, float b) { return !(a < b) && !(a > b); }

struct Strip {
  Model& m;
  uint32_t limit;

  uint32_t count() const {
    uint32_t n;
    std::memcpy(&n, m.header + 0x10, 4);
    return n;
  }
  void set_count(uint32_t n) { std::memcpy(m.header + 0x10, &n, 4); }
  Polygon& poly(uint32_t i) { return m.polygons[i]; }
  uint32_t n(uint32_t i) { return poly(i).flags & 0xff; }
  bool strip(uint32_t i) { return (poly(i).flags >> 10) & 1; }
  bool strip4(uint32_t i) { return strip(i) && n(i) == 4; }
  bool even_strip(uint32_t i) { return strip(i) && !(poly(i).flags & 1); }
  bool odd(uint32_t i) { return poly(i).flags & 1; }
  uint32_t v(uint32_t i, uint32_t k) { return poly(i).vertex_indices[k]; }  // Mech3.exe 1.2 @ 0x00553580
  Dir walk_of(uint32_t i, uint32_t p, uint32_t q) { return walk(v(i, p), v(i, q)); }
  Dir first(uint32_t i) { return walk_of(i, 0, 1); }                    // Mech3.exe 1.2 @ 0x00553700
  Dir last(uint32_t i) { return walk_of(i, n(i) - 2, n(i) - 1); }      // Mech3.exe 1.2 @ 0x00553760

  // FIX (A): the polygon array is new[]'d in the port (cls_mechlib.cpp); the exe reallocs it.
  void resize(uint32_t count) {
    Polygon* grown = count ? new Polygon[count] : nullptr;
    const uint32_t keep = count < this->count() ? count : this->count();
    for (uint32_t i = 0; i < keep; ++i) grown[i] = m.polygons[i];
    delete[] m.polygons;
    m.polygons = grown;
    set_count(count);
  }

  Rec rec(uint32_t i, uint32_t k) {  // Mech3.exe 1.2 @ 0x005535f0
    const Polygon& p = poly(i);
    Rec r{p.vertex_indices[k], p.normal_indices[k], {p.uvs[2 * k], p.uvs[2 * k + 1]},
          {p.colors[3 * k], p.colors[3 * k + 1], p.colors[3 * k + 2]}};
    return r;
  }
  void set_rec(uint32_t i, uint32_t k, const Rec& r) {  // Mech3.exe 1.2 @ 0x00553660
    Polygon& p = poly(i);
    p.vertex_indices[k] = r.v, p.normal_indices[k] = r.n;
    p.uvs[2 * k] = r.uv[0], p.uvs[2 * k + 1] = r.uv[1];
    for (int c = 0; c < 3; ++c) p.colors[3 * k + c] = r.col[c];
  }
  void swap(uint32_t i, uint32_t a, uint32_t b) {  // Mech3.exe 1.2 @ 0x005537d0
    const Rec ra = rec(i, a), rb = rec(i, b);
    set_rec(i, a, rb), set_rec(i, b, ra);
  }
  void reverse(uint32_t i) {  // Mech3.exe 1.2 @ 0x00553990
    for (int32_t k = static_cast<int32_t>((poly(i).flags >> 1) & 0x7f) - 1; k >= 0; --k)
      swap(i, static_cast<uint32_t>(k), n(i) - static_cast<uint32_t>(k) - 1);
  }
  void pair_swap(uint32_t i) {  // Mech3.exe 1.2 @ 0x00553b80
    if (n(i) == 1) return;
    for (uint32_t k = 0; k < n(i) - 1; k += 2) swap(i, k, k + 1);
  }

  bool edge_of(uint32_t i, const Edge& key, bool* dir);
  bool ends_meet(const Item& a, const Item& b, const Edge& key);
  bool join_test(const Item& a, const Item& b, const Edge& key);
  void to_strip(uint32_t i, const Edge& key);
  uint32_t concat(uint32_t x, uint32_t y);
  uint32_t join(const Item& a, const Item& b, const Edge& key);
  void remove(uint32_t i);
};

// Mech3.exe 1.2 @ 0x00553d60: whether polygon i has the edge and the way it walks it. A strip has only its
// first and last edges (a strip of 4 its long sides first), the last walked backwards when the count is even.
bool Strip::edge_of(uint32_t i, const Edge& key, bool* dir) {
  if (strip4(i)) {
    const Dir s = walk_of(i, 2, 0);
    if (s.e == key) return *dir = s.dir, true;
    const Dir t = walk_of(i, 1, 3);
    if (t.e == key) return *dir = t.dir, true;
  }
  if (!strip(i)) {
    for (uint32_t k = 0, prev = n(i) - 1; k < n(i); prev = k++) {
      const Dir s = walk_of(i, prev, k);
      if (s.e == key) return *dir = s.dir, true;
    }
    return false;
  }
  const Dir s = first(i);
  if (s.e == key) return *dir = s.dir, true;
  Dir t = last(i);
  if (even_strip(i)) t.dir = !t.dir;
  if (t.e == key) return *dir = t.dir, true;
  return false;
}

// Mech3.exe 1.2 @ 0x005503c0: whether a's and b's ends meet at the edge so that one strip can carry on
// into the other (read from the asm: the decompile drops branches). A polygon's two ends: a strip's first
// and last edges, a strip of 4 on its long side its two long sides, any other polygon the shared edge and
// the one across from it.
bool Strip::ends_meet(const Item& a, const Item& b, const Edge& key) {
  auto ends = [&](uint32_t i, Dir* e1, Dir* e2) {
    if (strip(i)) {
      *e1 = first(i), *e2 = last(i);
      return true;
    }
    uint32_t k = 0xffffffffu;
    for (uint32_t j = 0, prev = n(i) - 1; j < n(i); prev = j++)
      if (walk_of(i, j, prev).e == key) {
        k = prev;
        break;
      }
    if (k == 0xffffffffu) return false;  // FIX (bug), as in strip_model: the exe reads the word before the list
    *e1 = walk_of(i, k, (k + 1) % n(i));
    const uint32_t o = k + ((poly(i).flags >> 1) & 0x7f);
    *e2 = walk_of(i, o % n(i), (o + 1) % n(i));
    return true;
  };
  auto long_side = [&](uint32_t i) {
    return strip4(i) && (walk_of(i, 2, 0).e == key || walk_of(i, 3, 1).e == key);
  };
  Dir a1, a2, b1, b2;
  const bool flat_a = !strip(a.poly), flat_b = !strip(b.poly);
  if (!ends(a.poly, &a1, &a2) || !ends(b.poly, &b1, &b2)) return false;
  const bool side_a = long_side(a.poly), side_b = long_side(b.poly);
  if (side_a && side_b) return false;
  if (side_a) a1 = walk_of(a.poly, 0, 2), a2 = walk_of(a.poly, 1, 3);
  if (side_b) b1 = walk_of(b.poly, 0, 2), b2 = walk_of(b.poly, 1, 3);

  if (key == a2.e && key == b1.e) {
    if (!side_a) return a2 == b1 || flat_b;
    if (a2 == b1) return even_strip(b.poly);
    return odd(b.poly) && flat_b;
  }
  if (key == b2.e && key == a1.e) {
    if (!side_b) return b2 == a1 || flat_a;
    if (b2 == a1) return even_strip(a.poly);
    return odd(a.poly) && flat_a;
  }
  if (key == a2.e && key == b2.e) {
    if (!(a2 == b2)) return true;
    if (!side_b && flat_a) return true;
    return !side_a && flat_b;
  }
  if (!(key == a1.e) || !(key == b1.e)) return false;
  if (a1 == b1) {
    if (!side_a && even_strip(a.poly) && flat_b) return true;
    if (!side_b && even_strip(b.poly) && flat_a) return true;
    if (!side_a && odd(a.poly) && flat_a) return true;
    return !side_b && odd(b.poly) && flat_b;
  }
  if (!side_a && even_strip(a.poly)) return true;
  if (!side_b && even_strip(b.poly)) return true;
  return !side_a && flat_a && flat_b;
}

// Mech3.exe 1.2 @ 0x00550010: two UV pairs equal up to a whole number of repeats; the offset in *off.
bool uv_match(const float* a, const float* b, float* off) {
  for (int c = 0; c < 2; ++c) {
    const double d = static_cast<double>(a[c]) - static_cast<double>(b[c]);
    off[c] = static_cast<float>(static_cast<int32_t>(d >= 0.0 ? d + 0.5 : d - 0.5));
    if (!(std::fabs(d - static_cast<double>(off[c])) < static_cast<double>(0.001f))) return false;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x00554460: same normal, UVs a whole number of repeats apart, same colour.
bool rec_match(const Rec& a, const Rec& b, float* off) {
  if (a.n != b.n || !uv_match(a.uv, b.uv, off)) return false;
  return fp_eq(a.col[0], b.col[0]) && fp_eq(a.col[1], b.col[1]) && fp_eq(a.col[2], b.col[2]);
}

// Mech3.exe 1.2 @ 0x00554150: whether b joins a along the edge; on success b's UVs are moved by the whole
// repeats that line them up with a's.
bool Strip::join_test(const Item& a, const Item& b, const Edge& key) {
  if (a.dir == b.dir || !ends_meet(a, b, key)) return false;
  const Polygon& pa = poly(a.poly);
  const Polygon& pb = poly(b.poly);
  const uint32_t total = (pa.flags & 0xff) - 2 + (pb.flags & 0xff);
  if (total > 64 || total > limit || pa.material != pb.material || (pa.flags & 0x100) || (pb.flags & 0x100) ||
      pa.priority != pb.priority || pa.zone_set != pb.zone_set)  // 0x00550120: the zones' 4 bytes
    return false;
  auto at = [&](uint32_t i, uint32_t vert) {
    uint32_t k = 0;
    while (k < n(i) && v(i, k) != vert) ++k;
    return k;
  };
  const uint32_t a0 = at(a.poly, key.a), a1 = at(a.poly, key.b);
  const uint32_t b0 = at(b.poly, key.a), b1 = at(b.poly, key.b);
  if (a0 == n(a.poly) || a1 == n(a.poly) || b0 == n(b.poly) || b1 == n(b.poly)) return false;  // FIX (bug), as above
  float off1[2], off2[2];
  if (!rec_match(rec(a.poly, a0), rec(b.poly, b0), off1)) return false;
  if (!rec_match(rec(a.poly, a1), rec(b.poly, b1), off2)) return false;
  if (!fp_eq(off1[0], off2[0]) || !fp_eq(off1[1], off2[1])) return false;
  Polygon& q = poly(b.poly);  // 0x005544d0
  for (uint32_t k = 0; k < (q.flags & 0xff); ++k) q.uvs[2 * k] = off1[0] + q.uvs[2 * k], q.uvs[2 * k + 1] += off1[1];
  return true;
}

// The first part of 0x00551190: a polygon that is not a strip is laid out as one in place, starting from
// the shared edge and zigzagging round (k, k+1, k-1, k+2, ...).
void Strip::to_strip(uint32_t i, const Edge& key) {
  Rec order[0x40];
  const uint32_t cnt = n(i);
  uint32_t k = 0;
  for (; k < cnt; ++k)
    if (walk_of(i, k, (k + 1) % cnt).e == key) break;
  uint32_t w = 0;
  order[w++] = rec(i, k);
  uint32_t p = (k + 1) % cnt, q = (k + cnt - 1) % cnt;
  order[w++] = rec(i, p);
  while (p != q) {
    order[w++] = rec(i, q);
    p = (p + 1) % cnt;
    if (q == p) break;
    q = (q + cnt - 1) % cnt;
    order[w++] = rec(i, p);
  }
  for (uint32_t j = 0; j < cnt; ++j) set_rec(i, j, order[j]);
  poly(i).flags |= 0x400;
}

// Mech3.exe 1.2 @ 0x00550160: a new strip at the end of the model, x's vertices then y's past its first two.
uint32_t Strip::concat(uint32_t x, uint32_t y) {
  resize(count() + 1);
  const uint32_t s = count() - 1;
  Polygon& d = poly(s);
  d = Polygon{};
  const Polygon& px = poly(x);
  const Polygon& py = poly(y);
  const uint32_t nx = px.flags & 0xff, ny = py.flags & 0xff;
  d.flags = ((nx - 2 + ny) & 0xff) | (px.flags & 0x300) | 0x400;
  d.priority = px.priority, d.zone_set = px.zone_set, d.material = px.material;
  d.material_index = px.material_index;
  const uint32_t k = d.flags & 0xff;
  d.vertex_indices = static_cast<uint32_t*>(std::malloc(4 * k));
  d.normal_indices = static_cast<uint32_t*>(std::malloc(4 * k));
  d.uvs = static_cast<float*>(std::malloc(8 * k));
  d.colors = static_cast<float*>(std::malloc(12 * k));
  d.scratch = static_cast<uint32_t*>(std::malloc(4 * k));
  for (uint32_t j = 0; j < nx; ++j) set_rec(s, j, rec(x, j));
  for (uint32_t j = 2; j < ny; ++j) set_rec(s, nx - 2 + j, rec(y, j));
  return s;
}

// Mech3.exe 1.2 @ 0x00551190: a and b laid out as strips that carry on into each other at the edge, then
// joined (read from the asm: the decompile loses which polygon each reverse and pair swap acts on).
uint32_t Strip::join(const Item& ia, const Item& ib, const Edge& key) {
  const uint32_t a = ia.poly, b = ib.poly;
  const bool flat_a = !strip(a), flat_b = !strip(b);
  if (flat_a) to_strip(a, key);
  if (flat_b) to_strip(b, key);
  auto long_side = [&](uint32_t i) {
    return strip4(i) && (walk_of(i, 2, 0).e == key || walk_of(i, 3, 1).e == key);
  };
  const bool side_a = long_side(a), side_b = long_side(b);
  if (side_a) swap(a, 1, 2);
  if (side_b) swap(b, 1, 2);

  if (key == last(a).e && key == first(b).e) {
    if (!side_a) {
      if (!(last(a) == first(b)) && flat_b) pair_swap(b);
      return concat(a, b);
    }
    if (last(a) == first(b)) {
      if (!even_strip(b)) return concat(a, b);
    } else {
      if (!odd(b) || !flat_b) return concat(a, b);
      pair_swap(b);
    }
    reverse(b), reverse(a);
    return concat(b, a);
  }
  if (key == last(b).e && key == first(a).e) {
    if (!side_b) {
      if (first(a) == last(b)) return concat(b, a);
      if (!flat_a) return concat(a, b);
      pair_swap(a);
      return concat(b, a);
    }
    if (last(b) == first(a)) {
      if (even_strip(a)) reverse(a), reverse(b);
    } else if (odd(a) && flat_a) {
      pair_swap(a), reverse(a), reverse(b);
    }
    return concat(a, b);
  }
  if (key == last(a).e && key == last(b).e) {
    if (last(b) == last(a)) {
      if (!side_b && flat_a) {
        reverse(a), pair_swap(a);
        return concat(b, a);
      }
      if (!side_a && flat_b) reverse(b), pair_swap(b);
      return concat(a, b);
    }
    if (!side_a) {
      reverse(b);
      return concat(a, b);
    }
    reverse(a);
    return concat(b, a);
  }
  if (!(key == first(a).e) || !(key == first(b).e)) return concat(a, b);
  if (first(b) == first(a)) {
    if (!side_a && even_strip(a) && flat_b) {
      reverse(a), pair_swap(b);
      return concat(a, b);
    }
    if (!side_b && even_strip(b) && flat_a) {
      reverse(b), pair_swap(a);
      return concat(b, a);
    }
    if (!side_a && odd(a) && flat_a) {
      pair_swap(a), reverse(a);
      return concat(a, b);
    }
    if (!side_b && odd(b) && flat_b) {
      pair_swap(b), reverse(b);
      return concat(b, a);
    }
    return concat(a, b);
  }
  if (!side_a && even_strip(a)) {
    reverse(a);
    return concat(a, b);
  }
  if (!side_b && even_strip(b)) {
    reverse(b);
    return concat(b, a);
  }
  if (!side_a && flat_a && flat_b) pair_swap(a), reverse(a), pair_swap(b);
  return concat(a, b);
}

// The polygon's arrays freed and the last polygon moved into its place (in 0x00552540).
void Strip::remove(uint32_t i) {
  Polygon& p = poly(i);
  std::free(p.vertex_indices), std::free(p.colors);
  if (p.scratch) std::free(p.scratch);
  if (p.uvs) std::free(p.uvs);
  if (p.flags & 0x200) std::free(p.normal_indices);
  const uint32_t last = count() - 1;
  if (i != last) poly(i) = poly(last);
  resize(last);
}

}  // namespace

// Mech3.exe 1.2 @ 0x00553450 (the read)
// Mech3.exe 1.2 @ 0x00553460 (the set)
bool& strips() {
  static bool on = false;
  return on;
}

// Mech3.exe 1.2 @ 0x00552540
void strip_model(Model& m, uint32_t limit, uint32_t small) {
  if (m.header[0] | m.header[1] | m.header[2] | m.header[3]) return;  // +0x00 nonzero: left alone
  Strip s{m, limit};
  std::map<Edge, std::unique_ptr<Info>> edges;
  for (uint32_t i = 0; i < s.count(); ++i) {
    const Polygon& p = s.poly(i);
    const uint32_t f = p.flags;
    if (!(p.material->flags & 1) || !(f & 0x200) || (!(f & 0x400) && (f & 0xff) > small)) continue;
    for (uint32_t k = 0; k < (s.poly(i).flags & 0xff); ++k) {
      const Edge key = walk(s.v(i, k), s.v(i, (k + 1) % s.n(i))).e;
      auto& slot = edges[key];
      if (!slot) slot = std::make_unique<Info>(Info{key, {}});
      bool dir;
      if (s.edge_of(i, key, &dir)) slot->list.push_back({i, dir});
    }
  }
  auto add_edge = [&](uint32_t i, const Edge& key) {
    auto it = edges.find(key);
    bool dir;
    if (it != edges.end() && s.edge_of(i, key, &dir)) it->second->list.push_back({i, dir});
  };
  bool joined;
  do {
    joined = false;
    for (auto it = edges.begin(); it != edges.end(); ++it) {
      Info& e = *it->second;
    rescan:
      for (auto i = e.list.begin(); i != e.list.end(); ++i)
        for (auto j = std::next(i); j != e.list.end(); ++j) {
          // FIX (bug): a polygon on a list twice keeps a copy that follows no removal and no move (only the
          // first is dropped or renumbered), so an entry can name a polygon past the array, or one without the
          // edge; the exe reads past its array, or the word before a vertex list. Such a pair does not join.
          if (i->poly >= s.count() || j->poly >= s.count()) continue;
          if (!s.join_test(*i, *j, e.key)) continue;
          const Item a = *i, b = *j;
          joined = true;
          s.join(a, b, e.key);
          // The lists lose a and b and follow the polygon that moves into b's place.
          const uint32_t moved = s.count() - 2;
          for (auto& [k, info] : edges) {
            auto& l = info->list;
            for (auto x = l.begin(); x != l.end(); ++x)
              if (x->poly == a.poly) {
                l.erase(x);
                break;
              }
            for (auto x = l.begin(); x != l.end(); ++x)
              if (x->poly == b.poly) {
                l.erase(x);
                break;
              }
            for (auto& x : l)
              if (x.poly == moved) {
                x.poly = b.poly;
                break;
              }
          }
          const uint32_t joined_at = a.poly == moved ? b.poly : a.poly;
          s.remove(a.poly);
          s.remove(b.poly);
          if (s.strip4(joined_at)) {
            add_edge(joined_at, s.walk_of(joined_at, 0, 2).e);
            add_edge(joined_at, s.walk_of(joined_at, 3, 1).e);
          }
          add_edge(joined_at, s.first(joined_at).e);
          add_edge(joined_at, s.last(joined_at).e);
          goto rescan;
        }
    }
  } while (joined);
}

}  // namespace zmod
