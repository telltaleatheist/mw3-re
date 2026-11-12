#include "upgrades/mods/found_worlds.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "gamez/zutil/zutl_path.h"

namespace fs = std::filesystem;

namespace mech3 {
namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// "readerm12.zbd" with prefix "readerm": 12; -1 when not one.
int numbered(const std::string& file, const char* prefix) {
  const std::string l = lower(file);
  const size_t p = std::strlen(prefix);
  if (l.size() <= p + 4 || l.compare(0, p, prefix) != 0 || l.compare(l.size() - 4, 4, ".zbd") != 0) return -1;
  const std::string digits = l.substr(p, l.size() - p - 4);
  if (digits.empty() || digits.size() > 4 || !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }))
    return -1;
  return std::stoi(digits);
}

std::string plural(size_t n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }

std::string and_list(const std::vector<std::string>& v) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) out += (i == 0 ? "" : i + 1 == v.size() ? " and " : ", ") + v[i];
  return out;
}

// What a mission on a world reads from its folder beside gamez.zbd (measured: docs/modding.md, A world the
// game does not have): the file (or the first of a set), what is lost without it.
struct WorldFile {
  const char* name;
  const char* what;
};
// The world's own folder always (the mission's folder: its readers, and its textures, which the Load state
// searches in zbd\\<world> then zbd); the script's DB folder for the animations (AnimSetZBDFile %RTIME%\\%DB%\\anim.zbd).
constexpr WorldFile kOwnFiles[] = {{"reader.zbd", "its world records (world.zrd)"}, {"rtexture.zbd", "its textures"}};
constexpr WorldFile kSceneryFiles[] = {{"anim.zbd", "its animations"}};
const char* what_of(const std::string& file) {
  for (const WorldFile& f : kOwnFiles)
    if (file == f.name) return f.what;
  for (const WorldFile& f : kSceneryFiles)
    if (file == f.name) return f.what;
  return "";
}

// The folder a world script's scenery comes from: its "set DB <x>", else the world script it sources first
// ("source c1.gs": c1.gs sets DB c1). "" when neither.
std::string script_db(const std::vector<std::vector<std::string>>& lines) {
  for (const auto& l : lines)
    if (l.size() >= 3 && lower(l[0]) == "set" && l[1] == "DB") return l[2];
  for (const auto& l : lines)
    if (l.size() >= 2 && lower(l[0]) == "source") {
      const std::string f = lower(l[1]);
      if (f.size() > 3 && f.compare(f.size() - 3, 3, ".gs") == 0 && f.find_first_of("/\\") == std::string::npos) return l[1].substr(0, l[1].size() - 3);
    }
  return {};
}

std::vector<std::vector<std::string>> text_script(const std::string& file) {
  std::vector<std::vector<std::string>> out;
  std::ifstream f(file);
  std::string line;
  for (int n = 0; n < 100000 && std::getline(f, line); ++n) {
    std::istringstream ws(line);
    std::vector<std::string> words;
    for (std::string w; ws >> w;) words.push_back(w);
    if (!words.empty()) out.push_back(std::move(words));
  }
  return out;
}

}  // namespace

bool game_world_name(const std::string& name) {
  static const char* const kNine[] = {"c1", "c2", "c3", "c4", "c4b", "t1", "lab", "dev1", "dev2"};
  const std::string l = lower(name);
  return std::any_of(std::begin(kNine), std::end(kNine), [&](const char* w) { return l == w; });
}

bool world_name_ok(const std::string& name) { return !name.empty() && name.find_first_of("/\\. ") == std::string::npos; }

std::vector<std::string> interp_archive_names(const std::string& file) {
  std::vector<std::string> out;
  std::ifstream f(file, std::ios::binary);
  if (!f) return out;
  uint32_t head[3] = {};
  if (!f.read(reinterpret_cast<char*>(head), sizeof head) || head[0] != 0x08971119u) return out;  // interp.zbd's signature
  std::error_code ec;
  const uintmax_t size = fs::file_size(file, ec);
  if (ec || head[2] > (size - 12) / 128) return out;  // a count the file cannot hold
  char entry[128];
  for (uint32_t i = 0; i < head[2] && f.read(entry, sizeof entry); ++i)
    out.emplace_back(entry, strnlen(entry, 120));
  return out;
}

std::vector<std::vector<std::string>> interp_archive_script(const std::string& file, const std::string& name) {
  std::vector<std::vector<std::string>> out;
  std::ifstream f(file, std::ios::binary);
  std::error_code ec;
  const uintmax_t size = fs::file_size(file, ec);
  uint32_t head[3] = {};
  if (ec || !f.read(reinterpret_cast<char*>(head), sizeof head) || head[0] != 0x08971119u || head[2] > (size - 12) / 128) return out;
  uint32_t start = 0;
  bool found = false;
  char entry[128];
  for (uint32_t i = 0; i < head[2] && !found && f.read(entry, sizeof entry); ++i)
    if (lower(std::string(entry, strnlen(entry, 120))) == lower(name)) found = true, std::memcpy(&start, entry + 124, 4);
  if (!found) return out;
  uintmax_t at = start;
  for (int n = 0; n < 100000; ++n) {  // each line: its size, a word, then its words each ended by a zero
    uint32_t pair[2] = {};
    if (at + 8 > size || !f.seekg(static_cast<std::streamoff>(at)) || !f.read(reinterpret_cast<char*>(pair), sizeof pair)) return {};
    if (pair[0] == 0) break;
    if (pair[0] > size - at - 8) return {};
    std::string bytes(pair[0], '\0');
    if (!f.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) return {};
    if (!bytes.empty() && bytes.back() == '\0') bytes.pop_back();
    std::vector<std::string> words;
    for (size_t p = 0, q; p <= bytes.size(); p = q + 1) {
      q = bytes.find('\0', p);
      if (q == std::string::npos) q = bytes.size();
      words.push_back(bytes.substr(p, q - p));
    }
    out.push_back(std::move(words));
    at += 8 + pair[0];
  }
  return out;
}

std::vector<FoundWorld> found_worlds(const std::string& root) {
  std::vector<FoundWorld> out;
  std::error_code ec;
  const std::string zbd = zutl::any_case(root, "zbd");
  if (zbd.empty() || !fs::is_directory(zbd, ec)) return out;
  std::vector<std::string> scripts;
  const std::string interp = zutl::any_case(root, "zbd/interp.zbd");
  if (!interp.empty())
    for (const std::string& n : interp_archive_names(interp)) scripts.push_back(lower(n));
  for (fs::directory_iterator it(zbd, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (!it->is_directory(ec) || it->is_symlink(ec) || game_world_name(name) || !world_name_ok(name)) continue;
    const std::string dir = it->path().string();
    bool world = false;
    for (const char* rel : {"gamez.zbd", "gamez/metadata.json", "gamez.zip"})
      if (const std::string p = zutl::any_case(dir, rel); !p.empty() && fs::is_regular_file(p, ec)) world = true;
    if (!world) continue;
    FoundWorld w;
    w.folder = name;
    for (fs::directory_iterator f(dir, ec), fend; !ec && f != fend; f.increment(ec)) {
      if (!f->is_regular_file(ec)) continue;
      const std::string file = f->path().filename().string();
      if (const int n = numbered(file, "readermp"); n >= 0) w.mp_maps.push_back(n);
      else if (const int n = numbered(file, "readeria"); n >= 0) w.ia_maps.push_back(n);
      else if (const int n = numbered(file, "readerm"); n >= 0) w.missions.push_back(n);
    }
    for (auto* v : {&w.missions, &w.ia_maps, &w.mp_maps}) std::sort(v->begin(), v->end());
    // Its script: the game reads interp.zbd's first (the mod's, which is laid in front), else the text file.
    const std::string text = zutl::any_case(root, name + ".gs");
    w.script_root = !text.empty() && fs::is_regular_file(text, ec);
    w.script_archive = std::find(scripts.begin(), scripts.end(), lower(name) + ".gs") != scripts.end();
    const std::string db = script_db(w.script_archive ? interp_archive_script(interp, name + ".gs") : w.script_root ? text_script(text) : decltype(text_script(text)){});
    if (!db.empty() && lower(db) != lower(name)) w.reads_from = db;
    auto lacks = [&](const WorldFile& need) {
      const std::string p = zutl::any_case(dir, need.name);
      if (p.empty() || !fs::is_regular_file(p, ec)) w.lacks.push_back(need.name);
    };
    for (const WorldFile& need : kOwnFiles) lacks(need);
    if (w.reads_from.empty())
      for (const WorldFile& need : kSceneryFiles) lacks(need);
    out.push_back(std::move(w));
  }
  std::sort(out.begin(), out.end(), [](const FoundWorld& a, const FoundWorld& b) { return lower(a.folder) < lower(b.folder); });
  return out;
}

std::vector<std::string> found_worlds_mod_txt(const std::string& root, const std::vector<FoundWorld>& worlds,
                                              std::vector<std::string>* written, std::vector<std::string>* log) {
  std::vector<std::string> lines;
  std::error_code ec;
  for (const FoundWorld& w : worlds) {
    lines.push_back("world=" + w.folder);
    if (!w.ia_maps.empty()) lines.push_back("ia_world=" + w.folder + " " + w.folder);
    if (w.missions.empty()) continue;
    // Its campaign: the id a campaign's (letters, digits, _ and -, up to 32), a file no other at the root has.
    std::string id;
    for (char c : w.folder)
      if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') id += c;
    if (id.empty()) id = "world";
    id.resize(std::min<size_t>(id.size(), 28));
    std::string file = id + ".txt";
    for (int n = 2; !zutl::any_case(root, file).empty() && n < 100; ++n) file = id + "_" + std::to_string(n) + ".txt";
    if (!zutl::any_case(root, file).empty()) {
      if (log) log->push_back("world " + w.folder + ": no free name for its campaign file, none written");
      continue;
    }
    std::ofstream f(fs::path(root) / file, std::ios::binary);
    f << "# Written by the game when this mod was added: the world's missions in number order.\n"
      << "name=" << w.folder << "\n";
    for (int n : w.missions) f << "mission=" << w.folder << " " << n << "\n";
    f.close();
    if (!f) {
      if (log) log->push_back("world " + w.folder + ": its campaign file could not be written");
      fs::remove(fs::path(root) / file, ec);
      continue;
    }
    lines.push_back("campaign=" + file);
    if (written) written->push_back(file);
    if (log) log->push_back("world " + w.folder + ": campaign " + file + " of its " + plural(w.missions.size(), "mission", "missions"));
  }
  return lines;
}

std::vector<std::string> found_world_sentences(const FoundWorld& w) {
  std::vector<std::string> parts, out;
  if (!w.missions.empty()) parts.push_back(plural(w.missions.size(), "mission", "missions"));
  if (!w.ia_maps.empty()) parts.push_back(plural(w.ia_maps.size(), "Instant Action map", "Instant Action maps"));
  if (!w.mp_maps.empty()) parts.push_back(plural(w.mp_maps.size(), "multiplayer map", "multiplayer maps"));
  out.push_back("The world " + w.folder + (parts.empty() ? " has no missions or maps of its own." : " has " + and_list(parts) + "."));
  if (!w.script_root && !w.script_archive)
    out.push_back("Its build script " + w.folder + ".gs is in neither the mod's zbd\\interp.zbd nor its folder: nothing on it can load.");
  if (!w.reads_from.empty())
    out.push_back("Its script takes " + w.reads_from + "'s scenery (its gamez.zbd and animations); its missions, world records and textures are its own.");
  if (!w.lacks.empty()) {
    std::vector<std::string> what;
    for (const std::string& l : w.lacks) what.push_back(l + " (" + what_of(l) + ")");
    out.push_back("It has no " + and_list(what) + ".");
  }
  return out;
}

}  // namespace mech3
