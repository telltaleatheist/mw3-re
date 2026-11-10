// Ours, not original code (docs/editor.md, Dear ImGui for the editor's dialogs).
#include "frontend/sdl/editor_imgui.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include "upgrades/editor/form.h"

namespace sdlfe {

namespace {
// Where the last frame drew each form's items, by label (the tests' clicks).
std::map<std::pair<const mech3::EditorForm*, std::string>, ImVec4> g_rects;
void note_rect(const mech3::EditorForm* f, const std::string& label) {
  const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
  g_rects[{f, label}] = ImVec4(a.x, a.y, b.x, b.y);
}

// One form, as ImGui widgets over its fields (form.h). A change writes the
// field at once (the form's own copy); its `changed` and the buttons' presses
// are posted to the game's next editor frame.
void tree_items(mech3::FormTree& t) {
  ImGui::SetNextItemOpen(t.open);
  const ImGuiTreeNodeFlags flags = t.kids.empty() ? ImGuiTreeNodeFlags_Leaf : 0;
  t.open = ImGui::TreeNodeEx(&t, flags, "%s", t.text.c_str());
  if (!t.open) return;
  for (mech3::FormTree& k : t.kids) tree_items(k);
  ImGui::TreePop();
}

void field(mech3::EditorForm& form, mech3::FormField& f) {
  bool changed = false;
  switch (f.kind) {
    case mech3::FormField::kLabel:
      ImGui::TextUnformatted(f.label.c_str());
      break;
    case mech3::FormField::kInt:
      changed = ImGui::InputInt(f.label.c_str(), f.i);
      break;
    case mech3::FormField::kFloat: {
      bool on = !f.on || *f.on;
      if (f.on) {
        ImGui::PushID(f.label.c_str());
        if (ImGui::Checkbox("##on", &on)) *f.on = on ? 1 : 0, changed = true;
        ImGui::PopID();
        ImGui::SameLine();
      }
      ImGui::BeginDisabled(!on);
      changed |= ImGui::InputFloat(f.label.c_str(), f.f, 0.0f, 0.0f, "%.3f");
      ImGui::EndDisabled();
      break;
    }
    case mech3::FormField::kFlag: {
      bool v = *f.flag != 0;
      if (ImGui::Checkbox(f.label.c_str(), &v)) *f.flag = v ? 1 : 0, changed = true;
      break;
    }
    case mech3::FormField::kTri: {
      int v = !*f.set ? 0 : *f.flag ? 1 : 2;
      if (ImGui::Combo(f.label.c_str(), &v, "as the default\0yes\0no\0")) {
        *f.set = v != 0, *f.flag = v == 1;
        changed = true;
      }
      break;
    }
    case mech3::FormField::kText: {
      char buf[256];
      std::snprintf(buf, sizeof buf, "%s", f.text->c_str());
      if (ImGui::InputText(f.label.c_str(), buf, sizeof buf)) *f.text = buf, changed = true;
      break;
    }
    case mech3::FormField::kChoice: {
      std::vector<const char*> items;
      for (const std::string& c : f.choices) items.push_back(c.c_str());
      if (items.size() > 8)
        changed = ImGui::ListBox(f.label.c_str(), f.i, items.data(), static_cast<int>(items.size()), 12);
      else
        changed = ImGui::Combo(f.label.c_str(), f.i, items.data(), static_cast<int>(items.size()));
      break;
    }
    case mech3::FormField::kTree:
      if (f.tree) tree_items(*f.tree);
      break;
  }
  if (f.kind != mech3::FormField::kTree) note_rect(&form, f.label);
  if (changed && f.changed) {
    mech3::FormField* p = &f;
    mech3::EditorForm* fp = &form;
    mech3::form_post([fp, p] {
      if (mech3::form_shown(fp)) fp->changed(*p);
    });
  }
}

void form(mech3::EditorForm& f, int n) {
  ImGui::PushID(&f);
  if (f.modal) {
    ImGui::OpenPopup(f.title.c_str());
    if (!ImGui::BeginPopupModal(f.title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::PopID();
      return;
    }
  } else {
    ImGui::SetNextWindowPos(ImVec2(260.0f + 24 * n, 40.0f + 24 * n), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(f.title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::End();
      ImGui::PopID();
      return;
    }
  }
  for (mech3::FormField& x : f.fields) field(f, x);
  for (size_t i = 0; i < f.buttons.size(); ++i) {
    if (i) ImGui::SameLine();
    const bool pressed = ImGui::Button(f.buttons[i].label.c_str());
    note_rect(&f, f.buttons[i].label);
    if (pressed) {
      mech3::EditorForm* fp = &f;
      const std::string label = f.buttons[i].label;
      mech3::form_post([fp, label] {
        if (mech3::form_shown(fp)) fp->press(label);
      });
    }
  }
  if (f.modal)
    ImGui::EndPopup();
  else
    ImGui::End();
  ImGui::PopID();
}
}  // namespace

bool editor_item_rect(const mech3::EditorForm* f, const std::string& label, float r[4]) {
  const auto it = g_rects.find({f, label});
  if (it == g_rects.end()) return false;
  r[0] = it->second.x, r[1] = it->second.y, r[2] = it->second.z, r[3] = it->second.w;
  return true;
}

void draw_editor_forms() {
  const std::vector<mech3::EditorForm*> forms = mech3::editor_forms();  // a copy: a press may change the list
  int n = 0;
  for (mech3::EditorForm* f : forms) form(*f, n++);
}

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
  draw = draw_editor_forms;
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
