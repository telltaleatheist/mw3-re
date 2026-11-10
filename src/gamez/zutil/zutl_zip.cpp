#include "gamez/zutil/zutl_zip.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>

#include "gamez/zutil/zutl_path.h"
#include "miniz/miniz_tinfl.h"

namespace zutl_zip {
namespace {
uint32_t le16(const uint8_t* p) { return p[0] | p[1] << 8; }
uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }
}  // namespace

// Raw deflate into `out` (its size the expected length, or grown when 0).
// A chunk that ends on a flush, not a final block, is whole once every byte
// of it is taken.
bool inflate_raw(const uint8_t* in, size_t n, std::vector<uint8_t>* out, size_t expect) {
  tinfl_decompressor d;
  tinfl_init(&d);
  out->assign(expect ? expect : n * 4 + 1024, 0);
  size_t in_at = 0, out_at = 0;
  for (;;) {
    size_t in_n = n - in_at, out_n = out->size() - out_at;
    const tinfl_status s = tinfl_decompress(&d, in + in_at, &in_n, out->data(), out->data() + out_at, &out_n,
                                            TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    in_at += in_n, out_at += out_n;
    if (s == TINFL_STATUS_DONE || (s == TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS && in_at == n)) break;
    if (s == TINFL_STATUS_HAS_MORE_OUTPUT && !expect) {
      out->resize(out->size() * 2);
      continue;
    }
    return false;
  }
  if (expect && out_at != expect) return false;
  out->resize(out_at);
  return true;
}

// ZIP's CRC-32 (reflected, 0xedb88320), not zutl_zar's.
uint32_t zip_crc(const uint8_t* p, size_t n) {
  static uint32_t t[256];
  if (!t[1])
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
      t[i] = c;
    }
  uint32_t c = 0xffffffffu;
  for (size_t i = 0; i < n; ++i) c = t[(c ^ p[i]) & 0xff] ^ (c >> 8);
  return c ^ 0xffffffffu;
}

Zip::~Zip() {
  if (f) std::fclose(f);
}

bool Zip::open(const std::filesystem::path& p) {
  f = std::fopen(p.string().c_str(), "rb");
  if (!f || std::fseek(f, 0, SEEK_END) != 0) return false;
  const long size = std::ftell(f);
  const long tail = std::min<long>(size, 0x10000 + 22);
  std::vector<uint8_t> t(static_cast<size_t>(tail));
  if (std::fseek(f, size - tail, SEEK_SET) != 0 || std::fread(t.data(), 1, t.size(), f) != t.size()) return false;
  long eocd = -1;
  for (long i = tail - 22; i >= 0; --i)
    if (le32(&t[i]) == 0x06054b50) {
      eocd = i;
      break;
    }
  if (eocd < 0) return false;
  const uint8_t* e = &t[eocd];
  const uint32_t count = le16(e + 10), cd_size = le32(e + 12), cd_off = le32(e + 16);
  const long cd_at = size - tail + eocd - static_cast<long>(cd_size);
  if (cd_at < 0 || cd_off == 0xffffffffu) return false;
  const long shift = cd_at - static_cast<long>(cd_off);  // the stub before an appended ZIP
  std::vector<uint8_t> cd(cd_size);
  if (std::fseek(f, cd_at, SEEK_SET) != 0 || std::fread(cd.data(), 1, cd.size(), f) != cd.size()) return false;
  for (size_t at = 0, k = 0; k < count && at + 46 <= cd.size(); ++k) {
    const uint8_t* c = &cd[at];
    if (le32(c) != 0x02014b50) return false;
    const uint32_t nlen = le16(c + 28), xlen = le16(c + 30), clen = le16(c + 32);
    if (at + 46 + nlen > cd.size()) return false;
    Entry en{std::string(reinterpret_cast<const char*>(c + 46), nlen), le16(c + 10), le32(c + 16), le32(c + 20),
             le32(c + 24), static_cast<uint32_t>(le32(c + 42) + shift)};
    std::replace(en.name.begin(), en.name.end(), '\\', '/');
    if (!en.name.empty() && en.name.back() != '/') entries.push_back(en);
    at += 46 + nlen + xlen + clen;
  }
  return true;
}

bool Zip::read(const Entry& en, std::vector<uint8_t>* out, std::string* error) {
  uint8_t h[30];
  if (std::fseek(f, static_cast<long>(en.local), SEEK_SET) != 0 || std::fread(h, 1, 30, f) != 30 || le32(h) != 0x04034b50) {
    *error = en.name + ": no local header";
    return false;
  }
  std::vector<uint8_t> raw(en.csize);
  if (std::fseek(f, static_cast<long>(en.local + 30 + le16(h + 26) + le16(h + 28)), SEEK_SET) != 0 ||
      std::fread(raw.data(), 1, raw.size(), f) != raw.size()) {
    *error = en.name + ": cut short";
    return false;
  }
  if (en.method == 0) {
    *out = std::move(raw);
  } else if (en.method != 8 || !inflate_raw(raw.data(), raw.size(), out, en.usize)) {
    *error = en.name + (en.method == 8 ? ": does not inflate" : ": compressed by a method other than deflate");
    return false;
  }
  if (out->size() != en.usize || zip_crc(out->data(), out->size()) != en.crc) {
    *error = en.name + ": CRC does not match (a damaged download?)";
    return false;
  }
  return true;
}

ScratchFile::ScratchFile(const void* data, size_t size) {
  static unsigned serial = 0;
  std::error_code ec;
  const std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
  if (ec) return;
  const auto stamp = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
  for (int tries = 0; tries < 8 && !file; ++tries) {
    path = dir / ("mw3-entry-" + std::to_string(stamp) + "-" + std::to_string(serial++) + ".tmp");
    if (std::filesystem::exists(path, ec)) continue;  // never one we did not make
    file = std::fopen(path.string().c_str(), "w+b");
  }
  if (!file) return;
  if (size && std::fwrite(data, 1, size, file) != size) {
    std::fclose(file);
    file = nullptr;
    std::filesystem::remove(path, ec);
    return;
  }
  std::rewind(file);
}

ScratchFile::~ScratchFile() {
  if (!file) return;
  std::fclose(file);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

int mod_json(size_t mod, const std::string& dir, const std::string& stem, const std::string& file, std::string* text,
             std::string* what, std::string* why) {
  text->clear();
  if (const std::string json = zutl::find_in_mod(mod, dir + stem + "/" + file); !json.empty()) {
    *what = json;
    std::FILE* f = std::fopen(json.c_str(), "rb");
    if (!f) return *why = "cannot be opened", -1;
    char buf[1 << 14];
    for (size_t got; (got = std::fread(buf, 1, sizeof buf, f)) > 0;) text->append(buf, got);
    std::fclose(f);
    return 1;
  }
  const std::string zip = zutl::find_in_mod(mod, dir + stem + ".zip");
  if (zip.empty()) return 0;
  Zip z;
  if (!z.open(zip)) return *what = zip, *why = "not a zip the game can read", -1;
  auto same = [](const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
      if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
  };
  for (const Zip::Entry& e : z.entries) {
    if (!same(e.name, file)) continue;
    *what = zip + ":" + e.name;
    std::vector<uint8_t> data;
    if (!z.read(e, &data, why)) return -1;
    text->assign(data.begin(), data.end());
    return 1;
  }
  return 0;
}

}  // namespace zutl_zip
