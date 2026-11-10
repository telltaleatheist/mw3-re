#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Ours (docs/modding.md, gamez JSON): a world's gamez.zbd as unzbd writes it
// (metadata.json, textures.json, materials.json, models.json, nodes.json)
// turned back into the file cls_zbd 0x004ff520 reads, laid out as mech3ax's
// writer lays it (crates/gamez/src/gamez/mw). The big files are walked an
// item at a time (zutl_json::Pull).
namespace zcls {

// `file` gives a JSON file's text by name ("" when there is none). Until the
// models and nodes are read, `zbd` holds the header (its later offsets 0),
// the texture directory and the materials, and the call fails naming them.
bool gamez_from_json(const std::function<std::string(const std::string&)>& file, std::vector<uint8_t>* zbd, std::string* why);

// A world's gamez.zbd (its game path, zbd\\c1\\gamez.zbd) as an enabled mod's
// JSON, first mod first, up to the first with its own gamez.zbd: the folder
// zbd\\c1\\gamez\\ (unzbd's five files), else unzbd's zbd\\c1\\gamez.zip.
// 1: `zbd` holds the file, `what` where it came from; -1: one there that does
// not read (`what`, `why`); 0: none.
int gamez_from_mod(const char* game_path, std::vector<uint8_t>* zbd, std::string* what, std::string* why);

}  // namespace zcls
