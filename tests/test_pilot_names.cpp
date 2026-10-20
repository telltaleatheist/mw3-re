// FIX (bug) for pilot names (docs/fixes.md): kept apart from test_fixes.cpp,
// whose headers declare a different Pilot.
#include <cstring>
#include <string>

#include "check.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/pilot.h"

// Pilot names share a file without case (mech3/pilot.cpp): with the fix
// "BOB" finds bob and a rename onto a taken name is refused; the exe made a
// second pilot over bob.mw3 and renamed onto it (0x00478be0, 0x00477fe0).
TEST(fix_pilot_names_without_case) {
  zutl::mounts().read.clear();
  zutl::set_write_dir(std::string(MW3_TEST_TMP) + "/test_pilot_names");
  for (const bool fix : {true, false}) {
    zfix::options().bugs = fix;
    mech3::Pilot* bob = mech3::create_pilot("bob");
    mech3::Pilot* alice = mech3::create_pilot("alice");
    EXPECT(bob && alice && bob != alice);
    EXPECT((mech3::create_pilot("BOB") == bob) == fix);
    EXPECT(alice->rename("Bob") != fix);
    EXPECT((std::strcmp(alice->name, "alice") == 0) == fix);
    EXPECT(bob->rename("Bob"));  // its own name in other capitals
    EXPECT(std::strcmp(bob->name, "Bob") == 0);
    while (!mech3::pilots().empty()) mech3::delete_pilot(mech3::pilots().front()->name);
  }
  zfix::options().bugs = true;
}
