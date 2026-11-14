// Ours, not original code: running mw3import, the movies' converter (docs/fmv.md,
// The imported movies), shared by the launcher's install work and mw3's
// first-run fallback (first_run_import.h). Not on Windows, whose helper plays
// the AVIs as they are.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

struct SDL_IOStream;

namespace sdlfe {

// mw3import(.exe) beside the program (in a Mac app, Contents/MacOS); "" if absent.
std::string importer_path();

// Every folder of `source` named video in any case (on a case-sensitive disk an install's VIDEO
// and the converted movies' video can sit side by side; zutl::any_case would take video alone).
std::vector<std::string> video_dirs(const std::string& source);

// The AVIs in `source`'s video\ folders with no .fmv yet in the write dir's video\.
int movies_missing(const std::string& source, const std::string& write_dir);

// A child's output, read as it comes: SDL's process pipes are non-blocking, so a read with
// nothing there yet returns 0 with SDL_IO_STATUS_NOT_READY. This waits instead and returns 0
// only at the end (or an error). Before 1.1.2 the importer's readers took the first such 0
// for the end, and then sat in SDL_WaitProcess: the bar stopped where it was until the import
// was over, and CANCEL did nothing (seen on Arch Linux; docs/linux.md).
size_t read_child(SDL_IOStream* io, void* buf, size_t n);

// mw3import for `source`'s missing movies, waited for here. `progress` hears
// each "progress K N name" line (done, total, the movie) and stops the import
// by returning false. 0 when every movie was made, 1 stopped, -1 when it
// could not start or failed.
int run_importer(const std::string& source, const std::string& write_dir,
                 const std::function<bool(int done, int total, const std::string& name)>& progress);

}  // namespace sdlfe
