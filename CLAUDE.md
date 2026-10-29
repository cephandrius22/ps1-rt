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
./rt             # play the episode (levels/episode1.txt)
./rt levels/foo.level  # play one .level or TrenchBroom .map; reloads on save
./rt levels/other.txt  # play another episode file
make editor      # export textures + FGD, link TrenchBroom game config
./rt --convert in.level out.map  # hand-written .level → TrenchBroom .map
```

Requires: SDL2 (`sdl2-config`), C compiler, pthreads, `-lm`.

Windows builds from an MSYS2 UCRT64 shell (gcc, make, SDL2 via pacman; winpthreads supplies pthreads). Platform differences are kept in two places only:
- **Makefile:** one `ifeq ($(OS),Windows_NT)` block sets `CC`, `EXE`, `TB_GAMES`, `REPO_DIR`, SDL libs (drops `-mwindows` so the console stays); `make editor` copies the config instead of symlinking. `SDL_CONFIG` can point at another `sdl2-config` (cross builds).
- **Code:** a `/* --- Platform: ... --- */` block per file, one whole function per platform (`threadpool.c` CPU count, `filewatch.c` stat mtime). Never `#ifdef` inside other functions; use `MATH_PI` (math3d.h), not `M_PI`. Tests avoid POSIX-only calls (`utime()`, `remove()`, files under `build/tests/`).
- **Cross-check from macOS** (compiles, can't run): `brew install mingw-w64`, unpack the official `SDL2-devel-<ver>-mingw.tar.gz`, then
  `make OS=Windows_NT CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar BUILD=build-win SDL_CONFIG=<sdl>/x86_64-w64-mingw32/bin/sdl2-config rt.exe` (add `build-win/tests/test_<name>.exe` targets for tests). GCC there warns about things clang doesn't; keep it at zero warnings.

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
  shell/          menus (title, pause, options, save/load): a state machine
                  main.c drives; may use platform/settings and render's
                  OverlayMenu, nothing else
  game/  render/  game, player, movement, weapon, ai, logic, projectile, damage,
                  inventory, cmd, events | shading, overlay, font, particles, screenfx
  world/          scene, entity, sprite, light, environment, nav grid, level loader
  gfx/            camera, mesh, bvh, texture, proctex, ps1_effects
  core/           math3d (Vec3, Ray, Pose), threadpool, kv
  platform/       SDL: display (window, present), input, audio, clock; file
                  watch; talks to game via PlayerCmd only. main.c has no SDL
  tools/          editor export (`./rt --export-editor`), .level → .map
                  converter (`./rt --convert`), invoked by main
```

- **PlayerCmd (`game/cmd.h`) is the only input contract.** `input_poll()`
  maps keys/mouse to it (bindings + sensitivity live in `platform/input.c`).
  Game code must never include `input.h` or SDL, so AI or demo playback
  can drive any actor with the same struct.

### Core Pipeline

`main.c` per frame: `input_poll()` → menu (the title menu opens at start over the paused first map; Esc opens pause; while a menu is open `menu_update()` runs instead of the game, and main carries out its `MenuAction`) → hot reload check (`filewatch_poll()` → `game_reload()`) → `game_update()` → events to presentation (`play_event_sounds()` placed by `play_at()`, `play_event_effects()` shake/flash, `spawn_event_particles()`) → camera = `game_view()` + shake → view environment faded toward the camera's zone (`fade_view_env()`: fog, reverb, ambience loop) → `render_scene_lit_mt()` (color + depth) → `particles_draw()` → `overlay_draw()` (viewmodel, flash, HUD, F3 lines; an open menu instead of HUD) → `display_present()`

`game_update()` runs 0..`GAME_MAX_TICKS_PER_FRAME` fixed ticks at `GAME_TICK_HZ` (60). Each `game_tick()`: entities → `logic_update()` (doors, buttons, triggers) → lights → rebuild dynamic BVH + sprites → then by `GameMode`:
- `GAME_PLAYING`: AI → use (`logic_use()`) → player (exit pad → `GAME_LEVEL_END`; new weapon pickups → switch) → weapon select → weapon (muzzle flash light, noise wakes enemies via `ai_hear()`, rocket launch request) → `resolve_impacts()` → `projectile_update()` → player dead → `GAME_DEAD`. Kills are counted from `EV_ENEMY_DIED` events; on-screen notices (`game_message()`) are written from this tick's events.
- `GAME_DEAD`: AI and projectiles keep running, view sinks; attack/use after `GAME_CONTINUE_DELAY` restarts the map with the loadout it was entered with (`Game.entry`)
- `GAME_LEVEL_END`: frozen, intermission shown (map title, kills, secrets, time from `Game.map_time`, next map); attack/use after the delay loads the next map of the episode, carrying health, ammo and weapons (not keycards); after the last map, `GAME_EPISODE_END`, whose continue starts the episode fresh

Rendering path: `camera_ray()` → `scene_intersect()` (two-level BVH + sprites) → `shade_pixel_lit()` (material motion, optional affine UVs, zone ambient + glow, `shade_light()` per light, view fog) → PS1 dither → framebuffer → `particles_draw()` → `overlay_draw()` (viewmodel, desaturation, HUD, death tint, level-end kill count)

### Module Map

| Module | Purpose |
|--------|---------|
| `math3d.h` | Vec3, Mat4, Ray, UV, `Pose` (pos + yaw/pitch) — header-only |
| `camera` | Render camera (`camera_from_pose()`), ray per pixel, `camera_project()` (world point → pixel, its inverse), `camera_to_screen()` (unclamped, for affine UVs) |
| `mesh` | Triangle storage, ray-triangle test (`HitRecord.tri` points at the hit triangle), OBJ loader with fan triangulation |
| `bvh` | SAH-based BVH build, stack-based iterative traversal |
| `scene` | Static world BVH + dynamic entity BVH + sprite list (both rebuilt per tick); `scene_trace()` with `TraceMask`; `scene_spawn()` for runtime entities (reuses spent slots, `type_material`) |
| `sprite` | Upright billboards facing each ray, color-keyed holes, frame sheets; `SPRITE_SOLID`/`SPRITE_GHOST` |
| `entity` | Entity storage + type-agnostic dispatch (update, touch, damage, mesh build); brush entities (`entity_spawn_brush()`, per-entity models, `entity_bounds()`); `entity_find_usable()` |
| `entity_types` | `EntityDef` table: per-type name, `RenderKind`, `TouchEffect`, `mesh_solidity`, `runtime_only`, `brush`, `usable`, hooks (spawn/update/touch/damage/pose/build_mesh/sprite); shared enemy hooks (`enemy_damage`, `enemy_sprite`); movers (`func_door`, `func_button`), triggers, keycard |
| `nav` | Walkable grid built at load from static geometry (0.5 m cells, up to 4 floors each, 8-way links over steps ≤ `NAV_STEP`, one-way drop links off ledges ≤ `NAV_DROP_MAX`); A* `nav_next_waypoint()`, sound `nav_flood()`/`nav_heard()`, closed/locked doors (`nav_set_door()`) |
| `kv` | `key=value` lookup in a text line (level args): float, int, string (`"quoted"` values may hold spaces) |
| `render` | Shading: zone ambient + material glow + sun + scene lights + carried light (flashlight), one `shade_light()` for all, shadows; fog + sky from `RenderView.env` (else `scene->env`); scrolling materials; affine UVs (`RenderView.affine`); light billboards; writes a depth buffer; `render_light_at()` (viewmodel light probe) |
| `menu` | `shell/`: title/pause/options/save/load screens as a state machine; `MenuInput` in, `MenuAction` (new game, save/load slot, quit, settings changed/done) and an `OverlayMenu` view out; owns its row layout (mouse hit-tests match the drawing) |
| `overlay` | Viewmodel (weapon sheet, bob/recoil/switch offsets, lit + dithered), low-health desaturation, damage flash, HUD (health, ammo, keycard icons), message line, death tint, level-end kill count, F3 text. Caches viewmodel textures (`Overlay`); fed an `OverlayState` by `main.c` |
| `font` | 5x7 pixel text (`font_draw()`, `font_width()`) for messages; HUD numbers and F3 lines keep overlay's 3x5 font |
| `particles` | Blood, gibs, sparks, debris, spit: projected and splatted after the raytrace, depth-tested; presentation only |
| `screenfx` | Screen shake (camera only, never aim) and full-screen color flash; decays with frame time |
| `environment` | Level mood on `Scene.env`: ambient, sun (direction, color, intensity; 0 = no sun and no sun shadow rays), fog (start/end/color), sky gradient; set by `.level` lines or `.map` worldspawn keys (`environment_set()`). Zones (`Scene.zones`, `EnvZone` boxes) override ambient per surface and fog per eye |
| `light` | One light model (`Light`): point or spot (`LIGHT_SPOT`: `aim`, cone), radius window + `falloff`; attenuation, flicker, swing; temporary lights with a lifetime and fade (`light_add_temporary()`), swap-removed; `targetname` + `off` (toggled by triggers) |
| `texture` | Nearest-neighbor sampling; TGA load (level textures from `textures/`) and save (editor export) |
| `proctex` | Procedural textures: concrete, brick, metal_panel, metal_grate, rust, floor_tile, pipe; seamless animated surfaces slime, water, screen; color-keyed sprite sheets (`Sheet` helpers): barrel, ghoul + spitter (`EnemyFrame`), projectiles (`ProjectileFrame`), keycards (`KeycardFrame`), viewmodels `view_*` (`ViewFrame`) |
| `level` | `level_load()` dispatches by extension; `.level` text loader; `level_add_brush_entity()` and `level_finish()` (jitter, textures, BVH, nav grid) shared by both loaders |
| `mapfile` | TrenchBroom/Quake `.map` loader: brushes → triangles (plane clipping), Standard + Valve 220 UVs, point and brush entities |
| `matlib` | Named material registry shared by both loaders; resolves texture names (procedural, else `textures/<name>.tga`, or a `.tga` path), generating/loading on first use; entity default materials; default motion per texture (`texture_motions[]`: slime/water scroll, slime/screen glow) and `matlib_motion()` overrides |
| `levelcheck` | Load-time warnings (spawn/exit, stuck bodies, no floor, exit unreachable on foot, target wiring, missing keycards) and budget line (`BUDGET_*`) |
| `filewatch` | Polls a file's mtime; reports a change once it has settled (hot reload) |
| `level_convert` | `./rt --convert in.level out.map`: each `.level` line to its `.map` form (rooms become wall slabs); warns on what's lost: `quad`/`obj` lines (skipped), flat-color materials (draw untextured), material motion keys |
| `editor_export` | Writes TrenchBroom textures (TGA) and `rt.fgd` from the proctex generators and `EntityDef` table |
| `ps1_effects` | Vertex jitter (1/128 grid, applied at level load), 4x4 Bayer dither + 15-bit quantize (per pixel); `ps1_affine_uv()` used by render's F4 affine warp |
| `save` | Full-state saves (`saves/quick.sav`, `slot1..5.sav`): header with struct sizes (other builds refused), info block for menus (`save_peek()`), game fields, every entity and light; `game_load()` reloads the map, checks entities match, restores state |
| `episode` | Episode files (`title`, `map <path> <title>` lines); `episode_single()` wraps a lone level |
| `game` | `Game` struct: owns scene, player, weapon, events, mode, kills, message; fixed-timestep `game_update()`, `game_view()` (view + recoil), weapon selection, muzzle flash, gunfire noise, rocket launch, notices from events |
| `player` | First-person movement with gravity, steps, ground snap; health, `Inventory`; applies `TouchEffect`s; `player_flashlight()` (spot light from the view) on a battery (drains while on, recharges while off, dims/flickers when low) |
| `movement` | Shared walking bodies (`BodyShape`): `movement_slide()` (walls, X then Z), `movement_ground()` (steps, looks past steep faces) — player and enemies |
| `ai` | Enemy state machine (idle → chase → attack, pain, dead); gravity; chase straight or along nav waypoints; `ai_hear()` wakes idle enemies; `EnemyDef` table: melee or projectile attack, preferred range |
| `logic` | Doors (open/wait/close, never crush, monsters open plain ones, nav sync), keycard checks, buttons, triggers, `logic_fire()` (target → targetname: doors, hidden monsters, lights) |
| `projectile` | Rockets and spit: flight, collision (scene + player cylinder, skipping the shooter), direct + splash damage with line of sight, glow and explosion lights; `ProjectileDef` table |
| `damage` | `damage_entity()`: the one path for hurting an entity (pain/died events) |
| `inventory` | Ammo pools by `AmmoType` (bullets, shells, rockets) with caps; owned weapons and `Keycard`s as bitmasks |
| `weapon` | `WeaponDef` table (`weapon_types.c`): damage, cooldown, pellets, spread, kick, ammo, view texture, flash, shake, noise, projectile; switch phases (lower → swap → raise); dry fire |
| `event` | `GameEvent` queue: gameplay pushes, presentation drains each frame |
| `cmd` | `PlayerCmd`: move axes, look deltas, button bitmask (held + impulse) |
| `input` | SDL2 keyboard/mouse → `PlayerCmd`, relative mouse mode |
| `display` | Game window: opens it, uploads the framebuffer to a streaming texture, presents (vsync) |
| `clock` | High-resolution seconds for frame timing; a per-run seed |
| `files` | Portable `files_make_dir()` (POSIX `mkdir` / Windows `_mkdir`) |
| `settings` | Player preferences in `rt.cfg` (git-ignored): mouse sensitivity, master volume, vertical FOV; ranges (`SENSITIVITY_*`, `VOLUME_*`, `FOV_*`), clamped on load |
| `audio` | 8-voice stereo mixer via SDL2 audio callback (`audio_mix()`, testable without a device): per-voice pan, volume changes, reverb (two cross-fed delay lines); procedural sounds incl. ambient loops (drone, drip, hum); WAV loader ready |
| `threadpool` | pthread fork-join pool, generation-based sync, splits scanlines across workers |

### Key Patterns

- **Level files:** shipped levels are TrenchBroom `levels/*.map`, played in the order of `levels/episode1.txt`; `.level` text is for hand-written tests (`tests/test.level`) and converts to `.map`. `level_load()` picks the loader by extension; both resolve materials, build geometry, place entities/lights, and end in `level_finish()` (static BVH, nav grid).
- **Two-level BVH:** `scene.static_bvh` is built once at level load. `scene.dynamic_bvh` is rebuilt each tick from entity triangles. `scene_intersect()` tests both.
- **Procedural textures:** `proctex.c` generates 32x32/64x64 textures in code using hash-based noise. Materials in level files reference them by name (e.g., `concrete`, `brick`).
- **Multithreaded render:** `render_scene_lit_mt()` splits screen rows across thread pool workers. Each worker gets a pre-assigned chunk — no work stealing.
- **Fixed timestep:** gameplay only sees `GAME_TICK_DT`, never frame time. Input is folded into `Game.pending` (`cmd_accumulate`): look deltas and impulses apply exactly once even when a frame runs 0 or several ticks. Add one-shot buttons to `CMD_IMPULSES`.
- **Pose, not Camera, in gameplay:** `Player.view` is a `Pose`; weapons aim with a `Pose`. `main.c` builds the render `Camera` (adds FOV). Game code never includes `camera.h`.
- **Event queue:** gameplay pushes `GameEvent`s (type, pos, entity, amount) into `Game.events`, cleared at the start of each `game_update()`. `main.c` maps them to sounds (per weapon / per enemy type tables), screen effects and particles. Presentation reacts; gameplay code stays untouched.
- **Presentation vs simulation:** anything cosmetic (viewmodel bob, shake, flash, particles) lives in `main.c`/`render/` and uses frame `dt`; it never changes game state or aim. Simulation only sees `GAME_TICK_DT`.
- **One light model:** every light — lamps, spots, muzzle flashes, the flashlight — is a `Light` shaded by `render.c:shade_light()` (cone × facing × attenuation × intensity, shadow ray within the per-pixel budget). The flashlight is built each frame by `player_flashlight()` and passed to render as the *carried* light; it is not in the scene's list.
- **Animated materials:** `Material.scroll` (UV/s), `glow` (self-lit, added to ambient, so it shows in the dark; fog still applies) and `pulse` (Hz). Textures bring defaults through `matlib.c:texture_motions[]`, so `.map` faces animate without setup; `.level` material keys override.
- **PS1 affine warp (F4):** `RenderView.affine` swaps a hit's UVs for ones interpolated across the screen (`render.c:affine_uv()`, needs `HitRecord.tri`), skipping triangles with a vertex behind the camera. Presentation toggle via `UiCmd`, like F3.
- **Shootable lights:** a hitscan pellet that passes a lit `breakable` bulb (`light_bulb_hit()`, bulb radius = what render draws) before hitting anything stops there: `EV_LIGHT_SHOT` (entity = light index, valid that tick) → `game.c:break_lights()` sets `off` + `broken`; triggers skip broken lights.
- **Temporary lights:** muzzle flashes, projectile glows and explosions are `light_add_temporary()` lights (no shadows, no billboard); they fade and are swap-removed, so never hold a light index.
- **Runtime spawns:** `scene_spawn()` reuses a spent slot of the same type and uses `scene.type_material[type]` (prepared by `level_finish()` for `runtime_only` types). Spawning can grow the entity array: re-fetch any `Entity *` held across a spawn (see `spit()` in `ai.c`).
- **Weapon switching:** `PlayerCmd.weapon_slot` / `weapon_cycle` are impulses (applied once, see `cmd_accumulate`); `game.c:select_weapon()` → `weapon_request()`; only `WEAPON_READY` fires. Projectile weapons set `Weapon.launch_this_tick`; `game.c` fires (weapon only gets a read-only scene).
- **Hit → entity:** `Triangle.owner`/`HitRecord.owner` is an opaque tag at the `gfx` layer. Entity triangles and sprites get `owner = index + 1`; call `entity_hit(&rec)` for the entity index or `ENTITY_INDEX_NONE`. Zero-initialized triangles read as world.
- **Trace masks:** rendering uses `TRACE_VISIBLE`; anything that blocks (player/enemy movement, bullets, projectiles, enemy sight) uses `scene_trace(..., TRACE_SOLID)`, which skips ghost sprites (corpses, projectiles) and ghost meshes (pickups, exit pad: `EntityDef.mesh_solidity`). Sprites cast no shadows (`scene_occluded()` skips them). The player's ground probe looks past steep surfaces for real ground.
- **Sprites:** RENDER_SPRITE entities implement `EntityDef.sprite` to pick size, frame and solidity per tick. Textures use magenta (`TEXTURE_KEY_*`) for holes; sheets hold frames side by side, rows top-down.
- **Touch:** `entity_overlap()` finds touchables in the body cylinder without side effects; the player applies the type's `TouchEffect` (heal, ammo, weapon, keycard, exit) and only then calls `entity_touch()`. Health/ammo at cap leave pickups in place. Origins up to `TOUCH_FOOT_SLACK` below the feet count (floor pads).
- **Damage:** bullets emit `EV_WEAPON_IMPACT` (entity, amount) and `resolve_impacts()` applies them; projectiles damage directly. Both go through `damage_entity()` → type's `damage` hook → `EV_ENEMY_PAIN`/`EV_ENEMY_DIED`. The player is hurt via `player_damage()` (enemies, spit, your own rocket splash).
- **Enemies:** world owns health and `EnemyState` (and draws frames from it); `game/ai.c` owns transitions and stats (`EnemyDef`). Melee enemies close to reach; ranged ones hold at `preferred_range` with line of sight and fire a projectile only if they still see you after the windup. They fall and climb steps like the player (`movement.c`), walk straight at you when the way is clear, else follow `nav_next_waypoint()` (re-searched every `REPATH_TIME`; paths may drop off ledges, then gravity takes them down). Chasers steer apart (`separation()`) so a crowd spreads out. Gunfire wakes idle ones within `WeaponDef.noise` meters of walking distance (`nav_flood()`): walls and closed doors muffle it.
- **Nav grid:** built by `level_finish()` from the static BVH only, so doorways are open floor; `logic.c` marks closed doors (`nav_set_door()`): openable ones stop sound, locked ones (keycard, trigger-only) also stop paths. A* runs backwards from the goal so the start reads its next waypoint off directly; scratch is stamped, never cleared.
- **Movers and triggers:** world stores a mover's slide (`MoverState`, `dir`, `travel`, `offset`) and poses it; `game/logic.c` runs the state machine, like `ai.c` for enemies. Firing is Quake's: a trigger or button fires its `target`; every entity/light with that `targetname` reacts (door opens, hidden monster appears, light toggles). Named enemies start inactive. `EV_TRIGGERED` lets `game.c` show the source's `message`.
- **Brush entities:** `EntityDef.brush` types are drawn as brushes (`.map`) or `brush` boxes (`.level`). `level_add_brush_entity()` puts the entity at the bounds center (`half_size`) and keeps drawn faces as its own model (`Entity.model`, triangles relative to the entity, own materials); invisible types (triggers) keep only bounds.
- **Materials:** `Scene` holds parallel arrays of `Material` and `Texture`. Triangles reference materials by index. A material's texture is a procedural texture name or `none`; fallback is checkerboard. Loaders resolve every name through `MatLib`: `.map` faces and `mat=` may name a texture directly; entities without `mat=` get their type's `default_texture`/`default_color`.
- **Entity keys:** each `EntityDef` lists its keys (`EntityKey`: name, `KeyType`, default, description). The `.map` loader converts `KEY_LENGTH` from units to meters; both loaders warn on unknown keys; the FGD is generated from the same list. Keep spawn-hook defaults and key defaults on the same constant.
- **Map space:** Z up, 32 units = 1 m; `map_to_engine()` is (x, y, z) → (x, z, −y)/32. Map `angle` (0 = east) → engine yaw via `map_angle_to_yaw()` (−90°).
- **Hot reload:** `main.c` polls the level file 4×/s; `game_reload()` loads into a fresh `Scene` first, so a broken save keeps the old level. The player keeps position and view; the world, enemies and health reset.
- **Ray robustness:** `ray_triangle_intersect()` allows `BARY_EPS` slack so rays exactly on shared edges never fall through a crack.
- **Shadow rays:** Capped at `MAX_SHADOW_LIGHTS` (4) per pixel for performance. Sun shadow is separate.

### Level File Format

```
spawn <x> <y> <z> <yaw_degrees>
material <name> <texture_name> <r> <g> <b> [scroll_u= scroll_v= glow= pulse=]
room <x1> <y1> <z1> <x2> <y2> <z2> <wall_mat> <floor_mat> <ceil_mat>
box <x1> <y1> <z1> <x2> <y2> <z2> <mat>
brush <type> <x1> <y1> <z1> <x2> <y2> <z2> [mat] [key=value ...]
quad <12 floats> <mat>
obj <filename> <x> <y> <z> <rot_y> <scale> <mat>
entity <type> <x> <y> <z> [key=value ...]
light <x> <y> <z> <r> <g> <b> <intensity> <radius> [key=value ...]
ambient <level>
fog <start_m> <end_m> <r> <g> <b>
sun <dx> <dy> <dz> <r> <g> <b> <intensity>   # direction toward the sun; 0 = none
sky <top r g b> <horizon r g b>
reverb <0..1>
ambience <drone|drip|hum|none>
zone <x1> <y1> <z1> <x2> <y2> <z2> [ambient=<a>] [fog="<start> <end> <r> <g> <b>"] [reverb=<r>] [ambience=<name>]
```

Environment lines are optional; anything left out keeps `environment_default()`. Indoor levels should set the sun's intensity to 0: it can't reach them and its shadow ray costs ~15% of a frame. In `.map` files the same four are worldspawn keys (fog distances in units, sun direction in map axes).

Zones are areas with their own light level, fog and/or sound (reverb, ambient loop), like Doom sectors (`env_zone` brush entity in `.map`, textured `trigger`). A surface takes the ambient of the zone it's in; fog follows the zone the camera is in, faded by `main.c:fade_view_env()` (presentation; passed to render in `RenderView.env`). Overlapping zones: the later one wins.

Entity types (all take `rot=` (radians) and `mat=<material or texture name>`; without `mat=` the type's default material is used; values with spaces go in quotes, `message="Like this"`):

| Type | Keys | Notes |
|------|------|-------|
| `health` | `amount` (25) | heals, not taken at full health |
| `ammo` | `amount` (10), `ammo` (`bullets`/`shells`/`rockets`) | not taken at cap |
| `weapon` | `give` (`shotgun`/`launcher`), `amount` (8) | weapon + that much ammo; switches to it if new |
| `func_door` (brush) | `opens` (`up`/`down`/`side`), `speed` (2.5), `wait` (4, -1 = stays), `lip` (0.125), `key`, `targetname`, `toggle`, `target`, `delay` | E opens; monsters open plain ones; `key` = keycard color; named = only a trigger opens it; `toggle` = each firing flips it; fires `target` as it opens |
| `func_button` (brush) | `target`, `message`, `opens` (`side`), `speed`, `wait` (1), `lip`, `key` | E presses: fires `target` |
| `trigger_once` (brush) | `target`, `message` | fires when the player enters, then is spent |
| `trigger_multiple` (brush) | `target`, `message`, `wait` (1) | fires again every `wait` s while inside |
| `trigger_secret` (brush) | `target`, `message`, `delay` | a secret area: counted for the intermission, "A secret area!" |
| `trigger_hurt` (brush) | `damage` (10/s) | bites the player every 0.5 s while inside (slime, fire) |
| `trigger_relay` | `targetname`, `target`, `delay`, `message` | point: when its name fires, fires its target (after `delay`) |
| `keycard` | `color` (`red`/`blue`/`yellow`) | sprite; opens doors/buttons with that `key` |
| `prop` | — | static cube |
| `decor` | `w` (0.8), `h` (1.0), `solid` (1) | sprite; default texture `barrel` |
| `ghoul` | `health` (60), `targetname` | melee enemy sprite; named = hidden until triggered |
| `spitter` | `health` (45), `targetname` | ranged enemy sprite (spits); named = hidden until triggered |
| `exit` | — | floor pad; touching it ends the level |

`room` generates inner-facing geometry (you're inside it) — it is sealed; for rooms you walk between, build walls from `box`es with doorway gaps. `box` generates outer-facing geometry (obstacles). `brush` is a box-shaped brush entity (`mat` optional for invisible triggers). UVs auto-tile based on world dimensions. Lights take `targetname=` and `off=1`.

### TrenchBroom (.map) Levels

Setup once: `make editor`, then in TrenchBroom Preferences → RT → Game Path = `<repo>/editor/game`. The game config is versioned in `editor/trenchbroom/RT/`; textures and `rt.fgd` there are generated (git-ignored) — rerun `make editor` after changing textures or entity types.

- Brushes in `worldspawn`, `func_group` (TrenchBroom layers/groups) and `func_detail` become static geometry. Brush entities (`func_door`, `func_button`, `trigger_*`) become entities; their `angle` is the slide direction for `opens=side`.
- Point entities: `info_player_start` (origin = feet, `angle`), `light` (`intensity`, `radius` in units, `color`, `shadows`, `flicker_*`, `cord` in units, `swing_*`, `targetname`, `off`), and every entity type by name with its keys. Lengths are in units. `target`/`targetname` show as link lines in TrenchBroom.
- Textures `skip`/`clip`/`trigger` produce no geometry (texture trigger brushes with `trigger`). Use Valve 220 format (the config's only format); Standard faces also load.
- Run `./rt levels/foo.map`; saving in TrenchBroom reloads the game. Check the terminal for `check:` warnings and the `budget:` line; F3 shows the same budgets live.

### Adding Content

**New level:** Build it in TrenchBroom (see above), save as `levels/<name>.map` and add a `map` line to `levels/episode1.txt`; or hand-write a `.level` (`tests/test.level` is a small example) and convert it with `./rt --convert in.level out.map` (`tools/level_convert.c`: rooms become wall slabs, everything else maps 1:1; `quad`, `obj` and flat-color materials don't convert). Budgets: 30k triangles, 32 lights, 40 sprites (`levelcheck.h`; measured: ~0.12 ms per light, ~0.11 ms per sprite).

**New procedural texture:** Add to `ProceduralTextureID` enum in `proctex.h`, write generator function in `proctex.c`, add to `proctex_names[]` and `proctex_generators[]` arrays. Sprite textures fill empty pixels with the color key; preview with an offscreen render, since tests can't judge looks.

**New entity type:** Add to `EntityType` enum in `entity.h`, then one `EntityDef` row in `entity_types.c` with its level name, `RenderKind`, default material, `desc`, `keys`, and only the hooks it needs (NULL = no behavior). Level parsing (both formats), update, touch, use, mesh build and the FGD pick it up automatically — never add `switch (e->type)` to generic code. Entity indices are stable (inactive entities stay in the list). Run `make editor` to see it in TrenchBroom.

**New weapon:** Add to `WeaponType` in `weapon.h` (enum order = number key) and one row in `weapon_types.c`: stats, `ammo`, `view_texture` (a 2-frame `view_*` sheet in `proctex.c`), flash, `shake`, and `projectile` (`PROJECTILE_NONE` = hitscan). Shotgun-style = `pellets > 1` + `spread`. Add its fire sound to `weapon_fire_sounds[]` in `main.c`. Levels give it with `entity weapon ... give=<name>`.

**New projectile:** `ProjectileKind` + `ProjectileDef` row in `game/projectile.c` (speed, damage, splash, glow, burst light) + a frame in the `projectiles` sheet (`ProjectileFrame`).

**New enemy:** `EntityType` + `EntityDef` row (`RENDER_SPRITE`, `spawn` via `enemy_spawn()`, `damage = enemy_damage`, `sprite` via `enemy_sprite()` with its size, `default_texture`) + `EnemyDef` row in `game/ai.c` (melee or projectile) + an `EnemyFrame` sheet in `proctex.c` + a row in `enemy_sounds[]` in `main.c`.

**New light:** Use the `light` directive in a level file. Keys: `shadows`, `flicker_speed`, `flicker_amount`, `anchor_x/y/z`, `cord`, `swing_speed`, `swing_angle`, `targetname`, `off`, `breakable` (bullets shoot the bulb out for good), `cone` (spot half-angle in degrees) with `aim_x/y/z` (default straight down; `.map`: `aim` in map axes).

**New brush entity:** `EntityDef` row with `.brush = true` (and `RENDER_MESH` to draw its faces, `RENDER_NONE` for bounds only). Its behavior goes in `game/logic.c` (state on the entity, transitions in game), never in the loaders.

**Wiring a level:** give a trigger/button/door `target=X`; give doors, monsters, relays or lights `targetname=X`. Any firer takes `delay=` (seconds); a `trigger_relay` chains or delays a firing. Chains deeper than `FIRE_DEPTH_MAX` (16) are treated as a loop and stop. The checker warns about targets that name nothing, named doors/monsters nothing targets, keyed doors whose keycard isn't placed, and exits you can't walk to.

**New sound:** Add to `SoundID` enum, generate in `audio_generate_sounds()` or load WAV via `audio_load_wav()`. Push a `GameEvent` (with its `pos`) from gameplay and map it in `play_event_sounds()`; `play_at()` sets volume by distance and pan by direction from the listener. A new ambient loop also needs a row in `ambience_sounds[]` (main.c) so levels can name it.

**New PS1 effect:** Add to `ps1_effects.c`. Current effects are applied per-vertex (jitter) or per-pixel (dither/quantize).

## Tests

Each `tests/test_*.c` is a standalone binary using `tests/test_framework.h` (`TEST`, `RUN_TEST`, `TEST_SUITE`, `TEST_REPORT`).

- **New suite:** just add `tests/test_<name>.c`. Tests link against `build/librt.a` (all objects except `main.o`), so no Makefile edit is needed.
- **New source dir under `src/`:** picked up automatically (sources and include path).
- **Run from repo root.** Tests load fixtures by relative path (`tests/test.level`, `tests/test.map`) and write scratch files under `build/tests/`.
- **`tests/test.map`** is generated by `tests/gen_test_map.py` so every brush face winds outward; regenerate rather than hand-edit.
- **Shipped maps must stay clean:** `test_episode` loads every map in `levels/episode1.txt` and fails on any checker warning (spawn, reachable exit, wiring, budgets).
- **`tests/test.level`** spawns the player outside its room (no floor). The checker flags it; existing tests depend on that spawn, so leave it.

Controls: WASD move, mouse look, Space jump, LMB shoot, 1–3 / mouse wheel switch weapon, E use, F flashlight, F3 debug overlay, F4 PS1 texture warp, F5 quicksave, F9 quickload, Esc pause menu (back inside menus). Dead or at the exit: LMB/E (after 1 s) restarts the level.
