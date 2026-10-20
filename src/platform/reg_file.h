#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "platform/platform.h"

// The registry the settings store uses, as a file (FIX (A), docs/settings.md):
// regedit's .reg text, one section per key, so the values read as they would
// in the registry and a Windows export of the game's keys can be dropped in
// (UTF-16 or ASCII; HKCU's or HKLM's key, under WOW6432Node or not). Values the
// game does not know are kept when it saves. Not original code.
namespace platform {

class RegFile : public Settings {
 public:
  explicit RegFile(std::string path) : path_(std::move(path)) {}
  ~RegFile() override;

  bool open(const std::string& key, bool per_user, bool write) override;
  bool query(bool per_user, const std::string& name, uint32_t* size) override;
  bool read(bool per_user, const std::string& name, void* data, uint32_t* size) override;
  bool write(bool per_user, const std::string& name, uint32_t type, const void* data, uint32_t size) override;
  void close(bool per_user) override;

  struct Value {
    std::string name;  // as written (the map is keyed by its lower case)
    uint32_t type = 3;
    std::vector<uint8_t> bytes;
  };
  // Parses .reg text into the two keys' values (for tests).
  bool parse(const std::string& text, const std::string& key);
  std::string format(const std::string& key) const;
  const std::map<std::string, Value>& values(bool per_user) const { return keys_[per_user ? 1 : 0]; }

 private:
  void load(const std::string& key);
  void flush();
  std::string path_, key_;
  std::map<std::string, Value> keys_[2];  // [0] HKLM, [1] HKCU
  bool dirty_ = false;
};

}  // namespace platform
