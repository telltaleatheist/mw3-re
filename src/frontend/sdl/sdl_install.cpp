#include "frontend/sdl/sdl_install.h"
#include "gamez/zutil/zutl_path.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma comment(lib, "advapi32.lib")
#endif

namespace sdlfe {

namespace fs = std::filesystem;

namespace {
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string trimmed(std::string s) {
  while (!s.empty() && (s.back() == '/' || s.back() == '\\' || s.back() == ' ')) s.pop_back();
  return s;
}
std::string parent(const std::string& p) { return fs::path(p).parent_path().string(); }
}  // namespace

bool install_valid(const std::string& dir, std::string* missing) {
  std::error_code ec;
  if (!dir.empty() && fs::is_regular_file(zutl::any_case(dir, "zbd/reader.zbd"), ec)) return true;
  if (missing) {
    *missing = dir.empty() || !fs::is_directory(zutl::any_case(dir, "zbd"), ec) ? "zbd\\" : "zbd\\reader.zbd";
    if (dir.empty() || !fs::is_directory(zutl::any_case(dir, "video"), ec)) *missing += " and video\\";
  }
  return false;
}

std::string install_of_pick(const std::string& file) {
  std::string dir = trimmed(file);
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) dir = parent(dir);  // a file: its folder
  const std::string name = lower(fs::path(dir).filename().string());
  if ((name == "zbd" || name == "video") && !install_valid(dir)) dir = parent(dir);
  return dir;
}

std::string pick_source(const std::string& file, const std::vector<std::string>& kinds) {
  std::error_code ec;
  if (fs::is_directory(file, ec)) return file;
  const std::string ext = lower(fs::path(file).extension().string());
  const std::string bare = ext.empty() ? ext : ext.substr(1);
  if (bare == "bin") {  // a cue sheet's data file: the sheet
    for (const char* c : {".cue", ".CUE"}) {
      const fs::path cue = fs::path(file).replace_extension(c);
      if (fs::is_regular_file(cue, ec)) return cue.string();
    }
  }
  for (const std::string& k : kinds)
    if (bare == k) return file;
  return install_of_pick(file);
}

std::string install_of_program_value(std::string value) {
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
  value = trimmed(value);
  const std::string l = lower(value);
  if (l.size() > 4 && l.compare(l.size() - 4, 4, ".exe") == 0) value = parent(value);
  return value;
}

#if defined(_WIN32)
namespace {
// A string value, empty when the key or value is not there. `flags` picks the
// registry view (RRF_SUBKEY_WOW6432KEY: the 32-bit installer's).
std::string reg_string(HKEY root, const char* key, const char* name, DWORD flags) {
  char buf[1024];
  DWORD size = sizeof(buf);
  if (RegGetValueA(root, key, name, RRF_RT_REG_SZ | flags, nullptr, buf, &size) != ERROR_SUCCESS) return {};
  return buf;
}
}  // namespace
#endif

std::vector<std::string> install_candidates(bool pirates_moon) {
  std::vector<std::string> out;
  auto add = [&](const std::string& d) {
    if (!d.empty() && std::find(out.begin(), out.end(), d) == out.end()) out.push_back(d);
  };
  // The folder names the installers' defaults and players use, by game.
  // Pirate's Moon must be installed apart from MechWarrior 3 (its readme).
  const std::vector<std::string> names =
      pirates_moon ? std::vector<std::string>{"MicroProse/MechWarrior 3 Pirate's Moon", "MicroProse/Pirate's Moon",
                                              "Games/MechWarrior 3 Pirate's Moon", "Games/Pirate's Moon", "Games/PiratesMoon",
                                              "MechWarrior 3 Pirate's Moon", "Pirate's Moon", "PiratesMoon"}
                   : std::vector<std::string>{"MicroProse/MechWarrior 3", "MicroProse/MechWarrior3", "Games/MechWarrior 3",
                                              "Games/MechWarrior3", "Games/MW3", "MechWarrior 3", "MechWarrior3", "MW3"};
#if defined(_WIN32)
  // The original's InstallShield script (disc SETUP.INS, value.shl): SOFTWARE\
  // PUBLISHER_NAME\PRODUCT_NAME\PRODUCT_VERSION gets "Program" and "Version",
  // and InstallationInfo's App Paths entry for PRODUCT_KEY (mech3.exe). A
  // 32-bit installer on 64-bit Windows wrote the WOW6432Node view, or, without
  // admin rights, the user's VirtualStore copy. Pirate's Moon's key is
  // "MechWarrior 3 EP1" (its install's mw3pm.reg); both games' App Paths
  // entry is mech3.exe, so it is asked for either (the caller checks which).
  const std::string product = pirates_moon ? "MechWarrior 3 EP1" : "MechWarrior 3";
  const std::string mp = "SOFTWARE\\MicroProse\\" + product + "\\1.0";
  const char* const app = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\mech3.exe";
  for (DWORD view : {static_cast<DWORD>(RRF_SUBKEY_WOW6432KEY), static_cast<DWORD>(RRF_SUBKEY_WOW6464KEY)}) {
    add(install_of_program_value(reg_string(HKEY_LOCAL_MACHINE, mp.c_str(), "Program", view)));
    add(install_of_program_value(reg_string(HKEY_LOCAL_MACHINE, app, "Path", view)));
    add(install_of_program_value(reg_string(HKEY_LOCAL_MACHINE, app, nullptr, view)));
  }
  for (const char* store : {"Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\MicroProse\\",
                            "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\MicroProse\\"})
    add(install_of_program_value(reg_string(HKEY_CURRENT_USER, (store + product + "\\1.0").c_str(), "Program", 0)));
  // The installer's default, PROGRAMFILES\MicroProse\..., and the usual
  // hand-picked places on the system drive.
  auto win = [](std::string n) {
    for (char& c : n) c = c == '/' ? '\\' : c;
    return n;
  };
  for (const char* env : {"ProgramFiles(x86)", "ProgramFiles", "ProgramW6432"})
    if (const char* pf = std::getenv(env))
      for (const std::string& n : names)
        if (n.rfind("MicroProse/", 0) == 0) add(std::string(pf) + "\\" + win(n));
  const char* sd = std::getenv("SystemDrive");
  const std::string drive = sd ? sd : "C:";
  for (const std::string& n : names) add(drive + "\\" + win(n));
#else
  // Ours (docs/linux.md): a Windows install under Wine, Proton or Lutris, and
  // on the Mac CrossOver's and Whisky's bottles. Each prefix's drive_c with
  // the installer's default and the usual hand-picked folders; neither game is
  // sold on Steam, so a Proton prefix is a non-Steam game's (compatdata/<id>/pfx).
  const char* home_env = std::getenv("HOME");
  const std::string home = home_env ? home_env : "";
  std::vector<std::string> prefixes;
  if (const char* wp = std::getenv("WINEPREFIX")) prefixes.push_back(wp);
  if (!home.empty()) {
    prefixes.push_back(home + "/.wine");
    std::error_code ec;
    for (const char* many : {"/.steam/steam/steamapps/compatdata", "/.local/share/Steam/steamapps/compatdata",
                             "/.var/app/com.valvesoftware.Steam/data/Steam/steamapps/compatdata"})
      for (const fs::directory_entry& e : fs::directory_iterator(home + many, ec)) prefixes.push_back(e.path().string() + "/pfx");
    for (const char* many : {"/Games", "/Library/Application Support/CrossOver/Bottles",
                             "/Library/Containers/com.isaacmarovitz.Whisky/Bottles"})
      for (const fs::directory_entry& e : fs::directory_iterator(home + many, ec)) prefixes.push_back(e.path().string());
  }
  std::error_code ec;
  for (const std::string& pfx : prefixes)
    for (const std::string& n : names)
      for (const char* pf : {"/drive_c/Program Files (x86)/", "/drive_c/Program Files/", "/drive_c/"})
        if ((n.rfind("MicroProse/", 0) == 0 || std::string(pf) == "/drive_c/") && fs::is_directory(pfx + pf + n, ec))
          add(pfx + pf + n);
  if (!home.empty())
    for (const std::string& n : names)
      if (n.rfind("MicroProse/", 0) != 0) add(home + "/" + n);
#endif
  // This program's folder and the one above (mw3 beside or inside the
  // install); on the Mac, beside the .app.
  if (const char* base = SDL_GetBasePath()) {
    const std::string b = trimmed(base);
    add(b);
    add(parent(b));
    if (lower(b).size() > 19 && lower(b).find(".app/contents/resources") != std::string::npos)
      add(parent(parent(parent(b))));
  }
  return out;
}

bool save_install(const std::string& write_dir, const std::string& install) {
  return save_config_value(write_dir, "install", install);
}

bool save_config_value(const std::string& write_dir, const std::string& key, const std::string& value) {
  if (write_dir.empty()) return false;
  const fs::path cfg = fs::path(write_dir) / "mw3.cfg";
  std::vector<std::string> lines;
  {
    std::ifstream f(cfg);
    std::string line;
    while (std::getline(f, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.rfind(key + "=", 0) != 0) lines.push_back(line);
    }
  }
  lines.insert(lines.begin(), key + "=" + value);
  std::ofstream o(cfg, std::ios::trunc);
  for (const std::string& l : lines) o << l << '\n';
  return static_cast<bool>(o);
}

void apply_upgrades_config(const std::string& write_dir, const upgrades::Config& c, bool original) {
  upgrades::apply(c);
  if (upgrades::needs_migrating(c) && !write_dir.empty()) {
    for (const upgrades::Switch& sw : upgrades::switches())
      save_config_value(write_dir, std::string("upgrade_") + sw.key, c.bugfixes ? "1" : "0");
    remove_config_value(write_dir, "bugfixes");
  }
  if (original) zfix::set_upgrades(false);
}

bool remove_config_value(const std::string& write_dir, const std::string& key) {
  if (write_dir.empty()) return false;
  const fs::path cfg = fs::path(write_dir) / "mw3.cfg";
  std::vector<std::string> lines;
  bool found = false;
  {
    std::ifstream f(cfg);
    std::string line;
    while (std::getline(f, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.rfind(key + "=", 0) == 0) found = true;
      else lines.push_back(line);
    }
  }
  if (!found) return true;
  std::ofstream o(cfg, std::ios::trunc);
  for (const std::string& l : lines) o << l << '\n';
  return static_cast<bool>(o);
}

namespace {
// Two buttons: true for Browse.
bool ask_browse(const std::string& text) {
  const SDL_MessageBoxButtonData buttons[] = {
      {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"},
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Browse..."},
  };
  SDL_MessageBoxData box{};
  box.flags = SDL_MESSAGEBOX_INFORMATION;
  box.title = "Find your MechWarrior 3 installation";
  box.message = text.c_str();
  box.numbuttons = 2;
  box.buttons = buttons;
  int pressed = 0;
  return SDL_ShowMessageBox(&box, &pressed) && pressed == 1;
}

// SDL's open-file dialog, waited on (its answer comes through a callback while
// events are pumped; on Windows from another thread). Empty when cancelled.
std::string pick_file(const std::string& start) {
  struct Wait {
    std::atomic<bool> done{false};
    std::string file;  // set before done
  } w;
  static const SDL_DialogFileFilter filters[] = {{"MechWarrior 3 (Mech3.exe)", "exe"}, {"All files", "*"}};
  auto done = [](void* user, const char* const* list, int) {
    auto* w = static_cast<Wait*>(user);
    if (list && list[0]) w->file = list[0];
    w->done = true;
  };
  SDL_ShowOpenFileDialog(done, &w, nullptr, filters, 2, start.empty() ? nullptr : start.c_str(), false);
  while (!w.done) {
    SDL_Event e;
    SDL_WaitEventTimeout(&e, 50);
  }
  return w.file;
}
}  // namespace

std::string find_install(const std::string& configured, const std::vector<std::string>& more, bool may_ask,
                         bool* found_by_search) {
  if (found_by_search) *found_by_search = false;
  if (install_valid(configured)) return configured;
  if (found_by_search) *found_by_search = true;
  std::vector<std::string> all = install_candidates();
  all.insert(all.end(), more.begin(), more.end());
  for (const std::string& d : all)
    if (install_valid(d)) return d;
  if (!may_ask) return {};
  std::string text =
      "MechWarrior 3 needs the files of an installed MechWarrior 3, any version (its zbd folder).\n\n"
      "Choose Mech3.exe in your MechWarrior 3 folder.";
  if (!configured.empty()) text = "No MechWarrior 3 install at \"" + configured + "\" any more.\n\n" + text;
  std::string start;
  while (ask_browse(text)) {
    const std::string file = pick_file(start);
    if (file.empty()) return {};  // cancelled: quit
    const std::string dir = install_of_pick(file);
    std::string missing;
    if (install_valid(dir, &missing)) return dir;
    start = dir;
    text = "\"" + dir + "\" is not a MechWarrior 3 install: " + missing +
           (missing.find(" and ") != std::string::npos ? " are" : " is") + " missing there.\n\nChoose Mech3.exe in the folder MechWarrior 3 was installed to "
           "(the one that holds zbd and video).";
  }
  return {};
}

}  // namespace sdlfe
