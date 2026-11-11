// A Windows check of DELETE's remover (upgrades/mods/marked_remove.cpp) with directory junctions, without the game:
//
//   delete_check.exe DIR      (DIR must exist; the check works in DIR\delete-check, made anew)
//
// It builds  wd\mods\ours (our marker, a file, a junction "out" to DIR\delete-check\outside, a folder with a
// file) and  wd\mods\jmod, itself a junction to DIR\delete-check\elsewhere (which holds a marker and a file),
// then: (1) path_is_link says yes for both junctions and no for a real folder; (2) the remover refuses jmod (the
// folder itself is a link) and elsewhere is untouched; (3) the remover removes "ours": the folder gone, the
// junction gone with it, and outside\keep.txt still there with its bytes. Prints each line and OK or FAILED;
// returns 0 when all passed. Junctions are made with "cmd /c mklink /J" (no administrator needed).
// Built by the mingw cross compiler on the Mac as a compile and link check; run on Windows:
//   x86_64-w64-mingw32-g++ -std=c++17 -Isrc tests/win/delete_check.cpp src/upgrades/mods/marked_remove.cpp -o delete_check.exe -static
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "upgrades/mods/marked_remove.h"

namespace fs = std::filesystem;

static int failed = 0;
static void check(bool ok, const char* what) {
  std::printf("%s: %s\n", ok ? "ok    " : "FAILED", what);
  if (!ok) ++failed;
}
static void put(const fs::path& p, const char* text) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p, std::ios::binary) << text;
}
static std::string text_of(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
static bool junction(const fs::path& link, const fs::path& target) {
  const std::string cmd = "cmd /c mklink /J \"" + link.string() + "\" \"" + target.string() + "\" >nul";
  return std::system(cmd.c_str()) == 0;
}

int main(int argc, char** argv) {
  if (argc < 2) return std::printf("delete_check.exe DIR\n"), 2;
  std::error_code ec;
  const fs::path root = fs::path(argv[1]) / "delete-check", wd = root / "wd";
  if (fs::exists(root / "made-by-delete-check.txt", ec) || !fs::exists(root, ec)) {
    // Only a folder this check made before (its own marker), taken down without following junctions: by the remover's own rule.
    if (fs::exists(root, ec)) {
      std::system(("cmd /c rmdir \"" + (root / "wd" / "mods" / "ours" / "out").string() + "\" 2>nul").c_str());
      std::system(("cmd /c rmdir \"" + (root / "wd" / "mods" / "jmod").string() + "\" 2>nul").c_str());
      fs::remove_all(root, ec);
    }
  } else {
    return std::printf("%s exists and is not this check's: give another DIR\n", root.string().c_str()), 2;
  }
  put(root / "made-by-delete-check.txt", "delete_check's own folder\n");
  put(root / "outside" / "keep.txt", "must stay");
  put(root / "elsewhere" / "marker.txt", "m");
  put(root / "elsewhere" / "theirs.txt", "must stay too");
  put(wd / "mods" / "ours" / "marker.txt", "ours");
  put(wd / "mods" / "ours" / "file.txt", "x");
  put(wd / "mods" / "ours" / "sub" / "deep.txt", "y");
  put(wd / "mods" / "real" / "marker.txt", "ours");
  check(junction(wd / "mods" / "ours" / "out", root / "outside"), "a junction made inside the marked folder");
  check(junction(wd / "mods" / "jmod", root / "elsewhere"), "a junction made standing for a mod folder");
  check(mech3::path_is_link((wd / "mods" / "ours" / "out").string()), "path_is_link: the junction inside");
  check(mech3::path_is_link((wd / "mods" / "jmod").string()), "path_is_link: the junction standing for a folder");
  check(!mech3::path_is_link((wd / "mods" / "real").string()) && !mech3::path_is_link((wd / "mods" / "ours" / "file.txt").string()),
        "path_is_link: a real folder and a real file are not links");
  std::string why;
  check(!mech3::remove_marked_folder(wd.string(), "mods", "jmod", "marker.txt", &why), "the folder itself a junction: refused");
  std::printf("        (%s)\n", why.c_str());
  check(text_of(root / "elsewhere" / "theirs.txt") == "must stay too" && fs::exists(root / "elsewhere" / "marker.txt", ec), "its target untouched");
  check(mech3::remove_marked_folder(wd.string(), "mods", "ours", "marker.txt", &why), "the marked folder removed");
  if (!why.empty()) std::printf("        (%s)\n", why.c_str());
  check(!fs::exists(wd / "mods" / "ours", ec), "it is gone, the junction with it");
  check(text_of(root / "outside" / "keep.txt") == "must stay", "what the junction pointed at is still there, the same bytes");
  check(fs::exists(wd / "mods" / "real" / "marker.txt", ec), "the other folder untouched");
  std::printf("%s\n", failed ? "FAILED" : "OK");
  return failed ? 1 : 0;
}
