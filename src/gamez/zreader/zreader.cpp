#include "gamez/zreader/zreader.h"

#include <cstdlib>
#include <cstring>

#include "crt/msvc_math.h"
#include "crt/msvc_string.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zutil/zutl_zar.h"

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

// Mech3.exe 1.2 @ 0x00557180. Returns the bytes read.
uint32_t read_node(std::FILE* file, Node* node) {
  uint32_t total = read4(file, &node->type);
  switch (node->type) {
    case Int:
      return total + read4(file, &node->i);
    case Float:
      return total + read4(file, &node->f);
    case String:
      return zutl_zar::read_string(file, &node->s) + total;
    case List: {
      int32_t count = 0;
      total += read4(file, &count);
      node->list = alloc_list(count);
      node->list[0].i = count;
      node->list[0].type = Int;
      for (int32_t i = 1; i < node->list[0].i; ++i) total += read_node(file, &node->list[i]);
      return total;
    }
    default:
      zerr::report(0x400, kFile, 0x4b4, "Invalid reader node type in zRdrReadNode");
      return 0;
  }
}

// Mech3.exe 1.2 @ 0x00556840. Walks slot 0 too; it is an Int, so nothing happens.
void free_contents(Node* node) {
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
Node* find(Node* node, const char* key) { return find(node, key, 1); }

// Mech3.exe 1.2 @ 0x005568a0
Node* find_flat(Node* node, const char* key, int start) {
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

// Mech3.exe 1.2 @ 0x005572c0
std::FILE* find_in_archives(const char* name) {
  if (!g_state.archives_created) return nullptr;
  for (size_t i = 0; i < g_state.archives.size(); ++i) {
    zutl_zar::Archive* a = g_state.archives[i];
    if (std::FILE* f = zutl_zar::seek(*a, zutl_zar::find(*a, name), nullptr)) return f;
  }
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00556740. The text branch (a loose file found on the search
// path, parsed by 0x005563d0) is not ported; the shipped game has no loose files,
// so the original takes the same "not found" path there.
Node* load(const char* path) {
  crt::msvc_splitpath_name(path, g_fname, g_ext);
  std::strncat(g_fname, g_ext, sizeof g_fname - std::strlen(g_fname) - 1);
  if (std::FILE* f = find_in_archives(g_fname)) {
    Node* root = new_nodes(List, 1);
    read_node(f, root);
    return root;
  }
  return nullptr;
}

}  // namespace zrdr
