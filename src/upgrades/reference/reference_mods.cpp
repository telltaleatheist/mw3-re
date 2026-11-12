// The reference's registries for our own files: mod.txt (mech3/mods.cpp
// mod_parse, the mech= and weapon= rows mech3/mech_names.cpp) and a custom
// campaign's file (mech3/mod_campaign.cpp campaign_parse).
#include "upgrades/reference/reference.h"

namespace upgrades::reference {

File mod_txt() {
  return {"mod.txt", "mod.txt", "the mod folder's root (`mods\\<mod>\\mod.txt`)", "ours: `mech3::mod_parse`, `mech_row_parse`, `weapon_row_parse`",
          "`key=value` lines, the key in any case, spaces around either trimmed; `#` starts a comment line. An unknown key is "
          "ignored (a newer mod still loads). `world`, `ia_world`, `mech`, `weapon`, `campaign`, `needs` and `conflicts` may repeat. The values of "
          "`mech=` and `weapon=` are a name and then `key=value` words; there an unknown key refuses the line.",
          {
              {"", "name", "text", "required", "", "The mod's name, shown on the MODS screen. Without it the mod cannot be used."},
              {"", "version", "text", "none", "", "Shown beside the name; EXPORT puts it in the zip's name."},
              {"", "author", "text", "none", "", "Shown in DETAILS."},
              {"", "for", "text: `mw3` or `mw3pm`, a version may follow", "either game", "the first word, any case",
               "Which game the mod is for; MechWarrior 3 leaves out a `mw3pm` mod, Pirate's Moon a `mw3` one."},
              {"", "description", "text", "none", "", "A line about the mod."},
              {"", "world", "a folder's name", "none", "", "A new world: its files in `zbd\\<folder>\\`, its `<folder>.gs`."},
              {"", "ia_world", "`<folder> <name>`", "none", "", "Puts a world on Instant Action's list under the name given."},
              {"", "mech", "`NAME tech=… mass=… class=… [model=…] [title=…]`", "none", "",
               "A new mech, numbered after the game's own; its definition is `dfn_NAME.zrd`."},
              {"mech=", "tech", "`clan`, `sphere` or `any`", "required", "", "Which side may field it."},
              {"mech=", "mass", "int (tons)", "required", "10 to 100, in steps of 5", "Its tonnage."},
              {"mech=", "class", "`light`, `medium`, `heavy` or `assault`", "required", "", "Its weight class."},
              {"mech=", "model", "a mech's name", "NAME", "", "The mech whose models, motions and sounds it uses."},
              {"mech=", "title", "text, the rest of the line", "NAME", "", "The name shown; last on the line."},
              {"", "weapon", "`NAME ammo=… tech=… [title=…]`", "none", "",
               "A new weapon, numbered after the game's own; its definition is weapons.zrd's of that name."},
              {"weapon=", "ammo", "`yes` or `no`", "required", "", "Whether it takes ammunition."},
              {"weapon=", "tech", "`clan`, `sphere`, `any` or `none`", "required", "", "Which side's mech lab offers it."},
              {"weapon=", "title", "text, the rest of the line", "NAME", "", "The name shown; last on the line."},
              {"", "campaign", "a file in the mod", "none", "inside the mod's folder", "A custom campaign's file (the next section)."},
              {"", "anim", "`defs`", "none", "",
               "Its animations by its def files alone (`zbd\\<world>\\anim\\` and `zbd\\anim\\`): its whole anim files, kept for the "
               "original game, are not ours to read, so it combines with other def-file mods."},
              {"", "needs", "`<mod> [version]`", "none", "the last word a version when it starts with a digit",
               "Another mod it needs, by folder or `name=` (any case), and its least version. The MODS screens warn when it is "
               "missing, off or older; the mod is still turned on."},
              {"", "conflicts", "`<mod>`", "none", "", "A mod it does not work with, by folder or `name=`. The MODS screens warn "
                                                       "when both are on; neither is stopped."},
              {"", "game_version", "a version (`1.1.0`)", "none", "", "The least MechWarrior 3 Rebuild version it needs; an older "
                                                                       "program warns on the MODS screens and still loads it."},
              {"", "kind", "`campaign` or `assets`", "by its contents", "",
               "Where MODS lists it: a campaign of its own, or an asset mod ticked per campaign. Without it, a mod with "
               "`campaign=` or `world=` lines or files of the game's own worlds (`zbd\\c1\\` to `zbd\\t1\\`) is a campaign."},
              {"", "base", "an entry's id", "none", "written by the game",
               "A copy's: the campaign it was saved from (`mw3`, or `<mod>~<campaign>`), mounted under it."},
              {"", "salvage_chance", "int", "the original's 1 in 2", "0 to 100",
               "How many times in 100 a kill's salvage is offered, in every mission of the campaign the mod is ticked for: a "
               "destroyed vehicle's weapons, and a destroyed mech that lost a leg. The first ticked mod that sets it wins."},
              {"", "mission_salvage", "`<world> <n> <N>`", "none", "N 0 to 100; one line a mission",
               "The same for one mission (its world's folder and number), over `salvage_chance`."},
          }};
}

File campaign() {
  return {"campaign", "A custom campaign's file", "the mod, named by mod.txt's `campaign=`",
          "ours: `mech3::campaign_parse`",
          "`key=value` lines as mod.txt's (a UTF-8 mark at the start allowed). Unlike mod.txt, an unknown key or a line "
          "without `=` refuses the campaign whole, its line named. `op=` and `mission=` repeat, in play order; every `op=` "
          "needs a `mission=` after it.",
          {
              {"", "name", "text", "required", "once", "The campaign's name on the MODS screen."},
              {"", "stock", "`default`", "default", "only `default` for now", "The mechs and stock a new campaign starts with."},
              {"", "movie_start", "fmv.zrd's name, one word", "none", "once", "Played before the first briefing."},
              {"", "movie_end", "fmv.zrd's name, one word", "none", "once", "Played when the last mission is won."},
              {"", "op", "`<heading> [bases=N]`", "none", "bases 0 to 3", "An operation's heading for the missions after it; "
                                                                           "`bases=` resets the field bases at its first mission."},
              {"", "mission", "`<world> <number> [briefing] [movie] [bases=N]`", "required (one at least)",
               "number 1 to 99; 99 lines at most; `-` for no briefing or movie; bases 0 to 3",
               "A mission: the world's folder and its `m<number>` block, then the briefing and movie by name."},
          }};
}

}  // namespace upgrades::reference
