// mw3soundcheck: plays one of the game's sounds through the ported zSound on
// SDL3 audio, to hear that the chain works (docs/sound.md section 7).
//
//   build/mw3soundcheck [sound] [set] [lod]     default: menu_select MENU 2
//
// Starts zSound as the game does (init, sounds.zrd, the set), plays the sound,
// waits for it to end (at most 5 s), then shuts everything down.
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "frontend/sdl/sdl_sound.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_path.h"

int main(int argc, char** argv) {
  const char* name = argc > 1 ? argv[1] : "menu_select";
  const char* set = argc > 2 ? argv[2] : "MENU";
  static int32_t lod = argc > 3 ? std::atoi(argv[3]) : 2;
  if (!SDL_Init(SDL_INIT_AUDIO)) return std::fprintf(stderr, "SDL: %s\n", SDL_GetError()), 1;
  zutl::mount(MW3_GAME_DIR);
  zrdr::init();
  if (!zrdr::open_archive("zbd\\reader.zbd", true)) return std::fprintf(stderr, "no reader.zbd\n"), 1;
  sdlfe::SdlSoundOutput out;
  zsnd::hooks().output = &out;
  zsnd::hooks().find_var = [](const char* n) -> void* { return std::strcmp(n, "SoundLOD") ? nullptr : &lod; };
  zsnd::init(1);
  if (!zsnd::load(1, "sounds.zrd", nullptr, 0)) return std::fprintf(stderr, "sounds.zrd did not load\n"), 1;
  zsnd::load_set(set);
  zsnd::Object* s = zsnd::find(name);
  if (!s) return std::fprintf(stderr, "no sound %s in set %s\n", name, set), 1;
  zsnd::VoiceBase* v = zsnd::play(s, 1.0f);
  std::printf("playing %s: %s\n", name, v ? "yes" : "no");
  int ms = 0;
  for (; v && ms < 5000 && zsnd::status(v) == 1; ms += 20) SDL_Delay(20);
  std::printf("sounded for about %d ms\n", ms);
  std::printf("buffers still sounding: %d\n", out.playing_count());
  zsnd::close_device(nullptr);
  zsnd::shutdown();
  SDL_Quit();
  return 0;
}
