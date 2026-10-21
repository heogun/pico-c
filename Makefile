# pico-c — cross-platform build.
#
#   make                     # auto-detect the platform
#   make PLATFORM=macos      # macOS   — SDL2 from MacPorts (/opt/local)
#   make PLATFORM=linux      # Linux   — SDL2 from the system
#   make PLATFORM=windows    # Windows — MSYS2/MinGW + the vendored SDL2
#
# Every artifact lands in build/<platform>/ so the platforms never clobber
# each other.  Third-party sources live next to the platform that uses them:
#
#   platforms/common/deps/z8lua            shared Lua runtime (cloned by make)
#   platforms/macos/deps/                  macOS-only dependencies
#   platforms/linux/deps/                  Linux-only dependencies
#   platforms/windows/deps/SDL2-2.30.11    MinGW SDL2 (prebuilt)
#
# The per-platform helper scripts in platforms/<platform>/ wrap this file.

# The platform is detected from the host, but it can always be given
# explicitly:  make PLATFORM=windows
PLATFORM ?=
ifeq ($(PLATFORM),)
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
  else ifeq ($(UNAME_S),Linux)
    PLATFORM := linux
  else ifneq (,$(filter $(UNAME_S),MINGW% CYGWIN% MSYS%))
    PLATFORM := windows
  else
    $(error Unsupported "uname -s" value: $(UNAME_S) -- pass PLATFORM=<macos|linux|windows>)
  endif
endif

ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

# ---------------------------------------------------------------- toolchain --
ifeq ($(PLATFORM),macos)
  DEFAULT_CXX := clang++
  DEFAULT_AR  := ar
  SDL2_DIR   ?= /opt/local
  SDL_CFLAGS := -I$(SDL2_DIR)/include
  SDL_LIBS   := -L$(SDL2_DIR)/lib -lSDL2 -lSDL2main
  EXE_SUFFIX :=
  CODESIGN   := codesign --force --sign -
else ifeq ($(PLATFORM),linux)
  DEFAULT_CXX := g++
  DEFAULT_AR  := ar
  SDL2_DIR   ?= /usr
  SDL_CFLAGS := -I$(SDL2_DIR)/include
  SDL_LIBS   := -lSDL2
  EXE_SUFFIX :=
  CODESIGN   :=
else ifeq ($(PLATFORM),windows)
  DEFAULT_CXX := x86_64-w64-mingw32-g++
  DEFAULT_AR  := x86_64-w64-mingw32-ar
  SDL2_DIR   ?= $(ROOT)/platforms/windows/deps/SDL2-2.30.11/x86_64-w64-mingw32
  SDL_CFLAGS := -I$(SDL2_DIR)/include -I$(SDL2_DIR)/include/SDL2
  SDL_LIBS   := -L$(SDL2_DIR)/lib -lmingw32 -lSDL2main -lSDL2
  EXE_SUFFIX := .exe
  CODESIGN   :=
else
  $(error Unknown PLATFORM '$(PLATFORM)' (expected macos, linux or windows))
endif

# GNU make predefines CXX and AR; only those defaults are replaced, so the
# compiler can still be chosen from the environment or the command line.
ifeq ($(origin CXX),default)
  CXX := $(DEFAULT_CXX)
endif
ifeq ($(origin AR),default)
  AR := $(DEFAULT_AR)
endif

CXXFLAGS ?= -O2 -Wall -Wextra -std=gnu++17
CXXFLAGS += $(SDL_CFLAGS)
LIBS     += $(SDL_LIBS) -lm

# ------------------------------------------------------------------ layout --
SRC_DIR        := $(ROOT)/src
Z8LUA_DIR      := $(ROOT)/platforms/common/deps/z8lua
BUILD_DIR      := $(ROOT)/build/$(PLATFORM)
OBJ_DIR        := $(BUILD_DIR)/obj
Z8LUA_OBJ_DIR  := $(BUILD_DIR)/z8lua
Z8LUA_A        := $(BUILD_DIR)/libz8lua.a
Z8LUA_STAMP    := $(BUILD_DIR)/.z8lua.stamp
TARGET         := $(BUILD_DIR)/pico-c$(EXE_SUFFIX)
ASAN_TARGET    := $(BUILD_DIR)/pico-c-asan$(EXE_SUFFIX)
TEST_CART      := $(ROOT)/tests/carts/test.p8

CPP_SRCS := $(wildcard $(SRC_DIR)/*.cpp)
CPP_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CPP_SRCS))

Z8LUA_NAMES := lapi lcode lctype ldebug ldo ldump \
               lfunc lgc llex lmem lobject lopcodes \
               lparser lstate lstring ltable ltm lundump \
               lvm lzio lauxlib lbaselib lcorolib \
               ldblib ltablib lstrlib lpico8lib linit
Z8LUA_OBJS := $(addprefix $(Z8LUA_OBJ_DIR)/, $(addsuffix .o, $(Z8LUA_NAMES)))

.PHONY: all clean run asan help

all: $(TARGET)

help:
	@echo "targets: all (default), run, asan, clean"
	@echo "platforms: macos, linux, windows (PLATFORM=<name>)"

$(OBJ_DIR) $(Z8LUA_OBJ_DIR) $(BUILD_DIR):
	mkdir -p $@

# The pico-c sources include lua.h, so the z8lua checkout has to exist before
# they are compiled.  Depending on the stamp guarantees that, even on a tree
# where z8lua has never been cloned.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(Z8LUA_STAMP) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -I$(Z8LUA_DIR) -c -o $@ $<

$(TARGET): $(CPP_OBJS) $(Z8LUA_A)
	$(CXX) $(CXXFLAGS) -o $@ $(CPP_OBJS) $(Z8LUA_A) $(LIBS) $(LDFLAGS)
	@[ -n "$(CODESIGN)" ] && $(CODESIGN) $@ || true

# ------------------------------------------------------------------- z8lua --
$(Z8LUA_STAMP): | $(BUILD_DIR)
	@mkdir -p $(Z8LUA_DIR)
	@if [ ! -d "$(Z8LUA_DIR)/.git" ]; then \
	  if [ -n "$$(ls -A $(Z8LUA_DIR) 2>/dev/null)" ]; then \
	    echo "error: $(Z8LUA_DIR) is not empty and is not a git checkout" >&2; \
	    echo "       remove the directory and run make again" >&2; \
	    exit 1; \
	  fi; \
	  git clone --branch pico8 --depth 1 https://github.com/samhocevar/z8lua.git "$(Z8LUA_DIR)"; \
	else \
	  echo "z8lua already present in $(Z8LUA_DIR)"; \
	fi
	@touch $@

# The sources only exist once the clone happened, so they are modelled as
# depending on the stamp: make clones first and only then finds the file.
$(Z8LUA_DIR)/%.c: $(Z8LUA_STAMP)
	@test -f $@

$(Z8LUA_OBJ_DIR)/%.o: $(Z8LUA_DIR)/%.c $(Z8LUA_STAMP) | $(Z8LUA_OBJ_DIR)
	$(CXX) -x c++ $(CXXFLAGS) -I$(Z8LUA_DIR) -c -o $@ $<

$(Z8LUA_A): $(Z8LUA_OBJS)
	$(AR) rcs $@ $^

# -------------------------------------------------------------------- asan --
# Same sources, but built with the address sanitizer into its own object
# directory so the objects never mix with the release ones.
asan:
	$(MAKE) PLATFORM=$(PLATFORM) \
	        OBJ_DIR=$(BUILD_DIR)/obj_asan \
	        Z8LUA_OBJ_DIR=$(BUILD_DIR)/z8lua_asan \
	        Z8LUA_A=$(BUILD_DIR)/libz8lua_asan.a \
	        CXXFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -std=gnu++17 $(SDL_CFLAGS)" \
	        LDFLAGS="-fsanitize=address" \
	        TARGET=$(ASAN_TARGET) $(ASAN_TARGET)

# --------------------------------------------------------------------- run --
run: $(TARGET)
	cd $(ROOT)/tests/carts && "$(TARGET)" -headless "$(TEST_CART)"

clean:
	rm -rf $(BUILD_DIR)
