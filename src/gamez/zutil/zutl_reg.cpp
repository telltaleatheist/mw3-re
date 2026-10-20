#include "gamez/zutil/zutl_reg.h"

#include <cstring>
#include <memory>

#include "platform/platform.h"

namespace zreg {

namespace {
struct State {
  bool initialized = false;                       // 0x00731914
  std::string company, product, version;          // 0x00731918 0x0073191c 0x00731920
  std::vector<std::unique_ptr<Entry>> owned;
  std::vector<Entry*> list;                       // 0x00731910, newest first
  Machine machine;                                // 0x00731924
  uint32_t install_options = 0;                   // 0x00731954
  platform::Settings* settings = nullptr;
};
State g;

constexpr uint32_t kRegBinary = 3, kRegDword = 4;

// The value's size as the registry holds it (0x005977d0's switch).
uint32_t value_size(const Entry& e) {
  if (e.type == kDword || e.type == kBinary4) return 4;
  if (e.type == kBinary8) return 8;
  return e.size;
}
}  // namespace

void set_settings(platform::Settings* s) { g.settings = s; }
platform::Settings* settings() { return g.settings; }
const std::vector<Entry*>& entries() { return g.list; }

std::string key_path() { return "SOFTWARE\\" + g.company + "\\" + g.product + "\\" + g.version; }

// Mech3.exe 1.2 @ 0x00598210. The old list is dropped without being freed, as
// in the original (it is only ever called once).
void init(const char* company, const char* product, const char* version, const Machine& m,
          uint32_t install_options) {
  g.company = company ? company : "";
  g.product = product ? product : "";
  g.version = version ? version : "";
  g.list.clear();
  g.owned.clear();
  g.initialized = true;
  g.machine = m;  // 0x00597fc0
  g.install_options = install_options;
}

// Mech3.exe 1.2 @ 0x00598270
void shutdown() {
  if (!g.initialized) return;
  g.list.clear();
  g.owned.clear();
  g.company.clear();
  g.product.clear();
  g.version.clear();
  g.initialized = false;
}

// Mech3.exe 1.2 @ 0x00597d10: by exact name.
Entry* find(const char* name) {
  for (Entry* e : g.list)
    if (e->name == name) return e;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x00597d90
Entry* create(const char* name, int32_t type, uint32_t size, int32_t scope, void* storage) {
  if (Entry* e = find(name)) return e;
  auto e = std::make_unique<Entry>();
  e->name = name;
  e->type = type;
  e->size = size;
  e->scope = scope;
  switch (type) {
    case kDword:
    case kBinary4: e->size = 4; break;
    case kBinary8: e->size = 8; break;
    case 3: case 4: case 5: case 6: case 7:
      if (size == 0) return nullptr;
      if (storage) std::memset(storage, 0, size);  // calloc(1, size)
      break;
    default: break;
  }
  if (storage) {
    e->data = static_cast<uint8_t*>(storage);
  } else {
    e->own.assign(type >= 3 && type <= 7 ? size : 8, 0);
    e->data = e->own.data();
  }
  Entry* p = e.get();
  g.owned.push_back(std::move(e));
  g.list.insert(g.list.begin(), p);
  return p;
}

// Mech3.exe 1.2 @ 0x00597d80
uint32_t install_options(uint32_t mask) { return mask & g.install_options; }

// Mech3.exe 1.2 @ 0x00597fa0
const Machine& machine() { return g.machine; }

// Mech3.exe 1.2 @ 0x005977d0. Both keys must open, or nothing is read. A value
// is taken only when its size is the entry's; its registry type is not checked.
int32_t read_all() {
  platform::Settings* s = g.settings;
  if (!s) return 0;
  const std::string key = key_path();
  if (!s->open(key, true, false)) return 0;
  if (!s->open(key, false, false)) {
    s->close(true);
    return 0;
  }
  for (Entry* e : g.list) {
    if (e->scope != kUser && e->scope != kMachine) continue;
    const bool user = e->scope == kUser;
    uint32_t want = value_size(*e), got = 0;
    if (s->query(user, e->name, &got) && got == want) s->read(user, e->name, e->data, &want);
  }
  s->close(true);
  s->close(false);
  return 1;
}

// Mech3.exe 1.2 @ 0x00597a60. A failed write returns at once, the keys left
// open as the original leaves them.
int32_t write_all() {
  platform::Settings* s = g.settings;
  if (!g.initialized || !s) return 0;
  const std::string key = key_path();
  if (!s->open(key, true, true)) {
    s->close(true);  // the original closes the handle it did not get
    return 0;
  }
  if (!s->open(key, false, true)) {
    s->close(true);
    s->close(false);
    return 0;
  }
  bool failed = false;
  for (Entry* e : g.list) {
    if (e->scope != kUser && e->scope != kMachine) continue;
    const bool user = e->scope == kUser;
    if (e->type == kDword) failed = !s->write(user, e->name, kRegDword, e->data, e->size);
    else if (e->type >= kBinary4 && e->type <= kBlock) failed = !s->write(user, e->name, kRegBinary, e->data, e->size);
    if (failed) return 0;  // other types keep the last result
  }
  s->close(true);
  s->close(false);
  return 1;
}

}  // namespace zreg
