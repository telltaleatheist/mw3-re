#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Ours (Owen, 2026-11-09; docs/editor.md, New Weapon): the main menu's EDIT
// WEAPONS row, empty in the release (0x004bf620), opens New Weapon when the
// editor came from MODS's EDITOR: a weapon copied from one the game knows,
// its row (name, ammo, tech, title) and main numbers set, written into the
// mod: `weapon=` in its mod.txt and the definition at the end of BALLISTICS
// in its reader.zbd's weapons.zrd, or in its loose zbd\weapons.zrd as text
// when it has one (docs/modding.md, A new weapon). The lab offers it from the
// next start. Its effects and sounds are picked from those the shipped
// weapons use, each shown at the view's pick when picked. The dialog is a
// form (form.h), drawn by ImGui.
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

// The effects and sounds: a block's key (FIRE, FLYOUT, IMPACT's default),
// its value's first item a name.
enum EffectSlot { kMuzzle, kFireSound, kProjectile, kProjectileAnim, kTrail, kFlybySound, kImpact, kImpactSound, kEffectSlots };
struct EffectSlotInfo {
  const char* label;  // the form's
  const char* block;  // FIRE, FLYOUT or IMPACT (its `default`)
  const char* key;
  char kind;          // 'a' an animation, 's' a sound, 'm' a model (no preview)
};
const EffectSlotInfo& effect_slot(int slot);
constexpr const char* kNoEffect = "(none)";
struct WeaponEffects {
  std::string value[kEffectSlots];  // a name; kNoEffect: left out; "": the copy's as it is
};
bool weapon_effects_of(const uint8_t* zrd, size_t size, const std::string& name, WeaponEffects* out);
// Every name the file's definitions use in `slot`, sorted, once each.
std::vector<std::string> effect_choices(const uint8_t* zrd, size_t size, int slot);
// The preview: a slot's pick shown once at the view's pick (the editor's
// ground pick, else its camera). The tests set these; null: the engine's
// (zeff::anim_find and anim_start_at; zsnd::find and play_at). False when
// the name is not the world's or the game's.
struct NewWeaponPreview {
  std::function<bool(const std::string& name, const float pos[3])> anim, sound;
};
NewWeaponPreview& new_weapon_preview();

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
                                             bool ammo, const WeaponEffects* fx = nullptr);
// The same definition alone (its list's bytes), empty as above.
std::vector<uint8_t> weapon_definition(const uint8_t* zrd, size_t size, const std::string& copy, const std::string& name,
                                       const WeaponNumbers& n, int32_t tech, bool ammo, const WeaponEffects* fx = nullptr);
// A zrd node's bytes as the text reader reads them back (zrdr::parse_text):
// lists, quoted strings, ints, floats with a '.'.
std::string zrd_text_of(const uint8_t* p, size_t size);
// A loose weapons.zrd's text with `name` and its definition's text after
// BALLISTICS' last item, the modder's own text (comments, layout) as it was;
// "" and `why` when it has no BALLISTICS list to put it in.
std::string weapons_text_with_definition(const std::string& text, const std::string& name, const std::string& def_text,
                                         std::string* why);
// A loose weapons.zrd's text as the binary file it reads as (zrdr::parse_text,
// then its tree's bytes); empty when it does not read.
std::vector<uint8_t> weapons_text_bytes(const std::string& text, std::string* why);
// mod.txt's lines with `line` ("weapon=..."): in place of the weapon= line
// naming the same weapon (any case), else after the last line.
std::string mod_txt_with_weapon(const std::string& text, const std::string& name, const std::string& line);

}  // namespace mech3
