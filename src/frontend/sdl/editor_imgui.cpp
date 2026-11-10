// Ours, not original code (docs/editor.md, Dear ImGui for the editor's dialogs).
#include "frontend/sdl/editor_imgui.h"

#include <cstdio>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"

namespace sdlfe {

bool editor_event_to_game(const SDL_Event& e, bool want_mouse, bool want_keyboard, bool want_text) {
  switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
      return !want_mouse;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING:
      return !want_keyboard && !want_text;
    default:
      return true;
  }
}

EditorUi::EditorUi(SDL_Window* window, SDL_GPUDevice* device, std::function<bool()> up)
    : window_(window), device_(device), up_(std::move(up)) {
  draw = [] {
    ImGui::SetNextWindowPos(ImVec2(400, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 80), ImGuiCond_FirstUseEver);
    ImGui::Begin("Game editor");
    ImGui::End();
  };
}

EditorUi::~EditorUi() {
  if (!started_) return;
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

bool EditorUi::start() {
  if (started_) return true;
  if (!window_ || !device_) return false;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // no imgui.ini beside the game; window places are not kept
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();
  const float scale = SDL_GetWindowDisplayScale(window_);
  if (scale > 0) ImGui::GetStyle().ScaleAllSizes(scale), ImGui::GetStyle().FontScaleDpi = scale;
  ImGui_ImplSDL3_InitForSDLGPU(window_);
  ImGui_ImplSDLGPU3_InitInfo info;
  info.Device = device_;
  info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device_, window_);
  info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  ImGui_ImplSDLGPU3_Init(&info);
  started_ = true;
  return true;
}

bool EditorUi::event(const SDL_Event& e) {
  if (!up_ || !up_() || !start()) return false;
  ImGui_ImplSDL3_ProcessEvent(&e);
  const ImGuiIO& io = ImGui::GetIO();
  return !editor_event_to_game(e, io.WantCaptureMouse, io.WantCaptureKeyboard, io.WantTextInput);
}

bool EditorUi::step() {
  if (!up_ || !up_() || !start()) return false;
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  if (draw) draw();
  ImGui::Render();
  return true;
}

void EditorUi::overlay(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, Uint32, Uint32) {
  if (!step()) return;
  ImDrawData* d = ImGui::GetDrawData();
  if (!d || d->DisplaySize.x <= 0 || d->DisplaySize.y <= 0) return;
  ImGui_ImplSDLGPU3_PrepareDrawData(d, cmd);
  SDL_GPUColorTargetInfo t{};
  t.texture = target;
  t.load_op = SDL_GPU_LOADOP_LOAD;  // over the game's frame
  t.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &t, 1, nullptr);
  ImGui_ImplSDLGPU3_RenderDrawData(d, cmd, pass);
  SDL_EndGPURenderPass(pass);
  if (!drawn_) std::printf("mw3: the editor's dialogs drawn over the game (%d lists)\n", d->CmdListsCount), drawn_ = true;
}

}  // namespace sdlfe
