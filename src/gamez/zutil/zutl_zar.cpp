#include "gamez/zutil/zutl_zar.h"

#include <cstdlib>
#include <cstring>

#include "crt/msvc_string.h"

// FIX (A), whole file: CreateFileA / ReadFile / SetFilePointer / GetFileSize /
// CloseHandle become C stdio, which every platform has. The calls map one to one
// and keep their order. FILE_FLAG_SEQUENTIAL_SCAN, a caching hint, is dropped.
namespace zutl_zar {

namespace {
uint32_t read_bytes(std::FILE* f, void* buffer, uint32_t n) {
  return static_cast<uint32_t>(std::fread(buffer, 1, n, f));
}

long file_size(std::FILE* f) {
  long here = std::ftell(f);
  std::fseek(f, 0, SEEK_END);
  long size = std::ftell(f);
  std::fseek(f, here, SEEK_SET);
  return size;
}
}  // namespace

void host_path(const char* path, char* out, size_t size) {
  size_t i = 0;
  for (; path[i] && i + 1 < size; ++i) out[i] = path[i] == '\\' ? '/' : path[i];
  out[i] = '\0';
}

// Mech3.exe 1.2 @ 0x005764e0
void init(Archive& a) {
  a.path = nullptr;
  a.file = nullptr;
  a.dirty = false;
  a.count = 0;
  a.capacity = 0;
  a.entries = nullptr;
}

// Mech3.exe 1.2 @ 0x00576500
void destroy(Archive& a) {
  close(a);
  // Quirk kept: reset() inside close() already freed the path when there was a
  // table, and set it to null, so this only frees a path left by a failed open.
  if (a.path) std::free(a.path);
}

// Mech3.exe 1.2 @ 0x00576520. Opens for reading (GENERIC_READ, FILE_SHARE_READ,
// OPEN_EXISTING). The original formats GetLastError() on failure and frees the
// text without using it; the rebuild skips that.
bool open(Archive& a, const char* path) {
  char host[512];
  host_path(path, host, sizeof host);
  a.file = std::fopen(host, "rb");
  if (!a.file) return false;
  bool ok = read_table(a);
  a.path = strdup(path);
  return ok;
}

// Mech3.exe 1.2 @ 0x005767b0
bool read_table(Archive& a) {
  long size = file_size(a.file);
  if (static_cast<unsigned long>(size) < 8) return false;
  int32_t version = 0, count = 0;
  std::fseek(a.file, -8, SEEK_END);
  read_bytes(a.file, &version, 4);
  read_bytes(a.file, &count, 4);
  if (version != 1) return false;
  reserve(a, count);
  std::fseek(a.file, -static_cast<long>(count) * 0x94 - 8, SEEK_END);
  read_bytes(a.file, a.entries, static_cast<uint32_t>(count) * 0x94);
  std::fseek(a.file, 0, SEEK_SET);
  a.count = count;
  return true;
}

// Mech3.exe 1.2 @ 0x00576690: grows to at least count + 1, doubling.
void reserve(Archive& a, int count) {
  if (a.capacity <= count) {
    int doubled = a.capacity * 2;
    int want = count + 1;
    if (want < doubled) want = doubled;
    a.capacity = want;
    a.entries = static_cast<Entry*>(std::realloc(a.entries, static_cast<size_t>(want) * 0x94));
  }
}

// Mech3.exe 1.2 @ 0x00576650. The original writes the table first when dirty
// (0x00576720); only the unported writer sets dirty.
bool close(Archive& a) {
  if (a.file) std::fclose(a.file);
  a.file = nullptr;
  reset(a);
  return true;
}

// Mech3.exe 1.2 @ 0x005766d0. Quirk kept: does nothing when there is no table.
void reset(Archive& a) {
  if (a.entries) {
    std::free(a.entries);
    a.entries = nullptr;
    if (a.path) std::free(a.path);
    a.path = nullptr;
    a.file = nullptr;
    a.capacity = 0;
    a.count = 0;
    a.entries = nullptr;
    a.dirty = false;
  }
}

// Mech3.exe 1.2 @ 0x00576a80
int find(const Archive& a, const char* name) {
  if (a.count < 1) return -1;
  for (int i = 0; i < a.count; ++i)
    if (crt::msvc_stricmp(name, a.entries[i].name) == 0) return i;
  return -1;
}

// Mech3.exe 1.2 @ 0x00576ae0
std::FILE* seek(Archive& a, int index, uint32_t* length) {
  const Entry* e = (index < 0 || a.count <= index) ? nullptr : &a.entries[index];
  if (!e) return nullptr;
  if (length) *length = e->length;
  std::fseek(a.file, static_cast<long>(e->start), SEEK_SET);
  return a.file;
}

// Mech3.exe 1.2 @ 0x00576b30
uint32_t read(Archive& a, int index, void* buffer, uint32_t* size) {
  const Entry* e = (index < 0 || a.count <= index) ? nullptr : &a.entries[index];
  if (!e) return 0x10001;
  uint32_t have = *size;
  *size = e->length;
  if (have < e->length) return 0x10002;
  if (buffer) {
    std::fseek(a.file, static_cast<long>(e->start), SEEK_SET);
    read_bytes(a.file, buffer, e->length);
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00576460
uint32_t read_string(std::FILE* file, char** out) {
  int32_t length = 0;
  uint32_t total = read_bytes(file, &length, 4);
  char* s = static_cast<char*>(std::malloc(static_cast<size_t>(length) + 1));
  *out = s;
  std::memset(s, 0, static_cast<size_t>(length) + 1);
  if (0 < length) total += read_bytes(file, s, static_cast<uint32_t>(length));
  return total;
}

}  // namespace zutl_zar
