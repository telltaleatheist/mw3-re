#include "upgrades/mods/mod_report.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "gamez/zutil/zutl_path.h"
#include "mech3/mod_campaign.h"
#include "mech3/mod_install.h"
#include "mech3/mods.h"
#include "upgrades/mods/campaign_entries.h"

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
  // The totals, then the warnings in three groups, the worst first.
  static const struct { ModWarningGroup group; const char* head; const char* one; const char* many; } kGroups[] = {
      {ModWarningGroup::kStops, "WHAT STOPS PART OF THE MOD WORKING", "stops part of the mod working", "stop part of the mod working"},
      {ModWarningGroup::kMayCrash, "WHAT WORKS HERE BUT MAY CRASH THE ORIGINAL GAME", "works here but may crash the original game",
       "work here but may crash the original game"},
      {ModWarningGroup::kSlip, "SLIPS THAT DO NO HARM", "is a slip that does no harm", "are slips that do no harm"}};
  int counts[3] = {0, 0, 0};
  for (const ModNote& n : f.notes) ++counts[static_cast<int>(n.group)];
  if (!f.notes.empty()) {
    std::vector<std::string> parts;
    for (const auto& g : kGroups)
      if (const int c = counts[static_cast<int>(g.group)]) parts.push_back(std::to_string(c) + " " + (c == 1 ? g.one : g.many));
    out.push_back(sentence(counted(static_cast<int>(f.notes.size()), "warning", "warnings") +
                           (f.refused.empty() ? ", none of which refuses the mod: " : ": ") + listed(parts)));
    // The two words the warnings about an archive's entries lean on, once.
    if (std::any_of(f.notes.begin(), f.notes.end(), [](const ModNote& n) {
          return !n.plain && (n.w.wrong + n.w.fix).find("bytes into the entry") != std::string::npos;
        }))
      out.push_back("In them, an entry is one named part of a .zbd file (\"zbd/t1/readermp2.zbd: aiv.zrd\" is the entry aiv.zrd in that "
                    "file), and \"bytes into the entry\" counts from the entry's first byte.");
  }
  for (const auto& g : kGroups) {
    const int c = counts[static_cast<int>(g.group)];
    if (!c) continue;
    out.push_back("");
    out.push_back(std::string(g.head) + " (" + std::to_string(c) + ")");
    // Each warning its place, what is wrong and what to change; what the rebuild and the original game do with it
    // is said once for the warnings that share it ("For 1 to 9: ..."), which are put together: the kinds in the
    // order the check met them, the check's order within a kind, the plain notes last.
    std::vector<const ModNote*> in, parts, plain;
    for (const ModNote& n : f.notes)
      if (n.group == g.group) (n.plain ? plain : parts).push_back(&n);
    std::vector<bool> placed(parts.size(), false);
    for (size_t i = 0; i < parts.size(); ++i) {
      if (placed[i]) continue;
      for (size_t m = i; m < parts.size(); ++m)
        if (!placed[m] && parts[m]->w.did == parts[i]->w.did && parts[m]->w.original == parts[i]->w.original)
          in.push_back(parts[m]), placed[m] = true;
    }
    in.insert(in.end(), plain.begin(), plain.end());
    for (size_t i = 0; i < in.size();) {
      size_t end = i + 1;
      while (!in[i]->plain && end < in.size() && !in[end]->plain && in[end]->w.did == in[i]->w.did && in[end]->w.original == in[i]->w.original)
        ++end;
      // Within the kind, entries with the very same fault (and fix) are listed, and the fault said once for them.
      for (size_t a = i; a < end;) {
        size_t b = a + 1;
        while (!in[a]->plain && b < end && in[b]->w.wrong == in[a]->w.wrong && in[b]->w.fix == in[a]->w.fix) ++b;
        if (in[a]->plain) {
          split(in[a]->w.wrong, &what, &why);
          out.push_back(std::to_string(a + 1) + ". " + sentence(what.empty() ? why : mod_finding_subject(what) + ": " + why));
        } else {
          for (size_t k = a; k < b; ++k)
            out.push_back(std::to_string(k + 1) + ". " + in[k]->where + (in[k]->w.item.empty() ? "" : " (" + in[k]->w.item + ")"));
          const std::string each = b - a == 1 ? "" : "For " + std::to_string(a + 1) + " to " + std::to_string(b) + ": ";
          out.push_back(sentence(each + (each.empty() ? "Wrong: " : "wrong: ") + in[a]->w.wrong));
          if (!in[a]->w.fix.empty()) out.push_back(sentence(each + (each.empty() ? "To fix it: " : "to fix it: ") + in[a]->w.fix));
        }
        a = b;
      }
      if (!in[i]->plain) {
        const std::string which = end - i == 1 ? "" : "For " + std::to_string(i + 1) + " to " + std::to_string(end) + ": ";
        const auto said = [&](const std::string& label, const std::string& text) {
          out.push_back(sentence(which.empty() ? label + ": " + text : which + label + ": " + text));
        };
        if (!in[i]->w.did.empty()) said(which.empty() ? "The rebuild" : "the rebuild", in[i]->w.did);
        said(which.empty() ? "The original game" : "the original game", in[i]->w.original.empty() ? std::string("not known") : in[i]->w.original);
      }
      i = end;
    }
  }
  // What this start's mount left out, but what a warning above says already (a line of mod.txt).
  std::vector<std::string> left;
  for (const std::string& l : f.left_out) {
    split(l, &what, &why);
    if (std::none_of(f.notes.begin(), f.notes.end(), [&](const ModNote& n) { return !n.plain && n.where == what; })) left.push_back(l);
  }
  if (!left.empty() && !f.notes.empty()) out.push_back("");
  for (const std::string& l : left) {
    split(l, &what, &why);
    out.push_back(sentence("At this start the game left out " + (what.empty() ? why : mod_finding_subject(what) + ": " + why)));
  }
  if (f.refused.empty() && left.empty() && f.notes.empty()) out.push_back("Nothing in it is refused.");
  return out;
}

namespace {
std::string one_line(std::string s) {
  for (char& c : s)
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  return s;
}
}  // namespace

std::vector<ModNote> mod_left_out_read(const std::string& dir) {
  std::vector<ModNote> out;
  std::ifstream f(std::filesystem::path(dir) / kModLeftOutFile, std::ios::binary);
  for (std::string line; f && std::getline(f, line) && out.size() < 10000;) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    std::vector<std::string> parts;
    for (size_t at = 0;;) {
      const size_t tab = line.find('\t', at);
      parts.push_back(line.substr(at, tab == std::string::npos ? std::string::npos : tab - at));
      if (tab == std::string::npos) break;
      at = tab + 1;
    }
    if ((parts.size() != 5 && parts.size() != 6) || parts[0].empty() || parts[1].empty()) continue;  // not a line of ours
    out.push_back({parts[0], {parts[1], parts[2], parts[3], parts[4], parts.size() == 6 ? parts[5] : ""}, ModWarningGroup::kStops, false});
  }
  return out;
}

bool mod_left_out_write(const std::string& dir, const std::vector<ModNote>& notes) {
  std::ofstream f(std::filesystem::path(dir) / kModLeftOutFile, std::ios::binary);
  f << "# Written by the game when it added this mod: the files it left out of it, and why (REPORT shows them).\n";
  for (const ModNote& n : notes)
    f << one_line(n.where) << '\t' << one_line(n.w.wrong) << '\t' << one_line(n.w.did) << '\t' << one_line(n.w.original) << '\t'
      << one_line(n.w.fix) << '\t' << one_line(n.w.item) << '\n';
  return static_cast<bool>(f);
}

std::string mod_reports_dir(const std::string& write_dir) { return (std::filesystem::path(write_dir) / "mod-reports").generic_string(); }

bool mod_report_write(const std::string& write_dir, const std::string& folder, const std::vector<std::string>& lines,
                      std::string* path, std::string* why) {
  namespace fs = std::filesystem;
  std::string ignored;
  std::string& said = why ? *why : ignored;
  std::error_code ec;
  // One plain file name: the mod's folder name, nothing that leaves the folder.
  if (folder.empty() || folder == "." || folder == ".." || folder.find_first_of("/\\:") != std::string::npos)
    return said = "the mod's folder name cannot name a file", false;
  const fs::path dir = fs::path(mod_reports_dir(write_dir));
  const fs::path marker = dir / kModReportsMarker;
  if (fs::exists(fs::symlink_status(dir, ec))) {
    // There already: ours only with our marker in it, a real folder and a real file.
    if (fs::is_symlink(fs::symlink_status(dir, ec)) || !fs::is_directory(dir, ec) || fs::is_symlink(fs::symlink_status(marker, ec)) ||
        !fs::is_regular_file(marker, ec))
      return said = dir.generic_string() + " is not a folder the game made, so nothing is written into it", false;
  } else {
    fs::create_directories(dir, ec);
    if (ec) return said = "cannot make " + dir.generic_string() + " (" + ec.message() + ")", false;
    std::ofstream m(marker, std::ios::binary);
    m << "Made by the game for REPORT's text files, one a mod, replaced each time REPORT is pressed.\n";
    if (!m) return said = "cannot write in " + dir.generic_string(), false;
  }
  const fs::path file = dir / fs::path(folder + ".txt");
  if (fs::is_symlink(fs::symlink_status(file, ec)) || fs::is_directory(file, ec))
    return said = file.generic_string() + " is not a file the game can replace", false;
  std::ofstream f(file, std::ios::binary | std::ios::trunc);
  for (const std::string& l : lines) f << l << "\n";
  if (!f) return said = "cannot write " + file.generic_string(), false;
  if (path) *path = file.generic_string();
  return true;
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

namespace {

std::string lower_copy(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// A mod's files but its mod.txt and our markers: relative path, lower case -> as it is on disk.
std::map<std::string, std::filesystem::path> mod_files(const std::string& dir) {
  namespace fs = std::filesystem;
  std::map<std::string, fs::path> out;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    const std::string rel = fs::relative(it->path(), dir, ec).generic_string();
    const std::string low = lower_copy(rel);
    if (low == "mod.txt" || low == lower_copy(kModAddedMarker) || low == lower_copy(kCampaignMarker)) continue;
    out[low] = it->path();
  }
  return out;
}

bool same_bytes(const std::filesystem::path& a, const std::filesystem::path& b) {
  std::error_code ec;
  if (std::filesystem::file_size(a, ec) != std::filesystem::file_size(b, ec) || ec) return false;
  std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
  char ba[65536], bb[65536];
  while (fa && fb) {
    fa.read(ba, sizeof ba), fb.read(bb, sizeof bb);
    if (fa.gcount() != fb.gcount() || std::memcmp(ba, bb, static_cast<size_t>(fa.gcount())) != 0) return false;
    if (fa.gcount() == 0) break;
  }
  return fa.eof() && fb.eof();
}

// A row's name: its words before the first key=value ("Thor B tons=70" -> "Thor B").
std::string row_name(const std::string& row) {
  std::istringstream in(row);
  std::string word, name;
  while (in >> word && word.find('=') == std::string::npos) name += (name.empty() ? "" : " ") + word;
  return name.empty() ? row : name;
}

// A mod's rows: "mech Thor B" -> the row's text.
std::map<std::string, std::string> mod_rows(const ModInfo& m) {
  std::map<std::string, std::string> out;
  for (const std::string& r : m.mechs) out["mech " + row_name(r)] = r;
  for (const std::string& r : m.weapons) out["weapon " + row_name(r)] = r;
  for (const std::string& r : m.worlds) out["world " + r] = r;
  for (const auto& [folder, name] : m.ia_worlds) out["Instant Action world " + folder] = name;
  for (const std::string& r : m.campaigns) out["campaign " + r] = r;
  return out;
}

const ModInfo* mod_named(const std::vector<ModInfo>& all, const std::string& folder) {
  for (const ModInfo& m : all)
    if (m.folder == folder) return &m;
  return nullptr;
}

// "a", "a, b and c"; past `most` names, "a, b, c and 4 more".
std::string some(const std::vector<std::string>& v, size_t most = 6) {
  std::vector<std::string> shown(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(std::min(v.size(), most)));
  if (v.size() > most) {
    std::string s;
    for (const std::string& n : shown) s += n + ", ";
    s.erase(s.size() - 2);
    return s + " and " + std::to_string(v.size() - most) + " more";
  }
  return listed(shown);
}

}  // namespace

CopyChanges mod_copy_changes(const std::string& write_dir, const std::string& install, const std::string& folder) {
  CopyChanges c;
  const std::vector<ModInfo> mods = mods_found(write_dir);
  const ModInfo* self = mod_named(mods, folder);
  if (!self || self->base.empty()) return c;
  c.is_copy = true, c.base = self->base;
  // The chain: each base= entry's mod, until the game's own entry (no mod) or one that is gone.
  const std::vector<CampaignEntry> entries = campaign_entries(write_dir);
  std::vector<const ModInfo*> under;
  std::string at = self->base;
  for (int guard = 0; guard < 64 && !at.empty(); ++guard) {
    const CampaignEntry* e = find_entry(entries, at);
    if (!e || e->missing) {
      c.missing = at;
      break;
    }
    if (c.base_name.empty()) c.base_name = e->name;
    if (e->mod.empty()) break;  // the game's own
    const ModInfo* m = mod_named(mods, e->mod);
    if (!m || m == self || std::find(under.begin(), under.end(), m) != under.end()) {
      if (!m) c.missing = at;
      break;  // gone, or a chain that comes round to itself
    }
    under.push_back(m), c.under.push_back(m->folder);
    at = e->base;
  }
  std::vector<std::map<std::string, std::filesystem::path>> files;
  for (const ModInfo* m : under) files.push_back(mod_files(m->path));
  for (const auto& [low, path] : mod_files(self->path)) {
    const std::string rel = std::filesystem::relative(path, self->path).generic_string();
    bool found = false;
    for (const auto& f : files) {
      const auto it = f.find(low);
      if (it == f.end()) continue;
      (same_bytes(path, it->second) ? c.same : c.replaced).push_back(rel);  // the nearest one that has it
      found = true;
      break;
    }
    if (found) continue;
    (!install.empty() && !zutl::any_case(install, rel).empty() ? c.replaced_game : c.added).push_back(rel);
  }
  std::map<std::string, std::string> below;  // the nearest mod's row wins
  for (auto m = under.rbegin(); m != under.rend(); ++m)
    for (const auto& [name, text] : mod_rows(**m)) below[name] = text;
  for (const auto& [name, text] : mod_rows(*self)) {
    const auto it = below.find(name);
    if (it == below.end()) c.rows_added.push_back(name);
    else if (it->second != text) c.rows_changed.push_back(name);
  }
  return c;
}

std::vector<std::string> mod_copy_sentences(const CopyChanges& c) {
  std::vector<std::string> out;
  if (!c.is_copy) return out;
  const std::string base = c.base_name.empty() ? c.base : "\"" + c.base_name + "\"";
  out.push_back(sentence("It is a copy saved from the campaign " + base));
  if (!c.missing.empty())
    out.push_back(sentence("The campaign " + (c.missing == c.base ? std::string("it was saved from") : "\"" + c.missing + "\" under it") +
                           " is not in the mods folder, so what it changed there cannot be told"));
  if (!c.replaced.empty())
    out.push_back(sentence("It replaces " + counted(static_cast<int>(c.replaced.size()), "file", "files") + " of that campaign: " + some(c.replaced)));
  // With part of the chain gone, a file or row nothing above the gap has may be that campaign's: not said.
  const bool whole = c.missing.empty();
  if (whole && !c.replaced_game.empty())
    out.push_back(sentence("It replaces " + counted(static_cast<int>(c.replaced_game.size()), "of the game's own files", "of the game's own files") +
                           ": " + some(c.replaced_game)));
  if (whole && !c.added.empty()) out.push_back(sentence("It adds " + counted(static_cast<int>(c.added.size()), "new file", "new files") + ": " + some(c.added)));
  if (!c.rows_changed.empty()) out.push_back(sentence("It changes the " + some(c.rows_changed)));
  if (whole && !c.rows_added.empty()) out.push_back(sentence("It adds the " + some(c.rows_added)));
  if (!c.same.empty())
    out.push_back(sentence(counted(static_cast<int>(c.same.size()), "of its files is", "of its files are") +
                           " the same as that campaign's, so " + (c.same.size() == 1 ? "it changes" : "they change") + " nothing"));
  if (c.replaced.empty() && c.replaced_game.empty() && c.added.empty() && c.rows_changed.empty() && c.rows_added.empty() && c.missing.empty())
    out.push_back("It changes nothing from the campaign it was saved from.");
  return out;
}

std::vector<std::string> mod_copy_report(const std::string& write_dir, const std::string& install, const std::string& folder) {
  return mod_copy_sentences(mod_copy_changes(write_dir, install, folder));
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
