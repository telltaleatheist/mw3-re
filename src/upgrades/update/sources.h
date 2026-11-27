#pragma once
// Where the launcher looks, as data (2026-11-26, docs/launcher.md, Where the launcher looks): assets/launcher/
// sources.json, shipped beside the badges, names the catalog's addresses in order, the public keys that sign the
// catalog and the releases' SHA256SUMS, and the releases lists whose unsigned SHA256SUMS are still taken (GitHub's,
// so the releases from before signing keep installing). The build carries the same file as a last resort (CMake writes
// it in from that file; never in the code by hand), used only when the shipped one is missing or unreadable. Every
// other address comes from the catalog (its homes and catalog_sources).
#include <string>
#include <vector>

namespace upgrades::update {

struct Sources {
  std::vector<std::string> catalog;        // the catalog's addresses, asked in this order
  std::vector<std::string> keys;           // Ed25519 public keys, 64 hex digits each
  std::vector<std::string> unsigned_sums;  // releases-list address prefixes whose unsigned SHA256SUMS are taken
};
constexpr int kMaxSourceAddresses = 6, kMaxKeys = 8;

// sources.json read and checked: "sources": 1, every address https:// with a plain host (file:/// as well when
// `allow_file`, for checks), every key 64 hex digits (lower-cased), at most kMaxSourceAddresses addresses and
// kMaxKeys keys. False with `why` when not.
bool read_sources(const std::string& json, bool allow_file, Sources* out, std::string* why);
const std::string& builtin_sources_text();  // sources.json as built in
// The shipped file at `path` when it reads, else the built-in copy; `said` the log line saying which (and why).
Sources load_sources(const std::string& path, bool allow_file, std::string* said);

// The sources in use this run (the built-in copy until use_sources), and the keys learned from signed catalogs'
// next_keys (docs/launcher.md, Signed data): signing_keys() is both, deduplicated, the shipped ones first, less
// every revoked one (a signed catalog's revoked_keys). A revoked key is dropped from the learned ones and never
// learned again. learned_keys()/revoked_keys(): what <cache>/keys.txt keeps.
void use_sources(const Sources& s);
const Sources& active_sources();
void learn_keys(const std::vector<std::string>& keys);
void revoke_keys(const std::vector<std::string>& keys);
std::vector<std::string> signing_keys();
std::vector<std::string> learned_keys();
std::vector<std::string> revoked_keys();
void forget_keys();  // learned and revoked both cleared (tests)
// A releases list (its address) whose unsigned SHA256SUMS is taken.
bool takes_unsigned_sums(const std::string& list_url);

// "https://" and a plain host (letters, digits, '-', '.', a dot, no port or user), then (`path`) a path of printable
// characters without spaces, '\\' or ".."; without `path`, nothing after the host but one '/' (taken off).
bool https_address(std::string* url, bool path);
bool is_key_hex(std::string* key);  // 64 hex digits, lower-cased in place

}  // namespace upgrades::update
