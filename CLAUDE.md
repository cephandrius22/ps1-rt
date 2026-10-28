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
./rt levels/foo.level  # run a .level or TrenchBroom .map; reloads on save
make editor      # export textures + FGD, link TrenchBroom game config
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
- `platform/` knows only `cmd.h` from `game/`; `main.c` (and `tools/`,
  run from main) are the only code that sees every layer.

```
  main.c          wires platform + game + render, owns the loop
  game/  render/  game, player, weapon, ai, projectile, damage, inventory, cmd, events
                  | shading, overlay, particles, screenfx
  world/          scene, entity, sprite, light, level loader
  gfx/            camera, mesh, bvh, texture, proctex, ps1_effects
  core/           math3d (Vec3, Ray, Pose), threadpool, kv
  platform/       SDL input + audio, file watch; talks to game via PlayerCmd only
  tools/          editor export (`./rt --export-editor`), invoked by main
```

- **PlayerCmd (`game/cmd.h`) is the only input contract.** `input_poll()`
  maps keys/mouse to it (bindings + sensitivity live in `platform/input.c`).
  Game code must never include `input.h` or SDL, so AI or demo playback
  can drive any actor with the same struct.

### Core Pipeline

`main.c` per frame: `input_poll()` → hot reload check (`filewatch_poll()` → `game_reload()`) → `game_update()` → events to presentation (`play_event_sounds()`, `play_event_effects()` shake/flash, `spawn_event_particles()`) → camera = `game_view()` + shake → `render_scene_lit_mt()` (color + depth) → `particles_draw()` → `overlay_draw()` (viewmodel, flash, HUD, F3 lines) → SDL present

`game_update()` runs 0..`GAME_MAX_TICKS_PER_FRAME` fixed ticks at `GAME_TICK_HZ` (60). Each `game_tick()`: entities → lights → rebuild dynamic BVH + sprites → then by `GameMode`:
- `GAME_PLAYING`: AI → player (exit pad → `GAME_LEVEL_END`; new weapon pickups → switch) → weapon select → weapon (muzzle flash light, rocket launch request) → `resolve_impacts()` → `projectile_update()` → player dead → `GAME_DEAD`. Kills are counted from `EV_ENEMY_DIED` events.
- `GAME_DEAD`: AI and projectiles keep running, view sinks; attack/use after `GAME_CONTINUE_DELAY` reloads the level
- `GAME_LEVEL_END`: frozen; attack/use after the delay reloads the level

Rendering path: `camera_ray()` → `scene_intersect()` (two-level BVH + sprites) → `shade_pixel_lit()` → PS1 dither → framebuffer → `overlay_draw()` (HUD, death tint, level-end kill count)

### Module Map

| Module | Purpose |
|--------|---------|
| `math3d.h` | Vec3, Mat4, Ray, UV, `Pose` (pos + yaw/pitch) — header-only |
| `camera` | Render camera (`camera_from_pose()`), ray per pixel, `camera_project()` (world point → pixel, its inverse) |
| `mesh` | Triangle storage, OBJ loader with fan triangulation |
| `bvh` | SAH-based BVH build, stack-based iterative traversal |
| `scene` | Static world BVH + dynamic entity BVH + sprite list (both rebuilt per tick); `scene_trace()` with `TraceMask`; `scene_spawn()` for runtime entities (reuses spent slots, `type_material`) |
| `sprite` | Upright billboards facing each ray, color-keyed holes, frame sheets; `SPRITE_SOLID`/`SPRITE_GHOST` |
| `entity` | Entity storage + type-agnostic dispatch (update, touch, use, mesh build) |
| `entity_types` | `EntityDef` table: per-type name, `RenderKind`, `TouchEffect`, `mesh_solidity`, `runtime_only`, hooks (spawn/update/touch/use/damage/pose/build_mesh/sprite); shared enemy hooks (`enemy_damage`, `enemy_sprite`) |
| `kv` | `key=value` lookup in a text line (level args): float, int, string |
| `render` | Shading: directional sun + point lights + shadows + fog + light billboards; writes a depth buffer; `render_light_at()` (viewmodel light probe) |
| `overlay` | Viewmodel (weapon sheet, bob/recoil/switch offsets, lit + dithered), damage flash, HUD, death tint, level-end kill count, F3 text. Caches viewmodel textures (`Overlay`); fed an `OverlayState` by `main.c` |
| `particles` | Blood, gibs, sparks, debris, spit: projected and splatted after the raytrace, depth-tested; presentation only |
| `screenfx` | Screen shake (camera only, never aim) and full-screen color flash; decays with frame time |
| `light` | Point lights: attenuation, flicker, swing; temporary lights with a lifetime and fade (`light_add_temporary()`), swap-removed |
| `texture` | Nearest-neighbor sampling; TGA load (levels can't reference TGA files yet) and save (editor export) |
| `proctex` | Procedural textures: concrete, brick, metal_panel, metal_grate, rust, floor_tile, pipe; color-keyed sprite sheets (`Sheet` helpers): barrel, ghoul + spitter (`EnemyFrame`), projectiles (`ProjectileFrame`), viewmodels `view_*` (`ViewFrame`) |
| `level` | `level_load()` dispatches by extension; `.level` text loader; `level_finish()` shared last step (jitter, textures, BVH) |
| `mapfile` | TrenchBroom/Quake `.map` loader: brushes → triangles (plane clipping), Standard + Valve 220 UVs, entities |
| `matlib` | Named material registry shared by both loaders; generates procedural textures on first use; entity default materials |
| `levelcheck` | Load-time warnings (spawn/exit, stuck or floating bodies) and budget line (`BUDGET_*`) |
| `filewatch` | Polls a file's mtime; reports a change once it has settled (hot reload) |
| `editor_export` | Writes TrenchBroom textures (TGA) and `rt.fgd` from the proctex generators and `EntityDef` table |
| `ps1_effects` | Vertex jitter (1/128 grid, applied at level load), 4x4 Bayer dither + 15-bit quantize (per pixel). `ps1_affine_uv()` exists but shading doesn't use it |
| `game` | `Game` struct: owns scene, player, weapon, events, mode, kills; fixed-timestep `game_update()`, `game_view()` (view + recoil), weapon selection, muzzle flash, rocket launch |
| `player` | First-person movement with gravity, wall sliding, ground snap; health, `Inventory`; applies `TouchEffect`s |
| `ai` | Enemy state machine (idle → chase → attack, pain, dead); `EnemyDef` table: melee or projectile attack, preferred range |
| `projectile` | Rockets and spit: flight, collision (scene + player cylinder, skipping the shooter), direct + splash damage with line of sight, glow and explosion lights; `ProjectileDef` table |
| `damage` | `damage_entity()`: the one path for hurting an entity (pain/died events) |
| `inventory` | Ammo pools by `AmmoType` (bullets, shells, rockets) with caps; owned weapons as a bitmask |
| `weapon` | `WeaponDef` table (`weapon_types.c`): damage, cooldown, pellets, spread, kick, ammo, view texture, flash, shake, projectile; switch phases (lower → swap → raise); dry fire |
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
- **Event queue:** gameplay pushes `GameEvent`s (type, pos, entity, amount) into `Game.events`, cleared at the start of each `game_update()`. `main.c` maps them to sounds (per weapon / per enemy type tables), screen effects and particles. Presentation reacts; gameplay code stays untouched.
- **Presentation vs simulation:** anything cosmetic (viewmodel bob, shake, flash, particles) lives in `main.c`/`render/` and uses frame `dt`; it never changes game state or aim. Simulation only sees `GAME_TICK_DT`.
- **Temporary lights:** muzzle flashes, projectile glows and explosions are `light_add_temporary()` lights (no shadows, no billboard); they fade and are swap-removed, so never hold a light index.
- **Runtime spawns:** `scene_spawn()` reuses a spent slot of the same type and uses `scene.type_material[type]` (prepared by `level_finish()` for `runtime_only` types). Spawning can grow the entity array: re-fetch any `Entity *` held across a spawn (see `spit()` in `ai.c`).
- **Weapon switching:** `PlayerCmd.weapon_slot` / `weapon_cycle` are impulses (applied once, see `cmd_accumulate`); `game.c:select_weapon()` → `weapon_request()`; only `WEAPON_READY` fires. Projectile weapons set `Weapon.launch_this_tick`; `game.c` fires (weapon only gets a read-only scene).
- **Hit → entity:** `Triangle.owner`/`HitRecord.owner` is an opaque tag at the `gfx` layer. Entity triangles and sprites get `owner = index + 1`; call `entity_hit(&rec)` for the entity index or `ENTITY_INDEX_NONE`. Zero-initialized triangles read as world.
- **Trace masks:** rendering uses `TRACE_VISIBLE`; anything that blocks (player/enemy movement, bullets, projectiles, enemy sight) uses `scene_trace(..., TRACE_SOLID)`, which skips ghost sprites (corpses, projectiles) and ghost meshes (pickups, exit pad: `EntityDef.mesh_solidity`). Sprites cast no shadows (`scene_occluded()` skips them). The player's ground probe looks past steep surfaces for real ground.
- **Sprites:** RENDER_SPRITE entities implement `EntityDef.sprite` to pick size, frame and solidity per tick. Textures use magenta (`TEXTURE_KEY_*`) for holes; sheets hold frames side by side, rows top-down.
- **Touch:** `entity_overlap()` finds touchables in the body cylinder without side effects; the player applies the type's `TouchEffect` (heal, ammo, exit) and only then calls `entity_touch()`. Health/ammo at cap leave pickups in place. Origins up to `TOUCH_FOOT_SLACK` below the feet count (floor pads).
- **Damage:** bullets emit `EV_WEAPON_IMPACT` (entity, amount) and `resolve_impacts()` applies them; projectiles damage directly. Both go through `damage_entity()` → type's `damage` hook → `EV_ENEMY_PAIN`/`EV_ENEMY_DIED`. The player is hurt via `player_damage()` (enemies, spit, your own rocket splash).
- **Enemies:** world owns health and `EnemyState` (and draws frames from it); `game/ai.c` owns transitions and stats (`EnemyDef`). Melee enemies close to reach; ranged ones hold at `preferred_range` with line of sight and fire a projectile only if they still see you after the windup. Enemies move on a fixed height (no gravity/steps yet) and walk straight at the player.
- **Materials:** `Scene` holds parallel arrays of `Material` and `Texture`. Triangles reference materials by index. A material's texture is a procedural texture name or `none`; fallback is checkerboard. Loaders resolve every name through `MatLib`: `.map` faces and `mat=` may name a texture directly; entities without `mat=` get their type's `default_texture`/`default_color`.
- **Entity keys:** each `EntityDef` lists its keys (`EntityKey`: name, `KeyType`, default, description). The `.map` loader converts `KEY_LENGTH` from units to meters; both loaders warn on unknown keys; the FGD is generated from the same list. Keep spawn-hook defaults and key defaults on the same constant.
- **Map space:** Z up, 32 units = 1 m; `map_to_engine()` is (x, y, z) → (x, z, −y)/32. Map `angle` (0 = east) → engine yaw via `map_angle_to_yaw()` (−90°).
- **Hot reload:** `main.c` polls the level file 4×/s; `game_reload()` loads into a fresh `Scene` first, so a broken save keeps the old level. The player keeps position and view; the world, enemies and health reset.
- **Ray robustness:** `ray_triangle_intersect()` allows `BARY_EPS` slack so rays exactly on shared edges never fall through a crack.
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

Entity types (all take `rot=` (radians) and `mat=<material or texture name>`; without `mat=` the type's default material is used):

| Type | Keys | Notes |
|------|------|-------|
| `health` | `amount` (25) | heals, not taken at full health |
| `ammo` | `amount` (10), `ammo` (`bullets`/`shells`/`rockets`) | not taken at cap |
| `weapon` | `give` (`shotgun`/`launcher`), `amount` (8) | weapon + that much ammo; switches to it if new |
| `door` | `hinge_x`, `hinge_z` | E toggles |
| `prop` | — | static cube |
| `decor` | `w` (0.8), `h` (1.0), `solid` (1) | sprite; default texture `barrel` |
| `ghoul` | `health` (60) | melee enemy sprite; default texture `ghoul` |
| `spitter` | `health` (45) | ranged enemy sprite (spits); default texture `spitter` |
| `exit` | — | floor pad; touching it ends the level |

`room` generates inner-facing geometry (you're inside it). `box` generates outer-facing geometry (obstacles). UVs auto-tile based on world dimensions.

### TrenchBroom (.map) Levels

Setup once: `make editor`, then in TrenchBroom Preferences → RT → Game Path = `<repo>/editor/game`. The game config is versioned in `editor/trenchbroom/RT/`; textures and `rt.fgd` there are generated (git-ignored) — rerun `make editor` after changing textures or entity types.

- Brushes in `worldspawn`, `func_group` (TrenchBroom layers/groups) and `func_detail` become static geometry. Other brush entities are ignored for now (Phase 3 triggers/doors).
- Point entities: `info_player_start` (origin = feet, `angle`), `light` (`intensity`, `radius` in units, `color`, `shadows`, `flicker_*`, `cord` in units, `swing_*`), and every entity type by name with its keys. Lengths are in units.
- Textures `skip`/`clip` produce no geometry. Use Valve 220 format (the config's only format); Standard faces also load.
- Doors in `.map`: place the entity at the hinge (`hinge_x/z` are `.level` only).
- Run `./rt levels/foo.map`; saving in TrenchBroom reloads the game. Check the terminal for `check:` warnings and the `budget:` line; F3 shows the same budgets live.

### Adding Content

**New level:** Build it in TrenchBroom (see above) and save as `levels/<name>.map`, or hand-write a `.level` (see `levels/e1m1.level`). Budgets: 30k triangles, 32 lights, 40 sprites (`levelcheck.h`; measured: ~0.12 ms per light, ~0.11 ms per sprite).

**New procedural texture:** Add to `ProceduralTextureID` enum in `proctex.h`, write generator function in `proctex.c`, add to `proctex_names[]` and `proctex_generators[]` arrays. Sprite textures fill empty pixels with the color key; preview with an offscreen render, since tests can't judge looks.

**New entity type:** Add to `EntityType` enum in `entity.h`, then one `EntityDef` row in `entity_types.c` with its level name, `RenderKind`, default material, `desc`, `keys`, and only the hooks it needs (NULL = no behavior). Level parsing (both formats), update, touch, use, mesh build and the FGD pick it up automatically — never add `switch (e->type)` to generic code. Entity indices are stable (inactive entities stay in the list). Run `make editor` to see it in TrenchBroom.

**New weapon:** Add to `WeaponType` in `weapon.h` (enum order = number key) and one row in `weapon_types.c`: stats, `ammo`, `view_texture` (a 2-frame `view_*` sheet in `proctex.c`), flash, `shake`, and `projectile` (`PROJECTILE_NONE` = hitscan). Shotgun-style = `pellets > 1` + `spread`. Add its fire sound to `weapon_fire_sounds[]` in `main.c`. Levels give it with `entity weapon ... give=<name>`.

**New projectile:** `ProjectileKind` + `ProjectileDef` row in `game/projectile.c` (speed, damage, splash, glow, burst light) + a frame in the `projectiles` sheet (`ProjectileFrame`).

**New enemy:** `EntityType` + `EntityDef` row (`RENDER_SPRITE`, `spawn` via `enemy_spawn()`, `damage = enemy_damage`, `sprite` via `enemy_sprite()` with its size, `default_texture`) + `EnemyDef` row in `game/ai.c` (melee or projectile) + an `EnemyFrame` sheet in `proctex.c` + a row in `enemy_sounds[]` in `main.c`.

**New light:** Use the `light` directive in a level file. Keys: `shadows`, `flicker_speed`, `flicker_amount`, `anchor_x/y/z`, `cord`, `swing_speed`, `swing_angle`.

**New sound:** Add to `SoundID` enum, generate in `audio_generate_sounds()` or load WAV via `audio_load_wav()`. Push a `GameEvent` from gameplay and map it in `play_event_sounds()`.

**New PS1 effect:** Add to `ps1_effects.c`. Current effects are applied per-vertex (jitter) or per-pixel (dither/quantize).

## Tests

Each `tests/test_*.c` is a standalone binary using `tests/test_framework.h` (`TEST`, `RUN_TEST`, `TEST_SUITE`, `TEST_REPORT`).

- **New suite:** just add `tests/test_<name>.c`. Tests link against `build/librt.a` (all objects except `main.o`), so no Makefile edit is needed.
- **New source dir under `src/`:** picked up automatically (sources and include path).
- **Run from repo root.** Tests load fixtures by relative path (`tests/test.level`, `tests/test.map`) and write scratch files under `build/tests/`.
- **`tests/test.map`** is generated by `tests/gen_test_map.py` so every brush face winds outward; regenerate rather than hand-edit.
- **`tests/test.level`** spawns the player outside its room (no floor). The checker flags it; existing tests depend on that spawn, so leave it.

Controls: WASD move, mouse look, Space jump, LMB shoot, 1–3 / mouse wheel switch weapon, E use, F flashlight, F3 debug overlay, Esc quit. Dead or at the exit: LMB/E (after 1 s) restarts the level.
