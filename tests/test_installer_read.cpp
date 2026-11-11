// IMPORT from a mod's installer program (docs/modding.md; upgrades/mods/installer_read.h, bz2_read.h).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <iterator>
#include <functional>
#include <fstream>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "pm_side.h"
#include "message_dll.h"
#include "upgrades/mods/mod_report.h"
#include "mech3/mod_install.h"
#include "upgrades/mods/bz2_read.h"
#include "upgrades/mods/installer_read.h"
#include "upgrades/mods/import_folder.h"
#include "mech3/mods.h"
#include <chrono>

namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

Bytes bytes_of(const std::string& s) { return Bytes(s.begin(), s.end()); }
void put32(Bytes& b, uint32_t v) {
  for (int k = 0; k < 4; ++k) b.push_back(static_cast<uint8_t>(v >> (8 * k)));
}
void put16(Bytes& b, uint32_t v) { b.push_back(static_cast<uint8_t>(v)), b.push_back(static_cast<uint8_t>(v >> 8)); }
void set32(Bytes& b, size_t at, uint32_t v) {
  for (int k = 0; k < 4; ++k) b[at + k] = static_cast<uint8_t>(v >> (8 * k));
}
// A zlib stream of stored blocks: valid zlib with no compressor in the tree.
Bytes zlib_stored(const Bytes& d) {
  Bytes z = {0x78, 0x01};
  size_t at = 0;
  do {
    const size_t n = std::min<size_t>(d.size() - at, 65535);
    z.push_back(at + n == d.size() ? 1 : 0);
    put16(z, static_cast<uint32_t>(n)), put16(z, static_cast<uint32_t>(~n & 0xffff));
    z.insert(z.end(), d.begin() + static_cast<long>(at), d.begin() + static_cast<long>(at + n));
    at += n;
  } while (at < d.size());
  uint32_t a = 1, b = 0;
  for (uint8_t c : d) a = (a + c) % 65521, b = (b + a) % 65521;
  for (int k = 3; k >= 0; --k) z.push_back(static_cast<uint8_t>((b << 16 | a) >> (8 * k)));
  return z;
}
// A made-up bzip2 stream of made-up text, 330 bytes unpacked. Made with Python:
//   c = b"A made-up file for the bzip2 path: " + b"mech "*40 + bytes(range(32,127)); bz2.compress(c, 1).hex()
const char kBz2Hex[] =
    "425a6831314159265359b5ce81650000299f807fffffffffffffffffffffff2000954549a0d0686801a64d341a00699031346864d00641881a111468"
    "c434184d032068000000000069a31006d1eb546a10210801345624a2dc397463b78f5f1a71a7a0fd14602106143889269454e3479125474c32c59b56"
    "ee5dbd7f01b84e3f163c9973208a59d3d1a54d5af62ab307afb77240000203dde6b1c4b000100db941c49000102e1ccb23dabd46b162001018676299"
    "25bae70d860f6ccf03a1b9ebd8ff26fdfceff177245385090b5ce81650";
Bytes bz2_fixture() {
  Bytes b;
  for (size_t k = 0; kBz2Hex[k] && kBz2Hex[k + 1]; k += 2) b.push_back(static_cast<uint8_t>(std::stoi(std::string(kBz2Hex + k, 2), nullptr, 16)));
  return b;
}
Bytes bz2_fixture_text() {
  std::string c = "A made-up file for the bzip2 path: ";
  for (int k = 0; k < 40; ++k) c += "mech ";
  for (int k = 32; k < 127; ++k) c += static_cast<char>(k);
  return bytes_of(c);
}

// A made-up installer in the maker's format (docs/installer-format.md), nothing of a real mod in it: a stub
// program of one section, the signature, a texts block, the file list and the files' data.
struct Made {
  struct F {
    std::string name;      // as the installer names it, backslashes
    Bytes data;
    int method = 1;        // 1: zlib (stored blocks); 2: the bzip2 fixture (data must be its text)
    uint32_t kind = 0;     // 2: the uninstaller's entry
    int64_t declared = -1; // the size its entry declares (-1: the data's)
  };
  std::vector<F> files;
  std::string title = "A Made-up Mod", folder = "C:\\MicroProse";
  bool old_layout = false;  // 2,0,0,16's records
  std::vector<size_t> list_at;  // where each entry's record starts in the unpacked list (for the tests' patches)
  Bytes list;                   // the unpacked file list
  size_t list_block_at = 0;     // its block's header in the file

  static Bytes packed_block(const Bytes& d) {  // [u32 unpacked][method 1][zlib]
    Bytes c;
    put32(c, static_cast<uint32_t>(d.size()));
    c.push_back(1);
    const Bytes z = zlib_stored(d);
    c.insert(c.end(), z.begin(), z.end());
    return c;
  }
  static void block(Bytes& out, uint32_t type, uint32_t flags, const Bytes& content, uint32_t size_less = 0) {
    put16(out, type), put16(out, flags), put32(out, static_cast<uint32_t>(content.size()) - size_less);
    out.insert(out.end(), content.begin(), content.end());
  }
  Bytes build() {
    Bytes out(0x200, 0);  // the stub: MZ, PE at 0x80, one section whose bytes end at 0x200
    out[0] = 'M', out[1] = 'Z';
    set32(out, 0x3c, 0x80);
    out[0x80] = 'P', out[0x81] = 'E';
    out[0x86] = 1;                 // sections
    out[0x94] = 0xe0;              // the optional header's size
    set32(out, 0x80 + 24 + 0xe0 + 16, 0x100), set32(out, 0x80 + 24 + 0xe0 + 20, 0x100);  // raw size, raw place
    for (uint8_t c : {0x77, 0x77, 0x67, 0x54, 0x29, 0x48}) out.push_back(c);
    Bytes texts = {1, 2, 0};
    for (const std::string& t : {title, folder}) texts.insert(texts.end(), t.begin(), t.end()), texts.push_back(0);
    block(out, 0x143e, 1, packed_block(texts));
    Bytes data(4, 0);  // the data block's first word
    list.clear(), list_at.clear();
    put32(list, static_cast<uint32_t>(files.size()));
    for (const F& f : files) {
      const uint32_t offset = static_cast<uint32_t>(data.size() - 4);
      Bytes stream = f.method == 2 ? bz2_fixture() : zlib_stored(f.data);
      data.push_back(static_cast<uint8_t>(f.method));
      data.insert(data.end(), stream.begin(), stream.end());
      const uint32_t packed = static_cast<uint32_t>(stream.size() + 1);
      const uint32_t size = f.declared >= 0 ? static_cast<uint32_t>(f.declared) : static_cast<uint32_t>(f.data.size());
      Bytes r;
      const size_t name_at = f.kind != 0 ? 40 : old_layout ? 0x38 : 0x40;
      r.assign(name_at, 0);
      set32(r, 4, f.kind), set32(r, 8, 0xfe008);
      const size_t size_at = old_layout ? 0x14 : 0x18;
      if (f.kind == 0) set32(r, size_at, size), set32(r, size_at + 4, offset), set32(r, size_at + 8, packed);
      r.insert(r.end(), f.name.begin(), f.name.end());
      r.insert(r.end(), 9, 0);
      r[0] = static_cast<uint8_t>(r.size()), r[1] = static_cast<uint8_t>(r.size() >> 8);
      list_at.push_back(list.size());
      list.insert(list.end(), r.begin(), r.end());
    }
    list_block_at = out.size();
    block(out, 0x143a, 1, packed_block(list));
    block(out, 0x7f7f, 0, data, 4);  // as the maker writes it: the size leaves the first word out
    return out;
  }
};
std::string write_file(const std::string& path, const Bytes& b) {
  std::error_code ec;
  fs::create_directories(fs::path(path).parent_path(), ec);
  std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  return path;
}
std::string tmp(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
Made sample() {
  Made m;
  m.files = {{"Uninstal.exe", {}, 1, 2},
             {"Game Folder\\Mech3.exe", bytes_of("a patched game")},
             {"Game Folder\\Mech3Msg.dll", bytes_of("texts")},
             {"Game Folder\\DDraw.dll", bytes_of("dgVoodoo")},
             {"Game Folder\\Readme.txt", bytes_of("about the mod")},
             {"Game Folder\\zbd\\reader.txt", bytes_of("the mod's reader")},
             {"Game Folder\\zbd\\c1\\gamez.txt", bytes_of("abc")},
             {"Game Folder\\zbd\\c1\\New.txt", bz2_fixture_text(), 2},
             {std::string("Game Folder\\") + (TEST_IS_PM ? "zbd\\c1\\readersa1.txt" : "zbd\\c4b\\m5.txt"), bytes_of("a file only this game has")},
             {"Game Folder\\extras\\", {}},
             {"Outside\\other.txt", bytes_of("beside the game's folder")}};
  return m;
}
}  // namespace

// The made-up installer listed and every file unpacked to its bytes; its title and default folder; its names
// laid over a game folder by where zbd sits. Both of the maker's record layouts.
TEST(installer_read_lists_and_extracts) {
  for (const bool old_layout : {false, true}) {
    Made m = sample();
    m.old_layout = old_layout;
    const std::string path = write_file(tmp("installer_read") + "/Made Mod.exe", m.build());
    EXPECT(mech3::installer_is_clickteam(path));
    mech3::Installer in = mech3::installer_open(path);
    EXPECT(in.recognised && in.ok && in.error.empty());
    EXPECT(in.title == "A Made-up Mod" && in.default_folder == "MicroProse");
    EXPECT(in.files.size() == 9 && in.others == 2);  // the uninstaller's entry and the folder's are not files
    size_t k = 0;
    for (const Made::F& f : m.files) {
      if (f.kind != 0 || f.name.back() == '\\') continue;
      EXPECT(k < in.files.size());
      if (k >= in.files.size()) break;
      std::string want = f.name;
      std::replace(want.begin(), want.end(), '\\', '/');
      Bytes out;
      std::string why;
      EXPECT(in.files[k].name == want && in.files[k].size == f.data.size());
      EXPECT(mech3::installer_extract(path, in.files[k], &out, &why) && out == f.data);
      ++k;
    }
    std::string why;
    EXPECT(mech3::installer_map_to_game(&in, &why) && in.game_root == "Game Folder/");
    EXPECT(in.files[4].path == "zbd/reader.txt" && in.files[0].path == "Mech3.exe" && in.files[8].path.empty());
  }
  // Names from the game folder itself, ZBD in capitals; and none with zbd, the default folder inside zbd.
  Made a;
  a.files = {{"ZBD\\c1\\x.txt", bytes_of("x")}, {"notes.txt", bytes_of("n")}};
  const std::string pa = write_file(tmp("installer_read_a") + "/a.exe", a.build());
  mech3::Installer ia = mech3::installer_open(pa);
  std::string why;
  EXPECT(ia.ok && mech3::installer_map_to_game(&ia, &why) && ia.game_root.empty() && ia.files[0].path == "ZBD/c1/x.txt");
  Made b;
  b.folder = "C:\\MicroProse\\MechWarrior3\\zbd";
  b.files = {{"rmechtex.txt", bytes_of("skins")}};
  mech3::Installer ib = mech3::installer_open(write_file(tmp("installer_read_b") + "/b.exe", b.build()));
  EXPECT(ib.ok && mech3::installer_map_to_game(&ib, &why) && ib.files[0].path == "zbd/rmechtex.txt");
  // Not such a program: any other file, and a program with nothing after it.
  EXPECT(!mech3::installer_is_clickteam(write_file(tmp("installer_read_c") + "/plain.exe", bytes_of("MZ not really"))));
  Bytes stub = sample().build();
  stub.resize(0x200);
  const mech3::Installer none = mech3::installer_open(write_file(tmp("installer_read_d") + "/stub.exe", stub));
  EXPECT(!none.recognised && !none.ok && none.error.empty());
}

// A broken or hostile installer is refused, never trusted: every truncation; a size past the end; a path that
// leaves the folder; a stream that unpacks past its declared size; a method byte that is not one; two names
// that differ only in case; a tree that does not say where the game's folder is.
TEST(installer_read_refusals) {
  const std::string dir = tmp("installer_refuse");
  Made m = sample();
  const Bytes whole = m.build();
  // Every truncation: not recognised, or refused, or (cut inside the data) each file exact or refused.
  int refused = 0, plain = 0, partly = 0;
  for (size_t cut = 0; cut < whole.size(); cut += cut < 0x220 ? 37 : 1) {
    const std::string p = write_file(dir + "/cut.exe", Bytes(whole.begin(), whole.begin() + static_cast<long>(cut)));
    const mech3::Installer in = mech3::installer_open(p);
    if (!in.recognised) ++plain;
    else if (!in.ok) {
      ++refused;
      EXPECT(!in.error.empty() && in.files.empty());
    } else {
      ++partly;
    }
    EXPECT(!in.ok);  // a list that reads names data the cut file does not hold: refused whole
  }
  std::printf("  truncations: %d not an installer, %d refused, %d read\n", plain, refused, partly);
  EXPECT(refused > 100 && partly == 0);
  auto open_with = [&](const std::function<void(Made&)>& change) {
    Made x = sample();
    change(x);
    return mech3::installer_open(write_file(dir + "/x.exe", x.build()));
  };
  auto refused_for = [&](const std::function<void(Made&)>& change, const char* word) {
    const mech3::Installer in = open_with(change);
    if (in.ok || in.error.find(word) == std::string::npos) std::printf("  wanted a refusal with \"%s\": ok %d, \"%s\"\n", word, in.ok, in.error.c_str());
    return in.recognised && !in.ok && in.error.find(word) != std::string::npos;
  };
  EXPECT(refused_for([](Made& x) { x.files[5].name = "Game Folder\\..\\..\\evil.txt"; }, "not one inside a folder"));
  EXPECT(refused_for([](Made& x) { x.files[5].name = "C:\\Windows\\evil.txt"; }, "not one inside a folder"));
  EXPECT(refused_for([](Made& x) { x.files[5].name = "\\\\server\\share\\evil.txt"; }, "not one inside a folder"));
  EXPECT(refused_for([](Made& x) { x.files[5].name = "Game Folder\\zbd\\CON"; }, "not one inside a folder"));
  EXPECT(refused_for([](Made& x) { x.files[6].name = "Game Folder\\ZBD\\READER.TXT"; }, "names a file twice"));
  EXPECT(refused_for([](Made& x) { x.files.clear(); x.files.push_back({"Uninstal.exe", {}, 1, 2}); }, "holds no files"));
  // The list patched after it is built: a packed size, an offset and a count past the end; a method byte that is none.
  auto patched = [&](const std::function<void(Made&, Bytes&)>& patch) {
    Made x = sample();
    x.build();
    Bytes list = x.list;
    Made y = sample();
    // Rebuilt with the patched list: the list block is made from `list` (a stored zlib stream, so patching the
    // built file's bytes would break its checksum).
    Bytes out = y.build();
    patch(y, list);
    Bytes tail(out.begin() + static_cast<long>(y.list_block_at), out.end());
    const size_t old_size = 8 + (tail[4] | tail[5] << 8 | tail[6] << 16 | static_cast<size_t>(tail[7]) << 24);
    Bytes data_block(tail.begin() + static_cast<long>(old_size), tail.end());
    out.resize(y.list_block_at);
    Made::block(out, 0x143a, 1, Made::packed_block(list));
    out.insert(out.end(), data_block.begin(), data_block.end());
    return mech3::installer_open(write_file(dir + "/p.exe", out));
  };
  {
    const mech3::Installer in = patched([](Made& y, Bytes& l) { set32(l, y.list_at[5] + 0x20, 0x7fffffff); });  // packed size
    EXPECT(in.recognised && !in.ok && in.error.find("runs past the installer's end") != std::string::npos);
  }
  {
    const mech3::Installer in = patched([](Made& y, Bytes& l) { set32(l, y.list_at[5] + 0x1c, 0xfffffff0); });  // offset
    EXPECT(!in.ok && in.error.find("runs past the installer's end") != std::string::npos);
  }
  {
    const mech3::Installer in = patched([](Made&, Bytes& l) { set32(l, 0, 5000000); });  // the count
    EXPECT(!in.ok && !in.error.empty());
  }
  {
    const mech3::Installer in = patched([](Made& y, Bytes& l) { l[y.list_at[5]] = 0xff, l[y.list_at[5] + 1] = 0xff; });  // a record's length
    EXPECT(!in.ok && !in.error.empty());
  }
  // A file that unpacks past, or short of, its declared size; a method byte that is not one.
  for (const int64_t declared : {int64_t{3}, int64_t{400}}) {
    Made x = sample();
    x.files[5].declared = declared;
    const std::string p = write_file(dir + "/d.exe", x.build());
    const mech3::Installer in = mech3::installer_open(p);
    EXPECT(in.ok);
    Bytes out;
    std::string why;
    EXPECT(in.ok && !mech3::installer_extract(p, in.files[4], &out, &why) && out.empty() && !why.empty());
  }
  {
    Made x = sample();
    x.files[7].declared = 100;  // the bzip2 one, declared smaller than it unpacks to
    const std::string p = write_file(dir + "/b.exe", x.build());
    const mech3::Installer in = mech3::installer_open(p);
    Bytes out;
    std::string why;
    EXPECT(in.ok && !mech3::installer_extract(p, in.files[6], &out, &why) && out.empty());
    EXPECT(why.find("more than its declared size") != std::string::npos);
  }
  {
    Made x = sample();
    Bytes b = x.build();
    const mech3::Installer good = mech3::installer_open(write_file(dir + "/m.exe", b));
    EXPECT(good.ok);
    if (good.ok) {
      b[static_cast<size_t>(good.files[4].data_at)] = 0;  // "stored": never taken
      const mech3::Installer in = mech3::installer_open(write_file(dir + "/m.exe", b));
      EXPECT(!in.ok && in.error.find("does not start as packed data") != std::string::npos);
    }
  }
  // Where the game's folder is cannot be told: two zbd folders, or none anywhere.
  {
    mech3::Installer in = open_with([](Made& x) { x.files.push_back({"Other\\zbd\\y.txt", bytes_of("y")}); });
    std::string why;
    EXPECT(in.ok && !mech3::installer_map_to_game(&in, &why) && why.find("two places") != std::string::npos);
    Made n;
    n.files = {{"readme.txt", bytes_of("r")}};
    mech3::Installer in2 = mech3::installer_open(write_file(dir + "/n.exe", n.build()));
    EXPECT(in2.ok && !mech3::installer_map_to_game(&in2, &why) && why.find("does not say where") != std::string::npos);
  }
}

// Our bzip2 reader on the fixture: unpacked whole; every truncation and every single-bit flip refused (or, for
// a bit the format does not use, the same bytes), never more out than the cap, never other bytes; made-up
// headers refused.
TEST(bz2_read_refuses_what_is_wrong) {
  const Bytes z = bz2_fixture(), want = bz2_fixture_text();
  Bytes out;
  std::string why;
  size_t used = 0;
  EXPECT(mech3::bz2_decompress(z.data(), z.size(), want.size(), &out, &why, &used) && out == want && used == z.size());
  int refused = 0, same = 0, other = 0;
  for (size_t cut = 0; cut < z.size(); ++cut)
    if (mech3::bz2_decompress(z.data(), cut, want.size(), &out, &why)) {
      ++other;
    } else {
      ++refused;
      EXPECT(out.empty() && !why.empty());
    }
  for (size_t bit = 0; bit < z.size() * 8; ++bit) {
    Bytes m = z;
    m[bit / 8] ^= static_cast<uint8_t>(1u << (bit % 8));
    if (!mech3::bz2_decompress(m.data(), m.size(), want.size(), &out, &why)) ++refused;
    else if (out == want) ++same;
    else ++other;
    EXPECT(out.size() <= want.size());
  }
  std::printf("  bzip2: %d refused, %d the same bytes, %d other bytes\n", refused, same, other);
  EXPECT(other == 0 && refused > 1500);
  // The block's own CRC (bytes 10 to 13, after the stream's head and the block's magic) is checked, not only
  // the stream's: a bit of it flipped is a refusal though the data and the stream's CRC are untouched.
  for (size_t bit = 10 * 8; bit < 14 * 8; ++bit) {
    Bytes m = z;
    m[bit / 8] ^= static_cast<uint8_t>(1u << (bit % 8));
    EXPECT(!mech3::bz2_decompress(m.data(), m.size(), want.size(), &out, &why) && why.find("block's checksum") != std::string::npos);
  }
  for (size_t cap = 0; cap < want.size(); cap += 11) EXPECT(!mech3::bz2_decompress(z.data(), z.size(), cap, &out, &why) && out.empty());
  for (const char* head : {"BZh0", "BZh:", "BZx1", "AZh1", "BZ"}) {
    Bytes m = z;
    std::copy(head, head + std::strlen(head), m.begin());
    if (std::strlen(head) < 4) m.resize(2);
    EXPECT(!mech3::bz2_decompress(m.data(), m.size(), want.size(), &out, &why));
  }
  // A block that starts as the stream's end too early; an origin past the block; a randomised block.
  {
    Bytes m = z;
    m[14] |= 0x80;  // the randomised bit (after the block's magic and CRC)
    EXPECT(!mech3::bz2_decompress(m.data(), m.size(), want.size(), &out, &why));
    m = z;
    m[14] = 0x7f, m[15] = 0xff, m[16] = 0xff;  // the origin pointer
    EXPECT(!mech3::bz2_decompress(m.data(), m.size(), want.size(), &out, &why));
  }
  EXPECT(!mech3::bz2_decompress(nullptr, 0, 10, &out, &why));
}


namespace {
void put_file(const fs::path& p, const std::string& text) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p, std::ios::binary) << text;
}
std::map<std::string, std::string> snapshot(const std::string& dir) {
  std::map<std::string, std::string> m;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) {
      std::ifstream f(it->path(), std::ios::binary);
      m[fs::relative(it->path(), dir, ec).generic_string()] = std::string((std::istreambuf_iterator<char>(f)), {});
    }
  return m;
}
std::string names_in(const std::string& dir) {
  std::string s;
  for (const auto& [rel, data] : snapshot(dir)) s += rel + "|";
  return s;
}
void build_install(const std::string& inst) {
  put_file(inst + "/Mech3.exe", "the game");
  put_file(inst + "/zbd/reader.txt", "the reader");
  put_file(inst + "/zbd/c1/gamez.txt", "abc");  // the same as the installer's
  put_file(inst + "/" + TEST_ONLY_OURS, "a file only this game has");
  put_file(inst + "/video/intro.avi", "a movie");
}
}  // namespace

// IMPORT given the mod's installer program: nothing run; the files that differ from the player's install or are
// new under the install's folders become the mod, the same never written; the program, dlls and the rest left
// and said; the install untouched.
TEST(import_installer_takes_what_differs) {
  const std::string wd = tmp("import_installer_wd"), inst = tmp("import_installer_inst");
  build_install(inst);
  Made m = sample();
  Made nested;  // an add-on's installer carried inside
  nested.files = {{"zbd\\addon.txt", bytes_of("an add-on")}};
  m.files.push_back({"Game Folder\\extras\\Add-on Setup.exe", nested.build()});
  m.files.push_back({"Game Folder\\Reinstaller.exe", bytes_of("MZ a plain program")});
  const std::string path = write_file(tmp("import_installer_src") + "/Made Mod Setup.exe", m.build());
  const auto inst_before = snapshot(inst);
  const std::string src_before = snapshot(fs::path(path).parent_path().string()).begin()->second;
  int asked = 0;
  std::string report;
  const mech3::ModAddDone d = mech3::import_folder_run(wd, path, inst, [&](const std::string&) { return ++asked, 1; }, nullptr, &report);
  std::printf("  %s\n  %s\n", d.message.c_str(), report.c_str());
  EXPECT(d.ok && d.folder == "A Made-up Mod" && asked == 0);  // named by the installer's title; the game from its files
  const std::string at = wd + "/mods/" + d.folder;
  std::printf("  the mod: %s\n", names_in(at).c_str());
  // Its Mech3Msg.dll is no table of texts here ("texts"): a warning kept with the mod, the mod added.
  EXPECT(names_in(at) == std::string("mod.txt|") + mech3::kModAddedMarker + "|" + mech3::kModLeftOutFile + "|zbd/c1/New.txt|zbd/reader.txt|");
  EXPECT(d.warnings == 1 && d.texts.found && !d.texts.read);
  const auto got = snapshot(at);
  const Bytes text = bz2_fixture_text();
  EXPECT(got.count("zbd/c1/New.txt") && got.at("zbd/c1/New.txt") == std::string(text.begin(), text.end()));
  EXPECT(got.count("zbd/reader.txt") && got.at("zbd/reader.txt") == "the mod's reader");
  EXPECT(got.count(mech3::kModAddedMarker) && got.at(mech3::kModAddedMarker).find("Added by the game from " + path + ".") != std::string::npos);
  EXPECT(report == "Took 2 files from the installer that differ from your install; the program and its dlls are not part of a mod here.");
  EXPECT(snapshot(inst) == inst_before);  // read only
  EXPECT(snapshot(fs::path(path).parent_path().string()).begin()->second == src_before);
  std::error_code ec;
  EXPECT(!fs::exists(wd + "/mod-import", ec) && !fs::exists(wd + "/mod-staging", ec));
  // The counts, and the long line.
  int calls = 0, last_total = -1;
  mech3::ImportFolder f = mech3::import_installer_prepare(wd, path, inst, {}, [&](int, int total) { ++calls, last_total = total; });
  std::printf("  %s\n", mech3::import_folder_report(f).c_str());
  EXPECT(f.install && f.installer && f.error.empty() && f.taken == 2 && f.same == 2 && f.programs == 3 && f.other == 2 && f.own == 0);
  EXPECT(f.msg_dll && f.nested == 1 && f.game == (TEST_IS_PM ? mech3::ModGame::kPm : mech3::ModGame::kMw3) && f.title == "A Made-up Mod");
  EXPECT(calls == 6 && last_total == 5);  // the five it had to unpack to compare (its Mech3Msg.dll one), and the end
  EXPECT(mech3::import_folder_report(f) ==
         "Took 2 files from the installer that differ from your install; left 2 the same; the program and its dlls are not part of a "
         "mod here (3); 2 files outside the game's folders left; its Mech3Msg.dll's texts go into the mod's strings.txt; "
         "this installer also carries 1 more installer: import each on its own.");
  mech3::import_folder_discard(f);
  EXPECT(!fs::exists(wd + "/mod-import", ec));
}

// An installer whose Mech3Msg.dll is a table of texts: those that differ from the player's go into the mod's
// strings.txt, the dll not kept; with the player's own dll in it nothing is said of texts.
TEST(import_installer_takes_the_texts_of_its_mech3msg_dll) {
  const std::string wd = tmp("import_installer_texts_wd"), inst = tmp("import_installer_texts_inst");
  build_install(inst);
  const std::string mine = message_dll({{100, "CAMPAIGN"}, {101, "START"}});
  put_file(inst + "/Mech3Msg.dll", mine);
  Made m = sample();
  for (Made::F& f : m.files)
    if (f.name == "Game Folder\\Mech3Msg.dll") f.data = bytes_of(message_dll({{100, "KAMPAGNE"}, {101, "START"}, {300, "A new briefing"}}));
  const std::string path = write_file(tmp("import_installer_texts_src") + "/Words Setup.exe", m.build());
  std::string report;
  const mech3::ModAddDone d = mech3::import_folder_run(wd, path, inst, nullptr, nullptr, &report);
  std::printf("  %s\n", d.message.c_str());
  const std::string at = wd + "/mods/" + d.folder;
  EXPECT(d.ok && d.warnings == 0 && d.texts.read && d.texts.taken == 2);
  EXPECT(names_in(at) == std::string("mod.txt|") + mech3::kModAddedMarker + "|strings.txt|zbd/c1/New.txt|zbd/reader.txt|");
  EXPECT(snapshot(at).at("strings.txt").find("\n0x64=KAMPAGNE\n0x12c=A new briefing\n") != std::string::npos);
  // The player's own dll in the installer: a program, as before.
  for (Made::F& f : m.files)
    if (f.name == "Game Folder\\Mech3Msg.dll") f.data = bytes_of(mine);
  mech3::ImportFolder f = mech3::import_installer_prepare(wd, write_file(tmp("import_installer_texts_src2") + "/Same Setup.exe", m.build()), inst);
  EXPECT(f.error.empty() && !f.msg_dll && f.taken == 2 && f.programs == 3);  // the game, DDraw.dll and the dll
  mech3::import_folder_discard(f);
}

// Refused, and said: the other game's installer; one whose every file is the player's already; one that does
// not say where the game's folder is; one whose file does not unpack; a .rar; names in capitals land in the
// install's own folder.
TEST(import_installer_refusals_and_case) {
  const std::string wd = tmp("import_installer_refuse_wd"), inst = tmp("import_installer_refuse_inst"), dir = tmp("import_installer_refuse_src");
  build_install(inst);
  auto run = [&](Made m, const char* name = "x.exe") {
    std::string report = "x";
    const mech3::ModAddDone d = mech3::import_folder_run(wd, write_file(dir + "/" + name, m.build()), inst, [](const std::string&) { return TEST_OUR_ANSWER; }, nullptr, &report);
    EXPECT(d.ok || report.empty());
    return d;
  };
  Made pm;
  pm.folder = "C:\\MicroProse";
  pm.files = {{std::string("The Other Game\\") + (TEST_IS_PM ? "zbd\\c4b\\m5.txt" : "zbd\\c1\\readersa1.txt"), bytes_of("the other's")}};
  mech3::ModAddDone d = run(pm);
  std::printf("  other game: %s\n", d.message.c_str());
  EXPECT(!d.ok && d.message == std::string("Not added: that installer is for ") + TEST_THEIR_NAME + ", and yours is " + TEST_OUR_NAME + ".");
  Made same;
  same.files = {{"zbd\\c1\\gamez.txt", bytes_of("abc")}, {"Mech3.exe", bytes_of("another exe")}};
  d = run(same);
  EXPECT(!d.ok && d.message.find("nothing to add") != std::string::npos);
  Made nowhere;
  nowhere.files = {{"readme.txt", bytes_of("r")}};
  d = run(nowhere);
  EXPECT(!d.ok && d.message.find("does not say where its files go") != std::string::npos);
  Made broken = sample();
  broken.files[5].declared = 3;  // its reader declared shorter than it unpacks to
  d = run(broken);
  std::printf("  broken: %s\n", d.message.c_str());
  EXPECT(!d.ok && d.message.find("zbd/reader.txt does not unpack") != std::string::npos);
  std::error_code ec;
  EXPECT(!fs::exists(wd + "/mods", ec) && !fs::exists(wd + "/mod-import", ec));
  d = mech3::import_folder_run(wd, write_file(dir + "/A Mod.rar", bytes_of("Rar!\x1a\x07 not really")), inst, nullptr, nullptr);
  EXPECT(!d.ok && d.message == "Not added: unpack this .rar first, then IMPORT the program inside it.");
  // ZBD in capitals, names from the game folder itself (the oldest maker's records): into the install's zbd.
  Made caps;
  caps.old_layout = true;
  caps.title = "";
  caps.files = {{"ZBD\\c1\\Map.txt", bytes_of("a map")}, {"notes.txt", bytes_of("n")}};
  d = run(caps, "Caps Pack.exe");
  EXPECT(d.ok && d.folder == "Caps Pack");  // no title: the file's name
  EXPECT(names_in(wd + "/mods/" + d.folder) == std::string("mod.txt|") + mech3::kModAddedMarker + "|zbd/c1/Map.txt|");
}

// Real installers, when a folder of them is named: MW3_INSTALLERS=<folder> (searched for .exe files): one line
// each, every file unpacked and its size checked. Nothing is written.
TEST(installer_read_real_folder_env) {
  const char* dir = std::getenv("MW3_INSTALLERS");
  if (!dir || !*dir) return;
  std::vector<fs::path> found;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec) && it->path().extension() == ".exe" && it->path().parent_path().filename() == "dl") found.push_back(it->path());
  std::sort(found.begin(), found.end());
  for (const fs::path& p : found) {
    mech3::Installer in = mech3::installer_open(p.string());
    if (!in.recognised) {
      std::printf("  %s: not a Clickteam installer\n", p.filename().string().c_str());
      continue;
    }
    if (!in.ok) {
      std::printf("  %s: REFUSED: %s\n", p.filename().string().c_str(), in.error.c_str());
      continue;
    }
    std::string why;
    const bool mapped = mech3::installer_map_to_game(&in, &why);
    uint64_t bytes = 0;
    int good = 0, bad = 0, in_game = 0;
    std::string first_bad;
    for (const mech3::InstallerFile& f : in.files) {
      std::vector<uint8_t> out;
      std::string w;
      if (mech3::installer_extract(p.string(), f, &out, &w) && out.size() == f.size) ++good, bytes += f.size;
      else if (++bad == 1) first_bad = f.name + ": " + w;
      in_game += !f.path.empty();
    }
    std::printf("  %s: title \"%s\", default \"%s\"; %zu files (%d others), %d unpack whole, %d not, %llu bytes; game root \"%s\" (%s), %d under the game\n",
                p.filename().string().c_str(), in.title.c_str(), in.default_folder.c_str(), in.files.size(), in.others, good, bad,
                static_cast<unsigned long long>(bytes), in.game_root.c_str(), mapped ? "mapped" : why.c_str(), in_game);
    if (bad) std::printf("    first not read: %s\n", first_bad.c_str());
  }
}

// The whole import over real installers: MW3_IMPORT_INSTALLERS=<folder> (its .exe files under dl/ folders, or
// one file), each against the install of its game (MW3_GAME_DIR, and Pirate's Moon's beside it at ../pm), into
// the test's own write dir: one line each (name, taken, left by kind, refused and why, seconds).
TEST(import_installer_real_folder_env) {
  const char* dir = std::getenv("MW3_IMPORT_INSTALLERS");
  if (!dir || !*dir) return;
  std::vector<fs::path> found;
  std::error_code ec;
  if (fs::is_regular_file(dir, ec)) found.push_back(dir);
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec) && it->path().extension() == ".exe" && it->path().parent_path().filename() == "dl") found.push_back(it->path());
  std::sort(found.begin(), found.end());
  const std::string mw3 = MW3_GAME_DIR, pm = std::string(MW3_GAME_DIR) + "/../pm";
  const std::string wd = std::string(MW3_TEST_TMP) + "/import_installers";
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  for (const fs::path& p : found) {
    const auto t0 = std::chrono::steady_clock::now();
    std::string install = mw3, report;
    mech3::ModAddDone d = mech3::import_folder_run(wd, p.string(), install, nullptr, nullptr, &report);
    if (!d.ok && d.message.find("is for Pirate's Moon") != std::string::npos) {
      install = pm;
      const std::string was = mech3::mods_game();
      mech3::mods_game() = "mw3pm";
      d = mech3::import_folder_run(wd, p.string(), install, nullptr, nullptr, &report);
      mech3::mods_game() = was;
    }
    if (!d.ok && install == pm) {  // on this branch a Pirate's Moon mod is the other game's: what the import would take
      mech3::ImportFolder f = mech3::import_installer_prepare(wd, p.string(), pm);
      int zbd = 0;
      for (fs::recursive_directory_iterator it(f.picked.empty() ? fs::path(wd) / "none" : fs::path(f.picked) / "zbd", ec), end; !ec && it != end; it.increment(ec))
        zbd += it->is_regular_file(ec);
      std::string same;
      for (const std::string& n : f.same_names) same += n + " ";
      std::printf("    against Pirate's Moon's install: %s %s [%d under zbd; the same as the install's: %s]\n", f.error.c_str(),
                  mech3::import_folder_report(f).c_str(), zbd, same.c_str());
      mech3::import_folder_discard(f);
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    int files = 0;
    if (d.ok)
      for (fs::recursive_directory_iterator it(fs::path(wd) / "mods" / d.folder, ec), end; !ec && it != end; it.increment(ec)) files += it->is_regular_file(ec);
    std::printf("  %s (%s, %.0f s): %s%s%s\n", p.filename().string().c_str(), install == pm ? "Pirate's Moon" : "MechWarrior 3", secs,
                d.message.c_str(), d.ok ? (" [" + std::to_string(files) + " files in mods/" + d.folder + "]").c_str() : "", report.empty() ? "" : (" " + report).c_str());
  }
}
