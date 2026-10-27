# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# PS1-Style CPU Raytracer

Real-time software raytracer in C + SDL2, growing into a moody Doom/Quake-style FPS (see `ROADMAP.md`). Renders at 320x240, upscaled 3x. PS1 aesthetic: vertex jitter, Bayer dithering, 15-bit color.

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

`src/` is split by layer. A module calls only the layer directly below;
`core/` is the shared foundation every layer may use. Every layer dir is
on the include path, so headers are included by bare name (`"scene.h"`).

- `game/` and `render/` are siblings and never include each other —
  `main.c` copies values between them (e.g. `OverlayState`).
- `render/` is the one module that also uses `gfx/` directly: `world/`
  says *what* is in the scene, `gfx/` says *how* to draw it (camera
  rays, texture sampling, dither).
- `platform/` knows only `cmd.h` from `game/`; `main.c` is the only file
  that sees every layer.

```
  main.c          wires platform + game + render, owns the loop
  game/  render/  game state, player, weapon, ai, inventory, cmd, events | shading, overlay
  world/          scene, entity, sprite, light, level loader
  gfx/            camera, mesh, bvh, texture, proctex, ps1_effects
  core/           math3d (Vec3, Ray, Pose), threadpool, kv
  platform/       SDL input + audio; talks to game via PlayerCmd only
```

- **PlayerCmd (`game/cmd.h`) is the only input contract.** `input_poll()`
  maps keys/mouse to it (bindings + sensitivity live in `platform/input.c`).
  Game code must never include `input.h` or SDL, so AI or demo playback
  can drive any actor with the same struct.

### Core Pipeline

`main.c` per frame: `input_poll()` → `game_update()` → `play_event_sounds()` → `camera_from_pose()` → `render_scene_lit_mt()` → `overlay_draw()` → SDL present

`game_update()` runs 0..`GAME_MAX_TICKS_PER_FRAME` fixed ticks at `GAME_TICK_HZ` (60). Each `game_tick()`: entities → lights → rebuild dynamic BVH + sprites → then by `GameMode`:
- `GAME_PLAYING`: AI → player (exit pad → `GAME_LEVEL_END`) → weapon → `resolve_impacts()` (bullet damage, kills) → player dead → `GAME_DEAD`
- `GAME_DEAD`: AI keeps running, view sinks; attack/use after `GAME_CONTINUE_DELAY` reloads the level
- `GAME_LEVEL_END`: frozen; attack/use after the delay reloads the level

Rendering path: `camera_ray()` → `scene_intersect()` (two-level BVH + sprites) → `shade_pixel_lit()` → PS1 dither → framebuffer → `overlay_draw()` (HUD, death tint, level-end kill count)

### Module Map

| Module | Purpose |
|--------|---------|
| `math3d.h` | Vec3, Mat4, Ray, UV, `Pose` (pos + yaw/pitch) — header-only |
| `camera` | Render camera (`camera_from_pose()`), perspective ray per pixel |
| `mesh` | Triangle storage, OBJ loader with fan triangulation |
| `bvh` | SAH-based BVH build, stack-based iterative traversal |
| `scene` | Static world BVH + dynamic entity BVH + sprite list (both rebuilt per tick); `scene_trace()` with `TraceMask` |
| `sprite` | Upright billboards facing each ray, color-keyed holes, frame sheets; `SPRITE_SOLID`/`SPRITE_GHOST` |
| `entity` | Entity storage + type-agnostic dispatch (update, touch, use, mesh build) |
| `entity_types` | `EntityDef` table: per-type name, `RenderKind`, `TouchEffect`, hooks (spawn/update/touch/use/damage/pose/build_mesh/sprite) |
| `kv` | `key=value` lookup in a text line (level args) |
| `render` | Shading: directional sun + point lights + shadows + fog + light billboards |
| `overlay` | HUD (health, ammo), death tint, level-end kill count; 3x5 bitmap digits. Fed an `OverlayState` by `main.c` |
| `light` | Point light system: attenuation, flicker (layered sine), pendulum swing animation |
| `texture` | Nearest-neighbor sampling; TGA loader exists but levels can't reference TGA files yet |
| `proctex` | Procedural textures: concrete, brick, metal_panel, metal_grate, rust, floor_tile, pipe; color-keyed sprites: barrel, ghoul (5-frame sheet, `GhoulFrame`) |
| `level` | Text-based `.level` file loader — parses materials, geometry, entities, lights |
| `ps1_effects` | Vertex jitter (1/128 grid, applied at level load), 4x4 Bayer dither + 15-bit quantize (per pixel). `ps1_affine_uv()` exists but shading doesn't use it |
| `game` | `Game` struct: owns scene, player, weapon, events, mode, kills; fixed-timestep `game_update()`, `game_view()` (view + recoil) |
| `player` | First-person movement with gravity, wall sliding, ground snap; health, `Inventory`; applies `TouchEffect`s |
| `ai` | Enemy state machine (idle → chase → attack, pain, dead); `EnemyDef` table of behavior stats |
| `inventory` | Ammo pools by `AmmoType`, with caps |
| `weapon` | Hitscan firing driven by `WeaponDef` (`weapon_types.c`): damage, cooldown, range, pellets, spread, kick, ammo; dry fire when empty |
| `event` | `GameEvent` queue: gameplay pushes, presentation drains each frame |
| `cmd` | `PlayerCmd`: move axes, look deltas, button bitmask (held + impulse) |
| `input` | SDL2 keyboard/mouse → `PlayerCmd`, relative mouse mode |
| `audio` | 8-voice mixer via SDL2 audio callback, procedural sound gen, WAV loader ready |
| `threadpool` | pthread fork-join pool, generation-based sync, splits scanlines across workers |

### Key Patterns

- **Level files:** Scenes defined in `levels/*.level` text files. `level_load()` parses materials, generates procedural textures, builds geometry, places entities/lights, and constructs the static BVH.
- **Two-level BVH:** `scene.static_bvh` is built once at level load. `scene.dynamic_bvh` is rebuilt each tick from entity triangles. `scene_intersect()` tests both.
- **Procedural textures:** `proctex.c` generates 32x32/64x64 textures in code using hash-based noise. Materials in level files reference them by name (e.g., `concrete`, `brick`).
- **Multithreaded render:** `render_scene_lit_mt()` splits screen rows across thread pool workers. Each worker gets a pre-assigned chunk — no work stealing.
- **Fixed timestep:** gameplay only sees `GAME_TICK_DT`, never frame time. Input is folded into `Game.pending` (`cmd_accumulate`): look deltas and impulses apply exactly once even when a frame runs 0 or several ticks. Add one-shot buttons to `CMD_IMPULSES`.
- **Pose, not Camera, in gameplay:** `Player.view` is a `Pose`; weapons aim with a `Pose`. `main.c` builds the render `Camera` (adds FOV). Game code never includes `camera.h`.
- **Event queue:** player/weapon push `GameEvent`s (type, pos, entity, amount) into `Game.events`, cleared at the start of each `game_update()`. `main.c:play_event_sounds()` maps them to `audio_play()`. New reactions (HUD, particles) consume the queue; gameplay code stays untouched.
- **Hit → entity:** `Triangle.owner`/`HitRecord.owner` is an opaque tag at the `gfx` layer. Entity triangles and sprites get `owner = index + 1`; call `entity_hit(&rec)` for the entity index or `ENTITY_INDEX_NONE`. Zero-initialized triangles read as world.
- **Trace masks:** rendering uses `TRACE_VISIBLE`; anything that blocks (player/enemy movement, bullets, enemy sight) uses `scene_trace(..., TRACE_SOLID)`, which skips ghost sprites (corpses). Sprites cast no shadows (`scene_occluded()` skips them).
- **Sprites:** RENDER_SPRITE entities implement `EntityDef.sprite` to pick size, frame and solidity per tick. Textures use magenta (`TEXTURE_KEY_*`) for holes; sheets hold frames side by side, rows top-down.
- **Touch:** `entity_overlap()` finds touchables in the body cylinder without side effects; the player applies the type's `TouchEffect` (heal, ammo, exit) and only then calls `entity_touch()`. Health/ammo at cap leave pickups in place. Origins up to `TOUCH_FOOT_SLACK` below the feet count (floor pads).
- **Damage:** weapon emits `EV_WEAPON_IMPACT` (entity, amount); `game.c:resolve_impacts()` calls `entity_damage()` → type's `damage` hook → `EV_ENEMY_PAIN`/`EV_ENEMY_DIED`. Enemies hurt the player via `player_damage()`.
- **Enemies:** world owns health and `EnemyState` (and draws frames from it); `game/ai.c` owns transitions and stats (`EnemyDef`). Enemies move on a fixed height (no gravity/steps yet) and walk straight at the player.
- **Materials:** `Scene` holds parallel arrays of `Material` and `Texture`. Triangles reference materials by index. A material's texture is a procedural texture name or `none`; fallback is checkerboard.
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

Entity types (all take `rot=` and `mat=<material index, 0-based in file order>`):

| Type | Keys | Notes |
|------|------|-------|
| `health` | `amount` (25) | heals, not taken at full health |
| `ammo` | `amount` (10) | bullets, not taken at cap |
| `door` | `hinge_x`, `hinge_z` | E toggles |
| `prop` | — | static cube |
| `decor` | `w` (0.8), `h` (1.0), `solid` (1) | sprite; material should use a sprite texture (`barrel`) |
| `ghoul` | `health` (60) | melee enemy sprite; material should use `ghoul` |
| `exit` | — | floor pad; touching it ends the level |

`room` generates inner-facing geometry (you're inside it). `box` generates outer-facing geometry (obstacles). UVs auto-tile based on world dimensions.

### Adding Content

**New level:** Create a `.level` file in `levels/`. See `levels/e1m1.level` for reference. Run with `./rt levels/yourmap.level`.

**New procedural texture:** Add to `ProceduralTextureID` enum in `proctex.h`, write generator function in `proctex.c`, add to `proctex_names[]` and `proctex_generators[]` arrays. Sprite textures fill empty pixels with the color key; preview with an offscreen render, since tests can't judge looks.

**New entity type:** Add to `EntityType` enum in `entity.h`, then one `EntityDef` row in `entity_types.c` with its level name, `RenderKind`, and only the hooks it needs (NULL = no behavior). Level parsing, update, touch, use and mesh build pick it up automatically — never add `switch (e->type)` to generic code. Entity indices are stable (inactive entities stay in the list).

**New weapon:** Add to `WeaponType` in `weapon.h` and one row in `weapon_types.c`. Shotgun-style = `pellets > 1` + `spread`. Each pellet hit emits `EV_WEAPON_IMPACT` with `amount = damage`.

**New enemy:** `EntityType` + `EntityDef` row (`RENDER_SPRITE`, `spawn` setting `health`, `damage`, `sprite` picking frames from `EnemyState`) + `EnemyDef` row in `game/ai.c` + a sprite sheet in `proctex.c` + a level material using it.

**New light:** Use the `light` directive in a level file. Keys: `shadows`, `flicker_speed`, `flicker_amount`, `anchor_x/y/z`, `cord`, `swing_speed`, `swing_angle`.

**New sound:** Add to `SoundID` enum, generate in `audio_generate_sounds()` or load WAV via `audio_load_wav()`. Push a `GameEvent` from gameplay and map it in `play_event_sounds()`.

**New PS1 effect:** Add to `ps1_effects.c`. Current effects are applied per-vertex (jitter) or per-pixel (dither/quantize).

## Tests

Each `tests/test_*.c` is a standalone binary using `tests/test_framework.h` (`TEST`, `RUN_TEST`, `TEST_SUITE`, `TEST_REPORT`).

- **New suite:** just add `tests/test_<name>.c`. Tests link against `build/librt.a` (all objects except `main.o`), so no Makefile edit is needed.
- **New source dir under `src/`:** picked up automatically (sources and include path).
- **Run from repo root.** `test_level` loads `tests/test.level` by relative path.

Controls: WASD move, mouse look, Space jump, LMB shoot, E use, F flashlight, Esc quit. Dead or at the exit: LMB/E (after 1 s) restarts the level.
