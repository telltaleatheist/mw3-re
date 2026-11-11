#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Ours (docs/modding.md, Archives by their changed entries): a mod made for
// the original carries whole reader archives (zbd\reader.zbd, a mission's
// readerm<N>.zbd), each many records of which it changed a few; two such
// mods could not both apply, the first hiding the other's. So IMPORT keeps,
// in place of a reader archive the player's install also has, only the
// entries that differ from the install's or are new, their bytes untouched,
// in `<archive>.entries` (an archive of the same form). At the mount the
// archive is built once into a folder of ours, the install's with each mod's
// entries laid over it (the first mod's winning), and that folder is mounted
// in front: the game then reads an ordinary whole archive.
namespace mech3 {

constexpr const char* kEntriesSuffix = ".entries";
constexpr const char* kModMergedFolder = "mod-merged";
constexpr const char* kModMergedMarker = "mw3-mod-merged.txt";

// "zbd/c1/readerm1.zbd.entries" (any case): an archive's changed entries; its archive's path ("" when not one).
std::string entries_archive_of(const std::string& rel);
// A reader archive by its place and name: zbd/reader*.zbd or zbd/<world>/reader*.zbd.
bool record_archive(const std::string& rel);

struct ArchiveEntry {
  std::string name;
  std::vector<uint8_t> data;
};
// Every entry of the archive at a host path, in the table's order; false when its table does not read or an
// entry lies outside the file.
bool archive_read_entries(const std::string& host, std::vector<ArchiveEntry>* out);
std::vector<std::string> archive_entry_names(const std::string& host);

struct EntriesCut {
  enum What { kLeft, kSame, kCut } what = kLeft;  // left whole; dropped (every entry the install's); cut to its entries
  int changed = 0, added = 0, same = 0;           // the mod's entries against the install's
  int missing = 0;                                // entries the install's has and the mod's lacks (kept as the install's)
  std::string why;                                // kLeft: why it stays whole
};
// The mod's archive `mod_file` against the install's `own_file`: kCut writes `mod_file`.entries and removes
// `mod_file`; kSame removes it. Only ever `mod_file` and the file beside it are written (IMPORT's staging copy).
EntriesCut entries_cut(const std::string& mod_file, const std::string& own_file);

struct ModCut {
  int archives = 0, entries = 0;  // archives cut, and the entries kept in them
  int same = 0;                   // archives dropped: every entry the install's
  int whole = 0;                  // reader archives left whole (new ones, or a table that does not read)
};
// Every reader archive under `root` (a mod being added) that `install` also has.
ModCut entries_cut_mod(const std::string& root, const std::string& install);

// The whole archive again: `base`'s bytes (an archive file) with every entry of the .entries file `entries` laid
// over it or added after its last. Empty with `why` when either does not read. The mount's merge and EXPORT's
// repack both use it, so what is exported is what was played.
std::vector<uint8_t> entries_laid_over(std::vector<uint8_t> base, const std::string& entries, int* laid = nullptr, std::string* why = nullptr);

// The folders of earlier builds under <write dir>/mod-merged removed, each only with our marker in it and never
// `keep` (a folder's name; "" none). The count removed.
int entries_merged_clear(const std::string& write_dir, const std::string& keep);

// The mount's half. `mods`: the mounted mods' folders, the first winning; `later`: the patch's and the install's.
// Returns the folder to mount in front ("" when no mounted mod holds an .entries file, or none could be built),
// its lines for the log added to `log`.
std::string entries_merge(const std::string& write_dir, const std::vector<std::string>& mods, const std::vector<std::string>& later,
                          std::vector<std::string>* log);

}  // namespace mech3
