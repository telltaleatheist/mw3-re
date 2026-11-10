#include "upgrades/mods/mod_needs.h"

#include <algorithm>
#include <cctype>

#include "upgrades/update/update.h"

namespace mech3 {
namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// A mod named by needs= or conflicts=: its folder's name or its mod.txt name, any case.
const ModInfo* named(const std::vector<ModInfo>& all, const std::string& name) {
  for (const ModInfo& o : all)
    if (lower(o.folder) == lower(name)) return &o;
  for (const ModInfo& o : all)
    if (!o.name.empty() && lower(o.name) == lower(name)) return &o;
  return nullptr;
}

std::string shown(const ModInfo& o) { return o.name.empty() ? o.folder : o.name; }

bool is_on(const std::vector<std::string>& enabled, const ModInfo& o) {
  return std::find(enabled.begin(), enabled.end(), o.folder) != enabled.end();
}

}  // namespace

std::string& mods_program_version() {
  static std::string v;
  return v;
}

std::vector<std::string> mod_needs_warnings(const ModInfo& m, const std::vector<ModInfo>& all,
                                            const std::vector<std::string>& enabled) {
  std::vector<std::string> out;
  for (const auto& [name, version] : m.needs) {
    const ModInfo* o = named(all, name);
    if (!o) out.push_back("It needs the mod " + name + (version.empty() ? "" : " " + version) + ", which is not in the mods folder.");
    else if (!o->error.empty()) out.push_back("It needs " + shown(*o) + ", which cannot be used: " + o->error + ".");
    else if (!is_on(enabled, *o)) out.push_back("It needs " + shown(*o) + ", which is off.");
    else if (!version.empty() && upgrades::update::compare_versions(o->version, version) < 0)
      out.push_back("It needs " + shown(*o) + " " + version + " or later; that mod is " +
                    (o->version.empty() ? std::string("of no version") : o->version) + ".");
  }
  std::vector<std::string> said;  // each conflicting mod once
  auto conflict = [&](const ModInfo& o) {
    if (o.folder == m.folder || !is_on(enabled, o) || std::find(said.begin(), said.end(), o.folder) != said.end()) return;
    said.push_back(o.folder);
    out.push_back("It conflicts with " + shown(o) + ", which is on.");
  };
  for (const std::string& name : m.conflicts)
    if (const ModInfo* o = named(all, name)) conflict(*o);
  for (const ModInfo& o : all)
    for (const std::string& name : o.conflicts)
      if (lower(name) == lower(m.folder) || (!m.name.empty() && lower(name) == lower(m.name))) conflict(o);
  const std::string& have = mods_program_version();
  if (!m.game_version.empty() && !have.empty() && upgrades::update::compare_versions(have, m.game_version) < 0)
    out.push_back("It needs MechWarrior 3 Rebuild " + m.game_version + " or later; this is " +
                  upgrades::update::plain_version(have) + ".");
  return out;
}

}  // namespace mech3
