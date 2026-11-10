// The developers' teleport (mech3/teleport, docs/editor.md, Teleport): the
// list from c1's shipped location.zrd, the saved text as the exe writes it
// and read back, and dialog 132's handlers on the DDX stand-in.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/teleport.h"

namespace {
namespace fs = std::filesystem;
struct Hooks : mech3::TeleportHooks {
  std::vector<mech3::TeleportSpot> nav;
  int32_t answer = 6;
  std::vector<std::string> asked;
  std::vector<mech3::TeleportSpot> navs() override { return nav; }
  int32_t message_box(int32_t style, int32_t caption, int32_t text, const std::string& arg) override {
    char b[96];
    std::snprintf(b, sizeof b, "%d %#x %#x %s", style, caption, text, arg.c_str());
    asked.push_back(b);
    return answer;
  }
};
zrdr::Node* text_tree(const std::string& text) {
  std::FILE* f = std::tmpfile();
  std::fwrite(text.data(), 1, text.size(), f);
  std::rewind(f);
  zrdr::Node* n = zrdr::parse_text(f);
  std::fclose(f);
  return n;
}
}  // namespace

TEST(teleport_spots_from_c1_and_the_saved_text) {
  const std::string install = MW3_GAME_DIR;
  if (!fs::exists(install + "/zbd/c1/reader.zbd")) return;
  Hooks h;
  h.nav = {{{10.0f, 2.0f, -30.0f}, 99.0f, "NAV ALPHA", 1}};
  mech3::set_teleport_hooks(&h);
  const zutl::Mounts keep = zutl::mounts();
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(install);
  zrdr::init();
  EXPECT(zrdr::open_archive("zbd\\c1\\reader.zbd", true));
  zrdr::Node* loc = zrdr::load("location.zrd");
  EXPECT(loc != nullptr);
  const float me[3] = {1.0f, 2.0f, 3.0f};
  const std::vector<mech3::TeleportSpot> s = mech3::teleport_spots(me, loc);
  if (loc) zrdr::free_node(loc);
  zrdr::close_archives(true);
  zutl::mounts() = keep;
  std::string names;
  for (const auto& x : s) names += x.name + "; ";
  std::printf("  %s\n", names.c_str());
  EXPECT(s.size() == 6 && s[0].name == "Current Location" && s[0].unsaved && s[0].pos[2] == 3.0f);
  EXPECT(s[1].name == "NAV ALPHA" && s[1].yaw == 0.0f && !s[1].unsaved);  // a nav point's yaw 0, saved
  EXPECT(s[2].name == "Mission 1 Start" && s[2].pos[0] == -2897.0f && s[4].name == "Mission 3 Start" && s[4].yaw == -90.0f);
  EXPECT(s[5].name == "IA_base");
  const std::string t = mech3::location_zrd_text(s);
  std::printf("%s", t.c_str());
  EXPECT(t.rfind("(\n  ;; LOCATION.ZRD stores teleport data for this mission.\n  ;; Last updated Sep 20 1999 at 16:29:38.\n\n", 0) == 0);
  EXPECT(t.find("\"Mission 3 Start\" ( (    402.0     33.0   -980.0 )    -90.0 )  ;; position/yaw\n") != std::string::npos);
  EXPECT(t.find("Current Location") == std::string::npos && t.size() > 2 && t.compare(t.size() - 2, 2, ")\n") == 0);
  // Read back by the text reader: the same spots, to the tenth.
  zrdr::Node* back = text_tree(t);
  EXPECT(back != nullptr);
  h.nav.clear();
  const std::vector<mech3::TeleportSpot> r = mech3::teleport_spots(me, back);
  if (back) zrdr::free_node(back);
  EXPECT(r.size() == s.size());
  for (size_t i = 1; i < r.size() && i < s.size(); ++i)
    EXPECT(r[i].name == s[i].name && std::fabs(r[i].pos[0] - s[i].pos[0]) < 0.06f && std::fabs(r[i].yaw - s[i].yaw) < 0.06f);
  mech3::set_teleport_hooks(nullptr);
}

TEST(teleport_dialog_handlers) {
  Hooks h;
  mech3::set_teleport_hooks(&h);
  std::vector<mech3::TeleportSpot> spots = {{{1, 2, 3}, 0, "Current Location", 1}, {{5, 6, 7}, 45, "zeta", 0},
                                            {{8, 9, 10}, -90, "Alpha", 0}};
  mech3::TeleportDialog d;
  const float pos[3] = {1, 2, 3};
  mech3::teleport_dialog_begin(d, &spots, pos, 0.5f);
  mech3::DlgControl& list = d.dlg.at(mech3::TeleportDialog::kList);
  // OnInitDialog: the fields filled (their EN_CHANGE sets the flag), the list sorted, Current Location picked.
  EXPECT(d.changed == 1 && d.dlg.at(mech3::TeleportDialog::kYaw).text == "28.6479");
  EXPECT(list.combo.texts() == std::vector<std::string>({"Alpha", "Current Location", "zeta"}) && list.combo.cur == 1 &&
         list.text == "Current Location");
  list.combo.cur = 2;  // zeta
  d.pick();
  EXPECT(d.x == 5 && d.yaw == 45 && d.dlg.at(mech3::TeleportDialog::kX).text == "5" && list.text == "zeta");
  // A typed name: added in its sorted place at the fields' numbers.
  d.dlg.at(mech3::TeleportDialog::kZ).text = "12.5";
  list.text = "middle";
  d.left_list();
  EXPECT(spots.size() == 4 && spots[3].name == "middle" && spots[3].pos[2] == 12.5f);
  EXPECT(list.combo.texts() == std::vector<std::string>({"Alpha", "Current Location", "middle", "zeta"}) &&
         list.combo.data(2) == 3);
  list.text = "ALPHA";  // there already, any case: nothing added
  d.left_list();
  EXPECT(spots.size() == 4);
  list.text = "";
  d.left_list();
  EXPECT(h.asked.size() == 1 && h.asked.back() == "48 0x1dd 0x1de ");
  // Delete: asked with the list's text; Yes marks the picked one unsaved and takes it off the list.
  list.combo.cur = 0, list.text = "Alpha";
  d.remove();
  EXPECT(h.asked.back() == "4 0x1dd 0x1df Alpha" && spots[2].unsaved == 1 && list.combo.items.size() == 3 && list.combo.cur == -1);
  d.remove();  // nothing picked: asked, nothing done (the exe wrote through CB_ERR)
  EXPECT(list.combo.items.size() == 3);
  h.answer = 7;
  list.combo.cur = 0;
  d.remove();
  EXPECT(list.combo.items.size() == 3 && !spots[1].unsaved);
  // OK: the fields back, the yaw in radians; a yaw past 360 keeps the dialog up.
  float p[3], yaw = 0;
  int32_t changed = 0;
  d.dlg.at(mech3::TeleportDialog::kYaw).text = "400";
  EXPECT(!mech3::teleport_dialog_ok(d, p, &yaw, &changed));
  d.dlg.at(mech3::TeleportDialog::kYaw).text = "90";
  EXPECT(mech3::teleport_dialog_ok(d, p, &yaw, &changed) && p[0] == 5 && p[2] == 12.5f && std::fabs(yaw - 1.5707963f) < 1e-6f &&
         changed == 1);
  mech3::set_teleport_hooks(nullptr);
}
