#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Ours (docs/modding.md, mech3ax's JSON): mechlib.zbd's entries as unzbd
// writes them, turned back into the entry's bytes for the archive's own
// readers (cls_mechlib.cpp: mechlib_read_materials 0x004fc9e0, mechlib_load
// 0x004fc8f0), as mech3ax's writer lays them out (gamez/src/mechlib/mw).
namespace zutl_json { struct Value; }

namespace zcls {

// `materials.json`: a list of {"Textured": {texture_name, ptr}} or
// {"Colored": {color {r, g, b}, alpha}}.
bool mechlib_materials_from_json(const std::string& json, std::vector<uint8_t>* entry, std::string* why);
// `<model>.flt.json`: {"nodes": [...], "models": [...]}, the tree from node 0
// down, each node's record, its object3d and its model.
bool mechlib_model_from_json(const std::string& json, std::vector<uint8_t>* entry, std::string* why);

// One model's JSON (unzbd's, as in a mechlib tree or gamez's models.json) as its 0x5c record and its data
// (vertices, normals, morphs, lights, polygons): for gamez's model array, which keeps them apart. `path`
// prefixes the errors' places ("$[12]").
bool model_from_json(const zutl_json::Value& model, const std::string& path, std::vector<uint8_t>* record, std::vector<uint8_t>* data,
                     std::string* why);

// A node's object3d data (0x90), from its JSON node (`data.Object3d`), as in a mechlib tree.
bool object3d_from_json(const zutl_json::Value& node, const std::string& path, std::vector<uint8_t>* out, std::string* why);

}  // namespace zcls
