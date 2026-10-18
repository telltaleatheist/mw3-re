cmake_minimum_required(VERSION 3.20)
project(mw3re LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The rebuilt game and engine. No platform or engine types in here: the front end
# (Godot first, SDL as a fallback) talks to it through src/platform.
add_library(mw3core STATIC
  src/crt/msvc_rand.cpp
  src/gamez/zerror/zerr.cpp
  src/gamez/zutil/ztimer.cpp
  src/gamez/zgame/zgameapp.cpp
  src/gamez/zsound/zsnd_cd.cpp
  src/gamez/zrender/zrndr_span.cpp
  src/mech3/music.cpp
)
target_include_directories(mw3core PUBLIC src)

# A C interface over a few ported functions, for the differential tests that run
# the original x86 code in an emulator and compare (tests/differential).
add_library(mw3probe SHARED tests/probe.cpp)
target_link_libraries(mw3probe PRIVATE mw3core)

enable_testing()
add_executable(mw3tests
  tests/main.cpp
  tests/test_msvc_rand.cpp
  tests/test_ztimer.cpp
  tests/test_zgameapp.cpp
  tests/test_zsnd_cd.cpp
  tests/test_zrndr_span.cpp
)
target_link_libraries(mw3tests PRIVATE mw3core)
add_test(NAME mw3tests COMMAND mw3tests)
