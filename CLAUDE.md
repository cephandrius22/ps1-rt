# PS1-Style CPU Raytracer

Real-time software raytracer in C + SDL2. Renders at 320x240, upscaled 3x. PS1 aesthetic: vertex jitter, affine textures, Bayer dithering, 15-bit color.

## Build & Run

```bash
make rt          # build the binary
make test        # build and run all 14 test suites (126 tests)
make clean       # remove all build artifacts
./rt             # run the game (WASD + mouse look, click to shoot, E to interact)
```

Requires: SDL2 (`sdl2-config`), C compiler, pthreads, `-lm`.

## Architecture

### Core Pipeline

`main.c` — Game loop: input → physics → light update → render → audio triggers → SDL blit

Rendering path: `camera_ray()` → `scene_intersect()` (two-level BVH) → `shade_pixel_lit()` → PS1 dither → framebuffer

### Module Map

| Module | Purpose |
|--------|---------|
| `math3d.h` | Vec3, Mat4, Ray, UV — header-only with inline functions |
| `camera` | Camera state, perspective ray generation per pixel |
| `mesh` | Triangle storage, OBJ loader with fan triangulation |
| `bvh` | SAH-based BVH build, stack-based iterative traversal |
| `scene` | Two-level scene graph: static world BVH + dynamic entity BVH rebuilt per frame |
| `entity` | Typed entities (pickup/door/prop) with transforms, growable array |
| `render` | Shading: directional sun + point lights + shadows + fog + light billboards |
| `light` | Point light system: attenuation, flicker (layered sine), pendulum swing animation |
| `texture` | TGA loader, nearest-neighbor sampling |
| `ps1_effects` | Vertex jitter (1/128 grid), affine UV, 4x4 Bayer dither, 15-bit color quantize |
| `player` | First-person movement with gravity, wall sliding, ground snap via BVH raycasts |
| `weapon` | Hitscan weapon with cooldown and pitch recoil |
| `input` | SDL2 keyboard/mouse state polling, relative mouse mode |
| `audio` | 8-voice mixer via SDL2 audio callback, procedural sound gen, WAV loader ready |
| `threadpool` | pthread fork-join pool, generation-based sync, splits scanlines across workers |

### Key Patterns

- **Two-level BVH:** `scene.world_bvh` is static (built once). `scene.entity_bvh` is rebuilt each frame from entity triangles. `scene_intersect()` tests both.
- **Multithreaded render:** `render_scene_lit_mt()` splits screen rows across thread pool workers. Each worker gets a pre-assigned chunk — no work stealing.
- **Audio triggers in main loop:** Player struct carries per-frame event flags (`collected`, `door_action`, `walk_distance`). `main.c` reads these and calls `audio_play()` — audio module has no scene dependency.
- **Materials:** `SceneMaterials` holds parallel arrays of `Material` and `Texture`. Triangles reference materials by index. Fallback is checkerboard.
- **Shadow rays:** Capped at `MAX_SHADOW_LIGHTS` (4) per pixel for performance. Sun shadow is separate.

### Adding Content

**New level geometry:** Load OBJ via `mesh_load_obj()`, add triangles to `scene.world_tris`, call `scene_build()` to construct BVH.

**New entity type:** Add to `EntityType` enum in `entity.h`, handle in `entity_update()` and interaction functions.

**New light:** Call `light_add()` with a `PointLight` struct. Set `swing_speed > 0` for pendulum motion, `flicker_speed > 0` for flicker.

**New sound:** Add to `SoundID` enum, generate in `audio_generate_sounds()` or load WAV via `audio_load_wav()`. Trigger with `audio_play()` from main loop.

**New PS1 effect:** Add to `ps1_effects.c`. Current effects are applied per-vertex (jitter) or per-pixel (dither/quantize).

## Tests

Each `tests/test_*.c` is a standalone binary using `tests/test_framework.h`. Test targets in Makefile link only needed `.o` files. Run individual suites with `./tests/test_<module>`.
