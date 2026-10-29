# PS1-Style Real-Time CPU Raytracer

A moody first-person shooter in the spirit of Doom and Quake, drawn by a
real-time software raytracer written in C. Renders at 320x240 with
PS1-era effects: vertex jitter, ordered dithering, 15-bit color, fog and
optional affine texture warping.

## Playing

Run `./rt`. You start in an underground processing plant (`levels/e1m1.level`)
with a pistol and 50 rounds. Ghouls lurk in the dark and rush you once
they see you; spitters keep their distance and lob glowing acid you can
dodge; gunfire wakes anything that can hear it. A shotgun lies just left
of the start. The storage room to the east is locked: its blue keycard is
somewhere in the west corridor. The exit gate at the back opens from
inside the storage room — and opening it has consequences. The flashlight
(F) helps more than you'd like, until its battery runs low; the bare bulbs
can be shot out if you'd rather hide in the dark. Die, or reach the red exit pad, and click
to play again.

For a straight fight, try `./rt levels/arena.map`. See `ROADMAP.md` for
where it's heading.

## Features

- **CPU raytracer** — no GPU shaders, all rendering happens on the CPU, multithreaded by scanline
- **Two-level BVH** — SAH-built static world + per-tick dynamic entity BVH; sprites tested analytically
- **Lighting** — point and spot lights with shadows, flicker and swing, bulbs you can shoot out, a battery-powered flashlight, per-level and per-area ambient light and fog
- **PS1 look** — vertex jitter, 4x4 Bayer dithering, 15-bit color, 320x240 upscaled 3x
- **Level editing** — build levels in TrenchBroom (`.map`, Quake-style brushes) or a simple text format (`.level`); the game reloads on save and warns about mistakes and budget overruns
- **Procedural textures** — concrete, brick, metal, grate, rust, tile, pipe, plus animated slime, water and flickering screens; generated in code
- **Sprites** — Doom-style billboards with color-keyed holes and frame sheets (ghouls, spitters, rockets, toxic barrels)
- **Weapons** — pistol, shotgun and rocket launcher (splash damage, including to you), each with its own viewmodel, ammo, muzzle flash that lights the room, recoil and screen shake
- **Enemies** — ghouls that rush you, spitters that hold their distance and spit dodgeable acid; they path around walls and up stairs, open doors, and come running when they hear gunfire
- **Level logic** — Doom-style doors, red/blue/yellow keycards, buttons, triggers, hidden monster ambushes and switchable lights, wired Quake-style with `target`/`targetname`
- **Gore and feedback** — blood, gibs and sparks as depth-tested particles; red damage flash; explosions that light the room
- **Gameplay** — fixed 60 Hz simulation, first-person movement with collision, pickups, death/respawn, exit pad with kill count
- **HUD** — color drains from the screen at low health; health and ammo in bitmap digits, held keycards, on-screen messages, held weapon, red death tint, kill count at the exit
- **Procedural sound** — weapons, explosions, footsteps, pickups, doors, locked-door buzz, switches, breaking glass, a voice per enemy type; stereo and distance-placed; per-area echo and ambient loops (drone, dripping water, machinery hum) that crossfade as you move

## Building

### macOS / Linux

Needs a C compiler and SDL2 (`brew install sdl2` on macOS,
`libsdl2-dev` on Debian/Ubuntu).

```sh
make        # Build the raytracer (objects go to build/)
./rt        # Run it

make test   # Build and run all test suites
make clean  # Remove build artifacts
make editor # Set up TrenchBroom (see Levels)
./rt levels/other.map    # Run a specific level (.map or .level)
```

### Windows

Windows builds use [MSYS2](https://www.msys2.org/), which provides gcc,
make, a bash shell and SDL2. The Makefile detects Windows by itself.

1. Install MSYS2 (default location `C:\msys64`).
2. From the Start menu open **MSYS2 UCRT64**. The prompt should say
   `UCRT64`; the plain "MSYS2 MSYS" shell builds against the wrong runtime.
3. Update the package database. If the window closes, reopen UCRT64 and
   run it again until nothing is left to update:
   ```sh
   pacman -Syu
   ```
4. Install the tools:
   ```sh
   pacman -S git make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2
   ```
5. Get the code and build. Your Windows drives are under `/c/`, `/d/`, …:
   ```sh
   cd /c/Users/<you>/dev
   git clone https://github.com/cephandrius22/ps1-rt.git
   cd ps1-rt
   make
   ./rt.exe
   make test
   ```

Inside the UCRT64 shell `rt.exe` finds its DLLs on its own. To start it
from Explorer or `cmd`, copy them next to it once:

```sh
cp /ucrt64/bin/SDL2.dll /ucrt64/bin/libwinpthread-1.dll .
```

Run it from the repository folder either way: levels load by paths
relative to it. Level warnings (`check:`, `budget:`) print to
the console window.

For TrenchBroom, `make editor` copies the game config into
`%APPDATA%\TrenchBroom\games\RT` and prints the Game Path to enter in
TrenchBroom's preferences. Windows gets a copy rather than a link, so run
`make editor` again after pulling changes to entity types or the config.

The Windows build is cross-compiled during development but has had
little play-testing on Windows itself; please report anything odd.

## Architecture

```
src/
  main.c      Entry point, game loop
  core/       math3d (inline math, Pose), threadpool, kv parser
  gfx/        camera, mesh + OBJ loader, BVH, TGA textures, procedural
              textures, PS1 effects
  world/      scene (two-level BVH + sprites), entities, lights, level
              environment and zones, nav grid, .level and .map loaders,
              materials, level checker
  game/       Game state + fixed-timestep loop, PlayerCmd, events,
              player controller, shared movement, weapons, inventory,
              enemy AI, level logic (doors, keys, triggers)
  render/     per-pixel shading, multithreaded frame render, HUD overlay,
              text font, particles, screen effects
  platform/   SDL input → PlayerCmd, stereo audio mixer with reverb,
              file watching
  tools/      TrenchBroom export (textures, FGD)

tests/        one standalone binary per module (test_framework.h)
levels/       .map and .level files
editor/       TrenchBroom game config (textures/FGD generated by make editor)
```

### Rendering Pipeline

1. For each pixel, generate a world-space ray from the camera
2. Intersect the static and dynamic BVHs and the sprite list for the closest hit
3. Look up surface color (procedural texture, scrolled if animated; material
   color; or checkerboard). With F4, UVs come from the screen instead (affine)
4. Light it: the zone's ambient + glow of the material + sun + point and spot
   lights + the flashlight, all one light model (shadow rays, max 4 per pixel)
5. Blend with the fog of the zone the camera is in; draw light billboards
6. Apply PS1 dithering and 15-bit color quantization
7. Write to the 320x240 framebuffer, draw particles and the overlay
   (viewmodel, low-health desaturation, HUD); SDL2 upscales to the window

### PS1 Effects

- **Vertex jitter**: triangle vertices snap to a 1/128 grid at level load, before BVH construction, mimicking the PS1's lack of sub-pixel precision.
- **Dithering**: a 4x4 Bayer matrix adds ordered noise before quantizing each channel to 5 bits, matching the PS1's 15-bit VRAM.
- **Affine texture mapping** (F4): textures interpolated straight across the screen without perspective correction, so they swim and bend like on the PS1. Off by default; raytraced UVs are otherwise perspective-correct.

## Controls

| Key       | Action           |
|-----------|------------------|
| W/A/S/D   | Move             |
| Mouse     | Look around      |
| Space     | Jump             |
| Left click| Shoot            |
| 1 / 2 / 3 | Pistol / shotgun / launcher (once found) |
| Mouse wheel | Next / previous weapon |
| E         | Use (doors, buttons) |
| F         | Flashlight (runs on a battery that recharges while off) |
| F3        | Debug overlay: frame time, budgets, position |
| F4        | PS1 texture warp (affine mapping) on/off |
| Click / E | Restart level when dead or at the exit |
| Escape    | Quit             |

## Levels

Levels are designed in [TrenchBroom](https://trenchbroom.github.io/), the
Quake level editor:

1. `make editor` exports the game's textures and entity definitions and
   links the "RT" game config into TrenchBroom (copies it on Windows; on
   Linux add `TB_GAMES=~/.TrenchBroom/games`).
2. In TrenchBroom: Preferences → RT → Game Path = `<this repo>/editor/game`.
3. New Map → RT. Build with brushes (32 units = 1 m); place an
   `info_player_start`, lights, enemies (`ghoul`, `spitter`), pickups
   (`health`, `ammo`, `weapon`, `keycard`) and an `exit`. Doors, buttons
   and triggers are brush entities (`func_door`, `func_button`,
   `trigger_once`, `trigger_multiple`); wire them with `target` →
   `targetname` (TrenchBroom draws the links). `levels/arena.map` is an
   example to open.
   The level's mood (ambient light, fog distance and color, sun, sky)
   is set on worldspawn: `ambient`, `fog`, `sun`, `sky`, `reverb`,
   `ambience`. An `env_zone` brush gives an area its own light level,
   fog, echo and ambient loop (a dark, dripping crypt; a humming machine
   room); they fade in as you enter.
4. Save as `levels/<name>.map` and run `./rt levels/<name>.map`. Keep it
   running: every save reloads the level where you stand.

The terminal prints `check:` warnings (missing exit, an exit you can't
walk to, enemies stuck in walls, typo'd keys, targets naming nothing,
keyed doors without their keycard) and a `budget:` line; stay within 30k triangles,
32 lights and 40 sprites for 60 fps. F3 shows the same live.

Levels can also be hand-written as `.level` text (`levels/e1m1.level`);
the format and every entity's keys are in `CLAUDE.md`. OBJ models go in
`assets/models/`.

## License

Unlicensed — do whatever you want with it.
