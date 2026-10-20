#include "scene.h"

#include <algorithm>
#include <cmath>
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
#include "gamez/zsiscript/si_script.h"
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

// The in/out-of-bounds areas (`inout*`): the AI reads them at mission load and
// deletes the subtree before the first frame (0x4c3930, render.md section 6).
bool under_marker(const zcls::Node* n) {
  for (int d = 0; n && d < 64; ++d, n = n->parent_count() > 0 && n->parents ? n->parents[0] : nullptr)
    if (std::strncmp(n->name(), "inout", 5) == 0) return true;
  return false;
}

// The sky: the `horizon` node follows the camera's position every frame.
const zcls::Node* horizon_of(const zcls::Node* n) {
  for (int d = 0; n && d < 64; ++d, n = n->parent_count() > 0 && n->parents ? n->parents[0] : nullptr)
    if (std::strcmp(n->name(), "horizon") == 0) return n;
  return nullptr;
}

Xform world_xform(const zcls::Node* n, const Pose* pose = nullptr, int depth = 0) {
  Xform x = local_xform(n);
  if (pose) {
    auto it = pose->find(n->name());
    if (it != pose->end()) std::memcpy(x.m, it->second.data(), 36), std::memcpy(x.t, it->second.data() + 9, 12);
  }
  if (depth < 64 && n->parent_count() > 0 && n->parents && n->parents[0])
    return combine(x, world_xform(n->parents[0], pose, depth + 1));
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
    const uint8_t a = t.alpha ? t.alpha[i] : 255;  // no colour key anywhere (render.md, section 3)
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

  // One polygon, as the original colours it (render.md, section 3): textured
  // ones take the vertex colours (0..255) times the texture and ignore the
  // material; untextured ones take the material colour and alpha and ignore
  // the vertex colours. Alpha 0 draws nothing; below 255, or a texture with an
  // alpha plane, goes to the blended pass. Flag 0x100 is two-sided. Lighting
  // and fog are not done here.
  struct Poly {
    uint32_t flags;
    const uint32_t* index;
    const float* uvs;
    const float* colors;
    int img;
    const float* rgb;
    uint8_t alpha;
  };
  void polygon(const Xform& x, const float* verts, uint32_t vcount, const Poly& p, bool marker, const float* origin) {
    const uint32_t k = p.flags & 0xff;
    if (k < 3 || !p.index) return;
    if (p.img < 0 && p.alpha == 0) return;
    ++out_.polygons;
    const bool blend = p.img >= 0 ? out_.images[p.img].alpha : p.alpha < 255;
    const bool two_sided = (p.flags & 0x100) != 0;
    const bool sky = origin != nullptr;
    const int key = ((p.img + 2) << 4) | marker | blend << 1 | two_sided << 2 | sky << 3;
    auto [it, added] = batch_of_.try_emplace(key, static_cast<int>(out_.batches.size()));
    if (added) out_.batches.push_back(Batch{p.img, {}, marker, blend, two_sided, sky});
    Batch& b = out_.batches[it->second];
    auto vertex = [&](uint32_t j) {
      Vertex v{};
      if (p.index[j] < vcount) apply(x, verts + 3 * p.index[j], v.pos);
      if (sky)
        for (int a = 0; a < 3; ++a) v.pos[a] -= origin[a];  // drawn around the camera
      if (p.img >= 0 && p.uvs) v.uv[0] = p.uvs[2 * j], v.uv[1] = p.uvs[2 * j + 1];
      float c[4] = {1, 1, 1, 1};
      if (p.img >= 0) {
        if (p.colors) c[0] = p.colors[3 * j] / 255, c[1] = p.colors[3 * j + 1] / 255, c[2] = p.colors[3 * j + 2] / 255;
      } else {
        c[0] = p.rgb[0] / 255, c[1] = p.rgb[1] / 255, c[2] = p.rgb[2] / 255, c[3] = p.alpha / 255.0f;
      }
      std::copy(c, c + 4, v.color);
      if (!sky)
        for (int a = 0; a < 3; ++a) {
          out_.min[a] = first_ ? v.pos[a] : std::min(out_.min[a], v.pos[a]);
          out_.max[a] = first_ ? v.pos[a] : std::max(out_.max[a], v.pos[a]);
        }
      if (!sky) first_ = false;
      b.vertices.push_back(v);
    };
    for (uint32_t j = 1; j + 1 < k; ++j) {  // a fan, as the original (no world polygon is a strip)
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
    const zcls::Node* horizon = horizon_of(n);
    const Xform hx = horizon ? world_xform(horizon) : Xform{};
    for (uint32_t p = 0; p < m.count(0x10) && m.polygons; ++p) {
      const zmod::Polygon& poly = m.polygons[p];
      const zmod::Material* mat = poly.material;
      const bool textured = mat && (mat->flags & 1) && poly.uvs;
      const int img = textured && mat->texture ? build.image(mat->texture->name) : -1;
      const float rgb[3] = {mat ? mat->r : 255, mat ? mat->g : 255, mat ? mat->b : 255};
      build.polygon(x, m.vertices, m.count(0x14),
                    {poly.flags, poly.vertex_indices, poly.uvs, poly.colors, img, rgb,
                     static_cast<uint8_t>(mat ? mat->alpha : 255)},
                    marker, horizon ? hx.t : nullptr);
    }
  }
  return {};
}

struct Mech::Data {
  zutl_zar::Archive archive{};
  std::vector<zcls::Material> mats;
  zcls::Node* root = nullptr;
  std::vector<zimg::Library*> libs;
};

Mech::Mech() : d_(new Data) {}
Mech::~Mech() = default;

std::string Mech::load(const std::string& zbd_dir, const std::string& entry) {
  const std::string& dir = dir_slash(zbd_dir);
  zutl_zar::Archive& a = d_->archive;
  zutl_zar::init(a);
  if (!zutl_zar::open(a, (dir + "mechlib.zbd").c_str())) return "could not open " + dir + "mechlib.zbd";
  // The shared materials (0x4fc9e0), then the tree (0x4fcdc0).
  if (std::FILE* f = zutl_zar::seek(a, zutl_zar::find(a, "materials"), nullptr)) {
    uint32_t count = 0;
    std::fread(&count, 4, 1, f);
    d_->mats.resize(count);
    for (zcls::Material& m : d_->mats) zcls::read_material(f, m);
  }
  std::FILE* f = zutl_zar::seek(a, zutl_zar::find(a, entry.c_str()), nullptr);
  if (!f) {
    std::string names;
    for (int i = 0; i < a.count; ++i)
      if (std::strstr(a.entries[i].name, ".flt")) names += std::string(" ") + a.entries[i].name;
    return "no " + entry + " in mechlib.zbd; entries:" + names;
  }
  zcls::nodes() = zcls::NodePool{};
  zmod::models() = zmod::ModelPool{};
  zmod::reset_pool();  // 0x00526800: the mechlib models come from the world pool
  zcls::TreeReader reader;
  d_->root = zcls::read_tree(reader, f, nullptr);
  if (!d_->root) return "could not read " + entry;
  d_->libs = open_libraries({"rmechtex", "rmechtex16", "rimage"});
  return {};
}

void Mech::build(Scene& out, const Pose* pose) const {
  out = Scene{};
  Builder build(out, d_->libs);
  std::vector<zcls::Node*> todo{d_->root};
  while (!todo.empty()) {
    zcls::Node* n = todo.back();
    todo.pop_back();
    ++out.nodes;
    for (int c = n->child_count() - 1; c >= 0 && n->children; --c) todo.push_back(n->children[c]);
    if (n->cls() != kObject3d || !n->model) continue;
    ++out.objects;
    const zmod::Model& m = *n->model;
    const Xform x = world_xform(n, pose);
    for (uint32_t p = 0; p < m.count(0x10) && m.polygons; ++p) {
      const zmod::Polygon& poly = m.polygons[p];
      const zcls::Material* mat = poly.material_index < d_->mats.size() ? &d_->mats[poly.material_index] : nullptr;
      float rgb[3] = {255, 255, 255};
      int img = -1;
      uint8_t alpha = 255;
      if (mat && mat->raw) {
        std::memcpy(rgb, mat->raw + 4, 12);
        alpha = mat->raw[0];
        if ((mat->raw[1] & 1) && poly.uvs) img = build.image(mat->texture);
      }
      build.polygon(x, m.vertices, m.count(0x14), {poly.flags, poly.vertex_indices, poly.uvs, poly.colors, img, rgb, alpha},
                    false, nullptr);
    }
  }
}

struct Motion::Data {
  zsis::Script script;
};

Motion::Motion() : d_(new Data) {}
Motion::~Motion() = default;
Motion::Motion(Motion&&) noexcept = default;
Motion& Motion::operator=(Motion&&) noexcept = default;

std::string Motion::load(const std::string& zbd_dir, const std::string& name) {
  zutl_zar::Archive a;
  zutl_zar::init(a);
  const std::string path = zbd_dir + "/motion.zbd";
  if (!zutl_zar::open(a, path.c_str())) return "could not open " + path;
  std::FILE* f = zutl_zar::seek(a, zutl_zar::find(a, name.c_str()), nullptr);
  const bool ok = f && d_->script.read(f);
  zutl_zar::close(a);
  zutl_zar::destroy(a);
  return ok ? std::string{} : "no motion " + name + " in " + path;
}

float Motion::loop_time() const { return d_->script.loop_time; }

void Motion::pose(float t, Pose& out) const {
  const zsis::Script& s = d_->script;
  if (s.frame_count == 0 || s.loop_time <= 0) return;
  float f = std::fmod(t, s.loop_time) / s.loop_time * static_cast<float>(s.frame_count);
  if (f < 0) f += static_cast<float>(s.frame_count);
  const auto i = static_cast<uint32_t>(f);
  const float u = f - static_cast<float>(i);
  for (uint32_t p = 0; p < s.part_count; ++p) {
    const zsis::Part& part = *s.parts[p];
    std::array<float, 12> m{1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    if (part.rotations) {
      const float* a = part.rotations + 4 * i;
      const float* b = part.rotations + 4 * (i + 1);
      const float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
      const float sb = dot < 0 ? -u : u;
      float q[4];
      for (int k = 0; k < 4; ++k) q[k] = a[k] * (1 - u) + b[k] * sb;
      const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
      for (float& v : q) v /= len;
      const float w = q[0], x = q[1], y = q[2], z = q[3];
      // Rows are the rotated axes (row vectors): the transpose of the column form.
      const float r[9] = {1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y),
                          2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x),
                          2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)};
      std::copy(r, r + 9, m.begin());
    }
    if (part.translations)
      for (int k = 0; k < 3; ++k)
        m[9 + k] = part.translations[3 * i + k] * (1 - u) + part.translations[3 * (i + 1) + k] * u;
    out[part.name ? part.name : ""] = m;
  }
}

std::vector<std::string> motions(const std::string& zbd_dir, const std::string& prefix) {
  std::vector<std::string> names;
  zutl_zar::Archive a;
  zutl_zar::init(a);
  if (zutl_zar::open(a, (zbd_dir + "/motion.zbd").c_str()))
    for (int i = 0; i < a.count; ++i)
      if (std::strncmp(a.entries[i].name, prefix.c_str(), prefix.size()) == 0) names.emplace_back(a.entries[i].name);
  zutl_zar::close(a);
  zutl_zar::destroy(a);
  return names;
}

}  // namespace viewer
