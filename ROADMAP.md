# Roadmap: PS1-Style FPS Game

## Tier 1: Collision & Physics ✅
- Gravity, jumping, ground snapping with step climbing
- Per-axis wall sliding via BVH raycasts (3 heights: knee, chest, head)
- Player capsule approximation with PLAYER_RADIUS

## Tier 2: Weapons & Interaction ✅
- Hitscan weapon with cooldown and recoil
- Crosshair overlay
- Weapon viewmodel with muzzle flash
- Shoot input (left mouse button)

## Tier 3: Dynamic Objects & Entities ✅
- Two-level BVH: static world (bottom) + dynamic objects (top, rebuilt per frame)
- Entity system: position, rotation, health, type (growable array)
- Pickup items (ammo, health) — bob, rotate, walk-over collect
- Doors — swing open/closed on E key press

## Tier 4: AI Enemies
- Enemy spawning and placement
- Pathfinding (navmesh or waypoint-based)
- Basic state machine: idle → alert → chase → attack
- Enemy takes damage from hitscan, dies, ragdoll or death animation
- Enemy attacks player (melee or projectile)

## Tier 5: Lighting & Atmosphere ✅
- Point lights with distance attenuation and colored light
- Shadow rays (hard shadows via BVH occlusion test, sun + point lights)
- Flickering lights via layered sine waves
- Real-time only (no baked lighting)
- Distance fog retained from original renderer

## Tier 6: Audio
- SDL2 audio / SDL_mixer integration
- Weapon fire sound, impact sounds
- Footstep sounds
- Enemy sounds (alert, attack, death)
- Ambient background audio

## Tier 7: Level Design & Content
- Level file format (custom or extended OBJ with metadata)
- Multiple rooms / areas connected by doors
- Texture variety (walls, floors, ceilings)
- Level transitions or progression
- HUD: health bar, ammo counter

## Tier 8: Polish & Performance
- Multithreaded rendering (split scanlines across pthreads)
- Tile-based rendering for cache locality
- Spatial audio
- Menu / pause screen
- Save/load game state

## Design Notes
- Use two-level BVH for dynamic objects (user preference confirmed)
- Keep PS1 aesthetic throughout: vertex jitter, affine textures, dithering, 320x240
- CPU raytracing only — no GPU shaders
