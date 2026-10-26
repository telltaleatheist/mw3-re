#pragma once
#include <cstdint>
#include <cstdio>
#include <list>

#include "gamez/zutil/zutl_zar.h"

// zSave (0x005a8a20..0x005a9970; the original file is not named: between
// zentity\zent_init.cpp and zsiscript\si_script.cpp, its header entry is
// "zSaveHeader"). A saved game is a .zbd archive (zutl_zar): the header entry
// (8, the kind) and then each registrant's entries, named "<owner>/<sub>".
// Registrants are kept in a list, saved in priority order, and given back
// their entries on a load by owner name. docs/save.md.
namespace zsave {

struct Context;

// The registrant's calls: the save writes entries (write / write_temp), the
// load is given one entry's bytes and its sub-name.
using SaveFn = void (*)(Context& ctx, void* user);
using LoadFn = void (*)(Context& ctx, const char* sub, const void* data, uint32_t size, void* user);

// A list node's data (0x1c-byte nodes, the data from +0x08), 0x14 bytes.
struct Registrant {
  const char* name = nullptr;  // +0x00  compared with strcmp
  SaveFn save = nullptr;       // +0x04
  LoadFn load = nullptr;       // +0x08
  int32_t priority = 0;        // +0x0c  saved lowest first
  void* user = nullptr;        // +0x10
};

// The manager (new(0x34), [0x00736580]).
struct Manager {
  std::list<Registrant> list;    // +0x04 the head, +0x08 the count
  zutl_zar::Archive archive;     // +0x0c
  uint32_t capacity = 0;         // +0x24  the load buffer's size
  uint8_t* buffer = nullptr;     // +0x28
  int32_t stop = 0;              // +0x30  set by a load call: the rest of the file is skipped
};

// What a registrant's call is given (on the caller's stack in the exe).
struct Context {
  Manager* manager;
  Registrant* registrant;
};

Manager* manager();                // [0x00736580], null before init

int32_t init();                    // 0x005a8b30
void shutdown();                   // 0x005a8bb0 (0x005a8be0 the destructor)
// 0x005a8a20 (0x005a8cb0): a registrant added unless one of that name is there.
void add(const char* name, SaveFn save, LoadFn load, int32_t priority, void* user);
// 0x005a8a70 (0x005a8da0): `path` written from scratch; false when not made.
bool save(const char* path, int32_t kind);
// 0x005a8a90 (0x005a8e40): false when the file does not open or its kind is
// not `kind` (-1: any).
bool load(const char* path, int32_t kind);
int32_t kind_of(const char* path);  // 0x005a90d0: -1 without a good header
// 0x005a8a50 (0x005a9140): an entry "<owner>/<sub>".
void write(Context& ctx, const char* sub, const void* data, uint32_t size);
std::FILE* temp();                  // 0x005a8ab0: tmpfile(), null before init
// 0x005a8ad0 (0x005a9210): the temporary file's bytes as an entry, then every
// temporary file removed (_rmtmp).
void write_temp(std::FILE* f, Context& ctx, const char* sub);
// 0x005a8af0 (0x005a9290): a temporary file holding `data`, at its start.
std::FILE* temp_from(const void* data, uint32_t size);
void temp_done();                   // 0x005a8b10: _rmtmp

}  // namespace zsave
