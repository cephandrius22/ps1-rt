# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# PS1-Style CPU Raytracer

Real-time software raytracer in C + SDL2. Renders at 320x240, upscaled 3x. PS1 aesthetic: vertex jitter, affine textures, Bayer dithering, 15-bit color.

## Build & Run

```bash
make rt          # build the binary
make test        # build and run all test suites
make build/tests/test_bvh && ./build/tests/test_bvh   # one suite
make clean       # remove build/ and rt
make compile_flags.txt  # include paths for clangd
./rt             # run with default level (levels/e1m1.level)
./rt levels/foo.level  # run with a specific level file
```

Requires: SDL2 (`sdl2-config`), C compiler, pthreads, `-lm`.

## Architecture

### Layers

`src/` is split by layer. A module may call down, never up or sideways
across `game`/`render`. Every layer dir is on the include path, so
headers are included by bare name (`"scene.h"`).

```
  main.c        wires everything, owns the loop
  game/  render/   gameplay (player, weapon, cmd) | shading
  world/        scene, entity, light, level loader
  gfx/          camera, mesh, bvh, texture, proctex, ps1_effects
  core/         math3d, threadpool
  platform/     SDL input + audio; talks to game via PlayerCmd only
```

- **PlayerCmd (`game/cmd.h`) is the only input contract.** `input_poll()`
  maps keys/mouse to it (bindings + sensitivity live in `platform/input.c`).
  Game code must never include `input.h` or SDL, so AI or demo playback
  can drive any actor with the same struct.

### Core Pipeline

`main.c` — Game loop: level load → `input_poll()` → physics → light update → render → audio triggers → SDL blit

Rendering path: `camera_ray()` → `scene_intersect()` (two-level BVH) → `shade_pixel_lit()` → PS1 dither → framebuffer

### Module Map

| Module | Purpose |
|--------|---------|
| `math3d.h` | Vec3, Mat4, Ray, UV — header-only with inline functions |
| `camera` | Camera state, perspective ray generation per pixel |
| `mesh` | Triangle storage, OBJ loader with fan triangulation |
| `bvh` | SAH-based BVH build, stack-based iterative traversal |
| `scene` | Two-level scene graph: static world BVH + dynamic entity BVH rebuilt per frame |
| `entity` | Typed entities (pickup/door/prop) with transforms, growable array |
| `render` | Shading: directional sun + point lights + shadows + fog + light billboards |
| `light` | Point light system: attenuation, flicker (layered sine), pendulum swing animation |
| `texture` | TGA loader, nearest-neighbor sampling |
| `proctex` | Procedural texture generation: concrete, brick, metal_panel, metal_grate, rust, floor_tile, pipe |
| `level` | Text-based `.level` file loader — parses materials, geometry, entities, lights |
| `ps1_effects` | Vertex jitter (1/128 grid), affine UV, 4x4 Bayer dither, 15-bit color quantize |
| `player` | First-person movement with gravity, wall sliding, ground snap via BVH raycasts |
| `weapon` | Hitscan weapon with cooldown and pitch recoil |
| `cmd` | `PlayerCmd`: move axes, look deltas, button bitmask (held + impulse) |
| `input` | SDL2 keyboard/mouse → `PlayerCmd`, relative mouse mode |
| `audio` | 8-voice mixer via SDL2 audio callback, procedural sound gen, WAV loader ready |
| `threadpool` | pthread fork-join pool, generation-based sync, splits scanlines across workers |

### Key Patterns

- **Level files:** Scenes defined in `levels/*.level` text files. `level_load()` parses materials, generates procedural textures, builds geometry, places entities/lights, and constructs the static BVH.
- **Two-level BVH:** `scene.static_bvh` is built once at level load. `scene.dynamic_bvh` is rebuilt each frame from entity triangles. `scene_intersect()` tests both.
- **Procedural textures:** `proctex.c` generates 32x32/64x64 textures in code using hash-based noise. Materials in level files reference them by name (e.g., `concrete`, `brick`).
- **Multithreaded render:** `render_scene_lit_mt()` splits screen rows across thread pool workers. Each worker gets a pre-assigned chunk — no work stealing.
- **Audio triggers in main loop:** Player struct carries per-frame event flags (`collected`, `door_action`, `walk_distance`). `main.c` reads these and calls `audio_play()` — audio module has no scene dependency.
- **Materials:** `Scene` holds parallel arrays of `Material` and `Texture`. Triangles reference materials by index. Fallback is checkerboard.
- **Shadow rays:** Capped at `MAX_SHADOW_LIGHTS` (4) per pixel for performance. Sun shadow is separate.

### Level File Format

```
spawn <x> <y> <z> <yaw_degrees>
material <name> <texture_name> <r> <g> <b>
room <x1> <y1> <z1> <x2> <y2> <z2> <wall_mat> <floor_mat> <ceil_mat>
box <x1> <y1> <z1> <x2> <y2> <z2> <mat>
quad <12 floats> <mat>
obj <filename> <x> <y> <z> <rot_y> <scale> <mat>
entity <type> <x> <y> <z> [key=value ...]
light <x> <y> <z> <r> <g> <b> <intensity> <radius> [key=value ...]
```

`room` generates inner-facing geometry (you're inside it). `box` generates outer-facing geometry (obstacles). UVs auto-tile based on world dimensions.

### Adding Content

**New level:** Create a `.level` file in `levels/`. See `levels/e1m1.level` for reference. Run with `./rt levels/yourmap.level`.

**New procedural texture:** Add to `ProceduralTextureID` enum in `proctex.h`, write generator function in `proctex.c`, add to `proctex_names[]` and `proctex_generators[]` arrays.

**New entity type:** Add to `EntityType` enum in `entity.h`, handle in `entity_update()` and interaction functions. Add parsing in `level.c:parse_entity()`.

**New light:** Use the `light` directive in a level file. Keys: `shadows`, `flicker_speed`, `flicker_amount`, `anchor_x/y/z`, `cord`, `swing_speed`, `swing_angle`.

**New sound:** Add to `SoundID` enum, generate in `audio_generate_sounds()` or load WAV via `audio_load_wav()`. Trigger with `audio_play()` from main loop.

**New PS1 effect:** Add to `ps1_effects.c`. Current effects are applied per-vertex (jitter) or per-pixel (dither/quantize).

## Tests

Each `tests/test_*.c` is a standalone binary using `tests/test_framework.h` (`TEST`, `RUN_TEST`, `TEST_SUITE`, `TEST_REPORT`).

- **New suite:** just add `tests/test_<name>.c`. Tests link against `build/librt.a` (all objects except `main.o`), so no Makefile edit is needed.
- **New source dir under `src/`:** picked up automatically (sources and include path).
- **Run from repo root.** `test_level` loads `tests/test.level` by relative path.

Controls: WASD move, mouse look, Space jump, LMB shoot, E use, F flashlight, Esc quit.
