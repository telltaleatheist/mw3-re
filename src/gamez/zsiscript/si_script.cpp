#include "gamez/zsiscript/si_script.h"

#include <cstdlib>

#include "gamez/zerror/zerr.h"
#include "gamez/zutil/zutl_zar.h"

namespace zsis {

namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zSIScript\\si_script.cpp";
constexpr uint32_t kVersion = 4;  // 0x005f4bac

// Mech3.exe 1.2 @ 0x005aa530
void reset_matrix(Part& p, bool offset_too) {
  const float id[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  for (int i = 0; i < 9; ++i) p.matrix[i] = id[i];
  if (offset_too) p.offset[0] = p.offset[1] = p.offset[2] = 0;
}

uint32_t read4(std::FILE* f, void* out) { return static_cast<uint32_t>(std::fread(out, 1, 4, f)); }
}  // namespace

// Mech3.exe 1.2 @ 0x005aa500
Part::Part()
    : translations(nullptr), scales(nullptr), rotations(nullptr), unk_0c(0), matrix{},
      offset{}, owns_buffers(true), name(nullptr), script(nullptr) {
  reset_matrix(*this, true);
}

// Mech3.exe 1.2 @ 0x005aa5a0. With `share`, uses another part's buffers.
bool Part::attach(Script* s, const Part* share) {
  if (!s) return false;
  script = s;
  reset_matrix(*this, false);
  if (!share) {
    owns_buffers = true;
    const uint32_t n = s->frame_count + 1;
    translations = static_cast<float*>(std::malloc(n * 12));
    rotations = static_cast<float*>(std::malloc(n * 16));
    return true;
  }
  owns_buffers = false;
  name = share->name;
  translations = share->translations;
  rotations = share->rotations;
  scales = share->scales;
  return true;
}

// Mech3.exe 1.2 @ 0x005aa6d0. Flags: 8 translations, 2 scales, 4 rotations.
bool Part::read(std::FILE* f) {
  zutl_zar::read_string(f, &name);
  const uint32_t n = script->frame_count + 1;
  uint32_t flags = 0;
  read4(f, &flags);
  if (flags & 8) std::fread(translations, 1, n * 12, f);
  if (flags & 2) {
    if (!scales) scales = static_cast<float*>(std::malloc(n * 12));
    std::fread(scales, 1, n * 12, f);
  }
  if (flags & 4) std::fread(rotations, 1, n * 16, f);
  return true;
}

// Mech3.exe 1.2 @ 0x005aa3b0
void Script::set_loop_time(float t) {
  if (!active) return;
  if (t <= 0.0f) {
    zerr::report(0x400, kFile, 0x1d3, "CZSIScript::SetLoopTime( t=%.2f )");
    return;
  }
  time = 0;
  loop_time = t;
  inv_loop_time = 1.0f / t;
  frame_time = t / static_cast<float>(frame_count);
  inv_frame_time = 1.0f / frame_time;
}

// Mech3.exe 1.2 @ 0x005a9c70. Read through the archive's file, not checked.
bool Script::read(std::FILE* f) {
  uint32_t version = 0;
  read4(f, &version);
  if (version != kVersion) return false;
  read4(f, &loop_time);
  read4(f, &frame_count);
  read4(f, &part_count);
  set_loop_time(loop_time);
  read4(f, &unk_0c);
  read4(f, &unk_08);
  parts = static_cast<Part**>(std::calloc(part_count, sizeof(Part*)));
  for (uint32_t i = 0; i < part_count; ++i) {
    parts[i] = new Part;
    parts[i]->attach(this, nullptr);
    parts[i]->read(f);
  }
  return true;
}

}  // namespace zsis
