// Ours (docs/modding.md, gamez JSON): see gamez_json.h. The layouts are
// mech3ax's (crates/gamez/src: gamez/mw, textures/mw.rs, materials/write_*.rs),
// which match the shipped worlds. Fields unzbd leaves out take its defaults.
#include "upgrades/json/gamez_json.h"

#include <cstring>

#include "gamez/zclass/cls_mechlib_json.h"
#include "gamez/zutil/zutl_json.h"

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
  o.put32(32, static_cast<uint32_t>(o.b.size()));
  *zbd = std::move(o.b);
  return *why = "nodes.json is not read yet", false;
}

}  // namespace zcls
