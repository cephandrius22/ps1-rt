# Roadmap: Moody PS1-Style FPS

Goal: a dark, atmospheric shooter in the spirit of Doom/Quake, rendered by a
CPU raytracer with a PS1 look. Phases are ordered by what makes the game
playable soonest. Each phase ends in something you can play.

## Done

- Collision & physics: gravity, jump, step climbing, wall sliding
- Hitscan weapon with cooldown, recoil, viewmodel
- Two-level BVH, entity system, pickups
- Point lights, shadows, flicker, swinging bulbs, flashlight, fog
- 8-voice mixer, procedural sounds, footsteps, ambient loop
- Level file format, procedural textures
- Multithreaded rendering

## Phase 0: Structure

Do this before Phase 1. Each item removes a lock-in that Phase 1+ would
otherwise build on top of.

Target layering (each layer calls only layers below it):

```
  main.c          wires platform + game + render, owns the loop
     |
  game/ render/   gameplay (player, weapon, ai, events) | shading, overlay
     |
  world/          scene, entity, light, level loader
     |
  gfx/            camera, mesh, bvh, texture, proctex, ps1 effects
     |
  core/           math3d, threadpool

  platform/       SDL window, input device, audio device
                  (talks to game only through commands/events)
```

- ✅ **Directories by layer.** `src/*` moved into the folders above.
- ✅ **Commands, not keys.** Platform input produces a `PlayerCmd` (move,
  look delta, buttons). Player and weapon consume commands, never
  `InputState`/SDL. AI emits the same `PlayerCmd` → enemies reuse
  movement code; rebinding and demo recording become trivial.
- ✅ **Event queue.** Replace per-frame flags on `Player` (`collected`,
  `door_action`, `walk_distance`) with a `GameEvent` queue
  (type, position, entity). Audio, HUD and particles consume it.
  Positions in events enable spatial audio later.
- ✅ **Hits know what they hit.** `HitRecord.owner` tag +
  `entity_hit()` give the entity index. Required for damage.
- **Data-driven entity types.** Replace switch statements with a
  per-type table: `{ update, on_touch, on_use, on_damage, render_kind }`.
  `render_kind` = mesh | sprite | none, so the sprite-vs-model choice
  stays open per entity. Generic fields: health, velocity, radius.
- **Data-driven weapons.** `WeaponDef` table (damage, cooldown, spread,
  pellets, ammo type) instead of `#define`s.
- **Environment in the level.** Move sun, ambient, fog out of
  `make_render_params()` into a `Environment` struct on `Scene`, set by
  new level directives (`fog`, `ambient`, `sun`).
- **One light model.** Fold the flashlight into the light list as a
  spot light (`Light.kind` = point | spot). Muzzle flash and enemy glow
  reuse it.
- **Gameplay doesn't draw.** Move `weapon_draw_viewmodel()` and HUD into
  a `render/overlay` module fed by game state.
- **Fixed-timestep logic.** Update game at a fixed tick (e.g. 60 Hz),
  render as fast as possible. Deterministic physics, replayable demos.
- **Game state + modes.** `Game` struct owning scene, player, weapons;
  mode machine: menu → playing → intermission. Needed for level
  transitions, pause, save/load.
- ✅ **Tests link a static lib** of all non-main objects; no per-test
  Makefile rules.
- **Cleanup.** Drop legacy `render_scene()`/`SceneMaterials` path and
  the BVH-only `player_update()`/`weapon_update()` duplicates.
- **Player owns a Camera.** `player.h` reaches into `gfx/` for its pose.
  Give `Player` a position/yaw/pitch; derive the camera at render time.

## Phase 1: Vertical slice

One room that is fun to fight in.

- Player health, damage, death, respawn
- Ammo consumption
- Minimal HUD: health, ammo
- One enemy: idle → chase → attack → die; line of sight via
  `scene_occluded()`
- Enemy sounds: alert, attack, pain, death
- Exit trigger ends the level

## Phase 2: Combat feel

- Shotgun (multi-ray spread), one projectile weapon
- Muzzle flash as short-lived light
- Weapon bob, screen shake, damage flash
- Blood/gibs as billboard particles
- Second enemy type: melee + ranged

## Phase 3: Level logic

- Doors back in, keys, switches
- `trigger` directive: on enter → open door, spawn monsters, play sound
- Sound alerting: gunfire wakes nearby enemies
- Waypoint graph via `node` directive for enemy pathing

## Phase 4: Atmosphere

- Per-area light level and fog color (Doom sectors)
- Ambient soundscapes per zone, simple reverb, stereo panning
- Shootable lights, flashlight battery
- Animated textures: flickering screens, water, slime
- Low health desaturation

## Phase 5: Content

- Episode of 3–4 levels with transitions
- Secrets, intermission screen (kills, secrets, time)
- Hot-reload level files

## Phase 6: Shell

- Main menu, pause, mouse sensitivity
- Save / load

## Phase 7: Performance (when profiling demands)

- Refit dynamic BVH instead of rebuilding
- Tile-based rendering
- Analytic sprite intersection (no triangles)

## Open decisions

- **Enemies: sprites or low-poly models.** Leaning sprites (cheap, Doom
  look). Phase 0's `render_kind` keeps both possible.

## Design notes

- Two-level BVH for dynamic objects
- PS1 aesthetic throughout: vertex jitter, affine textures, dithering,
  320x240
- CPU raytracing only, no GPU shaders
