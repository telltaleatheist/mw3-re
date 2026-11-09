#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Ours (docs/modding.md, mech3ax's JSON): mechlib.zbd's entries as unzbd
// writes them, turned back into the entry's bytes for the archive's own
// readers (cls_mechlib.cpp: mechlib_read_materials 0x004fc9e0, mechlib_load
// 0x004fc8f0), as mech3ax's writer lays them out (gamez/src/mechlib/mw).
namespace zcls {

// `materials.json`: a list of {"Textured": {texture_name, ptr}} or
// {"Colored": {color {r, g, b}, alpha}}.
bool mechlib_materials_from_json(const std::string& json, std::vector<uint8_t>* entry, std::string* why);
// `<model>.flt.json`: {"nodes": [...], "models": [...]}, the tree from node 0
// down, each node's record, its object3d and its model.
bool mechlib_model_from_json(const std::string& json, std::vector<uint8_t>* entry, std::string* why);

}  // namespace zcls
