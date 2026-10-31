#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The official 1.2 patch applied to a 1.0 install's data (ours, not original
// code; docs/assets.md, The 1.2 patch). The patch is an InstallShield 5
// setup whose data1.cab holds the 1.2 files whole; it is read, never run.
// The 28 files the port reads (data_version.h) go into an overlay folder in
// the write dir, each checked against 1.2's size, CRC and SHA-256 first; the
// player's install is never written.
namespace mech3 {

struct PatchEntry {
  std::string path;  // as the source names it, '/'-separated ("zbd/c1/reader.zbd")
  std::vector<uint8_t> data;
};

// Every file of an InstallShield 5 cabinet (data1.hdr's directory, data1.cab's
// data), each under its directory; false with `error` set when it is not one
// this reads (another version, split across volumes) or a file fails to inflate.
bool read_installshield(const std::vector<uint8_t>& hdr, const std::vector<uint8_t>& cab,
                        std::vector<PatchEntry>* out, std::string* error);

// The same, file by file, from a cabinet read where it lies (a disc's
// DATA1.CAB, 198 MB): `read` gives `n` bytes at `at`; `each` takes each file
// (false stops with an error). Every file is checked against its MD5.
// The files a cabinet's header lists (path, expanded size), without its data.
bool list_installshield(const std::vector<uint8_t>& hdr, std::vector<std::pair<std::string, uint64_t>>* out,
                        std::string* error);
using CabRead = std::function<bool(uint64_t at, size_t n, uint8_t* out)>;
bool each_installshield(const std::vector<uint8_t>& hdr, uint64_t cab_size, const CabRead& read,
                        const std::function<bool(PatchEntry&&)>& each, std::string* error);

struct PatchImport {
  bool ok = false;
  std::string error;               // why nothing was written
  int found = 0;                   // of the 28, with 1.2's bytes
  std::vector<std::string> missing;
  std::vector<std::string> log;    // where each source was read
};

// From `source`, the patch in any form a player may have it: the ZIP as
// downloaded, the unpacked folder (or any folder above it), its data1.hdr, or
// loose 1.2 files. The 28 go into `out_dir` (written to `out_dir`.new, then
// renamed) only when all 28 are found.
PatchImport import_patch_12(const std::string& source, const std::string& out_dir);

std::string sha256_hex(const uint8_t* data, size_t n);
void md5(const uint8_t* data, size_t n, uint8_t out[16]);

}  // namespace mech3
