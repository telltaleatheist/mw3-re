#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Ours (docs/modding.md, mech3ax's JSON): a world's anim.zbd as unzbd writes
// it (metadata.json, a <def>.json per anim def, si-script-NNN.zan.json per
// script), turned back into the file the anim reader 0x0050a0e0 takes, laid
// out as mech3ax's writer lays it (crates/anim/src/mw, crates/anim-events).
// Names are zero-padded after their NUL where mech3ax restores the shipped
// padding from tables of its own (anim-names): those bytes are never read.
namespace zeff {

// `file` gives a JSON file's text by name ("" when there is none). `names`
// gets the byte ranges past a name's NUL that the shipped file may hold
// otherwise (for a comparison that masks them); may be null. On a def that
// does not read, `zbd` holds the file up to the defs before it.
bool anim_from_json(const std::function<std::string(const std::string&)>& file, std::vector<uint8_t>* zbd,
                    std::vector<std::pair<size_t, size_t>>* names, std::string* why);

}  // namespace zeff
