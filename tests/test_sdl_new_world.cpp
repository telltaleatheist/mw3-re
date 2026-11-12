// A world the game does not have, played (playtest Z3; docs/modding.md, A world the game does not have): the
// game's c1 copied under a new name (ironcoast, three missions, its own build script) in the test's own folder,
// as a zStudio export lays a world out, and added by IMPORT's own code (found_worlds.h writes its mod.txt).
// Then the real program, hidden and silent: its campaign played from MODS by the chain, an Instant Action wave
// on it, and a multiplayer lobby on loopback with the mod on both sides and a joiner without it turned back.
#if !defined(_WIN32)  // posix_spawn, waitpid

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "check.h"
#include "gamez/zutil/zutl_path.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/found_worlds.h"
#include "upgrades/mods/ia_mods.h"
#include "upgrades/mods/import_folder.h"

extern char** environ;

namespace {
namespace fs = std::filesystem;

// As test_sdl_frontend.cpp's: the program with its output in a log; a wait with a limit (killed past it).
pid_t spawn_mw3(const std::vector<std::string>& args, const std::string& log) {
  std::vector<std::string> v = {MW3_BIN};
  v.insert(v.end(), args.begin(), args.end());
  std::vector<char*> argv;
  for (std::string& x : v) argv.push_back(x.data());
  argv.push_back(nullptr);
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_addopen(&fa, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  posix_spawn_file_actions_adddup2(&fa, 1, 2);
  pid_t pid = 0;
  const int err = posix_spawn(&pid, argv[0], &fa, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&fa);
  return err == 0 ? pid : -1;
}
bool waited(pid_t pid, int* status, int seconds) {
  if (pid <= 0) return false;
  for (int i = 0; i < seconds * 20; ++i) {
    const pid_t got = waitpid(pid, status, WNOHANG);
    if (got == pid) return true;
    if (got < 0) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  std::printf("  the program ran past %d s: killed\n", seconds);
  kill(pid, SIGKILL);
  waitpid(pid, status, 0);
  return false;
}
bool have_fonts() { return fs::exists(std::string(MW3_GAME_DIR) + "/arial.ttf"); }

const char* const kMod = "Iron Coast";                  // the folder IMPORT is given, so the mod's name and folder
const char* const kEntry = "Iron Coast~ironcoast";      // the campaign IMPORT writes, as MODS lists it

std::string read(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
int count(const std::string& t, const std::string& what) {
  int n = 0;
  for (size_t at = 0; (at = t.find(what, at)) != std::string::npos; at += what.size()) ++n;
  return n;
}
bool has(const std::string& t, const std::string& what) { return t.find(what) != std::string::npos; }

// The world made and imported into <dir>/wd; "" when the install lacks c1 or the import fails. The copy is c1's
// folder less readerm4 (three missions), and c1.gs from the game's interp.zbd with its DB set to the new name,
// so the world loads its own gamez.zbd, readers and textures from zbd/ironcoast.
std::string new_world(const std::string& dir) {
  const std::string install = MW3_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/c1/gamez.zbd", ec) || !fs::exists(install + "/zbd/interp.zbd", ec)) return "";
  fs::remove_all(dir, ec);
  const std::string src = dir + "/export/" + kMod, wd = dir + "/wd";
  fs::create_directories(src + "/zbd/ironcoast", ec), fs::create_directories(wd, ec);
  for (const auto& f : fs::directory_iterator(install + "/zbd/c1", ec))
    if (f.path().filename() != "readerm4.zbd") fs::copy_file(f.path(), src + "/zbd/ironcoast/" + f.path().filename().string(), ec);
  std::string script;
  for (std::vector<std::string> line : mech3::interp_archive_script(install + "/zbd/interp.zbd", "c1.gs")) {
    if (line.size() == 3 && line[0] == "set" && line[1] == "DB" && line[2] == "c1") line[2] = "ironcoast";
    for (size_t i = 0; i < line.size(); ++i) script += (i ? " " : "") + line[i];
    script += "\r\n";
  }
  if (count(script, "set DB ironcoast") != 1) return "";
  std::ofstream(src + "/ironcoast.gs", std::ios::binary) << script;
  const bool confined = zutl::mounts().confined;
  zutl::mounts().confined = true;  // as the program has it when IMPORT runs
  const mech3::ModAddDone d = mech3::import_folder_run(wd, src, install, nullptr, nullptr);
  zutl::mounts().confined = confined;
  fs::remove_all(dir + "/export", ec);
  const std::string txt = read(wd + "/mods/" + kMod + "/mod.txt");
  std::printf("  imported: %s; mod.txt %s world=, %s ia_world=, %s campaign=\n", d.message.c_str(), has(txt, "\nworld=ironcoast\n") ? "has" : "LACKS",
              has(txt, "\nia_world=ironcoast") ? "has" : "LACKS", has(txt, "\ncampaign=ironcoast.txt\n") ? "has" : "LACKS");
  if (!d.ok || !has(txt, "\nworld=ironcoast\n")) return "";
  return wd;
}

// The entry's ticks saved by the game's own code (the folder's marker), as its MODS screen saves them.
bool tick(const std::string& wd, const char* entry) {
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd);
  const mech3::CampaignEntry* e = mech3::find_entry(all, entry);
  return e && mech3::entry_save_ticks(wd, *e, {kMod});
}

std::string run(const std::string& wd, std::vector<std::string> extra, const std::string& log, int seconds) {
  std::vector<std::string> args = {"--install", MW3_GAME_DIR, "--write-dir", wd, "-nofmv", "--no-launcher", "--hidden", "--nosound",
                                   "--frame-ms", "33", "--seed", "1"};
  args.insert(args.end(), extra.begin(), extra.end());
  int status = 0;
  const pid_t pid = spawn_mw3(args, log);
  EXPECT(waited(pid, &status, seconds) && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  return read(log);
}

}  // namespace

// The world's campaign of three missions (written by IMPORT, the world's missions in number order) from MODS:
// the entry's PLAY with the mod ticked, a new pilot, each mission won by the chain (MW3_CAMPAIGN=1, as the other
// chains), each on the world's own files.
TEST(sdl_program_new_world_campaign_from_mods) {
  if (!have_fonts() || !std::getenv("MW3_CAMPAIGN")) return;
  const std::string dir = std::string(MW3_TEST_TMP) + "/sdl_new_world_campaign", wd = new_world(dir);
  EXPECT(!wd.empty());
  if (wd.empty()) return;
  EXPECT(read(wd + "/mods/" + std::string(kMod) + "/ironcoast.txt").find("mission=ironcoast 1\nmission=ironcoast 2\nmission=ironcoast 3\n") !=
         std::string::npos);
  EXPECT(tick(wd, kEntry));
  const auto start = std::chrono::steady_clock::now();
  const std::string log = run(wd, {"--entry", kEntry, "--entry-play", "--campaign-chain", "kills:3"}, dir + "/run.log", 900);
  const int loads = count(log, "Loading [ ironcoast ]");
  std::printf("  campaign: %d loads of ironcoast, %d of c1, mounted %d, in %.0f s\n", loads, count(log, "Loading [ c1 ]"),
              has(log, "mw3: mod Iron Coast: campaign \"ironcoast\" (ironcoast.txt, 3 missions) added"),
              std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
  for (size_t at = 0; (at = log.find("mw3: chain won ", at)) != std::string::npos; ++at)
    std::printf("    %s\n", log.substr(at, log.find('\n', at) - at).c_str());
  EXPECT(has(log, "mw3: mod Iron Coast: campaign \"ironcoast\" (ironcoast.txt, 3 missions) added"));
  // The world loaded once, each mission's own data after it (as the game does for missions on one world).
  EXPECT(loads == 1 && count(log, "Loading [ c1 ]") == 0 && count(log, "mw3: load: Loading InOut data") == 3);
  EXPECT(has(log, "mw3: chain won op 4 mission 1 ") && has(log, "mw3: chain won op 4 mission 2 ") && has(log, "mw3: chain won op 4 mission 3 "));
  EXPECT(has(log, "mw3: chain credits after 3 wins") && has(log, "mw3: chain pilot screen again after the credits"));
  std::error_code ec;
  fs::remove_all(dir, ec);
}

// An Instant Action wave on the world: Instant Action's entry with the mod ticked lists it as the fifth map, the
// chain plays Battle Waves there (a round lost by the pause's REPLAY, a round won by two waves cleared), tallied.
TEST(sdl_program_new_world_instant_action_wave) {
  if (!have_fonts()) return;
  const std::string dir = std::string(MW3_TEST_TMP) + "/sdl_new_world_ia", wd = new_world(dir);
  EXPECT(!wd.empty());
  if (wd.empty()) return;
  EXPECT(tick(wd, mech3::kIaEntry));
  const std::string log = run(wd, {"--entry", mech3::kIaEntry, "--entry-ia", "--action-chain", "c5:2"}, dir + "/run.log", 600);
  std::printf("  ia: mounted %d, 5 maps %d, loads %d, cleared %d, done %d\n", has(log, "Instant Action world \"ironcoast\" on ironcoast"),
              has(log, "mw3: Instant Action open, MODS: 1 on, 5 maps"), count(log, "Loading [ ironcoast ]"), count(log, "cleared at core frame"),
              has(log, "mw3: modes done: 1 cases, 1 tallied as expected, 0 not"));
  EXPECT(has(log, "mw3: mod Iron Coast: Instant Action world \"ironcoast\" on ironcoast"));
  EXPECT(has(log, "mw3: Instant Action open, MODS: 1 on, 5 maps"));
  EXPECT(has(log, "mw3: modes case mod row 5 Battle Waves: world 9,"));
  EXPECT(count(log, "Loading [ ironcoast ]") == 1 && count(log, "cleared at core frame") == 1);  // REPLAY keeps the world loaded
  EXPECT(has(log, "mw3: modes done: 1 cases, 1 tallied as expected, 0 not"));
  std::error_code ec;
  fs::remove_all(dir, ec);
}

// The multiplayer lobby on loopback (as sdl_two_processes_multiplayer_mods_match_or_turn_back): A hosts from
// multiplayer's entry with the mod ticked and its lobby lists the world's two maps; a joiner with the same mod
// ticked stays in the lobby and lists them too; a joiner on a plain start is turned back by the mod's name.
TEST(sdl_two_processes_new_world_multiplayer) {
  if (!have_fonts()) return;
  const std::string dir = std::string(MW3_TEST_TMP) + "/sdl_new_world_mp", a = new_world(dir);
  EXPECT(!a.empty());
  if (a.empty()) return;
  std::error_code ec;
  const std::string same = dir + "/same", none = dir + "/none";
  fs::create_directories(same + "/mods", ec), fs::create_directories(none, ec);
  fs::copy(a + "/mods/" + kMod, same + "/mods/" + kMod, fs::copy_options::recursive, ec);
  EXPECT(!ec && tick(a, mech3::kMpEntry) && tick(same, mech3::kMpEntry));
  auto spawn = [&](const std::string& wd, std::vector<std::string> v, bool entry, std::initializer_list<const char*> name, const std::string& log) {
    if (entry) v.insert(v.end(), {"--entry", "mp", "--entry-mp"});
    else v.insert(v.end(), {"--click-at", "132,285@40"});
    int at = 70;
    for (const char* k : name) v.insert(v.end(), {"--tap", std::string(k) + "@" + std::to_string(at)}), at += 4;
    v.insert(v.end(), {"--click-at", "470,320@100", "--install", MW3_GAME_DIR, "--write-dir", wd, "-nofmv", "--no-launcher", "--hidden",
                       "--nosound", "--net-bind", "127.0.0.1", "--frame-ms", "33"});
    return spawn_mw3(v, log);
  };
  const int base = 43000 + static_cast<int>(getpid() % 2000) * 3;
  const std::string game = std::to_string(base), lan = std::to_string(base + 1);
  const pid_t host = spawn(a, {"--net-port", game, "--net-discovery", lan, "--click-at", "421,406@180", "--frames", "100000"}, true,
                           {"0x14", "0x12", "0x1f", "0x14"}, dir + "/a.log");
  for (int i = 0; i < 600 && !has(read(dir + "/a.log"), "multiplayer page 2"); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const std::string la = read(dir + "/a.log");
  const char* map1 = "mw3: multiplayer lists the mod world's map \"ironcoast 1\" (ironcoast readermp1)";
  const char* map2 = "mw3: multiplayer lists the mod world's map \"ironcoast 2\" (ironcoast readermp2)";
  std::printf("  host: entry %d, lobby %d, maps %d %d\n", has(la, "mw3: entry mp (Multiplayer): 1 mods"), has(la, "multiplayer page 2"), has(la, map1),
              has(la, map2));
  EXPECT(has(la, "mw3: entry mp (Multiplayer): 1 mods") && has(la, "multiplayer page 2") && has(la, map1) && has(la, map2));
  struct Join {
    const char* name;
    std::string wd;
    bool entry;
    const char* want;
    const char* never;
  };
  const Join joins[] = {
      {"none", none, false, "mw3: multiplayer: turned back: Your mods don't match the host's. You do not have: Iron Coast.", "mods the host's"},
      {"same", same, true, "mw3: multiplayer: mods the host's (Iron Coast)", "turned back"},
  };
  for (const Join& j : joins) {
    const pid_t b = spawn(j.wd, {"--net-port", "0", "--net-lan", "127.0.0.1:" + lan, "--click-at", "100,143@420", "--click-at", "240,406@450",
                                 "--frames", "600"},
                          j.entry, {"0x30", "0x12", "0x1f", "0x14"}, dir + "/" + j.name + ".log");
    int st = 0;
    EXPECT(waited(b, &st, 240));
    const std::string lb = read(dir + "/" + j.name + ".log");
    std::printf("  %s: lobby %d, %s, maps %d\n", j.name, has(lb, "multiplayer page 2"), has(lb, j.want) ? "as wanted" : "NOT as wanted",
                has(lb, map1) && has(lb, map2));
    EXPECT(has(lb, "multiplayer page 2") && has(lb, j.want) && !has(lb, j.never));
    if (std::string(j.name) == "same") EXPECT(has(lb, map1) && has(lb, map2));
    if (!has(lb, j.want))
      for (size_t at = 0; (at = lb.find("mw3: m", at)) != std::string::npos; ++at) std::printf("    %s\n", lb.substr(at, lb.find('\n', at) - at).c_str());
  }
  int st = 0;
  if (host > 0) kill(host, SIGTERM), waitpid(host, &st, 0);
  fs::remove_all(dir, ec);
}

#endif
