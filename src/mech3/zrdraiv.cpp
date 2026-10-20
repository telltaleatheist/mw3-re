#include "mech3/zrdraiv.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gamez/zerror/zerr.h"

namespace mech3::ai {

namespace {
const char kFile[] = "C:\\zipper\\Mech3\\ZRdrAiv.cpp";

void report(int level, int line, const std::string& msg) { zerr::report(level, kFile, line, msg.c_str()); }

// The folder part of a path, drive included, as _splitpath + "%s%s" give it.
std::string folder(const char* path) {
  std::string p = path;
  const size_t cut = p.find_last_of("\\/");
  return cut == std::string::npos ? (p.size() > 1 && p[1] == ':' ? p.substr(0, 2) : "") : p.substr(0, cut + 1);
}
}  // namespace

RdrHooks& rdr_hooks() {
  static RdrHooks h;
  return h;
}

// Mech3.exe 1.2 @ 0x004de9c0
RdrFile::RdrFile(const char* n, int32_t max, int32_t a18, int32_t f, int32_t q)
    : max_refs(max), x18(a18), first(f), quiet(q) {
  std::string base = n;
  const size_t dot = base.rfind('.');
  if (dot != std::string::npos) {
    if (base.compare(dot, std::string::npos, ".zrd") == 0)
      report(0x2000, 0x43, std::string(n) + ": suffix is ignored");
    else
      report(0x2000, 0x41, std::string(n) + ": suffix should be \".zrd\" or nothing");
    base.resize(dot);
  }
  base += ".zrd";
  name = strdup(base.c_str());
}

// Mech3.exe 1.2 @ 0x004deb20. The tree is not freed here (unload does that).
RdrFile::~RdrFile() {
  std::free(dir);
  std::free(name);
}

// Mech3.exe 1.2 @ 0x004deb80
int32_t RdrFile::unload() {
  zrdr::free_node(tree);
  tree = nullptr;
  return 1;
}

// Mech3.exe 1.2 @ 0x004deb50
int32_t RdrFile::add_ref() {
  const int32_t was = refs;
  if (was < max_refs) refs = was + 1;
  if (was > 0 || tree) return 0;
  return load();
}

// Mech3.exe 1.2 @ 0x004deba0
int32_t RdrFile::release() {
  const int32_t was = refs;
  if (was > 0) refs = was - 1;
  if (was > 1 || !tree) return 0;
  return unload();
}

// Mech3.exe 1.2 @ 0x004dec80
int32_t RdrFile::load() {
  if (tree) unload();
  tree = zrdr::load(name);
  if (tree) {
    resolve_dir();
    parse();
    return tree != nullptr;
  }
  if (!quiet) report(0x200, 0x9b, std::string("Couldn't read ") + name);
  return 0;
}

// Mech3.exe 1.2 @ 0x004debd0. The exe makes the path absolute (_fullpath)
// before cutting it; FIX (A): the folder of the path as found.
const char* RdrFile::resolve_dir() {
  if (dir) return dir;
  const char* found = rdr_hooks().find_loose ? rdr_hooks().find_loose(name) : nullptr;
  if (found) dir = strdup(folder(found).c_str());
  return dir;
}

// Mech3.exe 1.2 @ 0x004decf0. Only a tree that is one list holding one list
// has records; a dangling last key counts as a record. FIX (bug): the exe
// reads a non-list inner item's value as the list and crashes.
int32_t RdrFile::record_count() const {
  if (!tree || tree->type != zrdr::List || tree->list[0].i != 2) return 0;
  if (tree->list[1].type != zrdr::List) return 0;
  return (tree->list[1].list[0].i - first) >> 1;
}

// Mech3.exe 1.2 @ 0x004ded20
zrdr::Node* RdrFile::record(int32_t i, const char** key) const {
  if (i < 0 || i >= record_count()) return nullptr;
  zrdr::Node* item = &tree->list[1].list[first + 2 * i + 1];
  // FIX (bug): the exe hands back any key's value as the name; a non-string
  // one crashes the callers that copy it.
  *key = item->type == zrdr::String ? item->s : nullptr;
  return item + 1;
}

// Mech3.exe 1.2 @ 0x004ded60
zrdr::Node* RdrFile::find(const char* key) const { return zrdr::find(tree, key); }

}  // namespace mech3::ai
