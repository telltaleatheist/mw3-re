#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "gamez/zreader/zreader.h"
#include "mech3/zai_params.h"

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
  // 0x004c5c00: a region (zainetmechinout.cpp's inside/outside nets) by name,
  // loaded on first use. Not ported yet: none when null.
  void* (*find_region)(const char* name) = nullptr;
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

class AivRecord;

// aiv.zrd, the mission's vehicles (CZRdrAiv here, one object at 0x0060b5e0,
// vtable 0x005cd428): each record a vehicle by name, read into an AivRecord.
class AivFile : public RdrFile {
 public:
  AivFile() : RdrFile("aiv", 0x80, 0x64, 0, 1) {}  // 0x004df620 -> 0x004df640
  ~AivFile() override;                               // 0x004ded70
  int32_t unload() override;                         // +0x08 0x004df5f0
  AivRecord* cached(const char* name) const;         // 0x004def10

  // +0x24: records the editor dialog keeps (0x004defc0, not ported); none
  // while playing.
  std::map<std::string, AivRecord*>* cache = nullptr;
};
AivFile& aiv_file();
void aiv_file_reset();  // not original: the tests start from a fresh object

// One vehicle's record (vtable 0x005cd438, 0xe4 bytes). Making one takes a
// reference on aiv.zrd (the first loads it), dropping it releases one.
class AivRecord : public AiParams {
 public:
  AivRecord();                                       // 0x004e0490
  ~AivRecord() override;                             // 0x004e04f0
  AivRecord(const AivRecord&) = delete;
  AivRecord& operator=(const AivRecord&) = delete;

  AivRecord* read(int32_t i, const char** name);     // 0x004e0600: record i, the cache first
  AivRecord* read_named(const char* name);           // 0x004e0680
  AivRecord* parse(zrdr::Node* value, const char* name);  // 0x004df730

  int32_t mech3_set = 0;   // +0xdc  a "mech3" tag was there
  int32_t mech3 = 0;       // +0xe0  its value != 0 (left as it was without the tag)
};

}  // namespace mech3::ai
