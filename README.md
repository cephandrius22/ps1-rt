# PS1-Style Real-Time CPU Raytracer

A real-time software raytracer written in C that produces PlayStation 1-era graphics. Renders at 320x240 with PS1-authentic effects including vertex jitter, ordered dithering, 15-bit color quantization, and distance fog.

## Screenshots

Run `./rt` to see it in action — WASD to move, mouse to look around, Escape to quit.

## Features

- **CPU raytracer** — no GPU shaders, all rendering happens on the CPU
- **BVH acceleration** — Surface Area Heuristic (SAH) for fast ray-triangle intersection
- **OBJ model loading** — supports v, vt, vn, and quad/tri faces
- **TGA texture loading** — uncompressed 24/32-bit TGA files, nearest-neighbor sampling
- **PS1 vertex jitter** — vertices snapped to a coarse fixed-point grid
- **PS1 dithering** — 4x4 Bayer matrix ordered dithering with 5-bit-per-channel color
- **Distance fog** — objects fade into atmosphere at distance
- **First-person controls** — WASD movement + mouse look
- **320x240 internal resolution** — upscaled 3x to 960x720 window

## Building

### Requirements

- C compiler (cc/gcc/clang)
- SDL2 (`brew install sdl2` on macOS)

### Build & Run

```sh
make        # Build the raytracer (objects go to build/)
./rt        # Run it

make test   # Run all unit tests
make clean  # Remove build artifacts
```

## Architecture

```
src/
  main.c      Entry point, game loop
  core/       math3d (inline math), threadpool
  gfx/        camera, mesh + OBJ loader, BVH, TGA textures, procedural
              textures, PS1 effects
  world/      scene (two-level BVH), entities, lights, .level loader
  game/       PlayerCmd, player controller, weapon
  render/     per-pixel shading, multithreaded frame render
  platform/   SDL input → PlayerCmd, audio mixer

tests/        one standalone binary per module (test_framework.h)
levels/       .level scene files
```

### Rendering Pipeline

1. For each pixel, generate a world-space ray from the camera
2. Traverse the BVH to find the closest triangle intersection
3. Look up surface color (texture, material, or procedural checkerboard)
4. Apply directional lighting (N·L) with ambient term
5. Blend with distance fog
6. Apply PS1 dithering and 15-bit color quantization
7. Write to 320x240 framebuffer, SDL2 upscales to window

### PS1 Effects

The PS1 look comes from three key effects:

- **Vertex jitter**: Triangle vertices are snapped to a grid of resolution 1/128, simulating the PS1's lack of sub-pixel precision. Applied once at scene load before BVH construction.
- **Affine texture mapping**: The `ps1_affine_uv()` function provides linear (non-perspective-correct) UV interpolation, producing the characteristic texture warping.
- **Dithering**: A 4x4 Bayer matrix adds ordered noise before quantizing each color channel from 8-bit to 5-bit (32 levels), matching the PS1's 15-bit VRAM.

## Controls

| Key       | Action        |
|-----------|---------------|
| W/A/S/D   | Move          |
| Mouse     | Look around   |
| Escape    | Quit          |

## Loading Custom Models

The OBJ loader is ready to use:

```c
Mesh m;
mesh_init(&m);
mesh_load_obj(&m, "assets/models/room.obj");
```

Place `.obj` files in `assets/models/` and `.tga` textures in `assets/textures/`.

## License

Unlicensed — do whatever you want with it.
