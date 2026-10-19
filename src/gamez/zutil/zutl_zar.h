#pragma once
#include <cstdint>
#include <cstdio>

// C:\zipper\GameZMech3\zUtil\zutl_zar.cpp: the .zbd archive. A plain
// concatenation of files with the table of contents at the end:
//
//   file data ... | Entry[count] | uint32 version (1) | uint32 count
//
// Only the reading side is ported. The writer (0x005765b0 create, 0x00576890 and
// 0x00576990 add, 0x00576720 write the table) builds archives, which the game
// never does at run time.
namespace zutl_zar {

// One table entry, 0x94 bytes, read straight from the file.
struct Entry {
  uint32_t start;        // +0x00 offset of the data
  uint32_t length;       // +0x04
  char name[64];         // +0x08 matched case-insensitively
  uint32_t flags;        // +0x48 bit 1: comment and filetime are set
  char comment[64];      // +0x4c
  uint32_t filetime[2];  // +0x8c Win32 FILETIME, low then high
};
static_assert(sizeof(Entry) == 0x94, "Entry must match the file layout");

// The original object is 0x18 bytes; the fields keep its order.
struct Archive {
  char* path = nullptr;         // +0x00 _strdup of the name it was opened with
  std::FILE* file = nullptr;    // +0x04 HANDLE, INVALID_HANDLE_VALUE when closed
  bool dirty = false;           // +0x08 table changed, write it on close
  int count = 0;                // +0x0c entries in use
  int capacity = 0;             // +0x10 entries allocated
  Entry* entries = nullptr;     // +0x14
};

void init(Archive& a);                         // 0x005764e0
void destroy(Archive& a);                      // 0x00576500
bool open(Archive& a, const char* path);       // 0x00576520
bool read_table(Archive& a);                   // 0x005767b0
void reserve(Archive& a, int count);           // 0x00576690
bool close(Archive& a);                        // 0x00576650
void reset(Archive& a);                        // 0x005766d0
int find(const Archive& a, const char* name);  // 0x00576a80, -1 if absent
// 0x00576ae0: positions the file at entry `index` and returns it (the original
// returns the HANDLE), or nullptr for INVALID_HANDLE_VALUE.
std::FILE* seek(Archive& a, int index, uint32_t* length);
// 0x00576b30: copies entry `index` into `buffer`. *size is the buffer size in and
// the entry length out. 0 on success, 0x10001 no such entry, 0x10002 too small.
uint32_t read(Archive& a, int index, void* buffer, uint32_t* size);

// 0x00576460, next to the archive code (its source file is not named): a string
// stored as uint32 length then the bytes, read into a zeroed malloc(length + 1).
// Returns the bytes read.
uint32_t read_string(std::FILE* file, char** out);

// FIX (A): the original passes Windows paths ("zbd\\reader.zbd") to CreateFileA.
// The rebuild turns '\\' into '/' before opening.
void host_path(const char* path, char* out, size_t size);

}  // namespace zutl_zar
