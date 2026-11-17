# pico-c — CMake build, mainly for Windows with Visual Studio (MSVC).
#
# The Makefile stays the main build on macOS, Linux and MinGW; this file
# builds the same sources with any compiler CMake knows, which is how pico-c
# is built natively on Windows:
#
#   cmake -S . -B build/msvc -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
#   cmake --build build/msvc --config Release
#
# SDL2 is found with find_package(SDL2 CONFIG), which vcpkg (or the SDL2
# development package for Visual C++, through SDL2_DIR) provides.
#
# Like the Makefile, the configure step clones z8lua into
# platforms/common/deps/z8lua at the pinned revision and applies
# platforms/common/z8lua.patch to it (see platforms/common/README.md).
#
# pico-c의 CMake 빌드. 주로 Windows의 Visual Studio(MSVC)용이다. macOS, Linux,
# MinGW의 주 빌드는 여전히 Makefile이고, 이 파일은 같은 소스를 CMake가 아는 어떤
# 컴파일러로든 빌드한다. SDL2는 find_package(SDL2 CONFIG)로 찾는다(vcpkg 또는
# SDL2_DIR). Makefile처럼 configure 단계가 z8lua를 고정 리비전으로 클론하고
# 패치를 적용한다.

cmake_minimum_required(VERSION 3.21)
project(pico-c LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------ z8lua --
set(Z8LUA_DIR   "${CMAKE_SOURCE_DIR}/platforms/common/deps/z8lua")
set(Z8LUA_REV   "8575dfbc524003c869c0aa05e109819302c576f9")
set(Z8LUA_PATCH "${CMAKE_SOURCE_DIR}/platforms/common/z8lua.patch")

find_package(Git REQUIRED)
if(NOT EXISTS "${Z8LUA_DIR}/.git")
  message(STATUS "cloning z8lua into ${Z8LUA_DIR}")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -c core.autocrlf=false clone --branch pico8
            https://github.com/samhocevar/z8lua.git "${Z8LUA_DIR}"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "could not clone z8lua")
  endif()
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${Z8LUA_DIR}" checkout -q ${Z8LUA_REV}
                  RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "could not check out z8lua ${Z8LUA_REV}")
  endif()
endif()

execute_process(
  COMMAND "${GIT_EXECUTABLE}" -C "${Z8LUA_DIR}" apply --reverse --check "${Z8LUA_PATCH}"
  RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
if(already EQUAL 0)
  message(STATUS "z8lua patch already applied")
else()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${Z8LUA_DIR}" apply "${Z8LUA_PATCH}"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${Z8LUA_PATCH} does not apply to ${Z8LUA_DIR}; reset it with\n"
                        "  git -C ${Z8LUA_DIR} checkout -q ${Z8LUA_REV} -- .")
  endif()
  message(STATUS "z8lua patched")
endif()
# A change of the patch must run the configure step again.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${Z8LUA_PATCH}")

set(Z8LUA_NAMES lapi lcode lctype ldebug ldo ldump
                lfunc lgc llex lmem lobject lopcodes
                lparser lstate lstring ltable ltm lundump
                lvm lzio lauxlib lbaselib lcorolib
                ldblib ltablib lstrlib lpico8lib linit)
set(Z8LUA_SRCS "")
foreach(name ${Z8LUA_NAMES})
  list(APPEND Z8LUA_SRCS "${Z8LUA_DIR}/${name}.c")
endforeach()
# z8lua is C++ code in .c files (z8::fix32 numbers).
set_source_files_properties(${Z8LUA_SRCS} PROPERTIES LANGUAGE CXX)

add_library(z8lua STATIC ${Z8LUA_SRCS})
target_include_directories(z8lua PUBLIC "${Z8LUA_DIR}")

# ----------------------------------------------------------------- pico-c --
file(GLOB PICO_C_SRCS CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*/*.cpp")

find_package(SDL2 CONFIG REQUIRED)

# The sources include <SDL2/SDL.h>; some SDL2 packages only hand out the
# include/SDL2 folder itself, so look for the folder above it too.
# 소스는 <SDL2/SDL.h>를 쓰는데, 어떤 SDL2 패키지는 include/SDL2 폴더만 알려
# 주므로 그 위 폴더도 찾는다.
set(sdl_hints "")
foreach(dir ${SDL2_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR})
  list(APPEND sdl_hints "${dir}" "${dir}/..")
endforeach()
if(TARGET SDL2::SDL2)
  get_target_property(sdl_inc SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
  foreach(dir ${sdl_inc})
    list(APPEND sdl_hints "${dir}" "${dir}/..")
  endforeach()
endif()
find_path(SDL2_HEADER_ROOT SDL2/SDL.h HINTS ${sdl_hints} REQUIRED)

add_executable(pico-c ${PICO_C_SRCS})
target_include_directories(pico-c PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/src/api"
                                          "${SDL2_HEADER_ROOT}")
target_link_libraries(pico-c PRIVATE z8lua)
if(TARGET SDL2::SDL2main)
  target_link_libraries(pico-c PRIVATE SDL2::SDL2main)
endif()
if(TARGET SDL2::SDL2)
  target_link_libraries(pico-c PRIVATE SDL2::SDL2)
else()
  target_link_libraries(pico-c PRIVATE SDL2::SDL2-static)
endif()

if(MSVC)
  # The sources are UTF-8 (the comments, and the glyphs of the charset
  # table); the CRT's "unsafe" warnings are about fopen and friends.
  # 소스는 UTF-8이다(주석과 문자셋 표의 글리프). CRT의 "unsafe" 경고는
  # fopen 등에 대한 것이다.
  foreach(target z8lua pico-c)
    target_compile_options(${target} PRIVATE /utf-8 /W3)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  endforeach()
else()
  target_compile_options(pico-c PRIVATE -Wall -Wextra)
endif()

if(APPLE)
  # Like the Makefile: an ad-hoc signature, without which some macOS setups
  # refuse to start the binary.  Makefile처럼 ad-hoc 서명.
  add_custom_command(TARGET pico-c POST_BUILD
    COMMAND codesign --force --sign - $<TARGET_FILE:pico-c> || true)
endif()

# Next to the executable, so that it runs without setting PATH.
# 실행 파일 옆에 두어 PATH 없이 돌게 한다.
if(WIN32)
  add_custom_command(TARGET pico-c POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:pico-c> $<TARGET_FILE_DIR:pico-c>
    COMMAND_EXPAND_LISTS)
endif()
