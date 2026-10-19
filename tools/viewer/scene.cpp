#include "scene.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "gamez/zclass/cls_mechlib.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zutil/zutl_zar.h"

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

// The in/out-of-bounds areas (`inout*`, plain red) are drawn by nothing in the
// game as far as seen; how it hides them is not mapped yet.
bool under_marker(const zcls::Node* n) {
  for (int d = 0; n && d < 64; ++d, n = n->parent_count() > 0 && n->parents ? n->parents[0] : nullptr)
    if (std::strncmp(n->name(), "inout", 5) == 0) return true;
  return false;
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

// Turns polygons into batches, one per texture (markers apart).
class Builder {
 public:
  Builder(Scene& out, std::vector<zimg::Library*> libs) : out_(out), libs_(std::move(libs)) {}

  int image(const char* name) {
    if (!name || !*name) return -1;
    auto it = image_of_.find(name);
    if (it != image_of_.end()) return it->second;
    int idx = -1;
    for (zimg::Library* lib : libs_) {
      zimg::Texture* t = zimg::load(*lib, name);
      if (!t) continue;
      Image img;
      if (decode(*t, img)) {
        idx = static_cast<int>(out_.images.size());
        out_.images.push_back(std::move(img));
      }
      break;
    }
    if (idx < 0 && out_.missing_textures++ < 5) std::fprintf(stderr, "texture %s: not loaded\n", name);
    image_of_[name] = idx;
    return idx;
  }

  // One polygon: k vertex indices into `verts`, per-vertex UVs (textured only)
  // and colours 0..255; untextured ones are tinted by the material's rgb.
  void polygon(const Xform& x, uint32_t k, const uint32_t* index, const float* verts, uint32_t vcount, const float* uvs,
               const float* colors, int img, const float* rgb, bool marker) {
    if (k < 3 || !index) return;
    ++out_.polygons;
    const int key = marker ? -1000000 + img : img;  // markers batch apart from everything else
    auto [it, added] = batch_of_.try_emplace(key, static_cast<int>(out_.batches.size()));
    if (added) out_.batches.push_back(Batch{img, {}, marker});
    Batch& b = out_.batches[it->second];
    auto vertex = [&](uint32_t j) {
      Vertex v{};
      if (index[j] < vcount) apply(x, verts + 3 * index[j], v.pos);
      if (img >= 0 && uvs) v.uv[0] = uvs[2 * j], v.uv[1] = uvs[2 * j + 1];
      float c[3] = {1, 1, 1};
      if (colors) c[0] = colors[3 * j] / 255, c[1] = colors[3 * j + 1] / 255, c[2] = colors[3 * j + 2] / 255;
      if (img < 0 && rgb) c[0] *= rgb[0] / 255, c[1] *= rgb[1] / 255, c[2] *= rgb[2] / 255;
      v.color[0] = c[0], v.color[1] = c[1], v.color[2] = c[2], v.color[3] = 1;
      for (int a = 0; a < 3; ++a) {
        out_.min[a] = first_ ? v.pos[a] : std::min(out_.min[a], v.pos[a]);
        out_.max[a] = first_ ? v.pos[a] : std::max(out_.max[a], v.pos[a]);
      }
      first_ = false;
      b.vertices.push_back(v);
    };
    for (uint32_t j = 1; j + 1 < k; ++j) {  // a fan, a guess until the render path is mapped
      vertex(0), vertex(j), vertex(j + 1);
      ++out_.triangles;
    }
  }

 private:
  Scene& out_;
  std::vector<zimg::Library*> libs_;
  std::map<std::string, int> image_of_;
  std::map<int, int> batch_of_;
  bool first_ = true;
};

std::vector<zimg::Library*> open_libraries(const std::vector<std::string>& names) {
  std::vector<zimg::Library*> libs;
  for (const std::string& name : names) {
    zimg::Library* lib = zimg::new_library(name.c_str(), 0, false);
    if (zimg::open(*lib)) libs.push_back(lib);
  }
  return libs;
}

std::string& dir_slash(const std::string& zbd_dir) {
  static std::string dir;
  dir = zbd_dir + "/";
  zimg::state().dir = dir.c_str();
  return dir;
}

}  // namespace

std::string load_world(const std::string& zbd_dir, const std::string& mission, Scene& out) {
  const std::string& dir = dir_slash(zbd_dir);
  const std::string path = dir + mission + "/gamez.zbd";
  if (zcls::read_zbd(path.c_str()) != 0) return "could not read " + path;
  // The world's own textures, then the mech skins and the 2D images.
  std::vector<zimg::Library*> libs = open_libraries({mission + "/rtexture", "rmechtex", "rimage"});
  if (libs.empty()) return "no texture library under " + dir;
  Builder build(out, libs);
  const int total = zcls::node_total();
  for (int i = 0; i < total; ++i) {
    zcls::Node* n = zcls::node_at(i);
    if (!n || !(n->link & zcls::kNodeLive)) continue;
    ++out.nodes;
    if (n->cls() != kObject3d || !n->model) continue;
    ++out.objects;
    const bool marker = under_marker(n);
    if (marker) ++out.markers;
    const zmod::Model& m = *n->model;
    const Xform x = world_xform(n);
    for (uint32_t p = 0; p < m.count(0x10) && m.polygons; ++p) {
      const zmod::Polygon& poly = m.polygons[p];
      const zmod::Material* mat = poly.material;
      const bool textured = mat && (mat->flags & 1) && poly.uvs;
      const int img = textured && mat->texture ? build.image(mat->texture->name) : -1;
      const float rgb[3] = {mat ? mat->r : 255, mat ? mat->g : 255, mat ? mat->b : 255};
      build.polygon(x, poly.flags & 0xff, poly.vertex_indices, m.vertices, m.count(0x14), poly.uvs, poly.colors, img,
                    rgb, marker);
    }
  }
  return {};
}

std::string load_mech(const std::string& zbd_dir, const std::string& entry, Scene& out) {
  const std::string& dir = dir_slash(zbd_dir);
  zutl_zar::Archive a;
  zutl_zar::init(a);
  if (!zutl_zar::open(a, (dir + "mechlib.zbd").c_str())) return "could not open " + dir + "mechlib.zbd";
  // The shared materials (0x4fc9e0), then the tree (0x4fcdc0).
  std::vector<zcls::Material> mats;
  if (std::FILE* f = zutl_zar::seek(a, zutl_zar::find(a, "materials"), nullptr)) {
    uint32_t count = 0;
    std::fread(&count, 4, 1, f);
    mats.resize(count);
    for (zcls::Material& m : mats) zcls::read_material(f, m);
  }
  std::FILE* f = zutl_zar::seek(a, zutl_zar::find(a, entry.c_str()), nullptr);
  if (!f) {
    std::string names;
    for (int i = 0; i < a.count; ++i)
      if (std::strstr(a.entries[i].name, ".flt")) names += std::string(" ") + a.entries[i].name;
    return "no " + entry + " in mechlib.zbd; entries:" + names;
  }
  zcls::nodes() = zcls::NodePool{};
  zcls::TreeReader reader;
  const long at = std::ftell(f);
  zcls::Node* root = zcls::read_tree(reader, f, nullptr);
  if (std::getenv("MW3VIEW_STATS"))
    std::fprintf(stderr, "tree at %ld: root %p, read %ld bytes, %d nodes\n", at, static_cast<void*>(root),
                 std::ftell(f) - at, zcls::node_total());
  if (!root) return "could not read " + entry;
  std::vector<zimg::Library*> libs = open_libraries({"rmechtex", "rmechtex16", "rimage"});
  Builder build(out, libs);
  std::vector<zcls::Node*> todo{root};
  while (!todo.empty()) {
    zcls::Node* n = todo.back();
    todo.pop_back();
    ++out.nodes;
    for (int c = 0; c < n->child_count() && n->children; ++c) todo.push_back(n->children[c]);
    if (n->cls() != kObject3d || !n->lib_model) continue;
    ++out.objects;
    const zcls::Model& m = *n->lib_model;
    const Xform x = world_xform(n);
    for (uint32_t p = 0; p < m.polygon_count && m.polygons; ++p) {
      const zcls::Polygon& poly = m.polygons[p];
      const zcls::Material* mat = poly.material < mats.size() ? &mats[poly.material] : nullptr;
      float rgb[3] = {255, 255, 255};
      int img = -1;
      if (mat && mat->raw) {
        std::memcpy(rgb, mat->raw + 4, 12);
        if ((mat->raw[1] & 1) && poly.uvs) img = build.image(mat->texture);
      }
      build.polygon(x, poly.flags & 0xff, poly.vertex_indices, m.vertices, m.vertex_count, poly.uvs, poly.colors, img,
                    rgb, false);
    }
  }
  zutl_zar::close(a);
  zutl_zar::destroy(a);
  return {};
}

}  // namespace viewer
