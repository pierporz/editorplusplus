# editor++ build
#
# make test   - compiles core/ + tests/ natively and runs them (<10s)
# make win    - cross-compiles build/editor++.exe with MinGW-w64
# make smoke  - make win, then launches it under Wine/Xvfb to catch startup crashes
# make clean  - removes build/
# make all    - test + win

CXX_NATIVE := g++
CXX_WIN    := x86_64-w64-mingw32-g++
WINDRES    := x86_64-w64-mingw32-windres

CXXFLAGS_COMMON := -std=c++17 -O2 -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti
WIN_DEFS         := -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX
WINFLAGS         := -municode -mwindows -static -static-libgcc -static-libstdc++ $(WIN_DEFS)
LDFLAGS_WIN      := -lcomctl32 -lcomdlg32 -lshlwapi -lole32 -luuid -lgdi32 -luser32 -limm32 -loleaut32 -lshell32

# Scintilla/Lexilla need exceptions + RTTI; our own code (core/, platform/) does not.
SCI_INCLUDES := -Ithird_party/scintilla/include -Ithird_party/scintilla/src -Ithird_party/scintilla/win32
LEX_INCLUDES := -Ithird_party/lexilla/include -Ithird_party/lexilla/lexlib -Ithird_party/lexilla/access $(SCI_INCLUDES)
SCI_FLAGS    := -std=c++17 -O2 -Wall $(WIN_DEFS) $(SCI_INCLUDES)
LEX_FLAGS    := -std=c++17 -O2 -Wall $(WIN_DEFS) $(LEX_INCLUDES)

ROOT_INCLUDE := -I.

CORE_SRCS     := $(wildcard core/*.cpp)
PLATFORM_SRCS := $(wildcard platform/win32/*.cpp)
TEST_SRCS     := $(wildcard tests/*.cpp)

SCI_SRCS := $(wildcard third_party/scintilla/src/*.cxx) \
            third_party/scintilla/win32/PlatWin.cxx \
            third_party/scintilla/win32/ScintillaWin.cxx \
            third_party/scintilla/win32/ListBox.cxx \
            third_party/scintilla/win32/HanjaDic.cxx \
            third_party/scintilla/win32/SurfaceGDI.cxx \
            third_party/scintilla/win32/SurfaceD2D.cxx

LEX_SRCS := $(wildcard third_party/lexilla/lexlib/*.cxx) \
            $(wildcard third_party/lexilla/lexers/*.cxx) \
            $(wildcard third_party/lexilla/access/*.cxx) \
            third_party/lexilla/src/Lexilla.cxx

BUILD_TEST_DIR := build/test
BUILD_WIN_DIR  := build/win

TEST_OBJS := $(patsubst %.cpp,$(BUILD_TEST_DIR)/%.o,$(CORE_SRCS) $(TEST_SRCS))

WIN_CORE_OBJS     := $(patsubst %.cpp,$(BUILD_WIN_DIR)/%.o,$(CORE_SRCS))
WIN_PLATFORM_OBJS := $(patsubst %.cpp,$(BUILD_WIN_DIR)/%.o,$(PLATFORM_SRCS))
WIN_SCI_OBJS      := $(patsubst %.cxx,$(BUILD_WIN_DIR)/%.o,$(SCI_SRCS))
WIN_LEX_OBJS      := $(patsubst %.cxx,$(BUILD_WIN_DIR)/%.o,$(LEX_SRCS))
WIN_RES           := $(BUILD_WIN_DIR)/platform/win32/resource.o

.PHONY: test win smoke clean all

all: test win

test: $(BUILD_TEST_DIR)/run_tests
	./$(BUILD_TEST_DIR)/run_tests

$(BUILD_TEST_DIR)/run_tests: $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CXX_NATIVE) $(CXXFLAGS_COMMON) -o $@ $^

$(BUILD_TEST_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX_NATIVE) $(CXXFLAGS_COMMON) $(ROOT_INCLUDE) -c -o $@ $<

win: build/editor++.exe

# WIN_LEX_OBJS is wired in during Phase 4 (syntax highlighting). Lexilla.cxx's
build/editor++.exe: $(WIN_CORE_OBJS) $(WIN_PLATFORM_OBJS) $(WIN_SCI_OBJS) $(WIN_LEX_OBJS) $(WIN_RES)
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(CXXFLAGS_COMMON) $(WINFLAGS) -s -o $@ $^ $(LDFLAGS_WIN)

$(BUILD_WIN_DIR)/core/%.o: core/%.cpp
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(CXXFLAGS_COMMON) $(WIN_DEFS) $(ROOT_INCLUDE) -c -o $@ $<

$(BUILD_WIN_DIR)/platform/win32/%.o: platform/win32/%.cpp
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(CXXFLAGS_COMMON) $(WIN_DEFS) $(ROOT_INCLUDE) -Ithird_party/lexilla/include -Ithird_party/scintilla/include -c -o $@ $<

$(BUILD_WIN_DIR)/platform/win32/resource.o: platform/win32/resource.rc platform/win32/resource.h platform/win32/app.manifest
	@mkdir -p $(dir $@)
	$(WINDRES) -Iplatform/win32 -o $@ platform/win32/resource.rc

$(BUILD_WIN_DIR)/third_party/scintilla/src/%.o: third_party/scintilla/src/%.cxx
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(SCI_FLAGS) -c -o $@ $<

$(BUILD_WIN_DIR)/third_party/scintilla/win32/%.o: third_party/scintilla/win32/%.cxx
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(SCI_FLAGS) -c -o $@ $<

$(BUILD_WIN_DIR)/third_party/lexilla/lexlib/%.o: third_party/lexilla/lexlib/%.cxx
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(LEX_FLAGS) -c -o $@ $<

$(BUILD_WIN_DIR)/third_party/lexilla/lexers/%.o: third_party/lexilla/lexers/%.cxx
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(LEX_FLAGS) -c -o $@ $<

$(BUILD_WIN_DIR)/third_party/lexilla/access/%.o: third_party/lexilla/access/%.cxx
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(LEX_FLAGS) -c -o $@ $<

$(BUILD_WIN_DIR)/third_party/lexilla/src/%.o: third_party/lexilla/src/%.cxx
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(LEX_FLAGS) -c -o $@ $<

SMOKE_TIMEOUT := 5

smoke: win
	@echo "Launching build/editor++.exe under Wine for $(SMOKE_TIMEOUT)s (expect it still running = no startup crash)..."
	@xvfb-run -a timeout $(SMOKE_TIMEOUT) wine build/editor++.exe; ec=$$?; \
	if [ $$ec -eq 124 ]; then \
		echo "smoke OK: process was still running, killed by timeout"; \
	else \
		echo "smoke FAILED: process exited early with code $$ec (possible startup crash)"; \
		exit 1; \
	fi

clean:
	rm -rf build
