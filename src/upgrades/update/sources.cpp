#include "upgrades/update/sources.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <mutex>

#include "upgrades/update/json_reader.h"

namespace upgrades::update {

extern const char kBuiltinSources[];  // sources_builtin.cpp, written by CMake from assets/launcher/sources.json

namespace {
using detail::Json;

std::mutex g_mutex;
Sources* g_active = nullptr;
std::vector<std::string> g_learned;

bool plain_host(const std::string& h) {
  if (h.empty() || h.size() > 253 || h.find('.') == std::string::npos || h.find("..") != std::string::npos || h.front() == '.' ||
      h.back() == '.' || h.front() == '-')
    return false;
  for (char c : h)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '.') return false;
  return true;
}

// A list of strings at `key`, each checked by `ok`; absent: `needed` says. The first thing wrong in `why`.
bool strings(const Json& root, const char* key, bool needed, size_t most, const std::function<bool(std::string*)>& ok,
             std::vector<std::string>* out, std::string* why) {
  const Json* v = root.at(key);
  if (!v) return !needed || (*why = std::string("no ") + key, false);
  if (v->type != Json::kArray) return *why = std::string(key) + " is not a list", false;
  if (v->items.size() > most) return *why = std::string(key) + " lists more than " + std::to_string(most), false;
  for (const Json& j : v->items) {
    std::string s = j.s;
    if (j.type != Json::kString || !ok(&s)) return *why = std::string(key) + ": " + (j.type == Json::kString ? j.s : "not a text") + " is not one", false;
    for (const std::string& o : *out)
      if (o == s) return *why = std::string(key) + ": " + s + " twice", false;
    out->push_back(std::move(s));
  }
  return true;
}

}  // namespace

bool https_address(std::string* url, bool path) {
  const std::string kScheme = "https://";
  if (url->compare(0, kScheme.size(), kScheme) != 0) return false;
  const size_t slash = url->find('/', kScheme.size());
  if (!plain_host(url->substr(kScheme.size(), slash == std::string::npos ? std::string::npos : slash - kScheme.size()))) return false;
  if (slash == std::string::npos) return true;
  if (!path) return slash + 1 == url->size() ? (url->pop_back(), true) : false;
  if (url->find("..", slash) != std::string::npos) return false;
  for (size_t i = slash; i < url->size(); ++i)
    if ((*url)[i] <= ' ' || (*url)[i] > '~' || (*url)[i] == '\\') return false;
  return true;
}

bool is_key_hex(std::string* key) {
  if (key->size() != 64) return false;
  for (char& c : *key) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return true;
}

bool read_sources(const std::string& json, bool allow_file, Sources* out, std::string* why) {
  *out = {}, why->clear();
  detail::Reader r(json);
  Json root;
  if (!r.value(&root) || !r.end()) return *why = r.error, false;
  if (root.type != Json::kObject) return *why = "not an object", false;
  const Json* v = root.at("sources");
  if (!v || v->type != Json::kNumber) return *why = "no sources number", false;
  if (std::atoi(v->s.c_str()) != 1) return *why = "a sources format (" + v->s + ") this launcher does not read", false;
  const auto address = [allow_file](std::string* u) {
    if (allow_file && u->rfind("file:///", 0) == 0) return u->find("..") == std::string::npos;
    return https_address(u, true);
  };
  if (!strings(root, "catalog", true, kMaxSourceAddresses, address, &out->catalog, why)) return false;
  if (out->catalog.empty()) return *why = "catalog lists no address", false;
  if (!strings(root, "keys", true, kMaxKeys, is_key_hex, &out->keys, why)) return false;
  return strings(root, "unsigned_sums", false, kMaxSourceAddresses, address, &out->unsigned_sums, why);
}

const std::string& builtin_sources_text() {
  static const std::string text = kBuiltinSources;
  return text;
}

Sources load_sources(const std::string& path, bool allow_file, std::string* said) {
  Sources s;
  std::string why = "it is not there";
  if (!path.empty()) {
    std::ifstream f(path, std::ios::binary);
    if (f) {
      const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      if (read_sources(text, allow_file, &s, &why)) return *said = "sources: " + path, s;
    }
  }
  std::string built_why;
  if (!read_sources(builtin_sources_text(), false, &s, &built_why)) {  // a build's own data: its test stops this first
    std::fprintf(stderr, "the built-in sources.json cannot be read: %s\n", built_why.c_str());
    std::abort();
  }
  *said = "sources: the built-in copy (" + path + ": " + why + ")";
  return s;
}

void use_sources(const Sources& s) {
  std::lock_guard<std::mutex> lock(g_mutex);
  delete g_active;
  g_active = new Sources(s);
}

const Sources& active_sources() {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_active) {
    std::string said;
    g_active = new Sources(load_sources("", false, &said));
  }
  return *g_active;
}

void learn_keys(const std::vector<std::string>& keys) {
  std::lock_guard<std::mutex> lock(g_mutex);
  for (std::string k : keys)
    if (is_key_hex(&k) && g_learned.size() < static_cast<size_t>(kMaxKeys) &&
        std::find(g_learned.begin(), g_learned.end(), k) == g_learned.end())
      g_learned.push_back(k);
}

std::vector<std::string> signing_keys() {
  std::vector<std::string> out = active_sources().keys;
  std::lock_guard<std::mutex> lock(g_mutex);
  for (const std::string& k : g_learned)
    if (std::find(out.begin(), out.end(), k) == out.end()) out.push_back(k);
  return out;
}

bool takes_unsigned_sums(const std::string& list_url) {
  for (const std::string& p : active_sources().unsigned_sums)
    if (list_url.rfind(p, 0) == 0) return true;
  return false;
}

}  // namespace upgrades::update
