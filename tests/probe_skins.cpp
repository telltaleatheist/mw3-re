// The skins on a model tree (mech_skin.cpp's 0x004748b0, 0x004749e0,
// 0x00475520, 0x00475780, 0x00475870, 0x004757f0, 0x00475a80 and their
// leaves) for test_skins.py: one scenario (the layout in the test) built as
// the port's objects, one operation, then the state and the texture calls as
// ints. Textures are tagged: 0 none, 1 the default entry, 100 + i the table's
// entry i, 200 + i a newly loaded one, 300 + i a cached image.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gamez/zclass/cls_node.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "mech3/mech_skin.h"

namespace {
using mech3::Skin;
using mech3::SkinEntry;
using mech3::SkinSet;

struct World {
  int32_t holes = 0, op = 0, arg = 0, nt = 0, ns = 0, ne = 0, has_damage = 0, nm = 0, nn = 0;
  std::vector<std::string> names;
  std::vector<int32_t> in_table, img_w, img_h, cache_w, cache_h;
  std::vector<std::unique_ptr<zimg::TextureEntry>> table, loaded;
  std::vector<std::unique_ptr<zimg::Texture>> images;
  std::vector<std::pair<const void*, int32_t>> cached;  // a cached image and its texture
  zimg::TextureEntry deflt{};
  std::vector<zmod::Material> mats;
  std::vector<std::vector<zmod::Polygon>> polys;
  std::vector<zmod::Model> models;
  std::vector<zcls::Node> nodes;
  std::vector<std::vector<zcls::Node*>> kids;
  SkinSet* set = nullptr;
  std::vector<Skin*> skins;  // the set's, then the damage skin
  std::vector<SkinEntry*> originals;
  void* slot = nullptr;
  Skin* made = nullptr;
  std::vector<int32_t> log;
};
World* g = nullptr;

int32_t name_index(const char* n) {
  for (int32_t i = 0; i < g->nt; ++i)
    if (g->names[i] == n) return i;
  return -1;
}
zimg::Texture* image(int32_t w, int32_t h) {
  auto t = std::make_unique<zimg::Texture>();
  t->width = static_cast<int16_t>(w), t->height = static_cast<int16_t>(h);
  g->images.push_back(std::move(t));
  return g->images.back().get();
}
int32_t tag(const void* p) {
  if (!p) return 0;
  if (p == &g->deflt) return 1;
  for (int32_t i = 0; i < g->nt; ++i)
    if (p == g->table[i].get()) return 100 + i;
  for (const auto& e : g->loaded)
    if (p == e.get()) return 200 + name_index(e->name);
  for (const auto& [img, k] : g->cached)
    if (p == img) return 300 + k;
  return -1;
}
void* fetch(const char* n) {
  const int32_t i = name_index(n);
  g->log.insert(g->log.end(), {3, i});
  if (i < 0 || !g->cache_w[i]) return nullptr;
  zimg::Texture* t = image(g->cache_w[i], g->cache_h[i]);
  g->cached.push_back({t, i});
  return t;
}
void* find(const char* n) {
  const int32_t i = name_index(n);
  g->log.insert(g->log.end(), {1, i});
  return i >= 0 && g->in_table[i] ? g->table[i].get() : nullptr;
}
void* load(const char* n, int32_t group, int32_t free_pixels) {
  const int32_t i = name_index(n);
  g->log.insert(g->log.end(), {2, i, group, free_pixels});
  if (i < 0 || !g->img_w[i]) return &g->deflt;
  auto e = std::make_unique<zimg::TextureEntry>();
  std::snprintf(e->name, sizeof e->name, "%s", n);
  e->image = image(g->img_w[i], g->img_h[i]);
  g->loaded.push_back(std::move(e));
  return g->loaded.back().get();
}
void release(void* t) { g->log.insert(g->log.end(), {6, tag(t)}); }

void entry_out(const SkinEntry* e, std::vector<int32_t>& o) {
  int32_t dmg = -1;
  for (size_t k = 0; k < g->originals.size(); ++k)
    if (e->damage == g->originals[k]) dmg = static_cast<int32_t>(k);
  o.insert(o.end(), {tag(e->texture), e->cached, dmg, e->b14, e->refs});
}
}  // namespace

extern "C" {
// Returns the output's length: the materials (flags, texture, soil), every
// original entry, the skins' loaded flags, then the made skin's entries
// (-1 - k for original entry k shared, else a copy's fields), then the log.
int32_t probe_skin_apply(const int32_t* in, int32_t* out, int32_t cap) {
  World w;
  g = &w;
  int32_t k = 0;
  w.holes = in[k++], w.op = in[k++], w.arg = in[k++], w.nt = in[k++], w.ns = in[k++], w.ne = in[k++];
  w.has_damage = in[k++], w.nm = in[k++], w.nn = in[k++];
  const int32_t vult = in[k++];
  for (int32_t i = 0; i < w.nt; ++i) {
    char n[20];
    if (i == 0 && vult) std::snprintf(n, sizeof n, "vult_d03_c");
    else std::snprintf(n, sizeof n, "tex%02d", i);
    w.names.push_back(n);
    w.in_table.push_back(in[k++]), w.img_w.push_back(in[k++]), w.img_h.push_back(in[k++]);
    w.cache_w.push_back(in[k++]), w.cache_h.push_back(in[k++]);
  }
  // the table's entries (every texture has one; `in_table` says whether 0x00518220 finds it)
  for (int32_t i = 0; i < w.nt; ++i) {
    auto e = std::make_unique<zimg::TextureEntry>();
    std::snprintf(e->name, sizeof e->name, "%s", w.names[i].c_str());
    e->image = w.img_w[i] ? image(w.img_w[i], w.img_h[i]) : nullptr;
    w.table.push_back(std::move(e));
  }
  // pad the image list so cached images come after the table's (tag())
  while (static_cast<int32_t>(w.images.size()) < w.nt) image(0, 0);
  w.set = new SkinSet(w.ns);  // before HOLES: its hole textures are not this test's
  mech3::SkinGlobals& sg = mech3::skin_globals();
  const mech3::SkinGlobals saved = sg;
  sg.damage_mode = static_cast<uint8_t>(w.holes);
  sg.fetch_texture = fetch, sg.find_texture = find, sg.load_named = load;
  sg.soil_index = [](const char*) { return 7; };
  sg.release_cached = release, sg.release_texture = release;
  // skins
  const int32_t nskins = w.ns + (w.has_damage ? 1 : 0);
  for (int32_t s = 0; s < nskins; ++s) {
    char n[16];
    std::snprintf(n, sizeof n, s < w.ns ? "skin%d" : "damage", s);
    auto* sk = new Skin(n, static_cast<uint32_t>(w.ne));
    for (int32_t j = 0; j < w.ne; ++j) {
      const int32_t ni = in[k++], ai = in[k++];
      sk->add_entry(w.names[ni].c_str(), ai < 0 ? nullptr : w.names[ai].c_str());
      w.originals.push_back(sk->entries.back());
    }
    if (s < w.ns) w.set->skins.push_back(sk);
    else w.set->damage = sk;
    w.skins.push_back(sk);
  }
  // materials, polygons, models, nodes
  w.mats.resize(w.nm);
  for (int32_t i = 0; i < w.nm; ++i) {
    w.mats[i] = zmod::Material{};
    w.mats[i].flags = static_cast<uint8_t>(in[k++]);
    const int32_t t = in[k++];
    w.mats[i].texture = t >= 0 ? w.table[t].get() : nullptr;
  }
  w.polys.resize(w.nn), w.models.resize(w.nn), w.nodes.resize(w.nn), w.kids.resize(w.nn);
  std::vector<int32_t> parent(w.nn, -1);
  for (int32_t n = 0; n < w.nn; ++n) {
    parent[n] = in[k++];
    const int32_t np = in[k++];
    for (int32_t j = 0; j < np; ++j) {
      zmod::Polygon p{};
      const int32_t m = in[k++];
      p.material = m >= 0 ? &w.mats[m] : nullptr;
      w.polys[n].push_back(p);
    }
  }
  for (int32_t n = 0; n < w.nn; ++n) {
    zmod::Model& md = w.models[n];
    md = zmod::Model{};
    const uint32_t cnt = static_cast<uint32_t>(w.polys[n].size());
    std::memcpy(md.header + 0x10, &cnt, 4);
    md.polygons = w.polys[n].data();
    w.nodes[n] = zcls::Node{};
    w.nodes[n].model = cnt ? &md : nullptr;
    if (parent[n] >= 0) w.kids[parent[n]].push_back(&w.nodes[n]);
  }
  for (int32_t n = 0; n < w.nn; ++n) {
    w.nodes[n].put<int32_t>(0x5c, static_cast<int32_t>(w.kids[n].size()));
    w.nodes[n].children = w.kids[n].data();
  }
  zcls::Node* root = &w.nodes[0];
  zcls::Node* lod = w.nn > 1 ? &w.nodes[1] : nullptr;
  int32_t ret = 0;
  if (w.op == 0) {
    w.set->apply_first(root);
  } else if (w.op == 1) {
    w.made = w.set->mech_skin(w.arg, root);
  } else {
    w.slot = w.set->mech_skin(0, root);
    Skin* to = w.arg >= 0 && w.arg < w.ns ? w.set->skins[w.arg] : nullptr;
    ret = w.set->set_mech_skin(&w.slot, root, lod, to) ? 1 : 0;
    w.made = static_cast<Skin*>(w.slot);
  }
  std::vector<int32_t> o;
  o.push_back(ret);
  for (const zmod::Material& m : w.mats) o.insert(o.end(), {m.flags, tag(m.texture), static_cast<int32_t>(m.soil)});
  for (SkinEntry* e : w.originals) entry_out(e, o);
  for (Skin* s : w.skins) o.push_back(s->loaded);
  o.push_back(w.made ? static_cast<int32_t>(w.made->entries.size()) : -1);
  if (w.made)
    for (SkinEntry* e : w.made->entries) {
      int32_t shared = 0;
      for (size_t q = 0; q < w.originals.size(); ++q)
        if (e == w.originals[q]) shared = -1 - static_cast<int32_t>(q);
      o.push_back(shared);
      if (!shared) {
        o.push_back(name_index(e->name));
        o.push_back(e->alt == e->name ? -1 : name_index(e->alt));
        entry_out(e, o);
      }
    }
  o.push_back(-999);
  o.insert(o.end(), w.log.begin(), w.log.end());
  sg.damage_mode = saved.damage_mode;
  sg.fetch_texture = saved.fetch_texture, sg.find_texture = saved.find_texture, sg.load_named = saved.load_named;
  sg.soil_index = saved.soil_index, sg.release_cached = saved.release_cached, sg.release_texture = saved.release_texture;
  g = nullptr;
  if (static_cast<int32_t>(o.size()) > cap) return -1;
  std::memcpy(out, o.data(), o.size() * 4);
  return static_cast<int32_t>(o.size());
}
}
