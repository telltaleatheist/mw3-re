// One bar for a press (2026-11-27; launcher_job.h, docs/launcher.md, One bar for a press).
#include "frontend/sdl/launcher_job.h"
#include "frontend/sdl/launcher_settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sdlfe::job {

double Job::setup_share() { return settings::get(settings::kSetupShare); }
double Job::window() { return settings::get(settings::kSpeedWindowSeconds); }
double Job::settle() { return settings::get(settings::kSettleSeconds); }
double Job::stall() { return settings::get(settings::kStallSeconds); }

namespace {
const char* const kDot = " \xc2\xb7 ";  // " · "
}

std::string mb(uint64_t bytes) {
  const double m = static_cast<double>(bytes) / 1e6;
  char buf[32];
  std::snprintf(buf, sizeof buf, m < 10 ? "%.1f MB" : "%.0f MB", m);
  return buf;
}

std::string speed_words(double bytes_per_s) {
  char buf[32];
  if (bytes_per_s < 1e6) std::snprintf(buf, sizeof buf, "%.0f KB/s", bytes_per_s / 1e3);
  else std::snprintf(buf, sizeof buf, "%.1f MB/s", bytes_per_s / 1e6);
  return buf;
}

std::string left_words(double seconds) {
  if (seconds < 60) return "less than a minute left";
  const long minutes = std::lround(seconds / 60);
  if (minutes < 60) return "about " + std::to_string(minutes) + " min left";
  const long h = minutes / 60, m = minutes % 60;
  return "about " + std::to_string(h) + " h" + (m ? " " + std::to_string(m) + " min" : std::string()) + " left";
}

Job::Job(std::string name, std::vector<Part> parts) : name_(std::move(name)) {
  for (Part& p : parts) parts_.push_back(State{std::move(p)});
}

int Job::find(const std::string& key) const {
  for (size_t k = 0; k < parts_.size(); ++k)
    if (parts_[k].part.key == key) return static_cast<int>(k);
  return -1;
}

bool Job::has(const std::string& key) const {
  const int k = find(key);
  return k >= 0 && !parts_[k].dropped;
}

void Job::begin(int k, double now) {
  if (k == at_) return;
  for (int j = 0; j < k; ++j)  // those before it are in (a dropped one stays out)
    if (!parts_[j].dropped) parts_[j].in = true, parts_[j].got = std::max(parts_[j].got, parts_[j].part.bytes);
  at_ = k, parts_[k].begun = true, words_.clear(), note_.clear();
  samples_.clear(), first_byte_ = -1, last_change_ = now;
}

void Job::sample(double now) {
  const uint64_t g = got();
  if (!samples_.empty() && samples_.back().got != g) {
    if (first_byte_ < 0) first_byte_ = now;
    last_change_ = now;
  }
  samples_.push_back({now, g});
  while (samples_.size() > 2 && samples_[1].t <= now - window()) samples_.erase(samples_.begin());
}

void Job::bytes(const std::string& key, uint64_t got, double now) {
  const int k = find(key);
  if (k < 0 || parts_[k].dropped) return;
  begin(k, now);
  words_.clear();
  parts_[k].got = got;
  sample(now);
}

void Job::stage(const std::string& key, const std::string& words, double fraction, double now) {
  const int k = find(key);
  if (k < 0 || parts_[k].dropped) return;
  begin(k, now);
  words_ = words;
  if (parts_[k].part.setup) parts_[k].setup_fraction = std::clamp(fraction, 0.0, 1.0);
  else if (parts_[k].part.bytes) parts_[k].got = parts_[k].part.bytes;  // past its bytes: all in
  sample(now);
}

void Job::drop(const std::string& key, const std::string& why) {
  const int k = find(key);
  if (k < 0 || parts_[k].dropped) return;
  parts_[k].dropped = true, parts_[k].got = 0;
  note_ = why, samples_.clear(), first_byte_ = -1;
}

void Job::done(const std::string& key) {
  const int k = find(key);
  if (k < 0 || parts_[k].dropped) return;
  parts_[k].in = true, parts_[k].got = std::max(parts_[k].got, parts_[k].part.bytes), parts_[k].setup_fraction = 1;
}

uint64_t Job::total() const {
  uint64_t t = 0;
  for (const State& s : parts_)
    if (!s.dropped && !s.part.setup) t += s.part.bytes;
  return t;
}

uint64_t Job::got() const {
  uint64_t g = 0;
  for (const State& s : parts_)
    if (!s.dropped && !s.part.setup) g += s.part.bytes ? std::min(s.got, s.part.bytes) : s.got;
  return g;
}

View Job::view(double now) const {
  View v;
  // The bar: the bytes, then a setup's share by its own fractions. Sizes not known: by the parts.
  bool any_setup = false, all_known = true;
  int downloads = 0, downloads_in = 0;
  double setup = 0;
  for (const State& s : parts_) {
    if (s.dropped) continue;
    if (s.part.setup) {
      any_setup = true, setup = s.in ? 1.0 : s.setup_fraction;
      continue;
    }
    ++downloads, downloads_in += s.in || (s.part.bytes && s.got >= s.part.bytes);
    all_known = all_known && s.part.bytes;
  }
  const double share = any_setup ? 1.0 - setup_share() : 1.0;
  const uint64_t t = total(), g = got();
  const double bytes_part = all_known && t ? static_cast<double>(g) / static_cast<double>(t)
                                           : downloads ? static_cast<double>(downloads_in) / downloads : 1.0;
  v.fraction = std::clamp(share * bytes_part + (any_setup ? setup_share() * setup : 0.0), 0.0, 1.0);

  const State* cur = at_ >= 0 ? &parts_[at_] : nullptr;
  if (cur && cur->dropped) cur = nullptr;
  // The line: past the bytes, the stage's words; while they come, how much, how fast, how long.
  if (!words_.empty() || (cur && cur->part.setup)) {
    v.line = name_ + ": " + (words_.empty() ? std::string("setting up") : words_);
  } else {
    v.line = name_ + ": " + (all_known && t ? mb(g).substr(0, mb(g).size() - 3) + " of " + mb(t) : mb(g));
    if (all_known && t) {
      if (last_change_ >= 0 && now - last_change_ >= stall()) {
        v.line += std::string(kDot) + "waiting for the server...";
      } else if (first_byte_ < 0 || now - first_byte_ < settle() || samples_.size() < 2 || samples_.back().t - samples_.front().t <= 0) {
        v.line += std::string(kDot) + "starting...";
      } else {
        const double dt = samples_.back().t - samples_.front().t;
        const double rate = static_cast<double>(samples_.back().got - samples_.front().got) / dt;
        if (rate <= 0) v.line += std::string(kDot) + "starting...";
        else v.line += kDot + speed_words(rate) + kDot + left_words(static_cast<double>(t - g) / rate);
      }
    } else if (last_change_ >= 0 && now - last_change_ >= stall()) {
      v.line += std::string(kDot) + "waiting for the server...";
    }
  }
  // The smaller line: the part, its size; a dropped part's why until the next begins.
  if (!note_.empty()) v.part = note_;
  else if (cur && cur->part.setup) v.part = cur->part.words.empty() ? "setting up" : cur->part.words;
  else if (cur) v.part = cur->part.words + (cur->part.bytes ? " (" + mb(cur->part.bytes) + ")" : std::string());
  return v;
}

}  // namespace sdlfe::job
