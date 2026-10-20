#pragma once
#include <cstdint>

#include "gamez/zreader/zreader.h"

// mech3\ZRdrAiv.cpp: the AI's reader files (docs/ai.md, Data files). The base
// every one of them derives from lives here too (its errors name this file):
// a .zrd loaded on its first reference and freed on its last, whose records
// are the key/value pairs of its one top-level list.
namespace mech3::ai {

// Calls the AI readers make into modules not ported, or ported elsewhere.
struct RdrHooks {
  // 0x00556240(name, 0): a loose text file's path along the reader's search
  // path. FIX (A): loose .zrd files are not read (zreader.md), so none.
  const char* (*find_loose)(const char* name) = nullptr;
  const char* (*localize)(const char* name) = nullptr;  // 0x00575fb0; zutl::localize when null
  void* (*sound_find)(const char* name) = nullptr;      // 0x0056f6f0; zsnd::find when null
};
RdrHooks& rdr_hooks();

// CZRdrFile here: 0x24 bytes, vtable 0x005cd418.
class RdrFile {
 public:
  // 0x004de9c0: name gets ".zrd" (a suffix given is dropped, with a report).
  RdrFile(const char* name, int32_t max_refs, int32_t x18, int32_t first, int32_t quiet);
  virtual ~RdrFile();                    // 0x004deb20
  RdrFile(const RdrFile&) = delete;
  RdrFile& operator=(const RdrFile&) = delete;

  virtual int32_t unload();              // +0x08 0x004deb80
  virtual void parse() {}                // +0x0c 0x0049bfd0, empty in the base

  int32_t add_ref();                     // 0x004deb50: the first loads
  int32_t release();                     // 0x004deba0: the last unloads
  int32_t load();                        // 0x004dec80
  const char* resolve_dir();             // 0x004debd0: the loose file's folder, cached
  int32_t record_count() const;          // 0x004decf0
  zrdr::Node* record(int32_t i, const char** key) const;  // 0x004ded20: the value; key set
  zrdr::Node* find(const char* key) const;                // 0x004ded60: zrdr::find on the tree

  char* name = nullptr;        // +0x04  "<name>.zrd"
  char* dir = nullptr;         // +0x08  resolve_dir's
  zrdr::Node* tree = nullptr;  // +0x0c
  int32_t refs = 0;            // +0x10
  int32_t max_refs = 0;        // +0x14  0x80 for every AI file
  int32_t x18 = 0;             // +0x18  100 for every AI file, never read here
  int32_t first = 0;           // +0x1c  the top list's slot before the first record
  int32_t quiet = 0;           // +0x20  no "Couldn't read" report
};

}  // namespace mech3::ai
