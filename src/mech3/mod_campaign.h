#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Ours (docs/custom-campaigns.md): a mod's campaigns, each a text file named
// by a campaign= line in mod.txt (several per mod), one mission a line in
// play order. The original's campaign is code (its operations' sizes, the
// advance, the folders by index); a custom one is this list, played through
// the original's screens. A campaign with any line that does not read, or a
// mission on a world no enabled mod or the install has, is left out whole.
namespace mech3 {

struct CampaignMission {
  std::string world;     // a mission folder: the game's (c1..c4b, t1) or a mod's world=
  int32_t number = 0;    // its m<n> block in world.zrd and start animation
  std::string briefing;  // briefing.zrd's block; "" brief_<world><n> (0x00405230's)
  std::string movie;     // fmv.zrd's, before the briefing; "" none
  int32_t bases = -1;    // the field bases (+0xa8) set here, -1 carried on
  std::string op;        // the op= heading it falls under ("" before any)
  int line = 0;          // its line in the file
};

struct ModCampaign {
  std::string mod;   // the mod's folder (what mod= names)
  std::string file;  // as campaign= gives it, relative to the mod
  std::string id;    // the file's name without .txt: its pilots' folder
  std::string name, stock = "default", movie_start, movie_end;
  std::vector<CampaignMission> missions;
  std::string error;  // why it cannot be played, "" when it can
};

// The file's lines: name=, stock=default, movie_start=, movie_end=,
// op=<heading> [bases=N], mission=<world> <n> [briefing|-] [movie|-]
// [bases=N]; # a comment. Every line must read (a typo would drop a
// mission): false with `out->error` naming the first line that does not.
bool campaign_parse(const std::string& text, ModCampaign* out);

// campaign=<file> of the mod at `mod_path` (folder name `mod`): the name
// checked (a plain file in the mod, its id letters, digits, _ and -), read
// and parsed. False with `out->error`.
bool campaign_load(const std::string& mod_path, const std::string& mod, const std::string& file, ModCampaign* out);

// The first mission whose world `known` refuses, null when all are known.
const CampaignMission* campaign_missing_world(const ModCampaign& c, const std::function<bool(const std::string&)>& known);

// The campaigns of the mounted mods, in the mods' order (mods_mount).
std::vector<ModCampaign>& mod_campaigns();

// Its pilots, relative to the write dir: pilots/mods/<mod>/<id>, the
// original's pilots/ layout inside (custom-campaigns.md, question 2).
std::string campaign_pilots_dir(const ModCampaign& c);

// The file our code puts in a campaign's pilots folder when it makes it;
// only a folder holding it is ever removed (campaign_pilots_remove).
constexpr const char* kCampaignMarker = "mw3-campaign.txt";

// The campaign entered (a copy), null while the original's pilots are up.
const ModCampaign* entered_campaign();
// Into the campaign's own pilots: its folder made with our marker (an
// existing one kept when it has the marker or is empty; one holding other
// files refused, `why`), every pilot listed now saved and let go, the
// pilots folder set to it and its pilots loaded; their campaign games are
// ModCampaignGames on its list. campaign_leave goes back to the original's.
bool campaign_enter(const ModCampaign& c, std::string* why);
void campaign_leave();
// The player's removal of every pilot of a campaign, on their choice only:
// nothing happens unless `confirm` says yes, and only a folder with our
// marker is removed (never the one entered). False with `why` otherwise.
bool campaign_pilots_remove(const ModCampaign& c, const std::function<bool()>& confirm, std::string* why);

}  // namespace mech3
