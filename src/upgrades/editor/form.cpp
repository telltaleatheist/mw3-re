#include "upgrades/editor/form.h"

#include <algorithm>

namespace mech3 {

FormField* EditorForm::field(const std::string& label) {
  for (FormField& f : fields)
    if (f.label == label) return &f;
  return nullptr;
}

bool EditorForm::press(const std::string& label) {
  for (const FormButton& b : buttons)
    if (b.label == label) {
      if (b.press) b.press();  // may hide or rebuild this form: nothing of it is used after
      return true;
    }
  return false;
}

void EditorForm::changed(FormField& f) {
  if (f.changed) f.changed();
}

std::vector<EditorForm*>& editor_forms() {
  static std::vector<EditorForm*> forms;
  return forms;
}

void form_show(EditorForm* f) {
  form_hide(f);
  editor_forms().push_back(f);
}

void form_hide(EditorForm* f) {
  auto& v = editor_forms();
  v.erase(std::remove(v.begin(), v.end(), f), v.end());
}

bool form_shown(const EditorForm* f) {
  const auto& v = editor_forms();
  return std::find(v.begin(), v.end(), f) != v.end();
}

namespace {
std::vector<std::function<void()>>& posted() {
  static std::vector<std::function<void()>> v;
  return v;
}
}  // namespace

void form_post(std::function<void()> action) { posted().push_back(std::move(action)); }

void form_run_posted() {
  std::vector<std::function<void()>> run;
  run.swap(posted());
  for (auto& a : run) a();
}

}  // namespace mech3
