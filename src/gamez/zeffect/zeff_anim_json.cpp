// Ours (docs/modding.md, mech3ax's JSON): see zeff_anim_json.h. The layouts
// are mech3ax's (crates/anim/src/mw/anim, mw/anim_def, common/{anim_list,
// seq_def,support,activation_prereq}; crates/anim-events/src/{mw,events}),
// which match the shipped files but for names' bytes past their NUL.
#include "gamez/zeffect/zeff_anim_json.h"

#include <cstring>
#include <map>

#include "gamez/zutil/zutl_json.h"

namespace zeff {
namespace {

using zutl_json::Value;

struct Out {
  std::vector<uint8_t> b;
  std::vector<std::pair<size_t, size_t>>* masks = nullptr;
  void raw(const void* p, size_t n) { b.insert(b.end(), static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + n); }
  void u8(uint8_t v) { raw(&v, 1); }
  void u16(uint16_t v) { raw(&v, 2); }
  void i16(int16_t v) { raw(&v, 2); }
  void u32(uint32_t v) { raw(&v, 4); }
  void i32(int32_t v) { raw(&v, 4); }
  void f32(float v) { raw(&v, 4); }
  void zeros(size_t n) { b.insert(b.end(), n, 0); }
  // A name, NUL-terminated, zeros after: the shipped padding there is mech3ax's tables', so masked.
  void name_padded(const std::string& s, size_t n) {
    const size_t at = b.size();
    zeros(n);
    std::memcpy(b.data() + at, s.data(), std::min(s.size(), n - 1));
    if (masks && s.size() + 1 < n) masks->emplace_back(at + s.size() + 1, at + n);
  }
  // mech3ax's node names: over "Default_node_name", exact.
  void name_node(const std::string& s, size_t n) {
    static const char kDefault[] = "Default_node_name";
    const size_t at = b.size();
    zeros(n);
    std::memcpy(b.data() + at, kDefault, std::min(n, sizeof kDefault - 1));
    const size_t len = std::min(s.size(), n - 1);
    std::memcpy(b.data() + at, s.data(), len);
    b[at + len] = 0;
  }
  // A name with the shipped bytes after it (unzbd's "garbage"/"pad", base64), exact.
  void name_garbage(const std::string& s, const std::vector<uint8_t>& pad, size_t n) {
    const size_t at = b.size();
    zeros(n);
    const size_t len = std::min(s.size(), n - 1);
    std::memcpy(b.data() + at, s.data(), len);
    std::memcpy(b.data() + at + len + 1, pad.data(), std::min(pad.size(), n - len - 1));
  }
};

bool base64(const std::string& s, std::vector<uint8_t>* out) {
  out->clear();
  uint32_t acc = 0;
  int bits = 0;
  for (char c : s) {
    int v;
    if (c >= 'A' && c <= 'Z') v = c - 'A';
    else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
    else if (c >= '0' && c <= '9') v = c - '0' + 52;
    else if (c == '+') v = 62;
    else if (c == '/') v = 63;
    else if (c == '=') break;
    else return false;
    acc = acc << 6 | static_cast<uint32_t>(v), bits += 6;
    if (bits >= 8) bits -= 8, out->push_back(static_cast<uint8_t>(acc >> bits));
  }
  return true;
}

// "1999-04-12T16:09:52.0Z" as Unix seconds (mech3ax's timestamp).
bool unix_time(const std::string& s, uint32_t* out) {
  int y, mo, d, h, mi, se;
  if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6) return false;
  y -= mo <= 2;  // days from the civil date (Howard Hinnant's)
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + static_cast<unsigned>(d) - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const long long days = static_cast<long long>(era) * 146097 + static_cast<long long>(doe) - 719468;
  *out = static_cast<uint32_t>(days * 86400 + h * 3600 + mi * 60 + se);
  return true;
}

struct Json {
  zutl_json::Reader r;
  const Value* get(const Value& obj, const std::string& at, const char* key) { return r.object(obj, at) ? r.member(obj, at, key) : nullptr; }
  bool f32(const Value& obj, const std::string& at, const char* key, float* out) {
    const Value* v = get(obj, at, key);
    return v && r.f32(*v, at + "." + key, out);
  }
  bool num(const Value& v, const std::string& at, long long lo, long long hi, long long* out) {
    if (v.kind != Value::Number || v.text.find_first_of(".eE") != std::string::npos) return r.fail(v, at, "not a whole number");
    const long long n = v.text.size() > 12 ? (v.text[0] == '-' ? INT64_MIN : INT64_MAX) : std::strtoll(v.text.c_str(), nullptr, 10);
    if (n < lo || n > hi) return r.fail(v, at, v.text + ": past " + std::to_string(lo) + ".." + std::to_string(hi));
    *out = n;
    return true;
  }
  bool i64(const Value& obj, const std::string& at, const char* key, long long lo, long long hi, long long* out) {
    const Value* v = get(obj, at, key);
    return v && num(*v, at + "." + key, lo, hi, out);
  }
  bool u32(const Value& obj, const std::string& at, const char* key, uint32_t* out) {
    long long n;
    if (!i64(obj, at, key, 0, 0xffffffffll, &n)) return false;
    *out = static_cast<uint32_t>(n);
    return true;
  }
  bool boolean(const Value& obj, const std::string& at, const char* key, bool* out) {
    const Value* v = get(obj, at, key);
    if (!v) return false;
    if (v->kind != Value::Bool) return r.fail(*v, at + "." + key, "not true or false");
    *out = v->b;
    return true;
  }
  bool str(const Value& obj, const std::string& at, const char* key, std::string* out, size_t most) {
    const Value* v = get(obj, at, key);
    return v && r.game_string(*v, at + "." + key, out, most);
  }
  bool bytes(const Value& obj, const std::string& at, const char* key, std::vector<uint8_t>* out) {
    const Value* v = get(obj, at, key);
    if (!v) return false;
    if (v->kind == Value::Null) return out->clear(), true;
    if (v->kind != Value::String || !base64(v->text, out)) return r.fail(*v, at + "." + key, "not base64 text");
    return true;
  }
  // An enum by its name.
  bool name_of(const Value& obj, const std::string& at, const char* key, std::initializer_list<std::pair<const char*, int>> names,
               int* out) {
    const Value* v = get(obj, at, key);
    if (!v) return false;
    std::string all;
    for (const auto& [n, x] : names) {
      if (v->kind == Value::String && v->text == n) return *out = x, true;
      all += (all.empty() ? "" : ", ") + std::string(n);
    }
    return r.fail(*v, at + "." + key, "not one of " + all);
  }
  // A list member (null: absent), its items.
  const Value* list(const Value& obj, const std::string& at, const char* key, bool* present) {
    const Value* v = get(obj, at, key);
    if (!v) return nullptr;
    *present = v->kind != Value::Null;
    if (*present && !r.array(*v, at + "." + key)) return nullptr;
    return v;
  }
};

// What events name, by the def's lists (mech3ax's lookups: a list's index from 1).
struct Def {
  std::vector<std::string> nodes, lights, puffers, dyn_sounds, static_sounds;
  size_t anim_refs = 0;
  static int16_t find(const std::vector<std::string>& v, const std::string& n) {
    for (size_t i = 0; i < v.size(); ++i)
      if (v[i] == n) return static_cast<int16_t>(i + 1);
    return 0;
  }
};

constexpr int16_t kInputActive = -100, kInputCall = -200;  // INPUT_NODE, as events 6 and 24 write it

bool event_data(Json& j, const Def& def, const std::string& type, const Value& d, const std::string& at, Out& o) {
  const auto node = [&](const char* key, int16_t input, int16_t* out) {
    std::string n;
    if (!j.str(d, at, key, &n, 255)) return false;
    if (n == "INPUT_NODE") return *out = input, true;
    if (!(*out = Def::find(def.nodes, n))) return j.r.fail(d, at + "." + key, "no node \"" + n + "\" in the def's nodes");
    return true;
  };
  if (type == "ObjectActiveState") {
    bool state = false;
    int16_t n = 0;
    if (!(j.boolean(d, at, "state", &state) && node("node", kInputActive, &n))) return false;
    o.u32(state ? 1 : 0), o.i16(n), o.u16(0);
    return true;
  }
  if (type == "CallAnimation") {
    std::string name;
    if (!j.str(d, at, "name", &name, 31)) return false;
    int16_t operand = 0, wait = -1, n = 0;
    uint16_t flags = 0;
    float pos[3] = {0, 0, 0}, tr[3] = {0, 0, 0};
    const Value* op = j.get(d, at, "operand_node");
    const Value* wf = j.get(d, at, "wait_for_completion");
    const Value* params = j.get(d, at, "parameters");
    if (!op || !wf || !params) return false;
    if (op->kind != Value::Null) {
      std::string on;
      if (!j.r.game_string(*op, at + ".operand_node", &on, 255)) return false;
      if (!(operand = Def::find(def.nodes, on))) return j.r.fail(*op, at + ".operand_node", "no node \"" + on + "\" in the def's nodes");
    }
    if (wf->kind != Value::Null) {
      long long w;
      if (!j.num(*wf, at + ".wait_for_completion", 0, static_cast<long long>(def.anim_refs) - 1, &w)) return false;
      flags |= 0x10, wait = static_cast<int16_t>(w);
    }
    if (params->kind != Value::Null) {
      const Value* with = params->kind == Value::Object ? params->get("WithNode") : nullptr;
      const Value* atn = params->kind == Value::Object ? params->get("AtNode") : nullptr;
      if (!with && !atn) return j.r.fail(*params, at + ".parameters", "not WithNode or AtNode");
      const Value& p = with ? *with : *atn;
      const std::string pa = at + ".parameters." + (with ? "WithNode" : "AtNode");
      flags |= with ? 0x08 : 0x01;
      std::string nn;
      if (!j.str(p, pa, "node", &nn, 255)) return false;
      if (nn == "INPUT_NODE") n = kInputCall;
      else if (!(n = Def::find(def.nodes, nn))) return j.r.fail(p, pa + ".node", "no node \"" + nn + "\" in the def's nodes");
      const Value* ps = j.get(p, pa, "position");
      if (!ps) return false;
      if (ps->kind != Value::Null) {
        if (!(j.f32(*ps, pa + ".position", "x", &pos[0]) && j.f32(*ps, pa + ".position", "y", &pos[1]) && j.f32(*ps, pa + ".position", "z", &pos[2])))
          return false;
        flags |= 0x02;
      }
      if (atn) {
        const Value* t = j.get(p, pa, "translate");
        if (!t) return false;
        if (t->kind != Value::Null) {
          if (!(j.f32(*t, pa + ".translate", "x", &tr[0]) && j.f32(*t, pa + ".translate", "y", &tr[1]) && j.f32(*t, pa + ".translate", "z", &tr[2])))
            return false;
          flags |= 0x04;
        }
      }
    }
    o.name_padded(name, 32);
    o.i16(operand), o.u16(flags), o.i16(0), o.i16(wait), o.i16(n), o.u16(0);
    for (float x : pos) o.f32(x);
    for (float x : tr) o.f32(x);
    return true;
  }
  const auto vec = [&](const Value& obj, const std::string& oa, const char* key, float out[3]) {
    const Value* v = j.get(obj, oa, key);
    return v && j.f32(*v, oa + "." + key, "x", &out[0]) && j.f32(*v, oa + "." + key, "y", &out[1]) && j.f32(*v, oa + "." + key, "z", &out[2]);
  };
  const auto opt_node = [&](const char* key, int16_t input, int16_t* out) {  // null: 0
    const Value* v = j.get(d, at, key);
    if (!v) return false;
    if (v->kind == Value::Null) return *out = 0, true;
    std::string n;
    if (!j.r.game_string(*v, at + "." + key, &n, 255)) return false;
    if (n == "INPUT_NODE") return *out = input, true;
    if (!(*out = Def::find(def.nodes, n))) return j.r.fail(*v, at + "." + key, "no node \"" + n + "\" in the def's nodes");
    return true;
  };
  const auto name32 = [&](const char* key, std::string* n) { return j.str(d, at, key, n, 31); };
  if (type == "ObjectScaleState") {
    float v[3];
    int16_t n;
    if (!(vec(d, at, "state", v) && node("name", 0, &n))) return false;
    for (float x : v) o.f32(x);
    o.i32(n);
    return true;
  }
  if (type == "ObjectTranslateState") {
    bool relative;
    float v[3];
    int16_t n, an;
    if (!(j.boolean(d, at, "relative", &relative) && vec(d, at, "state", v) && node("node", 0, &n) && opt_node("at_node", kInputCall, &an)))
      return false;
    o.u32(relative ? 1 : 0);
    for (float x : v) o.f32(x);
    o.i16(n), o.i16(an);
    return true;
  }
  if (type == "ObjectRotateState") {
    float v[3];
    int16_t n, an = 0;
    uint32_t flags = 0;
    const Value* basis;
    if (!(vec(d, at, "state", v) && node("name", 0, &n) && (basis = j.get(d, at, "basis")))) return false;
    if (basis->kind == Value::String && (basis->text == "Relative" || basis->text == "Absolute")) {
      flags = basis->text == "Relative" ? 1 : 0;
    } else if (basis->kind == Value::Object && basis->members.size() == 1 &&
               (basis->members[0].first == "AtNodeXYZ" || basis->members[0].first == "AtNodeMatrix")) {
      flags = basis->members[0].first == "AtNodeXYZ" ? 2 : 4;
      std::string bn;
      if (!j.r.game_string(basis->members[0].second, at + ".basis." + basis->members[0].first, &bn, 255)) return false;
      if (bn == "INPUT_NODE") an = kInputCall;
      else if (!(an = Def::find(def.nodes, bn))) return j.r.fail(*basis, at + ".basis", "no node \"" + bn + "\" in the def's nodes");
    } else {
      return j.r.fail(*basis, at + ".basis", "not Relative, Absolute, AtNodeXYZ or AtNodeMatrix");
    }
    o.u32(flags);
    for (float x : v) o.f32(x);
    o.i16(n), o.i16(an);
    return true;
  }
  if (type == "ObjectOpacityState") {
    bool state;
    int16_t n;
    float opacity = 0;
    const Value* op;
    if (!(j.boolean(d, at, "state", &state) && node("name", kInputActive, &n) && (op = j.get(d, at, "opacity")))) return false;
    if (op->kind != Value::Null && !j.r.f32(*op, at + ".opacity", &opacity)) return false;
    o.u16(state ? 1 : 0), o.u16(op->kind != Value::Null ? 1 : 0), o.f32(opacity), o.i16(n), o.u16(0);
    return true;
  }
  if (type == "CallSequence" || type == "StopSequence") {
    std::string n;
    if (!name32("name", &n)) return false;
    o.name_padded(n, 32), o.i32(-1);
    return true;
  }
  if (type == "StopAnimation") {
    std::string n;
    if (!name32("name", &n)) return false;
    o.name_padded(n, 32), o.i16(0), o.i16(0);
    return true;
  }
  if (type == "ResetAnimation" || type == "InvalidateAnimation") {
    std::string n;
    if (!name32("name", &n)) return false;
    o.name_padded(n, 32), o.i32(0);
    return true;
  }
  if (type == "Loop") {
    if (!j.r.object(d, at) || d.members.size() != 1) return j.r.fail(d, at, "not one Count or RunTime");
    const auto& [k, v] = d.members[0];
    if (k == "Count") {
      long long c;
      if (!j.num(v, at + ".Count", -32768, 65535, &c)) return false;
      o.u32(1), o.u16(static_cast<uint16_t>(c)), o.u16(0);
    } else if (k == "RunTime") {
      float t;
      if (!j.r.f32(v, at + ".RunTime", &t)) return false;
      o.u32(2), o.f32(t);
    } else {
      return j.r.fail(d, at, "not Count or RunTime");
    }
    return true;
  }
  if (type == "Sound") {
    std::string n;
    if (!name32("name", &n)) return false;
    const int16_t si = Def::find(def.static_sounds, n);
    if (!si) return j.r.fail(d, at + ".name", "no sound \"" + n + "\" in the def's static sounds");
    const Value* an = j.get(d, at, "at_node");
    if (!an) return false;
    int16_t ni = 0;
    float pos[3] = {0, 0, 0};
    if (an->kind != Value::Null) {
      std::string nn;
      if (!(j.str(*an, at + ".at_node", "name", &nn, 255) && vec(*an, at + ".at_node", "pos", pos))) return false;
      if (!(ni = Def::find(def.nodes, nn))) return j.r.fail(*an, at + ".at_node.name", "no node \"" + nn + "\" in the def's nodes");
    }
    o.i16(si), o.i16(ni);
    for (float x : pos) o.f32(x);
    return true;
  }
  if (type == "If" || type == "Elseif") {
    const Value* c = j.get(d, at, "condition");
    if (!c || !j.r.object(*c, at + ".condition") || c->members.size() != 1) return j.r.fail(d, at + ".condition", "not one condition");
    const auto& [k, v] = c->members[0];
    const std::string ca = at + ".condition." + k;
    uint32_t ty = 0, node_index = 0, value = 0;
    float f = 0;
    if (k == "RandomWeight" || k == "PlayerRange") {
      if (!j.r.f32(v, ca, &f)) return false;
      ty = k == "RandomWeight" ? 1 : 2;
      std::memcpy(&value, &f, 4);
    } else if (k == "AnimationLod") {
      if (!j.r.u32(v, ca, &value)) return false;
      ty = 4;
    } else if (k == "NodeUndercover") {
      if (!(j.u32(v, ca, "node_index", &node_index) && j.u32(v, ca, "distance", &value))) return false;
      ty = 16;
    } else if (k == "HwRender" || k == "PlayerFirstPerson") {
      if (v.kind != Value::Bool) return j.r.fail(v, ca, "not true or false");
      ty = k == "HwRender" ? 32 : 64, value = v.b ? 1 : 0;
    } else {
      return j.r.fail(*c, at + ".condition", "not a condition this format has: " + k);
    }
    o.u32(ty), o.u32(node_index), o.u32(value);
    return true;
  }
  if (type == "Else" || type == "Endif") return j.r.object(d, at);
  return j.r.fail(d, at, "the event " + type + " is not read yet");
}

const std::map<std::string, uint8_t>& event_types() {
  static const std::map<std::string, uint8_t> t = {
      {"Sound", 1}, {"SoundNode", 2}, {"Effect", 3}, {"LightState", 4}, {"LightAnimation", 5}, {"ObjectActiveState", 6},
      {"ObjectTranslateState", 7}, {"ObjectScaleState", 8}, {"ObjectRotateState", 9}, {"ObjectMotion", 10},
      {"ObjectMotionFromTo", 11}, {"ObjectMotionSiScript", 12}, {"ObjectOpacityState", 13}, {"ObjectOpacityFromTo", 14},
      {"ObjectAddChild", 15}, {"ObjectDeleteChild", 16}, {"ObjectCycleTexture", 17}, {"ObjectConnector", 18},
      {"CallObjectConnector", 19}, {"CameraState", 20}, {"CameraFromTo", 21}, {"CallSequence", 22}, {"StopSequence", 23},
      {"CallAnimation", 24}, {"StopAnimation", 25}, {"ResetAnimation", 26}, {"InvalidateAnimation", 27}, {"FogState", 28},
      {"Loop", 30}, {"If", 31}, {"Else", 32}, {"Elseif", 33}, {"Endif", 34}, {"Callback", 35}, {"FbfxColorFromTo", 36},
      {"FbfxCsinwaveFromTo", 37}, {"AnimVerbose", 39}, {"DetonateWeapon", 41}, {"PufferState", 42}};
  return t;
}

// A list of events as one blob: each a 12-byte header, then its data.
bool events(Json& j, const Def& def, const Value& list, const std::string& at, Out& o) {
  for (size_t i = 0; i < list.items.size(); ++i) {
    const Value& e = list.items[i];
    const std::string ea = at + "[" + std::to_string(i) + "]";
    const Value *start = j.get(e, ea, "start"), *data = j.get(e, ea, "data");
    if (!start || !data || !j.r.object(*data, ea + ".data")) return false;
    if (data->members.size() != 1) return j.r.fail(*data, ea + ".data", "not one event");
    const std::string& type = data->members[0].first;
    const auto t = event_types().find(type);
    if (t == event_types().end()) return j.r.fail(*data, ea + ".data", "not an event this format has: " + type);
    int offset = 1;
    float time = 0;
    if (start->kind != Value::Null) {
      if (!(j.name_of(*start, ea + ".start", "offset", {{"Animation", 1}, {"Sequence", 2}, {"Event", 3}}, &offset) &&
            j.f32(*start, ea + ".start", "time", &time)))
        return false;
    }
    const size_t head = o.b.size();
    o.u8(t->second), o.u8(static_cast<uint8_t>(offset)), o.u16(0), o.u32(0), o.f32(time);
    if (!event_data(j, def, type, data->members[0].second, ea + ".data." + type, o)) return false;
    const uint32_t size = static_cast<uint32_t>(o.b.size() - head);
    std::memcpy(o.b.data() + head + 4, &size, 4);
  }
  return true;
}

// A seq def's 0x40 record, then its events (the size patched in after them).
bool sequence(Json& j, const Def& def, const std::string& name, int seq_state, int reset_state, uint32_t ptr, const Value& evs,
              const std::string& at, Out& o, bool padded_name) {
  if (padded_name) o.name_padded(name, 32);
  else o.name_padded("RESET_SEQUENCE", 32);
  o.u8(static_cast<uint8_t>(seq_state)), o.u8(static_cast<uint8_t>(reset_state)), o.u16(0);
  o.f32(0), o.f32(0), o.f32(0), o.u32(0), o.u32(0), o.u32(ptr);
  const size_t size_at = o.b.size();
  o.u32(0);
  const size_t from = o.b.size();
  if (!events(j, def, evs, at, o)) return false;
  const uint32_t size = static_cast<uint32_t>(o.b.size() - from);
  std::memcpy(o.b.data() + size_at, &size, 4);
  return true;
}

uint32_t ptr_of(Json& j, const Value* ptrs, const char* key, bool present, bool* ok) {
  if (!ptrs || ptrs->kind == Value::Null) return present ? 0xffffffffu : 0u;
  uint32_t v = 0;
  if (!j.u32(*ptrs, "$.ptrs", key, &v)) *ok = false;
  return v;
}

bool anim_def(Json& j, const Value& d, const std::string& at, Out& o) {
  std::string name, anim_name, root_name;
  bool callbacks, auto_reset, local_only, proximity, active;
  int activation = 0;
  float health = 0;
  long long min_sat = 0;
  if (!(j.str(d, at, "name", &name, 31) && j.str(d, at, "anim_name", &anim_name, 31) && j.str(d, at, "anim_root_name", &root_name, 31) &&
        j.boolean(d, at, "has_callbacks", &callbacks) && j.boolean(d, at, "auto_reset_node_states", &auto_reset) &&
        j.boolean(d, at, "local_nodes_only", &local_only) && j.boolean(d, at, "proximity_damage", &proximity) &&
        j.boolean(d, at, "active", &active) &&
        j.name_of(d, at, "activation", {{"WeaponHit", 0}, {"CollideHit", 1}, {"WeaponOrCollideHit", 2}, {"OnCall", 3}, {"OnStartup", 4}}, &activation) &&
        j.f32(d, at, "health", &health) && j.i64(d, at, "activ_prereq_min_to_satisfy", 0, 255, &min_sat)))
    return false;
  if (!active) return j.r.fail(d, at + ".active", "false: MechWarrior 3's defs are all active");
  if (local_only) return j.r.fail(d, at + ".local_nodes_only", "true: not in MechWarrior 3");
  uint32_t flags = 0;
  float range[2] = {0, 0}, reset_time = -1;
  for (const char* key : {"network_log", "save_log"}) {
    const Value* v = j.get(d, at, key);
    if (!v) return false;
    if (v->kind == Value::Null) continue;
    if (v->kind != Value::Bool) return j.r.fail(*v, at + "." + key, "not true, false or null");
    const uint32_t set = std::strcmp(key, "network_log") == 0 ? 0x400 : 0x1000;
    flags |= set | (v->b ? set << 1 : 0);
  }
  const Value* ex = j.get(d, at, "execution");
  if (!ex) return false;
  if (ex->kind == Value::String && ex->text == "ByZone") flags |= 8;
  else if (ex->kind == Value::Object && ex->get("ByRange")) {
    const Value& br = *ex->get("ByRange");
    if (!(j.f32(br, at + ".execution.ByRange", "min", &range[0]) && j.f32(br, at + ".execution.ByRange", "max", &range[1]))) return false;
    flags |= 2;
  } else if (!(ex->kind == Value::String && ex->text == "None")) {
    return j.r.fail(*ex, at + ".execution", "not None, ByZone or ByRange");
  }
  const Value* rt = j.get(d, at, "reset_time");
  if (!rt) return false;
  if (rt->kind != Value::Null) {
    if (!j.r.f32(*rt, at + ".reset_time", &reset_time)) return false;
    flags |= 0x20;
  }
  if (callbacks) flags |= 0x10;
  if (auto_reset) flags |= 0x10000;
  if (proximity) flags |= 0x100000;
  Def def;
  bool has[8] = {};
  const char* const kLists[] = {"objects", "nodes", "lights", "puffers", "dynamic_sounds", "static_sounds", "activ_prereqs", "anim_refs"};
  const Value* lists[8];
  for (int k = 0; k < 8; ++k)
    if (!(lists[k] = j.list(d, at, kLists[k], &has[k]))) return false;
  bool effects = false;
  if (!j.list(d, at, "effects", &effects)) return false;
  if (effects) return j.r.fail(*j.get(d, at, "effects"), at + ".effects", "effects: not in MechWarrior 3");
  const auto names_of = [&](int k, std::vector<std::string>* out) {
    if (!has[k]) return true;
    for (size_t i = 0; i < lists[k]->items.size(); ++i) {
      std::string n;
      if (!j.str(lists[k]->items[i], at + "." + kLists[k] + "[" + std::to_string(i) + "]", "name", &n, 31)) return false;
      out->push_back(n);
    }
    return true;
  };
  if (!(names_of(1, &def.nodes) && names_of(2, &def.lights) && names_of(3, &def.puffers) && names_of(4, &def.dyn_sounds) &&
        names_of(5, &def.static_sounds)))
    return false;
  def.anim_refs = has[7] ? lists[7]->items.size() : 0;
  bool seqs_present = false;
  const Value* seqs = j.list(d, at, "sequences", &seqs_present);
  const Value* reset = j.get(d, at, "reset_state");
  const Value* ptrs = j.get(d, at, "ptrs");
  if (!seqs || !reset || !ptrs) return false;
  const auto count = [&](int k, bool plus_one) -> uint8_t {
    return has[k] ? static_cast<uint8_t>(lists[k]->items.size() + (plus_one ? 1 : 0)) : 0;
  };
  bool ok = true;
  // The record: names, pointers, flags and counts; the reset state's size patched after its events.
  o.name_padded(anim_name, 32);
  o.name_padded(name, 32);
  o.u32(ptr_of(j, ptrs, "anim_ptr", true, &ok));
  o.name_padded(root_name, 32);
  o.u32(ptr_of(j, ptrs, "anim_root_ptr", true, &ok));
  o.zeros(44);
  o.u32(flags), o.u8(0), o.u8(static_cast<uint8_t>(activation)), o.u8(4), o.u8(2);
  o.f32(range[0]), o.f32(range[1]), o.f32(reset_time), o.f32(0), o.f32(health), o.f32(health);
  o.zeros(16);
  o.u32(ptr_of(j, ptrs, "seq_defs_ptr", true, &ok));
  uint32_t reset_ptr = 0;
  if (reset->kind != Value::Null && !j.u32(*reset, at + ".reset_state", "pointer", &reset_ptr)) return false;
  const size_t record_reset = o.b.size();
  o.name_padded("RESET_SEQUENCE", 32);
  o.zeros(4 + 12 + 8);
  o.u32(reset_ptr), o.u32(0);  // the size patched below
  o.u8(static_cast<uint8_t>(seqs_present ? seqs->items.size() : 0));
  for (int k = 0; k < 6; ++k) o.u8(count(k, true));
  o.u8(0), o.u8(count(6, false)), o.u8(static_cast<uint8_t>(min_sat)), o.u8(count(7, false)), o.u8(0);
  const char* const kPtrs[] = {"objects_ptr", "nodes_ptr", "lights_ptr", "puffers_ptr", "dynamic_sounds_ptr", "static_sounds_ptr"};
  for (int k = 0; k < 6; ++k) o.u32(ptr_of(j, ptrs, kPtrs[k], has[k], &ok));
  o.u32(0);
  o.u32(ptr_of(j, ptrs, "activ_prereqs_ptr", has[6], &ok));
  o.u32(ptr_of(j, ptrs, "anim_refs_ptr", has[7], &ok));
  o.u32(0);
  if (!ok) return false;
  // The lists, each but the prerequisites and refs after a blank first record.
  if (has[0]) {
    o.zeros(0x60);
    for (size_t i = 0; i < lists[0]->items.size(); ++i) {
      const Value& v = lists[0]->items[i];
      const std::string a = at + ".objects[" + std::to_string(i) + "]";
      std::string n;
      uint32_t f = 0, fm = 0, p = 0;
      std::vector<uint8_t> affine;
      const Value *pv = j.get(v, a, "ptr"), *fmv = j.get(v, a, "flags_merged");
      if (!(j.str(v, a, "name", &n, 31) && pv && fmv && j.u32(v, a, "flags", &f) && j.bytes(v, a, "affine", &affine))) return false;
      if (pv->kind != Value::Null && !j.u32(v, a, "ptr", &p)) return false;
      if (fmv->kind != Value::Null && !j.u32(v, a, "flags_merged", &fm)) return false;
      if (affine.size() != 48) return j.r.fail(v, a + ".affine", "not 48 bytes");
      o.name_node(n, 32), o.u32(0), o.u32(p), o.u32(f), o.u32(fm), o.raw(affine.data(), 48);
    }
  }
  if (has[1]) {
    o.zeros(0x28);
    for (size_t i = 0; i < lists[1]->items.size(); ++i) {
      uint32_t p = 0;
      if (!j.u32(lists[1]->items[i], at + ".nodes[" + std::to_string(i) + "]", "ptr", &p)) return false;
      o.name_node(def.nodes[i], 32), o.u32(0), o.u32(p);
    }
  }
  for (int k : {2, 3, 4}) {
    if (!has[k]) continue;
    o.zeros(0x2c);
    for (size_t i = 0; i < lists[k]->items.size(); ++i) {
      const Value& v = lists[k]->items[i];
      const std::string a = at + "." + kLists[k] + "[" + std::to_string(i) + "]";
      uint32_t p = 0, f = 0;
      if (!j.u32(v, a, "ptr", &p)) return false;
      if (k == 3) {
        long long pf;
        if (!j.i64(v, a, "flags", 0, 255, &pf)) return false;
        f = static_cast<uint32_t>(pf) << 24;
        o.name_padded(def.puffers[i], 32);
      } else {
        o.name_node(k == 2 ? def.lights[i] : def.dyn_sounds[i], 32);
      }
      o.u32(f), o.u32(p), o.u32(0);
    }
  }
  if (has[5]) {
    o.zeros(0x24);
    for (size_t i = 0; i < lists[5]->items.size(); ++i) {
      std::vector<uint8_t> pad;
      if (!j.bytes(lists[5]->items[i], at + ".static_sounds[" + std::to_string(i) + "]", "pad", &pad)) return false;
      o.name_garbage(def.static_sounds[i], pad, 32), o.u32(0);
    }
  }
  if (has[6]) {
    for (size_t i = 0; i < lists[6]->items.size(); ++i) {
      const Value& v = lists[6]->items[i];
      const std::string a = at + ".activ_prereqs[" + std::to_string(i) + "]";
      if (!j.r.object(v, a) || v.members.size() != 1) return j.r.fail(v, a, "not one Animation, Object or Parent");
      const std::string& kind = v.members[0].first;
      const Value& p = v.members[0].second;
      const std::string pa = a + "." + kind;
      std::string n;
      bool required = false;
      if (!(j.str(p, pa, "name", &n, 31) && j.boolean(p, pa, "required", &required))) return false;
      o.u32(required ? 0 : 1);
      if (kind == "Animation") {
        o.u32(1), o.name_padded(n, 32), o.u32(0), o.u32(0);
      } else if (kind == "Object" || kind == "Parent") {
        bool act = false;
        uint32_t ptr = 0;
        if (!(j.boolean(p, pa, "active", &act) && j.u32(p, pa, "ptr", &ptr))) return false;
        o.u32(kind == "Object" ? 2 : 3), o.u32(act ? 1 : 0), o.name_padded(n, 32), o.u32(ptr);
      } else {
        return j.r.fail(v, a, "not Animation, Object or Parent");
      }
    }
  }
  if (has[7]) {
    for (size_t i = 0; i < lists[7]->items.size(); ++i) {
      const Value& v = lists[7]->items[i];
      const std::string a = at + ".anim_refs[" + std::to_string(i) + "]";
      if (!j.r.object(v, a) || v.members.size() != 1) return j.r.fail(v, a, "not one CallAnimation or CallObjectConnector");
      const std::string& kind = v.members[0].first;
      const Value& p = v.members[0].second;
      const std::string pa = a + "." + kind;
      std::string n;
      std::vector<uint8_t> pad;
      if (!(j.str(p, pa, "name", &n, 31) && j.bytes(p, pa, "name_pad", &pad))) return false;
      if (kind == "CallAnimation") {
        o.name_garbage(n, pad, 32), o.zeros(32), o.u32(0);
      } else if (kind == "CallObjectConnector") {
        std::string ln;
        std::vector<uint8_t> lpad;
        if (!(j.str(p, pa, "local_name", &ln, 31) && j.bytes(p, pa, "local_name_pad", &lpad))) return false;
        o.name_garbage(n, pad, 32), o.name_garbage(ln, lpad, 32), o.u32(1);
      } else {
        return j.r.fail(v, a, "not CallAnimation or CallObjectConnector");
      }
      o.u32(0);
    }
  }
  // The reset state, its size into the def's own copy too, then each sequence.
  const size_t reset_at = o.b.size();
  if (reset->kind == Value::Null) {
    Value none;
    none.kind = Value::Array;
    if (!sequence(j, def, "", 0, 0, 0, none, at + ".reset_state.events", o, false)) return false;
  } else {
    const Value* evs = j.get(*reset, at + ".reset_state", "events");
    if (!evs || !j.r.array(*evs, at + ".reset_state.events") || !sequence(j, def, "", 0, 0, reset_ptr, *evs, at + ".reset_state.events", o, false))
      return false;
  }
  std::memcpy(o.b.data() + record_reset + 0x3c, o.b.data() + reset_at + 0x3c, 4);
  for (size_t i = 0; seqs_present && i < seqs->items.size(); ++i) {
    const Value& s = seqs->items[i];
    const std::string sa = at + ".sequences[" + std::to_string(i) + "]";
    std::string sn;
    int ss = 0, rs = 0;
    uint32_t sp = 0;
    const Value* evs;
    if (!(j.str(s, sa, "name", &sn, 31) && j.name_of(s, sa, "seq_state", {{"Initial", 0}, {"OnCall", 3}}, &ss) &&
          j.name_of(s, sa, "reset_state", {{"Initial", 0}, {"OnCall", 3}}, &rs) && j.u32(s, sa, "pointer", &sp) &&
          (evs = j.get(s, sa, "events")) && j.r.array(*evs, sa + ".events") && sequence(j, def, sn, ss, rs, sp, *evs, sa + ".events", o, true)))
      return false;
  }
  return true;
}

struct Mission {
  const char* name;
  uint32_t defs, world;
};
const Mission kMissions[] = {
    {"MwC1V10", 0x03213F1C, 0x0284000C}, {"MwC1V12", 0x03F5000C, 0x02B4000C}, {"MwC2V10", 0x032D17D4, 0x0284000C},
    {"MwC2V12", 0x0360E598, 0x02B4000C}, {"MwC2V12De", 0x036086CC, 0x02B4000C}, {"MwC3V10", 0x03A0ABE4, 0x02F0002C},
    {"MwC3V12", 0x03621248, 0x02B4000C}, {"MwC3V12De", 0x0361A508, 0x02B4000C}, {"MwC3V12DeP", 0x035E8A24, 0x02B4000C},
    {"MwC4V10", 0x032A8C00, 0x0284000C}, {"MwC4V12De", 0x036705B4, 0x02B4000C}, {"MwC4bV10", 0x0355000C, 0x0284000C},
    {"MwC4bV12De", 0x037EB6BC, 0x02B4000C}, {"MwT1V10", 0x02FA6600, 0x0284000C}, {"MwT1V12De", 0x030D9F94, 0x02B4000C},
    {"Unknown", 0xDEADBEEF, 0xDEADBEEF},
};

}  // namespace

bool anim_from_json(const std::function<std::string(const std::string&)>& file, std::vector<uint8_t>* zbd,
                    std::vector<std::pair<size_t, size_t>>* names, std::string* why) {
  Value meta;
  if (!zutl_json::parse(file("metadata.json"), &meta, why)) return *why = "metadata.json: " + *why, false;
  Json j;
  Out o;
  o.masks = names;
  std::string mission;
  float gravity = 0;
  bool lp = false, dp = false;
  const Value *list = nullptr, *defs = nullptr;
  if (!(j.str(meta, "$", "mission", &mission, 64) && j.f32(meta, "$", "gravity", &gravity) && (list = j.list(meta, "$", "anim_list", &lp)) &&
        (defs = j.list(meta, "$", "anim_def_names", &dp))))
    return *why = "metadata.json: " + j.r.why, false;
  const Mission* m = nullptr;
  for (const Mission& k : kMissions)
    if (mission == k.name) m = &k;
  if (!m) return j.r.fail(*meta.get("mission"), "$.mission", "not a MechWarrior 3 world mech3ax knows"), *why = "metadata.json: " + j.r.why, false;
  o.u32(0x08170616), o.u32(39);
  o.u32(lp ? static_cast<uint32_t>(list->items.size()) : 0);
  for (size_t i = 0; lp && i < list->items.size(); ++i) {
    const Value& f = list->items[i];
    const std::string a = "$.anim_list[" + std::to_string(i) + "]";
    std::string n, dt;
    std::vector<uint8_t> garbage;
    uint32_t ts = 0;
    if (!(j.str(f, a, "name", &n, 79) && j.str(f, a, "datetime", &dt, 64))) return *why = "metadata.json: " + j.r.why, false;
    if (const Value* g = f.get("garbage"); g && !(g->kind == Value::String && base64(g->text, &garbage)))
      return j.r.fail(*g, a + ".garbage", "not base64 text"), *why = "metadata.json: " + j.r.why, false;
    if (!unix_time(dt, &ts)) return j.r.fail(f, a + ".datetime", "not a date and time"), *why = "metadata.json: " + j.r.why, false;
    o.name_garbage(n, garbage, 80), o.u32(ts);
  }
  const size_t ndefs = dp ? defs->items.size() : 0;
  o.u32(0), o.u32(0), o.u16(0), o.u16(static_cast<uint16_t>(ndefs + 1)), o.u32(m->defs), o.u32(0), o.u32(0), o.u32(m->world);
  o.f32(gravity), o.zeros(28), o.u32(1), o.u32(0);
  const size_t def0 = o.b.size();  // def 0: zeros but its activation, OnCall; then a blank reset state
  o.zeros(0x13c + 0x40);
  o.b[def0 + 0x99] = 3;
  for (size_t i = 0; i < ndefs; ++i) {
    std::string n;
    if (!j.r.game_string(defs->items[i], "$.anim_def_names[" + std::to_string(i) + "]", &n, 255)) return *why = "metadata.json: " + j.r.why, false;
    Value d;
    const std::string text = file(n + ".json");
    if (text.empty()) return *why = n + ".json: not there", false;
    if (!zutl_json::parse(text, &d, why)) return *why = n + ".json: " + *why, false;
    Json dj;
    Out one;
    one.masks = nullptr;
    const size_t base = o.b.size();
    std::vector<std::pair<size_t, size_t>> local;
    one.masks = &local;
    if (!anim_def(dj, d, "$", one)) return *zbd = std::move(o.b), *why = n + ".json: " + dj.r.why, false;  // the defs before it kept
    o.raw(one.b.data(), one.b.size());
    if (names)
      for (const auto& [a, b] : local) names->emplace_back(base + a, base + b);
  }
  *zbd = std::move(o.b);
  return true;
}

}  // namespace zeff
