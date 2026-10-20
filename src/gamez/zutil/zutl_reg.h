#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace platform { class Settings; }

// The settings store (docs/settings.md): named values the game keeps between
// runs, saved in the registry under HKCU / HKLM \SOFTWARE\<company>\<product>\
// <version>. Original file unknown: the functions at 0x005977d0..0x00598270
// sit between gamezmech3\zinterp\zinterp_parse.cpp and zfmv\fmv_script.cpp;
// named zutl_reg here. The registry is the platform's Settings (FIX (A),
// fixes.md): a file in the write dir by default.
namespace zreg {

// Entry types: how the value is held and saved.
enum Type : int32_t {
  kDword = 0,   // 4 bytes in the entry, saved as REG_DWORD
  kBinary4 = 1, // 4 bytes in the entry (floats), saved as REG_BINARY
  kBinary8 = 2, // 8 bytes in the entry, REG_BINARY
  kBuffer = 3,  // 3..7: a calloc'd buffer of `size` bytes, REG_BINARY
  kBlock = 7,
};
// Which key an entry is saved under.
enum Scope : int32_t { kMachine = 0, kUser = 1, kNotSaved = 2 };

// One entry (0x20 bytes, a list at [0x00731910], newest first).
struct Entry {
  uint8_t* data = nullptr;   // +0x00 the value: types 0..2 in the entry itself, 3..7 a buffer
  int32_t type = 0;          // +0x08
  uint32_t size = 0;         // +0x0c 4, 8 or the buffer's size
  std::string name;          // +0x10 _strdup'd
  int32_t scope = 0;         // +0x14
  std::vector<uint8_t> own;  // the value's bytes when no field holds it
};

// The machine the game runs on, probed by 0x00597fc0 (0x00731924, 12 dwords;
// 0x00597fa0 copies it out). The rebuild's front end describes a modern one
// (FIX (A)): no CPUID timing loop or DirectDraw caps.
struct Machine {
  char vendor[16] = {};      // +0x00 CPUID's vendor string
  uint32_t cpu_class = 0;    // +0x10 CPU_CLASS (0x00598360)
  uint32_t cpu_mhz = 0;      // +0x14 CPU_MHZ (0x005986c0)
  uint32_t flags = 0;        // +0x18 bit 0 MMX, bit 6 a 3D card (HW_ACCEL); bits 1, 2 always 0
  uint32_t ram_kb = 0;       // +0x1c RAM_KB: GlobalMemoryStatus's dwTotalPhys >> 10
  uint32_t unset20 = 0;      // +0x20 never written, yet what VIDEO_KB reads
  uint32_t video_kb = 0;     // +0x24 DirectDraw's total video memory >> 10
  uint32_t unset28 = 0;      // +0x28 0x005981b0, always 0
  uint32_t unset2c = 0;      // +0x2c
};
static_assert(sizeof(Machine) == 0x30, "12 dwords");

// 0x00598210: the key's three names, then the machine probe 0x00597fc0 (whose
// results the caller gives, with HKLM's InstallOptions).
void init(const char* company, const char* product, const char* version, const Machine& m,
          uint32_t install_options);
void shutdown();                                   // 0x00598270, frees every entry
Entry* find(const char* name);                     // 0x00597d10
// 0x00597d90: the entry named `name`, made when there is none (a buffer type
// with size 0 makes nothing). `storage` is the field that holds the value
// (the rebuild's options are fields; the original's live in the entry).
Entry* create(const char* name, int32_t type, uint32_t size, int32_t scope, void* storage = nullptr);
uint32_t install_options(uint32_t mask);           // 0x00597d80, HKLM InstallOptions & mask
const Machine& machine();                          // 0x00597fa0
int32_t read_all();                                // 0x005977d0, the saved values into the entries
int32_t write_all();                               // 0x00597a60, every saved entry out
std::string key_path();                            // SOFTWARE\<company>\<product>\<version>
const std::vector<Entry*>& entries();              // newest first, as the original's list

void set_settings(platform::Settings* s);          // the registry's stand-in
platform::Settings* settings();

}  // namespace zreg
