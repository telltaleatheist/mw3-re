#pragma once
// Ours: the one form every warning about a mod's file takes, for the mod's maker to fix the file from (Owen,
// 2026-11-11). Four parts in plain words, no code terms beyond the file's own names:
//   where     the file, then the entry (or texture, row, mission) in it
//   wrong     what is wrong in data terms, with the offset or item number ("its list 8 bytes in says 38 items and
//             the entry holds 37")
//   did       what the rebuild did with it ("read the 37 and the entry after it as the rest")
//   original  what the original game does with it, when the exe says ("reads on into the next entry, so it works
//             there by luck"; "can crash when this map is loaded"); "not known" rather than a guess
//   fix       optional: what to change in the file to put it right ("its count should be 37")
// A warning's group in the report goes by what happened: something left out stops part of the mod working; else
// an `original` that says "crash" is one that may crash the original game; the rest are harmless slips.
// Every reader that warns about a mod's data fills a ModWarning; mod_warning_line makes the line the check, the
// report and the log all show.
#include <string>

namespace mech3 {

struct ModWarning {
  std::string wrong, did, original, fix;
  // Optional: which one of many in the file ("table entry 3361"), said after the place. Kept out of `wrong` and
  // `fix` so that the same fault in many items reads the same, and the report says it once for them all.
  std::string item;
};

inline std::string mod_warning_line(const std::string& where, const ModWarning& w) {
  return where + (w.item.empty() ? "" : " (" + w.item + ")") + ": " + w.wrong + ". The rebuild: " + w.did + ". The original game: " + (w.original.empty() ? "not known" : w.original) +
         "." + (w.fix.empty() ? "" : " To fix it: " + w.fix + ".");
}

// The report's three groups, in its order.
enum class ModWarningGroup { kStops, kMayCrash, kSlip };
inline ModWarningGroup mod_warning_group(const ModWarning& w, bool left_out) {
  if (left_out) return ModWarningGroup::kStops;
  return w.original.find("crash") != std::string::npos ? ModWarningGroup::kMayCrash : ModWarningGroup::kSlip;
}

}  // namespace mech3
