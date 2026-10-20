// Probes for test_settings.py (docs/settings.md): the settings store
// (gamez/zutil/zutl_reg) and the detail.zrd loader 0x0040c420 (mech3/options).
// The registry is a fake here and in the emulator alike: the same values, its
// calls recorded through the shared callback, its failures from the script.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_reg.h"
#include "mech3/options.h"
#include "platform/platform.h"
#include "probe_app.h"

namespace {
using probe_app::call;

constexpr uint32_t kHkcu = 0x80000001, kHklm = 0x80000002;
uint32_t handle(bool user) { return user ? 0x101 : 0x100; }

std::string hex(const void* p, uint32_t n) {
  std::string s;
  char b[4];
  for (uint32_t i = 0; i < n; ++i) {
    std::snprintf(b, sizeof b, "%02x", static_cast<const uint8_t*>(p)[i]);
    s += b;
  }
  return s;
}

struct FakeRegistry : platform::Settings {
  std::map<std::string, std::vector<uint8_t>> values[2];  // [0] HKLM, [1] HKCU
  bool opened[2] = {};
  bool open(const std::string& key, bool user, bool write) override {
    const uint32_t r = call(write ? "RegCreateKeyExA" : "RegOpenKeyExA", {user ? kHkcu : kHklm, key.c_str()});
    opened[user] = r == 0;
    return r == 0;
  }
  bool query(bool user, const std::string& name, uint32_t* size) override {
    call("RegQueryValueExA", {opened[user] ? handle(user) : 0xbad, name.c_str(), 0, 0});
    auto it = values[user].find(name);
    if (it == values[user].end()) return false;
    *size = static_cast<uint32_t>(it->second.size());
    return true;
  }
  bool read(bool user, const std::string& name, void* data, uint32_t* size) override {
    call("RegQueryValueExA", {opened[user] ? handle(user) : 0xbad, name.c_str(), 1, *size});
    auto it = values[user].find(name);
    if (it == values[user].end() || it->second.size() > *size) return false;
    std::memcpy(data, it->second.data(), it->second.size());
    *size = static_cast<uint32_t>(it->second.size());
    return true;
  }
  bool write(bool user, const std::string& name, uint32_t type, const void* data, uint32_t size) override {
    const std::string h = hex(data, size);
    return call("RegSetValueExA", {opened[user] ? handle(user) : 0xbad, name.c_str(), type, size, h.c_str()}) == 0;
  }
  void close(bool user) override {
    call("RegCloseKey", {opened[user] ? handle(user) : 0xbad});
    opened[user] = false;
  }
};
FakeRegistry g_reg;

zrdr::Node* g_tree = nullptr;

struct LoaderHooks : mech3::OptionsHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return call(key, args);
  }
  zrdr::Node* reader_load(const char* name) override {
    call("0x00556740", {name});
    if (std::strcmp(name, "detail.zrd") != 0) return nullptr;
    zrdr::Node* t = g_tree;
    g_tree = nullptr;
    return t;
  }
  void reader_free(zrdr::Node* n) override {
    call("0x00556820", {});
    zrdr::free_node(n);
  }
  const char* message(int32_t id) override {
    call("0x00576040", {id});
    return "DefaultCtlConfig";
  }
  void camera_get_fov(zgame::Handle cam, float* x, float* y) override {
    call("0x004f3ee0", {cam});
    *x = *y = 1.0f;
  }
};
LoaderHooks g_hooks;

}  // namespace

extern "C" {

// A fresh store (0x00598210) with the machine's 12 dwords and InstallOptions.
void probe_settings_reset(const uint32_t* machine, uint32_t install_options) {
  zreg::shutdown();
  mech3::options() = mech3::Options{};
  zreg::Machine m;
  std::memcpy(&m, machine, sizeof m);
  zreg::init("MicroProse", "MechWarrior 3", "1.0", m, install_options);
  g_reg = FakeRegistry{};
  zreg::set_settings(&g_reg);
  mech3::set_options_hooks(&g_hooks);
}

// zInput's entry, made before the loader as InitInstance makes it.
void probe_settings_cmdmap(uint32_t size) {
  zreg::Entry* e = zreg::create("DefaultCmdMap", zreg::kBlock, size, zreg::kUser);
  for (uint32_t i = 0; e && i < size; ++i) e->data[i] = static_cast<uint8_t>(i * 7 + 3);
}

void probe_settings_reg(int32_t user, const char* name, const uint8_t* data, uint32_t size) {
  g_reg.values[user ? 1 : 0][name].assign(data, data + size);
}

// The loader on a detail.zrd read from `zrd` (binary nodes; len < 0: the file
// cannot be read, has_path 0: no file named).
int32_t probe_settings_load(int32_t has_path, const uint8_t* zrd, int32_t len) {
  g_tree = nullptr;
  if (len >= 0) {
    std::FILE* f = std::tmpfile();
    std::fwrite(zrd, 1, static_cast<size_t>(len), f);
    std::rewind(f);
    g_tree = static_cast<zrdr::Node*>(std::calloc(1, sizeof(zrdr::Node)));
    zrdr::read_node(f, g_tree);
    std::fclose(f);
  }
  const int32_t r = mech3::load_options(has_path ? "detail.zrd" : nullptr);
  if (g_tree) zrdr::free_node(g_tree), g_tree = nullptr;
  return r;
}

int32_t probe_settings_save() { return zreg::write_all(); }

// Sets an option entry's value by name (as the menus would), for the save test.
void probe_settings_poke(const char* name, const uint8_t* data, uint32_t size) {
  if (zreg::Entry* e = zreg::find(name)) std::memcpy(e->data, data, size);
}

// The store: one line per entry, newest first: name type size scope hex.
int32_t probe_settings_dump(char* out, int32_t cap) {
  std::string s;
  for (const zreg::Entry* e : zreg::entries()) {
    char b[64];
    std::snprintf(b, sizeof b, " %d %u %d ", e->type, e->size, e->scope);
    s += e->name + b + hex(e->data, e->type <= 1 ? 4 : e->size) + "\n";
  }
  s += mech3::options().default_cmd_map ? "cmdmap set\n" : "cmdmap none\n";
  std::snprintf(out, static_cast<size_t>(cap), "%s", s.c_str());
  return static_cast<int32_t>(s.size());
}

void probe_settings_machine_copy(uint32_t* out) { std::memcpy(out, &mech3::options().machine, 0x30); }

}  // extern "C"
