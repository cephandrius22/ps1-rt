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
make        # Build the raytracer
./rt        # Run it

make test   # Run all unit tests
make clean  # Remove build artifacts
```

## Architecture

```
src/
  main.c           Entry point, game loop, test scene construction
  math3d.h         Vec3, Vec2, Mat4, Ray — inline math library
  camera.c/h       Camera state and per-pixel ray generation
  mesh.c/h         Triangle type, Moller-Trumbore intersection, OBJ loader
  bvh.c/h          Bounding Volume Hierarchy — build (SAH) and traversal
  render.c/h       Scene rendering: lighting, fog, material/texture lookup
  texture.c/h      TGA file loading and UV sampling
  ps1_effects.c/h  Vertex jitter, affine UV mapping, Bayer dithering
  input.c/h        SDL2 keyboard and mouse input
  player.c/h       First-person player movement and mouse look

tests/
  test_framework.h   Minimal C test framework (assert macros)
  test_math3d.c      15 tests — vector/matrix operations
  test_camera.c       5 tests — ray generation, rotation
  test_mesh.c        10 tests — ray-triangle intersection, mesh operations
  test_bvh.c         11 tests — AABB intersection, BVH build/query
  test_texture.c      6 tests — UV sampling, wrapping, edge cases
  test_ps1_effects.c  9 tests — vertex snap, affine UV, dithering
  test_player.c      12 tests — movement, mouse look, normalization
  test_render.c       9 tests — framebuffer, color packing, sky gradient
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
