#include "upgrades/mods/import_texts.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

#include "gamez/zutil/zutl_path.h"

namespace fs = std::filesystem;

namespace mech3 {
namespace {
// A text as strings.txt's value (mods_apply_strings reads it back): UTF-8, \n for a line break, \\ for a backslash.
std::string value_of(const std::string& game) {
  std::string out;
  const std::string t = zutl::utf8_text(game);
  for (size_t i = 0; i < t.size(); ++i) {
    if (t[i] == '\r') continue;  // the table's lines end \r\n: the reader makes \n that again
    if (t[i] == '\n') out += "\\n";
    else if (t[i] == '\\') out += "\\\\";
    else out += t[i];
  }
  return out;
}
std::string hex(int32_t id) {
  char b[16];
  std::snprintf(b, sizeof b, "0x%x", static_cast<unsigned>(id));
  return b;
}
// A message's text as the game compares it: its line ends alike.
std::string plain(const std::string& t) {
  std::string out;
  for (char c : t)
    if (c != '\r') out += c;
  return out;
}
}  // namespace

std::string import_texts_lines(const zutl::MessageDll& theirs, const zutl::MessageDll& mine, int* taken, int* names) {
  int lines = 0, named = 0;
  std::string out;
  // The names `mine` lacks or has under another number, by the number `theirs` gives them.
  std::multimap<int32_t, std::string> fresh;
  for (const auto& [name, id] : theirs.ids)
    if (const auto it = mine.ids.find(name); it == mine.ids.end() || it->second != id) fresh.emplace(id, name);
  for (const auto& [id, text] : theirs.text) {
    const auto [a, b] = fresh.equal_range(id);
    for (auto n = a; n != b; ++n) {
      if (n->second.find_first_of("=@#\r\n") != std::string::npos) continue;  // no name of the game's has one
      out += n->second + "@" + hex(id) + "=" + value_of(text) + "\n";
      ++lines, ++named;
    }
    if (a != b) continue;
    const auto it = mine.text.find(id);
    if (it != mine.text.end() && plain(it->second) == plain(text)) continue;
    out += hex(id) + "=" + value_of(text) + "\n";
    ++lines;
  }
  if (taken) *taken = lines;
  if (names) *names = named;
  return out;
}

ImportTexts import_texts(const std::string& dll, const std::string& install, const std::string& root) {
  ImportTexts r;
  r.found = true;
  zutl::MessageDll theirs, mine;
  if (!zutl::read_message_dll(dll, theirs) || theirs.text.empty()) return r.why = "it does not read as the game's table of texts", r;
  const std::string own = zutl::any_case(install, "Mech3Msg.dll");
  if (own.empty() || !zutl::read_message_dll(own, mine) || mine.text.empty())
    return r.why = "your own game's Mech3Msg.dll could not be read to compare it with", r;
  r.read = true;
  const std::string lines = import_texts_lines(theirs, mine, &r.taken, &r.names);
  if (lines.empty()) return r;
  const fs::path file = fs::path(root) / "strings.txt";
  std::string before;
  {
    std::ifstream in(file, std::ios::binary);
    before.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  if (before.rfind("\xef\xbb\xbf", 0) == 0) before.erase(0, 3);  // its UTF-8 mark: only a file's first line may have one
  std::ofstream o(file, std::ios::binary | std::ios::trunc);
  o << "# The texts of this mod's Mech3Msg.dll that differ from the game's own or are new, taken by IMPORT\n"
    << "# (the dll itself is not part of a mod here). id=text; NAME@id=text also gives a new message its name.\n"
    << lines;
  if (!before.empty()) o << "# The mod's own strings.txt, as it came: its lines win over those above.\n" << before;
  o.close();
  if (!o) r.read = false, r.taken = r.names = 0, r.why = "its texts could not be written into the mod's strings.txt";
  return r;
}

}  // namespace mech3
