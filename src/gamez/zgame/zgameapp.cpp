#include "gamez/zgame/zgameapp.h"

#include "gamez/zerror/zerr.h"
#include "gamez/zutil/ztimer.h"

namespace zgame {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zGame\\zGameApp.cpp";
}  // namespace

void App::queue(Command type, State* state, uint32_t arg) {
  pending_.push_back(Pending{type, state, arg});
}

// Mech3.exe 1.2 @ 0x00590230
State* App::current() const {
  if (depth_ < 0 || depth_ > kMaxDepth - 1) return nullptr;
  return stack_[depth_];
}

// Mech3.exe 1.2 @ 0x0058fd70, the body of Run()'s loop after PeekMessage/PumpMessage.
// The original also sleeps in MsgWaitForMultipleObjects while inactive; the front
// end owns waiting now.
bool App::frame() {
  ztimer::update();                        // 0x00575a00
  if (poll_network) poll_network();        // 0x00588930
  State* cur = current();
  if (!active_) return true;
  if (!pending_.empty()) {
    run_command(pending_.front());
    pending_.pop_front();
    return true;
  }
  if (cur && cur->update()) {
    active_ = false;                       // vtable +0xa8 before PostQuitMessage
    return false;
  }
  return true;
}

// The command half of 0x0058fd70. Kept as the original wrote it, including what a
// rewrite might "fix": Pop resumes stack[0] after an underflow, and Push past the
// top overwrites the topmost state.
void App::run_command(const Pending& cmd) {
  State* cur = current();
  switch (cmd.type) {
    case Command::Pop: {
      if (cur) cur->leave();
      // The original clears stack[depth] even at depth -1, which lands on the
      // unused field at +0x0d4; nothing reads it, so the rebuild skips the write.
      if (depth_ >= 0) stack_[depth_] = nullptr;
      depth_ = depth_ - 1;
      if (depth_ < 0) {
        zerr::report(0x400, kFile, 0xf8, "Stack Depth Underflow");
        depth_ = 0;
      }
      if (stack_[depth_]) stack_[depth_]->resume(cmd.arg);
      break;
    }
    case Command::Push: {
      if (!cmd.state) break;
      // The original calls stack[depth]->suspend() unchecked, so a Push onto an
      // empty stack would crash; the game always starts with a Replace.
      if (cur) cur->suspend(cmd.arg);
      if (cmd.state->enter()) {
        depth_ = depth_ + 1;
        if (depth_ > kMaxDepth - 1) {
          zerr::report(0x400, kFile, 0xe1, "Stack Depth Overflow");
          depth_ = kMaxDepth - 1;
        }
        stack_[depth_] = cmd.state;
      }
      break;
    }
    case Command::Replace: {
      if (!cmd.state) break;
      if (cur) cur->leave();
      if (depth_ < 0) depth_ = 0;
      if (depth_ > kMaxDepth - 1) depth_ = kMaxDepth - 1;
      if (cmd.state->enter()) {
        stack_[depth_] = cmd.state;
      } else if (cur) {
        cur->enter();                      // the refused state's predecessor comes back
      }
      break;
    }
  }
}

}  // namespace zgame
