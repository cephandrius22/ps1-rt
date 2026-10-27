CC = cc
BUILD = build

# Each layer directory is on the include path, so headers are included by
# bare name ("scene.h") regardless of where they live.
SRC_DIRS = $(patsubst %/.,%,$(wildcard src/*/.))
INC = $(addprefix -I,$(SRC_DIRS))

CFLAGS = -Wall -Wextra -O2 -MMD -MP $(INC) $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lm

SRC = $(wildcard src/*/*.c) src/main.c
OBJ = $(SRC:%.c=$(BUILD)/%.o)
BIN = rt

# Everything except main.o, archived so tests link only what they use
LIB_OBJ = $(filter-out $(BUILD)/src/main.o, $(OBJ))
LIB = $(BUILD)/librt.a

TEST_SRC = $(wildcard tests/test_*.c)
TEST_BIN = $(TEST_SRC:tests/%.c=$(BUILD)/tests/%)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(BUILD)/tests/%: tests/%.c $(LIB)
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

# TrenchBroom: generate textures + FGD from code, link the game config
# into TrenchBroom's games folder (macOS path; override TB_GAMES elsewhere)
TB_GAMES ?= $(HOME)/Library/Application Support/TrenchBroom/games

editor: $(BIN)
	@mkdir -p editor/game/textures/rt editor/trenchbroom/RT
	./$(BIN) --export-editor editor
	@mkdir -p "$(TB_GAMES)"
	@ln -sfn "$(CURDIR)/editor/trenchbroom/RT" "$(TB_GAMES)/RT"
	@echo "Linked $(TB_GAMES)/RT -> editor/trenchbroom/RT"
	@echo "In TrenchBroom: Preferences > RT > Game Path = $(CURDIR)/editor/game"

# Editor support: clangd reads this when there's no compile database
compile_flags.txt: Makefile
	@printf '%s\n' $(CFLAGS) | grep -v '^-M' > $@

clean:
	rm -rf $(BUILD) $(BIN)

-include $(OBJ:.o=.d) $(TEST_BIN:=.d)

.PHONY: clean test editor
