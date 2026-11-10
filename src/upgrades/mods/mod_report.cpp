#include "upgrades/mods/mod_report.h"

#include <cctype>

#include "mech3/mods.h"

namespace mech3 {
namespace {

// "a", "a and b", "a, b and c"
std::string listed(const std::vector<std::string>& items) {
  std::string s;
  for (size_t i = 0; i < items.size(); ++i)
    s += (i == 0 ? "" : i + 1 == items.size() ? " and " : ", ") + items[i];
  return s;
}

std::string counted(int n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }

// "<what>: <why>" split at the first ": " (none: all of it the why).
void split(const std::string& f, std::string* what, std::string* why) {
  const size_t at = f.find(": ");
  if (at == std::string::npos) *what = "", *why = f;
  else *what = f.substr(0, at), *why = f.substr(at + 2);
}

std::string sentence(std::string s) {
  if (s.empty()) return s;
  s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
  if (s.back() != '.' && s.back() != '?' && s.back() != '!') s += '.';
  return s;
}

}  // namespace

std::string mod_finding_subject(const std::string& what) {
  static const std::pair<const char*, const char*> kKinds[] = {
      {"mech ", "the mech "}, {"weapon ", "the weapon "}, {"world ", "the world "},
      {"ia_world ", "the Instant Action world "}, {"campaign ", "the campaign file "}, {"mod ", "the mod "}};
  if (what.empty()) return "";
  if (what == "mod.txt") return "its mod.txt";
  if (what == "the mod") return "the whole mod";
  for (const auto& [kind, said] : kKinds)
    if (what.rfind(kind, 0) == 0) return said + what.substr(std::char_traits<char>::length(kind));
  return "the file " + what;
}

std::vector<std::string> mod_sentences(const ModFindings& f) {
  std::vector<std::string> out;
  if (f.overlay) out.push_back("It has no mod.txt, so it is read as files laid over the game's.");
  if (f.against_install && (f.replaces || f.adds)) {
    std::string s = "It ";
    if (f.replaces) s += "replaces " + counted(f.replaces, "of the game's files", "of the game's files");
    if (f.replaces && f.adds) s += " and ";
    if (f.adds) s += "adds " + counted(f.adds, "new file", "new files");
    out.push_back(sentence(s));
  } else if (!f.against_install && f.files) {
    out.push_back(sentence("It holds " + counted(f.files, "game file", "game files")));
  }
  std::vector<std::string> adds;
  auto add = [&](const std::vector<std::string>& v, const char* one, const char* many, bool quote) {
    if (v.empty()) return;
    std::vector<std::string> names;
    for (const std::string& n : v) names.push_back(quote ? "\"" + n + "\"" : n);
    adds.push_back(std::string(v.size() == 1 ? one : many) + " " + listed(names));
  };
  add(f.mechs, "the mech", "the mechs", false);
  add(f.weapons, "the weapon", "the weapons", false);
  add(f.worlds, "the world", "the worlds", false);
  add(f.ia_worlds, "the Instant Action world", "the Instant Action worlds", true);
  add(f.campaigns, "the campaign", "the campaigns", true);
  if (!adds.empty()) out.push_back(sentence("It adds " + listed(adds)));
  if (out.empty() && f.refused.empty()) out.push_back("It changes nothing in the game.");
  if (f.ignored)
    out.push_back(sentence(counted(f.ignored, "of its files is", "of its files are") +
                           std::string(" the player's own kind (settings, pilots, saved games), which a mod cannot replace: ") +
                           (f.ignored == 1 ? "it is" : "they are") + " ignored"));
  if (f.unread_anims)
    out.push_back(sentence(counted(f.unread_anims, "of its animation files is", "of its animation files are") +
                           " not read by Pirate's Moon, whose animations come from a world's anim.zbd only"));
  std::string what, why;
  for (const std::string& r : f.refused) {
    split(r, &what, &why);
    out.push_back(sentence(what.empty() ? "Refused: " + why : mod_finding_subject(what) + " is refused: " + why));
  }
  for (const std::string& w : f.warnings) {
    split(w, &what, &why);
    out.push_back(sentence("Note: " + (what.empty() ? why : mod_finding_subject(what) + ": " + why)));
  }
  for (const std::string& l : f.left_out) {
    split(l, &what, &why);
    out.push_back(sentence("At this start the game left out " + (what.empty() ? why : mod_finding_subject(what) + ": " + why)));
  }
  if (f.refused.empty() && f.left_out.empty()) out.push_back("Nothing in it is refused.");
  return out;
}

std::map<std::string, std::vector<std::string>>& mods_left_out() {
  static std::map<std::string, std::vector<std::string>> m;
  return m;
}

std::vector<std::string> mod_report(const std::string& dir, const std::string& install, const std::string& folder, int* refused) {
  ModFindings f;
  int errors = 0;
  mod_check(dir, install, &errors, &f);
  if (const auto it = mods_left_out().find(folder); !folder.empty() && it != mods_left_out().end()) f.left_out = it->second;
  if (refused) *refused = errors;
  return mod_sentences(f);
}

std::vector<std::string> mod_report_wrap(const std::string& text, size_t width) {
  std::vector<std::string> lines;
  std::string line;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find(' ', at);
    if (end == std::string::npos) end = text.size();
    std::string word = text.substr(at, end - at);
    at = end + 1;
    while (word.size() > width) {  // a long path: cut where it must
      if (!line.empty()) lines.push_back(line), line.clear();
      lines.push_back(word.substr(0, width));
      word = word.substr(width);
    }
    if (line.empty()) line = word;
    else if (line.size() + 1 + word.size() <= width) line += " " + word;
    else lines.push_back(line), line = word;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

}  // namespace mech3
