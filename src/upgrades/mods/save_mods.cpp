#include "upgrades/mods/save_mods.h"

#include <cinttypes>
#include <cstdio>
#include <sstream>

#include "gamez/zsave/zsave.h"

namespace mech3 {

namespace {
struct Loaded {
  bool read = false;  // a saved game was read since the last note
  ModsIdentity id;    // its mods (none: hash 0)
};
Loaded g_loaded;

void save(zsave::Context& ctx, void*) {
  const ModsIdentity& id = mods_identity();
  if (id.hash == 0) return;  // no gameplay mod: the original's save
  const std::string t = save_mods_text(id);
  zsave::write(ctx, "identity", t.data(), static_cast<uint32_t>(t.size()));
}

void load(zsave::Context&, const char* sub, const void* data, uint32_t size, void*) {
  if (std::string(sub ? sub : "") != "identity" || !data) return;
  g_loaded.id = save_mods_parse(std::string(static_cast<const char*>(data), size));
}

std::string joined(const ModsIdentity& id) {
  std::string s;
  for (const std::string& n : id.names) s += (s.empty() ? "" : ", ") + n;
  return s.empty() ? "(unnamed)" : s;
}
}  // namespace

std::string save_mods_text(const ModsIdentity& id) {
  char h[32];
  std::snprintf(h, sizeof h, "%016" PRIx64, id.hash);
  std::string t = "# The mods this game was saved with (docs/modding.md)\nhash " + std::string(h) + "\n";
  for (const std::string& n : id.names) t += "mod " + n + "\n";
  return t;
}

// A line it does not know is passed over; no hash line reads as none.
ModsIdentity save_mods_parse(const std::string& text) {
  ModsIdentity id;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("hash ", 0) == 0) {
      uint64_t v = 0;
      if (std::sscanf(line.c_str() + 5, "%" SCNx64, &v) == 1) id.hash = v;
    } else if (line.rfind("mod ", 0) == 0 && line.size() > 4) {
      id.names.push_back(line.substr(4));
    }
  }
  return id;
}

// Priority 1: written before the animations' (0x32..0x34), so it is read first.
void save_mods_register() { zsave::add("Mods", save, load, 1, nullptr); }

void save_mods_begin_load() { g_loaded = Loaded{true, {}}; }

std::string save_mods_note() {
  if (!g_loaded.read) return {};
  g_loaded.read = false;
  const ModsIdentity& now = mods_identity();
  const ModsIdentity& was = g_loaded.id;
  if (was.hash == now.hash) return {};
  std::string t;
  if (was.hash == 0)
    t = "Saved without mods; now with " + joined(now);
  else if (now.hash == 0)
    t = "Saved with mods: " + joined(was) + "; none now";
  else
    t = "Saved with other mods: " + joined(was) + "; now " + joined(now);
  std::printf("mw3: %s (the mission's animations may not match the save)\n", t.c_str());
  return t;
}

}  // namespace mech3
