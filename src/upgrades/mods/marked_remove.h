#pragma once
#include <string>

// Ours (docs/mods-campaigns-plan.md, Deleting and resetting): the one remover
// of DELETE and RESET, on its own (no other code of the game's: the Windows
// check tests/win/delete_check.cpp links it alone).
namespace mech3 {

// The path is a link of any kind: a symlink, and on Windows any reparse point
// (a directory junction, mklink /J, is one, and the standard library may call
// it a plain directory). A link is never entered and never taken for a folder
// or a file of ours; removed, it is removed as itself, its target untouched.
bool path_is_link(const std::string& path);

// `under` is "mods" or "campaigns"; `name` one plain folder name; `marker`
// the file the folder must hold, a real file. Links inside are removed as
// links, never followed. False with `why`, nothing removed, when: the name is
// not plain, the folder or its parent is missing or is a link, the marker is
// missing or is a link, or (campaigns) it holds a folder the game does not
// make there (anything but pilots and configs).
bool remove_marked_folder(const std::string& write_dir, const char* under, const std::string& name, const char* marker, std::string* why);

// A folder of ours anywhere (a staging folder: its path whole), with the same
// rule: a real folder, not a link, holding `marker` as a real file; removed
// whole, links inside removed as links and never entered. False with `why`,
// nothing removed, otherwise.
bool remove_marked_tree(const std::string& dir, const char* marker, std::string* why = nullptr);

}  // namespace mech3
