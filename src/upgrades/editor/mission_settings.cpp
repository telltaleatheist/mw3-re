#include "upgrades/editor/mission_settings.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "gamez/zutil/zutl_path.h"
#include "mech3/game_editor_net.h"
#include "mech3/mission.h"
#include "upgrades/editor/form.h"
#include "upgrades/editor/mods_edit.h"
#include "upgrades/editor/save_as.h"
#include "upgrades/mods/salvage_chance.h"

namespace mech3 {

namespace {
namespace fs = std::filesystem;

struct State {
  EditorForm form;
  std::string world;
  int32_t number = 0;
  std::string value;  // the field: "", or 0 to 100
  std::string note;
};
State g;
std::string g_last;

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string trim(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
// A line of mod.txt as the mission's mission_salvage= line: 1 this mission's, 0 another's, -1 not one.
int line_kind(const std::string& line, const std::string& world, int32_t number) {
  const size_t eq = line.find('=');
  if (eq == std::string::npos || lower(trim(line.substr(0, eq))) != "mission_salvage") return -1;
  std::istringstream in(line.substr(eq + 1));
  std::string w;
  int32_t n = 0;
  if (!(in >> w >> n)) return 0;
  return lower(w) == lower(world) && n == number ? 1 : 0;
}
void show_note(const std::string& t) {
  g.note = t;
  if (!g.form.fields.empty()) g.form.fields.back().label = t;
}
std::string file_text(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
void close() { form_hide(&g.form); }

void ok() {
  const std::string typed = trim(g.value);
  int32_t chance = -1;
  if (!typed.empty() && !salvage_chance_parse(typed, &chance)) return show_note("A whole number from 0 to 100, or empty for none of its own");
  if (!editor_save_ready([] { ok(); })) return;  // ours (save_as.h): the copy's name first
  const std::string& mod = mods_edit_session().mod;
  if (mod.empty() || zutl::mounts().write.empty() || !editor_owns_mod(zutl::mounts().write, mod))
    return show_note("mods\\" + mod + " was not made by the editor, so it is never written");
  const fs::path txt = fs::path(zutl::mounts().write) / "mods" / mod / "mod.txt";
  std::error_code ec;
  const std::string text = file_text(txt);
  const std::string now = mod_txt_with_mission_salvage(text, g.world, g.number, chance);
  const std::string mission = g.world + " mission " + std::to_string(g.number);
  if (now == text) {
    close();
    return editor_status("Mission settings: nothing changed for " + mission);
  }
  if (fs::exists(txt, ec)) fs::copy_file(txt, txt.string() + ".bak", fs::copy_options::overwrite_existing, ec);
  {
    std::ofstream o(txt, std::ios::binary | std::ios::trunc);
    o << now;
    if (!o) return show_note("mods\\" + mod + "\\mod.txt could not be written");
  }
  g_last = chance < 0 ? "removed" : "mission_salvage=" + g.world + " " + std::to_string(g.number) + " " + std::to_string(chance);
  close();
  editor_status(chance < 0 ? mission + ": its own salvage chance taken out of " + mod + " (mod.txt); from the next start"
                           : mission + ": a kill's salvage offered " + std::to_string(chance) + " times in 100, written into " + mod +
                                 " (mod.txt); from the next start");
}

void build() {
  g.form = EditorForm{};
  g.form.title = "Mission settings";
  g.form.fields.push_back({"Mission: " + g.world + " " + std::to_string(g.number), FormField::kLabel});
  FormField f{"salvage chance", FormField::kText};
  f.text = &g.value;
  static const int32_t kThree = 3;
  f.limit = &kThree;
  g.form.fields.push_back(f);
  g.form.fields.push_back({g.note, FormField::kLabel});  // last: what it is now, what was wrong
  g.form.buttons = {{"OK", ok}, {"Cancel", close}};
}
}  // namespace

std::string mod_txt_with_mission_salvage(const std::string& text, const std::string& world, int32_t number, int32_t chance) {
  const std::string line = "mission_salvage=" + world + " " + std::to_string(number) + " " + std::to_string(chance);
  std::vector<std::string> lines;  // each with its own line end
  for (size_t at = 0; at < text.size();) {
    const size_t nl = text.find('\n', at);
    lines.push_back(text.substr(at, nl == std::string::npos ? std::string::npos : nl - at + 1));
    at = nl == std::string::npos ? text.size() : nl + 1;
  }
  const std::string end = !lines.empty() && lines[0].size() >= 2 && lines[0].compare(lines[0].size() - 2, 2, "\r\n") == 0 ? "\r\n" : "\n";
  bool put = false;
  int last = -1;  // the last mission_salvage= line kept
  std::vector<std::string> out;
  for (const std::string& l : lines) {
    const int kind = line_kind(l, world, number);
    if (kind == 1) {
      if (chance >= 0 && !put) out.push_back(line + end), put = true, last = static_cast<int>(out.size()) - 1;
      continue;  // its old line, and any second one for the same mission
    }
    out.push_back(l);
    if (kind == 0) last = static_cast<int>(out.size()) - 1;
  }
  if (chance >= 0 && !put) {
    if (!out.empty() && out.back().find('\n') == std::string::npos) out.back() += end;  // a last line without its end
    out.insert(last >= 0 ? out.begin() + last + 1 : out.end(), line + end);
  }
  std::string s;
  for (const std::string& l : out) s += l;
  return s;
}

int32_t mod_txt_mission_salvage(const std::string& text, const std::string& world, int32_t number) {
  std::istringstream in(text);
  for (std::string l; std::getline(in, l);) {
    if (line_kind(l, world, number) != 1) continue;
    MissionSalvage m;
    if (mission_salvage_parse(trim(l.substr(l.find('=') + 1)), &m)) return m.chance;
  }
  return -1;
}

void mission_settings_open(int32_t) {
  if (!mods_edit_session().active) return editor_status("MISSION SETTINGS writes into a mod: open a mission from MODS, MISSIONS");
  const Missions& ms = missions();
  if (ms.current < 0 || ms.number < 1) return editor_status("Mission settings: no mission is loaded");
  g = State{};
  g.world = mission_dir(ms.current), g.number = ms.number;
  // The copy's own line for this mission, when it has one; and what the mission plays with now.
  const std::string& mod = mods_edit_session().mod;
  const int32_t own = mod.empty() || zutl::mounts().write.empty()
                          ? -1
                          : mod_txt_mission_salvage(file_text(fs::path(zutl::mounts().write) / "mods" / mod / "mod.txt"), g.world, g.number);
  if (own >= 0) g.value = std::to_string(own);
  std::string from;
  const int32_t now = salvage_chance_for(g.world, g.number, &from);
  g.note = now < 0 ? "Now: the original's, 1 in 2. 0 to 100; empty: none of its own"
                   : "Now: " + std::to_string(now) + " in 100 (" + from + "). 0 to 100; empty: none of its own";
  build();
  form_show(&g.form);
}

EditorForm& mission_settings_form() { return g.form; }
const std::string& mission_settings_last() { return g_last; }

}  // namespace mech3
