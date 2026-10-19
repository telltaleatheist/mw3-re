#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgameapp.h"
#include "platform/platform.h"

namespace {
std::vector<std::string> g_log;

struct Probe : zgame::State {
  std::string name;
  bool accept = true;
  bool quit = false;
  explicit Probe(std::string n) : name(std::move(n)) {}
  void on_queued() override { g_log.push_back(name + ".queued"); }
  void on_leaving() override { g_log.push_back(name + ".leaving"); }
  bool enter() override { g_log.push_back(name + ".enter"); return accept; }
  bool update() override { g_log.push_back(name + ".update"); return quit; }
  void leave() override { g_log.push_back(name + ".leave"); }
  void suspend(uint32_t a) override { g_log.push_back(name + ".suspend " + std::to_string(a)); }
  void resume(uint32_t a) override { g_log.push_back(name + ".resume " + std::to_string(a)); }
};

struct Hooks : zgame::AppHooks {
  void timer_update() override { g_log.push_back("timer"); }
  void network_poll(int32_t, uint8_t) override {}
};

// Runs `frames` passes of the loop (one per peek with no message), then quits.
struct Pump : platform::MessagePump {
  int frames;
  bool quit = false;
  explicit Pump(int n) : frames(n) {}
  bool peek() override {
    if (quit || frames == 0) return true;
    --frames;
    return false;
  }
  bool pump() override { return false; }
  void wait(uint32_t ms) override { g_log.push_back("wait " + std::to_string(ms)); }
  void post_quit() override { quit = true; }
};

// The loop peeks twice per pass when nothing is waiting: once to drain, once
// in the inactive branch only, so an active pass consumes one peek.
void run(zgame::App& app, int passes) {
  Pump pump(passes);
  app.run(pump);
}

std::vector<std::string> without_timer() {
  std::vector<std::string> out;
  for (const auto& s : g_log)
    if (s != "timer") out.push_back(s);
  return out;
}
}  // namespace

TEST(zgameapp_start_queues_the_first_state) {
  g_log.clear();
  Hooks hooks;
  zgame::App app(&hooks);
  Probe intro("intro");
  app.first_state_ = &intro;
  EXPECT(app.start(0));
  EXPECT(app.active());
  EXPECT(app.pending() == 1);
  run(app, 2);  // the Replace, then one update
  const std::vector<std::string> want = {"intro.queued", "intro.enter", "intro.update"};
  EXPECT(without_timer() == want);
}

TEST(zgameapp_runs_one_command_or_one_update_per_pass) {
  g_log.clear();
  Hooks hooks;
  zgame::App app(&hooks);
  Probe menu("menu"), lab("lab");
  app.activate();
  app.replace(&menu, 0);
  app.push(&lab, 7);
  run(app, 3);
  const std::vector<std::string> want = {"menu.queued", "lab.queued", "timer", "menu.enter",
                                         "timer", "menu.suspend 7", "lab.enter", "timer",
                                         "lab.update"};
  EXPECT(g_log == want);
  EXPECT(app.depth() == 1);
  EXPECT(app.current() == &lab);
}

TEST(zgameapp_push_does_not_warn_the_state_below) {
  g_log.clear();
  Hooks hooks;
  zgame::App app(&hooks);
  Probe menu("menu"), lab("lab");
  app.activate();
  app.replace(&menu, 0);
  run(app, 1);
  g_log.clear();
  app.push(&lab, 0);
  app.pop(0);
  const std::vector<std::string> want = {"lab.queued", "menu.leaving"};
  EXPECT(g_log == want);
}

TEST(zgameapp_pop_resumes_the_state_below) {
  g_log.clear();
  Hooks hooks;
  zgame::App app(&hooks);
  Probe menu("menu"), lab("lab");
  app.activate();
  app.replace(&menu, 0);
  app.push(&lab, 0);
  app.pop(3);
  run(app, 3);
  EXPECT(without_timer().back() == "menu.resume 3");
  EXPECT(app.current() == &menu);
}

TEST(zgameapp_refused_replace_reenters_the_old_state) {
  g_log.clear();
  Hooks hooks;
  zgame::App app(&hooks);
  Probe menu("menu"), broken("broken");
  broken.accept = false;
  app.activate();
  app.replace(&menu, 0);
  app.replace(&broken, 0);
  run(app, 2);
  // menu is not on the stack yet when broken is queued, so it is not warned.
  const std::vector<std::string> want = {"menu.queued", "broken.queued", "menu.enter",
                                         "menu.leave", "broken.enter", "menu.enter"};
  EXPECT(without_timer() == want);
  EXPECT(app.current() == &menu);
}

TEST(zgameapp_refused_push_leaves_the_state_below_suspended) {
  g_log.clear();
  Hooks hooks;
  zgame::App app(&hooks);
  Probe menu("menu"), broken("broken");
  broken.accept = false;
  app.activate();
  app.replace(&menu, 0);
  app.push(&broken, 5);
  run(app, 3);
  const std::vector<std::string> want = {"menu.queued", "broken.queued", "menu.enter",
                                         "menu.suspend 5", "broken.enter", "menu.update"};
  EXPECT(without_timer() == want);
  EXPECT(app.depth() == 0);
}

TEST(zgameapp_quit_comes_from_update) {
  g_log.clear();
  Hooks hooks;
  zgame::App app(&hooks);
  Probe menu("menu");
  menu.quit = true;
  app.activate();
  app.replace(&menu, 0);
  Pump pump(100);
  app.run(pump);
  EXPECT(pump.quit);
  EXPECT(!app.active());
  EXPECT(without_timer().back() == "menu.update");
}

TEST(zgameapp_inactive_app_waits_and_does_not_update) {
  g_log.clear();
  Hooks hooks;
  zgame::App app(&hooks);
  Probe menu("menu");
  app.replace(&menu, 0);
  run(app, 4);  // two inactive passes, each peeking twice
  const std::vector<std::string> want = {"menu.queued", "wait 100", "wait 100"};
  EXPECT(without_timer() == want);
}

TEST(zgameapp_window_activation_reaches_the_state) {
  struct Act : Probe {
    using Probe::Probe;
    void on_activate(uint32_t a) override { g_log.push_back(name + ".activate " + std::to_string(a)); }
  };
  g_log.clear();
  Hooks hooks;
  zgame::App app(&hooks);
  Act menu("menu");
  app.activate();
  app.replace(&menu, 0);
  run(app, 1);
  app.on_window_activate(0);
  EXPECT(!app.active());
  app.on_window_activate(1);
  EXPECT(app.active());
  EXPECT(g_log[g_log.size() - 2] == "menu.activate 0");
  EXPECT(g_log.back() == "menu.activate 1");
}
