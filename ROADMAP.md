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
- Enemies path over a nav grid, climb steps, jump down ledges, spread
  out, and hear gunfire
- Episode 1, three maps, Doom carry-over between them, secrets,
  intermission tally; scripted levels (relays, delays, hurt zones);
  `.level` → `.map` converter; hand-made TGA textures
- Particles, temporary lights, damage flash
- Health, death, respawn, level end; HUD
- One light model (point and spot), shadows, flicker, swinging and
  shootable bulbs, battery-powered flashlight
- Per-level and per-zone ambient, fog, sky, sun; animated slime, water and
  screens; low-health desaturation; optional PS1 affine warp
- 8-voice stereo mixer with reverb, procedural sounds (weapons,
  footsteps, creature voices), sounds placed in space, ambient loops
  per zone
- Title, pause, options and save/load menus; settings file; save
  anywhere, F5/F9 quicksave
- Level file format, procedural textures (industrial and church sets)
- Church theme: textures, decor sprites (a candelabra that lights
  itself, gravestones, a statue), wind/organ/bell ambience, and
  `levels/chapel.map` to try them
- Multithreaded tiled rendering; faster BVH traversal and refit;
  cached soft shadows for fixed lights; camera smoothed between ticks;
  `./rt --bench`

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
- Phase 4 (done): environment settings and one light model.
- ~~Phase 6: move SDL window code out of `main.c` into `platform/`~~ (done).
- ~~Phase 7: interpolate rendering between ticks~~ (camera eased
  between ticks, Phase 7).

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
- ~~Phase 5: exit replays the level instead of loading the next~~ (episodes, Phase 5).
  (`mat=` by name: done in Phase 1.5.)
- ~~Phase 7: sprites are a linear scan per ray~~ (culled per screen
  tile, Phase 7).
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
- ✅ Budgets measured: ~0.12 ms per light, ~0.11 ms per sprite (after
  Phase 7: ~0.15-0.35 ms per light, ~0.05 ms per sprite), triangles
  cheap; warn above 30k triangles / 32 lights / 40 sprites
- ✅ F3 debug overlay: work time, budget use, position in meters and map
  units
- ✅ Fixed: rays on shared triangle edges fell through (crack); floor
  diagonals now always hit

Carried forward:

- ~~Phase 3: brush entities ignored by the importer~~ (done in Phase 3)
- ~~Phase 5: e1m1 is still a `.level`~~ (ported in Phase 5 step 6).
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
- Still open (see Phase 7): projectile glows are lights refreshed per
  tick; fine at current counts, watch the light budget with many
  spitters.
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

- ~~Phase 5: doors don't fire their own `target`; no `trigger_relay`,
  `delay`, or toggling doors~~ (done in Phase 5).
- ~~Phase 5: e1m1 is still a `.level`~~ (ported in Phase 5 step 6).
- ~~Phase 5: nav ignores jumps and other monsters~~ (drop links and
  separation, Phase 5); monsters still can't jump *up*.
- ~~Phase 7: door models are re-posed into the dynamic BVH every
  tick~~ (refit instead of rebuilt, Phase 7; still re-posed).
- Playtest: door speed/wait, hearing distances and ambush timing are set
  from numbers, not long play sessions.

## Phase 4: Atmosphere ✅

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
- ✅ Stage 4 — **Shootable lights, flashlight battery.** `breakable`
  bulbs go dark for good when shot (glass sound, sparks); e1m1's bare
  bulbs are breakable. The flashlight drains in 90 s, recharges in 30 s
  while off, dims and flickers below 20%; HUD bar while not full.
- ✅ Stage 5 — **Animated textures, desaturation, affine warp.** Slime,
  water and screen textures scroll/glow/pulse (defaults per texture,
  `.level` keys override); color drains below 35 health; F4 toggles PS1
  affine texture warping (screen-space UVs, no measurable cost).
- ✅ Stage 6 — **Sound in space.** Stereo mixer with constant-power pan;
  event sounds placed by distance and direction; reverb (two cross-fed
  delays) and an ambient loop (drone, drip, hum) per level and zone,
  crossfaded as you move. e1m1: dripping corridor, humming storage room,
  echoing exit passage; arena echoes.

Carried forward:

- Still open: sound isn't occluded by walls (the nav grid could muffle
  it, as it does for enemy hearing); no doppler or HRTF.
- Still open: affine warp falls back to true UVs on triangles reaching
  behind the camera; PS1 games subdivided large polygons instead.
- Playtest: fog, zone and battery numbers are set by eye on a few
  screenshots, not long play sessions.

## Phase 5: Content ✅

- ✅ Step 1 — **Episodes.** `levels/episode1.txt` lists the maps; the exit
  loads the next one, Doom carry-over (health, ammo, weapons; not
  keycards), death restarts the map with its entry loadout, an episode
  end screen after the last. A test keeps every shipped map checker-clean.
- ✅ Step 2 — **Secrets and intermission.** `trigger_secret` brushes
  (chime, "A secret area!"); the level end shows map title, kills,
  secrets, time and the next map. e1m1 has a crate-top stash.
- ✅ Step 3 — **Scripting extras.** `delay` on any firer, `trigger_relay`,
  doors that fire their `target` and `toggle` doors, `trigger_hurt`
  (e1m1's slime now burns); relay loops stop after 16 links.
- ✅ Step 4 — **Monster movement.** Chasers steer apart instead of
  stacking; the nav grid has one-way drop links off ledges up to 3 m, so
  monsters jump down after you (never path back up).
- ✅ Step 5 — **TGA textures.** A texture name that isn't procedural loads
  `textures/<name>.tga` (or a `.tga` path); `make editor` copies them
  for TrenchBroom; unknown names warn and fall back to flat color.
- ✅ Step 6 — **`.level` → `.map` converter.** `./rt --convert` maps each
  line to its `.map` form (rooms become wall slabs); e1m1 is now
  `levels/e1m1.map`, edited in TrenchBroom. A round-trip test checks
  entities, keys, lights, zones and geometry survive.
- ✅ Step 7 — **Episode 1, *Descent*: three maps.** e1m2 Pumping Station
  (slime channel, gallery spitters drop from, filter room, blue-locked
  control room) and e1m3 Reactor (slime moat around the core, coolant
  wing, yellow-locked control room whose shutdown springs a six-monster
  ambush and, via a relay, opens the escape). Authored as `.level` text
  and converted; the arena stays a test map outside the episode. Found
  and fixed on the way: the nav grid treated the floor under a thin slab
  (a 2 cm pool) as a separate floor, cutting off pickups inside it.

Carried forward:

- Content: more episodes; arena could become a deathmatch-style bonus map.
- Monster movement: no climbing (only drop links), no infighting, no
  flying or ranged dodging.
- Level authoring: generated maps are plain boxes; real detail (trim,
  arches, varied ceilings) comes from editing them in TrenchBroom.
- Playtest: the three maps were verified by scripted runs (reachability,
  keys, buttons, ambushes, exits) and screenshots, then played by hand.
  First tuning pass: rooms ~25% brighter (ambient ×1.5, lamps ×1.2);
  guns hit harder (more damage, kick, shake, flash, knockback, flinch,
  gore and punchier sounds).

## Phase 6: Shell ✅

- ✅ Step 1 — **SDL out of `main.c`.** `platform/display` (window,
  present) and `platform/clock`; audio initializes its own SDL subsystem.
- ✅ Step 2 — **Settings.** `rt.cfg` holds mouse sensitivity, master
  volume and field of view; input, the mixer and the camera apply them.
- ✅ Step 3 — **Save anywhere.** Full-state saves (entities, lights,
  player, weapon, tallies) checked against the reloaded map; F5/F9
  quicksave/quickload. Saves only load in the build that wrote them.
- ✅ Step 4 — **Menus.** Title screen over the paused first map; Esc
  pause menu (resume, save, load, options, quit); options sliders;
  five save slots labelled with map and time. Keyboard and mouse.
  `shell/menu` is a pure state machine, drawn by the overlay.

Carried forward:

- No key rebinding or invert-mouse yet (only the three options).
- Saves are build-specific (raw structs); a versioned format would
  survive updates.
- No confirmation before overwriting a save slot or quitting.

## Theme: Church ✅

A gothic/church look as the first alternative to the industrial set,
all generated in code (no image files).

- ✅ **Textures** (`proctex_church.c`): ashlar, crypt rubble, flagstones,
  tomb slab, plaster, oak panelling and boards, marble, aisle carpet,
  studded iron, stained glass and a rose window (both glow). Tiles are
  tested for seams.
- ✅ **Decor:** `decor_candelabra` (brings its own flickering light via
  the new `EntityDef.light` hook; `Light.no_bulb`), `decor_gravestone`,
  `decor_statue`; named types with their own sizes.
- ✅ **Ambience:** wind, organ and bell loops that repeat exactly (no
  fade at the seam).
- ✅ **`levels/chapel.map`:** porch, nave, moonlit graveyard (crypt key,
  secret), crypt with the exit. Checker-clean, kept so by a test.

Carried forward:

- Playtest: the chapel was checked by renders and the level checker,
  not played by hand.
- Looks: plaster's flaked patches read as spots; the rose window's
  stone square glows with the glass; sprites lit only from behind show
  as silhouettes.
- No ground texture (grass, earth): the graveyard is paved.
- Converted maps need non-64 px and one-per-face textures rescaled by
  hand (see `level_convert` in CLAUDE.md).
- Next themes: a Victorian manor set (wallpaper, wood floors,
  fireplaces) would reuse the same approach.

## Phase 7: Performance ✅

Measured first: `./rt --bench` renders fixed views of every shipped map
(on the pool, and on one core for steady comparisons) and times ticks.
Each step was its own commit with before/after numbers.

| | frame (mean) | worst view | one core | tick |
|---|---|---|---|---|
| Before | 12.5 ms | 21 ms | 51 ms | 0.15-0.5 ms |
| Steps 1-5 | 7.5-8.3 ms | 12-15 ms | 36 ms | 0.005 ms |
| Step 6 | 5.8 ms | 9.5 ms | 27 ms | 0.005 ms |

- ✅ Step 1 — **Bench.** `./rt --bench [map ...]`; profiling showed BVH
  traversal at two thirds of a frame, half of it shadow rays.
- ✅ Step 2 — **BVH traversal.** Nearer child first, boxes past the best
  hit skipped, 1/dir once per ray, full hit records only for the kept
  triangle; shadow and sight rays stop at the first hit.
- ✅ Step 3 — **Tiles.** 16x16 tiles taken from an atomic counter
  (fixed row bands left workers idle); sprites and glowing bulbs culled
  per tile from their projected screen rects. Renders identical.
- ✅ Step 4 — **Dynamic BVH refit.** Moved entities refit the tree in
  place; it is rebuilt only when one comes or goes.
- ✅ Step 5 — **Smooth camera.** Frames between 60 Hz ticks ease the
  eye from the last tick's spot and apply mouse look at once.
- ✅ Step 6 — **Shadow cache.** Shadow rays had become a third of a
  frame. At load, each static triangle gets a 25 cm grid of samples
  recording which fixed lights it sees (Quake lightmaps, of visibility);
  shading blends the four nearest (soft edges) and tests doors live.
  Every fixed light now casts shadows, not just four per pixel. Load
  takes 0.05-0.1 s longer.

Carried forward:

- Shadow edges are soft at 25 cm; small details (pew legs, thin
  bars) cast blurrier shadows than before. `SHADOW_TEXEL` trades
  sharpness for load time and memory.
- Swinging lamps, the flashlight and muzzle flashes still trace shadow
  rays (capped at four per pixel).
- Only the camera is smoothed: monsters, doors and projectiles still
  move at the 60 Hz tick rate.
- 8 workers reach about 4.8x one core: efficiency cores are slower, and
  the main thread waits during the render.
- Projectile glows are lights refreshed per tick; watch the light budget
  with many spitters.
- Not performance, kept from the old list: sound isn't occluded by
  walls; affine warp falls back on triangles behind the camera.

## Next (not yet planned)

Every planned phase is done. Candidates, gathered from the carried-
forward notes above:

- **Playtest and balance.** Phase 6's menus and saves, the chapel, Phase
  7's soft shadows and smooth camera, and fight balance overall have had
  scripted checks, not long play.
- **Shell polish.** Key rebinding, invert mouse, confirmations before
  overwriting a save or quitting, a versioned save format that survives
  updates.
- **Monsters.** Infighting, climbing (only drop links today), flying or
  dodging enemies, a third enemy type.
- **Content.** A second episode (the church theme is ready for it),
  a Victorian manor texture set, ground textures (grass, earth).
- **Polish.** Smooth monsters, doors and projectiles between ticks (only
  the camera is), sound occlusion by walls, subdividing large polygons
  for the affine warp.

## Design notes

- Enemies are sprites (Doom look, cheap per pixel), decided Phase 1.
  `RenderKind` keeps meshes possible per type.
- Two-level BVH for dynamic objects
- PS1 aesthetic throughout: vertex jitter, dithering, 320x240; affine
  texture warping as an F4 toggle (Phase 4)
- CPU raytracing only, no GPU shaders
