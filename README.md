# PS1-Style Real-Time CPU Raytracer

A real-time software raytracer written in C that produces PlayStation 1-era graphics. Renders at 320x240 with PS1-authentic effects including vertex jitter, ordered dithering, 15-bit color quantization, and distance fog.

## Screenshots

Run `./rt` to see it in action — WASD to move, mouse to look around, Escape to quit.

## Features

- **CPU raytracer** — no GPU shaders, all rendering happens on the CPU, multithreaded by scanline
- **Two-level BVH** — SAH-built static world + per-tick dynamic entity BVH
- **Lighting** — point lights with shadows, flicker and swing, a player flashlight, distance fog
- **PS1 look** — vertex jitter, 4x4 Bayer dithering, 15-bit color, 320x240 upscaled 3x
- **Level files** — rooms, boxes, quads, OBJ models, entities and lights in a text format (`levels/*.level`)
- **Procedural textures** — concrete, brick, metal, grate, rust, tile, pipe; generated in code
- **Sprites** — Doom-style billboards with color-keyed holes and frame sheets (ghoul enemy, toxic barrels)
- **Gameplay** — fixed 60 Hz simulation, first-person movement with collision, hitscan weapon with ammo and recoil, health and ammo pickups, doors, an enemy that hunts you, death/respawn, exit pad with kill count
- **HUD** — health and ammo in bitmap digits, red death tint
- **Procedural sound** — weapon, footsteps, pickups, doors, growls, pain and death cries, ambient drone

## Building

### Requirements

- C compiler (cc/gcc/clang)
- SDL2 (`brew install sdl2` on macOS)

### Build & Run

```sh
make        # Build the raytracer (objects go to build/)
./rt        # Run it

make test   # Build and run all test suites
make clean  # Remove build artifacts
```

## Architecture

```
src/
  main.c      Entry point, game loop
  core/       math3d (inline math, Pose), threadpool, kv parser
  gfx/        camera, mesh + OBJ loader, BVH, TGA textures, procedural
              textures, PS1 effects
  world/      scene (two-level BVH + sprites), entities, lights, .level loader
  game/       Game state + fixed-timestep loop, PlayerCmd, events,
              player controller, weapons, inventory, enemy AI
  render/     per-pixel shading, multithreaded frame render, HUD overlay
  platform/   SDL input → PlayerCmd, audio mixer

tests/        one standalone binary per module (test_framework.h)
levels/       .level scene files
```

### Rendering Pipeline

1. For each pixel, generate a world-space ray from the camera
2. Intersect the static and dynamic BVHs and the sprite list for the closest hit
3. Look up surface color (procedural texture, material color, or checkerboard)
4. Light it: ambient + sun + point lights (shadow rays, max 4 per pixel) + flashlight
5. Blend with distance fog; draw light billboards
6. Apply PS1 dithering and 15-bit color quantization
7. Write to the 320x240 framebuffer, draw the HUD overlay; SDL2 upscales to the window

### PS1 Effects

- **Vertex jitter**: triangle vertices snap to a 1/128 grid at level load, before BVH construction, mimicking the PS1's lack of sub-pixel precision.
- **Dithering**: a 4x4 Bayer matrix adds ordered noise before quantizing each channel to 5 bits, matching the PS1's 15-bit VRAM.
- **Affine texture mapping**: `ps1_affine_uv()` exists, but shading does not use it yet; raytraced UVs are perspective-correct.

## Controls

| Key       | Action           |
|-----------|------------------|
| W/A/S/D   | Move             |
| Mouse     | Look around      |
| Space     | Jump             |
| Left click| Shoot            |
| E         | Use (doors)      |
| F         | Flashlight       |
| Click / E | Restart level when dead or at the exit |
| Escape    | Quit             |

## Levels and Models

Levels are text files in `levels/`; run one with `./rt levels/yourmap.level`.
The format is documented in `CLAUDE.md`. OBJ models are placed with the
`obj` directive; put `.obj` files in `assets/models/`.

## License

Unlicensed — do whatever you want with it.
