#pragma once
#include <functional>
#include <string>
#include <vector>

// Finding a font on a system whose fonts are in no fixed place (Linux; docs/linux.md,
// The launcher's font): the folders fontconfig reads, walked, no fontconfig needed.
namespace sdlfe {

// The font folders of this system: the user's, then XDG_DATA_DIRS' and /usr's.
std::vector<std::string> font_folders();
// Every .ttf, .otf and .ttc under `roots`, in name order (a set number at most).
std::vector<std::string> scan_fonts(const std::vector<std::string>& roots);
// Of `files`, the first of `wanted` (file names, any case; best first); else
// a bold upright text face, else an upright one, else "": no emoji or symbols,
// and only a face `usable` accepts when given (has_latin).
std::string pick_font(const std::vector<std::string>& files, const std::vector<std::string>& wanted,
                      const std::function<bool(const std::string&)>& usable = {});
// The file opens and has our letters: a script's own face (NotoSansArabic) doesn't.
bool has_latin(const std::string& file);

}  // namespace sdlfe
