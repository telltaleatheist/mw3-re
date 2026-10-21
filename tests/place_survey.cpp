// The placement survey's capture (test_program.cpp's
// placements_on_every_campaign_mission_headless): kept apart because
// mech_place.h's Handle and the app's differ. Everything is read at the
// query, while the mission's nodes and materials are alive.
#include <cstdio>
#include <string>
#include <vector>

#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_matl.h"
#include "mech3/mech_place.h"
#include "mech3/mission_world.h"
#include "mech3/veh_basic.h"

namespace {
struct Place {
  int leg;
  float x, y, z;
  float ys[2];
  std::string under;
};
std::vector<Place> g_places;
int g_leg = 0;
// The data survey: every aiv.zrd record's place (MW3_PLACE_POINTS lines
// "world archive name x y z"), checked on a world before its first mech.
struct Point {
  std::string world, archive, name;
  float x, y, z;
};
std::vector<Point> g_points;
std::vector<std::string> g_point_lines;
std::string g_world, g_done;
int g_point_moves = 0;

void picks(const zcls::GroundHits& hits, float y, float ys[2], std::string* under) {
  ys[0] = ys[1] = y;
  if (hits.count == 0) return;
  for (int fix = 0; fix < 2; ++fix) {
    int32_t best = -1, in_water = 0;
    float surface = 0;
    ys[fix] = mech3::ground_pick(hits, y, fix ? 10.0f : y + 10.0f, &best, 1, &in_water, &surface) + 0.01f;
  }
  char b[128];
  for (int i = 0; i < hits.count; ++i) {
    const zcls::GroundHit& h = hits.hit[i];
    std::snprintf(b, sizeof b, " %.2f(%s%s)", h.height, h.node ? h.node->name() : "?",
                  h.material && h.material->soil == 1 ? ", water" : "");
    *under += b;
  }
}

void survey_points(zcls::Node* w) {
  if (g_world == g_done) return;
  g_done = g_world;
  zmod::Zones& zones = zmod::zones();
  const int32_t zone = zones.mode;
  zones.mode = 0;
  for (const Point& pt : g_points) {
    if (pt.world != g_world) continue;
    zcls::GroundHits hits;
    if (zcls::ground_point(w, pt.x, pt.y + 100.0f, pt.z, &hits) != 0) hits.count = 0;
    float ys[2];
    std::string under;
    picks(hits, pt.y, ys, &under);
    const bool moves = ys[0] != ys[1];
    g_point_moves += moves;
    char b[256];
    std::snprintf(b, sizeof b, "%s %s %s  at (%.2f %.2f %.2f)  original %.2f  fixed %.2f%s  under:", pt.world.c_str(),
                  pt.archive.c_str(), pt.name.c_str(), pt.x, pt.y, pt.z, ys[0], ys[1], moves ? "  MOVES" : "");
    g_point_lines.push_back(b + under);
  }
  zones.mode = zone;
}
}  // namespace

void place_survey_begin(const char* points) {
  g_places.clear(), g_leg = 0, g_points.clear(), g_point_lines.clear(), g_done.clear(), g_point_moves = 0;
  if (std::FILE* f = points ? std::fopen(points, "r") : nullptr) {
    char w[16], a[32], n[64];
    float x, y, z;
    while (std::fscanf(f, "%15s %31s %63s %f %f %f", w, a, n, &x, &y, &z) == 6) g_points.push_back({w, a, n, x, y, z});
    std::fclose(f);
  }
  mech3::place_calls().ground = [](float x, float y, float z, zcls::GroundHits* out) {
    zcls::Node* w = mech3::world_hooks().world;
    if (w) survey_points(w);
    const int none = w ? zcls::ground_point(w, x, y, z, out) : (out->count = 0, 1);
    Place pl{g_leg, x, y - 100.0f, z, {0, 0}, {}};
    zcls::GroundHits empty;
    empty.count = 0;
    picks(none == 0 ? *out : empty, pl.y, pl.ys, &pl.under);
    g_places.push_back(pl);
    return none;
  };
}
void place_survey_leg(int leg, const char* world) { g_leg = leg, g_world = world; }
// Writes the lists (when f), returns the placements and how many the fix
// moves (the live ones, then the records').
int place_survey_end(std::FILE* f, const char* const* leg_names, int* moved, int* points, int* point_moves) {
  *points = static_cast<int>(g_point_lines.size()), *point_moves = g_point_moves;
  mech3::place_calls() = mech3::PlaceCalls{};
  *moved = 0;
  for (const Place& pl : g_places) {
    const bool moves = pl.ys[0] != pl.ys[1];
    *moved += moves;
    if (f)
      std::fprintf(f, "%s  at (%.2f %.2f %.2f)  original %.2f  fixed %.2f%s  under:%s\n", leg_names[pl.leg], pl.x, pl.y,
                   pl.z, pl.ys[0], pl.ys[1], moves ? "  MOVES" : "", pl.under.c_str());
  }
  if (f) {
    std::fprintf(f, "\nEvery aiv.zrd record's place, on its world before any mech:\n");
    for (const std::string& l : g_point_lines) std::fprintf(f, "%s\n", l.c_str());
  }
  return static_cast<int>(g_places.size());
}
