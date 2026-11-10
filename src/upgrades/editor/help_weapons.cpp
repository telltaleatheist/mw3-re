// Ours: New Weapon's tips (help.h), beside new_weapon.cpp's form. Each number
// says the weapons.zrd key it is written to (docs/weapons.md, Definitions).
#include "upgrades/editor/help.h"

namespace mech3 {

void help_weapons(std::vector<HelpForm>& out) {
  out.push_back(
      {"New Weapon", "New Weapon (EDIT WEAPONS)",
       "A new weapon for the mod the editor was opened on from MODS, EDITOR. It starts as a copy of a weapon the "
       "game knows, under a free name; set its row and its main numbers, then OK. The weapon's effects, sounds and "
       "every other number are the copy's. OK writes the weapon= line into the mod's mod.txt (the old file kept as "
       "mod.txt.bak) and the definition at the end of BALLISTICS in the mod's zbd\\reader.zbd (weapons.zrd; the "
       "archive it replaced kept as .bak). The mech lab offers it from the next start. The original game reads "
       "the file too and leaves the extra weapon alone.",
       {
           {"copy of", "", "The weapon it starts from: every number below, and all the rest, are this one's."},
           {"name", "",
            "The weapon's name in weapons.zrd and mod.txt, up to 31 letters, digits, _ and -. A name any weapon "
            "already has is refused."},
           {"ammo", "", "yes: the lab sells rounds for it and its bin empties as it fires. no: it never runs out."},
           {"tech", "", "The techs the lab offers it in: Clan, Inner Sphere, any, or none (as TAG)."},
           {"title", "", "The name the lab shows; empty or the same as the name: the name."},
           {"damage", "", "The damage of a hit (DAMAGE)."},
           {"heat", "", "The heat a shot adds to the mech that fires it (HEAT's first value)."},
           {"range", "", "How far it reaches, in metres (RANGE)."},
           {"fire rate", "", "Shots a second (FIRE_RATE): 0.25 is one every four seconds."},
           {"velocity", "", "How fast its shot flies, metres a second (VELOCITY). A beam's own is kept: not written."},
           {"Clan tons", "", "Its weight in a Clan mech's lab (MASS clan); written when it is offered to the Clans."},
           {"Clan slots", "", "The critical slots it takes in a Clan mech (MASS clan)."},
           {"Inner Sphere tons", "", "Its weight in an Inner Sphere mech's lab (MASS sphere); written when offered there."},
           {"Inner Sphere slots", "", "The critical slots it takes in an Inner Sphere mech (MASS sphere)."},
           {"rounds a ton", "", "With ammo: the rounds one ton of its ammo holds (MASS ammo)."},
           {"OK", "", "Write it into the mod (mod.txt and zbd\\reader.zbd), each file's earlier copy kept as .bak."},
           {"Cancel", "", "Close it; nothing is written."},
       }});
}

}  // namespace mech3
