#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Ours (docs/modding.md, Saves that hold a mod's mechs and weapons): a
// pilot's or loadout's file keeps the original's numbers, and beside it
// `<file>.rows` names the mods' rows those numbers mean (`mech 26 thor_b`,
// `weapon 56 ERPPC_B`), so a file loads with its own mechs and weapons
// whatever the mods' order, and is refused, naming the row, when no enabled
// mod adds it. The calls between a begin and its end note or translate the
// numbers; a file begun inside another's keeps its own (a stack).
namespace mech3::rows_file {

// Writing: begin, the writers note each number (only the mods' rows count),
// then end writes <path>.rows beside the file, or deletes an old one when
// the file now holds none. `path` is the game's (the write dir's) name.
void write_begin();
void note_mech(int32_t mech);
void note_weapon(int32_t weapon);
void write_end(const std::string& path);

// Reading: begin reads <path>.rows when there is one; the readers pass each
// number through mech() / weapon() (the exe's numbers unchanged, a row's
// translated to its number now); end is false with `error` when the file
// named a row no enabled mod adds, held a row number past the table with no
// sidecar to name it, or its sidecar has a line that does not read (then
// the file is refused, never read with the raw numbers). A sidecar is
// ignored when the file holds no row numbers.
void read_begin(const std::string& path);
int32_t mech(int32_t mech);
int32_t weapon(int32_t weapon);
bool read_end(std::string* error);

// The sidecar's name, and the files' moves: a rename or delete of a file
// takes its sidecar with it (only a file named exactly <file>.rows).
std::string sidecar(const std::string& path);
void move_sidecar(const std::string& from, const std::string& to);
void delete_sidecar(const std::string& path);

// What was refused this run, each a line for the log (`mw3: ...`).
std::vector<std::string>& refusals();

}  // namespace mech3::rows_file
