#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

namespace zutl_zar { struct Archive; }

// C:\zipper\GameZMech3\zReader\zreader.cpp: the game's data reader. Game data is
// a tree of typed nodes. Objects are flat lists of key/value pairs:
//
//   ("CD_TRACKS" (("menu" 2) ("c1" 3) ...) "OTHER_KEY" 1.5 ...)
//
// Shipped data comes from the .zbd archives in a binary form; the developers'
// loose text files (*.zrd, with #if/#endif) are read by a tokenizer that is not
// ported yet (0x005563f0, 0x00556510, 0x00557550 and neighbours), since no text
// files ship with the game.
//
// Binary node: uint32 type, then int32 | float32 | (uint32 length, bytes) | list.
// A list is uint32 count, then count - 1 nodes. In memory the list keeps that
// count in its own first slot: list[0] = {Int, count}, items in list[1..count-1].
namespace zrdr {

enum Type : int32_t { Int = 1, Float = 2, String = 3, List = 4 };

struct Node {
  int32_t type;
  union {
    int32_t i;
    float f;
    char* s;
    Node* list;
  };
};

inline int32_t list_count(const Node* list_node) { return list_node->list[0].i; }

Node* new_nodes(int32_t first_type, int32_t count);       // 0x005564f0
Node* alloc_list(int32_t count);  // a list's slots, plus the FIX (A) spare (zreader.cpp)
uint32_t read_node(std::FILE* file, Node* node);           // 0x00557180
void free_contents(Node* node);                            // 0x00556840
int free_node(Node* node);                                 // 0x00556820

// Searches a list for a string equal to `key` and returns the node after it.
Node* find(Node* node, const char* key, int start);        // 0x00556930, nested lists too
Node* find(Node* node, const char* key);                   // 0x005569e0, find(node, key, 1)
Node* find_flat(Node* node, const char* key, int start);   // 0x005568a0, this list only

// Typed lookups. A value may also be a one-item list: ("KEY" (5)).
const char* get_string(Node* node, const char* key);                 // 0x005569f0
const char* get_string_flat(Node* node, const char* key, int start); // 0x00556a20
bool get_float(Node* node, const char* key, float* out);             // 0x00556a60
bool get_int(Node* node, const char* key, int32_t* out);             // 0x00556ae0
void get_int_if(Node* node, const char* key, int32_t* out);          // 0x00556b30
void get_byte_if(Node* node, const char* key, uint8_t* out);         // 0x00556b50

bool get_vec3(Node* list_node, float out[3]);                        // 0x00556b70
bool get_vec2(Node* list_node, float out[2]);                        // 0x00556c00
bool get_ivec2(Node* list_node, int32_t out[2]);                     // 0x00556c70
bool get_ivec3(Node* list_node, int32_t out[3]);                     // 0x00556cf0
bool get_bool_int(Node* node, int32_t* out);                         // 0x00556da0

bool get_vec3(Node* node, const char* key, float out[3]);            // 0x005570b0
bool get_vec2(Node* node, const char* key, float out[2]);            // 0x005570d0
bool get_ivec3(Node* node, const char* key, int32_t out[3]);         // 0x005570f0
bool get_ivec2(Node* node, const char* key, int32_t out[2]);         // 0x00557110
bool get_bool_int(Node* node, const char* key, int32_t* out);        // 0x00557130
bool get_bool(Node* node, const char* key, bool* out);               // 0x00557150

// The open archives, searched in the order they were opened.
struct State {
  bool archives_created = false;                  // 0x0072c1a4 != 0
  std::vector<zutl_zar::Archive*> archives;       // 0x0072c1a4
  zutl_zar::Archive* current = nullptr;           // 0x0072c1a8
};
State& state();

void init();                                                  // 0x00556170
bool open_archive(const char* path, bool make_current);      // 0x00557320
void close_archives(bool all);                                // 0x005573d0
std::FILE* find_in_archives(const char* name);                // 0x005572c0
// 0x00556740: reads the named data file. The directory is ignored for archives:
// "foo\\bar\\detail.zrd" is looked up as "detail.zrd". nullptr if not found.
Node* load(const char* path);

}  // namespace zrdr
