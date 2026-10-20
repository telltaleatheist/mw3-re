#include "mech3/zrdrainet.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gamez/zerror/zerr.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_msg.h"

namespace mech3::ai {

namespace {
const char kFile[] = "C:\\zipper\\Mech3\\ZRdrAiNet.cpp";
const char kReserved[] = "mcbwaynet";  // [0x005e8d64]: the multiplayer waynet's name

template <class... A>
void report(int line, const char* fmt, A... args) {
  char msg[512];
  std::snprintf(msg, sizeof msg, fmt, args...);
  zerr::report(0x2000, kFile, line, msg);
}

char* dup(const char* s) { return s ? strdup(s) : nullptr; }

// A list item's value as the exe reads it: the 32-bit word, whatever the type.
// A string's or list's pointer bits have no meaning here; FIX (A): 0.
uint32_t raw(const zrdr::Node& n) {
  if (n.type == zrdr::Int) return static_cast<uint32_t>(n.i);
  if (n.type == zrdr::Float) {
    uint32_t u;
    std::memcpy(&u, &n.f, 4);
    return u;
  }
  return 0;
}
int32_t raw_i(const zrdr::Node& n) { return static_cast<int32_t>(raw(n)); }
float raw_f(const zrdr::Node& n) {
  const uint32_t u = raw(n);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
// FIX (bug): the exe takes any value as a string pointer; a non-string crashes it.
const char* str(const zrdr::Node& n) { return n.type == zrdr::String ? n.s : nullptr; }

const char* localize(const char* s) {
  return rdr_hooks().localize ? rdr_hooks().localize(s) : zutl::localize(s);
}
void* sound(const char* s) {
  return rdr_hooks().sound_find ? rdr_hooks().sound_find(s) : static_cast<void*>(zsnd::find(s));
}

// 0x004dacb0 through 0x004dac70: the first string item after `start` that
// begins with `prefix`; the count when none, -1 for no list.
int32_t find_prefixed(const zrdr::Node* list, const char* prefix, int32_t start) {
  if (!list || !prefix || list->type != zrdr::List || start < 0 || start >= list->list[0].i) return -1;
  const size_t n = std::strlen(prefix);
  const int32_t count = list->list[0].i;
  int32_t i = start + 1;
  for (; i < count; ++i) {
    const zrdr::Node& item = list->list[i];
    if (item.type == zrdr::String && std::strncmp(item.s, prefix, n) == 0) break;
  }
  return i;
}

// 0x004daee0: the non-negative values of a list, in order.
void read_links(NetNodeRec& rec, const zrdr::Node& node) {
  if (node.type != zrdr::List) return;  // FIX (bug): the exe reads any value as the list
  const zrdr::Node* l = node.list;
  const int32_t n = l[0].i - 1;
  int32_t keep = 0;
  for (int32_t k = 1; k <= n; ++k) keep += raw_i(l[k]) >= 0;
  rec.links.reserve(static_cast<size_t>(keep));
  for (int32_t k = 1; k <= n; ++k)
    if (raw_i(l[k]) >= 0) rec.links.push_back(raw_i(l[k]));
}

// The XX of node_XX (strtol from the sixth character).
int32_t node_index(const char* key) {
  if (!key) return 0;
  const size_t len = std::strlen(key);
  return static_cast<int32_t>(std::strtol(key + (len < 5 ? len : 5), nullptr, 10));  // FIX (bug): not past the end
}

zrdr::Node* item(zrdr::Node* found, int k) { return &found->list[k]; }
bool is_list(const zrdr::Node* n) { return n && n->type == zrdr::List; }

// Mech3.exe 1.2 @ 0x004db1b0. `found` is the node after each key; a key whose
// value is not a list is skipped (FIX (bug): the exe reads it as one).
void read_props(NetNodeRec& r, zrdr::Node* value) {
  zrdr::Node* f;
  if (is_list(f = zrdr::find(value, "speed"))) r.speed_on = raw_i(*item(f, 1)), r.speed = raw_f(*item(f, 2));
  if (is_list(f = zrdr::find(value, "canleave"))) r.canleave[0] = raw_i(*item(f, 1)), r.canleave[1] = raw_i(*item(f, 2));
  if (is_list(f = zrdr::find(value, "wait_interval")))
    r.wait_interval_on = raw_i(*item(f, 1)), r.wait_interval = raw_f(*item(f, 2));
  if (is_list(f = zrdr::find(value, "wait_time"))) r.wait_time_on = raw_i(*item(f, 1)), r.wait_time = raw_f(*item(f, 2));
  if (is_list(f = zrdr::find(value, "valve"))) {
    r.valve_on = raw_i(*item(f, 1));
    r.valve = valve_get(str(*item(f, 2)));
    if (f->list[0].i == 4) r.valve_has_value = 1, r.valve_value = raw_i(*item(f, 3));
  }
  const char* written = nullptr;
  if (is_list(f = zrdr::find(value, "name"))) {
    written = str(*item(f, 1));
    if (written) r.name = dup(localize(written));
  }
  if (is_list(f = zrdr::find(value, "auxname"))) written = str(*item(f, 1));
  if (written) r.auxname = dup(written);
  if (is_list(f = zrdr::find(value, "jump"))) r.jump_on = raw_i(*item(f, 1)), r.jump = raw_f(*item(f, 2));
  if (is_list(f = zrdr::find(value, "scan_aim"))) r.scan_aim[0] = raw_i(*item(f, 1)), r.scan_aim[1] = raw_i(*item(f, 2));
  if (is_list(f = zrdr::find(value, "scan_time"))) r.scan_time[0] = raw_i(*item(f, 1)), r.scan_time[1] = raw_i(*item(f, 2));
  if (is_list(f = zrdr::find(value, "scan_hit"))) r.scan_hit[0] = raw_i(*item(f, 1)), r.scan_hit[1] = raw_i(*item(f, 2));
  if (is_list(f = zrdr::find(value, "hidden"))) r.hidden_set = 1, r.hidden = raw(*item(f, 1)) != 0;
  if (is_list(f = zrdr::find(value, "fixed"))) r.fixed_set = 1, r.fixed = raw(*item(f, 1)) != 0;
  if (is_list(f = zrdr::find(value, "autonext"))) r.autonext_set = 1, r.autonext = raw(*item(f, 1)) != 0;
  if (is_list(f = zrdr::find(value, "visible"))) r.visible_set = 1, r.visible = raw(*item(f, 1)) != 0;
  if (is_list(f = zrdr::find(value, "deploy"))) {
    r.deploy_set = 1;
    r.deploy_valve_a = valve_get(str(*item(f, 1)));
    r.deploy_a = static_cast<uint8_t>(raw(*item(f, 2)));
    r.deploy_valve_b = valve_get(str(*item(f, 3)));
    r.deploy_b = static_cast<uint8_t>(raw(*item(f, 4)));
    r.deploy_c = static_cast<int16_t>(raw(*item(f, 5)));
  }
  if (is_list(f = zrdr::find(value, "mcbwait"))) {
    r.mcbwait_set = 1;
    r.mcbwait_valve = valve_get(str(*item(f, 1)));
    r.mcbwait_b = static_cast<uint8_t>(raw(*item(f, 2)));
    r.mcbwait_time = raw_f(*item(f, 3));
  }
  if (is_list(f = zrdr::find(value, "arrangenodes"))) r.arrange_set = 1, r.arrange = raw_f(*item(f, 1));
  if (is_list(f = zrdr::find(value, "sound"))) {
    const char* s = str(*item(f, 1));
    r.sound = s ? sound(s) : nullptr;  // FIX (bug): the exe looks up any value as a name
  }
  if (zrdr::find(value, "backup")) r.backup = 1;
  if (is_list(f = zrdr::find(value, "gate"))) r.gate = dup(str(*item(f, 1)));
  if (zrdr::find(value, "shunt")) r.shunt = 1;
  f = zrdr::find(value, "valveunion");
  if (is_list(f) && f->list[0].i > 1) {
    auto* u = new ValveUnion;  // 0x004d44b0(count - 1): reserved
    u->valves.reserve(static_cast<size_t>(f->list[0].i - 1));
    r.valve_union = u;
    for (int32_t k = 1; k < f->list[0].i; ++k) {
      const zrdr::Node* sub = item(f, k);
      if (!is_list(sub)) continue;  // FIX (bug)
      if (raw(sub->list[1]) == 0) continue;
      ValveCount vc;
      vc.valve = valve_get(str(sub->list[2]));
      vc.count = sub->list[0].i == 4 ? raw_i(sub->list[3]) : 1;
      u->valves.push_back(vc);
    }
  }
  if (is_list(f = zrdr::find(value, "valve_assign"))) {
    Valve* v = valve_get(str(*item(f, 1)));
    r.valve_assign = new ValveAssign{v, raw_i(*item(f, 2))};
  }
}
}  // namespace

// --- the header ----------------------------------------------------------

// Mech3.exe 1.2 @ 0x004bca20
NetHeader::~NetHeader() { std::free(name); }

// Mech3.exe 1.2 @ 0x004bca40. `first` is not copied.
NetHeader& NetHeader::operator=(const NetHeader& o) {
  std::free(name), name = nullptr;
  net = o.net, version = o.version, has_pos = o.has_pos;
  std::memcpy(pos, o.pos, sizeof pos);
  name = dup(o.name);
  return *this;
}

// Mech3.exe 1.2 @ 0x004bca90. Names are equal when both are the same
// pointer, else when both are set and strcmp says so; positions only count
// with has_pos (0x00404600 compares the three floats).
bool NetHeader::operator==(const NetHeader& o) const {
  if (net != o.net || version != o.version || has_pos != o.has_pos) return false;
  if (name != o.name && (!name || !o.name || std::strcmp(name, o.name) != 0)) return false;
  if (!has_pos) return true;
  return pos[0] == o.pos[0] && pos[1] == o.pos[1] && pos[2] == o.pos[2];
}

// Mech3.exe 1.2 @ 0x004da900. Every complaint is a report; none stops the read.
void NetHeader::parse(zrdr::Node* node, const char* base) {
  if (!node) return;
  first = 0;
  // A key whose value is not a list is passed over (FIX (bug): the exe reads it as one).
  zrdr::Node* f = zrdr::find(node, "version");
  if (f) {
    first += 2;
    if (is_list(f)) {
      version = raw_i(*item(f, 1));
      const char* tag = f->list[0].i > 2 ? str(*item(f, 2)) : nullptr;
      if (version < 0x69)
        report(0xfb, "%s has past version: %d will update on save", base, version);
      else if (version >= 0x6d)
        report(0xfd, "%s has future version: %d", base, version);
      if (version == 0x69 && tag) {
        if (std::strcmp(tag, "mech3") == 0)
          version = 0x6a;
        else
          report(0x106, "%s has incompatible version string: expected %s got %s", base, "mech3", tag);
      }
    }
  } else {
    report(0x10b, "%s has no version", base);
  }
  f = zrdr::find(node, "name");
  if (f) {
    first += 2;
    const char* n = is_list(f) ? str(*item(f, 1)) : nullptr;
    if (n && std::strcmp(n, kReserved) == 0) {
      report(0x116, "%s: ignoring attempt to assign reserved net name: %s", base, n);
    } else {
      std::free(name);
      name = dup(n);
    }
  }
  f = zrdr::find(node, "netparams");
  if (f) {
    first += 2;
    has_pos = 0;
    zrdr::Node* p = zrdr::find(f, "pos");
    if (p && is_list(p)) {
      pos[0] = raw_f(*item(p, 1)), pos[1] = raw_f(*item(p, 2)), pos[2] = raw_f(*item(p, 3));
      has_pos = 1;
    }
  }
  switch (version) {
    case 0x69:
      if (name) {
        const char* expect = net_name(net);
        if (std::strcmp(expect, name) != 0)
          report(0x13b, "%s.zrd: name field (\"%s\") must be \"%s\" for this net version (%d)", base, name, expect,
                 version);
        if (std::strcmp(expect, base) != 0)
          report(0x13e, "%s.zrd: file name must be \"%s.zrd\" for this net version (%d)", base, expect, version);
      }
      break;
    case 0x6b:
      if (!name) report(0x144, "%s.zrd: name field must be present for this net version (%d)", base, 0x6b);
      break;
    case 0x6c:
      if (!name)
        report(0x149, "%s.zrd: name field must be present and set to \"%s\" for this net version (%d)", base, base,
               0x6c);
      break;
    default:
      break;
  }
  first = find_prefixed(node, "node_", 0) - 1;
}

// --- the file ------------------------------------------------------------

NetFile::NetFile(int32_t net, const char* n) : RdrFile(n, 0x80, 100, 0, 1) { header.net = net; }

// Mech3.exe 1.2 @ 0x004da840. The cached records (+0xe4) are not freed here.
NetFile::~NetFile() { std::free(base); }

// Mech3.exe 1.2 @ 0x004de090
int32_t NetFile::unload() {
  clear_cache();
  return RdrFile::unload();
}

// Mech3.exe 1.2 @ 0x004de010 -> 0x004da320: each cached record's deleting
// destructor, then the map emptied.
void NetFile::clear_cache() {
  if (cache.empty()) return;
  for (auto& [k, rec] : cache) delete rec;
  cache.clear();
}

// Mech3.exe 1.2 @ 0x004da060
const char* NetFile::base_name() {
  if (!base) {
    const char* dot = std::strrchr(name, '.');
    const size_t n = dot ? static_cast<size_t>(dot - name) : std::strlen(name);
    base = static_cast<char*>(std::malloc(n + 1));
    if (base) std::memcpy(base, name, n), base[n] = 0;
  }
  return base;
}

// Mech3.exe 1.2 @ 0x004da0d0. FIX (bug): the exe takes the tree as a list.
void NetFile::parse() {
  header.parse(tree->type == zrdr::List ? &tree->list[1] : nullptr, base_name());
  first = header.first;
}

// Mech3.exe 1.2 @ 0x004db850 (through the slot +0x18, 0x004da800)
NetNodeRec* NetFile::read_node(NetNodeRec* rec, zrdr::Node* v, const char* k) {
  if (!v) return nullptr;
  rec->reset();
  if (v->type != zrdr::List) return rec;  // FIX (bug): the exe reads it as a list
  const int32_t type = v->list[1].type;
  if (type == zrdr::Int) {  // 0x004dae40: (12 (x y z) (links) props...)
    rec->kind = 0;
    rec->index = node_index(k);
    zrdr::Node& p = v->list[2];
    if (p.type == zrdr::List) {  // FIX (bug)
      rec->pos[0] = raw_f(p.list[1]), rec->pos[1] = raw_f(p.list[2]), rec->pos[2] = raw_f(p.list[3]);
    }
    rec->links.clear();
    if (v->list[0].i >= 4) read_links(*rec, v->list[3]);
    if (v->list[0].i > 4) read_props(*rec, v);
  } else if (type == zrdr::List) {  // 0x004db140: ((links) props...)
    rec->kind = 1;
    rec->index = node_index(k);
    rec->links.clear();
    read_links(*rec, v->list[1]);
    read_props(*rec, v);
  }
  return rec;
}

// Mech3.exe 1.2 @ 0x004de730. Its third argument is never read.
NetNodeRec* NetFile::node(int32_t i, const char** out) {
  value = record(i, &key);
  *out = key;
  if (!value) return nullptr;
  auto it = key ? cache.find(key) : cache.end();
  if (it != cache.end()) return current = it->second;
  return current = read_node(&scratch, value, key);
}

// Mech3.exe 1.2 @ 0x004dc690
NetNodeRec* NetFile::node_raw(int32_t i, const char** out) {
  value = record(i, &key);
  *out = key;
  if (!value) return nullptr;
  return current = read_node(&scratch, value, key);
}

// Mech3.exe 1.2 @ 0x004de830. The key is found anywhere in the tree.
NetNodeRec* NetFile::node_named(const char* k) {
  current = nullptr;
  auto it = k ? cache.find(k) : cache.end();
  if (it != cache.end()) return current = it->second;
  key = nullptr;
  value = find(k);
  return current = value ? read_node(&scratch, value, k) : nullptr;
}

// --- the cache by name ---------------------------------------------------

std::map<std::string, NetFile*>& net_files() {
  static std::map<std::string, NetFile*> files;  // 0x0060b5a8
  return files;
}

void net_files_clear() {
  for (auto& [k, f] : net_files()) {
    if (f && f->tree) f->unload();
    delete f;
  }
  net_files().clear();
}

// Mech3.exe 1.2 @ 0x004d5280
const char* net_name(int32_t net) {
  static char buf[32];  // 0x0060b530
  std::snprintf(buf, sizeof buf, "net_%02d", net);
  return buf;
}

// Mech3.exe 1.2 @ 0x004da570
NetRef::NetRef(int32_t net, const char* n) {
  const std::string k = n ? n : net_name(net);
  NetFile*& slot = net_files()[k];
  if (!slot) slot = new NetFile(net, k.c_str());
  file = slot;
  file->add_ref();
}

// Mech3.exe 1.2 @ 0x004da8e0
NetRef::~NetRef() {
  if (file) file->release();
}

}  // namespace mech3::ai
