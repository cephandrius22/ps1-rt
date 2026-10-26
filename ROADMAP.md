# Roadmap: Moody PS1-Style FPS

Goal: a dark, atmospheric shooter in the spirit of Doom/Quake, rendered by a
CPU raytracer with a PS1 look. Phases are ordered by what makes the game
playable soonest. Each phase ends in something you can play.

## Done

- Collision & physics: gravity, jump, step climbing, wall sliding
- Fixed 60 Hz simulation driven by `PlayerCmd`, events to presentation
- Hitscan weapons from a `WeaponDef` table (pellets, spread); recoil is
  tracked but not applied, no viewmodel drawn yet
- Two-level BVH, data-driven entities: pickups, doors, props
- Point lights, shadows, flicker, swinging bulbs, flashlight, fog
- 8-voice mixer, procedural sounds, footsteps, ambient loop
- Level file format, procedural textures
- Multithreaded rendering

## Phase 0: Structure ✅

Removed lock-ins that Phase 1+ would otherwise build on.

```
  main.c          wires platform + game + render, owns the loop
     |
  game/ render/   game state, player, weapon, events | shading, overlay
     |
  world/          scene, entity, light, level loader
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

- Phase 1: `damage` entity hook, generic health/velocity/radius, ammo
  type on `WeaponDef`, game modes (dead, level end).
- Phase 1: apply weapon `kick` to the view (tracked, never applied).
- Phase 4: environment settings and one light model (below).
- Phase 6: move SDL window code out of `main.c` into `platform/`.
- Phase 7: interpolate rendering between ticks (displays above 60 Hz
  show each tick on several frames).

## Phase 1: Vertical slice

One room that is fun to fight in.

- Player health, damage, death, respawn
- Ammo consumption
- Minimal HUD: health, ammo
- One enemy: idle → chase → attack → die; line of sight via
  `scene_occluded()`
- Enemy sounds: alert, attack, pain, death
- Exit trigger ends the level
- Game modes: dead, level end
- Apply weapon `kick` to the view

## Phase 2: Combat feel

- Shotgun (a `WeaponDef` row: pellets + spread), one projectile weapon
- Muzzle flash as short-lived light
- Weapon bob, screen shake, damage flash
- Blood/gibs as billboard particles
- Second enemy type: melee + ranged

## Phase 3: Level logic

- Doors back in e1m1 (entity type exists), keys, switches
- `trigger` directive: on enter → open door, spawn monsters, play sound
- Sound alerting: gunfire wakes nearby enemies
- Waypoint graph via `node` directive for enemy pathing

## Phase 4: Atmosphere

- **Environment in the level.** Move sun, ambient, fog out of
  `make_render_params()` into an `Environment` on `Scene`, set by level
  directives (`fog`, `ambient`, `sun`).
- **One light model.** Fold the flashlight into the light list as a
  spot light (`Light.kind` = point | spot). Muzzle flash and enemy glow
  reuse it.
- Per-area light level and fog color (Doom sectors)
- Ambient soundscapes per zone, simple reverb, stereo panning
- Shootable lights, flashlight battery
- Animated textures: flickering screens, water, slime
- Low health desaturation
- Affine texture warping in shading (`ps1_affine_uv()` exists, unused)

## Phase 5: Content

- Episode of 3–4 levels with transitions
- Secrets, intermission screen (kills, secrets, time)
- Hot-reload level files
- TGA textures from level materials (loader exists, not reachable from
  `.level` files)

## Phase 6: Shell

- Main menu, pause, mouse sensitivity
- Save / load
- Move SDL window/present code from `main.c` into `platform/`

## Phase 7: Performance (when profiling demands)

- Refit dynamic BVH instead of rebuilding
- Tile-based rendering
- Analytic sprite intersection (no triangles)
- Interpolate rendering between fixed ticks

## Open decisions

- **Enemies: sprites or low-poly models.** Leaning sprites (cheap, Doom
  look). `RenderKind` on `EntityDef` keeps both possible.

## Design notes

- Two-level BVH for dynamic objects
- PS1 aesthetic throughout: vertex jitter, dithering, 320x240; affine
  texture warping planned (Phase 4)
- CPU raytracing only, no GPU shaders
