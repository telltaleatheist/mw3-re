// Ours: see launcher_ui.h.
#include "frontend/sdl/launcher_ui.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "frontend/sdl/text_fit.h"

namespace sdlfe {
namespace {
namespace fs = std::filesystem;

constexpr float kW = 854.0f, kH = 480.0f;  // the layout's canvas: dp's size is the window's share of it

// ---- pictures ----

// The picture's lit part (brighter than `thresh`), cropped; with `alpha`, its near-black made see-through (the
// generated art is on black).
SDL_Surface* cut_out(SDL_Surface* s, int thresh, bool alpha) {
  if (!s) return nullptr;
  int x0 = s->w, y0 = s->h, x1 = -1, y1 = -1;
  for (int y = 0; y < s->h; ++y) {
    const uint32_t* row = reinterpret_cast<const uint32_t*>(static_cast<uint8_t*>(s->pixels) + y * s->pitch);
    for (int x = 0; x < s->w; ++x) {
      const uint32_t p = row[x];
      const int lum = std::max({int(p >> 16 & 0xff), int(p >> 8 & 0xff), int(p & 0xff)});
      if (lum > thresh) x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
    }
  }
  if (x1 < x0) return s;
  SDL_Surface* c = SDL_CreateSurface(x1 - x0 + 1, y1 - y0 + 1, SDL_PIXELFORMAT_ARGB8888);
  if (!c) return s;
  const SDL_Rect r{x0, y0, c->w, c->h};
  SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
  SDL_BlitSurface(s, &r, c, nullptr);
  SDL_DestroySurface(s);
  for (int y = 0; alpha && y < c->h; ++y) {
    uint32_t* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(c->pixels) + y * c->pitch);
    for (int x = 0; x < c->w; ++x) {
      const uint32_t p = row[x];
      const int lum = std::max({int(p >> 16 & 0xff), int(p >> 8 & 0xff), int(p & 0xff)});
      row[x] = (p & 0xffffffu) | (static_cast<uint32_t>(std::clamp((lum - 18) * 255 / 42, 0, 255)) << 24);
    }
  }
  return c;
}
// Whether a picture has see-through pixels of its own (a cut-out badge).
bool has_alpha(SDL_Surface* s) {
  for (int y = 0; y < s->h; ++y) {
    const uint32_t* row = reinterpret_cast<const uint32_t*>(static_cast<uint8_t*>(s->pixels) + y * s->pitch);
    for (int x = 0; x < s->w; ++x)
      if ((row[x] >> 24) != 0xff) return true;
  }
  return false;
}

// A picture of assets/launcher by its name, ready to draw: the mark cropped, black made see-through
// where the picture has none of its own.
SDL_Surface* picture(const std::string& dir, const std::string& name) {
  SDL_Surface* raw = SDL_LoadPNG((fs::path(dir) / name).string().c_str());
  if (!raw) return nullptr;
  SDL_Surface* s = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
  SDL_DestroySurface(raw);
  if (!s) return nullptr;
  if (name == "emblem.png" || (name.rfind("badge_", 0) == 0 && !has_alpha(s))) return cut_out(s, 24, true);
  return s;
}

// ---- RmlUi's interfaces ----

struct System : Rml::SystemInterface {
  Uint64 start = SDL_GetTicksNS();
  double GetElapsedTime() override { return static_cast<double>(SDL_GetTicksNS() - start) / 1e9; }
  void JoinPath(Rml::String& out, const Rml::String&, const Rml::String& path) override { out = path; }
  bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
    if (type <= Rml::Log::LT_WARNING) std::fprintf(stderr, "mw3: launcher: ui: %s\n", message.c_str());
    return true;
  }
};

// The document and its styles by their names, out of assets/launcher/ui.
struct Files : Rml::FileInterface {
  std::string dir;
  Rml::FileHandle Open(const Rml::String& path) override {
    return reinterpret_cast<Rml::FileHandle>(SDL_IOFromFile((fs::path(dir) / "ui" / path).string().c_str(), "rb"));
  }
  static SDL_IOStream* io(Rml::FileHandle f) { return reinterpret_cast<SDL_IOStream*>(f); }
  void Close(Rml::FileHandle f) override { SDL_CloseIO(io(f)); }
  size_t Read(void* buffer, size_t size, Rml::FileHandle f) override { return SDL_ReadIO(io(f), buffer, size); }
  bool Seek(Rml::FileHandle f, long offset, int origin) override {
    const SDL_IOWhence w = origin == SEEK_SET ? SDL_IO_SEEK_SET : origin == SEEK_CUR ? SDL_IO_SEEK_CUR : SDL_IO_SEEK_END;
    return SDL_SeekIO(io(f), offset, w) >= 0;
  }
  size_t Tell(Rml::FileHandle f) override { return static_cast<size_t>(std::max<Sint64>(0, SDL_TellIO(io(f)))); }
};

// Drawn by an SDL_Renderer (after RmlUi's own SDL backend, MIT): textured triangles, premultiplied alpha, a clip.
struct Renderer : Rml::RenderInterface {
  SDL_Renderer* ren = nullptr;
  std::string dir, art;  // art: LauncherUi::art_from
  float scale = 1.0f;  // pixels in a dp: a picture's "?w" is in dp
  struct Geometry {
    Rml::Span<const Rml::Vertex> v;
    Rml::Span<const int> i;
  };
  std::vector<SDL_Vertex> vs;
  SDL_Rect clip{};
  bool clipping = false, moved = false;
  Rml::Matrix4f move;
  bool straight = false;  // SDL's software renderer has no premultiplied triangles: straight alpha there

  Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> v, Rml::Span<const int> i) override {
    return reinterpret_cast<Rml::CompiledGeometryHandle>(new Geometry{v, i});
  }
  void ReleaseGeometry(Rml::CompiledGeometryHandle g) override { delete reinterpret_cast<Geometry*>(g); }
  void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f t, Rml::TextureHandle texture) override {
    const Geometry& g = *reinterpret_cast<Geometry*>(handle);
    vs.resize(g.v.size());
    for (size_t k = 0; k < vs.size(); ++k) {
      const Rml::Vertex& in = g.v[k];
      Rml::Vector2f p = in.position + t;
      if (moved) {
        const Rml::Vector4f q = move * Rml::Vector4f(p.x, p.y, 0, 1);
        p = straight ? Rml::Vector2f(std::round(q.x), std::round(q.y)) : Rml::Vector2f(q.x, q.y);
      }
      vs[k].position = {p.x, p.y};
      vs[k].tex_coord = {in.tex_coord.x, in.tex_coord.y};
      const float a = in.colour.alpha / 255.0f, k1 = straight && in.colour.alpha ? 1.0f / in.colour.alpha : 1.0f / 255.0f;
      vs[k].color = {in.colour.red * k1, in.colour.green * k1, in.colour.blue * k1, a};
    }
    if (straight && texture && vs.size() == 4 && picture_quad(reinterpret_cast<SDL_Texture*>(texture))) return;
    SDL_RenderGeometry(ren, reinterpret_cast<SDL_Texture*>(texture), vs.data(), static_cast<int>(vs.size()), g.i.data(),
                       static_cast<int>(g.i.size()));
  }
  // A whole picture drawn upright, by the software renderer: as a copy (its triangles overflow at the stars' size).
  bool picture_quad(SDL_Texture* t) {
    const SDL_FPoint p = vs[0].position, q = vs[2].position, u = vs[0].tex_coord, w = vs[2].tex_coord;
    if (vs[1].position.y != p.y || vs[1].position.x != q.x || vs[3].position.x != p.x || vs[3].position.y != q.y) return false;
    const SDL_FRect src{u.x * t->w, u.y * t->h, (w.x - u.x) * t->w, (w.y - u.y) * t->h}, dst{p.x, p.y, q.x - p.x, q.y - p.y};
    SDL_SetTextureColorModFloat(t, vs[0].color.r, vs[0].color.g, vs[0].color.b);
    SDL_SetTextureAlphaModFloat(t, vs[0].color.a);
    const bool ok = SDL_RenderTexture(ren, t, &src, &dst);
    SDL_SetTextureColorModFloat(t, 1, 1, 1), SDL_SetTextureAlphaModFloat(t, 1);
    return ok;
  }
  void EnableScissorRegion(bool on) override { clipping = on, SDL_SetRenderClipRect(ren, on ? &clip : nullptr); }
  void SetScissorRegion(Rml::Rectanglei r) override {
    clip = {r.Left(), r.Top(), r.Width(), r.Height()};
    if (clipping) SDL_SetRenderClipRect(ren, &clip);
  }
  void SetTransform(const Rml::Matrix4f* m) override {
    moved = m != nullptr;
    if (m) move = *m;
  }
  // "name.png", or "name.png?w": `w` dp across, shrunk by halves here so a big badge does not sparkle.
  Rml::TextureHandle LoadTexture(Rml::Vector2i& size, const Rml::String& source) override {
    const size_t q = source.find('?');
    SDL_Surface* s = picture(dir, source.substr(0, q));
    if (!s && !art.empty()) s = picture(art, source.substr(0, q));
    if (!s) return std::printf("mw3: launcher: picture %s not found\n", source.substr(0, q).c_str()), Rml::TextureHandle{};
    uint8_t* row = static_cast<uint8_t*>(s->pixels);
    for (int y = 0; y < s->h && !straight; ++y, row += s->pitch)
      for (int x = 0; x < s->w; ++x) {
        uint32_t& p = reinterpret_cast<uint32_t*>(row)[x];
        const uint32_t a = p >> 24;
        p = a << 24 | (p >> 16 & 0xff) * a / 255 << 16 | (p >> 8 & 0xff) * a / 255 << 8 | (p & 0xff) * a / 255;
      }
    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
    auto scaled = [&](int w, int h) {
      SDL_Surface* r = SDL_ScaleSurface(s, w, h, SDL_SCALEMODE_LINEAR);
      if (r) SDL_DestroySurface(s), s = r, SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
      return r != nullptr;
    };
    if (q != std::string::npos) {
      const int want = std::max(1, static_cast<int>(std::lround(std::atof(source.c_str() + q + 1) * scale)));
      const float tall = static_cast<float>(s->h) / static_cast<float>(s->w);
      while (s->w >= want * 2 && scaled(s->w / 2, std::max(1, s->h / 2))) {
      }
      if (s->w > want) scaled(want, std::max(1, static_cast<int>(std::lround(static_cast<float>(want) * tall))));
    }
    size = {s->w, s->h};
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    return blended(t);
  }
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i size) override {
    SDL_Surface* s = SDL_CreateSurfaceFrom(size.x, size.y, SDL_PIXELFORMAT_RGBA32,
                                           const_cast<Rml::byte*>(source.data()), size.x * 4);
    SDL_Surface* own = straight ? SDL_DuplicateSurface(s) : nullptr;
    for (int y = 0; own && y < own->h; ++y) {
      uint8_t* px = static_cast<uint8_t*>(own->pixels) + y * own->pitch;
      for (int x = 0; x < own->w; ++x, px += 4)
        for (int c = 0; c < 3 && px[3]; ++c) px[c] = static_cast<uint8_t>(std::min(255, px[c] * 255 / px[3]));
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, own ? own : s);
    if (own) SDL_DestroySurface(own);
    SDL_DestroySurface(s);
    return blended(t);
  }
  SDL_BlendMode blend() const { return straight ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_BLEND_PREMULTIPLIED; }
  Rml::TextureHandle blended(SDL_Texture* t) {
    if (t) SDL_SetTextureBlendMode(t, blend()), SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    return reinterpret_cast<Rml::TextureHandle>(t);
  }
  void ReleaseTexture(Rml::TextureHandle t) override { SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(t)); }
};

// A button's markup: the one place it is written.
Rml::String markup(const UiButton& b) {
  auto enc = [](const std::string& s) { return Rml::StringUtilities::EncodeRml(s); };
  std::string s = "<btn i=\"" + std::to_string(b.i) + "\" class=\"" + b.kind + (b.on ? "" : " off") + (b.sub.empty() ? "" : " two") + "\">";
  if (b.kind == "gear") return s + "<img src=\"cog.png?21\"/></btn>";
  if (b.kind == "seg") return s + "<div class=\"divider\"/><img src=\"download.png?17\"/><div class=\"word\">UPDATE</div></btn>";
  s += "<fit class=\"label\" least=\"" + std::string(b.kind == "plate" ? "13" : "11") + "\" keep=\"3\" text=\"" + enc(b.label) + "\"/>";
  if (!b.sub.empty()) s += "<fit class=\"sub\" least=\"10\" keep=\"" + std::to_string(b.keep) + "\" text=\"" + enc(b.sub) + "\"/>";
  return s + "</btn>";
}
}  // namespace

struct LauncherUi::Impl {
  System system;
  Files files;
  Renderer renderer;
  bool up = false;
  Rml::Context* context = nullptr;
  Rml::ElementDocument* doc = nullptr;
  Rml::DataModelHandle model;
  UiModel m, given;  // `m` as drawn (widen)
  int hot = -2;
  bool filled = false, rescaled = false;
  std::string shown;  // the screen last laid out
  float list_at = 0;

  Rml::FontFaceHandle face(const std::string& family, int size) {
    return Rml::GetFontEngineInterface()->GetFontFaceHandle(family, Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, size);
  }
  static int raw_width(Rml::FontFaceHandle f, const std::string& s) {
    static const Rml::String lang;
    return f ? Rml::GetFontEngineInterface()->GetStringWidth(f, s, Rml::TextShapingContext{lang}) : 0;
  }
  static int measure(Rml::FontFaceHandle f, const std::string& s) { return raw_width(f, wide(f, s)); }
  // Small letters (under 24 pixels) whose space is under a fifth of their size ran together: the space is drawn and
  // measured as two, the first one that does not break a line. Larger ones have the font's own.
  static std::string wide(Rml::FontFaceHandle f, const std::string& s) {
    const int size = f ? Rml::GetFontEngineInterface()->GetFontMetrics(f).size : 0;
    return size > 0 && size < 24 && raw_width(f, " ") * 5 < size ? wide(s) : s;
  }
  static std::string wide(const std::string& s) {
    std::string out;
    for (char c : s) out += c == ' ' ? "\xc2\xa0 " : std::string(1, c);
    return out;
  }
  void widen(UiModel& m) {
    auto w = [&](std::string& s, int size) { s = wide(face("title", static_cast<int>(static_cast<float>(size) * renderer.scale + 0.5f)), s); };
    // The words' size on each screen (launcher.rcss).
    const bool small = m.screen == "find" || m.screen == "update" || m.screen == "copies";
    w(m.words, small ? 13 : m.variant == "found" || m.variant == "place" ? 14 : m.variant == "nomusic" ? 15 : 16), w(m.path, 14), w(m.status, m.screen == "main" ? 12 : 13), w(m.package, 12);
    w(m.stage, 13), w(m.message, 10), w(m.picker, 13);
    for (UiLine& l : m.lines) w(l.text, 13);
    for (std::string& n : m.notes) w(n, m.screen == "update" ? 13 : m.notes_size);
  }
  bool bind() {
    Rml::DataModelConstructor c = context->CreateDataModel("launcher");
    if (!c) return false;
    if (auto h = c.RegisterStruct<UiLine>()) h.RegisterMember("text", &UiLine::text), h.RegisterMember("kind", &UiLine::kind), h.RegisterMember("keep", &UiLine::keep);
    c.RegisterArray<std::vector<UiLine>>();
    c.RegisterArray<std::vector<std::string>>();
    if (auto h = c.RegisterStruct<UiPanel>()) {
      h.RegisterMember("badge", &UiPanel::badge), h.RegisterMember("title", &UiPanel::title), h.RegisterMember("on", &UiPanel::on);
      h.RegisterMember("lines", &UiPanel::lines), h.RegisterMember("buttons", &UiPanel::buttons);
      h.RegisterMember("section", &UiPanel::section);
    }
    c.RegisterArray<std::vector<UiPanel>>();
    const std::pair<const char*, std::string*> strings[] = {
        {"screen", &m.screen}, {"head", &m.head}, {"heading", &m.heading}, {"heading_kind", &m.heading_kind}, {"game", &m.game}, {"badge", &m.badge},
        {"title", &m.title}, {"words", &m.words}, {"text", &m.text}, {"path", &m.path}, {"status", &m.status},
        {"status_kind", &m.status_kind}, {"package", &m.package}, {"stage", &m.stage}, {"message", &m.message},
        {"percent", &m.percent}, {"picker", &m.picker}, {"variant", &m.variant}};
    for (const auto& [name, s] : strings) c.Bind(name, s);
    c.Bind("fraction", &m.fraction);
    c.Bind("notes_size", &m.notes_size), c.Bind("notes_width", &m.notes_width);
    c.Bind("notes_gap", &m.notes_gap), c.Bind("notes_line", &m.notes_line);
    c.Bind("lines", &m.lines), c.Bind("notes", &m.notes), c.Bind("panels", &m.panels), c.Bind("rows", &m.rows);
    c.Bind("empty", &m.empty);
    model = c.GetModelHandle();
    return true;
  }
  // The button groups' places filled, when what they hold changed.
  void fill() {
    Rml::ElementList places;
    doc->QuerySelectorAll(places, "[buttons]");
    for (Rml::Element* e : places) {
      const std::string group = e->GetAttribute<Rml::String>("buttons", "");
      std::string rml;
      if (auto it = m.buttons.find(group); it != m.buttons.end())
        for (const UiButton& b : it->second) rml = b.kind == "gear" ? markup(b) + rml : rml + markup(b);  // a gear is left of its PLAY
      if (e->GetAttribute<Rml::String>("filled", "") == rml && e->GetNumChildren() > 0) continue;
      e->SetInnerRML(rml);
      e->SetAttribute("filled", rml);
      e->SetClass("empty", rml.empty());
    }
  }
  // Every <fit>'s words inside its width, at its parent's size or smaller down to `least` (dp), then cut in the
  // middle, its first `keep` characters and its end kept.
  bool fit() {
    bool changed = false;
    Rml::ElementList all;
    doc->QuerySelectorAll(all, "fit");
    const float dp = context->GetDensityIndependentPixelRatio();
    for (Rml::Element* e : all) {
      const int room = static_cast<int>(e->GetBox().GetSize().x + 0.5f);
      const std::string text = e->GetAttribute<Rml::String>("text", "");
      if (!e->IsVisible(true) || (room <= 0 && !text.empty())) continue;
      const std::string key = std::to_string(room) + "|" + text;
      if (e->GetAttribute<Rml::String>("done", "") == key) continue;
      const auto& cv = e->GetComputedValues();
      // The size its styles give it (not one set below), read once a window size: a smaller one set is taken off first.
      if (const float had = e->GetAttribute<float>("dp", 0.0f); had != dp) {
        e->SetAttribute("dp", dp);
        if (had > 0.0f) {
          e->RemoveProperty(Rml::PropertyId::FontSize), e->RemoveAttribute("base"), e->RemoveAttribute("done");
          changed = true;
          continue;
        }
      }
      float base = e->GetAttribute<float>("base", 0.0f);
      if (base <= 0.0f) e->SetAttribute("base", base = cv.font_size());
      const int big = static_cast<int>(base + 0.5f);
      const int least = std::min(big, static_cast<int>(e->GetAttribute<float>("least", base / dp) * dp + 0.5f));
      int size = big;
      while (size > least && measure(face(cv.font_family(), size), text) > room) --size;
      std::string shown = text;
      Rml::FontFaceHandle f = face(cv.font_family(), size);
      if (measure(f, shown) > room)
        shown = fit_middle(shown, static_cast<size_t>(e->GetAttribute<int>("keep", 0)), room, [&](const std::string& s) { return measure(f, s); });
      if (size != big) e->SetProperty(Rml::PropertyId::FontSize, Rml::Property(static_cast<float>(size), Rml::Unit::PX));
      else e->RemoveProperty(Rml::PropertyId::FontSize);
      e->SetInnerRML(Rml::StringUtilities::EncodeRml(wide(f, shown)));
      e->SetAttribute("done", key);
      changed = true;
    }
    return changed;
  }
  void light() {
    Rml::ElementList all;
    doc->QuerySelectorAll(all, "btn, .panel");
    for (Rml::Element* e : all) e->SetClass("hot", false);
    if (hot < 0) return;
    for (Rml::Element* b : all) {  // a group may have a place on more than one screen
      if (b->GetTagName() != "btn" || b->GetAttribute<int>("i", -1) != hot || b->IsClassSet("off")) continue;
      b->SetClass("hot", true);
      if (Rml::Element* p = b->Closest(".panel")) p->SetClass("hot", true);
    }
  }
  Rml::Element* button(int i) { return doc ? doc->QuerySelector("btn[i=\"" + std::to_string(i) + "\"]") : nullptr; }
};

LauncherUi::LauncherUi() : p_(std::make_unique<Impl>()) {}
LauncherUi::~LauncherUi() {
  detach();
  if (p_->up) Rml::Shutdown();
}

void LauncherUi::art_from(const std::string& dir) { p_->renderer.art = dir; }

bool LauncherUi::init(const std::string& dir, const std::string& font, const std::string& body_font) {
  p_->files.dir = dir, p_->renderer.dir = dir;
  Rml::SetSystemInterface(&p_->system);
  Rml::SetFileInterface(&p_->files);
  if (!(p_->up = Rml::Initialise())) return false;
  // Paths as they are: the file interface above reads only the document's folder.
  auto load = [](const std::string& path, const char* family, std::vector<Rml::byte>& data) {
    size_t n = 0;
    void* d = path.empty() ? nullptr : SDL_LoadFile(path.c_str(), &n);
    if (!d) return;
    data.assign(static_cast<Rml::byte*>(d), static_cast<Rml::byte*>(d) + n);
    SDL_free(d);
    Rml::LoadFontFace(Rml::Span<const Rml::byte>(data.data(), data.size()), family, Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal);
  };
  static std::vector<Rml::byte> data[2];  // RmlUi reads them until its Shutdown
  load(font, "title", data[0]);
  load(body_font, "body", data[1]);
  return true;
}

bool LauncherUi::attach(SDL_Renderer* ren) {
  if (!p_->up) return false;
  p_->renderer.ren = ren;
  const char* name = SDL_GetRendererName(ren);
  p_->renderer.straight = name && std::strcmp(name, SDL_SOFTWARE_RENDERER) == 0;
  int w = static_cast<int>(kW), h = static_cast<int>(kH);
  SDL_GetCurrentRenderOutputSize(ren, &w, &h);
  p_->context = Rml::CreateContext("launcher", {w, h}, &p_->renderer);
  if (!p_->context || !p_->bind()) return false;
  update();
  return reload();
}

bool LauncherUi::reload() {
  if (!p_->context) return false;
  if (p_->doc) p_->doc->Close(), p_->context->Update();
  Rml::Factory::ClearStyleSheetCache();
  p_->doc = p_->context->LoadDocument("launcher.rml");
  p_->hot = -2, p_->filled = false;
  if (!p_->doc) return false;
  p_->doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
  return true;
}

void LauncherUi::detach() {
  if (p_->context) Rml::RemoveContext("launcher");
  p_->context = nullptr, p_->doc = nullptr;
  if (p_->up) Rml::ReleaseRenderManagers();
}
bool LauncherUi::ready() const { return p_->doc != nullptr; }

int LauncherUi::width(const std::string& s, int size, bool body) {
  return p_->up ? Impl::measure(p_->face(body ? "body" : "title", size), s) : 0;
}
int LauncherUi::line_skip(int size, bool body) {
  Rml::FontFaceHandle f = p_->up ? p_->face(body ? "body" : "title", size) : 0;
  return f ? static_cast<int>(std::lround(Rml::GetFontEngineInterface()->GetFontMetrics(f).line_spacing)) : 0;
}

void LauncherUi::show(const UiModel& m, int hot) {
  if (!p_->doc) return;
  if (p_->rescaled || !p_->filled || !(m == p_->given)) {
    p_->rescaled = false;
    p_->given = p_->m = m;
    p_->widen(p_->m);
    p_->model.DirtyAllVariables();
    p_->filled = false;
  }
  p_->hot = hot;
}

bool LauncherUi::update() {
  if (!p_->context) return false;
  int w = 0, h = 0;
  if (SDL_GetCurrentRenderOutputSize(p_->renderer.ren, &w, &h) && w > 0 && h > 0) {
    const float dp = std::min(static_cast<float>(w) / kW, static_cast<float>(h) / kH);
    if (dp != p_->renderer.scale) {
      p_->renderer.scale = dp;
      Rml::ReleaseTextures(&p_->renderer);  // the pictures are scaled to it, the narrow spaces chosen by it
      p_->rescaled = true;
    }
    p_->context->SetDensityIndependentPixelRatio(dp);
    if (p_->context->GetDimensions() != Rml::Vector2i(w, h)) p_->context->SetDimensions({w, h});
  }
  // The list's place kept while another screen is up.
  if (Rml::Element* list = p_->doc ? p_->doc->GetElementById("list") : nullptr; list && p_->shown == "main") p_->list_at = list->GetScrollTop();
  const bool back = p_->m.screen == "main" && p_->shown != "main";
  p_->shown = p_->m.screen;
  p_->context->Update();
  if (!p_->doc) return false;
  if (!p_->filled) {
    p_->fill(), p_->filled = true;
    p_->context->Update();
  }
  p_->light();
  for (int k = 0; k < 3 && p_->fit(); ++k) p_->context->Update();
  if (Rml::Element* list = p_->doc->GetElementById("list")) {
    if (back && p_->list_at > 0) list->SetScrollTop(p_->list_at), p_->context->Update();
    const float most = list->GetScrollHeight() - list->GetClientHeight(), at = list->GetScrollTop();
    for (const auto& [id, on] : {std::pair<const char*, bool>{"more_up", at > 0.5f}, {"more_down", at < most - 0.5f}})
      if (Rml::Element* e = p_->doc->GetElementById(id)) e->SetClass("on", on);
  }
  if (Rml::Element* n = p_->doc->GetElementById("update_notes"))
    if (Rml::Element* e = p_->doc->GetElementById("update_more"))
      e->SetClass("on", n->GetScrollTop() < n->GetScrollHeight() - n->GetClientHeight() - 0.5f);
  return p_->rescaled;
}

void LauncherUi::render() {
  SDL_Renderer* r = p_->renderer.ren;
  if (!r) return;
  SDL_SetRenderDrawColor(r, 8, 8, 12, 255);
  SDL_RenderClear(r);
  SDL_SetRenderDrawBlendMode(r, p_->renderer.blend());
  if (p_->context) p_->context->Render();
  SDL_SetRenderClipRect(r, nullptr);
}

void LauncherUi::mouse_move(float x, float y) {
  if (p_->context) p_->context->ProcessMouseMove(static_cast<int>(x), static_cast<int>(y), 0);
}
void LauncherUi::mouse_button(bool down) {
  if (!p_->context) return;
  if (down) p_->context->ProcessMouseButtonDown(0, 0);
  else p_->context->ProcessMouseButtonUp(0, 0);
}
void LauncherUi::mouse_wheel(float notches) {
  if (p_->context) p_->context->ProcessMouseWheel(-notches, 0);
}
void LauncherUi::mouse_leave() {
  if (p_->context) p_->context->ProcessMouseLeave();
}
int LauncherUi::over() const {
  Rml::Element* e = p_->context ? p_->context->GetHoverElement() : nullptr;
  for (; e; e = e->GetParentNode())
    if (e->GetTagName() == "btn") return e->GetAttribute<int>("i", -1);
  return -1;
}
void LauncherUi::into_view(int b) {
  update();
  Rml::Element* e = p_->button(b);
  Rml::Element* panel = e ? e->Closest(".panel") : nullptr;
  if (panel) panel->ScrollIntoView({Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest, Rml::ScrollBehavior::Instant, Rml::ScrollParentage::Closest});
}
void LauncherUi::scroll_list(float y) {
  update();
  if (Rml::Element* list = p_->doc ? p_->doc->GetElementById("list") : nullptr) list->SetScrollTop(y * p_->renderer.scale);
  update();
}

}  // namespace sdlfe
