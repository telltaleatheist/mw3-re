// Probe for test_pm_state.py: Pirate's Moon's multiplayer menu state made and deleted (mech3/pilot_menu
// GameMenuState: its constructor, PM 0x004a0120, and its destructor, PM 0x004a0230). The engine here only
// records: each screen it is asked to make (the constructor's address as the port names it, and the state's
// address passed), answering with the test's handle or 0 for a failed `new`; and each virtual call on a screen.
#include <cstdint>
#include <memory>
#include <vector>

#include "mech3/app.h"
#include "mech3/app_hooks.h"
#include "mech3/pilot_menu.h"
#include "probe_app.h"

namespace {
struct StateEngine : mech3::Engine {
  std::vector<uint32_t> log;
  const uint32_t* handles = nullptr;
  uint32_t fail = 0;
  int made = 0;
  mech3::Handle construct(uint32_t ctor, std::initializer_list<mech3::Arg> args) override {
    const int i = made++;
    const uint32_t h = i < 3 && !(fail >> i & 1) ? handles[i] : 0;
    log.insert(log.end(), {1u, ctor, args.size() ? args.begin()->v : 0u, h});
    return h;
  }
  uint32_t vcall(mech3::Handle obj, uint32_t slot, std::initializer_list<mech3::Arg> args) override {
    log.insert(log.end(), {2u, static_cast<uint32_t>(obj), slot, args.size() ? args.begin()->v : 0u});
    return 0;
  }
};
}  // namespace

// The state at `addr` made (a failed `new` for each bit of `fail`), then the screens of `gone`'s bits cleared
// as if already let go, then deleted. out: the records, four words each; the count returned.
extern "C" int probe_pm_state_run(uint32_t addr, const uint32_t* handles, uint32_t fail, uint32_t gone, uint32_t* out, int cap) {
  StateEngine engine;
  engine.handles = handles, engine.fail = fail;
  std::unique_ptr<mech3::Mech3App> app;
  {
    probe_app::Mute mute;
    app = std::make_unique<mech3::Mech3App>(&engine);
  }
  mech3::set_app(app.get());
  engine.log.clear(), engine.made = 0;
  {
    auto state = std::make_unique<mech3::GameMenuState>(mech3::GameMenuState::kMultiplayer, addr);
    engine.log.push_back(3u);  // made: the three screens as the state holds them
    for (const mech3::Handle s : state->screens) engine.log.push_back(static_cast<uint32_t>(s));
    for (int i = 0; i < 3; ++i)
      if (gone >> i & 1) state->screens[i] = 0;
  }
  mech3::set_app(nullptr);
  const int n = static_cast<int>(engine.log.size());
  for (int i = 0; i < n && i < cap; ++i) out[i] = engine.log[static_cast<size_t>(i)];
  return n;
}
