CC = cc
BUILD = build
SDL_CONFIG ?= sdl2-config
EXE =

# Platform differences live here; the rules below stay the same.
# TB_GAMES is TrenchBroom's games folder (the editor target installs the
# RT config there); override it for Linux or a custom install.
#
# Windows (MSYS2 MinGW shell): gcc instead of cc, programs get .exe, and
# TrenchBroom lives under %APPDATA% and wants C:/ paths, not MSYS /c/ ones.
# SDL links as a GUI app (-mwindows), which hides the console; keep it,
# since check:/budget: lines print there.
ifeq ($(OS),Windows_NT)
    CC = gcc
    EXE = .exe
    REPO_DIR = $(shell cygpath -m "$(CURDIR)")
    TB_GAMES ?= $(subst \,/,$(APPDATA))/TrenchBroom/games
    SDL_LIBS = $(filter-out -mwindows,$(shell $(SDL_CONFIG) --libs)) -mconsole
else
    REPO_DIR = $(CURDIR)
    TB_GAMES ?= $(HOME)/Library/Application Support/TrenchBroom/games
    SDL_LIBS = $(shell $(SDL_CONFIG) --libs)
endif

# Each layer directory is on the include path, so headers are included by
# bare name ("scene.h") regardless of where they live.
SRC_DIRS = $(patsubst %/.,%,$(wildcard src/*/.))
INC = $(addprefix -I,$(SRC_DIRS))

CFLAGS = -Wall -Wextra -O2 -MMD -MP $(INC) $(shell $(SDL_CONFIG) --cflags)
LDFLAGS = $(SDL_LIBS) -lm

SRC = $(wildcard src/*/*.c) src/main.c
OBJ = $(SRC:%.c=$(BUILD)/%.o)
BIN = rt$(EXE)

# Everything except main.o, archived so tests link only what they use
LIB_OBJ = $(filter-out $(BUILD)/src/main.o, $(OBJ))
LIB = $(BUILD)/librt.a

TEST_SRC = $(wildcard tests/test_*.c)
TEST_BIN = $(TEST_SRC:tests/%.c=$(BUILD)/tests/%$(EXE))

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB): $(LIB_OBJ)
	$(AR) rcs $@ $^

$(BUILD)/tests/%$(EXE): tests/%.c $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(LIB) $(LDFLAGS)

test: $(TEST_BIN)
	@echo ""
	@failed=0; \
	for t in $(TEST_BIN); do \
		echo "=== $$t ==="; \
		./$$t || failed=$$((failed + 1)); \
	done; \
	echo ""; \
	if [ $$failed -gt 0 ]; then \
		echo "$$failed test suite(s) failed"; \
		exit 1; \
	else \
		echo "All test suites passed"; \
	fi

# TrenchBroom: generate textures + FGD from code, add the hand-made TGAs
# from textures/ (so the editor shows them too), then install the game
# config into TrenchBroom's games folder. A symlink keeps it live on
# macOS/Linux; MSYS can't make one without admin rights, so Windows gets
# a copy (rerun `make editor` after changing the config or entity types).
ifeq ($(OS),Windows_NT)
    INSTALL_CONFIG = mkdir -p "$(TB_GAMES)/RT" && cp -r editor/trenchbroom/RT/. "$(TB_GAMES)/RT/"
else
    INSTALL_CONFIG = ln -sfn "$(REPO_DIR)/editor/trenchbroom/RT" "$(TB_GAMES)/RT"
endif

editor: $(BIN)
	@mkdir -p editor/game/textures/rt editor/trenchbroom/RT
	./$(BIN) --export-editor editor
	@if ls textures/*.tga >/dev/null 2>&1; then cp textures/*.tga editor/game/textures/rt/; fi
	@mkdir -p "$(TB_GAMES)"
	@$(INSTALL_CONFIG)
	@echo "Installed $(TB_GAMES)/RT from editor/trenchbroom/RT"
	@echo "In TrenchBroom: Preferences > RT > Game Path = $(REPO_DIR)/editor/game"

# Editor support: clangd reads this when there's no compile database
compile_flags.txt: Makefile
	@printf '%s\n' $(CFLAGS) | grep -v '^-M' > $@

clean:
	rm -rf $(BUILD) $(BIN)

-include $(OBJ:.o=.d) $(TEST_BIN:=.d)

.PHONY: clean test editor
