// Ours (docs/modding.md, gamez JSON): see gamez_json.h. The layouts are
// mech3ax's (crates/gamez/src: gamez/mw, textures/mw.rs, materials/write_*.rs),
// which match the shipped worlds. Fields unzbd leaves out take its defaults.
#include "upgrades/json/gamez_json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "gamez/zclass/cls_mechlib_json.h"
#include "gamez/zutil/zutl_json.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zip.h"

namespace zcls {
namespace {

using zutl_json::Value;

struct Out {
  std::vector<uint8_t> b;
  void raw(const void* p, size_t n) { b.insert(b.end(), static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + n); }
  void u8(uint8_t v) { raw(&v, 1); }
  void u16(uint16_t v) { raw(&v, 2); }
  void i16(int16_t v) { raw(&v, 2); }
  void u32(uint32_t v) { raw(&v, 4); }
  void i32(int32_t v) { raw(&v, 4); }
  void f32(float v) { raw(&v, 4); }
  void put32(size_t at, uint32_t v) { std::memcpy(b.data() + at, &v, 4); }
};

const Value kNull;
// A member, or a null one when unzbd left it out (its default).
const Value& opt(const Value& obj, const char* key) {
  const Value* v = obj.get(key);
  return v ? *v : kNull;
}

struct Json {
  zutl_json::Reader r;
  bool num(const Value& v, const std::string& at, long long lo, long long hi, long long* out) {
    if (v.kind != Value::Number || v.text.find_first_of(".eE") != std::string::npos) return r.fail(v, at, "not a whole number");
    const long long n = v.text.size() > 12 ? (v.text[0] == '-' ? INT64_MIN : INT64_MAX) : std::strtoll(v.text.c_str(), nullptr, 10);
    if (n < lo || n > hi) return r.fail(v, at, v.text + ": past " + std::to_string(lo) + ".." + std::to_string(hi));
    *out = n;
    return true;
  }
  bool i64(const Value& obj, const std::string& at, const char* key, long long lo, long long hi, long long* out) {
    const Value* v = r.object(obj, at) ? r.member(obj, at, key) : nullptr;
    return v && num(*v, at + "." + key, lo, hi, out);
  }
  bool f32(const Value& obj, const std::string& at, const char* key, float* out) {
    const Value* v = r.object(obj, at) ? r.member(obj, at, key) : nullptr;
    return v && r.f32(*v, at + "." + key, out);
  }
  bool name_of(const Value& v, const std::string& at, std::initializer_list<const char*> names, uint32_t* out) {
    uint32_t k = 0;
    std::string all;
    for (const char* n : names) {
      if (v.kind == Value::String && v.text == n) return *out = k, true;
      all += (all.empty() ? "" : ", ") + std::string(n), ++k;
    }
    return r.fail(v, at, "not one of " + all);
  }
};

constexpr uint32_t kSignature = 0x02971222, kVersion = 27;

// mech3ax's from_str_suffix: the name in 20 bytes, its last '.' a NUL (the suffix kept after it).
void texture_name(Out& o, const std::string& name) {
  uint8_t n[20] = {};
  const size_t len = std::min(name.size(), sizeof n);
  std::memcpy(n, name.data(), len);
  size_t dot = len;
  while (dot > 0 && n[dot - 1] != '.') --dot;
  if (dot > 0) n[dot - 1] = 0;
  else n[len < sizeof n ? len : sizeof n - 1] = 0;
  o.raw(n, sizeof n);
}

bool soil(Json& j, const Value& v, const std::string& at, uint32_t* out) {
  if (v.kind == Value::Null) return *out = 0, true;
  return j.name_of(v, at, {"Default", "Water", "Seafloor", "Quicksand", "Lava", "Fire", "Dirt", "Mud", "Grass", "Concrete", "Snow", "Mech",
                           "Silt", "NoSlip"},
                   out);
}

bool materials(Json& j, const Value& list, uint32_t array_size, uint32_t textures, Out& o) {
  if (!j.r.array(list, "$")) return false;
  const uint32_t count = static_cast<uint32_t>(list.items.size());
  if (count >= array_size) return j.r.fail(list, "$", std::to_string(count) + " materials, where the array holds " + std::to_string(array_size));
  o.u32(array_size), o.u32(count), o.i32(static_cast<int32_t>(count)), o.i32(static_cast<int32_t>(count) - 1);
  struct Cycle {
    bool looping;
    int32_t frame;
    float speed;
    uint32_t ptr;
    std::vector<int32_t> textures;
  };
  std::vector<Cycle> cycles;
  for (uint32_t i = 0; i < count; ++i) {
    const Value& m = list.items[i];
    const std::string at = "$[" + std::to_string(i) + "]";
    if (!j.r.object(m, at) || m.members.size() != 1) return j.r.fail(m, at, "not one Textured or Colored");
    const auto& [kind, v] = m.members[0];
    const std::string ka = at + "." + kind;
    uint32_t s = 0;
    if (kind == "Textured") {
      long long tex;
      if (!(j.i64(v, ka, "texture_index", 0, static_cast<long long>(textures) - 1, &tex) && soil(j, opt(v, "soil"), ka + ".soil", &s))) return false;
      const Value& flag = opt(v, "flag");
      const Value& cy = opt(v, "cycle");
      uint8_t flags = 0x10 | 0x01;  // ALWAYS | TEXTURED
      if (flag.kind == Value::Bool && flag.b) flags |= 0x02;
      else if (flag.kind != Value::Null && flag.kind != Value::Bool) return j.r.fail(flag, ka + ".flag", "not true or false");
      uint32_t cycle_ptr = 0;
      if (cy.kind != Value::Null) {
        Cycle c{};
        long long frame, ptr, tptr;
        const Value& lp = opt(cy, "looping");
        const Value* ti = j.r.object(cy, ka + ".cycle") ? j.r.member(cy, ka + ".cycle", "texture_indices") : nullptr;
        if (!(ti && j.r.array(*ti, ka + ".cycle.texture_indices") && j.i64(cy, ka + ".cycle", "current_frame", INT32_MIN, INT32_MAX, &frame) &&
              j.f32(cy, ka + ".cycle", "speed", &c.speed) && j.i64(cy, ka + ".cycle", "cycle_ptr", 0, 0xffffffffll, &ptr) &&
              j.i64(cy, ka + ".cycle", "tex_map_ptr", 0, 0xffffffffll, &tptr)))
          return false;
        c.looping = lp.kind == Value::Bool && lp.b, c.frame = static_cast<int32_t>(frame), c.ptr = static_cast<uint32_t>(tptr);
        for (size_t k = 0; k < ti->items.size(); ++k) {
          long long t;
          if (!j.num(ti->items[k], ka + ".cycle.texture_indices[" + std::to_string(k) + "]", 0, static_cast<long long>(textures) - 1, &t)) return false;
          c.textures.push_back(static_cast<int32_t>(t));
        }
        cycle_ptr = static_cast<uint32_t>(ptr), flags |= 0x04;
        cycles.push_back(std::move(c));
      }
      o.u8(0xff), o.u8(flags), o.u16(0x7fff), o.f32(255), o.f32(255), o.f32(255), o.i32(static_cast<int32_t>(tex));
      o.f32(0), o.f32(0.5f), o.f32(0.5f), o.u32(s), o.u32(cycle_ptr);
    } else if (kind == "Colored") {
      const Value* c = j.r.object(v, ka) ? j.r.member(v, ka, "color") : nullptr;
      float rgb[3];
      long long alpha;
      if (!(c && j.f32(*c, ka + ".color", "r", &rgb[0]) && j.f32(*c, ka + ".color", "g", &rgb[1]) && j.f32(*c, ka + ".color", "b", &rgb[2]) &&
            j.i64(v, ka, "alpha", 0, 255, &alpha) && soil(j, opt(v, "soil"), ka + ".soil", &s)))
        return false;
      o.u8(static_cast<uint8_t>(alpha)), o.u8(0x10), o.u16(0);
      for (float x : rgb) o.f32(x);
      o.i32(0), o.f32(0), o.f32(0.5f), o.f32(0.5f), o.u32(s), o.u32(0);
    } else {
      return j.r.fail(m, at, "not Textured or Colored");
    }
    o.i16(i + 1 < count ? static_cast<int16_t>(i + 1) : -1), o.i16(i > 0 ? static_cast<int16_t>(i - 1) : -1);
  }
  for (uint32_t i = count; i < array_size; ++i) {  // the free slots, chained
    o.u8(0), o.u8(0x20), o.u16(0);
    for (int k = 0; k < 9; ++k) o.u32(0);
    o.i16(i > count ? static_cast<int16_t>(i - 1) : -1), o.i16(i + 1 < array_size ? static_cast<int16_t>(i + 1) : -1);
  }
  for (const Cycle& c : cycles) {
    o.u32(c.looping ? 1 : 0), o.i32(c.frame), o.f32(0), o.f32(c.speed);
    o.u32(static_cast<uint32_t>(c.textures.size())), o.u32(static_cast<uint32_t>(c.textures.size())), o.u32(c.ptr);
    for (int32_t t : c.textures) o.i32(t);
  }
  return true;
}

const std::pair<const char*, uint32_t> kNodeFlags[] = {
    {"active", 1u << 2},      {"altitude_surface", 1u << 3}, {"intersect_surface", 1u << 4}, {"intersect_bbox", 1u << 5},
    {"proximity", 1u << 6},   {"landmark", 1u << 7},         {"bbox_node", 1u << 8},         {"bbox_model", 1u << 9},
    {"bbox_child", 1u << 10}, {"terrain", 1u << 15},         {"can_modify", 1u << 16},       {"clip_to", 1u << 17},
    {"tree_valid", 1u << 19}, {"override", 1u << 23},        {"id_zone_check", 1u << 24},    {"unk25", 1u << 25},
    {"unk28", 1u << 28},
};
const std::pair<const char*, uint32_t> kLightFlags[] = {
    {"recalc", 1},         {"unk1", 2},       {"directional", 4}, {"directed_source", 8}, {"point_source", 0x10}, {"saturated", 0x20},
    {"subdivide", 0x40},   {"static", 0x80},  {"color", 0x100},   {"unk9", 0x200},        {"light_map", 0},       {"bicolored", 0},
};

bool bits(Json& j, const Value& v, const std::string& at, const std::pair<const char*, uint32_t>* names, size_t n, uint32_t* out) {
  if (!j.r.object(v, at)) return false;
  *out = 0;
  for (const auto& [name, val] : v.members) {
    size_t k = 0;
    while (k < n && name != names[k].first) ++k;
    if (k == n) return j.r.fail(val, at + "." + name, "not a flag this format has");
    if (val.kind != Value::Bool) return j.r.fail(val, at + "." + name, "not true or false");
    if (val.b) *out |= names[k].second;
  }
  return true;
}
bool vec(Json& j, const Value& obj, const std::string& at, const char* key, float out[3]) {
  const Value* v = j.r.object(obj, at) ? j.r.member(obj, at, key) : nullptr;
  return v && j.f32(*v, at + "." + key, "x", &out[0]) && j.f32(*v, at + "." + key, "y", &out[1]) && j.f32(*v, at + "." + key, "z", &out[2]);
}
bool rgb(Json& j, const Value& obj, const std::string& at, const char* key, float out[3]) {
  const Value* v = j.r.object(obj, at) ? j.r.member(obj, at, key) : nullptr;
  return v && j.f32(*v, at + "." + key, "r", &out[0]) && j.f32(*v, at + "." + key, "g", &out[1]) && j.f32(*v, at + "." + key, "b", &out[2]);
}
bool range(Json& j, const Value& obj, const std::string& at, const char* key, float out[2]) {
  const Value* v = j.r.object(obj, at) ? j.r.member(obj, at, key) : nullptr;
  return v && j.f32(*v, at + "." + key, "min", &out[0]) && j.f32(*v, at + "." + key, "max", &out[1]);
}
bool indices(Json& j, const Value& obj, const std::string& at, const char* key, std::vector<int32_t>* out) {
  const Value* v = j.r.object(obj, at) ? j.r.member(obj, at, key) : nullptr;
  if (!v || !j.r.array(*v, at + "." + key)) return false;
  for (size_t k = 0; k < v->items.size(); ++k) {
    long long x;
    if (!j.num(v->items[k], at + "." + key + "[" + std::to_string(k) + "]", 0, 0x7fffffff, &x)) return false;
    out->push_back(static_cast<int32_t>(x));
  }
  return true;
}

// mech3ax's partition_diag: in doubles, then its approximate square root (the bits halved).
float partition_diag(float min_y, float max_y) {
  const double z = (static_cast<double>(min_y) - static_cast<double>(max_y)) * 0.5;
  const float t = static_cast<float>(2.0 * 128.0 * 128.0 + z * z);
  int32_t bits;
  std::memcpy(&bits, &t, 4);
  bits = (bits >> 1) + 0x1fc00000;
  float r;
  std::memcpy(&r, &bits, 4);
  return r;
}

// A node's data by its class, then its parents (but an empty's) and children.
bool node_data(Json& j, const Value& n, const std::string& at, const std::string& cls, const Value& d, Out& o, uint32_t* class_id, std::string* why) {
  const std::string da = at + ".data." + cls;
  if (cls == "Empty") {
    *class_id = 0;
  } else if (cls == "Object3d") {
    *class_id = 5;
    std::vector<uint8_t> b;
    if (!zcls::object3d_from_json(n, at, &b, why)) return j.r.why = *why, false;
    o.raw(b.data(), b.size());
  } else if (cls == "Display") {
    *class_id = 4;
    long long v[4];
    float c[3];
    const char* const k[] = {"origin_x", "origin_y", "resolution_x", "resolution_y"};
    for (int i = 0; i < 4; ++i)
      if (!j.i64(d, da, k[i], 0, 0xffffffffll, &v[i])) return false;
    if (!rgb(j, d, da, "clear_color", c)) return false;
    for (long long x : v) o.u32(static_cast<uint32_t>(x));
    for (float x : c) o.f32(x);
  } else if (cls == "Window") {
    *class_id = 3;
    long long v[4];
    const char* const k[] = {"origin_x", "origin_y", "resolution_x", "resolution_y"};
    for (int i = 0; i < 4; ++i)
      if (!j.i64(d, da, k[i], INT32_MIN, INT32_MAX, &v[i])) return false;
    for (long long x : v) o.i32(static_cast<int32_t>(x));
    for (int i = 0; i < 4 * 52; i += 4) o.u32(0);  // four empty clear polygons
    o.i32(0), o.i32(-1), o.u32(0), o.u32(0), o.u32(0), o.u32(0);
  } else if (cls == "Camera") {
    *class_id = 1;
    long long idx[4];
    float near_, far_, lod, hs, vs, hb, vb;
    const char* const k[] = {"world_index", "window_index", "focus_node_xy", "focus_node_xz"};
    for (int i = 0; i < 4; ++i)
      if (!j.i64(d, da, k[i], -1, 0x7fffffff, &idx[i])) return false;
    if (!(j.f32(d, da, "clip_near", &near_) && j.f32(d, da, "clip_far", &far_) && j.f32(d, da, "lod_multiplier", &lod) &&
          j.f32(d, da, "fov_h_scale", &hs) && j.f32(d, da, "fov_v_scale", &vs) && j.f32(d, da, "fov_h_base", &hb) && j.f32(d, da, "fov_v_base", &vb)))
      return false;
    for (long long x : idx) o.i32(static_cast<int32_t>(x));
    o.u32(0);
    for (int i = 0; i < 4 * 3 + 12 + 3 + 12; ++i) o.u32(0);  // translate..world_rotate, the ancestor matrix, the view, the translate matrix
    o.f32(near_), o.f32(far_);
    for (int i = 0; i < 6; ++i) o.u32(0);
    const float hsc = hb * hs, vsc = vb * vs, hh = hsc / 2.0f, vh = vsc / 2.0f;
    o.f32(lod), o.f32(1.0f / (lod * lod)), o.f32(hs), o.f32(vs), o.f32(hb), o.f32(vb), o.f32(hsc), o.f32(vsc), o.f32(hh), o.f32(vh);
    o.i32(1);
    for (int i = 0; i < 3 + 4 * 3; ++i) o.u32(0);
    o.i32(1);
    for (int i = 0; i < 6 * 3; ++i) o.u32(0);
    o.i32(1);
    for (int i = 0; i < 6 * 3; ++i) o.u32(0);
    o.i32(0), o.f32(static_cast<float>(1.0 / std::tan(static_cast<double>(hh)))), o.f32(static_cast<float>(1.0 / std::tan(static_cast<double>(vh))));
    o.i32(0), o.i32(0), o.u32(0xffffff00u);
  } else if (cls == "Lod") {
    *class_id = 6;
    long long f00, f48, f68;
    float r[2], f[16];
    const char* const kf[] = {"field16", "field20", "field24", "field28", "field32", "field36", "field40", "field44",
                              "field52", "field56", "field60", "field64", "field72", "field76"};
    if (!(j.i64(d, da, "field00", INT32_MIN, INT32_MAX, &f00) && range(j, d, da, "range", r) && j.i64(d, da, "field48", INT32_MIN, INT32_MAX, &f48) &&
          j.i64(d, da, "field68", INT32_MIN, INT32_MAX, &f68)))
      return false;
    for (int i = 0; i < 14; ++i)
      if (!j.f32(d, da, kf[i], &f[i])) return false;
    o.i32(static_cast<int32_t>(f00)), o.f32(r[0] * r[0]), o.f32(r[1]), o.f32(r[1] * r[1]);
    for (int i = 0; i < 8; ++i) o.f32(f[i]);
    o.i32(static_cast<int32_t>(f48)), o.f32(f[8]), o.f32(f[9]), o.f32(f[10]), o.f32(f[11]), o.i32(static_cast<int32_t>(f68)), o.f32(f[12]), o.f32(f[13]);
  } else if (cls == "Light") {
    *class_id = 9;
    uint32_t flags;
    float orient[3], tr[3], diff, amb, c[3], r[2];
    std::vector<int32_t> parents;
    long long pptr;
    const Value* fl = j.r.object(d, da) ? j.r.member(d, da, "flags") : nullptr;
    if (!(fl && bits(j, *fl, da + ".flags", kLightFlags, 12, &flags) && vec(j, d, da, "orientation", orient) && vec(j, d, da, "translate", tr) &&
          j.f32(d, da, "diffuse", &diff) && j.f32(d, da, "ambient", &amb) && rgb(j, d, da, "color", c) && range(j, d, da, "range", r) &&
          indices(j, d, da, "parent_indices", &parents) && j.i64(d, da, "parent_ptr", 0, 0xffffffffll, &pptr)))
      return false;
    if (r[1] - r[0] == 0.0f) return j.r.fail(d, da + ".range", "its near and far are the same");
    for (float x : orient) o.f32(x);
    for (float x : tr) o.f32(x);
    for (int i = 0; i < 3 + 3 + 12 + 3 + 3 + 3; ++i) o.u32(0);
    o.f32(0), o.f32(1), o.f32(0);  // the world's view vector
    o.u32(0), o.u32(0), o.u32(0);
    o.f32(diff), o.f32(amb);
    for (float x : c) o.f32(x);
    o.u32(flags), o.f32(r[0]), o.f32(r[1]), o.f32(r[0] * r[0]), o.f32(r[1] * r[1]), o.f32(1.0f / (r[1] - r[0]));
    o.u32(static_cast<uint32_t>(parents.size())), o.u32(static_cast<uint32_t>(pptr));
    for (int32_t x : parents) o.i32(x);
  } else if (cls == "World") {
    *class_id = 2;
    const Value *fog, *area, *parts, *ptrs;
    if (!(j.r.object(d, da) && (fog = j.r.member(d, da, "fog")) && (area = j.r.member(d, da, "area")) && (parts = j.r.member(d, da, "partitions")) &&
          (ptrs = j.r.member(d, da, "ptrs")) && j.r.array(*parts, da + ".partitions")))
      return false;
    uint32_t fog_type;
    float fc[3], fr[2], fa[2], fd;
    long long l, t, rt, b, unk, maxdec;
    std::vector<int32_t> lights, sounds;
    const Value* ft = j.r.member(*fog, da + ".fog", "fog_type");
    if (!(ft && j.name_of(*ft, da + ".fog.fog_type", {"Off", "Linear", "Exponential"}, &fog_type) && rgb(j, *fog, da + ".fog", "fog_color", fc) &&
          range(j, *fog, da + ".fog", "fog_range", fr) && range(j, *fog, da + ".fog", "fog_altitude", fa) && j.f32(*fog, da + ".fog", "fog_density", &fd) &&
          j.i64(*area, da + ".area", "left", INT32_MIN, INT32_MAX, &l) && j.i64(*area, da + ".area", "top", INT32_MIN, INT32_MAX, &t) &&
          j.i64(*area, da + ".area", "right", INT32_MIN, INT32_MAX, &rt) && j.i64(*area, da + ".area", "bottom", INT32_MIN, INT32_MAX, &b) &&
          j.i64(d, da, "unk", INT32_MIN, INT32_MAX, &unk) && j.i64(d, da, "partition_max_dec_feature_count", 0, 255, &maxdec) &&
          indices(j, d, da, "light_indices", &lights) && indices(j, d, da, "sound_indices", &sounds)))
      return false;
    const Value& vp = opt(d, "virtual_partition");
    if (vp.kind != Value::Bool) return j.r.fail(d, da + ".virtual_partition", "not true or false");
    long long p[6];
    const char* const kp[] = {"area_partition_ptr", "virt_partition_ptr", "light_nodes_ptr", "light_data_ptr", "sound_nodes_ptr", "sound_data_ptr"};
    for (int i = 0; i < 6; ++i)
      if (!j.i64(*ptrs, da + ".ptrs", kp[i], 0, 0xffffffffll, &p[i])) return false;
    const int32_t xc = static_cast<int32_t>((rt - l) / 256), zc = static_cast<int32_t>((b - t) / 256);
    o.u32(0), o.i32(0), o.i32(static_cast<int32_t>(unk)), o.u32(static_cast<uint32_t>(p[0])), o.u32(fog_type);
    for (float x : fc) o.f32(x);
    o.f32(fr[0]), o.f32(fr[1]), o.f32(fa[0]), o.f32(fa[1]), o.f32(fd);
    o.f32(static_cast<float>(l)), o.f32(static_cast<float>(b)), o.f32(static_cast<float>(rt - l)), o.f32(static_cast<float>(t - b));
    o.f32(static_cast<float>(rt)), o.f32(static_cast<float>(t)), o.u32(static_cast<uint32_t>(maxdec)), o.u32(vp.b ? 1 : 0);
    o.i32(1), o.i32(1), o.i32(xc - 1), o.i32(zc - 1), o.f32(256), o.f32(-256), o.f32(128), o.f32(-128), o.f32(1.0f / 256), o.f32(1.0f / -256);
    o.f32(-192), o.f32(3), o.f32(3), o.i32(xc), o.i32(zc), o.u32(static_cast<uint32_t>(p[1])), o.f32(1), o.f32(1), o.f32(1);
    o.u32(static_cast<uint32_t>(lights.size())), o.u32(static_cast<uint32_t>(p[2])), o.u32(static_cast<uint32_t>(p[3]));
    o.u32(static_cast<uint32_t>(sounds.size())), o.u32(static_cast<uint32_t>(p[4])), o.u32(static_cast<uint32_t>(p[5])), o.i32(0);
    for (int32_t x : lights) o.i32(x);
    for (int32_t x : sounds) o.i32(x);
    for (size_t row = 0; row < parts->items.size(); ++row) {
      const Value& r = parts->items[row];
      const std::string ra = da + ".partitions[" + std::to_string(row) + "]";
      if (!j.r.array(r, ra)) return false;
      for (size_t c = 0; c < r.items.size(); ++c) {
        const Value& pt = r.items[c];
        const std::string pa = ra + "[" + std::to_string(c) + "]";
        long long x, z, nptr;
        float mn[3], mx[3];
        std::vector<int32_t> nodes;
        if (!(j.i64(pt, pa, "x", INT32_MIN, INT32_MAX, &x) && j.i64(pt, pa, "z", INT32_MIN, INT32_MAX, &z) && vec(j, pt, pa, "min", mn) &&
              vec(j, pt, pa, "max", mx) && indices(j, pt, pa, "node_indices", &nodes) && j.i64(pt, pa, "nodes_ptr", 0, 0xffffffffll, &nptr)))
          return false;
        // the middle's y in doubles, rounded once: the shipped bits (mech3ax keeps a table of those its float sum misses)
        const float mid_y = static_cast<float>((static_cast<double>(mx[1]) + static_cast<double>(mn[1])) * 0.5);
        o.u32(0x100), o.i32(-1), o.f32(static_cast<float>(x)), o.f32(static_cast<float>(z));
        for (float v : mn) o.f32(v);
        for (float v : mx) o.f32(v);
        o.f32(static_cast<float>(x) + 128.0f), o.f32(mid_y), o.f32(static_cast<float>(z) - 128.0f), o.f32(partition_diag(mn[1], mx[1]));
        o.u16(0), o.u16(static_cast<uint16_t>(nodes.size())), o.u32(static_cast<uint32_t>(nptr)), o.i32(0), o.i32(0);
        for (int32_t v : nodes) o.i32(v);
      }
    }
  } else {
    return j.r.fail(d, at + ".data", "not a node class this format has: " + cls);
  }
  return true;
}

}  // namespace

bool gamez_from_json(const std::function<std::string(const std::string&)>& file, std::vector<uint8_t>* zbd, std::string* why) {
  Json j;
  Value meta, tex, mats;
  if (!zutl_json::parse(file("metadata.json"), &meta, why)) return *why = "metadata.json: " + *why, false;
  long long mat_size, model_size, node_size, last_free;
  if (!(j.i64(meta, "$", "material_array_size", 1, 32767, &mat_size) && j.i64(meta, "$", "model_array_size", 0, 0x7fffffff, &model_size) &&
        j.i64(meta, "$", "node_array_size", 0, 0x7fffffff, &node_size) && j.i64(meta, "$", "node_last_free", -1, 0x7fffffff, &last_free)))
    return *why = "metadata.json: " + j.r.why, false;
  if (!zutl_json::parse(file("textures.json"), &tex, why)) return *why = "textures.json: " + *why, false;
  if (!j.r.array(tex, "$")) return *why = "textures.json: " + j.r.why, false;
  const uint32_t ntex = static_cast<uint32_t>(tex.items.size());
  if (ntex >= 4096) return j.r.fail(tex, "$", "4096 textures or more"), *why = "textures.json: " + j.r.why, false;
  Out o;
  o.u32(kSignature), o.u32(kVersion), o.u32(ntex), o.u32(36), o.u32(0), o.u32(0), o.i32(static_cast<int32_t>(node_size));
  o.i32(static_cast<int32_t>(last_free)), o.u32(0);  // the last free node and the later offsets patched when known
  for (uint32_t i = 0; i < ntex; ++i) {
    const Value& t = tex.items[i];
    const std::string at = "$[" + std::to_string(i) + "]";
    std::string name;
    const Value* n = j.r.object(t, at) ? j.r.member(t, at, "name") : nullptr;
    if (!(n && j.r.game_string(*n, at + ".name", &name, 64))) return *why = "textures.json: " + j.r.why, false;
    long long mip = -1;
    if (const Value& m = opt(t, "mip_index"); m.kind != Value::Null && !j.num(m, at + ".mip_index", -1, static_cast<long long>(ntex) - 1, &mip))
      return *why = "textures.json: " + j.r.why, false;
    o.u32(0), o.u32(0);
    texture_name(o, name);
    o.u32(2), o.u32(0), o.i32(static_cast<int32_t>(mip));
  }
  o.put32(16, static_cast<uint32_t>(o.b.size()));
  if (!zutl_json::parse(file("materials.json"), &mats, why)) return *why = "materials.json: " + *why, false;
  if (!materials(j, mats, static_cast<uint32_t>(mat_size), ntex, o)) return *zbd = std::move(o.b), *why = "materials.json: " + j.r.why, false;
  const size_t models_at = o.b.size();
  o.put32(20, static_cast<uint32_t>(models_at));
  // The models, an item at a time: each one's record and its data (cls_mechlib_json's model writer, lights too).
  {
    const std::string text = file("models.json");
    zutl_json::Pull pull(text);
    std::vector<std::vector<uint8_t>> records, datas;
    if (!pull.begin_list()) return *zbd = std::move(o.b), *why = "models.json: " + pull.why(), false;
    while (pull.item()) {
      Value m;
      if (!pull.value(&m)) break;
      records.emplace_back(), datas.emplace_back();
      if (!zcls::model_from_json(m, pull.path(), &records.back(), &datas.back(), why)) return *zbd = std::move(o.b), *why = "models.json: " + *why, false;
    }
    if (!pull.why().empty() || !pull.end()) return *zbd = std::move(o.b), *why = "models.json: " + pull.why(), false;
    const uint32_t count = static_cast<uint32_t>(records.size()), size = static_cast<uint32_t>(model_size);
    if (count >= size) return *zbd = std::move(o.b), *why = "models.json: " + std::to_string(count) + " models, where the array holds " + std::to_string(size), false;
    o.u32(size), o.u32(count), o.i32(static_cast<int32_t>(count));
    uint32_t at = static_cast<uint32_t>(models_at) + 12 + (0x5c + 4) * size;
    for (uint32_t i = 0; i < count; ++i) {
      o.raw(records[i].data(), records[i].size()), o.u32(at);
      at += static_cast<uint32_t>(datas[i].size());
    }
    for (uint32_t i = count; i < size; ++i) {  // the free slots, chained forward
      for (int k = 0; k < 0x5c / 4; ++k) o.u32(0);
      o.i32(i + 1 < size ? static_cast<int32_t>(i + 1) : -1);
    }
    for (const auto& d : datas) o.raw(d.data(), d.size());
  }
  const size_t nodes_at = o.b.size();
  o.put32(32, static_cast<uint32_t>(nodes_at));
  // The nodes, an item at a time: each one's 0xd0 record, then (after the array) its data.
  {
    const std::string text = file("nodes.json");
    zutl_json::Pull pull(text);
    std::vector<std::vector<uint8_t>> records, datas;
    std::vector<int32_t> empty_parent;  // an empty's data offset is its one parent's index
    if (!pull.begin_list()) return *zbd = std::move(o.b), *why = "nodes.json: " + pull.why(), false;
    while (pull.item()) {
      Value n;
      if (!pull.value(&n)) break;
      const std::string at = pull.path();
      Out r, dd;
      std::string name;
      uint32_t flags, update, cls = 0, data_ptr, parent_ptr, child_ptr;
      long long zone, model, f192, f196, f200, f204;
      std::vector<int32_t> parents, children;
      const Value *nm, *fl, *data;
      if (!(j.r.object(n, at) && (nm = j.r.member(n, at, "name")) && j.r.game_string(*nm, at + ".name", &name, 35) && (fl = j.r.member(n, at, "flags")) &&
            bits(j, *fl, at + ".flags", kNodeFlags, 17, &flags) && (data = j.r.member(n, at, "data"))))
        return *zbd = std::move(o.b), *why = "nodes.json: " + j.r.why, false;
      long long u, dp, pp, cp;
      if (!(j.i64(n, at, "update_flags", 0, 0xffffffffll, &u) && j.i64(n, at, "zone_id", -128, 127, &zone) &&
            j.i64(n, at, "model_index", -1, 0x7fffffff, &model) && indices(j, n, at, "parent_indices", &parents) &&
            indices(j, n, at, "child_indices", &children) && j.i64(n, at, "data_ptr", 0, 0xffffffffll, &dp) &&
            j.i64(n, at, "parent_array_ptr", 0, 0xffffffffll, &pp) && j.i64(n, at, "child_array_ptr", 0, 0xffffffffll, &cp) &&
            j.i64(n, at, "field192", INT32_MIN, INT32_MAX, &f192) && j.i64(n, at, "field196", INT32_MIN, INT32_MAX, &f196) &&
            j.i64(n, at, "field200", INT32_MIN, INT32_MAX, &f200) && j.i64(n, at, "field204", INT32_MIN, INT32_MAX, &f204)))
        return *zbd = std::move(o.b), *why = "nodes.json: " + j.r.why, false;
      update = static_cast<uint32_t>(u), data_ptr = static_cast<uint32_t>(dp), parent_ptr = static_cast<uint32_t>(pp), child_ptr = static_cast<uint32_t>(cp);
      std::string cls_name;
      const Value* cls_value = &kNull;
      if (data->kind == Value::String) cls_name = data->text;
      else if (data->kind == Value::Object && data->members.size() == 1) cls_name = data->members[0].first, cls_value = &data->members[0].second;
      else return *zbd = std::move(o.b), *why = "nodes.json: line " + std::to_string(data->line) + " (at " + at + ".data): not one node class", false;
      std::string err;
      if (!node_data(j, n, at, cls_name, *cls_value, dd, &cls, &err)) return *zbd = std::move(o.b), *why = "nodes.json: " + j.r.why, false;
      const bool empty = cls == 0;
      if (!empty)
        for (int32_t x : parents) dd.i32(x);
      for (int32_t x : children) dd.i32(x);
      if (empty && parents.size() != 1) return *zbd = std::move(o.b), *why = "nodes.json: line " + std::to_string(n.line) + " (at " + at + "): an empty node has one parent", false;
      empty_parent.push_back(empty ? parents[0] : -1);
      int32_t area[2] = {-1, -1};
      if (const Value& ap = opt(n, "area_partition"); ap.kind != Value::Null) {
        long long ax, az;
        if (!(j.i64(ap, at + ".area_partition", "x", INT32_MIN, INT32_MAX, &ax) && j.i64(ap, at + ".area_partition", "z", INT32_MIN, INT32_MAX, &az)))
          return *zbd = std::move(o.b), *why = "nodes.json: " + j.r.why, false;
        area[0] = static_cast<int32_t>(ax), area[1] = static_cast<int32_t>(az);
      }
      char rec_name[36] = "Default_node_name";
      std::memcpy(rec_name, name.c_str(), name.size() + 1);
      r.raw(rec_name, 36);
      r.u32(flags), r.u32(0), r.u32(update), r.u32(static_cast<uint32_t>(zone) & 0xff), r.u32(cls), r.u32(data_ptr);
      r.i32(static_cast<int32_t>(model)), r.u32(0), r.u32(1), r.u32(0), r.i32(area[0]), r.i32(area[1]);
      r.u32(empty ? 0 : static_cast<uint32_t>(parents.size())), r.u32(parent_ptr), r.u32(static_cast<uint32_t>(children.size())), r.u32(child_ptr);
      r.f32(0), r.f32(0), r.f32(0), r.f32(0);
      for (const char* bb : {"node_bbox", "model_bbox", "child_bbox"}) {
        const Value* box = j.r.member(n, at, bb);
        float a[3], b2[3];
        if (!(box && vec(j, *box, at + "." + bb, "a", a) && vec(j, *box, at + "." + bb, "b", b2))) return *zbd = std::move(o.b), *why = "nodes.json: " + j.r.why, false;
        for (float x : a) r.f32(x);
        for (float x : b2) r.f32(x);
      }
      r.u32(0), r.i32(static_cast<int32_t>(f192)), r.i32(static_cast<int32_t>(f196)), r.i32(static_cast<int32_t>(f200)), r.i32(static_cast<int32_t>(f204));
      records.push_back(std::move(r.b)), datas.push_back(std::move(dd.b));
    }
    if (!pull.why().empty() || !pull.end()) return *zbd = std::move(o.b), *why = "nodes.json: " + pull.why(), false;
    const uint32_t count = static_cast<uint32_t>(records.size()), size = static_cast<uint32_t>(node_size);
    if (count > size) return *zbd = std::move(o.b), *why = "nodes.json: more nodes than the array holds", false;
    if (last_free == -1) o.put32(28, count);
    uint32_t at = static_cast<uint32_t>(nodes_at) + (0xd0 + 4) * size;
    for (uint32_t i = 0; i < count; ++i) {
      o.raw(records[i].data(), records[i].size());
      o.u32(empty_parent[i] >= 0 ? static_cast<uint32_t>(empty_parent[i]) : at);
      at += static_cast<uint32_t>(datas[i].size());
    }
    for (uint32_t i = count; i < size; ++i) {  // the free slots: a blank record (no model), the next free one
      for (int k = 0; k < 0x3c / 4; ++k) o.u32(0);
      o.i32(-1);
      for (int k = 0; k < (0xd0 - 0x40) / 4; ++k) o.u32(0);
      o.i32(i + 1 < size ? static_cast<int32_t>(i + 1) : 0x00ffffff);
    }
    for (const auto& d : datas) o.raw(d.data(), d.size());
  }
  *zbd = std::move(o.b);
  return true;
}

int gamez_from_mod(const char* game_path, std::vector<uint8_t>* zbd, std::string* what, std::string* why) {
  if (zutl::mounts().mods.empty() || !game_path || !game_path[0]) return 0;
  std::string dir = zutl::host(game_path);
  const size_t slash = dir.rfind('/');
  std::string stem = slash == std::string::npos ? dir : dir.substr(slash + 1);
  dir = slash == std::string::npos ? std::string{} : dir.substr(0, slash + 1);
  if (const size_t dot = stem.rfind('.'); dot != std::string::npos) stem.resize(dot);
  for (size_t m = 0; m < zutl::mounts().mods.size(); ++m) {
    const std::string meta = zutl::find_in_mod(m, dir + stem + "/metadata.json");
    if (!meta.empty()) {  // the folder: the five files side by side
      const std::string folder = meta.substr(0, meta.size() - std::strlen("metadata.json"));
      *what = folder;
      const auto file = [&](const std::string& n) {
        std::string text;
        if (std::FILE* f = std::fopen((folder + n).c_str(), "rb")) {
          char buf[1 << 16];
          for (size_t got; (got = std::fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, got);
          std::fclose(f);
        }
        return text;
      };
      return gamez_from_json(file, zbd, why) ? 1 : -1;
    }
    if (const std::string zip = zutl::find_in_mod(m, dir + stem + ".zip"); !zip.empty()) {
      *what = zip;
      zutl_zip::Zip z;
      if (!z.open(zip)) return *why = "not a zip the game can read", -1;
      const auto file = [&](const std::string& n) {
        for (const zutl_zip::Zip::Entry& e : z.entries) {
          if (e.name.size() != n.size()) continue;
          bool same = true;
          for (size_t k = 0; k < n.size() && same; ++k)
            same = std::tolower(static_cast<unsigned char>(e.name[k])) == std::tolower(static_cast<unsigned char>(n[k]));
          std::vector<uint8_t> d;
          std::string err;
          if (same && z.read(e, &d, &err)) return std::string(d.begin(), d.end());
        }
        return std::string();
      };
      return gamez_from_json(file, zbd, why) ? 1 : -1;
    }
    if (!zutl::find_in_mod(m, dir + stem + ".zbd").empty()) return 0;  // this mod's own gamez.zbd comes first
  }
  return 0;
}

}  // namespace zcls
