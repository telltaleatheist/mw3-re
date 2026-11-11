#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Ours (Owen, 2026-11-11: mods in multiplayer; docs/modding.md, Multiplayer): the host decides which mods a
// game uses, and a joiner is let in only with the same files. Each player's lobby record already carries one
// hash over all its gameplay mods and their names (mp_lobby.cpp, the mods check); after it and the upgrades
// block comes this one, a hash for each of those mods, so a refusal can say which mod is missing or differs.
// A rebuild before it sends none and ignores it: the two sides then say only that the sets differ.
namespace mech3 {

constexpr uint32_t kModsEachMark = 0x31444f4d;  // "MOD1"

struct MpMods {
  std::vector<std::string> names;  // the gameplay mods on (mod.txt's names), at most 16 sent
  std::vector<uint64_t> hashes;    // each one's own gameplay hash; empty from a build that sends none
};

// The block appended to a lobby record: the mark, a count, then each hash (low word, high word).
void mp_mods_each_put(std::vector<uint32_t>& data, const std::vector<uint64_t>& hashes);
// Read back from `at` (where the block would start): false when it is not there. Untrusted: at most 16.
bool mp_mods_each_get(const std::vector<uint32_t>& data, size_t at, std::vector<uint64_t>* hashes);

// The hash is over the mounted result (mods_gameplay_hash's `mounted_over`): one file of a mod as it changes
// the mounted game. A reader archive, whole or kept as its changed entries (entry_archives.h), gives the
// entries that differ from the install's or are new, by name, and the names of the install's entries a whole
// one lacks; any other gameplay file gives its bytes. False when it changes nothing (the install's own bytes).
// `key` is the mounted path in lower case. So a player who imported a mod and one who laid its whole archives
// by hand hold the same number, given the same install.
bool mp_mounted_part(const std::string& host, const std::string& rel, const std::string& install, std::string* key, std::string* bytes);

// What differs, in the joiner's words ("You do not have: X. Your copy of Y differs from the host's. The host
// does not use: Z."), or in the host's about a player `who` ("Bob does not have: X. Bob's copy of Y differs
// from yours. Bob also uses: Z."). At most three mods named in each part, then "and N more". "" when either
// side sent no hashes or nothing differs mod by mod (the caller then says the sets differ, as before).
std::string mp_mods_difference(const MpMods& host, const MpMods& mine);
std::string mp_mods_difference_of(const std::string& who, const MpMods& host, const MpMods& theirs);

}  // namespace mech3
