// Probes for test_landmines.py (docs/landmines.md): the load 0x004135c0 over a
// zReader tree the test lays out, and the check 0x00413560 with the
// detonation 0x00413470 over a list it lays out. The engine calls go to the
// shared callback; handles are the emulator's addresses.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_di.h"
#include "gamez/zreader/zreader.h"
#include "mech3/landmines.h"
#include "probe_app.h"

namespace {
uint32_t fb(float f) {
  uint32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bf(uint32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
void* h(uint32_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); }

// The tree, preorder: Int [1 v], Float [2 bits], String [3 offset into strs],
// List [4 n] then its n items.
const char* g_strs;
zrdr::Node* g_root;
void build(const int32_t*& p, zrdr::Node* out) {
  out->type = *p++;
  switch (out->type) {
    case zrdr::Int: out->i = *p++; break;
    case zrdr::Float: out->f = bf(static_cast<uint32_t>(*p++)); break;
    case zrdr::String: out->s = const_cast<char*>(g_strs + *p++); break;
    default: {
      const int32_t n = *p++;
      out->list = zrdr::alloc_list(n + 1);
      out->list[0].type = zrdr::Int, out->list[0].i = n + 1;
      for (int32_t i = 1; i <= n; ++i) build(p, &out->list[i]);
    }
  }
}

zwep::WeaponDef g_def;
uint32_t g_def_handle;
uint32_t def_of(const zwep::WeaponDef* d) { return d == &g_def ? g_def_handle : u(d); }
// The ground answers, five words a call: found (0) or not, then the hit's
// normal x, height, zone[0], material; the rest from them.
std::vector<uint32_t> g_ground;
size_t g_ground_at;

void hooks() {
  mech3::LandmineHooks& k = mech3::landmine_hooks();
  k = mech3::LandmineHooks{};
  k.load = [](const char* file) {
    probe_app::call("0x00556740", {file});
    return g_root;
  };
  k.free = [](zrdr::Node*) { probe_app::call("0x00556820", {}); };
  k.find_def = [](const char* name) { return static_cast<zwep::WeaponDef*>(h(probe_app::call("0x00581ed0", {name}))); };
  k.ground = [](float x, float y, float z, zcls::GroundHits* out) -> int32_t {
    probe_app::call("0x004edab0", {fb(x), fb(y), fb(z)});
    const uint32_t* w = &g_ground[g_ground_at];
    g_ground_at += 5;
    zcls::GroundHit& g = out->hit[0];
    g.normal[0] = bf(w[1]), g.normal[1] = bf(w[1] ^ 0x80000000u), g.normal[2] = bf(w[1] + 1);
    g.x = 0, g.height = bf(w[2]), g.z = 0;
    g.zone[0] = w[3], g.zone[1] = w[3] * 3;
    g.material = static_cast<zmod::Material*>(h(w[4]));
    g.node = static_cast<zcls::Node*>(h(w[4] + 0x40));
    return static_cast<int32_t>(w[0]);
  };
  k.impact = [](zwep::WeaponDef* d, zwep::HitRecord* r, zwep::Projectile* p, zwep::Handle a) -> uint32_t {
    uint32_t dist;
    std::memcpy(&dist, &r->distance_sq, 4);
    return probe_app::call("0x005826b0", {def_of(d), fb(r->normal.x), fb(r->normal.y), fb(r->normal.z), fb(r->point.x),
                                          fb(r->point.y), fb(r->point.z), r->x18, dist, u(r->material), u(r->node),
                                          fb(p->spawn_at.x), fb(p->spawn_at.y), fb(p->spawn_at.z), fb(p->position.x),
                                          fb(p->position.y), fb(p->position.z), fb(p->damage_scale), u(a)});
  };
  k.splash_query = [](zwep::WeaponDef* d, zwep::Projectile* p, int32_t flag, zwep::HitList*) -> int32_t {
    return static_cast<int32_t>(probe_app::call(
        "0x00582920", {def_of(d), fb(p->position.x), fb(p->position.y), fb(p->position.z), fb(p->damage_scale), flag}));
  };
  k.splash = [](zwep::WeaponDef* d, zwep::Projectile*, zwep::HitList*, uintptr_t ex) -> int32_t {
    return static_cast<int32_t>(probe_app::call("0x005829b0", {def_of(d), static_cast<uint32_t>(ex)}));
  };
}

void set_list(const uint32_t* mines, int32_t n) {
  mech3::Landmines& L = mech3::landmines();
  L.list.clear();
  for (int32_t i = 0; i < n; ++i) {
    mech3::Landmine m;
    m.armed = static_cast<uint8_t>(mines[4 * i]);
    for (int k = 0; k < 3; ++k) m.at[k] = bf(mines[4 * i + 1 + k]);
    L.list.push_back(m);
  }
}

// The list back as the exe's 16-byte records, then the globals.
int32_t get_list(uint32_t* out, int32_t cap) {
  const mech3::Landmines& L = mech3::landmines();
  int32_t n = 0;
  for (const mech3::Landmine& m : L.list) {
    if (n >= cap) break;
    out[4 * n] = m.armed;
    for (int k = 0; k < 3; ++k) out[4 * n + 1 + k] = fb(m.at[k]);
    ++n;
  }
  return static_cast<int32_t>(L.list.size());
}
}  // namespace

extern "C" {

// The load: `tree` (or no file when tree_len is 0), the list before it.
// out: the answer, then the weapon, radius, radius squared; then the list.
int32_t probe_mines_load(const int32_t* tree, int32_t tree_len, const char* strs, const char* file, const char* block,
                         const uint32_t* mines, int32_t n, uint32_t* globals, uint32_t* out, int32_t cap) {
  hooks();
  g_strs = strs;
  static zrdr::Node root;
  g_root = nullptr;
  if (tree_len) {
    const int32_t* p = tree;
    build(p, &root);
    g_root = &root;
  }
  mech3::Landmines& L = mech3::landmines();
  set_list(mines, n);
  L.weapon = static_cast<zwep::WeaponDef*>(h(globals[0]));
  L.radius = bf(globals[1]), L.radius_sq = bf(globals[2]);
  const bool r = mech3::landmines_load(file, block);
  globals[0] = u(L.weapon), globals[1] = fb(L.radius), globals[2] = fb(L.radius_sq);
  const int32_t count = get_list(out, cap);
  return (r ? 1 : 0) | count << 1;
}

// The check at `at` over `mines`, the def `def` (its IMPACT_PROXIMITY) and the
// ground's answers. globals: radius squared in; out: the list after, then the
// shot's +0x24, +0x48 (3 each) and +0x5bc.
int32_t probe_mines_check(const uint32_t* mines, int32_t n, uint32_t radius_sq, uint32_t def, uint32_t proximity,
                          const uint32_t* ground, int32_t ground_n, const uint32_t* at, uint32_t* out, uint32_t* shot) {
  hooks();
  g_def = zwep::WeaponDef{};
  g_def.impact_proximity = bf(proximity);
  g_def_handle = def;
  g_ground.assign(ground, ground + 5 * ground_n);
  g_ground_at = 0;
  mech3::Landmines& L = mech3::landmines();
  set_list(mines, n);
  L.weapon = &g_def;
  L.radius_sq = bf(radius_sq);
  L.shot = zwep::Projectile{};
  L.shot.damage_scale = 0.0f;  // the exe's static starts zero
  float p[3] = {bf(at[0]), bf(at[1]), bf(at[2])};
  const bool r = mech3::landmines_check(p);
  get_list(out, n);
  const zwep::Projectile& s = L.shot;
  const float f[7] = {s.spawn_at.x, s.spawn_at.y, s.spawn_at.z, s.position.x, s.position.y, s.position.z, s.damage_scale};
  for (int i = 0; i < 7; ++i) shot[i] = fb(f[i]);
  return r ? 1 : 0;
}

}  // extern "C"
