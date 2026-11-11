// The shared tests in Pirate's Moon's test program (mw3pmtests defines MW3PM_GAME_DIR): its install, its
// table for a test's length (its archives open only as the running game's), its word in a mod.txt.
#pragma once
#ifdef MW3PM_GAME_DIR
#include "gamez/zgame/zgame_table.h"
#include "pm/pm_game.h"
#define TEST_GAME_DIR MW3PM_GAME_DIR
#define TEST_IS_PM 1
struct TestGame {
  TestGame() { zgame::set_game(pm::table()); }
  ~TestGame() { zgame::set_game(zgame::mw3_table()); }
};
#else
#define TEST_GAME_DIR MW3_GAME_DIR
#define TEST_IS_PM 0
struct TestGame {
  TestGame() {}
  ~TestGame() {}
};
#endif
// A file only this game ships and one only the other does (mod_install's mod_game_of), the games' names,
// and the answer to "which game is this mod for" (1 MechWarrior 3, 2 Pirate's Moon).
#define TEST_ONLY_OURS (TEST_IS_PM ? "zbd/c1/readersa1.txt" : "zbd/c4b/m5.txt")
#define TEST_ONLY_THEIRS (TEST_IS_PM ? "zbd/c4b/m5.txt" : "zbd/c1/readersa1.txt")
#define TEST_OUR_NAME (TEST_IS_PM ? "Pirate's Moon" : "MechWarrior 3")
#define TEST_THEIR_NAME (TEST_IS_PM ? "MechWarrior 3" : "Pirate's Moon")
#define TEST_OUR_ANSWER (TEST_IS_PM ? 2 : 1)
