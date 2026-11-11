#include "upgrades/mods/mod_warning.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zreader/zreader.h"
#include "upgrades/reference/reference.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>

#include "crt/msvc_math.h"
#include "crt/msvc_string.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "gamez/zutil/zutl_zip.h"

namespace zrdr {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zReader\\zreader.cpp";
State g_state;
char g_fname[256];  // 0x0072bfa0, also holds name + extension
char g_ext[256];    // 0x0072be90

uint32_t read4(std::FILE* f, void* out) { return static_cast<uint32_t>(std::fread(out, 1, 4, f)); }

// The inline strcmp the compiler emitted in both searches: 0 when equal.
bool same(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
}  // namespace

State& state() { return g_state; }

// Mech3.exe 1.2 @ 0x005564f0: malloc(count nodes), first node's type set.
Node* new_nodes(int32_t first_type, int32_t count) {
  Node* n = static_cast<Node*>(std::malloc(static_cast<size_t>(count) * sizeof(Node)));
  n->type = first_type;
  return n;
}

// FIX (A): one spare zeroed slot after every list, so the original's reads one
// past the end mean "no value" (docs/fixes.md, docs/zreader.md).
Node* alloc_list(int32_t count) {
  int32_t slots = count < 1 ? 1 : count;
  Node* list = static_cast<Node*>(std::malloc(static_cast<size_t>(slots + 1) * sizeof(Node)));
  std::memset(&list[slots], 0, sizeof(Node));
  return list;
}

// Ours (docs/modding.md, M3): an archive entry read with its own end known.
// A list longer than the bytes left could hold (every node is at least 8),
// a node of no type, or lists nested past 64 deep mark the entry broken
// (the original read on into whatever followed, or allocated what the count
// said); shipped entries read exactly as before.
namespace {
// Ours, while an archive's entry is read (load): where the archive's own data ends (its table's start). -1: a
// bare file, the original's read with no bound at all (the differential test's, the text files').
long g_data_end = -1;
int g_depth = 0;
bool g_broken = false;  // a count, a string or nesting no file could hold: the entry is not read
bool g_short = false;   // the archive's data ran out under a list: its items from there on are empty ones
bool g_odd = false;     // a word that is no node type was met (read on, as the original)
// Ours: the longest list taken on trust when its data is not all there. The longest shipped list has 3,251 items
// (c3's reg_ionet_c3.zrd; Pirate's Moon's longest 409); a count past this that the data cannot hold is a hand
// edit gone wrong, not a list. And the nesting limit of 64 used below: the deepest shipped entry nests 10
// (c1's objectives.zrd), so 64 is far past any real file and only stops a runaway.
constexpr int32_t kShortListMost = 4096;
bool room(std::FILE* file, long bytes) { return g_data_end < 0 || std::ftell(file) + bytes <= g_data_end; }

// Ours, the graceful read (FIX (bug), `fixed`): what a hand-edited entry gets instead of the exe's read of
// whatever follows. A list that runs past its entry into the next entries' nodes is read on (the exe's way,
// and those maps work); at the first thing that is no node (a word of no type, a count or a string no file
// could hold, the archive's data running out, nesting past the limit) the reading stops for the whole entry:
// every node read before it is kept and each open list ends there.
enum NoteKind { kNone, kRanOut, kBadWord, kBadCount, kBadString, kNesting };
struct Stop {
  NoteKind kind = kNone;
  long at = 0;          // bytes into the entry
  int32_t value = 0;    // the word, the count or the string's length
  long list_at = -1;    // the list it cut short: where, what it said, what it holds
  int32_t said = 0, held = 0;
};
struct Tail {
  bool is = false;      // a list's items ran past its entry's end
  bool read = false;    // and a node was read from there (the entries after it)
  long list_at = 0;
  int32_t said = 0, held = 0;
};
long g_entry_start = 0, g_entry_end = -1;
bool g_stop = false, g_drop = false;
Stop g_stopped;
Tail g_tail;
std::vector<mech3::ModWarning> g_notes;

void stop(NoteKind kind, long at, int32_t value) {
  if (!g_stop) g_stopped = Stop{kind, at - g_entry_start, value, -1, 0, 0};
  g_stop = true;
  g_drop = true;  // what was being read is no node: its list ends before it
}

uint32_t read_graceful(std::FILE* file, Node* node) {
  const long at = std::ftell(file);
  node->type = Int, node->i = 0;
  if (g_stop || !room(file, 4)) {
    stop(kRanOut, at, 0);
    return 0;
  }
  int32_t type = 0;
  uint32_t total = read4(file, &type);
  switch (type) {
    case Int:
    case Float:
      if (!room(file, 4)) return stop(kRanOut, at, 0), total;
      node->type = type;
      return total + (type == Int ? read4(file, &node->i) : read4(file, &node->f));
    case String: {
      int32_t length = -1;
      if (!room(file, 4)) return stop(kRanOut, at, 0), total;
      read4(file, &length);
      if (length < 0 || !room(file, length)) return stop(kBadString, at, length), total;
      std::fseek(file, at + 4, SEEK_SET);
      node->type = String;
      return zutl_zar::read_string(file, &node->s) + total;
    }
    case List: {
      int32_t count = 0;
      if (!room(file, 4)) return stop(kRanOut, at, 0), total;
      total += read4(file, &count);
      const long left = g_data_end - std::ftell(file);
      if (g_depth >= 64) return stop(kNesting, at, count), total;
      if (count < 0 || (static_cast<long>(count) - 1 > left / 8 && count > kShortListMost)) return stop(kBadCount, at, count), total;
      node->type = List;
      node->list = alloc_list(count);
      node->list[0].type = Int;
      int32_t have = 1;
      ++g_depth;
      for (int32_t i = 1; i < count; ++i) {
        // This list's items have run past its own entry: the entries after it are its tail.
        const bool beyond = std::ftell(file) >= g_entry_end;
        if (!g_tail.is && beyond) g_tail = Tail{true, false, at - g_entry_start, count - 1, i - 1};
        g_drop = false;
        total += read_graceful(file, &node->list[i]);
        if (beyond && !g_drop) g_tail.read = true;
        if (g_drop) {  // no node there: the list ends before it
          if (g_stopped.list_at < 0) g_stopped.list_at = at - g_entry_start, g_stopped.said = count - 1, g_stopped.held = i - 1;
          break;
        }
        have = i + 1;  // (a list inside that ended early is kept as it is; the next item here is then none)
      }
      --g_depth;
      g_drop = false;
      node->list[0].i = have;
      return total;
    }
    default:
      zerr::report(0x400, kFile, 0x4b4, "Invalid reader node type in zRdrReadNode");
      return stop(kBadWord, at, type), 0;
  }
}

std::string plural(int32_t n, const char* one) { return std::to_string(n) + " " + one + (n == 1 ? "" : "s"); }

// The entry's notes (mod_warning.h's four parts, the `where` the caller's): what is wrong and where, what was
// done, and what the exe's 0x00557180 does with the same bytes.
void make_notes(bool none) {
  g_notes.clear();
  char b[320];
  const Stop& s = g_stopped;
  const bool tail = g_tail.is && g_tail.read;
  if (tail) {
    mech3::ModWarning w;
    std::snprintf(b, sizeof b, "its list %ld bytes in says %s and the entry holds %d", g_tail.list_at, plural(g_tail.said, "item").c_str(),
                  g_tail.held);
    w.wrong = b;
    w.did = "read the " + std::to_string(g_tail.held) + ", and the entry after it in the archive as the rest";
    w.original = "does the same (it reads on into the next entry), so the file works there, by luck: the extra items are the next "
                 "entry's data";
    g_notes.push_back(w);
  }
  if (!g_stop) return;
  mech3::ModWarning w;
  switch (s.kind) {
    case kBadWord:
      std::snprintf(b, sizeof b, "a value that is not a node type (%d; the types are 1 to 4) %ld bytes in", s.value, s.at);
      w.original = "keeps the value as a node of no kind and reads on from the next four bytes, out of step (seen in the emulator): "
                   "what it reads after that is chance, and it can crash when this entry is loaded";
      break;
    case kBadCount:
      std::snprintf(b, sizeof b, "a list count of %d, %ld bytes in, that no file could hold", s.value, s.at);
      w.original = "asks for memory for that many items, is refused, and writes through the empty answer: a likely crash when this "
                   "entry is loaded (seen in the emulator)";
      break;
    case kBadString:
      std::snprintf(b, sizeof b, "a text said to be %d bytes long, %ld bytes in, which runs past the archive's data", s.value, s.at);
      w.original = "asks for that much memory and reads to the end of the file; when the memory is refused (seen in the emulator "
                   "for 2 GB) it writes through the empty answer: a likely crash when this entry is loaded";
      break;
    case kNesting:
      std::snprintf(b, sizeof b, "lists nested more than 64 deep, %ld bytes in", s.at);
      w.original = "has no limit and reads them all";
      break;
    default:
      std::snprintf(b, sizeof b, "the archive's data ends %ld bytes in, before the entry's lists are full", s.at);
      w.original = "reads on through the archive's index and past the end of the file, taking whatever is there as items: it "
                   "usually gets by, and can crash";
      break;
  }
  w.wrong = b;
  if (tail && s.list_at == g_tail.list_at) {
    w.wrong += " (the same list, still short of its items)";
  } else if (s.list_at >= 0) {
    std::snprintf(b, sizeof b, " (its list %ld bytes in says %s and holds %d)", s.list_at, plural(s.said, "item").c_str(), s.held);
    w.wrong += b;
  }
  w.did = none ? "nothing of the entry can be read, so it is treated as not there" : "read what comes before it; the rest is left out";
  g_notes.push_back(w);
}
}  // namespace

const std::vector<mech3::ModWarning>& load_notes() { return g_notes; }

// Mech3.exe 1.2 @ 0x00557180. Returns the bytes read.
// The exe reads nodes from the file until every list's count is met and knows no end: an entry whose list
// says more items than the entry holds (the community's hand-made mpinfo.zrd and aiv.zrd entries do) takes
// the entries after it as its tail, and loads. Ours: the same, as far as the archive's own data goes. Where
// that runs out (the exe reads on through the table and past the file's end, into unset memory) the items
// still owed are empty ones (Int 0) and the entry is kept; a count, a string or nesting no file could hold
// makes the entry broken, not read (fixes.md).
uint32_t read_node(std::FILE* file, Node* node) {
  if (g_data_end >= 0 && zfix::options().fixed) return read_graceful(file, node);
  if (g_data_end >= 0 && (g_broken || !room(file, 4))) {
    if (!g_broken) g_short = true;
    node->type = Int, node->i = 0;
    return 0;
  }
  uint32_t total = read4(file, &node->type);
  switch (node->type) {
    case Int:
    case Float:
      if (!room(file, 4)) {  // its value past the data: an empty one
        g_short = true;
        node->type = Int, node->i = 0;
        return total;
      }
      return total + (node->type == Int ? read4(file, &node->i) : read4(file, &node->f));
    case String:
      if (g_data_end >= 0) {  // its length first: the string whole inside the archive's data
        const long at = std::ftell(file);
        int32_t length = -1;
        if (room(file, 4)) read4(file, &length);
        if (length < 0 || !room(file, length)) {
          g_broken = true;
          node->type = Int, node->i = 0;
          return total;
        }
        std::fseek(file, at, SEEK_SET);
      }
      return zutl_zar::read_string(file, &node->s) + total;
    case List: {
      int32_t count = 0;
      if (!room(file, 4)) {
        g_short = true;
        node->type = Int, node->i = 0;
        return total;
      }
      total += read4(file, &count);
      if (g_data_end >= 0) {
        const long left = g_data_end - std::ftell(file);
        // A count the data left cannot hold: a small one is a list short of its items (they are read as far
        // as the data goes, the rest empty); a large one no file could hold, and is not allocated.
        const bool fits = static_cast<long>(count) - 1 <= left / 8;
        if (count < 0 || (!fits && count > kShortListMost) || g_depth >= 64) {
          g_broken = true;
          count = 0;
        } else if (!fits) {
          g_short = true;
        }
      }
      node->list = alloc_list(count);
      node->list[0].i = count;
      node->list[0].type = Int;
      ++g_depth;
      for (int32_t i = 1; i < node->list[0].i; ++i) total += read_node(file, &node->list[i]);
      --g_depth;
      return total;
    }
    default:
      zerr::report(0x400, kFile, 0x4b4, "Invalid reader node type in zRdrReadNode");
      // The exe returns with the word left as the node's type and its value whatever malloc gave, and the
      // list's loop reads on from the next word. (With `fixed` an archive's entry never gets here: read_graceful.)
      if (g_data_end >= 0) g_odd = true;
      return 0;
  }
}

// Mech3.exe 1.2 @ 0x00556840. Walks slot 0 too; it is an Int, so nothing happens.
void free_contents(Node* node) {
  upgrades::reference::freed(node);  // ours: the reference test's observer (null in play)
  if (node->type == String) {
    std::free(node->s);
    node->s = nullptr;
    return;
  }
  if (node->type == List) {
    for (int32_t i = 0; i < node->list[0].i; ++i) free_contents(&node->list[i]);
    std::free(node->list);
    node->list = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x00556820
int free_node(Node* node) {
  if (node) {
    free_contents(node);
    std::free(node);
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00556930. Depth first, values match as keys, and a key in the
// last slot returns the slot past the end: all original (docs/zreader.md).
Node* find(Node* node, const char* key, int start) {
  if (!node || node->type != List || node->list[0].i <= start) return nullptr;
  for (int i = start; i < node->list[0].i; ++i) {
    Node* item = &node->list[i];
    if (item->type == List) {
      if (Node* hit = find(item, key, 1)) return hit;
    } else if (item->type == String && same(item->s, key)) {
      return item + 1;
    }
  }
  return nullptr;
}

// Mech3.exe 1.2 @ 0x005569e0
Node* find(Node* node, const char* key) {
  upgrades::reference::seen(key, node);  // ours: the modding reference's test observer (null in play)
  return find(node, key, 1);
}

// Mech3.exe 1.2 @ 0x005568a0
Node* find_flat(Node* node, const char* key, int start) {
  upgrades::reference::seen(key, node);  // ours: the modding reference's test observer (null in play)
  if (!node || node->type != List) return nullptr;
  int32_t count = node->list[0].i;
  for (int i = start; i < count; ++i) {
    Node* item = &node->list[i];
    if (item->type == String && same(item->s, key)) return item + 1;
  }
  return nullptr;
}

namespace {
// The shared tail of 0x005569f0 and 0x00556a20.
const char* string_of(const Node* v) {
  if (v) {
    if (v->type == String) return v->s;
    if (v->type == List && v->list[1].type == String) return v->list[1].s;
  }
  return nullptr;
}
}  // namespace

// Mech3.exe 1.2 @ 0x005569f0
const char* get_string(Node* node, const char* key) { return string_of(find(node, key)); }

// Mech3.exe 1.2 @ 0x00556a20
const char* get_string_flat(Node* node, const char* key, int start) {
  return string_of(find_flat(node, key, start));
}

// Mech3.exe 1.2 @ 0x00556a60. A Float is copied as raw bits (mov, not fld/fstp,
// so NaNs pass through unchanged); an Int is converted (fild).
bool get_float(Node* node, const char* key, float* out) {
  Node* v = find(node, key);
  if (!v) return false;
  if (v->type == Float) {
    std::memcpy(out, &v->f, 4);
    return true;
  }
  if (v->type == Int) {
    *out = static_cast<float>(v->i);
    return true;
  }
  if (v->type == List) {
    if (v->list[1].type == Float) {
      std::memcpy(out, &v->list[1].f, 4);
      return true;
    }
    if (v->list[1].type == Int) {
      *out = static_cast<float>(v->list[1].i);
      return true;
    }
  }
  return false;
}

// Mech3.exe 1.2 @ 0x00556ae0
bool get_int(Node* node, const char* key, int32_t* out) {
  Node* v = find(node, key);
  if (!v) return false;
  if (v->type == Int) {
    *out = v->i;
    return true;
  }
  if (v->type == List && v->list[1].type == Int) {
    *out = v->list[1].i;
    return true;
  }
  return false;
}

// Mech3.exe 1.2 @ 0x00556b30: leaves *out alone when the key is missing.
void get_int_if(Node* node, const char* key, int32_t* out) {
  int32_t v;
  if (get_int(node, key, &v)) *out = v;
}

// Mech3.exe 1.2 @ 0x00556b50: the low byte of the int.
void get_byte_if(Node* node, const char* key, uint8_t* out) {
  int32_t v;
  if (get_int(node, key, &v)) *out = static_cast<uint8_t>(v & 0xff);
}

namespace {
void float_item(const Node& c, float* out) {
  if (c.type == Float) std::memcpy(out, &c.f, 4);
  else if (c.type == Int) *out = static_cast<float>(c.i);
}
void int_item(const Node& c, int32_t* out) {
  if (c.type == Float) *out = crt::msvc_ftol(c.f);
  else if (c.type == Int) *out = c.i;
}
bool short_list(const Node* n, int32_t slots) { return !n || n->type != List || n->list[0].i < slots; }
}  // namespace

// Mech3.exe 1.2 @ 0x00556b70: a list of at least three numbers. A component that
// is neither Int nor Float keeps its old value.
bool get_vec3(Node* list_node, float out[3]) {
  if (!list_node || list_node->type != List || list_node->list[0].i < 4) return false;
  for (int k = 0; k < 3; ++k) float_item(list_node->list[1 + k], &out[k]);
  return true;
}


// Mech3.exe 1.2 @ 0x00556c00
bool get_vec2(Node* list_node, float out[2]) {
  if (short_list(list_node, 3)) return false;
  for (int k = 0; k < 2; ++k) float_item(list_node->list[1 + k], &out[k]);
  return true;
}

// Mech3.exe 1.2 @ 0x00556c70
bool get_ivec2(Node* list_node, int32_t out[2]) {
  if (short_list(list_node, 3)) return false;
  for (int k = 0; k < 2; ++k) int_item(list_node->list[1 + k], &out[k]);
  return true;
}

// Mech3.exe 1.2 @ 0x00556cf0
bool get_ivec3(Node* list_node, int32_t out[3]) {
  if (short_list(list_node, 4)) return false;
  for (int k = 0; k < 3; ++k) int_item(list_node->list[1 + k], &out[k]);
  return true;
}

// Mech3.exe 1.2 @ 0x00556da0
bool get_bool_int(Node* node, int32_t* out) {
  int32_t scratch;
  if (!out) out = &scratch;
  if (!node) return false;
  if (node->type == List && node->list[0].i > 1) node = &node->list[1];
  if (node->type == Int) {
    *out = node->i != 0;
    return true;
  }
  if (node->type != String) return false;
  if (crt::msvc_stricmp(node->s, "true") == 0 || crt::msvc_stricmp(node->s, "on") == 0) {
    *out = 1;
    return true;
  }
  if (crt::msvc_stricmp(node->s, "false") == 0 || crt::msvc_stricmp(node->s, "off") == 0) {
    *out = 0;
    return true;
  }
  return false;
}

// Mech3.exe 1.2 @ 0x00556fe0. A Float is copied as raw bits; an int goes
// through fild / fstp.
bool get_float_value(Node* node, float* out) {
  if (!node) return false;
  if (node->type == List && node->list[0].i > 1) node = &node->list[1];
  if (node->type == Float) {
    std::memcpy(out, &node->f, 4);
    return true;
  }
  if (node->type != Int) return false;
  *out = static_cast<float>(node->i);
  return true;
}

// Mech3.exe 1.2 @ 0x00557030
bool get_string_value(Node* node, const char** out) {
  if (!node || !out) return false;
  if (node->type == List && node->list[0].i > 1) node = &node->list[1];
  if (node->type == Int) {
    if (node->i != 0) return false;
    *out = nullptr;
    return true;
  }
  if (node->type != String) return false;
  *out = node->s;
  return true;
}

// Mech3.exe 1.2 @ 0x00556e90
bool get_tristate(Node* node, int32_t* out) {
  int32_t scratch;
  if (!out) out = &scratch;
  if (!node) return false;
  if (node->type == List && node->list[0].i > 1) node = &node->list[1];
  if (node->type == Int) {
    *out = node->i >= 0 && node->i <= 2 ? node->i : 2;
    return true;
  }
  if (node->type != String) return false;
  if (crt::msvc_stricmp(node->s, "true") == 0 || crt::msvc_stricmp(node->s, "on") == 0) {
    *out = 1;
    return true;
  }
  for (const char* zero : {"false", "off", "none", "default"}) {
    if (crt::msvc_stricmp(node->s, zero) == 0) {
      *out = 0;
      return true;
    }
  }
  return false;
}

// Mech3.exe 1.2 @ 0x005570b0
bool get_vec3(Node* node, const char* key, float out[3]) { return get_vec3(find(node, key), out); }
// Mech3.exe 1.2 @ 0x005570d0
bool get_vec2(Node* node, const char* key, float out[2]) { return get_vec2(find(node, key), out); }
// Mech3.exe 1.2 @ 0x005570f0
bool get_ivec3(Node* node, const char* key, int32_t out[3]) { return get_ivec3(find(node, key), out); }
// Mech3.exe 1.2 @ 0x00557110
bool get_ivec2(Node* node, const char* key, int32_t out[2]) { return get_ivec2(find(node, key), out); }
// Mech3.exe 1.2 @ 0x00557130
bool get_bool_int(Node* node, const char* key, int32_t* out) { return get_bool_int(find(node, key), out); }

// Mech3.exe 1.2 @ 0x00557150
bool get_bool(Node* node, const char* key, bool* out) {
  int32_t v;
  if (!get_bool_int(node, key, &v)) return false;
  *out = v == 1;
  return true;
}

// Mech3.exe 1.2 @ 0x00556170. The original also sets up the search path for loose
// text files (0x005561a0), which belongs with the unported text reader.
void init() {
  if (!g_state.archives_created) {
    g_state.archives_created = true;
    g_state.archives.clear();
    g_state.current = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x00557320. Quirk kept: before init() there is no list, the
// append (0x00555f30) does nothing, and the open archive is simply lost.
bool open_archive(const char* path, bool make_current) {
  auto* a = new zutl_zar::Archive;
  zutl_zar::init(*a);
  if (zutl_zar::open(*a, path)) {
    if (make_current) g_state.current = a;
    if (g_state.archives_created) g_state.archives.push_back(a);
    return true;
  }
  zutl_zar::destroy(*a);
  delete a;
  return false;
}

// Mech3.exe 1.2 @ 0x005573d0. With all == false the current archive survives and
// goes back on the list, now last.
void close_archives(bool all) {
  if (!g_state.archives_created) return;
  std::vector<zutl_zar::Archive*> list;
  list.swap(g_state.archives);
  for (zutl_zar::Archive* a : list) {
    if (all || a != g_state.current) {
      if (g_state.current == a) g_state.current = nullptr;
      zutl_zar::close(*a);
      zutl_zar::destroy(*a);
      delete a;
    }
  }
  if (!all && g_state.current) g_state.archives.push_back(g_state.current);
}

// Mech3.exe 1.2 @ 0x00556210, at the app's end: the search paths freed
// (0x00576120; the port's are the mounts), every archive closed (a changed
// table written, none for the read-only .zbd files) and the list freed.
void shutdown() {
  close_archives(true);
  g_state.archives_created = false;  // [0x0072c1a0] = [0x0072c1a4] = 0
  g_state.archives.clear();
}

// Mech3.exe 1.2 @ 0x005572c0
std::FILE* find_in_archives(const char* name) {
  if (!g_state.archives_created) return nullptr;
  for (size_t i = 0; i < g_state.archives.size(); ++i) {
    zutl_zar::Archive* a = g_state.archives[i];
    if (std::FILE* f = zutl_zar::seek(*a, zutl_zar::find(*a, name), nullptr)) return f;
  }
  return nullptr;
}

namespace {
// Mech3.exe 1.2 @ 0x005563a0: one byte, false at the end. FIX (A): stdio for
// ReadFile on the file's handle.
bool read_char(std::FILE* f, char* c) {
  const int v = std::fgetc(f);
  if (v == EOF) return false;
  *c = static_cast<char>(v);
  return true;
}

// MSVCRT isspace in the C locale on a char passed sign-extended: bytes from
// 0x80 are not spaces.
bool space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

void too_long(const char* token) { std::fprintf(stderr, "token[%s] too long\n", token); }  // printf, 0x005ef940

// Mech3.exe 1.2 @ 0x005562a0: the parentheses outside strings and comments
// must balance; the file is rewound either way.
bool balanced(std::FILE* f) {
  int32_t depth = 0;
  bool comment = false, quote = false;
  char c;
  while (read_char(f, &c)) {
    switch (c) {
      case '\n':
      case '\r':
        if (!quote) comment = false;
        break;
      case '"':
        if (!comment) quote = !quote;
        break;
      case '(':
        if (!quote && !comment) ++depth;
        break;
      case ')':
        if (!quote && !comment) --depth;
        break;
      case ';':
        if (!quote) comment = true;
        break;
      default:
        break;
    }
  }
  std::fseek(f, 0, SEEK_SET);
  return depth == 0;
}

// The word after a '#' or an #ifdef (0x00557650, 0x005577d0): read up to a
// space, which is consumed. False at the end of the file or a word too long.
bool read_word(std::FILE* f, char* c, char (&word)[0x102]) {
  int32_t n = 0;
  word[0] = '\0';  // the original compares its stack as it was
  if (!read_char(f, c)) return false;
  while (!space(*c)) {
    if (n > 0xff) {
      too_long(word);
      return false;
    }
    word[n] = *c;
    word[n + 1] = '\0';
    ++n;
    if (!read_char(f, c)) return false;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x00557650: 0 ifdef, 1 else, 2 endif, 3 anything else.
int directive_word(std::FILE* f, char* c) {
  char w[0x102];
  if (!read_word(f, c, w)) return 3;
  if (std::strcmp(w, "ifdef") == 0) return 0;
  if (std::strcmp(w, "else") == 0) return 1;
  return std::strcmp(w, "endif") == 0 ? 2 : 3;
}

// Mech3.exe 1.2 @ 0x005577d0: the name after #ifdef is in the defines.
bool defined(std::FILE* f, char* c) {
  char w[0x102];
  if (!read_word(f, c, w)) return false;
  for (const std::string& d : g_state.defines)
    if (d == w) return true;
  return false;
}

// Reads on to the next '#' (looking at *c first); false at the end of the file.
bool skip_to_hash(std::FILE* f, char* c) {
  while (*c != '#')
    if (!read_char(f, c)) return false;
  return true;
}

// Mech3.exe 1.2 @ 0x00557550: a directive at the start of a token. A false
// #ifdef skips to its #else (true: the else branch is read) or its #endif;
// an #else reached while reading skips to the #endif. A '#' counts anywhere
// in the skipped text, in comments and strings too.
bool directive(std::FILE* f, char* c, bool skipping) {
  const int32_t depth = g_state.if_depth;
  switch (directive_word(f, c)) {
    case 0:
      ++g_state.if_depth;
      if (defined(f, c) || g_state.if_depth <= depth) return false;
      for (;;) {
        // FIX (bug): the original scans on forever when the file ends here.
        if (!skip_to_hash(f, c)) return false;
        if (directive(f, c, true)) return true;
        if (g_state.if_depth <= depth) return false;
      }
    case 1:
      if (skipping) return true;
      while (depth <= g_state.if_depth) {
        if (!skip_to_hash(f, c)) return false;  // FIX (bug), as above
        directive(f, c, false);
      }
      return false;
    case 2:
      --g_state.if_depth;
      return false;
    default:
      return false;
  }
}

// Mech3.exe 1.2 @ 0x00556690: an unquoted token that strtol or strtod reads
// whole is an int or a float; anything else a string.
Node* token_value(const char* s, bool quoted) {
  Node* n = new_nodes(String, 1);
  const size_t len = std::strlen(s);
  if (!quoted && len > 0) {
    const char* end = s;
    const int32_t i = crt::msvc_strtol(s, &end);
    if (static_cast<size_t>(end - s) == len) {
      n->type = Int;
      n->i = i;
      return n;
    }
    const double d = crt::msvc_strtod(s, &end);
    if (static_cast<size_t>(end - s) == len) {
      n->type = Float;
      n->f = static_cast<float>(d);
      return n;
    }
  }
  n->type = String;
  const size_t size = len + 1;
  n->s = static_cast<char*>(std::malloc(size));  // _strdup
  std::memcpy(n->s, s, size);
  return n;
}

// Mech3.exe 1.2 @ 0x00556510: the next token into *out (nullptr when there is
// none before the delimiter). Returns the delimiter, '(' ')' or a space, or 0
// at the end of the file, where a token in progress is dropped. A ';' comment
// runs to the end of its line without ending the token; quotes may sit in a
// token's middle.
int read_token(std::FILE* f, Node** out) {
  char buf[0x102];
  int32_t n = 0;
  bool quoted = false;
  char c;
  if (!read_char(f, &c)) return 0;
  for (;;) {
    if (c == ';') {
      do {
        if (!read_char(f, &c)) return 0;
      } while (c != '\n');
    } else {
      if (n == 0 && c == '#') directive(f, &c, false);
      if (c == '(' || c == ')' || space(c)) {
        *out = n ? token_value(buf, quoted) : nullptr;
        return c;
      }
      if (n >= 0x100) {
        *out = nullptr;
        too_long(buf);
        return 0;
      }
      if (c == '"') {
        quoted = true;
        // FIX (bug): the original reads on past the end of the file and past
        // its 256-byte buffer; the string stops at either.
        bool more = read_char(f, &c);
        while (more && c != '"') {
          if (n < 0xff) buf[n++] = c;
          more = read_char(f, &c);
        }
        buf[n++] = '\0';
      } else {
        buf[n++] = c;
        buf[n] = '\0';
      }
    }
    if (!read_char(f, &c)) return 0;
  }
}

// Mech3.exe 1.2 @ 0x005563f0: the items up to the ')' that closes the list
// (or the end of the file), in order, as a list node.
Node* parse_list(std::FILE* f) {
  std::vector<Node> items;
  Node* out = nullptr;
  for (;;) {
    const int r = read_token(f, &out);
    if (r == 0) break;
    if (out) {
      items.push_back(*out);
      std::free(out);
    }
    if (r == '(') {
      out = parse_list(f);
      items.push_back(*out);
      std::free(out);
    } else if (r == ')') {
      break;
    }
  }
  Node* node = new_nodes(List, 1);
  const int32_t count = static_cast<int32_t>(items.size()) + 1;
  node->list = alloc_list(count);
  node->list[0].type = Int;
  node->list[0].i = count;
  for (size_t i = 0; i < items.size(); ++i) node->list[i + 1] = items[i];
  return node;
}
}  // namespace

std::string text_error(std::FILE* f) {
  int32_t line = 1, quote_line = 0;
  std::vector<int32_t> open;  // the line of each '(' still open
  bool comment = false, quote = false, cr = false;
  int ch;
  std::string why;
  while (why.empty() && (ch = std::fgetc(f)) != EOF) {
    const char c = static_cast<char>(ch);
    if (c == '\n' && !cr) ++line;
    if (c == '\r') ++line;
    cr = c == '\r';
    switch (c) {
      case '\n':
      case '\r':
        if (!quote) comment = false;
        break;
      case '"':
        if (!comment) quote = !quote, quote_line = line;
        break;
      case '(':
        if (!quote && !comment) open.push_back(line);
        break;
      case ')':
        if (!quote && !comment) {
          if (open.empty()) why = "line " + std::to_string(line) + ": a ')' with no '(' before it";
          else open.pop_back();
        }
        break;
      case ';':
        if (!quote) comment = true;
        break;
      default:
        break;
    }
  }
  if (why.empty() && quote) why = "line " + std::to_string(quote_line) + ": a quote never closed";
  if (why.empty() && !open.empty()) why = "line " + std::to_string(open.front()) + ": a '(' never closed";
  std::fseek(f, 0, SEEK_SET);
  return why;
}

// Mech3.exe 1.2 @ 0x005563d0
Node* parse_text(std::FILE* file) {
  if (!balanced(file)) return nullptr;
  return parse_list(file);
}

// Mech3.exe 1.2 @ 0x00556740. Not in an archive: the text branch (the search
// 0x00556240, then 0x005563d0). FIX (A): the file is found through
// zutl::open, the write dir then the mounts, for the original's _access and
// search path, and read with stdio.
LoadHook g_load_hook = nullptr;
void set_load_hook(LoadHook hook) { g_load_hook = hook; }
MissingHook g_missing_hook = nullptr;
void set_missing_hook(MissingHook hook) { g_missing_hook = hook; }

// Ours (docs/modding.md, Readable files read directly): a mod's own copy of
// an archive's entry, at the archive's own folder (zbd\\c1\\ for the c1
// world's reader): per mod, first to last, a loose `<entry>` (text, as
// 0x005563d0 reads), then mech3ax's JSON, `<archive>\\<entry>.json` (the
// folder unzbd's zip unpacks to), then `<archive>.zip` holding `<entry>.json`.
// The first found is the one; one that does not read is said once, with its
// line, and null comes back (the entry read instead). Null without one.
// `added`: the archive has no such entry (a mod's new one, as a mech row's
// dfn_<name>.zrd beside reader.zbd), so the log says added, not instead.
bool same_name(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
  return true;
}
Node* mod_copy(const zutl_zar::Archive& a, const char* name, bool added = false) {
  if (zutl::mounts().mods.empty() || !a.path) return nullptr;
  std::string dir = zutl::host(a.path);
  const size_t slash = dir.rfind('/');
  std::string stem = slash == std::string::npos ? dir : dir.substr(slash + 1);
  dir = slash == std::string::npos ? std::string{} : dir.substr(0, slash + 1);
  if (const size_t dot = stem.rfind('.'); dot != std::string::npos) stem.resize(dot);
  const std::string entry = zutl::host(name);
  static std::vector<std::string> said;  // each copy said once in the log, again if why it reads (or not) changes
  auto say = [&](const std::string& what, const char* fmt, const std::string& why) {
    const std::string key = what + '\n' + why;
    if (std::find(said.begin(), said.end(), key) != said.end()) return;
    said.push_back(key);
    std::printf(fmt, what.c_str(), why.c_str(), a.path);
  };
  const char* fail = added ? "mw3: mod: %s: %s; not added to %s\n" : "mw3: mod: %s: %s; %s's entry read instead\n";
  auto took = [&](const std::string& what, Node* root) {
    say(what, added ? "mw3: mod: %s%s added to %s\n" : "mw3: mod: %s%s instead of %s's entry\n", "");
    if (g_load_hook) g_load_hook(a.path, g_fname, root);
    return root;
  };
  for (size_t m = 0; m < zutl::mounts().mods.size(); ++m) {
    if (const std::string loose = zutl::find_in_mod(m, dir + entry); !loose.empty()) {
      std::FILE* t = std::fopen(loose.c_str(), "rb");
      if (!t) return nullptr;
      std::string why = text_error(t);
      Node* root = why.empty() ? parse_text(t) : nullptr;
      std::fclose(t);
      if (root) return took(loose, root);
      say(loose, fail, why.empty() ? "it does not read" : why);
      return nullptr;
    }
    std::string text, what, why;
    const int got = zutl_zip::mod_json(m, dir, stem, entry + ".json", &text, &what, &why);
    if (got == 0) continue;  // none here: the next mod's, or the entry
    if (got < 0) {
      say(what, fail, why);
      return nullptr;
    }
    if (Node* root = parse_json(text, &why)) return took(what, root);
    say(what, fail, why);
    return nullptr;
  }
  return nullptr;
}

Node* load(const char* path) {
  crt::msvc_splitpath_name(path, g_fname, g_ext);
  std::strncat(g_fname, g_ext, sizeof g_fname - std::strlen(g_fname) - 1);
  // find_in_archives, keeping which archive answered (for the load hook)
  for (size_t i = 0; g_state.archives_created && i < g_state.archives.size(); ++i) {
    zutl_zar::Archive* a = g_state.archives[i];
    uint32_t length = 0;
    std::FILE* f = zutl_zar::seek(*a, zutl_zar::find(*a, g_fname), &length);
    if (!f) continue;
    // A mod's own copy (mod_copy), given to the load hook as the entry is.
    if (Node* root = mod_copy(*a, g_fname)) return upgrades::reference::loaded(g_fname, root);  // ours: the reference test's observer
    Node* root = new_nodes(List, 1);
    g_data_end = zutl_zar::data_end(*a), g_depth = 0, g_broken = false, g_short = false, g_odd = false;
    const long start = std::ftell(f);
    g_entry_start = start, g_entry_end = start + static_cast<long>(length);
    g_stop = g_drop = false, g_stopped = Stop{}, g_tail = Tail{}, g_notes.clear();
    read_node(f, root);
    const long end = std::ftell(f);
    g_data_end = -1;
    if (zfix::options().fixed) {  // ours, the graceful read: said in plain words, the entry kept with what it has
      const bool none = g_stop && g_drop;  // the entry's own first node was none
      make_notes(none);
      for (const mech3::ModWarning& n : g_notes)
        std::printf("mw3: %s\n", mech3::mod_warning_line(std::string(a->path) + ": " + g_fname, n).c_str());
      if (none) {
        free_node(root);
        return g_missing_hook ? g_missing_hook(g_fname) : nullptr;
      }
    } else if (g_broken) {  // the exe's path, bounded (the tests'): a broken entry is read as no entry
      free_node(root);
      std::printf("mw3: %s: its entry %s is broken (a count, a string or nesting no file could hold), not read\n", a->path, g_fname);
      return g_missing_hook ? g_missing_hook(g_fname) : nullptr;
    } else if (g_short) {
      std::printf("mw3: %s: its entry %s says more items than the archive holds after it; those it has are read, the rest left empty\n",
                  a->path, g_fname);
    } else if (g_odd) {
      std::printf("mw3: %s: its entry %s has a word that is no node type; read on, as the original does\n", a->path, g_fname);
    } else if (end - start > static_cast<long>(length)) {
      std::printf("mw3: %s: its entry %s says more items than it holds; the %ld bytes after it read as its tail, as the original does\n",
                  a->path, g_fname, end - start - static_cast<long>(length));
    }
    if (g_load_hook) g_load_hook(a->path, g_fname, root);
    return upgrades::reference::loaded(g_fname, root);
  }
  // Ours (docs/modding.md, A new mech): no archive has it; a mod's file of
  // that name in an archive's folder (loose or mech3ax's JSON), as the mount's
  // and --check-mod's checks find it, is a new entry of that archive.
  for (size_t i = 0; g_state.archives_created && i < g_state.archives.size(); ++i)
    if (Node* root = mod_copy(*g_state.archives[i], g_fname, true)) return upgrades::reference::loaded(g_fname, root);
  std::FILE* f = zutl::open(path, "rb");
  if (!f) return g_missing_hook ? g_missing_hook(g_fname) : nullptr;
  Node* root = parse_text(f);
  std::fclose(f);
  return upgrades::reference::loaded(g_fname, root);
}

}  // namespace zrdr
