#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Ours (docs/modding.md, The MODS screen's text fits its boxes): text on the mods screens measured with the
// font it is drawn in, so a long mod name, a report's rows and the notes stay inside their boxes at 4:3 and
// wide alike (the boxes are the same size in both; only their place changes). Where the host cannot measure
// (a check without fonts) a width in characters stands in.
namespace ztwod {
class FontText;
}
namespace mech3 {

struct ModAddDone;

// The boxes' inner widths in the 640-wide menu's pixels (MODSDIALOG, patch12_edits.cpp).
constexpr int32_t kModsRowPx = 200;     // a row of MODS' list or the left box: from the text's left to the frame
constexpr int32_t kModsDetailPx = 184;  // DETAILS' first line
constexpr int32_t kModsNotePx = 250;    // a line of the note under MODS, to the screen's edge
constexpr size_t kModsRowChars = 25, kModsDetailChars = 23, kModsNoteChars = 58;  // unmeasured: about as many

// Whether `text` is at most `px` wide in the font `font` is drawn in (a Text of the screen's); `chars` decides
// when unmeasured.
bool mods_text_fits(const std::string& text, const ztwod::FontText* font, int32_t px, size_t chars);
// `text`, or its start and "..." when it is too wide.
std::string mods_text_fit(const std::string& text, const ztwod::FontText* font, int32_t px, size_t chars);
// Wrapped at spaces; a word wider than a line is cut where it must be.
std::vector<std::string> mods_text_wrap(const std::string& text, const std::function<bool(const std::string&)>& fits);
std::vector<std::string> mods_text_wrap(const std::string& text, const ztwod::FontText* font, int32_t px, size_t chars);
// The note's two lines: wrapped, what is left over on the second cut with "...".
std::pair<std::string, std::string> mods_note_lines(const std::string& text, const std::function<bool(const std::string&)>& fits);

// IMPORT's two lines on the screen: the outcome first ("Added <name>: 9 warnings: REPORT lists them." or, when
// that is too wide, without the name, which DETAILS shows), then what to do next, or with no warnings and an
// import from an install or installer, how many files were taken. `took` is import_folder_report's ("" none).
std::pair<std::string, std::string> mods_import_lines(const ModAddDone& d, const std::string& took,
                                                      const std::function<bool(const std::string&)>& fits);

}  // namespace mech3
