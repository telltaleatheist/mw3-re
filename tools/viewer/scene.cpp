#include "scene.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"

namespace viewer {
namespace {

constexpr int kObject3d = 5;

// Object3d data: a 3x3 matrix at +0x30 (rows) and the translation at +0x54.
struct Xform {
  float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  float t[3] = {0, 0, 0};
};

Xform local_xform(const zcls::Node* n) {
  Xform x;
  if (n->cls() != kObject3d || !n->data) return x;
  const auto* d = static_cast<const uint8_t*>(n->data);
  std::memcpy(x.m, d + 0x30, 36);
  std::memcpy(x.t, d + 0x54, 12);
  return x;
}

// Row vectors: p' = p * M + t, so child then parent is a.m * b.m.
Xform combine(const Xform& a, const Xform& b) {
  Xform r;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      r.m[3 * i + j] = a.m[3 * i] * b.m[j] + a.m[3 * i + 1] * b.m[3 + j] + a.m[3 * i + 2] * b.m[6 + j];
  for (int j = 0; j < 3; ++j) r.t[j] = a.t[0] * b.m[j] + a.t[1] * b.m[3 + j] + a.t[2] * b.m[6 + j] + b.t[j];
  return r;
}

void apply(const Xform& x, const float* p, float* out) {
  for (int j = 0; j < 3; ++j) out[j] = p[0] * x.m[j] + p[1] * x.m[3 + j] + p[2] * x.m[6 + j] + x.t[j];
}

Xform world_xform(const zcls::Node* n, int depth = 0) {
  Xform x = local_xform(n);
  if (depth < 64 && n->parent_count() > 0 && n->parents && n->parents[0])
    return combine(x, world_xform(n->parents[0], depth + 1));
  return x;
}

float c565(uint16_t v, int shift, int bits) { return static_cast<float>((v >> shift) & ((1 << bits) - 1)) / ((1 << bits) - 1); }

bool decode(const zimg::Texture& t, Image& img) {
  img.width = t.width, img.height = t.height;
  const int n = img.width * img.height;
  if (n <= 0 || !t.pixels) return false;
  img.rgba.resize(static_cast<size_t>(n) * 4);
  const bool pal = t.palette_count != 0 && t.palette;
  for (int i = 0; i < n; ++i) {
    uint16_t c = pal ? t.palette[static_cast<const uint8_t*>(t.pixels)[i]] : static_cast<const uint16_t*>(t.pixels)[i];
    uint8_t* o = &img.rgba[static_cast<size_t>(i) * 4];
    o[0] = static_cast<uint8_t>(c565(c, 11, 5) * 255), o[1] = static_cast<uint8_t>(c565(c, 5, 6) * 255);
    o[2] = static_cast<uint8_t>(c565(c, 0, 5) * 255);
    uint8_t a = 255;
    if (t.alpha) a = t.alpha[i];
    else if ((t.flags & zimg::kAlpha) && c == 0) a = 0;  // colour key, a guess (docs/textures.md)
    o[3] = a;
    if (a < 255) img.alpha = true;
  }
  return true;
}

}  // namespace

std::string load_world(const std::string& zbd_dir, const std::string& mission, Scene& out) {
  static std::string dir;
  dir = zbd_dir + "/";
  const std::string path = dir + mission + "/gamez.zbd";
  if (zcls::read_zbd(path.c_str()) != 0) return "could not read " + path;

  // The world's own textures, then the mech skins and the 2D images.
  zimg::state().dir = dir.c_str();
  std::vector<zimg::Library*> libs;
  for (const std::string& name : {mission + "/rtexture", std::string("rmechtex"), std::string("rimage")}) {
    zimg::Library* lib = zimg::new_library(name.c_str(), 0, false);
    if (zimg::open(*lib)) libs.push_back(lib);
  }
  if (libs.empty()) return "no texture library under " + dir;
  auto load = [&](const char* name) -> zimg::Texture* {
    for (zimg::Library* lib : libs)
      if (zimg::Texture* t = zimg::load(*lib, name)) return t;
    return nullptr;
  };

  std::map<const zimg::TextureEntry*, int> image_of;
  std::map<int, int> batch_of;
  auto image_index = [&](const zimg::TextureEntry* e) -> int {
    if (!e) return -1;
    auto it = image_of.find(e);
    if (it != image_of.end()) return it->second;
    int idx = -1;
    if (zimg::Texture* t = load(e->name)) {
      Image img;
      if (decode(*t, img)) {
        idx = static_cast<int>(out.images.size());
        out.images.push_back(std::move(img));
      }
    }
    if (idx < 0 && out.missing_textures++ < 5) std::fprintf(stderr, "texture %s: not loaded\n", e->name);
    image_of[e] = idx;
    return idx;
  };

  bool first = true;
  const int total = zcls::node_total();
  for (int i = 0; i < total; ++i) {
    zcls::Node* n = zcls::node_at(i);
    if (!n || !(n->link & zcls::kNodeLive)) continue;
    ++out.nodes;
    if (n->cls() != kObject3d || !n->model) continue;
    ++out.objects;
    const zmod::Model& m = *n->model;
    const Xform x = world_xform(n);
    const uint32_t vcount = m.count(0x14), pcount = m.count(0x10);
    for (uint32_t p = 0; p < pcount && m.polygons; ++p) {
      const zmod::Polygon& poly = m.polygons[p];
      const uint32_t k = poly.flags & 0xff;
      if (k < 3 || !poly.vertex_indices) continue;
      ++out.polygons;
      const zmod::Material* mat = poly.material;
      const bool textured = mat && (mat->flags & 1) && poly.uvs;
      const int img = textured ? image_index(mat->texture) : -1;
      auto [it, added] = batch_of.try_emplace(img, static_cast<int>(out.batches.size()));
      if (added) out.batches.push_back(Batch{img, {}});
      Batch& b = out.batches[it->second];
      auto vertex = [&](uint32_t j) {
        Vertex v{};
        const uint32_t vi = poly.vertex_indices[j];
        if (vi < vcount) apply(x, m.vertices + 3 * vi, v.pos);
        if (textured) v.uv[0] = poly.uvs[2 * j], v.uv[1] = poly.uvs[2 * j + 1];
        // Vertex colours are 0..255; untextured polygons' already carry the material's.
        float r = 1, g = 1, bl = 1;
        if (poly.colors) r = poly.colors[3 * j] / 255, g = poly.colors[3 * j + 1] / 255, bl = poly.colors[3 * j + 2] / 255;
        else if (mat) r = mat->r / 255, g = mat->g / 255, bl = mat->b / 255;
        v.color[0] = r, v.color[1] = g, v.color[2] = bl, v.color[3] = 1;
        for (int a = 0; a < 3; ++a) {
          out.min[a] = first ? v.pos[a] : std::min(out.min[a], v.pos[a]);
          out.max[a] = first ? v.pos[a] : std::max(out.max[a], v.pos[a]);
        }
        first = false;
        b.vertices.push_back(v);
      };
      for (uint32_t j = 1; j + 1 < k; ++j) {  // a fan, a guess until the render path is mapped
        vertex(0), vertex(j), vertex(j + 1);
        ++out.triangles;
      }
    }
  }
  if (std::getenv("MW3VIEW_STATS")) {
    float cmin = 1e9f, cmax = -1e9f, umin = 1e9f, umax = -1e9f;
    size_t textured = 0, flat = 0;
    for (const Batch& b : out.batches)
      for (const Vertex& v : b.vertices) {
        for (int i = 0; i < 3; ++i) cmin = std::min(cmin, v.color[i]), cmax = std::max(cmax, v.color[i]);
        if (b.image >= 0) {
          ++textured;
          for (float u : v.uv) umin = std::min(umin, u), umax = std::max(umax, u);
        } else {
          ++flat;
        }
      }
    std::fprintf(stderr, "colours %g..%g, uvs %g..%g, %zu textured and %zu flat vertices\n", cmin, cmax, umin, umax,
                 textured, flat);
  }
  return {};
}

}  // namespace viewer
