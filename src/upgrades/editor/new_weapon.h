#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Ours (Owen, 2026-11-09; docs/editor.md, New Weapon): the main menu's EDIT
// WEAPONS row, empty in the release (0x004bf620), opens New Weapon when the
// editor came from MODS's EDITOR: a weapon copied from one the game knows,
// its row (name, ammo, tech, title) and main numbers set, written into the
// mod: `weapon=` in its mod.txt and the definition at the end of BALLISTICS
// in its reader.zbd's weapons.zrd (docs/modding.md, A new weapon). The lab
// offers it from the next start. The dialog is a form (form.h), drawn by
// ImGui. The effects and sounds are a later sitting.
namespace mech3 {

struct EditorForm;

void new_weapon_open(int32_t y);  // the EDIT WEAPONS row
EditorForm& new_weapon_form();
const std::string& new_weapon_last();  // what OK wrote last ("" when nothing)

// A definition's main numbers, as weapons.zrd holds them.
struct WeaponNumbers {
  bool beam = false;           // BEAM: the velocity is the beam's own (not edited)
  float damage = 0, heat = 0, range = 0, fire_rate = 0, velocity = 0;  // DAMAGE, HEAT's first, RANGE, FIRE_RATE, VELOCITY
  int32_t clan_tons = 0, clan_slots = 0;      // MASS clan (the sphere's when the copy has none)
  int32_t sphere_tons = 0, sphere_slots = 0;  // MASS sphere (the clan's when the copy has none)
  int32_t rounds = 0;           // MASS ammo: rounds a ton
};

// BALLISTICS' names in a weapons.zrd entry, in their order.
std::vector<std::string> weapons_defined(const uint8_t* zrd, size_t size);
// The copies offered: those of BALLISTICS the lab knows (the exe's 37, the
// mods' rows), in BALLISTICS' order.
std::vector<std::string> weapon_copies(const uint8_t* zrd, size_t size);
bool weapon_numbers_of(const uint8_t* zrd, size_t size, const std::string& name, WeaponNumbers* out);
// The first of <copy>_B, _C, ... _Z, _B2 ... that no weapon and no
// definition has.
std::string weapon_free_name(const std::string& copy, const std::vector<std::string>& defined);
// The entry's bytes with `name` = `copy`'s definition (its NAME, if it has
// one, made `name`), the numbers set, MASS's tons and slots under each tech
// `tech` offers it in (1 Clan, 2 Inner Sphere) and its rounds when `ammo`,
// appended after BALLISTICS' last; every other byte as it was. Empty when
// the bytes are not a weapons.zrd or `copy` is not in it.
std::vector<uint8_t> weapons_with_definition(const uint8_t* zrd, size_t size, const std::string& copy,
                                             const std::string& name, const WeaponNumbers& n, int32_t tech,
                                             bool ammo);
// mod.txt's lines with `line` ("weapon=..."): in place of the weapon= line
// naming the same weapon (any case), else after the last line.
std::string mod_txt_with_weapon(const std::string& text, const std::string& name, const std::string& line);

}  // namespace mech3
