// The port's side of test_zsave.py: a plan of zSave calls run on a real
// folder as the write dir, logged as the test logs the original's.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "gamez/zsave/zsave.h"
#include "gamez/zutil/zutl_path.h"

namespace {

const char* const kNames[] = {"Anim", "RunningAnim", "AnimActivation", "GWWorld", "Weapons", "zDEClient", "anim", "W"};
constexpr int kCount = sizeof kNames / sizeof kNames[0];

struct Chunk {
  std::string sub, data;
};
struct Fake {
  std::vector<Chunk> chunks;
  bool stops = false;
};
Fake g_fakes[kCount];
std::string g_log;

std::string unhex(const std::string& h) {
  std::string out;
  if (h == "-") return out;
  for (size_t i = 0; i + 1 < h.size(); i += 2) out += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
  return out;
}
std::string hex(const void* p, size_t n) {
  static const char* d = "0123456789abcdef";
  std::string out;
  const auto* b = static_cast<const uint8_t*>(p);
  for (size_t i = 0; i < n; ++i) out += d[b[i] >> 4], out += d[b[i] & 15];
  return out.empty() ? "-" : out;
}

void fake_save(zsave::Context& ctx, void* user) {
  for (const Chunk& c : g_fakes[reinterpret_cast<intptr_t>(user)].chunks)
    zsave::write(ctx, c.sub.c_str(), c.data.data(), static_cast<uint32_t>(c.data.size()));
}
void fake_load(zsave::Context& ctx, const char* sub, const void* data, uint32_t size, void* user) {
  const intptr_t k = reinterpret_cast<intptr_t>(user);
  g_log += "load " + std::to_string(k) + " " + hex(sub, std::strlen(sub)) + " " + std::to_string(size) + " " +
           hex(data, size) + "\n";
  if (g_fakes[k].stops) ctx.manager->stop = 1;
}

}  // namespace

extern "C" int probe_zsave_run(const char* plan, const char* dir, char* out, int out_len) {
  zutl::mounts().read = {dir};
  zutl::mounts().write = dir;
  for (Fake& f : g_fakes) f = Fake{};
  g_log.clear();
  std::istringstream lines(plan);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream w(line);
    std::string op;
    w >> op;
    if (op == "I") {
      zsave::init();
    } else if (op == "X") {
      zsave::shutdown();
    } else if (op == "R") {
      int k = 0, prio = 0;
      w >> k >> prio;
      zsave::add(kNames[k], fake_save, fake_load, prio, reinterpret_cast<void*>(static_cast<intptr_t>(k)));
    } else if (op == "C") {
      int k = 0;
      std::string sub, data;
      w >> k >> sub >> data;
      g_fakes[k].chunks.push_back({unhex(sub), unhex(data)});
    } else if (op == "Z") {
      int k = 0;
      w >> k;
      g_fakes[k].chunks.clear();
    } else if (op == "P") {
      int k = 0;
      w >> k;
      g_fakes[k].stops = true;
    } else if (op == "S" || op == "L") {
      std::string path;
      int kind = 0;
      w >> path >> kind;
      const bool ok = op == "S" ? zsave::save(unhex(path).c_str(), kind) : zsave::load(unhex(path).c_str(), kind);
      g_log += op + " " + (ok ? "1" : "0") + "\n";
    } else if (op == "K") {
      std::string path;
      w >> path;
      g_log += "K " + std::to_string(zsave::kind_of(unhex(path).c_str())) + "\n";
    } else if (op == "U" || op == "B") {
      // The file damaged in place: cut to n bytes, or bytes written at pos.
      std::string path;
      w >> path;
      const std::string host = zutl::write_path(unhex(path).c_str());
      if (!std::filesystem::exists(host)) continue;
      if (op == "U") {
        size_t n = 0;
        w >> n;
        if (n < std::filesystem::file_size(host)) std::filesystem::resize_file(host, n);
      } else {
        size_t pos = 0;
        std::string data;
        w >> pos >> data;
        const std::string bytes = unhex(data);
        if (std::FILE* f = std::fopen(host.c_str(), "r+b")) {
          std::fseek(f, 0, SEEK_END);
          const size_t size = static_cast<size_t>(std::ftell(f));
          for (size_t i = size; i < pos; ++i) std::fputc(0, f);
          std::fseek(f, static_cast<long>(pos), SEEK_SET);
          std::fwrite(bytes.data(), 1, bytes.size(), f);
          std::fclose(f);
        }
      }
    }
  }
  if (zsave::manager()) zsave::shutdown();
  const int n = static_cast<int>(g_log.size());
  if (n < out_len) std::memcpy(out, g_log.c_str(), static_cast<size_t>(n) + 1);
  return n;
}
