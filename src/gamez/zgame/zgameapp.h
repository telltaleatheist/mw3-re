#pragma once
#include <cstdint>
#include <deque>

// C:\zipper\GameZMech3\zGame\zGameApp.cpp: the engine's application object. In the
// original it is an MFC CWinApp whose Run() (0x0058fd70) pumps Windows messages
// and drives a stack of game states. The rebuild keeps the state stack and the
// per-frame order; the front end pumps its own events and calls frame().
namespace zgame {

// A game state (front end, briefing, MechLab, mission, ...). Methods are named
// for the vtable slots Run() calls.
class State {
 public:
  virtual ~State() = default;
  virtual bool enter() = 0;               // +0x0c  false: refused, not pushed
  virtual bool update() = 0;              // +0x10  true: quit the game
  virtual void leave() = 0;               // +0x18
  virtual void suspend(uint32_t arg) = 0; // +0x1c  another state is pushed on top
  virtual void resume(uint32_t arg) = 0;  // +0x20  the state above was popped
};

enum class Command : int { Pop = 1, Push = 2, Replace = 3 };  // field +0x4

class App {
 public:
  static constexpr int kMaxDepth = 16;

  // Queues a state change. It runs on a later frame(), one per frame, in order.
  void queue(Command type, State* state, uint32_t arg);

  // One pass of Run()'s loop after the message pump. Returns false when the
  // current state asks to quit (Run() then calls PostQuitMessage).
  bool frame();

  State* current() const;                  // 0x00590230
  void set_active(bool active) { active_ = active; }  // 0x0058fa80 / 0x0058faa0
  bool active() const { return active_; }
  int depth() const { return depth_; }

  // Hooks the original calls every frame, outside this file.
  void (*poll_network)() = nullptr;        // 0x00588930, zNetwork\znet_dplay.cpp

 private:
  struct Pending {
    Command type;
    State* state;
    uint32_t arg;
  };
  void run_command(const Pending& cmd);

  bool active_ = true;                     // +0x0d0
  int depth_ = -1;                         // +0x0c8, -1 until the first push
  State* stack_[kMaxDepth] = {};           // +0x0d8
  std::deque<Pending> pending_;            // +0x120..+0x148, an MSVC std::deque
};

}  // namespace zgame
