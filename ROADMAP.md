# Roadmap: Moody PS1-Style FPS

Goal: a dark, atmospheric shooter in the spirit of Doom/Quake, rendered by a
CPU raytracer with a PS1 look. Phases are ordered by what makes the game
playable soonest. Each phase ends in something you can play.

## Done

- Collision & physics: gravity, jump, step climbing, wall sliding
- Fixed 60 Hz simulation driven by `PlayerCmd`, events to presentation
- Hitscan weapons from a `WeaponDef` table (pellets, spread, ammo,
  recoil); no viewmodel drawn yet
- Two-level BVH plus sprites; data-driven entities: pickups, doors,
  props, decor sprites, ghoul enemy, exit pad
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

- Phase 2: viewmodel sprite (overlay has `weapon_kick` ready).
- Phase 3: enemies walk straight at the player on a fixed height (no
  gravity, steps or pathing); only sight wakes them.
- Phase 5: exit replays the level instead of loading the next; entity
  `mat=` is a material index (fragile), should accept a name.
- Phase 7: sprites are a linear scan per ray (~1.6 ms/frame for the six
  in e1m1).
- Playtest: sounds and fight balance are untested by ear/hand.

## Phase 2: Combat feel

- Viewmodel: weapon sprite drawn by the overlay, with bob and recoil
- Shotgun (a `WeaponDef` row: pellets + spread + shells ammo type), one
  projectile weapon
- Muzzle flash as short-lived light
- Screen shake, damage flash
- Blood/gibs as billboard particles
- Second enemy type: ranged (the ghoul is melee)

## Phase 3: Level logic

- Doors back in e1m1 (entity type exists), keys, switches
- `trigger` directive: on enter → open door, spawn monsters, play sound
- Sound alerting: gunfire wakes nearby enemies
- Waypoint graph via `node` directive for enemy pathing
- Enemy gravity and step climbing (they keep their spawn height today)

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

- Episode of 3–4 levels; exit loads the next level (it replays today)
- `mat=<name>` on entities instead of a material index
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
