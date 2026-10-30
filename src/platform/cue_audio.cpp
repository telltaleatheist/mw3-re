#include "platform/cue_audio.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace platform {
namespace {

constexpr int64_t kSector = 2352;  // a raw CD frame: 588 stereo 16-bit samples

// mm:ss:ff (75 frames a second) as frames; -1 when malformed.
int64_t msf(const std::string& s) {
  int m = 0, sec = 0, f = 0;
  char a = 0, b = 0;
  std::istringstream in(s);
  if (!(in >> m >> a >> sec >> b >> f) || a != ':' || b != ':' || sec > 59 || f > 74) return -1;
  return (static_cast<int64_t>(m) * 60 + sec) * 75 + f;
}

void put32(std::ofstream& o, uint32_t v) {
  const char b[4] = {static_cast<char>(v), static_cast<char>(v >> 8), static_cast<char>(v >> 16), static_cast<char>(v >> 24)};
  o.write(b, 4);
}
void put16(std::ofstream& o, uint16_t v) {
  const char b[2] = {static_cast<char>(v), static_cast<char>(v >> 8)};
  o.write(b, 2);
}

}  // namespace

bool read_cue(const std::string& cue_path, std::vector<CueTrack>* tracks, std::string* err) {
  tracks->clear();
  std::ifstream in(cue_path);
  if (!in) return *err = "cannot open " + cue_path, false;
  std::string line, file;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream ls(line);
    std::string word;
    if (!(ls >> word)) continue;
    for (char& c : word) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (word == "FILE") {
      std::string rest;
      std::getline(ls, rest);
      const size_t q = rest.find('"');
      if (q != std::string::npos) {
        const size_t e = rest.find('"', q + 1);
        file = rest.substr(q + 1, e == std::string::npos ? std::string::npos : e - q - 1);
      } else {
        std::istringstream rs(rest);
        rs >> file;
      }
    } else if (word == "TRACK") {
      CueTrack t;
      std::string type;
      ls >> t.number >> type;
      for (char& c : type) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      t.audio = type == "AUDIO";
      t.file = file;
      tracks->push_back(t);
    } else if (word == "INDEX" && !tracks->empty()) {
      int n = -1;
      std::string at;
      ls >> n >> at;
      const int64_t f = msf(at);
      if (f < 0) return *err = "bad INDEX in " + line, false;
      if (n == 0) tracks->back().index0 = f;
      if (n == 1) tracks->back().index1 = f;
    }
  }
  if (tracks->empty()) return *err = "no tracks in " + cue_path, false;
  for (const CueTrack& t : *tracks)
    if (t.index1 < 0 || t.file.empty()) return *err = "track " + std::to_string(t.number) + " has no INDEX 01 or FILE", false;
  return true;
}

int32_t write_cue_audio(const std::string& cue_path, const std::string& out_dir,
                        const std::function<void(int32_t, uint64_t)>& done, std::string* err) {
  namespace fs = std::filesystem;
  std::vector<CueTrack> tracks;
  if (!read_cue(cue_path, &tracks, err)) return -1;
  const fs::path dir = fs::path(cue_path).parent_path();
  std::error_code ec;
  fs::create_directories(out_dir, ec);
  int32_t written = 0;
  for (size_t i = 0; i < tracks.size(); ++i) {
    const CueTrack& t = tracks[i];
    if (!t.audio) continue;
    const fs::path bin = dir / t.file;
    const auto size = static_cast<int64_t>(fs::file_size(bin, ec));
    if (ec) {
      *err = "cannot read " + bin.string();
      continue;
    }
    int64_t start = t.index1 * kSector, end = size;
    if (i + 1 < tracks.size() && tracks[i + 1].file == t.file) {  // the next track in the same file
      const CueTrack& n = tracks[i + 1];
      end = (n.index0 >= 0 ? n.index0 : n.index1) * kSector;
    }
    if (end > size) end = size;
    if (start >= end) continue;
    std::ifstream src(bin, std::ios::binary);
    src.seekg(start);
    char name[16];
    std::snprintf(name, sizeof name, "track%02d.wav", t.number);
    std::ofstream o(fs::path(out_dir) / name, std::ios::binary);
    const auto bytes = static_cast<uint32_t>(end - start);
    o.write("RIFF", 4), put32(o, 36 + bytes), o.write("WAVE", 4);
    o.write("fmt ", 4), put32(o, 16), put16(o, 1), put16(o, 2), put32(o, 44100), put32(o, 44100 * 4), put16(o, 4),
        put16(o, 16);
    o.write("data", 4), put32(o, bytes);
    std::vector<char> buf(1 << 20);
    for (int64_t left = end - start; left > 0;) {
      const auto n = static_cast<std::streamsize>(left < static_cast<int64_t>(buf.size()) ? left : static_cast<int64_t>(buf.size()));
      src.read(buf.data(), n);
      o.write(buf.data(), src.gcount());
      if (src.gcount() < n) break;
      left -= n;
    }
    if (!o) {
      *err = "cannot write " + (fs::path(out_dir) / name).string();
      continue;
    }
    ++written;
    if (done) done(t.number, bytes);
  }
  return written;
}

}  // namespace platform
