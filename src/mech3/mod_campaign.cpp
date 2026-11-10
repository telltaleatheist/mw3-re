#include "mech3/mod_campaign.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

#include "gamez/zutil/zutl_path.h"
#include "mech3/mission.h"
#include "mech3/pilot.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/reference/reference.h"

namespace mech3 {

namespace fs = std::filesystem;

namespace {
std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
// A whole decimal number in [lo, hi], else -1.
int32_t number(const std::string& s, int32_t lo, int32_t hi) {
  if (s.empty() || s.size() > 4) return -1;
  for (char c : s)
    if (!std::isdigit(static_cast<unsigned char>(c))) return -1;
  const int32_t v = std::atoi(s.c_str());
  return v >= lo && v <= hi ? v : -1;
}
// bases=N: 0..3, the most the original ever holds (0x00434e00 sets 3).
bool take_bases(std::vector<std::string>& words, int32_t* bases, std::string* why) {
  for (size_t i = 0; i < words.size(); ++i) {
    if (lower(words[i]).rfind("bases=", 0) != 0) continue;
    if ((*bases = number(words[i].substr(6), 0, 3)) < 0) return *why = words[i] + ": the field bases are 0 to 3", false;
    words.erase(words.begin() + static_cast<long>(i));
    return true;
  }
  return true;
}
bool plain_name(const std::string& s) { return !s.empty() && s.find_first_of("/\\. \t") == std::string::npos; }
}  // namespace

bool campaign_parse(const std::string& text, ModCampaign* out) {
  std::istringstream in(text);
  std::string op;
  int n = 0, op_line = 0;  // an op= waiting for its first mission
  int32_t op_bases = -1;
  auto refuse = [&](const std::string& line, const std::string& why) {
    out->error = "line " + std::to_string(n) + ": " + line + ": " + why;
    return false;
  };
  for (std::string raw; std::getline(in, raw);) {
    ++n;
    if (n == 1 && raw.rfind("\xef\xbb\xbf", 0) == 0) raw.erase(0, 3);  // a UTF-8 mark
    const std::string line = trim(raw);
    if (line.empty() || line[0] == '#') continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) return refuse(line, "no key=value here");
    const std::string key = lower(trim(line.substr(0, eq))), value = trim(line.substr(eq + 1));
    if (upgrades::reference::key_is(key, "name") || upgrades::reference::key_is(key, "movie_start") || upgrades::reference::key_is(key, "movie_end") || upgrades::reference::key_is(key, "stock")) {
      std::string& field = key == "name" ? out->name : key == "movie_start" ? out->movie_start : key == "movie_end" ? out->movie_end : out->stock;
      if (value.empty()) return refuse(line, "no value");
      if (key != "stock" && !field.empty()) return refuse(line, "a second " + key + "= line");
      if (key == "stock" && lower(value) != "default") return refuse(line, "only stock=default (the original's new-campaign stock) for now");
      if (key != "name" && !plain_name(value)) return refuse(line, "a movie is fmv.zrd's name, one word");
      field = key == "stock" ? "default" : value;
    } else if (upgrades::reference::key_is(key, "op")) {
      std::istringstream ws(value);
      std::vector<std::string> words;
      for (std::string w; ws >> w;) words.push_back(w);
      int32_t bases = -1;
      std::string why;
      if (!take_bases(words, &bases, &why)) return refuse(line, why);
      op.clear();
      for (const std::string& w : words) op += (op.empty() ? "" : " ") + w;
      if (op.empty()) return refuse(line, "an operation needs its heading");
      if (op_line) return out->error = "line " + std::to_string(op_line) + ": an op= with no mission after it", false;
      op_line = n, op_bases = bases;
    } else if (upgrades::reference::key_is(key, "mission")) {
      std::istringstream ws(value);
      std::vector<std::string> words;
      for (std::string w; ws >> w;) words.push_back(w);
      CampaignMission m;
      std::string why;
      if (!take_bases(words, &m.bases, &why)) return refuse(line, why);
      if (words.size() < 2 || words.size() > 4) return refuse(line, "mission=<world> <number> [briefing] [movie] [bases=N]");
      if (!plain_name(words[0])) return refuse(line, "the world is a folder's name, one word");
      if ((m.number = number(words[1], 1, 99)) < 0) return refuse(line, words[1] + ": the mission's number is 1 to 99 (its m<n> block)");
      for (size_t i = 2; i < words.size(); ++i) {
        if (words[i] != "-" && !plain_name(words[i])) return refuse(line, words[i] + ": a briefing or movie is one word, - for none");
        (i == 2 ? m.briefing : m.movie) = words[i] == "-" ? "" : words[i];
      }
      m.world = words[0];
      m.op = op;
      m.line = n;
      // The op's first mission: its bases reset here unless the line sets its own.
      if (op_line && m.bases < 0) m.bases = op_bases;
      op_line = 0;
      if (out->missions.size() >= 99) return refuse(line, "more than 99 missions");
      out->missions.push_back(m);
    } else {
      return refuse(line, "no such key (name, stock, movie_start, movie_end, op, mission)");
    }
  }
  if (op_line) {
    out->error = "line " + std::to_string(op_line) + ": an op= with no mission after it";
  } else if (out->name.empty()) {
    out->error = "no name= line";
  } else if (out->missions.empty()) {
    out->error = "no mission= line";
  }
  return out->error.empty();
}

bool campaign_load(const std::string& mod_path, const std::string& mod, const std::string& file, ModCampaign* out) {
  out->mod = mod;
  out->file = file;
  const fs::path rel = fs::path(file).lexically_normal();
  std::string stem = rel.filename().string();
  if (lower(stem).size() > 4 && lower(stem).compare(stem.size() - 4, 4, ".txt") == 0) stem.resize(stem.size() - 4);
  out->id = stem;
  bool ok_id = !stem.empty() && stem.size() <= 32;
  for (char c : stem) ok_id = ok_id && (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-');
  if (file.empty() || rel.is_absolute() || rel.has_root_name() || file.find("..") != std::string::npos) {
    out->error = "campaign=" + file + ": a file inside the mod";
    return false;
  }
  if (!ok_id) {
    out->error = "campaign=" + file + ": its name (its pilots' folder) is letters, digits, _ and -, up to 32";
    return false;
  }
  std::ifstream f(fs::path(mod_path) / rel);
  if (!f) {
    out->error = "campaign=" + file + ": no such file in the mod";
    return false;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return campaign_parse(ss.str(), out);
}

const CampaignMission* campaign_missing_world(const ModCampaign& c, const std::function<bool(const std::string&)>& known) {
  for (const CampaignMission& m : c.missions)
    if (!known(m.world)) return &m;
  return nullptr;
}

std::vector<ModCampaign>& mod_campaigns() {
  static std::vector<ModCampaign> c;
  return c;
}

std::string campaign_pilots_dir(const ModCampaign& c) { return std::string(kEntriesFolder) + "/" + c.mod + "~" + c.id + "/pilots"; }

namespace {
std::unique_ptr<ModCampaign> g_entered;

// Its entry (upgrades/mods/campaign_entries.h): the game's pilots folder in it (the original's
// separators) and the entry's folder on disk, our marker in it.
std::string entry_id(const ModCampaign& c) { return c.mod + "~" + c.id; }
std::string game_folder(const ModCampaign& c) { return entry_pilots_folder(entry_id(c)); }
fs::path disk_folder(const ModCampaign& c) { return fs::path(zutl::write_path((std::string(kEntriesFolder) + "\\" + entry_id(c)).c_str())); }

// Every pilot let go, each saved while dirty (its destructor), as a new start finds them.
void drop_pilots() {
  current_pilot() = nullptr;
  for (Pilot* p : pilots()) delete p;
  pilots().clear();
}
}  // namespace

const ModCampaign* entered_campaign() { return g_entered.get(); }

bool campaign_enter(const ModCampaign& c, std::string* why) {
  CampaignEntry e;
  e.kind = CampaignEntry::kImported, e.id = entry_id(c), e.name = c.name, e.mod = c.mod, e.campaign = c.id;
  const fs::path dir = disk_folder(c);
  if (!entry_ensure(dir.parent_path().parent_path().string(), e, why)) return false;  // the write dir, two up
  drop_pilots();
  g_entered = std::make_unique<ModCampaign>(c);
  set_pilots_folder(game_folder(c));
  load_pilots();
  return true;
}

void campaign_leave() {
  drop_pilots();
  g_entered.reset();
  set_pilots_folder("pilots");
  load_pilots();
}

bool campaign_pilots_remove(const ModCampaign& c, const std::function<bool()>& confirm, std::string* why) {
  const fs::path dir = disk_folder(c);
  std::error_code ec;
  auto refuse = [&](const std::string& w) {
    if (why) *why = w;
    return false;
  };
  if (g_entered && g_entered->mod == c.mod && lower(g_entered->id) == lower(c.id)) return refuse("the campaign is entered");
  if (!fs::is_directory(dir, ec)) return refuse("no pilots for it");
  if (!fs::is_regular_file(dir / kCampaignMarker, ec)) return refuse(dir.generic_string() + " has no " + kCampaignMarker + ": not the game's");
  if (!confirm || !confirm()) return refuse("not confirmed");
  fs::remove_all(dir, ec);
  return !ec || refuse(ec.message());
}

// ---- the campaign game on the list (pilot.h) ----

CampaignGame* new_campaign_game(Pilot* p, int32_t campaign, int32_t mission) {
  if (g_entered) return new ModCampaignGame(p, mission);
  return new CampaignGame(p, campaign, mission);
}
CampaignGame* read_campaign_game(Pilot* p) {
  if (g_entered) return new ModCampaignGame(p);
  return new CampaignGame(p);
}

ModCampaignGame::ModCampaignGame(Pilot* p, int32_t place) : CampaignGame(p, 1, place) {
  if (const CampaignMission* e = entry(); e && place == 1 && e->bases >= 0) a8 = ac = e->bases;
}

const CampaignMission* ModCampaignGame::entry() const {
  if (!g_entered || mission < 1 || static_cast<size_t>(mission) > g_entered->missions.size()) return nullptr;
  return &g_entered->missions[static_cast<size_t>(mission) - 1];
}

int32_t ModCampaignGame::operation() const {
  const CampaignMission* e = entry();
  if (!e) return 0;
  const std::string w = lower(e->world);
  return w == "c1" ? 1 : w == "c2" ? 2 : w == "c3" ? 3 : 4;
}

int32_t ModCampaignGame::mission_number() const {
  const CampaignMission* e = entry();
  return e ? e->number : 0;
}

int32_t ModCampaignGame::world_index() const {
  const CampaignMission* e = entry();
  return e ? mission_dir_index(e->world.c_str()) : -1;
}

const char* ModCampaignGame::name() {
  const CampaignMission* e = entry();
  if (!e) return (text = "Mission " + std::to_string(mission)).c_str();
  int32_t n = 1;
  for (const CampaignMission* m = e; m != &g_entered->missions.front() && (m - 1)->op == e->op; --m) ++n;
  text = (e->op.empty() ? "" : e->op + ", ") + "Mission " + std::to_string(n);
  return text.c_str();
}

// The list's next line, with its bases= when it sets them (an op= heading's
// first line takes the heading's), else carried; past the last, over.
bool ModCampaignGame::advance(int32_t& op, int32_t& m, int32_t& bases) const {
  op = 1;
  m = mission + 1;
  if (!g_entered || m < 2 || static_cast<size_t>(m) > g_entered->missions.size()) return true;
  const int32_t b = g_entered->missions[static_cast<size_t>(m) - 1].bases;
  bases = b >= 0 ? b : a8;
  return false;
}

const char* ModCampaignGame::intro_movie() const {
  return mission == 1 && g_entered && !g_entered->movie_start.empty() ? g_entered->movie_start.c_str() : nullptr;
}

const char* ModCampaignGame::briefing() const {
  const CampaignMission* e = entry();
  return e && !e->briefing.empty() ? e->briefing.c_str() : nullptr;
}

const char* ModCampaignGame::movie() const {
  const CampaignMission* e = entry();
  return e && !e->movie.empty() ? e->movie.c_str() : nullptr;
}

// A world's start on the list (the original's Op N Mission 1 and Op 4
// Mission 4): the first mission and any whose world differs from the one
// before. 0x004352d0's restore skips it; a replay loads its world afresh.
bool ModCampaignGame::world_start() const {
  const CampaignMission* e = entry();
  return !e || mission == 1 || lower((e - 1)->world) != lower(e->world);
}

bool ModCampaignGame::last() const { return g_entered && static_cast<size_t>(mission) == g_entered->missions.size(); }

bool ModCampaignGame::restore() {
  if (b0 || world_start() || !readable()) return false;
  return pilot_hooks().load_world(path(), 2);
}

}  // namespace mech3
