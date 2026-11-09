#include "mech3/mod_rows_file.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "gamez/zutil/zutl_path.h"
#include "mech3/mech_names.h"

namespace mech3::rows_file {

namespace fs = std::filesystem;

namespace {
bool is_mech_row(int32_t m) { return m >= kFirstModMech; }
bool is_weapon_row(int32_t w) { return w >= kBuiltinWeapons; }

struct Writing {
  bool on = false;
  std::set<int32_t> mechs, weapons;
};
// A stack each: a file read or written while another is (a pilot's loadout
// bringing in its mech's saved loadouts) keeps its own.
std::vector<Writing> g_writes;
Writing g_write;

// A sidecar's line: the number it names and the row's name; `now` its number
// in this run's table, -1 when no enabled mod adds it.
struct Named {
  std::string name;
  int32_t now = -1;
};
struct Reading {
  bool on = false, sidecar = false, rows = false;
  std::string path, bad_line;  // bad_line: the first that did not read
  std::map<int32_t, Named> mechs, weapons;
  std::string error;           // the first refusal
};
std::vector<Reading> g_reads;
Reading g_read;

void refuse(const std::string& why) {
  if (g_read.error.empty()) g_read.error = why;
}
}  // namespace

std::string sidecar(const std::string& path) { return path + ".rows"; }

void write_begin() {
  g_writes.push_back(g_write);
  g_write = Writing{}, g_write.on = true;
}
// Only the rows a mod adds: a number past the table (a damaged file's) is
// written and read back as it is, as the exe does.
void note_mech(int32_t m) {
  if (g_write.on && is_mech_row(m) && m < mech_count()) g_write.mechs.insert(m);
}
void note_weapon(int32_t w) {
  if (g_write.on && is_weapon_row(w) && w < weapon_count()) g_write.weapons.insert(w);
}

void write_end(const std::string& path) {
  const Writing w = g_write;
  g_write = g_writes.empty() ? Writing{} : g_writes.back();
  if (!g_writes.empty()) g_writes.pop_back();
  if (w.mechs.empty() && w.weapons.empty()) {
    delete_sidecar(path);  // a file holding no rows: a stale sidecar never remaps it
    return;
  }
  std::ofstream out(zutl::write_path(sidecar(path).c_str()), std::ios::binary | std::ios::trunc);
  out << "# The mods' mechs and weapons this file's numbers name (docs/modding.md)\n";
  for (int32_t m : w.mechs) out << "mech " << m << " " << mech_name(m) << "\n";
  for (int32_t x : w.weapons) out << "weapon " << x << " " << weapon_name(x) << "\n";
}

void read_begin(const std::string& path) {
  g_reads.push_back(g_read);
  g_read = Reading{};
  g_read.on = true;
  g_read.path = path;
  std::ifstream in(zutl::write_path(sidecar(path).c_str()), std::ios::binary);
  if (!in) return;
  g_read.sidecar = true;
  int n = 0;
  for (std::string line; std::getline(in, line);) {
    ++n;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    std::istringstream words(line);
    std::string kind, name, more;
    long long number = -1;
    const bool ok = static_cast<bool>(words >> kind >> number >> name) && !(words >> more);
    Named row{name, -1};
    if (ok && kind == "mech" && is_mech_row(static_cast<int32_t>(number)) && number < 0x10000) {
      row.now = mech_index(name.c_str());
      g_read.mechs[static_cast<int32_t>(number)] = row;
    } else if (ok && kind == "weapon" && is_weapon_row(static_cast<int32_t>(number)) && number < 0x10000) {
      row.now = weapon_index(name.c_str());
      g_read.weapons[static_cast<int32_t>(number)] = row;
    } else if (g_read.bad_line.empty()) {
      g_read.bad_line = "line " + std::to_string(n) + " (\"" + line + "\")";
    }
  }
}

int32_t mech(int32_t m) {
  if (!g_read.on || !is_mech_row(m)) return m;
  g_read.rows = true;
  const auto it = g_read.mechs.find(m);
  if (it != g_read.mechs.end()) {
    if (it->second.now < 0) refuse("it uses the mod mech " + it->second.name + ", which no enabled mod adds");
    return it->second.now;
  }
  if (!g_read.sidecar) return m;  // no sidecar: the number as it is, as the exe reads it (a damaged file's too)
  refuse("it uses mech " + std::to_string(m) + ", which no enabled mod adds");
  return -1;
}

int32_t weapon(int32_t w) {
  if (!g_read.on || !is_weapon_row(w)) return w;
  g_read.rows = true;
  const auto it = g_read.weapons.find(w);
  if (it != g_read.weapons.end()) {
    if (it->second.now < 0) refuse("it uses the mod weapon " + it->second.name + ", which no enabled mod adds");
    return it->second.now;
  }
  if (!g_read.sidecar) return w;
  refuse("it uses weapon " + std::to_string(w) + ", which no enabled mod adds");
  return -1;
}

bool read_end(std::string* error) {
  const Reading r = g_read;
  g_read = g_reads.empty() ? Reading{} : g_reads.back();
  if (!g_reads.empty()) g_reads.pop_back();
  std::string why = r.error;
  if (r.rows && !r.bad_line.empty()) why = "its " + sidecar(r.path) + " does not read at " + r.bad_line;
  if (!r.rows || why.empty()) return true;  // no rows: the sidecar ignored
  if (error) *error = why;
  refusals().push_back(r.path + " not loaded: " + why);
  std::printf("mw3: %s not loaded: %s\n", r.path.c_str(), why.c_str());
  return false;
}

void move_sidecar(const std::string& from, const std::string& to) {
  const std::string a = zutl::write_path(sidecar(from).c_str()), b = zutl::write_path(sidecar(to).c_str());
  std::error_code ec;
  if (!fs::is_regular_file(a, ec)) return;
  fs::remove(b, ec);  // the new name's own (a file with no rows has none)
  fs::rename(a, b, ec);
}

void delete_sidecar(const std::string& path) {
  const std::string s = zutl::write_path(sidecar(path).c_str());
  std::error_code ec;
  if (fs::is_regular_file(s, ec)) fs::remove(s, ec);
}

std::vector<std::string>& refusals() {
  static std::vector<std::string> r;
  return r;
}

}  // namespace mech3::rows_file
