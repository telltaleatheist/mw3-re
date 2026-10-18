#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgameapp.h"

namespace {
std::vector<std::string> g_log;

struct Probe : zgame::State {
  std::string name;
  bool accept = true;
  bool quit = false;
  explicit Probe(std::string n) : name(std::move(n)) {}
  bool enter() override { g_log.push_back(name + ".enter"); return accept; }
  bool update() override { g_log.push_back(name + ".update"); return quit; }
  void leave() override { g_log.push_back(name + ".leave"); }
  void suspend(uint32_t a) override { g_log.push_back(name + ".suspend " + std::to_string(a)); }
  void resume(uint32_t a) override { g_log.push_back(name + ".resume " + std::to_string(a)); }
};
}  // namespace

TEST(zgameapp_runs_one_command_or_one_update_per_frame) {
  g_log.clear();
  zgame::App app;
  Probe menu("menu"), lab("lab");
  app.queue(zgame::Command::Replace, &menu, 0);
  app.queue(zgame::Command::Push, &lab, 7);
  app.frame();  // Replace
  app.frame();  // Push
  app.frame();  // update the top state
  const std::vector<std::string> want = {"menu.enter", "menu.suspend 7", "lab.enter", "lab.update"};
  EXPECT(g_log == want);
  EXPECT(app.depth() == 1);
  EXPECT(app.current() == &lab);
}

TEST(zgameapp_pop_resumes_the_state_below) {
  g_log.clear();
  zgame::App app;
  Probe menu("menu"), lab("lab");
  app.queue(zgame::Command::Replace, &menu, 0);
  app.queue(zgame::Command::Push, &lab, 0);
  app.queue(zgame::Command::Pop, nullptr, 3);
  app.frame();
  app.frame();
  app.frame();
  EXPECT(g_log.back() == "menu.resume 3");
  EXPECT(app.current() == &menu);
}

TEST(zgameapp_refused_replace_reenters_the_old_state) {
  g_log.clear();
  zgame::App app;
  Probe menu("menu"), broken("broken");
  broken.accept = false;
  app.queue(zgame::Command::Replace, &menu, 0);
  app.queue(zgame::Command::Replace, &broken, 0);
  app.frame();
  app.frame();
  const std::vector<std::string> want = {"menu.enter", "menu.leave", "broken.enter", "menu.enter"};
  EXPECT(g_log == want);
  EXPECT(app.current() == &menu);
}

TEST(zgameapp_quit_comes_from_update) {
  zgame::App app;
  Probe menu("menu");
  menu.quit = true;
  app.queue(zgame::Command::Replace, &menu, 0);
  EXPECT(app.frame());
  EXPECT(!app.frame());
}

TEST(zgameapp_inactive_app_does_not_update) {
  g_log.clear();
  zgame::App app;
  Probe menu("menu");
  app.queue(zgame::Command::Replace, &menu, 0);
  app.frame();
  app.set_active(false);
  app.frame();
  EXPECT(g_log.size() == 1);
}
