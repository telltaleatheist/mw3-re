#pragma once
#include <cstddef>
#include <string>
#include <vector>

// Ours (Owen, 2026-11-09; modder step 2): the modding reference. Every key a
// reader takes, by file, with its type, default and limits, written beside
// each reader's registry here (one .cpp per file kind) and turned into
// docs/modding-reference.md by `mw3 --reference` (reference_markdown). The
// readers report each key they look up to an observer (null in play); the
// test runs them over the shipped files and fails when a key is read that
// the registry lacks or the registry lists one never read, so the reference
// follows the code.
namespace upgrades::reference {

struct Key {
  const char* path;     // where in the record ("" the top; "EFFECT/SOUND")
  const char* key;      // as the file spells it
  const char* type;     // "text", "int", "float", "flag 0/1", "list: x y z", ...
  const char* fallback; // when it is absent ("required", "none", a value)
  const char* limits;   // what the reader takes or refuses ("" none)
  const char* meaning;  // one line, for modders
};

struct File {
  const char* id;       // the observer's context ("mod.txt", "weapons.zrd", ...)
  const char* title;    // the section's heading
  const char* where;    // where the file lives in a mod
  const char* reader;   // the code that reads it (a function and its address, or ours)
  const char* intro;    // a paragraph: the syntax, what an unknown key does
  std::vector<Key> keys;
};

// Every file kind, in the reference's order.
const std::vector<File>& files();
// docs/modding-reference.md as `mw3 --reference` writes it.
std::string markdown();

// The test's observer: each key a reader looks up, with the context the test
// set (`context`) and the reader node searched (null for our parsers), so a
// test can tell one file's lookups from another's. Null in play: the
// readers' one check is a pointer.
using Observer = void (*)(const char* context, const char* key, const void* node);
Observer& observer();
const char*& context();
inline void seen(const char* key, const void* node = nullptr) {
  if (Observer o = observer()) o(context(), key, node);
}
// Our parsers' key test: `key == lit`, reported.
inline bool key_is(const std::string& key, const char* lit) {
  seen(lit);
  return key == lit;
}

// The registries, one per file kind (reference_<kind>.cpp).
File mod_txt();
File campaign();
File weapons();
File dfn();

}  // namespace upgrades::reference
