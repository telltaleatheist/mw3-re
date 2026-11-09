// Ours (docs/modding.md, mech3ax's JSON): see si_script_json.h. The entry
// (si_script.cpp, Script::read): version 4, loop time, frame count, part
// count, then -1.0 and 1.0 (stride and multiplier, the same in every motion);
// each part its name (a length and the bytes), flags 12 (translations 8,
// rotations 4), frames + 1 translations and frames + 1 rotations, the last
// key the first's again, which unzbd leaves out. unzbd's quaternion x, y, z, w
// is the file's order (our w, x, y, z).
#include "gamez/zsiscript/si_script_json.h"

#include <cstdio>
#include <cstring>

#include "gamez/zutil/zutl_json.h"

namespace zsis {
namespace {

constexpr uint32_t kVersion = 4, kFlags = 12;
constexpr uint32_t kMostFrames = 100000, kMostParts = 1024;  // ours: far past any shipped motion's
constexpr float kStride = -1.0f, kMult = 1.0f;

void put4(std::vector<uint8_t>* o, const void* p) {
  const auto* b = static_cast<const uint8_t*>(p);
  o->insert(o->end(), b, b + 4);
}
void put_u32(std::vector<uint8_t>* o, uint32_t v) { put4(o, &v); }
void put_f32(std::vector<uint8_t>* o, float v) { put4(o, &v); }

}  // namespace

bool motion_from_json(const std::string& json, std::vector<uint8_t>* entry, std::string* why) {
  using zutl_json::Value;
  Value doc;
  if (!zutl_json::parse(json, &doc, why)) return false;
  zutl_json::Reader r;
  float loop_time = 0;
  uint32_t frames = 0;
  const Value *lt = nullptr, *fc = nullptr, *parts = nullptr;
  const bool head = r.object(doc, "$") && (lt = r.member(doc, "$", "loop_time")) && r.f32(*lt, "$.loop_time", &loop_time) &&
                    (fc = r.member(doc, "$", "frame_count")) && r.u32(*fc, "$.frame_count", &frames, kMostFrames) &&
                    (parts = r.member(doc, "$", "parts")) && r.array(*parts, "$.parts");
  if (head && !(loop_time > 0.0f)) r.fail(*lt, "$.loop_time", "not above 0");
  if (head && frames == 0) r.fail(*fc, "$.frame_count", "0: a motion has at least one frame");
  if (head && parts->items.size() > kMostParts) r.fail(*parts, "$.parts", "more than " + std::to_string(kMostParts) + " parts");
  if (!r.why.empty()) return *why = r.why, false;
  std::vector<uint8_t> o;
  put_u32(&o, kVersion), put_f32(&o, loop_time), put_u32(&o, frames), put_u32(&o, static_cast<uint32_t>(parts->items.size()));
  put_f32(&o, kStride), put_f32(&o, kMult);
  for (size_t i = 0; i < parts->items.size(); ++i) {
    const Value& part = parts->items[i];
    const std::string at = "$.parts[" + std::to_string(i) + "]";
    std::string name;
    const Value *nm = nullptr, *fr = nullptr;
    if (!(r.object(part, at) && (nm = r.member(part, at, "name")) && r.game_string(*nm, at + ".name", &name, 255) &&
          (fr = r.member(part, at, "frames")) && r.array(*fr, at + ".frames")))
      return *why = r.why, false;
    if (fr->items.size() != frames)
      return r.fail(*fr, at + ".frames", std::to_string(fr->items.size()) + " frames, where frame_count says " + std::to_string(frames)),
             *why = r.why, false;
    put_u32(&o, static_cast<uint32_t>(name.size()));
    o.insert(o.end(), name.begin(), name.end());
    put_u32(&o, kFlags);
    std::vector<float> t, q;
    for (size_t k = 0; k < frames; ++k) {
      const Value& f = fr->items[k];
      const std::string fa = at + ".frames[" + std::to_string(k) + "]";
      const Value *tr = nullptr, *ro = nullptr;
      if (!(r.object(f, fa) && (tr = r.member(f, fa, "translation")) && r.object(*tr, fa + ".translation") &&
            (ro = r.member(f, fa, "rotation")) && r.object(*ro, fa + ".rotation")))
        return *why = r.why, false;
      for (const char* c : {"x", "y", "z"}) {
        const Value* v = r.member(*tr, fa + ".translation", c);
        float x = 0;
        if (!v || !r.f32(*v, fa + ".translation." + c, &x)) return *why = r.why, false;
        t.push_back(x);
      }
      for (const char* c : {"x", "y", "z", "w"}) {
        const Value* v = r.member(*ro, fa + ".rotation", c);
        float x = 0;
        if (!v || !r.f32(*v, fa + ".rotation." + c, &x)) return *why = r.why, false;
        q.push_back(x);
      }
    }
    for (int k = 0; k < 3; ++k) t.push_back(t[k]);  // the last key the first's
    for (int k = 0; k < 4; ++k) q.push_back(q[k]);
    for (float x : t) put_f32(&o, x);
    for (float x : q) put_f32(&o, x);
  }
  *entry = std::move(o);
  return true;
}

bool motion_to_json(const uint8_t* p, size_t size, std::string* json) {
  size_t at = 0;
  auto u32 = [&](uint32_t* v) { return at + 4 <= size && (std::memcpy(v, p + at, 4), at += 4, true); };
  auto f32 = [&](float* v) { return at + 4 <= size && (std::memcpy(v, p + at, 4), at += 4, true); };
  uint32_t version = 0, frames = 0, count = 0;
  float loop_time = 0, stride = 0, mult = 0;
  if (!(u32(&version) && f32(&loop_time) && u32(&frames) && u32(&count) && f32(&stride) && f32(&mult))) return false;
  if (version != kVersion || frames == 0 || frames > kMostFrames || count > kMostParts || stride != kStride || mult != kMult) return false;
  using zutl_json::f32_text;
  std::string o = "{\n  \"loop_time\": " + f32_text(loop_time) + ",\n  \"parts\": [";
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t len = 0, flags = 0;
    if (!u32(&len) || len > 255 || at + len > size) return false;
    std::string name(reinterpret_cast<const char*>(p + at), len);
    at += len;
    if (!u32(&flags) || flags != kFlags) return false;
    const size_t keys = frames + 1;
    if (keys * 28 > size - at) return false;
    const auto* t = reinterpret_cast<const uint8_t*>(p + at);
    const auto* q = t + keys * 12;
    if (std::memcmp(t, t + frames * 12, 12) != 0 || std::memcmp(q, q + frames * 16, 16) != 0) return false;
    o += std::string(i ? "," : "") + "\n    {\n      \"name\": \"";
    for (unsigned char c : name) {  // the game's text as UTF-8 (Windows-1252's upper half as Latin-1)
      if (c == '"' || c == '\\') o += '\\', o += static_cast<char>(c);
      else if (c < 0x20) {
        char b[8];
        std::snprintf(b, sizeof b, "\\u%04x", c);
        o += b;
      } else if (c < 0x80) o += static_cast<char>(c);
      else return false;  // none ship; unzbd's mapping of the upper half is its own
    }
    o += "\",\n      \"frames\": [";
    for (uint32_t k = 0; k < frames; ++k) {
      float v[7];
      std::memcpy(v, t + k * 12, 12);
      std::memcpy(v + 3, q + k * 16, 16);
      o += std::string(k ? "," : "") + "\n        {\"translation\": {\"x\": " + f32_text(v[0]) + ", \"y\": " + f32_text(v[1]) +
           ", \"z\": " + f32_text(v[2]) + "}, \"rotation\": {\"x\": " + f32_text(v[3]) + ", \"y\": " + f32_text(v[4]) +
           ", \"z\": " + f32_text(v[5]) + ", \"w\": " + f32_text(v[6]) + "}}";
    }
    o += "\n      ]\n    }";
    at += keys * 28;
  }
  if (at != size) return false;
  o += "\n  ],\n  \"frame_count\": " + std::to_string(frames) + "\n}\n";
  *json = std::move(o);
  return true;
}

}  // namespace zsis
