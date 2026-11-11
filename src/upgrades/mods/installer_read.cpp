#include "upgrades/mods/installer_read.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <map>

#include "mech3/mod_install.h"
#include "miniz/miniz_tinfl.h"
#include "upgrades/mods/bz2_read.h"

namespace mech3 {
namespace {

constexpr uint8_t kSig[6] = {0x77, 0x77, 0x67, 0x54, 0x29, 0x48};  // "wwgT)H", then two more bytes
constexpr uint64_t kMaxBlocks = 256;

struct File {
  std::FILE* f = nullptr;
  uint64_t size = 0;
  explicit File(const std::string& path) {
    f = std::fopen(path.c_str(), "rb");
    if (!f) return;
    if (std::fseek(f, 0, SEEK_END) == 0) {
      const long long n = std::ftell(f);
      size = n > 0 ? static_cast<uint64_t>(n) : 0;
    }
  }
  ~File() {
    if (f) std::fclose(f);
  }
  // `n` bytes at `at`, only when they are all inside the file.
  bool read(uint64_t at, void* to, uint64_t n) const {
    if (!f || at > size || n > size - at) return false;
#if defined(_WIN32)
    if (_fseeki64(f, static_cast<long long>(at), SEEK_SET) != 0) return false;
#else
    if (fseeko(f, static_cast<off_t>(at), SEEK_SET) != 0) return false;
#endif
    return std::fread(to, 1, static_cast<size_t>(n), f) == n;
  }
  bool u16(uint64_t at, uint32_t* v) const {
    uint8_t b[2];
    if (!read(at, b, 2)) return false;
    *v = b[0] | b[1] << 8;
    return true;
  }
  bool u32(uint64_t at, uint32_t* v) const {
    uint8_t b[4];
    if (!read(at, b, 4)) return false;
    *v = b[0] | b[1] << 8 | b[2] << 16 | static_cast<uint32_t>(b[3]) << 24;
    return true;
  }
};

uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }

// Where the program ends: the end of the last section's bytes in the file (0: not such a program).
uint64_t program_end(const File& f) {
  uint8_t mz[2];
  uint32_t pe_at = 0, pe = 0, sections = 0, opt = 0;
  if (!f.read(0, mz, 2) || mz[0] != 'M' || mz[1] != 'Z' || !f.u32(0x3c, &pe_at)) return 0;
  if (!f.u32(pe_at, &pe) || pe != 0x00004550 || !f.u16(pe_at + 6ull, &sections) || !f.u16(pe_at + 20ull, &opt)) return 0;
  if (sections == 0 || sections > 96) return 0;
  uint64_t end = 0;
  const uint64_t table = pe_at + 24ull + opt;
  for (uint32_t i = 0; i < sections; ++i) {
    uint32_t raw_size = 0, raw_at = 0;
    if (!f.u32(table + i * 40ull + 16, &raw_size) || !f.u32(table + i * 40ull + 20, &raw_at)) return 0;
    end = std::max<uint64_t>(end, uint64_t{raw_at} + raw_size);
  }
  return end <= f.size ? end : 0;
}

bool has_signature(const File& f, uint64_t at) {
  uint8_t s[6];
  return f.read(at, s, 6) && std::memcmp(s, kSig, 6) == 0;
}

// A zlib stream into exactly `size` bytes.
bool inflate_exact(const uint8_t* in, size_t n, uint64_t size, std::vector<uint8_t>* out) {
  out->assign(static_cast<size_t>(size) + 1, 0);  // one spare byte: a stream of nothing still has somewhere to point
  tinfl_decompressor d;
  tinfl_init(&d);
  size_t in_n = n, out_n = static_cast<size_t>(size);
  const tinfl_status s = tinfl_decompress(&d, in, &in_n, out->data(), out->data(), &out_n,
                                          TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  out->resize(static_cast<size_t>(size));
  return s == TINFL_STATUS_DONE && out_n == size;
}

// A block's or a file's bytes from their method byte on: 1 zlib, 2 bzip2 (no installer seen stores one unpacked:
// any other byte is refused); exactly `size` out.
bool unpack(const std::vector<uint8_t>& packed, uint64_t size, std::vector<uint8_t>* out, std::string* why) {
  auto fail = [&](const std::string& w) {
    if (why) *why = w;
    out->clear();
    return false;
  };
  if (packed.empty()) return fail("an entry with no data");
  const uint8_t* p = packed.data() + 1;
  const size_t n = packed.size() - 1;
  switch (packed[0]) {
    case 1:
      if (!inflate_exact(p, n, size, out)) return fail("a zlib entry does not unpack to its declared size");
      return true;
    case 2: {
      std::string w;
      if (!bz2_decompress(p, n, static_cast<size_t>(size), out, &w)) return fail(w);
      if (out->size() != size) return fail("a bzip2 entry does not unpack to its declared size");
      return true;
    }
    default:
      return fail("an entry packed in a way this does not read (type " + std::to_string(packed[0]) + ")");
  }
}

}  // namespace

bool installer_bytes_are_clickteam(const uint8_t* d, size_t n) {
  auto u16 = [&](uint64_t at, uint32_t* v) { return at + 2 <= n ? (*v = d[at] | d[at + 1] << 8, true) : false; };
  auto u32 = [&](uint64_t at, uint32_t* v) { return at + 4 <= n ? (*v = le32(d + at), true) : false; };
  uint32_t pe_at = 0, pe = 0, sections = 0, opt = 0;
  if (n < 0x40 || d[0] != 'M' || d[1] != 'Z' || !u32(0x3c, &pe_at)) return false;
  if (!u32(pe_at, &pe) || pe != 0x00004550 || !u16(pe_at + 6ull, &sections) || !u16(pe_at + 20ull, &opt)) return false;
  if (sections == 0 || sections > 96) return false;
  uint64_t end = 0;
  const uint64_t table = pe_at + 24ull + opt;
  for (uint32_t i = 0; i < sections; ++i) {
    uint32_t raw_size = 0, raw_at = 0;
    if (!u32(table + i * 40ull + 16, &raw_size) || !u32(table + i * 40ull + 20, &raw_at)) return false;
    end = std::max<uint64_t>(end, uint64_t{raw_at} + raw_size);
  }
  return end + 6 <= n && std::memcmp(d + end, kSig, 6) == 0;
}

bool installer_is_clickteam(const std::string& path) {
  const File f(path);
  const uint64_t end = program_end(f);
  return end != 0 && has_signature(f, end);
}

namespace {
constexpr uint32_t kTexts = 0x143e, kFileList = 0x143a, kData = 0x7f7f;
struct Block { uint32_t type = 0, flags = 0; uint64_t at = 0, size = 0; };  // `at`: its content

// A packed block's content: [u32 unpacked][method byte][stream].
bool block_bytes(const File& f, const Block& b, uint64_t max, std::vector<uint8_t>* out, std::string* why) {
  uint32_t unpacked = 0;
  if (b.size < 5 || !f.u32(b.at, &unpacked)) return *why = "cut short", false;
  if (unpacked > max) return *why = "larger than a mod's can be", false;
  std::vector<uint8_t> packed(static_cast<size_t>(b.size - 4));
  if (!f.read(b.at + 4, packed.data(), packed.size())) return *why = "cut short", false;
  return unpack(packed, unpacked, out, why);
}

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// The texts' printable runs of three letters and more, in order (bounded: the block is).
std::vector<std::string> strings_of(const std::vector<uint8_t>& b) {
  std::vector<std::string> out;
  std::string run;
  for (size_t i = 0; i <= b.size() && out.size() < 4096; ++i) {
    const bool printable = i < b.size() && b[i] >= 0x20 && b[i] <= 0x7e;
    if (printable) {
      if (run.size() < 512) run += static_cast<char>(b[i]);
    } else {
      if (run.size() >= 3) out.push_back(run);
      run.clear();
    }
  }
  return out;
}
}  // namespace

Installer installer_open(const std::string& path, const InstallerLimits& limits) {
  Installer r;
  const File f(path);
  const uint64_t start = program_end(f);
  if (!start || !has_signature(f, start)) return r;
  r.recognised = true;
  auto fail = [&](const std::string& w) {
    r.error = w, r.ok = false;
    r.files.clear();
    return r;
  };
  // The blocks after the signature: [u16 type][u16 flags][u32 size][content], to the file's end but for a few bytes.
  std::vector<Block> blocks;
  uint64_t at = start + 6;
  while (f.size - at >= 8) {
    Block b;
    uint32_t size = 0;
    if (blocks.size() >= kMaxBlocks) return fail("it has more blocks than an installer of this kind has");
    if (!f.u16(at, &b.type) || !f.u16(at + 2, &b.flags) || !f.u32(at + 4, &size)) return fail("a block's header does not read");
    if (size > f.size - (at + 8)) return fail("a block's size runs past the file's end");
    b.at = at + 8, b.size = size;
    blocks.push_back(b);
    at += 8 + uint64_t{size};
  }
  const Block *list_block = nullptr, *data_block = nullptr, *texts = nullptr;
  for (const Block& b : blocks) {
    if (b.type == kFileList && !list_block) list_block = &b;
    if (b.type == kData && !data_block) data_block = &b;
    if (b.type == kTexts && !texts) texts = &b;
  }
  if (!list_block || !data_block) return fail("it has no file list or no files' data");
  if (data_block->size < 4) return fail("its files' data is cut short");
  // The files' data: after the block's first word, to the file's end (the block's size leaves that word out, so
  // the last file ends four bytes past it; the file's end is the bound).
  const uint64_t data_at = data_block->at + 4, data_size = f.size - data_at;
  std::string why;
  if (texts) {  // its title and where it would install: read when they read, never needed
    std::vector<uint8_t> t;
    std::string w;
    if (block_bytes(f, *texts, limits.max_list_bytes, &t, &w)) {
      const std::vector<std::string> strs = strings_of(t);
      for (const std::string& s : strs)
        if (s.size() > 3 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' && s[2] == '\\') {
          r.default_folder = s.substr(3);
          std::replace(r.default_folder.begin(), r.default_folder.end(), '\\', '/');
          while (!r.default_folder.empty() && r.default_folder.back() == '/') r.default_folder.pop_back();
          break;
        }
      // The title: the first line of six letters or more before the default folder's (the newest maker's texts
      // open with a few stray bytes; the oldest's have no title).
      for (const std::string& t0 : strs) {
        if (t0.size() > 2 && t0[1] == ':') break;
        if (t0.size() >= 6) {
          r.title = t0;
          break;
        }
      }
    }
  }
  std::vector<uint8_t> list;
  if (!block_bytes(f, *list_block, limits.max_list_bytes, &list, &why)) return fail("its file list does not read (" + why + ")");
  if (list.size() < 4) return fail("its file list is empty");
  const uint32_t count = le32(list.data());
  if (count == 0 || count > limits.max_files) return fail("its file list names " + std::to_string(count) + " files");
  // The records, in the maker's two layouts: from 2,0,0,28 on the size is at +0x18 and the name at +0x40; in
  // 2,0,0,16 each is four bytes earlier. The one that holds for every entry is taken: each file's data inside
  // the installer and starting with a method byte (1 zlib, 2 bzip2). Neither: refused.
  auto read_list = [&](uint32_t size_at, uint32_t name_at, std::vector<InstallerFile>* files, int* others, std::string* err) {
    files->clear(), *others = 0;
    size_t p = 4;
    std::map<std::string, std::string> seen;  // lower-case name -> as written
    for (uint32_t i = 0; i < count; ++i) {
      if (list.size() - p < 8) return *err = "its file list is cut short", false;
      const uint8_t* e = list.data() + p;
      const uint32_t len = e[0] | e[1] << 8;
      if (len < 8 || len > list.size() - p) return *err = "an entry of its file list runs past the list", false;
      p += len;
      if (le32(e + 4) != 0) {  // not a file (2: its uninstaller): nothing to place
        ++*others;
        continue;
      }
      if (len < name_at + 1) return *err = "a file's entry is too short to name it", false;
      const char* name = reinterpret_cast<const char*>(e + name_at);
      const void* nul = std::memchr(name, 0, len - name_at);
      if (!nul) return *err = "an entry's name has no end", false;
      const std::string raw(name, static_cast<const char*>(nul));
      if (raw.empty() || raw.back() == '\\') {  // a folder
        ++*others;
        continue;
      }
      InstallerFile file;
      file.size = le32(e + size_at);
      const uint64_t offset = le32(e + size_at + 4);
      file.packed = le32(e + size_at + 8);
      if (file.packed < 2 || offset > data_size || file.packed > data_size - offset)
        return *err = "a file's data runs past the installer's end (" + raw.substr(0, 80) + ")", false;
      if (file.size > limits.max_file_bytes) return *err = "a file in it is larger than a mod's can be", false;
      file.data_at = data_at + offset;
      uint8_t method = 0;
      if (!f.read(file.data_at, &method, 1) || (method != 1 && method != 2))
        return *err = "a file's data does not start as packed data (" + raw.substr(0, 80) + ")", false;
      // Nothing outside the folder: no absolute path, drive, "..", control characters, or a name Windows cannot hold.
      std::string clean;
      if (!mod_entry_path_ok(raw, &clean)) return *err = "a file's path is not one inside a folder (" + raw.substr(0, 80) + ")", false;
      file.name = clean;
      // Two names that differ only in case would land on one file: refused, not last-wins.
      if (!seen.emplace(lower(clean), clean).second) return *err = "it names a file twice (" + clean + ")", false;
      files->push_back(file);
    }
    return true;
  };
  // Which layout: the one whose sizes and offsets hold for every file (names aside).
  auto fits = [&](uint32_t size_at, uint32_t name_at, std::string* err) {
    size_t p = 4;
    int files = 0;
    for (uint32_t i = 0; i < count; ++i) {
      if (list.size() - p < 8) return *err = "its file list is cut short", false;
      const uint8_t* e = list.data() + p;
      const uint32_t len = e[0] | e[1] << 8;
      if (len < 8 || len > list.size() - p) return *err = "an entry of its file list runs past the list", false;
      p += len;
      if (le32(e + 4) != 0) continue;
      if (len < name_at + 1) return *err = "a file's entry is too short", false;
      {  // a folder (its name ends in a backslash): no data to check
        const char* name = reinterpret_cast<const char*>(e + name_at);
        const void* nul = std::memchr(name, 0, len - name_at);
        if (!nul) return *err = "an entry's name has no end", false;
        const size_t n = static_cast<size_t>(static_cast<const char*>(nul) - name);
        if (n == 0 || name[n - 1] == '\\') continue;
      }
      const uint64_t offset = le32(e + size_at + 4), packed = le32(e + size_at + 8);
      if (packed < 2 || offset > data_size || packed > data_size - offset) return *err = "a file's data runs past the installer's end", false;
      uint8_t method = 0;
      if (!f.read(data_at + offset, &method, 1) || (method != 1 && method != 2))
        return *err = "a file's data does not start as packed data", false;
      ++files;
    }
    if (!files) return *err = "it holds no files", false;
    return true;
  };
  std::string err_new, err_old, err;
  const bool is_new = fits(0x18, 0x40, &err_new), is_old = !is_new && fits(0x14, 0x38, &err_old);
  if (!is_new && !is_old) return fail(err_new);
  if (!read_list(is_new ? 0x18 : 0x14, is_new ? 0x40 : 0x38, &r.files, &r.others, &err)) return fail(err);
  if (r.files.empty()) return fail("it holds no files");
  r.ok = true;
  return r;
}

bool installer_map_to_game(Installer* in, std::string* why) {
  auto parts = [](const std::string& s) {
    std::vector<std::string> v;
    size_t a = 0;
    while (a <= s.size()) {
      const size_t b = s.find('/', a);
      v.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
      if (b == std::string::npos) break;
      a = b + 1;
    }
    return v;
  };
  // Where zbd sits in the names: what comes before it is the game's folder as the installer names it. Every name
  // with a zbd folder must agree.
  bool found = false;
  std::string root;
  for (const InstallerFile& f : in->files) {
    const std::vector<std::string> v = parts(f.name);
    for (size_t k = 0; k + 1 < v.size(); ++k)
      if (lower(v[k]) == "zbd") {
        std::string before;
        for (size_t j = 0; j < k; ++j) before += v[j] + "/";
        if (found && lower(before) != lower(root)) {
          if (why) *why = "its files name the game's zbd folder in two places (" + root + "zbd and " + before + "zbd)";
          return false;
        }
        found = true, root = before;
        break;
      }
  }
  std::string add;  // put before each name instead, when the default folder is inside the game's
  if (!found) {
    // No zbd in the names: the default folder's tail says where they land (…/MechWarrior3/zbd: under zbd).
    const std::vector<std::string> v = parts(in->default_folder);
    for (size_t k = 0; k < v.size(); ++k)
      if (lower(v[k]) == "zbd") {
        for (size_t j = k; j < v.size(); ++j) add += v[j] + "/";
        found = true;
        break;
      }
  }
  if (!found) {
    if (why) *why = "it does not say where its files go in the game's folder (no zbd folder in its names or its default folder)";
    return false;
  }
  in->game_root = root;
  for (InstallerFile& f : in->files) {
    if (!add.empty()) f.path = add + f.name;
    else if (root.empty()) f.path = f.name;
    else if (f.name.size() > root.size() && lower(f.name.substr(0, root.size())) == lower(root)) f.path = f.name.substr(root.size());
    else f.path.clear();  // beside the game's folder: not the game's
  }
  return true;
}

bool installer_extract(const std::string& path, const InstallerFile& file, std::vector<uint8_t>* out, std::string* why) {
  out->clear();
  const File f(path);
  if (file.packed == 0 || file.packed > f.size || file.data_at > f.size - file.packed) {
    if (why) *why = "the file's data is not inside the installer";
    return false;
  }
  std::vector<uint8_t> packed(static_cast<size_t>(file.packed));
  if (!f.read(file.data_at, packed.data(), packed.size())) {
    if (why) *why = "the installer cannot be read";
    return false;
  }
  return unpack(packed, file.size, out, why);
}

}  // namespace mech3
