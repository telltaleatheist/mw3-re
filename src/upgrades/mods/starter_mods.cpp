#include "upgrades/mods/starter_mods.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/mods.h"
#include "mech3/net_save.h"
#include "upgrades/editor/new_weapon.h"

namespace mech3 {

namespace {
namespace fs = std::filesystem;
using Files = std::map<std::string, std::string>;  // the mod's path ('/') -> its text

// An entry of the install's zbd\reader.zbd, its bytes; empty when missing.
std::vector<uint8_t> reader_entry(const std::string& install, const char* name) {
  zutl_zar::Archive a;
  zutl_zar::init(a);
  std::vector<uint8_t> out;
  if (zutl_zar::open(a, (fs::path(install) / "zbd" / "reader.zbd").string().c_str())) {
    const int i = zutl_zar::find(a, name);
    if (i >= 0) {
      uint32_t n = a.entries[i].length;
      out.resize(n ? n : 1);
      if (zutl_zar::read(a, i, out.data(), &n) == 0) out.resize(n);
      else out.clear();
    }
    zutl_zar::close(a);
  }
  zutl_zar::destroy(a);
  return out;
}

zrdr::Node* tree_of(const std::string& text) {
  std::FILE* f = std::tmpfile();
  if (!f) return nullptr;
  std::fwrite(text.data(), 1, text.size(), f);
  std::rewind(f);
  zrdr::Node* n = zrdr::parse_text(f);
  std::fclose(f);
  return n;
}

// An entry's bytes as a tree: their text read back, the reader's wrapping list
// around it (its item 1 the entry's own list).
zrdr::Node* entry_tree(const std::vector<uint8_t>& bytes) { return tree_of(zrd_text_of(bytes.data(), bytes.size())); }

// A tree's items as a loose file's text, one a line: the text reader wraps
// the file's items in a list of its own, so this reads back as `root`.
std::string file_text(const zrdr::Node& root) {
  std::string t;
  for (int32_t i = 1; root.type == zrdr::List && i < zrdr::list_count(&root); ++i) {
    std::vector<uint8_t> b;
    ai::put_node(b, root.list[i]);
    t += zrd_text_of(b.data(), b.size()) + "\n";
  }
  return t;
}

// A key's first number scaled (its value a list: `key (v ...)`); false when absent.
bool scale(zrdr::Node* root, const char* key, float by) {
  zrdr::Node* v = zrdr::find(root, key);
  if (!v || v->type != zrdr::List || v->list[0].i < 2) return false;
  zrdr::Node& x = v->list[1];
  if (x.type == zrdr::Float) x.f *= by;
  else if (x.type == zrdr::Int) x.type = zrdr::Float, x.f = static_cast<float>(x.i) * by;
  else return false;
  return true;
}

std::string head(const std::string& name, const std::string& what) {
  return "name=" + name + "\nfor=" + mods_game() + "\nauthor=MechWarrior 3 rebuild (a starter mod)\nversion=1\n" +
         "description=" + what + "\n";
}

std::string readme(const std::string& kind, const std::string& body) {
  return body + "\n\nMade by --make-starter " + kind + " from your own install. The guide: docs/starter-mods/" + kind +
         ".md in the rebuild's source, or the Starter mods section of the modding guide.\n";
}

bool mech(const std::string& install, Files* out, std::string* why) {
  const std::vector<uint8_t> b = reader_entry(install, "dfn_thor.zrd");
  if (b.empty()) return *why = "the install's zbd\\reader.zbd has no dfn_thor.zrd", false;
  zrdr::Node* root = entry_tree(b);
  if (!root || zrdr::list_count(root) < 2 || root->list[1].type != zrdr::List)
    return zrdr::free_node(root), *why = "dfn_thor.zrd did not read", false;
  const bool ok = scale(root, "max_turn_rate", 1.25f) && scale(root, "accel_rate", 1.5f);
  const std::string edited = ok ? file_text(root->list[1]) : std::string();
  zrdr::free_node(root);
  if (!ok) return *why = "dfn_thor.zrd has no max_turn_rate or accel_rate", false;
  (*out)["mod.txt"] = head("Starter: Thor B", "A Thor that turns a quarter faster and speeds up half again as fast.") +
                      "mech=thor_b tech=clan mass=70 class=heavy model=thor title=Thor B\n";
  (*out)["zbd/dfn_thor_b.zrd"] = "; The Thor's definition (dfn_thor.zrd) with max_turn_rate x1.25 and accel_rate x1.5.\n" +
                                 edited;
  (*out)["about_this_mod.txt"] = readme("mech",
                                "Thor B: a new mech in the mech lab, the Thor's models, loadouts and motions with a quicker "
                                "turn and pick-up (zbd\\dfn_thor_b.zrd, the mech= line in mod.txt).");
  return true;
}

bool weapon(const std::string& install, Files* out, std::string* why) {
  const std::vector<uint8_t> b = reader_entry(install, "weapons.zrd");
  if (b.empty()) return *why = "the install's zbd\\reader.zbd has no weapons.zrd", false;
  WeaponNumbers n;
  if (!weapon_numbers_of(b.data(), b.size(), "ERPPC", &n)) return *why = "weapons.zrd has no ERPPC", false;
  n.damage *= 1.25f, n.heat *= 1.2f;
  const std::vector<uint8_t> w = weapons_with_definition(b.data(), b.size(), "ERPPC", "ERPPC_B", n, 3, false);
  zrdr::Node* root = w.empty() ? nullptr : entry_tree(w);
  if (!root || zrdr::list_count(root) < 2 || root->list[1].type != zrdr::List)
    return zrdr::free_node(root), *why = "ERPPC could not be copied", false;
  const std::string text = file_text(root->list[1]);
  zrdr::free_node(root);
  (*out)["mod.txt"] = head("Starter: ER PPC B", "An ER PPC that hits a quarter harder and runs a fifth hotter.") +
                      "weapon=ERPPC_B ammo=no tech=any title=ER PPC B\n";
  (*out)["zbd/weapons.zrd"] = "; The game's weapons.zrd with ERPPC copied as ERPPC_B (DAMAGE x1.25, HEAT x1.2) after the last.\n" +
                              text;
  (*out)["about_this_mod.txt"] = readme("weapon",
                                "ER PPC B: a new weapon in the mech lab for either tech, the ER PPC's look and sound with "
                                "more damage and heat (zbd\\weapons.zrd, the weapon= line in mod.txt).");
  return true;
}

// The messages by the install's own numbers (Mech3Msg.dll's names: the two
// games number them apart), each line's name in a comment above it.
bool translation(const std::string& install, Files* out, std::string* why) {
  zutl::MessageDll dll;
  if (!zutl::read_message_dll((fs::path(install) / "Mech3Msg.dll").string(), dll))
    return *why = "the install's Mech3Msg.dll did not read", false;
  static const std::pair<const char*, const char*> kLines[] = {
      {"MSG_CAMPAIGN_BTN", "K A M P A G N E"},       {"MSG_INSTANT_ACTION_BTN", "S O F O R T - E I N S A T Z"},
      {"MSG_TRAINING_BTN", "T R A I N I N G"},       {"MSG_MP_BTN", "M E H R S P I E L E R"},
      {"MSG_OPTIONS_BTN", "O P T I O N E N"},        {"MSG_CREDITS_BTN", "M I T W I R K E N D E"},
      {"MSG_QUIT_BTN", "B E E N D E N"}};
  std::string t = "# The main menu's buttons in German: a message's number = its text (the name above each).\n";
  for (const auto& [name, text] : kLines) {
    const auto it = dll.ids.find(name);
    if (it == dll.ids.end()) return *why = std::string("Mech3Msg.dll has no ") + name, false;
    t += std::string("# ") + name + "\n" + std::to_string(it->second) + "=" + text + "\n";
  }
  (*out)["mod.txt"] = head("Starter: German menu", "The main menu's buttons in German.");
  (*out)["strings.txt"] = t;
  (*out)["about_this_mod.txt"] = readme("translation", "The main menu's buttons in German (strings.txt).");
  return true;
}

bool mission(const std::string&, Files* out, std::string*) {
  (*out)["mod.txt"] = head("Starter: One Mission", "A campaign of one mission: the game's own c1 mission 1.") +
                      "campaign=one_mission.txt\n";
  (*out)["one_mission.txt"] =
      "# One mission in play order: its world (c1) and its number (1); the briefing and movie are the game's.\n"
      "name=Starter: One Mission\n"
      "stock=default\n"
      "op=The Drop bases=3\n"
      "mission=c1 1\n";
  (*out)["about_this_mod.txt"] = readme("mission",
                                "A custom campaign of one mission, on the game's own c1 mission 1, with its own pilots "
                                "(MODS, CAMPAIGNS: Starter: One Mission; one_mission.txt).");
  return true;
}
}  // namespace

const std::vector<std::string>& starter_kinds() {
  static const std::vector<std::string> k = {"mech", "weapon", "translation", "mission"};
  return k;
}

std::string starter_folder(const std::string& kind) { return "starter_" + kind; }

StarterMade starter_make(const std::string& kind, const std::string& install, const std::string& mods_dir) {
  StarterMade r;
  r.path = (fs::path(mods_dir) / starter_folder(kind)).string();
  Files files;
  std::string why;
  bool built = false;
  if (kind == "mech") built = mech(install, &files, &why);
  else if (kind == "weapon") built = weapon(install, &files, &why);
  else if (kind == "translation") built = translation(install, &files, &why);
  else if (kind == "mission") built = mission(install, &files, &why);
  else why = "no starter " + kind + " (mech, weapon, translation, mission)";
  if (!built) return r.message = "Not made: " + why + ".", r;
  // Only a new folder: one there already, whatever it holds, is left alone.
  std::error_code ec;
  fs::create_directories(mods_dir, ec);
  if (fs::exists(fs::symlink_status(r.path, ec)) || !fs::create_directory(r.path, ec))
    return r.message = "Not made: " + r.path + " is there already (rename or remove it yourself to make it again).", r;
  for (const auto& [rel, text] : files) {
    const fs::path to = fs::path(r.path) / fs::path(rel);
    fs::create_directories(to.parent_path(), ec);
    std::ofstream f(to, std::ios::binary);
    f << text;
    if (!f) return r.message = "Not made in full: " + to.string() + " could not be written.", r;
  }
  r.ok = true;
  r.message = "Made " + r.path + ": turn it on in MODS (or mod=" + starter_folder(kind) + " in mw3.cfg) and restart.";
  return r;
}

}  // namespace mech3
