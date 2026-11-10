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

// Ours (docs/modding.md, Animations a def at a time): one def's JSON, the
// file unzbd writes for it, as its bytes in the file (its record, lists,
// reset state and sequences), and its key, unzbd's file name for it (the
// node, anim and root names less ".flt": node-anim, or node-anim-root when
// the root is another node). An ObjectMotionSiScript event may name its
// script's file ("script": "si-script-<name>.zan"), read by `file` as
// "<name>.json"; with an "index" there is no script list here to index.
bool anim_def_from_json(const std::string& text, const std::function<std::string(const std::string&)>& file,
                        std::vector<uint8_t>* out, std::string* key, std::string* why);
std::string anim_def_key(const std::string& name, const std::string& anim_name, const std::string& root_name);

// A world's anim.zbd (its game path, zbd\\c1\\anim.zbd) as an enabled mod's
// JSON, first mod first, up to the first with its own anim.zbd: the folder
// zbd\\c1\\anim\\ (unzbd's files, metadata.json first), else unzbd's
// zbd\\c1\\anim.zip. 1: `zbd` holds the file, `what` where it came from;
// -1: one there that does not read (`what`, `why`); 0: none.
// `skip` (ours, modding.md's anim=defs): a mod passed over, its whole files not read.
int anim_from_mod(const char* game_path, std::vector<uint8_t>* zbd, std::string* what, std::string* why,
                  const std::function<bool(size_t mod)>& skip = nullptr);

}  // namespace zeff
