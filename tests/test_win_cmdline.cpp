// The Windows command line of a restart (frontend/sdl/win_cmdline.h): each argument quoted so that the started
// program's splitting gives it back. Checked against a table of the documented cases, and by splitting the
// line again with the rules CommandLineToArgvW follows (below) over arguments made of the characters that matter.
#include <cstdint>
#include <string>
#include <vector>

#include "check.h"
#include "frontend/sdl/win_cmdline.h"

namespace {
using V = std::vector<std::string>;

// CommandLineToArgvW's rules (Microsoft's "Parsing C++ command-line arguments"): the first word ends at a space
// or tab unless in quotes, and takes no escapes; after it arguments are split at spaces and tabs outside quotes;
// 2n backslashes before a quote give n and the quote opens or closes; 2n+1 give n and a literal quote; backslashes
// before anything else are themselves; inside quotes "" is a literal quote.
V split(const std::string& line) {
  V out;
  size_t i = 0;
  std::string word;
  if (i < line.size() && line[i] == '"') {
    for (++i; i < line.size() && line[i] != '"'; ++i) word += line[i];
    if (i < line.size()) ++i;
  } else {
    for (; i < line.size() && line[i] != ' ' && line[i] != '\t'; ++i) word += line[i];
  }
  out.push_back(word);
  for (;;) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size()) break;
    word.clear();
    bool quoted = false;
    for (; i < line.size(); ++i) {
      const char c = line[i];
      if (c == '\\') {
        size_t n = 0;
        while (i < line.size() && line[i] == '\\') ++n, ++i;
        if (i < line.size() && line[i] == '"') {
          word.append(n / 2, '\\');
          if (n % 2) word += '"';
          else --i;  // the quote itself is read next, as a quote
        } else {
          word.append(n, '\\');
          --i;
        }
        continue;
      }
      if (c == '"') {
        if (quoted && i + 1 < line.size() && line[i + 1] == '"') word += '"', ++i;
        else quoted = !quoted;
        continue;
      }
      if (!quoted && (c == ' ' || c == '\t')) break;
      word += c;
    }
    out.push_back(word);
  }
  return out;
}
}  // namespace

TEST(win_cmdline_quotes_by_the_documented_rules) {
  struct Case {
    const char* arg;
    const char* quoted;
  };
  const Case cases[] = {
      {"--entry", "--entry"},                                   // nothing to quote
      {"C:\\Games\\MW3", "C:\\Games\\MW3"},                     // backslashes alone are themselves
      {"", "\"\""},                                             // an empty argument is still an argument
      {"my games", "\"my games\""},                             // a space
      {"a\tb", "\"a\tb\""},                                     // a tab
      {"C:\\my games\\mw3", "\"C:\\my games\\mw3\""},           // backslashes inside quotes, not before a quote
      {"C:\\my games\\", "\"C:\\my games\\\\\""},               // a trailing backslash doubled, so the quote closes
      {"C:\\my games\\\\", "\"C:\\my games\\\\\\\\\""},         // two trailing: four
      {"C:\\Games\\", "C:\\Games\\"},                           // trailing, but nothing to quote: as it is
      {"say \"hi\"", "\"say \\\"hi\\\"\""},                     // quotes escaped
      {"a\"b", "\"a\\\"b\""},                                   // a quote alone makes it quoted
      {"a\\\"b", "\"a\\\\\\\"b\""},                             // a backslash before a quote: doubled, then the quote's own
      {"a\\\\\"b c", "\"a\\\\\\\\\\\"b c\""},                   // two before a quote: five
      {"camp~night", "camp~night"},                             // an entry's id
      {"Long Night \\ \"copy\"\\", "\"Long Night \\ \\\"copy\\\"\\\\\""},
  };
  for (const Case& c : cases) {
    const std::string q = sdlfe::win_quote_arg(c.arg);
    if (q != c.quoted) std::printf("  %s -> %s, want %s\n", c.arg, q.c_str(), c.quoted);
    EXPECT(q == c.quoted);
    EXPECT((split("x " + q) == V{"x", c.arg}));
  }
  // The line: the program in plain quotes (a space in its path), the arguments after it.
  const std::string prog = "C:\\Program Files\\MechWarrior 3 Rebuild\\mw3.exe";
  const V args = {"--write-dir", "C:\\Users\\A B\\AppData\\Roaming\\MicroProse\\MechWarrior 3\\", "--entry", "my mod~night", "", "-nofmv"};
  const std::string line = sdlfe::win_command_line(prog, args);
  EXPECT(line == "\"C:\\Program Files\\MechWarrior 3 Rebuild\\mw3.exe\" --write-dir "
                 "\"C:\\Users\\A B\\AppData\\Roaming\\MicroProse\\MechWarrior 3\\\\\" --entry \"my mod~night\" \"\" -nofmv");
  V want = {prog};
  want.insert(want.end(), args.begin(), args.end());
  EXPECT(split(line) == want);
  EXPECT((split(sdlfe::win_command_line("C:\\mw3.exe", {})) == V{"C:\\mw3.exe"}));
}

// Every argument of up to five of the characters that matter comes back as it went in, alone and beside others.
TEST(win_cmdline_round_trips_through_the_splitting_rules) {
  const char alphabet[] = {'a', ' ', '\\', '"', '\t'};
  int tried = 0, bad = 0;
  V all;
  for (int len = 0; len <= 5; ++len) {
    int count = 1;
    for (int k = 0; k < len; ++k) count *= 5;
    for (int n = 0; n < count; ++n) {
      std::string arg;
      for (int k = 0, v = n; k < len; ++k, v /= 5) arg += alphabet[v % 5];
      ++tried;
      if (split(sdlfe::win_command_line("p", {arg, "end"})) != V{"p", arg, "end"}) {
        if (++bad <= 5) std::printf("  not given back: [%s] as %s\n", arg.c_str(), sdlfe::win_quote_arg(arg).c_str());
      }
      if (len <= 3) all.push_back(arg);
    }
  }
  std::printf("  %d arguments tried, %d not given back; %zu in one line\n", tried, bad, all.size());
  EXPECT(tried == 3906 && bad == 0);
  V want = {"C:\\a b\\p.exe"};
  want.insert(want.end(), all.begin(), all.end());
  EXPECT(split(sdlfe::win_command_line("C:\\a b\\p.exe", all)) == want);
}
