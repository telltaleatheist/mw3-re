// New Weapon (upgrades/editor/new_weapon, docs/editor.md, New Weapon): the
// definition appended to the shipped weapons.zrd reads through zWeapon's
// reader (zwep::init) with the copy's numbers changed and every other
// definition as it was; mod.txt's weapon= line put in place or after the
// last; and the form from MODS's EDITOR, OK writing into the mod, which the
// next start mounts with the weapon in the lab's list.
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech_names.h"
#include "mech3/mods.h"
#include "mech3/net_save.h"
#include "upgrades/editor/form.h"
#include "upgrades/editor/mods_edit.h"
#include "upgrades/editor/new_weapon.h"

namespace {
namespace fs = std::filesystem;
struct SavedMounts {
  zutl::Mounts saved = zutl::mounts();
  ~SavedMounts() { zutl::mounts() = saved; }
};
std::string tmp_dir(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
std::vector<uint8_t> file_bytes(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
std::string text_of(const fs::path& p) {
  const std::vector<uint8_t> b = file_bytes(p);
  return std::string(b.begin(), b.end());
}
// An archive's entry, empty when it has none so named.
std::vector<uint8_t> entry_of(const std::string& archive, const char* name) {
  zutl_zar::Archive a;
  zutl_zar::init(a);
  std::vector<uint8_t> out;
  if (zutl_zar::open(a, archive.c_str())) {
    const int i = zutl_zar::find(a, name);
    if (i >= 0) {
      uint32_t size = a.entries[i].length;
      out.assign(size, 0);
      if (zutl_zar::read(a, i, out.data(), &size) != 0) out.clear();
      else out.resize(size);
    }
    zutl_zar::close(a);
  }
  zutl_zar::destroy(a);
  return out;
}
// The definitions zWeapon reads from the reader archive mounted now: for
// each name, its damage, range and period (-1 when it has none).
std::vector<float> defs_read(const std::vector<std::string>& names) {
  zrdr::init();
  zrdr::open_archive("zbd\\reader.zbd", true);
  std::vector<float> out;
  const zwep::InitHooks hooks = zwep::init_hooks();
  zwep::init_hooks() = zwep::InitHooks{};
  if (zwep::init(nullptr, "weapons.zrd", 0, nullptr) == 0 || zwep::find_def(names.front().c_str()))
    for (const std::string& n : names) {
      const zwep::WeaponDef* d = zwep::find_def(n.c_str());
      out.push_back(d ? d->damage : -1.0f), out.push_back(d ? d->range : -1.0f), out.push_back(d ? d->period : -1.0f);
    }
  zwep::shutdown(nullptr);
  zwep::init_hooks() = hooks;
  zrdr::close_archives(true);
  return out;
}
}  // namespace

TEST(new_weapon_definition_reads_through_zweapon) {
  const std::string install = MW3_GAME_DIR;
  if (!fs::exists(install + "/zbd/reader.zbd")) return;
  SavedMounts keep;
  const std::vector<uint8_t> zrd = entry_of(install + "/zbd/reader.zbd", "weapons.zrd");
  EXPECT(!zrd.empty());
  const std::vector<std::string> defined = mech3::weapons_defined(zrd.data(), zrd.size());
  const std::vector<std::string> copies = mech3::weapon_copies(zrd.data(), zrd.size());
  std::printf("  %zu definitions, %zu offered to copy\n", defined.size(), copies.size());
  EXPECT(defined.size() == 49 && copies.size() >= 37 && copies.size() < defined.size());
  mech3::WeaponNumbers n;
  EXPECT(mech3::weapon_numbers_of(zrd.data(), zrd.size(), "ERPPC", &n));
  EXPECT(n.damage == 17.0f && n.range == 920.0f && n.fire_rate == 0.25f && n.velocity == 900.0f && !n.beam);
  EXPECT(n.clan_tons == 6 && n.clan_slots == 2 && n.sphere_tons == 7 && n.sphere_slots == 3 && n.rounds == 0);
  EXPECT(std::fabs(n.heat - 0.408f) < 1e-6f);
  mech3::WeaponNumbers ac;
  EXPECT(mech3::weapon_numbers_of(zrd.data(), zrd.size(), "AC20", &ac) && ac.sphere_tons == 14 && ac.sphere_slots == 10 && ac.clan_tons == 14 && ac.rounds == 5);
  EXPECT(mech3::weapon_free_name("ERPPC", defined) == "ERPPC_B");
  // ERPPC_B: harder, further, slower, Inner Sphere only, its numbers changed.
  mech3::WeaponNumbers b = n;
  b.damage = 21.5f, b.range = 1000.0f, b.fire_rate = 0.2f, b.velocity = 800.0f, b.sphere_tons = 8, b.sphere_slots = 4;
  const std::vector<uint8_t> out = mech3::weapons_with_definition(zrd.data(), zrd.size(), "ERPPC", "ERPPC_B", b,
                                                                  mech3::kInnerSphere, false);
  EXPECT(out.size() > zrd.size());
  // Every byte as it was, but BALLISTICS' count: the new pair goes after its last item.
  size_t diffs = 0;
  for (size_t i = 0; i < zrd.size() && i < out.size(); ++i) diffs += zrd[i] != out[i];
  EXPECT(diffs == 1);
  const std::vector<std::string> now = mech3::weapons_defined(out.data(), out.size());
  EXPECT(now.size() == 50 && now.back() == "ERPPC_B");
  mech3::WeaponNumbers back;
  EXPECT(mech3::weapon_numbers_of(out.data(), out.size(), "ERPPC_B", &back));
  EXPECT(back.damage == 21.5f && back.range == 1000.0f && back.fire_rate == 0.2f && back.velocity == 800.0f);
  EXPECT(back.sphere_tons == 8 && back.sphere_slots == 4 && back.clan_tons == 6);  // the clan pair as the copy's
  EXPECT(mech3::weapon_numbers_of(out.data(), out.size(), "ERPPC", &back) && back.damage == 17.0f);
  // Through the game's reader, from an archive holding it: the new one with
  // its numbers, the shipped ones as they were.
  const std::string wd = tmp_dir("new_weapon_defs");
  const std::vector<uint8_t> a = file_bytes(install + "/zbd/reader.zbd");
  const std::vector<uint8_t> written = mech3::ai::archive_with_entry(a, "weapons.zrd", out);
  EXPECT(!written.empty());
  fs::create_directories(wd + "/m/zbd");
  { std::ofstream(wd + "/m/zbd/reader.zbd", std::ios::binary).write(reinterpret_cast<const char*>(written.data()), written.size()); }
  std::vector<std::string> names = copies;
  names.push_back("ERPPC_B");
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(install);
  const std::vector<float> shipped = defs_read(names);
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(wd + "/m");
  zutl::mount(install);
  const std::vector<float> with = defs_read(names);
  EXPECT(shipped.size() == names.size() * 3 && with.size() == shipped.size());
  if (with.size() == shipped.size() && !with.empty()) {
    EXPECT(std::equal(shipped.begin(), shipped.end() - 3, with.begin()));
    EXPECT(shipped[shipped.size() - 3] == -1.0f);
    const float* e = with.data() + with.size() - 3;
    std::printf("  ERPPC_B read: damage %.1f, range %.0f, period %.2f\n", e[0], e[1], e[2]);
    EXPECT(e[0] == 21.5f && e[1] == 1000.0f && std::fabs(e[2] - 5.0f) < 1e-5f);
  }
}

TEST(new_weapon_mod_txt_line_in_place_or_last) {
  const std::string line = "weapon=ERPPC_B ammo=no tech=sphere";
  EXPECT(mech3::mod_txt_with_weapon("name=My Mod\n", "ERPPC_B", line) == "name=My Mod\n" + line + "\n");
  EXPECT(mech3::mod_txt_with_weapon("name=My Mod", "ERPPC_B", line) == "name=My Mod\n" + line + "\n");
  EXPECT(mech3::mod_txt_with_weapon("", "ERPPC_B", line) == line + "\n");
  // The same weapon (any case) replaced where it stood; another kept; CRLF kept.
  EXPECT(mech3::mod_txt_with_weapon("name=M\r\nweapon=erppc_b ammo=yes tech=any\r\nweapon=Other ammo=no tech=any\r\n",
                                    "ERPPC_B", line) ==
         "name=M\r\n" + line + "\r\nweapon=Other ammo=no tech=any\r\n");
}

// The form under a MODS EDIT session: the copy's name and numbers; a taken
// name refused with the form left up and nothing written; then OK into the
// mod (mod.txt and its own zbd\reader.zbd), and the next start's mount with
// the row in the lab's list and the definition read.
TEST(new_weapon_form_writes_into_the_mod) {
  const std::string install = MW3_GAME_DIR;
  if (!fs::exists(install + "/zbd/reader.zbd")) return;
  SavedMounts keep;
  const std::string wd = tmp_dir("new_weapon_form");
  const fs::path mod = fs::path(wd) / "mods" / "mine";
  fs::create_directories(mod);
  { std::ofstream(mod / "mod.txt") << "name=Mine\n"; }
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"mine"}, {install});
  zutl::mount(install);
  // Without a session: the line under the menu, no form.
  mech3::new_weapon_open(0);
  EXPECT(!mech3::form_shown(&mech3::new_weapon_form()));
  mech3::mods_edit_begin("mine", nullptr);
  mech3::new_weapon_open(0);
  mech3::EditorForm& f = mech3::new_weapon_form();
  EXPECT(mech3::form_shown(&f) && f.title == "New Weapon");
  mech3::FormField* copy = f.field("copy of");
  EXPECT(copy && copy->kind == mech3::FormField::kChoice && !copy->choices.empty());
  int32_t erppc = -1;
  if (copy)
    for (size_t i = 0; i < copy->choices.size(); ++i)
      if (copy->choices[i] == "ERPPC") erppc = static_cast<int32_t>(i);
  EXPECT(erppc >= 0);
  if (!copy || erppc < 0) return mech3::mods_edit_end();
  *copy->i = erppc;
  f.changed(*copy);
  EXPECT(*f.field("name")->text == "ERPPC_B" && *f.field("damage")->f == 17.0f && *f.field("Clan tons")->i == 6 &&
         *f.field("Inner Sphere tons")->i == 7);
  EXPECT(*f.field("ammo")->i == 1 && *f.field("tech")->i == 2);  // as the ERPPC: no ammo, both techs
  // A taken name: refused on the form, nothing written.
  *f.field("name")->text = "PPC";
  EXPECT(f.press("OK") && mech3::form_shown(&f));
  EXPECT(f.fields.back().label.find("already") != std::string::npos);
  EXPECT(!fs::exists(mod / "zbd" / "reader.zbd") && text_of(mod / "mod.txt") == "name=Mine\n");
  *f.field("name")->text = "bad name";
  EXPECT(f.press("OK") && mech3::form_shown(&f) && !fs::exists(mod / "zbd" / "reader.zbd"));
  // ERPPC_B, its title, its damage up.
  *f.field("name")->text = "ERPPC_B";
  *f.field("title")->text = "ER PPC B";
  *f.field("damage")->f = 25.0f;
  EXPECT(f.press("OK") && !mech3::form_shown(&f) && mech3::new_weapon_last() == "ERPPC_B");
  mech3::mods_edit_end();
  const std::string txt = text_of(mod / "mod.txt");
  std::printf("  mod.txt: %s", txt.c_str());
  EXPECT(txt == "name=Mine\nweapon=ERPPC_B ammo=no tech=any title=ER PPC B\n");
  EXPECT(fs::exists(mod / "mod.txt.bak") && fs::exists(mod / "zbd" / "reader.zbd"));
  const std::vector<uint8_t> zrd = entry_of((mod / "zbd" / "reader.zbd").string(), "weapons.zrd");
  mech3::WeaponNumbers n;
  EXPECT(mech3::weapon_numbers_of(zrd.data(), zrd.size(), "ERPPC_B", &n) && n.damage == 25.0f && n.clan_tons == 6 && n.sphere_slots == 3);
  // The next start: the mod mounted, the row in the lab's list, its definition read.
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"mine"}, {install});
  zutl::mount(install);
  const int32_t w = mech3::weapon_index("ERPPC_B");
  EXPECT(w >= 38 && !mech3::weapon_uses_ammo(w) && mech3::weapon_has_tech(w, mech3::kClan) &&
         mech3::weapon_has_tech(w, mech3::kInnerSphere));
  const std::vector<float> d = defs_read({"ERPPC_B"});
  EXPECT(d.size() == 3 && d[0] == 25.0f && d[1] == 920.0f);
  mech3::weapon_rows_clear();
}
