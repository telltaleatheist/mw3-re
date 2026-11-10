#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Ours (docs/modding.md, Animations a def at a time): a world's animations a
// def at a time. A mod's def files, each one def as unzbd writes it, are
// merged into the world's anim.zbd as it loads:
// - zbd\<world>\anim\<key>.json for one world (that folder without a
//   metadata.json: with one it is the world's whole JSON, as before);
// - zbd\anim\<key>.json for every world.
// Each replaces the base's def of the same key (unzbd's file name for it,
// zeff::anim_def_key) where it stands, or is added after the base's last
// def. Per key the first enabled mod wins, a mod's world folder before its
// world-less one. The base is the first enabled mod's whole file for the
// world (its anim.zbd or JSON), else the install's; def files of the mods
// listed after the base's mod only add, never replace its defs.
namespace mech3::anim_defs {

struct DefSpan {
  size_t at = 0, size = 0;  // the def's bytes in the file
  std::string key;          // unzbd's file name for it ("" for def 0)
};
// The defs of an anim.zbd in file order, def 0 first; false when the bytes
// do not walk as one (a count or size past the end).
bool def_spans(const uint8_t* p, size_t n, std::vector<DefSpan>* out);

struct DefFile {
  std::string key, from;      // from: the file, for the log
  std::vector<uint8_t> bytes;  // zeff::anim_def_from_json's
};
// `base` with each def file in: a key the base has replaced in place, the
// rest added after the last def in the order given; the header's def count
// set. Empty and `why` when the base does not walk or the count would pass
// the 16-bit count.
std::vector<uint8_t> with_defs(const std::vector<uint8_t>& base, const std::vector<DefFile>& defs, std::string* why);

// The def files the enabled mods hold for the world of `game_path`
// (zbd\c1\anim.zbd), by the rules above, against `base` (the bytes chosen
// to load). `notes` gets one line a file that does not read or is passed
// over. Each def file is read by zeff::anim_def_from_json.
std::vector<DefFile> mod_def_files(const char* game_path, const std::vector<uint8_t>& base, std::vector<std::string>* notes);

// A mod's file (its path in the mod, '/', any case) as a def file: true for
// zbd/<world>/anim/<x>.json (`world` the world) and zbd/anim/<x>.json
// (`world` ""), not a script (si-script-*), and not in a folder holding the
// world's whole JSON (metadata.json). `mod_dir` is the mod's folder.
bool is_def_file(const std::string& mod_dir, const std::string& rel, std::string* world);
// A def file's key from its own names (zeff::anim_def_key), "" when its JSON
// does not read or has none.
std::string def_file_key(const std::string& path);

// Ours (modding.md, Both ways): whether the mounted mod `m` (zutl::mounts().mods)
// has mod.txt's anim=defs: its whole anim files are kept for the original game
// and not ours to read (its def files are). Its mod.txt read at each call.
bool mod_defs_only(size_t m);
// The world's whole anim.zbd for `game_path` when the first mod holding one
// has anim=defs: the next holder's (a mod's, else the install's), "" when no
// such mod is in the way (the mounts' own lookup is right then).
std::string whole_file_past_defs_only(const char* game_path);

// --build-anim (modding.md, Both ways): for each world of `install` the mod
// has def files for, the install's anim.zbd with this mod's def files merged
// in (its world folder before its world-less one, the first file of a key)
// written to <mod>\original\zbd\<world>\anim.zbd, the file the original
// game reads (a .bak of one there before). One line a world or a file left
// out; the number of errors.
int build_for_original(const std::string& mod_dir, const std::string& install, std::vector<std::string>* lines);

}  // namespace mech3::anim_defs
