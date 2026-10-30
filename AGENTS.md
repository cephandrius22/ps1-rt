# Repository Guidelines

## Project Structure & Module Organization

This project is a PS1-style CPU raytracer and FPS written in C with SDL2. `src/main.c` wires together the layers: `core/` (math, parsing, threads), `gfx/` (geometry, textures, BVH), `world/` (scenes and level loading), `game/` (simulation), `render/` (presentation), `platform/` (SDL and OS services), `shell/` (menus), and `tools/` (export, conversion, benchmarks).

Keep `game/` and `render/` independent; exchange data through `main.c`. Gameplay consumes `PlayerCmd` and must not include SDL or platform input headers. See `CLAUDE.md` for detailed architecture and `ROADMAP.md` for planned work.

`tests/` holds suites and fixtures; `levels/` holds maps and episode definitions. Hand-made textures belong in `textures/`; procedural textures live in `src/gfx/`. `editor/` contains TrenchBroom configuration. Build outputs go in `build/`.

## Build, Test, and Development Commands

Install a C compiler, Make, and SDL2 with `sdl2-config`. Run commands from the repository root:

- `make`: build `rt`; `./rt` starts the default episode.
- `./rt levels/arena.map`: play a specific map; saving the map triggers reload.
- `make test`: build and run every test suite.
- `make build/tests/test_bvh && ./build/tests/test_bvh`: run one suite.
- `./rt --bench`: measure frame and simulation times on shipped maps.
- `make editor`: export textures/FGD and install the TrenchBroom configuration.
- `make compile_flags.txt`: refresh clangd flags.
- `make clean`: remove the default build directory and executable.

Windows development uses MSYS2 UCRT64 and produces `rt.exe`.

## Coding Style & Naming Conventions

Match existing C code: four-space indentation, opening braces on the declaration line, `snake_case` functions/variables, `PascalCase` types, and uppercase constants. Pair module `.c` and `.h` files; include project headers by bare name, such as `"scene.h"`. Keep `-Wall -Wextra` builds warning-free. No formatter or lint configuration is provided. Isolate platform differences in dedicated function blocks.

## Testing Guidelines

Use `tests/test_framework.h` macros (`TEST`, `RUN_TEST`, assertions, `TEST_REPORT`). New `tests/test_<module>.c` suites are discovered automatically. Add regression cases for changed behavior; no numeric coverage threshold is configured. Keep scratch files under `build/tests/`. Regenerate `tests/test.map` with `tests/gen_test_map.py`. Shipped maps must pass `test_episode` without checker warnings.

## Commit & Pull Request Guidelines

Use short, imperative commit subjects matching history, such as “Cache fixed lights' shadows at level load”. Keep changes focused. PRs should explain the behavior change, link relevant issues, and report validation commands/results. Include screenshots for visual changes and benchmark comparisons for performance work. Exclude generated binaries, editor exports, local settings, and saves.
