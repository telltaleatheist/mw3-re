#include "upgrades/mods/screen_fit.h"

#include "gamez/ztwod/twod.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/mod_install.h"

namespace mech3 {

namespace {
// The text's width in the font `like` is drawn in (the Text's own measuring, FontText::measure_prefix's way):
// 0 when it has no font yet or the host cannot measure.
int32_t width_of(const std::string& text, const ztwod::FontText* like) {
  if (!like || !like->hfont) return 0;
  ztwod::Host& h = ztwod::host();
  const auto dc = h.CreateCompatibleDC(0);
  if (!dc) return 0;
  const auto old = h.SelectObject(dc, like->hfont);
  ztwod::Rect r{};
  const bool ok = h.DrawTextA(dc, text.c_str(), -1, &r, 0x400) != 0;  // DT_CALCRECT
  h.SelectObject(dc, old);
  h.DeleteDC(dc);
  return ok ? r.right - r.left : 0;
}
}  // namespace

bool mods_text_fits(const std::string& text, const ztwod::FontText* font, int32_t px, size_t chars) {
  if (text.empty()) return true;
  const int32_t w = width_of(text, font);
  return w > 0 ? w <= px : text.size() <= chars;
}

std::string mods_text_fit(const std::string& text, const ztwod::FontText* font, int32_t px, size_t chars) {
  if (mods_text_fits(text, font, px, chars)) return text;
  std::string t = text;
  while (!t.empty()) {
    t.pop_back();
    while (!t.empty() && t.back() == ' ') t.pop_back();
    if (mods_text_fits(t + "...", font, px, chars)) return t + "...";
  }
  return "...";
}

std::vector<std::string> mods_text_wrap(const std::string& text, const std::function<bool(const std::string&)>& fits) {
  std::vector<std::string> lines;
  std::string line;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find(' ', at);
    if (end == std::string::npos) end = text.size();
    std::string word = text.substr(at, end - at);
    at = end + 1;
    while (word.size() > 1 && !fits(word)) {  // a long path: cut where it must be
      if (!line.empty()) lines.push_back(line), line.clear();
      size_t n = word.size() - 1;
      while (n > 1 && !fits(word.substr(0, n))) --n;
      lines.push_back(word.substr(0, n));
      word = word.substr(n);
    }
    if (line.empty()) line = word;
    else if (fits(line + " " + word)) line += " " + word;
    else lines.push_back(line), line = word;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

std::vector<std::string> mods_text_wrap(const std::string& text, const ztwod::FontText* font, int32_t px, size_t chars) {
  return mods_text_wrap(text, [&](const std::string& s) { return mods_text_fits(s, font, px, chars); });
}

std::pair<std::string, std::string> mods_note_lines(const std::string& text, const std::function<bool(const std::string&)>& fits) {
  const std::vector<std::string> lines = mods_text_wrap(text, fits);
  std::pair<std::string, std::string> out;
  if (!lines.empty()) out.first = lines[0];
  if (lines.size() > 1) out.second = lines[1];
  if (lines.size() > 2) {  // more than two lines: the second ends "..."
    std::string& b = out.second;
    b += "...";
    while (b.size() > 3 && !fits(b)) {
      b.erase(b.size() - 4, 1);
      while (b.size() > 3 && b[b.size() - 4] == ' ') b.erase(b.size() - 4, 1);
    }
  }
  return out;
}

std::pair<std::string, std::string> mods_import_lines(const ModAddDone& d, const std::string& took,
                                                      const std::function<bool(const std::string&)>& fits) {
  if (!d.ok) return mods_note_lines(d.message, fits);
  // mod_add_run's message: "Added <name>: <outcome>." or, with warnings, "Added <name>: N warnings: REPORT ... . <Next>."
  const std::string head = "Added " + d.name + ": ";
  std::string first = d.message, next;
  if (d.warnings > 0 && d.message.rfind(head, 0) == 0) {
    const size_t dot = d.message.find(". ", head.size());
    if (dot != std::string::npos) first = d.message.substr(0, dot + 1), next = d.message.substr(dot + 2);
  }
  if (!fits(first) && first.rfind(head, 0) == 0) {  // the name is in DETAILS: the outcome without it
    std::string rest = first.substr(head.size());
    if (!rest.empty() && rest[0] >= 'a' && rest[0] <= 'z') rest[0] = static_cast<char>(rest[0] - 'a' + 'A');
    first = "Added. " + rest;
  }
  if (next.empty() && !took.empty()) next = took.substr(0, took.find(';')) + ".";  // "Took N files ... your install."
  std::pair<std::string, std::string> out = mods_note_lines(first, fits);
  if (out.second.empty()) out.second = mods_note_lines(next, fits).first;  // one line of it, whole or cut at a word
  else if (d.warnings > 0 && first.find("REPORT") != std::string::npos && (out.first + out.second).find("REPORT") == std::string::npos)
    out.second = "REPORT lists them.";  // never lost to the cut
  return out;
}

}  // namespace mech3
