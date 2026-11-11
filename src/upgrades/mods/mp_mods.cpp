#include "upgrades/mods/mp_mods.h"

#include <algorithm>

namespace mech3 {

void mp_mods_each_put(std::vector<uint32_t>& data, const std::vector<uint64_t>& hashes) {
  const size_t n = std::min<size_t>(hashes.size(), 16);
  data.push_back(kModsEachMark);
  data.push_back(static_cast<uint32_t>(n));
  for (size_t i = 0; i < n; ++i) {
    data.push_back(static_cast<uint32_t>(hashes[i]));
    data.push_back(static_cast<uint32_t>(hashes[i] >> 32));
  }
}

bool mp_mods_each_get(const std::vector<uint32_t>& d, size_t at, std::vector<uint64_t>* hashes) {
  hashes->clear();
  if (at + 2 > d.size() || d[at] != kModsEachMark) return false;
  const uint32_t n = std::min<uint32_t>(d[at + 1], 16);
  at += 2;
  for (uint32_t i = 0; i < n && at + 1 < d.size(); ++i, at += 2)
    hashes->push_back(static_cast<uint64_t>(d[at]) | static_cast<uint64_t>(d[at + 1]) << 32);
  return true;
}

namespace {
struct Parts {
  std::vector<std::string> missing, differ, extra;  // of `b` against the host `a`
  bool told = false;
};
Parts compare(const MpMods& a, const MpMods& b) {
  Parts p;
  if (a.hashes.size() != a.names.size() || b.hashes.size() != b.names.size()) return p;  // a side that sent none
  p.told = true;
  for (size_t i = 0; i < a.names.size(); ++i) {
    const auto it = std::find(b.names.begin(), b.names.end(), a.names[i]);
    if (it == b.names.end()) p.missing.push_back(a.names[i]);
    else if (b.hashes[static_cast<size_t>(it - b.names.begin())] != a.hashes[i]) p.differ.push_back(a.names[i]);
  }
  for (const std::string& n : b.names)
    if (std::find(a.names.begin(), a.names.end(), n) == a.names.end()) p.extra.push_back(n);
  return p;
}
// At most three named, then "and N more".
std::string named(const std::vector<std::string>& names) {
  std::string t;
  for (size_t i = 0; i < names.size() && i < 3; ++i) t += (i ? ", " : "") + names[i];
  if (names.size() > 3) t += " and " + std::to_string(names.size() - 3) + " more";
  return t;
}
void add(std::string& t, const std::string& sentence) { t += (t.empty() ? "" : " ") + sentence; }
}  // namespace

std::string mp_mods_difference(const MpMods& host, const MpMods& mine) {
  const Parts p = compare(host, mine);
  std::string t;
  if (!p.missing.empty()) add(t, "You do not have: " + named(p.missing) + ".");
  if (!p.differ.empty()) add(t, "Your copy of " + named(p.differ) + " differs from the host's.");
  if (!p.extra.empty()) add(t, "The host does not use: " + named(p.extra) + ".");
  return t;
}

std::string mp_mods_difference_of(const std::string& who, const MpMods& host, const MpMods& theirs) {
  const Parts p = compare(host, theirs);
  std::string t;
  if (!p.missing.empty()) add(t, who + " does not have: " + named(p.missing) + ".");
  if (!p.differ.empty()) add(t, who + "'s copy of " + named(p.differ) + " differs from yours.");
  if (!p.extra.empty()) add(t, who + " also uses: " + named(p.extra) + ".");
  return t;
}

}  // namespace mech3
