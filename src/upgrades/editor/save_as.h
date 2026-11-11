#pragma once
#include <functional>
#include <string>

#include "upgrades/editor/form.h"

// Ours (docs/mods-campaigns-plan.md, EDIT; step 4b): the editor only ever
// saves as. Inside a campaign entry its saves go into a copy of that
// campaign: a mod folder of ours (the editor's marker, net_save.h's
// kEditorMarker) whose mod.txt names what it was saved from (base=<entry>)
// and which holds only the files changed (a file that is one archive of many
// records, a world's reader archive or reader.zbd, is copied whole at its
// first edit). The first save of a session asks for the copy's name once;
// later saves go into it. Editing a copy saves into the copy itself. The
// game's own campaign, an imported mod and a folder made by hand are never
// written: a folder without the marker is refused as a save's target, always.
namespace mech3 {

// The mod folder is the editor's own (its marker in it): the only kind a save may write.
bool editor_owns_mod(const std::string& write_dir, const std::string& folder);
// "<campaign>, my edit", then "..., my edit 2" and on while a mod folder of that name exists.
std::string save_as_default_name(const std::string& write_dir, const std::string& entry);
// The copy made: mods/<folder from name> with the marker and a mod.txt (name=,
// kind=campaign, base=<entry>, the base's for=), its entry given the base's
// ticks. Refused, with `why` and nothing made: no name, a name with nothing a
// folder can be called, a folder of that name already there (never written
// over), an entry that is not among the campaigns.
bool save_as_make_copy(const std::string& write_dir, const std::string& entry, const std::string& name, std::string* folder,
                       std::string* why);

// The session inside an entry (set by the program's edit start): the entry
// edited, and the copy the saves go into once there is one ("" until then;
// the entry's own mod when the entry is a copy of ours).
struct SaveAsSession {
  std::string write_dir, entry;
  std::string copy;      // the folder saves go into
  bool made = false;     // this session made it (the leave chooses it on MODS)
};
SaveAsSession& save_as_session();
void save_as_begin(const std::string& write_dir, const std::string& entry);  // inside an entry: the session, the target set
void save_as_end();

// A save command's gate: true when a save may go on now (outside an entry,
// or the copy is there). Inside an entry with no copy yet it opens the Save
// as box (the name, Save, Cancel) and returns false; Save makes the copy and
// runs `again` (the same command), Cancel says nothing was saved.
bool editor_save_ready(std::function<void()> again);
EditorForm& save_as_form();  // the box ("Save as a new campaign"), for the view's tips and the checks
// The box's answer for checks: set, it is asked in place of the box (false: cancelled).
std::function<bool(std::string* name)>& save_as_ask_hook();

}  // namespace mech3
