// Ours: New Weapon's tips (help.h), beside new_weapon.cpp's form. Each number
// says the weapons.zrd key it is written to (docs/weapons.md, Definitions).
#include "upgrades/editor/help.h"

namespace mech3 {

void help_weapons(std::vector<HelpForm>& out) {
  out.push_back(
      {"New Weapon", "New Weapon (EDIT WEAPONS)",
       "A new weapon for your copy of the campaign the editor was opened on from MODS, MISSIONS (the first save asks "
       "the copy's name; the campaign itself and an imported mod are never written). It is part of that campaign: "
       "on whenever the copy is played. It starts as a copy of a weapon the "
       "game knows, under a free name; set its row, its main numbers, its effects and sounds, then OK. Every other "
       "number is the copy's. An effect or sound is picked from those the game's weapons use, and shown once where "
       "the view points when picked. OK writes the weapon= line into the copy's mod.txt (the old file kept as "
       "mod.txt.bak) and the definition at the end of BALLISTICS in the copy's zbd\\reader.zbd (weapons.zrd; the "
       "archive it replaced kept as .bak), or, when the copy has its own zbd\\weapons.zrd text file, into that "
       "file (its own text kept, the old one as .bak). The mech lab offers it from the next start. The original "
       "game reads the file too and leaves the extra weapon alone.",
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
           {"muzzle", "", "The animation at the firing end as it fires (FIRE ANIMATION). Shown where the view points."},
           {"firing sound", "", "The sound of a shot (FIRE SOUND). Played where the view points."},
           {"projectile", "", "The model that flies (FLYOUT MODEL): a shell, a missile, a beam's model. Not shown here."},
           {"projectile animation", "", "The animation on the flying model (FLYOUT MODEL_ANIMATION)."},
           {"trail", "", "The trail behind the shot (FLYOUT ANIMATION)."},
           {"flyby sound", "", "The sound while it flies, or a beam's while it is on (FLYOUT SOUND)."},
           {"impact", "", "The animation where it hits (IMPACT default ANIMATION); the other soils' are the copy's."},
           {"impact sound", "", "The sound where it hits (IMPACT default SOUND)."},
           {"OK", "", "Write it into your copy of the campaign (mod.txt and zbd\\reader.zbd, or its own zbd\\weapons.zrd), each file's earlier copy kept as .bak."},
           {"Cancel", "", "Close it; nothing is written."},
       }});
}

}  // namespace mech3
