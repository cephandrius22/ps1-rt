# Roadmap: Moody PS1-Style FPS

Goal: a dark, atmospheric shooter in the spirit of Doom/Quake, rendered by a
CPU raytracer with a PS1 look. Phases are ordered by what makes the game
playable soonest. Each phase ends in something you can play.

## Done

- Collision & physics: gravity, jump, step climbing, wall sliding
- Fixed 60 Hz simulation driven by `PlayerCmd`, events to presentation
- Pistol, shotgun and rocket launcher from a `WeaponDef` table, with
  viewmodels, muzzle flashes, recoil, shake
- Two-level BVH plus sprites; data-driven entities: pickups, weapons,
  keycards, props, decor sprites, ghoul and spitter enemies, projectiles,
  exit pad; brush entities: doors, buttons, triggers
- Enemies path over a nav grid, climb steps, fall, and hear gunfire
- Particles, temporary lights, damage flash
- Health, death, respawn, level end; HUD
- Point lights, shadows, flicker, swinging bulbs, flashlight, fog
- 8-voice mixer, procedural sounds (weapon, footsteps, creature voices),
  ambient loop
- Level file format, procedural textures
- Multithreaded rendering

## Phase 0: Structure ✅

Removed lock-ins that Phase 1+ would otherwise build on.

```
  main.c          wires platform + game + render, owns the loop
     |
  game/ render/   game state, player, weapon, ai, inventory, events
     |            | shading, overlay
  world/          scene, entity, sprite, light, level loader
     |
  gfx/            camera, mesh, bvh, texture, proctex, ps1 effects
     |
  core/           math3d, threadpool, kv (shared foundation)

  platform/       SDL input device, audio device
                  (talks to game only through commands/events)
```

- ✅ **Directories by layer.**
- ✅ **Commands, not keys.** `PlayerCmd`; AI/demos can drive any actor.
- ✅ **Event queue.** `GameEvent` (type, position, entity, amount).
- ✅ **Hits know what they hit.** `HitRecord.owner` + `entity_hit()`.
- ✅ **Data-driven entity types.** `EntityDef` hooks + `RenderKind`.
- ✅ **Data-driven weapons.** `WeaponDef` (damage, cooldown, range,
  pellets, spread, kick).
- ✅ **Gameplay doesn't draw.** `render/overlay` fed by `OverlayState`.
- ✅ **Fixed-timestep logic.** 60 Hz ticks, input accumulated so look
  and impulses apply once.
- ✅ **Game state.** `Game` owns scene, player, weapon, events, mode.
- ✅ **Tests link a static lib.**
- ✅ **Cleanup.** Legacy `render_scene()` and BVH-only update paths gone.
- ✅ **Pose, not Camera.** Player and weapons use `Pose`; camera is
  derived at render time.

Carried into later phases:

- Phase 1 (done): `damage` hook, generic health, ammo type on
  `WeaponDef`, game modes, weapon `kick` applied to the view.
- Phase 4: environment settings and one light model (below).
- Phase 6: move SDL window code out of `main.c` into `platform/`.
- Phase 7: interpolate rendering between ticks (displays above 60 Hz
  show each tick on several frames).

## Phase 1: Vertical slice ✅

One level that is fun to fight in.

- ✅ Player health, damage, death, respawn (level reload)
- ✅ Ammo pools (`Inventory`), consumption, dry fire, ammo pickups
- ✅ HUD: health, ammo; red death tint; level-end kill count
- ✅ Sprite rendering: billboards, color-key holes, frame sheets,
  solid vs ghost (corpses)
- ✅ Ghoul enemy: idle → chase → attack, pain, death; line of sight via
  `scene_trace(TRACE_SOLID)`
- ✅ Procedural sounds: enemy alert/attack/pain/death, player pain/death,
  dry fire
- ✅ Exit pad ends the level; game modes: dead, level end
- ✅ Weapon `kick` applied to view and aim

Carried forward:

- ~~Phase 3: enemies walk straight at the player on a fixed height;
  only sight wakes them~~ (done in Phase 3)
- Phase 5: exit replays the level instead of loading the next.
  (`mat=` by name: done in Phase 1.5.)
- Phase 7: sprites are a linear scan per ray (~1.6 ms/frame for the six
  in e1m1).
- Playtest: sounds and fight balance are untested by ear/hand.

## Phase 1.5: Tooling ✅

Make levels cheap to build and iterate on before designing them for real.

- ✅ Materials by name: `mat=<material or texture>`; entity types bring
  default materials, so most entities need no `mat=` at all
- ✅ Hot reload: saving the level file reloads it in place (player keeps
  position); a broken save keeps the old level
- ✅ TrenchBroom `.map` importer: brushes (Standard + Valve 220),
  layers/groups, `info_player_start`, `light`, every entity type by name;
  32 units = 1 m
- ✅ `make editor`: textures and FGD generated from code (proctex, the
  `EntityDef` key table), game config symlinked into TrenchBroom
- ✅ Level checker: spawn/exit present, player/enemies not stuck or
  floating, unknown keys/classes/textures, budget line
- ✅ Budgets measured: ~0.12 ms per light, ~0.11 ms per sprite, triangles
  cheap; warn above 30k triangles / 32 lights / 40 sprites
- ✅ F3 debug overlay: work time, budget use, position in meters and map
  units
- ✅ Fixed: rays on shared triangle edges fell through (crack); floor
  diagonals now always hit

Carried forward:

- ~~Phase 3: brush entities ignored by the importer~~ (done in Phase 3)
- Phase 5: e1m1 is still a `.level`; port it to `.map` when content work
  starts.
- Not done: noclip (offered, not chosen).

## Phase 2: Combat feel ✅

Plan and design notes: [docs/PHASE2_PLAN.md](docs/PHASE2_PLAN.md)

- ✅ Weapons: pistol, shotgun (7 pellets, shells), rocket launcher
  (projectile, splash incl. self-damage); 1–3 / wheel switching with
  lower/raise; weapon and named ammo pickups
- ✅ Viewmodel per weapon: bob, recoil, switch animation, lit by
  surroundings, PS1-dithered
- ✅ Muzzle flash, projectile glow and explosions as temporary lights
- ✅ Screen shake (camera only) and red damage flash
- ✅ Blood, gibs, sparks, debris, spit as projected, depth-tested particles
- ✅ Spitter: ranged enemy that holds at range and spits dodgeable acid
- ✅ `levels/arena.map` combat test map; weapons and spitters in e1m1
- ✅ Fixed: standing on an untaken pickup dropped the player through the
  floor (pickups are now ghost meshes; ground probe looks past steep faces)

Carried forward:

- ~~Phase 3: enemies walk straight at you on a fixed height~~ (done);
  no infighting (projectiles hurt any enemy, but enemies don't retaliate).
- Phase 4: projectile glows are point lights refreshed per tick; fine at
  current counts, watch the light budget with many spitters.
- Particles land on the player's floor height, not real geometry.
- Playtest: balance (damage, ammo, spitter cadence) is set from numbers,
  not long play sessions.

## Phase 3: Level logic ✅

- ✅ Shared walking bodies (`movement.c`): player and enemies climb the
  same steps (≤ 0.6 m); enemies have gravity
- ✅ Nav grid built at load from static geometry (no designer work):
  A* paths around walls, up stairs, never onto crates or roofs
- ✅ Sound alerting: gunfire floods the nav grid by walking distance;
  walls and closed doors muffle it
- ✅ Brush entities in both formats (`.map` brushes, `.level` `brush`
  boxes): `func_door` (Doom-style, rises; side/down too; never crushes;
  monsters open plain ones), `func_button`, `trigger_once`,
  `trigger_multiple`
- ✅ Keycards (red/blue/yellow), keyed doors and buttons, HUD key icons,
  "You need the blue keycard"
- ✅ Quake wiring: `target` → `targetname` opens doors, releases hidden
  monsters, toggles lights; `message` shows on screen (5x7 font)
- ✅ Checker: exit unreachable on foot, targets naming nothing, named
  doors/monsters nobody targets, keyed doors without their keycard
- ✅ e1m1 fixed (its inner rooms were sealed; the exit was unreachable)
  and rebuilt with a west door, blue-key storage, a gate button and an
  ambush; arena got a stair-trigger ambush
- ✅ The old swinging `door` entity was removed (`func_door` replaces it)

Carried forward:

- Phase 5: doors don't fire their own `target` when opened (Quake
  does); no `trigger_relay`, `delay`, or toggling doors yet.
- Phase 5: e1m1 is still a `.level`; port it to `.map` when content work
  starts.
- Phase 5: nav ignores jumps and other monsters; enemies can bunch up
  in doorways.
- Phase 7: door models are re-posed into the dynamic BVH every tick,
  even when still.
- Playtest: door speed/wait, hearing distances and ambush timing are set
  from numbers, not long play sessions.

## Phase 4: Atmosphere (in progress, in stages)

- ✅ Stage 1 — **Environment in the level.** `Environment` on `Scene`:
  ambient, sun, fog, sky; `.level` lines and `.map` worldspawn keys.
  Sun intensity 0 skips the sun's shadow ray: e1m1 and arena turned it
  off (invisible underground, ~15% faster single-threaded).
- ✅ Stage 2 — **One light model.** `PointLight` became `Light` with
  `kind` point | spot (`aim`, cone) and per-light `falloff`; one shading
  function for all. The flashlight is a spot built by
  `player_flashlight()` and passed to render as the carried light. Levels
  can place spots (`cone`, `aim`).
- ✅ Stage 3 — **Per-area light and fog** (Doom sectors): `zone` lines /
  `env_zone` brushes override ambient (per surface) and fog (per eye,
  faded in over ~0.6 s). e1m1's west corridor and exit passage use them.
- Ambient soundscapes per zone, simple reverb, stereo panning
- Shootable lights, flashlight battery
- Animated textures: flickering screens, water, slime
- Low health desaturation
- Affine texture warping in shading (`ps1_affine_uv()` exists, unused)

## Phase 5: Content

- Episode of 3–4 levels; exit loads the next level (it replays today)
- Secrets (a `trigger_secret` counted like kills), intermission screen
  (kills, secrets, time)
- Scripting extras as levels need them: doors that fire their `target`,
  `trigger_relay` with `delay`, toggling doors, `trigger_hurt`
- Monster movement as levels demand: avoid each other, jump-down links
  in the nav grid
- TGA textures from level materials (loader exists, not reachable from
  `.level` files)

## Phase 6: Shell

- Main menu, pause, mouse sensitivity
- Save / load
- Move SDL window/present code from `main.c` into `platform/`

## Phase 7: Performance (when profiling demands)

- Refit dynamic BVH instead of rebuilding (still doors re-posed every tick)
- Tile-based rendering
- Cull sprites per frame (bounding check or put them in the dynamic BVH)
  instead of scanning all of them per ray
- Interpolate rendering between fixed ticks

## Design notes

- Enemies are sprites (Doom look, cheap per pixel), decided Phase 1.
  `RenderKind` keeps meshes possible per type.
- Two-level BVH for dynamic objects
- PS1 aesthetic throughout: vertex jitter, dithering, 320x240; affine
  texture warping planned (Phase 4)
- CPU raytracing only, no GPU shaders
