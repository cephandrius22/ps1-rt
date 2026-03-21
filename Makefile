CC = cc
CFLAGS = -Wall -Wextra -O2 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lm

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = rt

# Source objects minus main.o for linking with tests
LIB_OBJ = $(filter-out src/main.o, $(OBJ))

# Test files
TEST_SRC = $(wildcard tests/test_*.c)
TEST_BIN = $(TEST_SRC:tests/%.c=tests/%)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Build individual test binaries
tests/test_math3d: tests/test_math3d.c src/math3d.h
	$(CC) $(CFLAGS) -o $@ $< -lm

tests/test_camera: tests/test_camera.c src/camera.o
	$(CC) $(CFLAGS) -o $@ $< src/camera.o -lm

tests/test_mesh: tests/test_mesh.c src/mesh.o
	$(CC) $(CFLAGS) -o $@ $< src/mesh.o -lm

tests/test_bvh: tests/test_bvh.c src/bvh.o src/mesh.o
	$(CC) $(CFLAGS) -o $@ $< src/bvh.o src/mesh.o -lm

tests/test_texture: tests/test_texture.c src/texture.o
	$(CC) $(CFLAGS) -o $@ $< src/texture.o -lm

tests/test_ps1_effects: tests/test_ps1_effects.c src/ps1_effects.o src/mesh.o
	$(CC) $(CFLAGS) -o $@ $< src/ps1_effects.o src/mesh.o -lm

tests/test_player: tests/test_player.c src/player.o src/camera.o src/input.o
	$(CC) $(CFLAGS) -o $@ $< src/player.o src/camera.o src/input.o $(LDFLAGS)

tests/test_render: tests/test_render.c src/render.o src/camera.o src/mesh.o src/bvh.o src/texture.o src/ps1_effects.o
	$(CC) $(CFLAGS) -o $@ $< src/render.o src/camera.o src/mesh.o src/bvh.o src/texture.o src/ps1_effects.o -lm

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

clean:
	rm -f $(OBJ) $(BIN) $(TEST_BIN)

.PHONY: clean test
