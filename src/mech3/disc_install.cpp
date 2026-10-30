#include "mech3/disc_install.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <system_error>

#include "gamez/zutil/zutl_zar.h"
#include "mech3/data_version.h"
#include "mech3/patch_import.h"
#include "platform/cue_audio.h"

namespace mech3 {
namespace fs = std::filesystem;
namespace {

constexpr uint32_t kUser = 2048;  // an ISO 9660 sector's user data
constexpr const char* kMarker = "mw3-install.txt";
constexpr const char* kExe10 = "1c6419fcdbb0503d2dee006861477fc7354039e6ce95c431b5ac5e4995b465bc";  // assets.md

uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
  return s;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
  return s;
}
bool ends_with(const std::string& s, const std::string& e) {
  return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0;
}

bool write_file(const fs::path& p, const uint8_t* data, size_t n) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::FILE* f = std::fopen(p.string().c_str(), "wb");
  if (!f) return false;
  const bool ok = std::fwrite(data, 1, n, f) == n;
  return std::fclose(f) == 0 && ok;
}

bool read_all(const fs::path& p, std::vector<uint8_t>* out) {
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return false;
  out->clear();
  uint8_t buf[1 << 16];
  size_t got;
  while ((got = std::fread(buf, 1, sizeof buf, f)) > 0) out->insert(out->end(), buf, buf + got);
  std::fclose(f);
  return true;
}

}  // namespace

Disc::~Disc() {
  if (image_) std::fclose(image_);
}

std::string Disc::kind() const {
  if (!root_.empty()) return "folder";
  return sector_ == kUser ? "ISO image" : "raw image (" + std::to_string(sector_) + "-byte sectors)";
}

const Disc::File* Disc::find(const std::string& path) const {
  const std::string p = lower(path);
  for (const File& f : files_)
    if (lower(f.path) == p) return &f;
  return nullptr;
}

bool Disc::read_sectors(uint64_t sector, uint32_t in, size_t n, uint8_t* out) {
  while (n) {
    const size_t take = std::min<size_t>(n, kUser - in);
    const int64_t at = start_ + static_cast<int64_t>(sector) * sector_ + user_ + in;
    if (std::fseek(image_, static_cast<long>(at), SEEK_SET) != 0 || std::fread(out, 1, take, image_) != take) return false;
    out += take, n -= take, ++sector, in = 0;
  }
  return true;
}

bool Disc::read(const File& f, uint64_t off, size_t n, uint8_t* out) {
  if (off + n > f.size) return false;
  if (!root_.empty()) {
    std::FILE* h = std::fopen((fs::path(root_) / f.path).string().c_str(), "rb");
    const bool ok = h && std::fseek(h, static_cast<long>(off), SEEK_SET) == 0 && std::fread(out, 1, n, h) == n;
    if (h) std::fclose(h);
    return ok;
  }
  return read_sectors(f.at + off / kUser, static_cast<uint32_t>(off % kUser), n, out);
}

// An ISO 9660 directory's records (ECMA-119 9.1): a record never crosses a
// sector, a zero length byte ends the sector's records; "\0" and "\1" are
// the directory itself and its parent, bit 1 of the flags a directory.
bool Disc::read_image_dir(uint64_t sector, uint64_t size, const std::string& prefix, int depth) {
  if (depth > 8 || size > (16u << 20) || files_.size() > 100000) return false;
  std::vector<uint8_t> d(static_cast<size_t>(size));
  if (!read_sectors(sector, 0, d.size(), d.data())) return false;
  for (size_t i = 0; i < d.size();) {
    const uint8_t len = d[i];
    if (len == 0) {
      i = (i / kUser + 1) * kUser;
      continue;
    }
    if (len < 34 || i + len > d.size()) return false;
    const uint8_t* r = &d[i];
    const uint8_t nlen = r[32];
    if (33u + nlen > len) return false;
    std::string name(reinterpret_cast<const char*>(r + 33), nlen);
    i += len;
    if (nlen == 1 && (name[0] == 0 || name[0] == 1)) continue;
    if (const size_t semi = name.find(';'); semi != std::string::npos) name.resize(semi);
    if (!name.empty() && name.back() == '.') name.pop_back();
    const uint64_t at = le32(r + 2), bytes = le32(r + 10);
    const std::string path = prefix.empty() ? name : prefix + "/" + name;
    if (r[25] & 2) {
      if (!read_image_dir(at, bytes, path, depth + 1)) return false;
    } else {
      files_.push_back({path, bytes, at});
    }
  }
  return true;
}

bool Disc::open(const std::string& path, std::string* error) {
  std::error_code ec;
  files_.clear();
  if (fs::is_directory(path, ec)) {
    root_ = path;
    int seen = 0;
    for (auto it = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator() && seen < 100000; it.increment(ec), ++seen) {
      if (ec) break;
      if (it.depth() > 8) it.disable_recursion_pending();
      if (!it->is_regular_file(ec)) continue;
      files_.push_back({fs::relative(it->path(), path, ec).generic_string(), static_cast<uint64_t>(it->file_size(ec)), 0});
    }
    return true;
  }
  std::string image = path;
  int64_t start = -1;
  if (ends_with(lower(path), ".cue")) {
    // The first data track (Redump's discs: track 1, MODE1/2352).
    std::vector<platform::CueTrack> tracks;
    if (!platform::read_cue(path, &tracks, error)) return false;
    auto t = std::find_if(tracks.begin(), tracks.end(), [](const platform::CueTrack& c) { return !c.audio; });
    if (t == tracks.end()) return *error = path + ": no data track", false;
    const std::string mode = upper(t->mode);
    if (mode == "MODE1/2352") sector_ = 2352, user_ = 16;
    else if (mode == "MODE2/2352") sector_ = 2352, user_ = 24;  // XA form 1: the subheader after the header
    else if (mode == "MODE1/2048") sector_ = kUser, user_ = 0;
    else return *error = path + ": data track " + t->mode + " (MODE1/2352, MODE2/2352 or MODE1/2048 read)", false;
    image = (fs::path(path).parent_path() / t->file).string();
    start = t->index1 * sector_;
    cue = path;
  }
  image_ = std::fopen(image.c_str(), "rb");
  if (!image_) return *error = "cannot open " + image, false;
  if (start < 0) {
    // A bare image: raw sectors start with the 12-byte sync pattern.
    uint8_t h[16] = {};
    const bool raw = std::fread(h, 1, 16, image_) == 16 && h[0] == 0 && h[11] == 0 &&
                     std::all_of(h + 1, h + 11, [](uint8_t b) { return b == 0xff; });
    sector_ = raw ? 2352 : kUser;
    user_ = raw ? (h[15] == 2 ? 24 : 16) : 0;
    start = 0;
  }
  start_ = start;
  // The primary volume descriptor at sector 16: type 1, "CD001"; its root
  // directory's record at +156.
  uint8_t pvd[kUser];
  if (!read_sectors(16, 0, kUser, pvd) || pvd[0] != 1 || std::memcmp(pvd + 1, "CD001", 5) != 0)
    return *error = image + ": no ISO 9660 file system (not a data disc image?)", false;
  if (!read_image_dir(le32(pvd + 156 + 2), le32(pvd + 156 + 10), "", 0))
    return *error = image + ": a damaged directory", false;
  return true;
}

DiscInstall install_from_disc(const std::string& source, const std::string& out_dir, const std::string& patch,
                              const std::string& music_dir, const std::function<void(const std::string&)>& progress) {
  DiscInstall r;
  auto say = [&](const std::string& s) {
    r.log.push_back(s);
    if (progress) progress(s);
  };
  auto fail = [&](const std::string& why) {
    r.error = why;
    return r;
  };
  Disc disc;
  std::string error;
  if (!disc.open(source, &error)) return fail(error);
  say(source + ": " + disc.kind() + ", " + std::to_string(disc.files().size()) + " files");

  // DATA1.HDR and its DATA1.CAB, at the root or the shallowest folder.
  const Disc::File* hdr = nullptr;
  for (const Disc::File& f : disc.files())
    if (lower(fs::path(f.path).filename().string()) == "data1.hdr" &&
        (!hdr || std::count(f.path.begin(), f.path.end(), '/') < std::count(hdr->path.begin(), hdr->path.end(), '/')))
      hdr = &f;
  if (!hdr) return fail(source + ": no DATA1.HDR (not the MechWarrior 3 disc?)");
  const std::string dir = fs::path(hdr->path).parent_path().generic_string();
  const Disc::File* cab = disc.find(dir.empty() ? "DATA1.CAB" : dir + "/DATA1.CAB");
  if (!cab) return fail(source + ": " + hdr->path + " without its DATA1.CAB");
  std::vector<uint8_t> hdr_bytes(static_cast<size_t>(hdr->size));
  if (!disc.read(*hdr, 0, hdr_bytes.size(), hdr_bytes.data())) return fail(source + ": cannot read " + hdr->path);

  const fs::path out(out_dir), tmp(out_dir + ".new");
  std::error_code ec;
  if (fs::exists(out, ec) && !fs::exists(out / kMarker, ec) && !fs::is_empty(out, ec))
    return fail(out_dir + " is not empty and not an earlier disc install: name another folder");
  fs::remove_all(tmp, ec);

  // Every file group into one folder; the same path twice must be the
  // same bytes (MW3's disc has none twice).
  std::map<std::string, std::pair<uint64_t, uint32_t>> seen;  // lower-cased path: size, CRC
  int k = 0;
  say("reading " + cab->path + " (" + std::to_string(cab->size >> 20) + " MB)");
  const bool read_ok = each_installshield(
      hdr_bytes, cab->size, [&](uint64_t at, size_t n, uint8_t* to) { return disc.read(*cab, at, n, to); },
      [&](PatchEntry&& e) {
        const uint32_t crc = zutl_zar::crc32(0, e.data.data(), e.data.size());
        auto [it, fresh] = seen.emplace(lower(e.path), std::make_pair(static_cast<uint64_t>(e.data.size()), crc));
        if (!fresh) {
          if (it->second.first == e.data.size() && it->second.second == crc) return true;
          error = e.path + " is in two file groups with different contents";
          return false;
        }
        if (!write_file(tmp / e.path, e.data.data(), e.data.size())) {
          error = "cannot write " + (tmp / e.path).string();
          return false;
        }
        ++r.files, r.bytes += e.data.size();
        if (++k % 50 == 0) say(std::to_string(k) + " files");
        return true;
      },
      &error);
  if (!read_ok) {
    fs::remove_all(tmp, ec);
    return fail(source + ": " + error);
  }
  say(cab->path + ": " + std::to_string(r.files) + " files, " + std::to_string(r.bytes >> 20) + " MB");

  // VIDEO\ as video/, its file names as the disc has them.
  std::vector<uint8_t> buf(1 << 20);
  for (const Disc::File& f : disc.files()) {
    const std::string p = lower(f.path);
    if (p.rfind("video/", 0) != 0 || p.find('/', 6) != std::string::npos) continue;
    std::FILE* o = nullptr;
    const fs::path to = tmp / "video" / fs::path(f.path).filename();
    fs::create_directories(to.parent_path(), ec);
    o = std::fopen(to.string().c_str(), "wb");
    bool ok = o != nullptr;
    for (uint64_t at = 0; ok && at < f.size;) {
      const size_t n = static_cast<size_t>(std::min<uint64_t>(buf.size(), f.size - at));
      ok = disc.read(f, at, n, buf.data()) && std::fwrite(buf.data(), 1, n, o) == n;
      at += n;
    }
    if (o && std::fclose(o) != 0) ok = false;
    if (!ok) {
      fs::remove_all(tmp, ec);
      return fail(source + ": cannot copy " + f.path);
    }
    ++r.movies, r.bytes += f.size;
  }
  say("video: " + std::to_string(r.movies) + " files");

  // The data files against 1.0's (assets.md, Data versions).
  int n = 0, of_10 = 0, in_10 = 0;
  const DataFile* files = data_files(&n);
  std::vector<uint8_t> data;
  for (int i = 0; i < n; ++i) {
    const DataFile& d = files[i];
    if (d.size_10) ++in_10;
    if (!read_all(tmp / d.path, &data)) {
      if (d.size_10) r.warnings.push_back(std::string(d.path) + " is not on the disc");
      continue;
    }
    if (d.size_10 && data.size() == d.size_10 && zutl_zar::crc32(0, data.data(), data.size()) == d.crc_10 &&
        sha256_hex(data.data(), data.size()) == d.sha256_10)
      ++of_10;
    else
      r.warnings.push_back(std::string(d.path) + " is not the 1.0 disc's");
  }
  if (read_all(tmp / "Mech3.exe", &data) && sha256_hex(data.data(), data.size()) != kExe10)
    r.warnings.push_back("Mech3.exe is not the 1.0 disc's");
  r.version = of_10 == in_10 && r.warnings.empty() ? "1.0" : "unknown";
  say("data: " + std::to_string(of_10) + " of the 1.0 disc's " + std::to_string(in_10) + " files");
  if (!fs::exists(tmp / "zbd" / "reader.zbd", ec)) {
    fs::remove_all(tmp, ec);
    return fail(source + ": the cabinet has no zbd\\reader.zbd (not the MechWarrior 3 disc?)");
  }

  // The 1.2 patch's 28, over them.
  if (!patch.empty()) {
    const std::string pdir = (tmp / ".patch12").string();
    const PatchImport p = import_patch_12(patch, pdir);
    for (const std::string& l : p.log) say(l);
    if (!p.ok) {
      fs::remove_all(tmp, ec);
      return fail("the 1.2 patch: " + p.error);
    }
    for (auto it = fs::recursive_directory_iterator(pdir, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
      if (ec || !it->is_regular_file(ec)) continue;
      const fs::path to = tmp / fs::relative(it->path(), pdir, ec);
      fs::create_directories(to.parent_path(), ec);
      fs::remove(to, ec);
      fs::rename(it->path(), to, ec);
      if (ec) {
        fs::remove_all(tmp, ec);
        return fail("cannot move the patch's " + it->path().string());
      }
    }
    fs::remove_all(pdir, ec);
    r.version = "1.2";
    r.warnings.clear();  // the 28 are 1.2's now, each checked by the import
    say("the 1.2 patch: " + std::to_string(p.found) + " files");
  }

  {
    const std::string note = "Installed by mw3 --install-from (docs/assets.md, Installing from the disc).\nsource=" +
                             source + "\nversion=" + r.version + "\nfiles=" + std::to_string(r.files) +
                             "\nmovies=" + std::to_string(r.movies) + "\n";
    if (!write_file(tmp / kMarker, reinterpret_cast<const uint8_t*>(note.data()), note.size())) {
      fs::remove_all(tmp, ec);
      return fail("cannot write " + (tmp / kMarker).string());
    }
  }
  fs::remove_all(out, ec);
  fs::create_directories(out.parent_path(), ec);
  fs::rename(tmp, out, ec);
  if (ec) return fail("cannot rename " + tmp.string() + " to " + out_dir + ": " + ec.message());

  // A cue sheet's CD audio as the music (platform/cue_audio.h).
  if (!disc.cue.empty() && !music_dir.empty()) {
    std::string err;
    const int32_t m = platform::write_cue_audio(disc.cue, music_dir, nullptr, &err);
    if (m < 0 || !err.empty()) r.warnings.push_back("music: " + err);
    r.music = m < 0 ? 0 : m;
    say("music: " + std::to_string(r.music) + " tracks into " + music_dir);
  }
  r.ok = true;
  return r;
}

}  // namespace mech3
