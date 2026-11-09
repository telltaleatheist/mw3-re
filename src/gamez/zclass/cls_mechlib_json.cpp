// Ours (docs/modding.md, mech3ax's JSON): see cls_mechlib_json.h. The
// layouts are mech3ax's (crates/gamez/src: mechlib/mw/write.rs,
// nodes/node/mw, nodes/object3d, model/mw, materials/write_single.rs), which
// match the shipped bytes: a node's 0xd0-byte record, its object3d (0x90),
// its model (0x5c) and the model's data, then its children.
#include "gamez/zclass/cls_mechlib_json.h"

#include <cmath>
#include <cstring>

#include "gamez/zutil/zutl_json.h"

#pragma STDC FP_CONTRACT OFF  // mech3ax's sums, unfused (CLAUDE.md, FPU)

namespace zcls {
namespace {

using zutl_json::Value;

struct Out {
  std::vector<uint8_t> b;
  void u32(uint32_t v) { raw(&v, 4); }
  void i32(int32_t v) { raw(&v, 4); }
  void f32(float v) { raw(&v, 4); }
  void raw(const void* p, size_t n) { b.insert(b.end(), static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + n); }
};

struct Json {
  zutl_json::Reader r;
  const Value* get(const Value& obj, const std::string& at, const char* key) {
    return r.object(obj, at) ? r.member(obj, at, key) : nullptr;
  }
  bool f32(const Value& obj, const std::string& at, const char* key, float* out) {
    const Value* v = get(obj, at, key);
    return v && r.f32(*v, at + "." + key, out);
  }
  bool i32(const Value& obj, const std::string& at, const char* key, int32_t* out, int32_t lo = INT32_MIN, int32_t hi = INT32_MAX) {
    const Value* v = get(obj, at, key);
    if (!v) return false;
    if (v->kind != Value::Number || v->text.find_first_of(".eE") != std::string::npos) return r.fail(*v, at + "." + key, "not a whole number");
    const long long n = v->text.size() > 12 ? (v->text[0] == '-' ? INT64_MIN : INT64_MAX) : std::strtoll(v->text.c_str(), nullptr, 10);
    if (n < lo || n > hi) return r.fail(*v, at + "." + key, v->text + ": past " + std::to_string(lo) + ".." + std::to_string(hi));
    *out = static_cast<int32_t>(n);
    return true;
  }
  bool u32(const Value& obj, const std::string& at, const char* key, uint32_t* out) {
    const Value* v = get(obj, at, key);
    return v && r.u32(*v, at + "." + key, out);
  }
  // A member that is a list of objects or numbers (null an empty one, as unzbd may write).
  const Value* list(const Value& obj, const std::string& at, const char* key, size_t most) {
    const Value* v = get(obj, at, key);
    if (!v) return nullptr;
    static const Value kEmpty = [] {
      Value e;
      e.kind = Value::Array;
      return e;
    }();
    if (v->kind == Value::Null) return &kEmpty;
    if (!r.array(*v, at + "." + key)) return nullptr;
    if (v->items.size() > most) return r.fail(*v, at + "." + key, "more than " + std::to_string(most) + " items"), nullptr;
    return v;
  }
  bool vec(const Value& v, const std::string& at, const char* const* keys, int n, float* out) {
    if (!r.object(v, at)) return false;
    for (int k = 0; k < n; ++k)
      if (!f32(v, at, keys[k], out + k)) return false;
    return true;
  }
  bool vec3(const Value& obj, const std::string& at, const char* key, float out[3]) {
    static const char* const kXyz[] = {"x", "y", "z"};
    const Value* v = get(obj, at, key);
    return v && vec(*v, at + "." + key, kXyz, 3, out);
  }
  bool bbox(const Value& obj, const std::string& at, const char* key, Out& o) {
    const Value* v = get(obj, at, key);
    float a[3], b[3];
    if (!(v && vec3(*v, at + "." + key, "a", a) && vec3(*v, at + "." + key, "b", b))) return false;
    for (float x : a) o.f32(x);
    for (float x : b) o.f32(x);
    return true;
  }
  // Booleans by name into bits; a name left out is false, one not known refused.
  bool bits(const Value& obj, const std::string& at, const char* key, const std::pair<const char*, uint32_t>* names, int n,
            uint32_t* out) {
    const Value* v = get(obj, at, key);
    if (!v || !r.object(*v, at + "." + key)) return false;
    *out = 0;
    for (const auto& [name, val] : v->members) {
      int k = 0;
      while (k < n && name != names[k].first) ++k;
      if (k == n) return r.fail(val, at + "." + key + "." + name, "not a flag this format has");
      if (val.kind != Value::Bool) return r.fail(val, at + "." + key + "." + name, "not true or false");
      if (val.b) *out |= names[k].second;
    }
    return true;
  }
  bool name_of(const Value& obj, const std::string& at, const char* key, const char* const* names, int n, uint32_t* out) {
    const Value* v = get(obj, at, key);
    if (!v) return false;
    for (int k = 0; k < n; ++k)
      if (v->kind == Value::String && v->text == names[k]) return *out = static_cast<uint32_t>(k), true;
    std::string all;
    for (int k = 0; k < n; ++k) all += std::string(k ? ", " : "") + names[k];
    return r.fail(*v, at + "." + key, "not one of " + all);
  }
};

// mech3ax's assert_ptr: a count of 0 has a null pointer; a count with a null one gets its stand-in.
uint32_t ptr_for(size_t count, uint32_t ptr) { return count == 0 ? 0 : ptr == 0 ? 0xdefa0175u : ptr; }

const std::pair<const char*, uint32_t> kNodeFlags[] = {
    {"active", 1u << 2},          {"altitude_surface", 1u << 3}, {"intersect_surface", 1u << 4}, {"intersect_bbox", 1u << 5},
    {"proximity", 1u << 6},       {"landmark", 1u << 7},         {"bbox_node", 1u << 8},         {"bbox_model", 1u << 9},
    {"bbox_child", 1u << 10},     {"terrain", 1u << 15},         {"can_modify", 1u << 16},       {"clip_to", 1u << 17},
    {"tree_valid", 1u << 19},     {"override", 1u << 23},        {"id_zone_check", 1u << 24},    {"unk25", 1u << 25},
    {"unk28", 1u << 28},
};
const std::pair<const char*, uint32_t> kModelFlags[] = {
    {"lighting", 1},     {"fog", 2},        {"texture_registered", 4},   {"morph", 8},
    {"texture_scroll", 0x10}, {"clouds", 0x20}, {"facade_centroid", 0x40},
};
const std::pair<const char*, uint32_t> kPolygonFlags[] = {
    {"show_backface", 0x100}, {"unk3", 0}, {"tri_strip", 0x400}, {"in_out", 0},  // unk3 and in_out are PM's, ignored
};
const char* const kModelTypes[] = {"Default", "Facade", "Points"};
const char* const kFacadeModes[] = {"CylindricalY", "SphericalY", "CylindricalX", "CylindricalZ"};
constexpr size_t kMost = 1u << 20;  // ours: past anything a mech holds

// mech3ax's object_matrix: y, x, z, then transposed; rows scaled. Each sine and cosine is the float nearest
// the true one (from double): the shipped matrices' bits, where a float libm's cosf can miss by one (blackhawk's
// lcalf: cosf(0.855211) 0x3f27f37c, the file and the true value 0x3f27f37b) and differs between systems.
float sin_f(float a) { return static_cast<float>(std::sin(static_cast<double>(a))); }
float cos_f(float a) { return static_cast<float>(std::cos(static_cast<double>(a))); }
void object_matrix(const float rot[3], const float scale[3], const float pos[3], float m[12]) {
  const float sx = sin_f(rot[0]), cx = cos_f(rot[0]), sy = sin_f(rot[1]), cy = cos_f(rot[1]);
  const float sz = sin_f(rot[2]), cz = cos_f(rot[2]);
  const float r[12] = {sy * sx * sz + cy * cz, cx * sz, cy * sx * sz - sy * cz, sy * sx * cz - cy * sz, cx * cz,
                       cy * sx * cz + sy * sz, sy * cx, -sx, cy * cx, pos[0], pos[1], pos[2]};
  std::memcpy(m, r, sizeof r);
  for (int row = 0; row < 3; ++row)
    if (scale[row] != 1.0f)
      for (int k = 0; k < 3; ++k) m[row * 3 + k] = m[row * 3 + k] * scale[row];
}

bool object3d(Json& j, const Value& node, const std::string& at, Out& o) {
  const Value* data = j.get(node, at, "data");
  if (!data || !j.r.object(*data, at + ".data")) return false;
  const Value* obj = data->get("Object3d");
  if (!obj) return j.r.fail(*data, at + ".data", "not an Object3d (a mechlib's nodes are)");
  const std::string oa = at + ".data.Object3d";
  uint32_t flags = 0x20;
  float opacity = 0, color[3] = {0, 0, 0}, unk = 0, rot[3] = {0, 0, 0}, scale[3] = {1, 1, 1};
  float m[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
  uint32_t signs = 0;
  const Value* op = j.get(*obj, oa, "opacity");
  if (!op) return false;
  if (op->kind != Value::Null) {
    if (!j.r.f32(*op, oa + ".opacity", &opacity)) return false;
    flags |= 2;
  }
  const Value* co = j.get(*obj, oa, "color");
  if (!co) return false;
  if (co->kind != Value::Null) {
    static const char* const kRgb[] = {"r", "g", "b"};
    if (!j.vec(*co, oa + ".color", kRgb, 3, color)) return false;
    flags |= 4;
  }
  if (!j.f32(*obj, oa, "unk", &unk) || !j.u32(*obj, oa, "signs", &signs)) return false;
  const Value* tr = j.get(*obj, oa, "transform");
  if (!tr) return false;
  if (tr->kind == Value::String && tr->text == "Initial") {
    flags |= 8;
  } else if (const Value* rts = tr->kind == Value::Object ? tr->get("RotateTranslateScale") : nullptr) {
    const std::string ra = oa + ".transform.RotateTranslateScale";
    float pos[3];
    if (!(j.vec3(*rts, ra, "rotate", rot) && j.vec3(*rts, ra, "translate", pos) && j.vec3(*rts, ra, "scale", scale))) return false;
    object_matrix(rot, scale, pos, m);
    const Value* orig = rts->get("original");
    if (orig && orig->kind != Value::Null) {  // unzbd's: the file's own matrix, where its sums differ
      static const char* const kM[] = {"r00", "r01", "r02", "r10", "r11", "r12", "r20", "r21", "r22", "r30", "r31", "r32"};
      if (!j.vec(*orig, ra + ".original", kM, 12, m)) return false;
    }
  } else if (const Value* mat = tr->kind == Value::Object ? tr->get("Matrix") : nullptr) {
    static const char* const kM[] = {"r00", "r01", "r02", "r10", "r11", "r12", "r20", "r21", "r22", "r30", "r31", "r32"};
    if (!j.vec(*mat, oa + ".transform.Matrix", kM, 12, m)) return false;
    flags |= 0x10;
  } else {
    return j.r.fail(*tr, oa + ".transform", "not \"Initial\", RotateTranslateScale or Matrix");
  }
  for (int k = 0; k < 15; ++k) {  // mech3ax's load_signs: -0.0 where its bit says, over a +0.0
    float* v = k < 12 ? &m[k] : &rot[k - 12];
    uint32_t bits;
    std::memcpy(&bits, v, 4);
    if ((signs >> k & 1) && bits == 0) bits = 0x80000000u, std::memcpy(v, &bits, 4);
  }
  o.u32(flags), o.f32(opacity);
  for (float x : color) o.f32(x);
  o.f32(unk);
  for (float x : rot) o.f32(x);
  for (float x : scale) o.f32(x);
  for (float x : m) o.f32(x);
  for (int k = 0; k < 12; ++k) o.f32(0.0f);  // field096
  return true;
}

bool model(Json& j, const Value& md, const std::string& at, Out& o) {
  uint32_t type = 0, facade = 0, flags = 0, parents = 0, vp = 0, np = 0, lp = 0, mp = 0, pp = 0;
  float scroll[2] = {0, 0}, mid[3], diag = 0;
  const Value *verts, *norms, *morphs, *lights, *polys, *ts;
  if (!(j.name_of(md, at, "model_type", kModelTypes, 3, &type) && j.name_of(md, at, "facade_mode", kFacadeModes, 4, &facade) &&
        j.bits(md, at, "flags", kModelFlags, 7, &flags) && j.u32(md, at, "parent_count", &parents) &&
        (verts = j.list(md, at, "vertices", kMost)) && (norms = j.list(md, at, "normals", kMost)) &&
        (morphs = j.list(md, at, "morphs", kMost)) && (lights = j.list(md, at, "lights", kMost)) &&
        (polys = j.list(md, at, "polygons", kMost)) && (ts = j.get(md, at, "texture_scroll")) &&
        j.f32(*ts, at + ".texture_scroll", "u", &scroll[0]) && j.f32(*ts, at + ".texture_scroll", "v", &scroll[1]) &&
        j.vec3(md, at, "bbox_mid", mid) && j.f32(md, at, "bbox_diag", &diag) && j.u32(md, at, "polygons_ptr", &pp) &&
        j.u32(md, at, "vertices_ptr", &vp) && j.u32(md, at, "normals_ptr", &np) && j.u32(md, at, "lights_ptr", &lp) &&
        j.u32(md, at, "morphs_ptr", &mp)))
    return false;
  if (!lights->items.empty()) return j.r.fail(*lights, at + ".lights", "a mechlib model's lights are not read (none ship)");
  o.u32(type), o.u32(facade), o.u32(flags), o.u32(parents);
  const size_t np_ = polys->items.size(), nv = verts->items.size(), nn = norms->items.size(), nm = morphs->items.size();
  o.u32(static_cast<uint32_t>(np_)), o.u32(static_cast<uint32_t>(nv)), o.u32(static_cast<uint32_t>(nn)), o.u32(static_cast<uint32_t>(nm)), o.u32(0);
  o.f32(0.0f), o.f32(scroll[0]), o.f32(scroll[1]), o.u32(0);
  o.u32(ptr_for(np_, pp)), o.u32(ptr_for(nv, vp)), o.u32(ptr_for(nn, np)), o.u32(ptr_for(0, lp)), o.u32(ptr_for(nm, mp));
  for (float x : mid) o.f32(x);
  o.f32(diag), o.u32(0);
  static const char* const kXyz[] = {"x", "y", "z"};
  for (const auto& [list, key] : {std::pair{verts, "vertices"}, std::pair{norms, "normals"}, std::pair{morphs, "morphs"}})
    for (size_t i = 0; i < list->items.size(); ++i) {
      float v[3];
      if (!j.vec(list->items[i], at + "." + key + "[" + std::to_string(i) + "]", kXyz, 3, v)) return false;
      for (float x : v) o.f32(x);
    }
  // The polygons' records, then each one's indices, UVs and colours.
  struct Poly {
    const Value *vi, *ni, *uv, *vc;
  };
  std::vector<Poly> data;
  for (size_t i = 0; i < np_; ++i) {
    const Value& p = polys->items[i];
    const std::string pa = at + ".polygons[" + std::to_string(i) + "]";
    uint32_t pflags = 0, vip = 0, nip = 0, uvp = 0, vcp = 0, mrp = 0;
    int32_t priority = 0, material = 0;
    const Value *vi, *ni, *vc, *mats, *zones;
    if (!(j.bits(p, pa, "flags", kPolygonFlags, 4, &pflags) && j.i32(p, pa, "priority", &priority, -50, 50) &&
          (zones = j.list(p, pa, "zone_set", 3)) && (vi = j.list(p, pa, "vertex_indices", 0xff)) && (ni = j.get(p, pa, "normal_indices")) &&
          (vc = j.list(p, pa, "vertex_colors", 0xff)) && (mats = j.list(p, pa, "materials", 16)) &&
          j.u32(p, pa, "vertex_indices_ptr", &vip) && j.u32(p, pa, "normal_indices_ptr", &nip) && j.u32(p, pa, "uvs_ptr", &uvp) &&
          j.u32(p, pa, "vertex_colors_ptr", &vcp) && j.u32(p, pa, "matl_refs_ptr", &mrp)))
      return false;
    if (ni->kind != Value::Null && !(j.r.array(*ni, pa + ".normal_indices"))) return false;
    if (mats->items.empty()) return j.r.fail(*mats, pa + ".materials", "no material");
    const Value& m0 = mats->items[0];
    if (!j.i32(m0, pa + ".materials[0]", "material_index", &material, 0, 0xffff)) return false;
    const Value* uv = j.get(m0, pa + ".materials[0]", "uv_coords");
    if (!uv || (uv->kind != Value::Null && !j.r.array(*uv, pa + ".materials[0].uv_coords"))) return false;
    uint32_t zone = 0;
    int32_t z[3] = {-1, -1, -1};
    for (size_t k = 0; k < zones->items.size(); ++k) {
      uint32_t zz = 0;
      const Value& zv = zones->items[k];
      const bool neg = zv.kind == Value::Number && zv.text == "-1";
      if (!neg && !j.r.u32(zv, pa + ".zone_set[" + std::to_string(k) + "]", &zz, 127)) return false;
      z[k] = neg ? -1 : static_cast<int32_t>(zz);
    }
    zone = static_cast<uint32_t>(zones->items.size()) | (static_cast<uint32_t>(z[0]) & 0xff) << 8 | (static_cast<uint32_t>(z[1]) & 0xff) << 16 |
           (static_cast<uint32_t>(z[2]) & 0xff) << 24;
    const size_t nni = ni->kind == Value::Null ? 0 : ni->items.size(), nuv = uv->kind == Value::Null ? 0 : uv->items.size();
    o.u32(static_cast<uint32_t>(vi->items.size()) | pflags | (ni->kind != Value::Null ? 0x200u : 0u));
    o.i32(priority), o.u32(vip), o.u32(ptr_for(nni, nip)), o.u32(ptr_for(nuv, uvp)), o.u32(vcp), o.u32(mrp);
    o.i32(material), o.u32(zone);
    data.push_back({vi, ni->kind == Value::Null ? nullptr : ni, uv->kind == Value::Null ? nullptr : uv, vc});
  }
  for (size_t i = 0; i < data.size(); ++i) {
    const std::string pa = at + ".polygons[" + std::to_string(i) + "]";
    for (const auto& [list, key] : {std::pair{data[i].vi, "vertex_indices"}, std::pair{data[i].ni, "normal_indices"}}) {
      if (!list) continue;
      for (size_t k = 0; k < list->items.size(); ++k) {
        uint32_t x = 0;
        if (!j.r.u32(list->items[k], pa + "." + key + "[" + std::to_string(k) + "]", &x)) return false;
        o.u32(x);
      }
    }
    static const char* const kUv[] = {"u", "v"};
    static const char* const kRgb[] = {"r", "g", "b"};
    if (data[i].uv)
      for (size_t k = 0; k < data[i].uv->items.size(); ++k) {
        float v[2];
        if (!j.vec(data[i].uv->items[k], pa + ".materials[0].uv_coords[" + std::to_string(k) + "]", kUv, 2, v)) return false;
        o.f32(v[0]), o.f32(v[1]);
      }
    for (size_t k = 0; k < data[i].vc->items.size(); ++k) {
      float v[3];
      if (!j.vec(data[i].vc->items[k], pa + ".vertex_colors[" + std::to_string(k) + "]", kRgb, 3, v)) return false;
      for (float x : v) o.f32(x);
    }
  }
  return true;
}

bool tree(Json& j, const Value& nodes, const Value& models, size_t index, int depth, std::vector<bool>& seen, Out& o) {
  const std::string at = "$.nodes[" + std::to_string(index) + "]";
  if (index >= nodes.items.size()) return j.r.fail(nodes, "$.nodes", "no node " + std::to_string(index));
  if (seen[index]) return j.r.fail(nodes.items[index], at, "reached twice (a loop in child_indices)");
  if (depth > 64) return j.r.fail(nodes.items[index], at, "nested past 64 deep");
  seen[index] = true;
  const Value& n = nodes.items[index];
  std::string name;
  uint32_t flags = 0, update = 0, data_ptr = 0, parent_ptr = 0, child_ptr = 0;
  int32_t zone = 0, self = 0, f192 = 0, f196 = 0, f200 = 0, f204 = 0, area[2] = {-1, -1};
  const Value *nm, *parents, *children, *mi, *ap;
  if (!(j.r.object(n, at) && (nm = j.get(n, at, "name")) && j.r.game_string(*nm, at + ".name", &name, 35) &&
        j.bits(n, at, "flags", kNodeFlags, 17, &flags) && j.u32(n, at, "update_flags", &update) &&
        j.i32(n, at, "zone_id", &zone, -128, 127) && j.u32(n, at, "data_ptr", &data_ptr) && j.i32(n, at, "index", &self) &&
        (parents = j.list(n, at, "parent_indices", 1)) && (children = j.list(n, at, "child_indices", kMost)) &&
        j.u32(n, at, "parent_array_ptr", &parent_ptr) && j.u32(n, at, "child_array_ptr", &child_ptr) &&
        j.i32(n, at, "field192", &f192) && j.i32(n, at, "field196", &f196) && j.i32(n, at, "field200", &f200) &&
        j.i32(n, at, "field204", &f204) && (mi = j.get(n, at, "model_index")) && (ap = j.get(n, at, "area_partition"))))
    return false;
  if (ap->kind != Value::Null && !(j.i32(*ap, at + ".area_partition", "x", &area[0]) && j.i32(*ap, at + ".area_partition", "z", &area[1])))
    return false;
  char rec_name[36] = "Default_node_name";  // mech3ax's padding: the name over it, its NUL, the rest as it was
  std::memcpy(rec_name, name.c_str(), name.size() + 1);
  o.raw(rec_name, 36);
  o.u32(flags), o.u32(0), o.u32(update), o.u32(static_cast<uint32_t>(zone) & 0xff), o.u32(5), o.u32(data_ptr);
  o.i32(self), o.u32(0), o.u32(1), o.u32(0), o.i32(area[0]), o.i32(area[1]);
  o.u32(static_cast<uint32_t>(parents->items.size())), o.u32(parent_ptr), o.u32(static_cast<uint32_t>(children->items.size())), o.u32(child_ptr);
  o.f32(0), o.f32(0), o.f32(0), o.f32(0);
  if (!(j.bbox(n, at, "node_bbox", o) && j.bbox(n, at, "model_bbox", o) && j.bbox(n, at, "child_bbox", o))) return false;
  o.u32(0), o.i32(f192), o.i32(f196), o.i32(f200), o.i32(f204);
  if (!object3d(j, n, at, o)) return false;
  if (mi->kind != Value::Null && !(mi->kind == Value::Number && mi->text == "-1")) {  // -1 or null: none
    uint32_t m = 0;
    if (!j.r.u32(*mi, at + ".model_index", &m)) return false;
    if (m >= models.items.size()) return j.r.fail(*mi, at + ".model_index", "no model " + std::to_string(m));
    if (!model(j, models.items[m], "$.models[" + std::to_string(m) + "]", o)) return false;
  }
  for (size_t k = 0; k < children->items.size(); ++k) {
    uint32_t c = 0;
    if (!j.r.u32(children->items[k], at + ".child_indices[" + std::to_string(k) + "]", &c)) return false;
    if (!tree(j, nodes, models, c, depth + 1, seen, o)) return false;
  }
  return true;
}

}  // namespace

bool mechlib_materials_from_json(const std::string& json, std::vector<uint8_t>* entry, std::string* why) {
  Value doc;
  if (!zutl_json::parse(json, &doc, why)) return false;
  Json j;
  if (!j.r.array(doc, "$")) return *why = j.r.why, false;
  Out o;
  o.i32(static_cast<int32_t>(doc.items.size()));
  for (size_t i = 0; i < doc.items.size(); ++i) {
    const Value& m = doc.items[i];
    const std::string at = "$[" + std::to_string(i) + "]";
    if (!j.r.object(m, at)) return *why = j.r.why, false;
    if (const Value* t = m.get("Textured")) {
      std::string name;
      uint32_t ptr = 0;
      const Value* tn = j.get(*t, at + ".Textured", "texture_name");
      if (!(tn && j.r.game_string(*tn, at + ".Textured.texture_name", &name, 255) && j.u32(*t, at + ".Textured", "ptr", &ptr)))
        return *why = j.r.why, false;
      const uint8_t head[4] = {0xff, 0x11, 0xff, 0x7f};  // alpha, flags ALWAYS | TEXTURED, rgb 0x7fff
      o.raw(head, 4), o.f32(255), o.f32(255), o.f32(255), o.u32(ptr), o.f32(0), o.f32(0.5f), o.f32(0.5f), o.u32(0), o.u32(0);
      o.u32(static_cast<uint32_t>(name.size()));
      o.raw(name.data(), name.size());
    } else if (const Value* c = m.get("Colored")) {
      float rgb[3];
      int32_t alpha = 0;
      static const char* const kRgb[] = {"r", "g", "b"};
      const Value* col = j.get(*c, at + ".Colored", "color");
      if (!(col && j.vec(*col, at + ".Colored.color", kRgb, 3, rgb) && j.i32(*c, at + ".Colored", "alpha", &alpha, 0, 255)))
        return *why = j.r.why, false;
      const uint8_t head[4] = {static_cast<uint8_t>(alpha), 0x10, 0, 0};  // flags ALWAYS, rgb 0
      o.raw(head, 4);
      for (float x : rgb) o.f32(x);
      o.u32(0), o.f32(0), o.f32(0.5f), o.f32(0.5f), o.u32(0), o.u32(0);
    } else {
      return j.r.fail(m, at, "not Textured or Colored"), *why = j.r.why, false;
    }
  }
  *entry = std::move(o.b);
  return true;
}

bool mechlib_model_from_json(const std::string& json, std::vector<uint8_t>* entry, std::string* why) {
  Value doc;
  if (!zutl_json::parse(json, &doc, why)) return false;
  Json j;
  const Value *nodes = nullptr, *models = nullptr;
  if (!((nodes = j.list(doc, "$", "nodes", kMost)) && (models = j.list(doc, "$", "models", kMost)))) return *why = j.r.why, false;
  if (nodes->items.empty()) return j.r.fail(*nodes, "$.nodes", "no nodes"), *why = j.r.why, false;
  std::vector<bool> seen(nodes->items.size());
  Out o;
  if (!tree(j, *nodes, *models, 0, 0, seen, o)) return *why = j.r.why, false;
  *entry = std::move(o.b);
  return true;
}

}  // namespace zcls
