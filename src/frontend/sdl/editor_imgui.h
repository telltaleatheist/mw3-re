// Ours, not original code: Dear ImGui over the game while the game editor is
// up (docs/editor.md, Dear ImGui for the editor's dialogs). The editor's
// dialogs draw here; the editor's own menus stay zTwoD's. Made at the editor's
// first frame, so nothing of ImGui exists in play.
#pragma once

#include <functional>

#include <SDL3/SDL.h>

namespace sdlfe {

// Whether an event goes on to zInput while the editor's ImGui is up: a mouse
// event when ImGui does not want the mouse, a key or text event when it wants
// neither the keyboard nor text. Everything else (the window's, the pad's)
// goes on as before.
bool editor_event_to_game(const SDL_Event& e, bool want_mouse, bool want_keyboard, bool want_text);

class EditorUi {
 public:
  // `up`: whether the editor is up (mech3::editor_state().active).
  EditorUi(SDL_Window* window, SDL_GPUDevice* device, std::function<bool()> up);
  ~EditorUi();
  EditorUi(const EditorUi&) = delete;
  EditorUi& operator=(const EditorUi&) = delete;

  // SdlPump's overlay_event: true when the event is ImGui's and not the game's.
  bool event(const SDL_Event& e);
  // SdlRender3D's overlay: ImGui's frame drawn on the swapchain after the game's.
  void overlay(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, Uint32 width, Uint32 height);
  // ImGui's frame without the draw: what the dialogs want of the input is
  // decided here (the overlay runs it; the tests, on a hidden window, alone).
  bool step();
  // What the editor's dialogs draw, once a frame between ImGui's NewFrame and Render.
  std::function<void()> draw;
  bool started() const { return started_; }

 private:
  bool start();
  SDL_Window* window_;
  SDL_GPUDevice* device_;
  std::function<bool()> up_;
  bool started_ = false;
  bool drawn_ = false;
};

}  // namespace sdlfe
