#pragma once
#include <cmath>

// The game's frame in the window (FIX (B), behind zfix any_resolution;
// resolution.md step 2): the frame (the display rect, 640x480 or a mode's
// size) keeps its shape, as the largest centred rect of its aspect, with
// black bars where the window's shape differs (pillarbox or letterbox).
// Without the fix the frame is stretched over the whole window, as before.
// Pure geometry, no SDL, so the tests check it headless.
namespace sdlfe {

struct FrameShape {
  int frame_w = 640, frame_h = 480;  // the frame the game draws (its render rect)
  bool fit = false;                  // any_resolution: keep the frame's shape
  // The 2D canvas over it (the display rect, resolution.md step 4: the frame
  // over the art scale); 0: the frame's own size. The cursor is in its pixels.
  int canvas_w = 0, canvas_h = 0;
  int client_w() const { return canvas_w > 0 ? canvas_w : frame_w; }
  int client_h() const { return canvas_h > 0 ? canvas_h : frame_h; }
};

struct FitRect {
  float x = 0, y = 0, w = 0, h = 0;
};

// Where the frame sits in a window of win_w x win_h (pixels or points alike).
inline FitRect fit_rect(float win_w, float win_h, const FrameShape& s) {
  if (!s.fit || s.frame_w <= 0 || s.frame_h <= 0 || win_w <= 0 || win_h <= 0) return {0, 0, win_w, win_h};
  const float scale = std::fmin(win_w / s.frame_w, win_h / s.frame_h);
  const float w = std::round(s.frame_w * scale), h = std::round(s.frame_h * scale);
  return {std::floor((win_w - w) / 2), std::floor((win_h - h) / 2), w, h};
}

// A window position as a client (canvas) pixel, and back (the cursor's mapping).
inline void window_to_frame(float wx, float wy, float win_w, float win_h, const FrameShape& s, float* fx, float* fy) {
  const FitRect r = fit_rect(win_w, win_h, s);
  *fx = r.w > 0 ? (wx - r.x) * s.client_w() / r.w : 0;
  *fy = r.h > 0 ? (wy - r.y) * s.client_h() / r.h : 0;
}
inline void frame_to_window(float fx, float fy, float win_w, float win_h, const FrameShape& s, float* wx, float* wy) {
  const FitRect r = fit_rect(win_w, win_h, s);
  *wx = r.x + fx * r.w / s.client_w();
  *wy = r.y + fy * r.h / s.client_h();
}

// The relative mouse's scale: window motion to client pixels, so a mickey is
// a game pixel at any window size (the exe's 640x480 window: one to one).
inline float mickey_scale(float win_w, float win_h, const FrameShape& s) {
  const FitRect r = fit_rect(win_w, win_h, s);
  return r.w > 0 ? s.client_w() / r.w : 1.0f;
}

// The window a frame asks for when --window is not given: the frame itself,
// doubled while it is under 1280 wide (640x480 opens at 1280x960, as before).
inline void default_window(const FrameShape& s, int* w, int* h) {
  int k = 1;
  while (s.frame_w * k < 1280) ++k;
  *w = s.frame_w * k, *h = s.frame_h * k;
}

}  // namespace sdlfe
