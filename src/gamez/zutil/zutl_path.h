#pragma once
#include <cstdio>
#include <string>
#include <vector>

// Where game files come from (docs/assets-layer.md). The original looks files
// up along a search path (0x005762b0 find, 0x005763b0 fopen; ".;zbd" for the
// interpreter) relative to the install. FIX (A): the rebuild generalises that
// into an ordered list of mounted folders, so a set of replacement files can
// sit in front of the original install, or stand in for it entirely.
//
// Paths are the game's own, Windows style and relative ("zbd\\c1\\gamez.zbd");
// the write dir (what the game saved) is searched first, then the mounts in
// order; the first holding the file wins. An absolute or ./-relative host path
// that exists is used as it is (tools and tests pass those).
namespace zutl {

struct Mounts {
  std::vector<std::string> read;   // searched in order
  std::string write;               // where the game writes (configs\, saves); "" = the first read mount
};
Mounts& mounts();

void mount(const std::string& dir);          // appended: searched after the ones before
void mount_front(const std::string& dir);    // searched first (an override folder)
void set_write_dir(const std::string& dir);

std::string host(const char* path);          // '\\' -> '/'
std::string find(const char* path);          // the host path in the first mount that has it, "" if none
bool exists(const char* path);
std::FILE* open(const char* path, const char* mode);  // reads search the mounts; writes go to the write dir
std::string write_path(const char* path);    // the host path a write of `path` goes to
// FIX (A) for FindFirstFileA / FindNextFileA: the files (not folders) a game
// pattern ("keys\\*.zrd"; * and ? in the last part, any case) matches in the
// write dir and every mount, each name once (the first found), sorted as NTFS
// lists a folder (by the upper-cased name).
std::vector<std::string> list(const char* pattern);

}  // namespace zutl
