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

// Mech3.exe 1.2 @ 0x004dad10. 105 has no netparams; 107 and 108 name the file
// when no name was read, the rest net_NN.
int NetHeader::write(std::FILE* f, const char* base) const {
  int n = std::fprintf(f, "  version ( %d )    ;;reader file version\n", version);
  const auto params = [&] {
    return std::fprintf(f, "netparams ( pos ( %6.1f %6.1f %6.1f ) )\n", pos[0], pos[1], pos[2]);
  };
  if (version == 0x69) return n + std::fprintf(f, "  name ( %s )\n", name ? name : net_name(net));
  if (version >= 0x6b && version <= 0x6c) {
    if (has_pos) n += params();
    return n + std::fprintf(f, "  name ( %s )\n", name ? name : base);
  }
  n += std::fprintf(f, "  name ( %s )\n", name ? name : net_name(net));
  if (has_pos) n += params();
  return n;
}

// Mech3.exe 1.2 @ 0x004da1d0
int NetFile::write_header(std::FILE* f) { return header.write(f, base_name()); }

// Mech3.exe 1.2 @ 0x004da7e0 -> 0x004db8d0. Version 105 ("BS style") keeps
// three links, padded with -1; the rest writes each property that differs
// from the defaults (0x005e8af4) on its own line, indented 14 spaces. The
// sound is never written, and a valveunion item has a comma (editor.md).
int NetFile::write_record(const NetNodeRec& r, std::FILE* f, const char* k) const {
  static const NetNodeRec d;
  const char* in = "              ";
  // fcomp's equal: C3, which an unordered (NaN) compare sets too.
  const auto same = [](float a, float b) { return !(a < b) && !(a > b); };
  const int links = static_cast<int>(r.links.size());
  if (header.version == 0x69) {
    int n = std::fprintf(f, "  node_%02d ( 12 ( %6.1f %6.1f %6.1f ) ( ", r.index, r.pos[0], r.pos[1], r.pos[2]);
    int m = links;
    if (m > 3) {
      report(0x311, "net %d %s has too many connections for a BS style net: %d > 3", header.net, k, m);
      m = 3;
    }
    for (int i = 0; i < m; ++i) n += std::fprintf(f, " %2d", r.links[static_cast<size_t>(i)]);
    for (int i = m; i < 3; ++i) n += std::fprintf(f, " %2d", -1);
    return n + std::fprintf(f, " ) )\n");
  }
  int n = 0;
  if (r.kind == 0)
    n = std::fprintf(f, "  node_%02d ( 12 ( %6.1f %6.1f %6.1f ) ( ", r.index, r.pos[0], r.pos[1], r.pos[2]);
  else if (r.kind == 1)
    n = std::fprintf(f, "  node_%02d ( ( ", r.index);
  for (int32_t l : r.links) n += std::fprintf(f, " %2d", l);
  n += std::fprintf(f, " )");
  const int head = n;
  if (r.name) n += std::fprintf(f, "\n%sname ( \"%s\" )", in, r.name);
  if (r.speed_on != d.speed_on || !same(r.speed, d.speed))
    n += std::fprintf(f, "\n%sspeed ( %d %6.3f )", in, r.speed_on, r.speed);
  if (r.wait_interval_on != d.wait_interval_on || !same(r.wait_interval, d.wait_interval))
    n += std::fprintf(f, "\n%swait_interval ( %d %6.3f )", in, r.wait_interval_on, r.wait_interval);
  if (r.wait_time_on != d.wait_time_on || !same(r.wait_time, d.wait_time))
    n += std::fprintf(f, "\n%swait_time ( %d %6.3f )", in, r.wait_time_on, r.wait_time);
  if (r.valve && (r.valve_on != d.valve_on || r.valve != d.valve || r.valve_has_value != d.valve_has_value ||
                  (r.valve_has_value && r.valve_value != d.valve_value))) {
    if (!r.valve_has_value) n += std::fprintf(f, "\n%svalve ( %d \"%s\" )", in, r.valve_on, r.valve->name);
    else n += std::fprintf(f, "\n%svalve ( %d \"%s\" %d )", in, r.valve_on, r.valve->name, r.valve_value);
  }
  if (r.canleave[0] != d.canleave[0] || r.canleave[1] != d.canleave[1])
    n += std::fprintf(f, "\n%scanleave ( %d %d )", in, r.canleave[0], r.canleave[1]);
  if (r.jump_on != d.jump_on || !same(r.jump, d.jump)) n += std::fprintf(f, "\n%sjump ( %d %6.3f )", in, r.jump_on, r.jump);
  if (r.scan_aim[0] != d.scan_aim[0] || r.scan_aim[1] != d.scan_aim[1])
    n += std::fprintf(f, "\n%sscan_aim ( %d %d )", in, r.scan_aim[0], r.scan_aim[1]);
  if (r.scan_time[0] != d.scan_time[0] || r.scan_time[1] != d.scan_time[1])
    n += std::fprintf(f, "\n%sscan_time ( %d %d )", in, r.scan_time[0], r.scan_time[1]);
  if (r.scan_hit[0] != d.scan_hit[0] || r.scan_hit[1] != d.scan_hit[1])
    n += std::fprintf(f, "\n%sscan_hit ( %d %d )", in, r.scan_hit[0], r.scan_hit[1]);
  const auto flag = [&](uint8_t set, uint8_t v, uint8_t dset, uint8_t dv, const char* what) {
    if (set != dset || v != dv) n += std::fprintf(f, "\n%s%s ( %d )", in, what, v != 0);
  };
  flag(r.hidden_set, r.hidden, d.hidden_set, d.hidden, "hidden");
  flag(r.fixed_set, r.fixed, d.fixed_set, d.fixed, "fixed");
  flag(r.autonext_set, r.autonext, d.autonext_set, d.autonext, "autonext");
  flag(r.visible_set, r.visible, d.visible_set, d.visible, "visible");
  // FIX (bug): the exe reads deploy's, mcbwait's and valve_assign's valves
  // through their pointers whatever they hold; a null one crashed it. Such a
  // line is left out (a node read from a file always has its valves).
  if ((r.deploy_set != d.deploy_set || r.deploy_valve_a != d.deploy_valve_a || r.deploy_a != d.deploy_a ||
       r.deploy_valve_b != d.deploy_valve_b || r.deploy_b != d.deploy_b || r.deploy_c != d.deploy_c) &&
      r.deploy_valve_a && r.deploy_valve_b)
    n += std::fprintf(f, "\n%sdeploy ( \"%s\" %d \"%s\" %d %d )", in, r.deploy_valve_a->name, r.deploy_a,
                      r.deploy_valve_b->name, r.deploy_b, r.deploy_c);
  if ((r.mcbwait_set != d.mcbwait_set || r.mcbwait_valve != d.mcbwait_valve || r.mcbwait_b != d.mcbwait_b ||
       !same(r.mcbwait_time, d.mcbwait_time)) &&
      r.mcbwait_valve)
    n += std::fprintf(f, "\n%smcbwait ( \"%s\" %d %6.3f )", in, r.mcbwait_valve->name, r.mcbwait_b, r.mcbwait_time);
  if (r.arrange_set) n += std::fprintf(f, "\n%sarrangenodes ( %6.3f )", in, r.arrange);
  if (r.backup != d.backup) n += std::fprintf(f, "\n%sbackup ( )", in);
  if (r.gate) n += std::fprintf(f, "\n%sgate ( \"%s\" )", in, r.gate);
  if (r.shunt != d.shunt) n += std::fprintf(f, "\n%sshunt ( )", in);
  if (r.valve_union) {
    n += std::fprintf(f, "\n%svalveunion (", in);
    for (const ValveCount& v : r.valve_union->valves)
      if (v.valve) n += std::fprintf(f, "\n%s  ( 1 \"%s\", %d )", in, v.valve->name, v.count);
    n += std::fprintf(f, "\n%s)", in);
  }
  if (r.valve_assign && r.valve_assign->valve)  // FIX (bug), as deploy's
    n += std::fprintf(f, "\n%svalve_assign ( \"%s\" %d )", in, r.valve_assign->valve->name, r.valve_assign->value);
  if (r.auxname) n += std::fprintf(f, "\n%sauxname ( \"%s\" )", in, r.auxname);
  if (n != head) n += std::fprintf(f, "\n%s", in);
  return n + std::fprintf(f, " )\n");
}

const char* net_localize(const char* s) { return localize(s); }

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
