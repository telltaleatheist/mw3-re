#pragma once
#include <string>
#include <vector>

// Ours (docs/modding.md, A world the game does not have; playtest Z2): a mod's world folder that is none of
// the game's, found by its files: zbd\<name>\gamez.zbd (or our gamez JSON or zip form), <name> none of the
// game's nine (c1 c2 c3 c4 c4b t1 lab dev1 dev2, any case). What a zStudio export of a new world holds is such
// a folder and an interp.zbd carrying the world's build script, <name>.gs. IMPORT writes the mod.txt lines the
// mount needs for it (world=, ia_world=, a campaign file of its missions) into the mod.txt it makes; the check
// and INFO say what it found.
namespace mech3 {

struct FoundWorld {
  std::string folder;                       // as the mod spells it
  std::vector<int> missions, ia_maps, mp_maps;  // readerm<N>, readeria<N>, readermp<N>: the Ns, in order
  bool script_root = false;                 // <folder>.gs at the mod's root (a text script)
  bool script_archive = false;              // <folder>.gs in the mod's zbd\interp.zbd
  // The folder its script reads the scenery from (its DB: gamez.zbd and anim.zbd), when the script
  // names another world's ("source c1.gs", "set DB c1"); "" for its own.
  std::string reads_from;
  std::vector<std::string> lacks;           // the files a mission on it reads from its folder that it has not
};

// One of the game's nine world folders (any case).
bool game_world_name(const std::string& name);
// A name the mount takes for a world (no slash, dot or space; not empty).
bool world_name_ok(const std::string& name);
// The scripts interp.zbd's directory names ("c1.gs", "Support\\c1.gw"); empty when it is not one.
std::vector<std::string> interp_archive_names(const std::string& file);
// One script of interp.zbd as its lines' words (each line's words as the archive keeps them); empty when the
// archive has no such script (any case) or it does not read inside the file.
std::vector<std::vector<std::string>> interp_archive_script(const std::string& file, const std::string& name);
// The mod's new worlds under `root`, by folder name. Never reads outside `root`.
std::vector<FoundWorld> found_worlds(const std::string& root);

// IMPORT (a mod with no mod.txt of its own): the lines for its new worlds, each world's campaign file written
// at `root` when it has missions (its file and id from the world's name, a name no file at the root has), and
// the files written. `log` gets a line for each thing done.
std::vector<std::string> found_worlds_mod_txt(const std::string& root, const std::vector<FoundWorld>& worlds,
                                              std::vector<std::string>* written, std::vector<std::string>* log);

// INFO and REPORT: one sentence a world ("The world mymap has 4 missions, 3 Instant Action maps and 2
// multiplayer maps."), and what it lacks.
std::vector<std::string> found_world_sentences(const FoundWorld& w);

}  // namespace mech3
