# Cross-platform build (macOS uses MacPorts, Windows uses MSYS2/mingw)

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
CXX       ?= clang++
AR        ?= ar
CXXFLAGS  ?= -O2 -Wall -Wextra -std=gnu++17
SDL2_DIR  ?= /opt/local
SDL_CFLAGS := -I$(SDL2_DIR)/include
SDL_LIBS   := -L$(SDL2_DIR)/lib -lSDL2 -lSDL2main
CXXFLAGS   += $(SDL_CFLAGS)
LIBS       += $(SDL_LIBS) -lm
else
CXX       ?= /c/devkitPro/msys2/opt/bin/x86_64-w64-mingw32-g++.exe
AR        ?= /c/devkitPro/msys2/opt/bin/x86_64-w64-mingw32-ar.exe
CXXFLAGS  ?= -O2 -Wall -Wextra -std=gnu++17
SDL2_DIR   ?= SDL2_mgw/SDL2-2.30.11/x86_64-w64-mingw32
SDL_CFLAGS := -I$(SDL2_DIR)/include -I$(SDL2_DIR)/include/SDL2
SDL_LIBS   := -L$(SDL2_DIR)/lib -lmingw32 -lSDL2main -lSDL2
CXXFLAGS   += $(SDL_CFLAGS)
LIBS       += $(SDL_LIBS) -lm
endif
Z8LUA_DIR  := z8lua
Z8LUA_A    := $(Z8LUA_DIR)/libz8lua.a

SRC_DIR    := src
OBJ_DIR    := obj

CPP_SRCS   := $(wildcard $(SRC_DIR)/*.cpp)
CPP_OBJS   := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CPP_SRCS))

TARGET     := pico-c.exe

.PHONY: all clean

all: $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -I$(Z8LUA_DIR) -c -o $@ $<

$(TARGET): $(CPP_OBJS) $(Z8LUA_A)
	$(CXX) $(CXXFLAGS) -o $@ $(CPP_OBJS) $(Z8LUA_A) $(LIBS) $(LDFLAGS)

# z8lua
Z8LUA_OBJS := lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o \
              lfunc.o lgc.o llex.o lmem.o lobject.o lopcodes.o \
              lparser.o lstate.o lstring.o ltable.o ltm.o lundump.o \
              lvm.o lzio.o lauxlib.o lbaselib.o lcorolib.o \
              ldblib.o ltablib.o lstrlib.o lpico8lib.o linit.o

$(Z8LUA_DIR):
	git clone --branch pico8 --depth 1 https://github.com/samhocevar/z8lua.git $(Z8LUA_DIR)

AR       ?= ar
$(Z8LUA_A): $(Z8LUA_DIR) $(addprefix $(Z8LUA_DIR)/, $(Z8LUA_OBJS))
	$(AR) rcs $@ $(addprefix $(Z8LUA_DIR)/, $(Z8LUA_OBJS))

$(Z8LUA_DIR)/%.o: $(Z8LUA_DIR)/%.cpp | $(Z8LUA_DIR)
	$(CXX) $(CXXFLAGS) -I$(Z8LUA_DIR) -c -o $@ $<

$(Z8LUA_DIR)/%.o: $(Z8LUA_DIR)/%.c | $(Z8LUA_DIR)
	$(CXX) -x c++ $(CXXFLAGS) -I$(Z8LUA_DIR) -c -o $@ $<

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	rm -f $(addprefix $(Z8LUA_DIR)/, $(Z8LUA_OBJS)) $(Z8LUA_A)

run: $(TARGET)
	./$(TARGET) -headless test.p8

.PHONY: all clean run
