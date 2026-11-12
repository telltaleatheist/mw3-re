// The vehicle editor's save (upgrades/editor/aiv_save) for
// test_aiv_save_mech3ax.py: every record of an archive's aiv.zrd written over
// itself, as read (its bytes must come back) or with one group of its
// settings changed (the record must read back as changed), the archive saved
// with all of them.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gamez/znetwork/znet_object.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/net_save.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_region.h"
#include "mech3/zrdraiv.h"
#include "upgrades/editor/aiv_save.h"
#include "upgrades/editor/zrd_bytes.h"

namespace {
using namespace mech3;
using namespace mech3::zrdbytes;

zrdr::Node* tree(const uint8_t* p, size_t n) {
  std::FILE* f = std::tmpfile();
  std::fwrite(p, 1, n, f);
  std::rewind(f);
  auto* node = new zrdr::Node{};
  zrdr::read_node(f, node);
  std::fclose(f);
  return node;
}

// Every string in the bytes that names a region ("reg_..."), so the records'
// orders find one: the probe has no world to read them from.
void regions_for(const uint8_t* p, size_t size) {
  for (size_t q = 0; q + 8 <= size; q += 4)
    if (get32(p + q) == zrdr::String) {
      const std::string s = string_at(p, size, q);
      if (s.rfind("reg_", 0) == 0 && !ai::region_named(s.c_str()))
        ai::regions().push_back(new ai::Region(s.c_str(), znet::alloc_id(znet::kKeptIds, znet::kKeptIds, znet::kKeptIds + 1000), 0));
    }
}

uint32_t g_seed = 1;
uint32_t rnd(uint32_t n) {
  g_seed = g_seed * 1103515245u + 12345u;
  return (g_seed >> 8) % n;
}

// One group changed, in the file's units (as the dialog edits them).
void mutate(ai::AiParams& p, int32_t* mech3) {
  p.to_file_units();
  static const int kWords[] = {0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 0x3c, 0x24, 0x40, 0x50, 0x58, 0x64,
                               0x80, 0x8c, 0xa4, 0xac, 0xb4, 0xb8, 0xc0, 0xc8};
  switch (rnd(9)) {
    case 0: {
      const int off = kWords[rnd(sizeof kWords / sizeof kWords[0])];
      const bool is_int = off == 0x04 || off == 0x18 || off == 0x3c || off == 0x40 || off == 0x58 || off == 0x80 ||
                          off == 0x8c || off == 0xa4 || off == 0xc8;
      if (is_int)
        p.put<int32_t>(off, p.get<int32_t>(off) + 1 + static_cast<int32_t>(rnd(5)));
      else
        p.put<float>(off, p.get<float>(off) + 1.25f + static_cast<float>(rnd(100)));
      break;
    }
    case 1: {  // a flag bit
      const int byte = rnd(2) ? 0xcc : 0xcd;
      p.raw[byte] ^= static_cast<uint8_t>(1u << rnd(byte == 0xcc ? 8 : 2));
      if ((p.raw[0xcd] & 3) == 3) p.raw[0xcd] &= static_cast<uint8_t>(~2u);  // lancemate is 0, 1 or 2 (PM's target switches above kept)
      break;
    }
    case 2:
      p.put<int32_t>(0xc4, rnd(3) ? static_cast<int32_t>(rnd(19)) : -1);
      break;
    case 3:
      std::free(p.pilot), p.pilot = rnd(4) ? strdup("Tester Pilot") : nullptr;
      if (!p.pilot) {  // none, can_configure as the defaults' (else "" is written)
        const ai::AiParams d;
        p.raw[0xcc] = static_cast<uint8_t>((p.raw[0xcc] & 0x7f) | (d.raw[0xcc] & 0x80));
      }
      break;
    case 4:
      std::free(p.sound_tag), p.sound_tag = strdup(rnd(2) ? "dom" : "epona");  // none cannot be written for a lancemate
      break;
    case 5:
    case 6:
      if (p.vlist && !p.vlist->empty()) {
        ai::VListEntry& e = p.vlist->front();
        switch (rnd(4)) {
          case 0: e.f04 += 0.5f; break;
          case 1: e.f10 += 7.0f; break;
          case 2: e.valve = ai::valve_get("probe_valve"); break;
          default: e.flags ^= 0x10, e.flags2 ^= 1;
        }
        break;
      }
      [[fallthrough]];
    case 7: {  // an order added
      if (!p.vlist) p.vlist = new std::list<ai::VListEntry>;
      ai::VListEntry e;
      e.valve = ai::valve_get("probe_valve");
      e.f0c = 12.5f;
      p.vlist->push_back(e);
      break;
    }
    default:
      *mech3 = !*mech3;
  }
  p.finish();
}

bool same(const ai::AiParams& a, const ai::AiParams& b) {
  // The yaw is degrees in the file, radians read: a rewritten one comes back
  // within an ulp or so.
  ai::AiParams x, y;
  x = a, y = b;
  const float ya = x.yaw(), yb = y.yaw();
  x.put<float>(0x14, 0), y.put<float>(0x14, 0);
  const auto text = [](const char* m, const char* n) { return (!m && !n) || (m && n && std::strcmp(m, n) == 0); };
  return std::fabs(ya - yb) <= 1e-5f && x.same_numbers(y) && x.same_vlist(y) && text(x.pilot, y.pilot) &&
         text(x.sound_tag, y.sound_tag);
}
}  // namespace

// stats: records, kept byte for byte (unchanged runs), read back the same,
// read back differently, records the writer refused.
extern "C" int probe_aiv_save(const char* in, const char* out, uint32_t seed, int changed, int32_t* stats, char* bad,
                              int cap) {
  std::ifstream f(in, std::ios::binary);
  const std::vector<uint8_t> a((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (a.size() < 12) return -1;
  // Pirate's Moon's archives are version 2: version, count and a CRC after the table (MW3's: version and count).
  const bool v2 = get32(a.data() + a.size() - 12) == 2;
  const size_t tail = v2 ? 12 : 8;
  const uint32_t count = get32(a.data() + a.size() - tail + 4);
  if (static_cast<uint64_t>(count) * sizeof(zutl_zar::Entry) > a.size() - tail) return -1;
  const size_t table = a.size() - tail - count * sizeof(zutl_zar::Entry);
  std::vector<uint8_t> aiv;
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry e;
    std::memcpy(&e, a.data() + table + i * sizeof e, sizeof e);
    if (e.start > table || e.length > table - e.start) return -1;
    if (std::strncmp(e.name, "aiv.zrd", sizeof e.name) == 0) aiv.assign(a.begin() + e.start, a.begin() + e.start + e.length);
  }
  if (aiv.empty()) return -2;
  g_seed = seed;
  std::memset(stats, 0, 5 * sizeof(int32_t));
  bad[0] = 0;
  regions_for(aiv.data(), aiv.size());
  std::vector<Pair> pairs;
  size_t list_end = 0;
  if (!record_pairs_of(aiv.data(), aiv.size(), &pairs, &list_end)) return -3;
  std::vector<AivEdit> edits;
  for (const Pair& pr : pairs) {
    ++stats[0];
    zrdr::Node* t = tree(aiv.data() + pr.value, pr.end - pr.value);
    ai::AivRecord rec;
    rec.parse(t, pr.key.c_str());
    zrdr::free_node(t);
    int32_t m3 = rec.mech3;
    ai::AiParams want;
    want = rec;
    if (changed) mutate(want, &m3);
    {
      // No pilot with can_configure away from the defaults' is written as ""
      // (aiv_save.h: the flag has no other home), which reads as an empty name.
      const ai::AiParams d;
      if (!want.pilot && ((want.raw[0xcc] ^ d.raw[0xcc]) & 0x80)) want.pilot = strdup("");
    }
    const int32_t m3_set = changed && m3 != rec.mech3 ? 1 : rec.mech3_set;
    std::vector<uint8_t> v =
        aiv_record_written(aiv.data() + pr.value, pr.end - pr.value, want, m3_set, m3, pr.key.c_str());
    if (v.empty()) {
      ++stats[4];
      continue;
    }
    if (!changed && v == std::vector<uint8_t>(aiv.begin() + pr.value, aiv.begin() + pr.end)) ++stats[1];
    zrdr::Node* back = tree(v.data(), v.size());
    ai::AivRecord got;
    got.parse(back, pr.key.c_str());
    zrdr::free_node(back);
    if (same(got, want) && (!m3_set || got.mech3 == m3)) {
      ++stats[2];
    } else {
      ++stats[3];
      if (!bad[0]) {
        int at = 0;
        for (int off = 4; off < 0xd0 && !at; off += 4)
          if (got.get<uint32_t>(off) != want.get<uint32_t>(off) && off != 0x14) at = off;
        std::snprintf(bad, size_t(cap), "%s (+%#x %08x/%08x; vlist %d; pilot %s/%s; voice %s/%s)", pr.key.c_str(), at,
                      at ? got.get<uint32_t>(at) : 0, at ? want.get<uint32_t>(at) : 0, got.same_vlist(want) ? 1 : 0,
                      got.pilot ? got.pilot : "-", want.pilot ? want.pilot : "-", got.sound_tag ? got.sound_tag : "-",
                      want.sound_tag ? want.sound_tag : "-");
      }
    }
    edits.push_back({pr.key, std::move(v)});
  }
  const std::vector<uint8_t> edited = aiv_with_edits(aiv.data(), aiv.size(), edits);
  const std::vector<uint8_t> saved = edited.empty() ? edited : ai::archive_with_entry(a, "aiv.zrd", edited);
  if (saved.empty()) return -4;
  std::ofstream(out, std::ios::binary).write(reinterpret_cast<const char*>(saved.data()), static_cast<std::streamsize>(saved.size()));
  ai::regions_clear();
  ai::valves_clear();
  return 0;
}
