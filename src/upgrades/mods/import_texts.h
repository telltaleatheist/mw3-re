#pragma once
#include <string>

#include "gamez/zutil/zutl_msgdll.h"

// Ours (docs/modding.md, IMPORT: a mod's Mech3Msg.dll): a mod made for the
// original changes the game's texts (briefings, objectives, the names of the
// mechs and weapons it adds) in Mech3Msg.dll, a program file no mod here may
// carry. IMPORT reads that dll's message table, compares it with the
// player's own and writes the messages that differ or are new into the
// mod's strings.txt (mods.cpp, Words). Nothing of the dll is kept.
namespace mech3 {

struct ImportTexts {
  bool found = false;  // the mod carried a Mech3Msg.dll
  bool read = false;   // both tables read (the mod's and the player's to compare with)
  int taken = 0;       // lines written: texts that differ from the player's or are new
  int names = 0;       // of them, with a message name the player's dll lacks or has under another number
  std::string why;     // not read: which side, in plain words
};

// strings.txt's lines for `theirs` against `mine`, in the messages' order: `0x<id>=text` for a text that differs
// or is new, `NAME@0x<id>=text` for a name `mine` lacks or has under another number (the text said with it, so
// the line stands alone). UTF-8, a line break as \n, a backslash as \\. "" when nothing differs.
std::string import_texts_lines(const zutl::MessageDll& theirs, const zutl::MessageDll& mine, int* taken, int* names);

// The mod's dll at `dll` against <install>/Mech3Msg.dll; the lines put at the head of <root>/strings.txt (a
// strings.txt the mod has keeps its own lines after them, so its lines win). Neither dll is changed or removed.
ImportTexts import_texts(const std::string& dll, const std::string& install, const std::string& root);

}  // namespace mech3
