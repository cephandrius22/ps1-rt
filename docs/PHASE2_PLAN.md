# Phase 2 — Combat Feel: Implementation Plan

> **Status: implemented.** The code follows this plan; differences:
> the launcher arrived with projectiles (Step 6) rather than as a Step 1
> stub; `damage_entity()` lives in `game/damage.c`; projectiles also carry
> a glow light in flight; a pre-existing bug (falling through the floor on
> an untaken pickup) was fixed along the way with ghost meshes. CLAUDE.md
> describes the current code.

## Context

Phases 0–1.5 gave the game a fight loop (pistol, ghoul, health/ammo, exit) and the
tooling to build levels in TrenchBroom. Fights still feel thin: one hitscan gun with
no visible weapon, one melee enemy, and hits that only make a sound. Phase 2 makes
combat *feel* right before real levels are designed:

- weapons you hold and switch between (pistol, **shotgun**, **rocket launcher**)
- a visible viewmodel with bob, recoil and raise/lower
- muzzle flashes that light the room, screen shake, a red damage flash
- blood, gibs and sparks as particles
- a second enemy, the **spitter**, that keeps its distance and spits a slow,
  dodgeable glowing projectile

You'll implement this yourself. Each step below says **why** it exists, **where** code
goes (and why there, per the layer rules), the **types and signatures** to add, short
**pseudo-code** for the tricky parts, the **tests** to write, and when it's **done**.
Do the steps in order: later steps build on earlier plumbing. Commit each step on its
own, green (your usual per-step style).

### Ground rules (from CLAUDE.md, apply to every step)

- Layers: `game/` and `render/` never include each other; `main.c` translates.
  `render/` may use `gfx/`. `world/` must not know `game/` concepts (weapons,
  ammo types, AI).
- Simulation runs in fixed ticks (`GAME_TICK_DT`); gameplay never sees frame time.
  Visual-only effects (shake, particles, bob) may use frame `dt` in `main.c`/`render/`.
- Data over code: new weapons, enemies, projectiles are table rows
  (`weapon_types.c`, `entity_types.c`, `EnemyDef` in `ai.c`, a new `ProjectileDef`).
- Gameplay reports what happened through `GameEvent`s; `main.c` maps events to sounds,
  effects and particles (like `play_event_sounds()` in `src/main.c`).
- Style: braces always, enums not bool parameters, named constants, a short what/why
  comment per block, early returns. Every new module gets a header comment with a
  small diagram, like the existing ones.
- After adding entity types or textures: `make editor` regenerates the FGD/textures.

### Event flow after Phase 2 (the picture to keep in mind)

```
 game_tick ──> weapon fires ───> EV_WEAPON_FIRED ──┐
          ├──> projectile hits ─> EV_EXPLOSION ────┤
          ├──> enemy hurt/dies ─> EV_ENEMY_PAIN/DIED┤   main.c, per frame:
          └──> player hurt ─────> EV_PLAYER_HURT ──┤──> play_event_sounds()
                                                   ├──> play_event_effects()  shake, flash
                                                   └──> spawn_event_particles() blood, sparks
 game_tick also adds temporary lights (muzzle flash, explosions) to scene->lights.
```

---

## Step 1 — Weapon inventory, switching, shotgun, shells

**Why first:** everything else (viewmodel, rockets) assumes there is more than one
weapon and a way to pick one. This step is mostly plumbing through existing layers.

### 1a. Ammo types and owned weapons — `src/game/inventory.h/.c`

- Add `AMMO_SHELLS`, `AMMO_ROCKETS` to `AmmoType` (before `AMMO_TYPE_COUNT`), with
  `#define`d start/max values and rows in the `ammo_max[]` table.
- Add owned weapons as a **bitmask**: `uint32_t weapons; /* bit n = WeaponType n */`.
  *Why a bitmask, not `bool owned[WEAPON_TYPE_COUNT]`:* `weapon.h` already includes
  `inventory.h`; using `WeaponType` here would create a circular include. Bits keep
  inventory independent of weapons.
- New functions:
  ```c
  AmmoType inventory_ammo_by_name(const char *name);   /* "shells" → AMMO_SHELLS, else AMMO_NONE */
  void inventory_give_weapon(Inventory *inv, int weapon);  /* sets the bit */
  bool inventory_has_weapon(const Inventory *inv, int weapon);
  ```
  `inventory_create()` gives the pistol (bit 0) and starting bullets.

### 1b. Weapon table — `src/game/weapon.h`, `weapon_types.c`

- `WeaponType`: add `WEAPON_SHOTGUN`, `WEAPON_LAUNCHER` (slots 1/2/3 = enum order).
- `WeaponDef` gains:
  ```c
  const char *view_texture;  /* viewmodel sheet name (Step 2), e.g. "view_shotgun" */
  float switch_time;         /* seconds to lower + raise when switching */
  /* Step 3 adds flash fields, Step 6 adds projectile fields */
  ```
- Shotgun row: `pellets = 7`, `spread ≈ 0.08 rad`, `damage = 10` per pellet,
  `cooldown ≈ 0.9`, `kick ≈ 0.09`, `ammo = AMMO_SHELLS`, `ammo_per_shot = 1`.
  The firing code already handles pellets — no new code path (that was the point of
  `WeaponDef`).
- Launcher row: stub for now (`ammo = AMMO_ROCKETS`); Step 6 makes it fire rockets.
- `WeaponType weapon_type_by_name(const char *name);` (loops the table, like
  `entity_type_by_name()` in `src/world/entity_types.c`).

### 1c. Switching state — `src/game/weapon.h/.c`

The single `Weapon` in `Game` stays; switching changes its `type` with a delay.
```c
typedef enum { WEAPON_READY, WEAPON_LOWERING, WEAPON_RAISING } WeaponPhase;
/* in Weapon: */
WeaponPhase phase;
float phase_time;        /* seconds into the current phase */
WeaponType pending;      /* what we switch to once lowered */

void weapon_request(Weapon *w, WeaponType type);  /* ignored if already that type */
```
In `weapon_update()`: advance `phase_time`; LOWERING for `switch_time/2` → set
`type = pending`, go RAISING; RAISING for `switch_time/2` → READY. Only READY can fire
(`tick_ready()` returns false otherwise). *Why model phases:* the viewmodel (Step 2)
animates from them, and Doom/Quake both block firing mid-switch.

### 1d. The command — `src/game/cmd.h`

A weapon choice is a number, not a held button, so add fields to `PlayerCmd`:
```c
int8_t weapon_slot;   /* impulse: 1..N selects, 0 = none */
int8_t weapon_cycle;  /* impulse: +1 next, -1 previous, summed */
```
Update `cmd_accumulate()` (latest non-zero `weapon_slot` wins; `weapon_cycle` adds) and
`cmd_consume()` (zero both). *Why:* the fixed timestep may run 0 or several ticks per
frame; impulses must apply exactly once (see `test_game_update_impulse_once` in
`tests/test_game.c` — write the same test for these fields).

### 1e. Input — `src/platform/input.c`

In the event loop: `SDLK_1..SDLK_3` → `cmd->weapon_slot = key - SDLK_0`;
`SDL_MOUSEWHEEL` → `cmd->weapon_cycle += (e.wheel.y > 0) ? -1 : 1` (wheel up =
previous, as in Quake). Bindings stay in platform; game never sees SDL.

### 1f. Game applies the choice — `src/game/game.c`

In `tick_playing()` before `weapon_update()`: `select_weapon(g, cmd)`:
slot → `WeaponType`, skip if not owned; cycle → next/previous **owned** type, wrapping.
Then `weapon_request()`. Also on dry fire, auto-switch to the best owned weapon with
ammo (Doom behaviour) — optional, keep it small.

### 1g. Weapon and ammo pickups — world + loader + player

- World doesn't know weapons, so pickups carry a **name**:
  - `src/world/entity.h`: add `KEY_STRING` to `KeyType`; add `char item[16]` to the
    `pickup` union member; add `TOUCH_EFFECT_WEAPON`; add entity type
    `ENTITY_PICKUP_WEAPON` (level name `"weapon"`).
  - `src/world/entity_types.c`: `ammo` gets key `ammo` (`KEY_STRING`, default
    `"bullets"`); `weapon` gets key `give` (`KEY_STRING`, e.g. `"shotgun"`) and
    `amount` (ammo that comes with it). Spawn hooks copy the string with
    `kv_string()` (`src/core/kv.h`).
  - `EntityKey.def` is a float; add `const char *def_string` for string keys.
- `src/tools/editor_export.c` `write_key()`: handle `KEY_STRING` →
  `name(string) : "desc" : "default"`.
- `src/game/player.c` `apply_touch_effect()`:
  `TOUCH_EFFECT_AMMO` → `inventory_add(inv, inventory_ammo_by_name(e->pickup.item), …)`;
  `TOUCH_EFFECT_WEAPON` → give weapon + its ammo; refuse (return false) only if the
  weapon is owned *and* its ammo is full. Push a new `EV_WEAPON_PICKUP` so `main.c`
  can auto-switch feedback / play a sound.
- `game.c`: on `EV_WEAPON_PICKUP` for a weapon the player didn't have, request it
  (Doom switches to new weapons).

### 1h. HUD ammo

`game_ammo()` already reads the current weapon's pool — shells/rockets just work.

**Tests:** `tests/test_inventory.c` (ammo by name, weapon bits), `tests/test_weapon.c`
(switch phases: can't fire while lowering, type changes at midpoint; shotgun emits up
to 7 impacts), `tests/test_game.c` (slot/cycle impulses apply once; cycle skips
unowned), `tests/test_player.c` (weapon pickup gives weapon + shells),
`tests/test_editor_export.c` (string keys appear in FGD).
**Done when:** in a test map you pick up a shotgun, 1/2/wheel switch with a delay,
and the HUD shows shells.

---

## Step 2 — Viewmodel (weapon on screen)

**Why:** without a visible gun, recoil and switching are invisible.

### 2a. Textures — `src/gfx/proctex.h/.c`

Add color-keyed sheets `view_pistol`, `view_shotgun`, `view_launcher`, each
**2 frames** (idle, firing) of 64×48, drawn with the same helpers as the ghoul
(`sheet_rect`, `sheet_ellipse` in `src/gfx/proctex.c`). Firing frame adds a muzzle
burst. Preview them with an offscreen dump (the same way the ghoul sheet was checked).

### 2b. Overlay owns its textures — `src/render/overlay.h/.c`

The overlay is currently stateless. Give it state:
```c
typedef struct {
    Texture views[PTEX_COUNT];  /* generated on first use, by proctex id */
    bool generated[PTEX_COUNT];
} Overlay;
void overlay_init(Overlay *o);
void overlay_free(Overlay *o);
void overlay_draw(Overlay *o, Framebuffer *fb, const OverlayState *state);  /* signature change */
```
*Why not pass a `Texture*` in `OverlayState`:* main would have to own render data;
looking textures up by name (`proctex_find()`) keeps `WeaponDef.view_texture` the
single source and render self-contained.

`OverlayState` gains:
```c
typedef struct {
    const char *texture;  /* WeaponDef.view_texture; NULL = no weapon (dead) */
    int frame;            /* 0 idle, 1 firing */
    float offset_x;       /* pixels: bob sway */
    float offset_y;       /* pixels: bob + recoil + raise/lower (positive = down) */
    float light;          /* 0..1+ brightness multiplier */
} ViewmodelState;
ViewmodelState viewmodel;
```
Drawing (before the HUD digits so numbers stay on top): blit the frame scaled 2× at
bottom-center, skipping color-key texels (`texture_opaque()` already exists), each
texel multiplied by `light`. Use a `draw_sprite_2d()` helper; keep it PS1-crude
(nearest neighbour, no filtering).

### 2c. Filling it — `src/main.c` (`fill_viewmodel()`)

All presentation math lives here, from game state:
- `frame`: 1 while `game.weapon.fired_this_tick` was true recently — keep a
  frame-time timer in main (`VIEW_FIRE_SHOW ≈ 0.06 s`) because a tick flag can be missed
  between frames.
- bob: needs walking speed. Add `float move_speed;` to `Player`
  (`src/game/player.c`: horizontal distance moved / dt each tick; useful later for AI
  noise too). Then `phase += dt * BOB_FREQ * (move_speed / PLAYER_SPEED)`;
  `offset_x = sin(phase) * BOB_X`, `offset_y = |cos(phase)| * BOB_Y`.
- recoil: `offset_y += weapon.kick * RECOIL_PIXELS_PER_RAD`.
- switching: LOWERING/RAISING → `offset_y += lowered_fraction * VIEW_LOWER_PIXELS`.
  Expose `float weapon_lowered(const Weapon *w)` (0 ready … 1 fully down) in
  `weapon.h` so main doesn't re-derive the phase math.
- `light`: a crude lit factor so the gun is dark in dark rooms. Add to `render.h`
  `float render_light_at(const Scene *s, Vec3 p, float time, bool flashlight)` —
  ambient + unshadowed point lights at the eye, reusing `light_attenuation()` /
  `light_effective_intensity()` from `src/world/light.c`. Clamp to a minimum so
  it's never invisible.
- Dead / level end: `texture = NULL`.

**Tests:** `tests/test_overlay.c` — viewmodel draws in the bottom-center region and not
elsewhere; keyed texels leave the background; `light = 0.5` darkens pixels.
`tests/test_weapon.c` — `weapon_lowered()` 0→1→0 across a switch.
**Done when:** walking sways the gun, firing shows the flash frame and kicks it, switching
drops one gun and raises the next, and the gun goes dark in dark corners.

---

## Step 3 — Temporary lights, muzzle flash

**Why:** in a dark game, a gunshot lighting the corridor is the single strongest mood
cue. It also gives Step 6 explosions for free.

### 3a. Lights with a lifetime — `src/world/light.h/.c`

```c
/* in PointLight: */
float ttl;   /* seconds left; 0 = permanent */

/* replaces light_update(list, time): */
void light_update(LightList *list, float time, float dt);
int  light_add_temporary(LightList *list, PointLight light, float ttl);
```
`light_update()` decrements `ttl` for temporary lights and removes expired ones by
**swap-remove** (copy the last light over it, `count--`). Nothing references lights by
index, so order doesn't matter. Fade intensity with the remaining fraction so the flash
decays instead of popping: store `ttl_total`, scale in `light_effective_intensity()`.
Update callers: `game.c` (`light_update(&g->scene.lights, g->time, dt)`) and
`tests/test_light.c`.

*Why lights stay in `Scene` (world):* render already loops `scene->lights`; a muzzle
flash is just another light for a few ticks. No renderer change needed.

### 3b. Flash data — `WeaponDef`

```c
Vec3 flash_color;     /* e.g. {1.0, 0.8, 0.4} */
float flash_intensity;
float flash_radius;   /* meters */
float flash_time;     /* seconds, ~0.06 */
```
Shotgun brighter and longer than pistol.

### 3c. Spawning — `src/game/game.c`

After `weapon_update()` in `tick_playing()`: if `g->weapon.fired_this_tick`, call
`spawn_muzzle_flash(g)`: position = eye + forward·0.5 m (use
`pose_to_world_dir(game_view(g), (0,0,-1))`), `cast_shadows = false` (keep it cheap —
shadows cost a ray per pixel), `light_add_temporary(...)`.
*Why in game, not weapon.c:* `weapon_update()` gets a `const Scene*` and shouldn't
mutate the world; game.c already coordinates effects of a tick (`resolve_impacts()`).

**Tests:** `tests/test_light.c` — temporary light disappears after its ttl, permanent
ones stay, intensity fades; `tests/test_game.c` — firing adds exactly one light that is
gone a few ticks later.
**Done when:** shooting in a dark corridor briefly lights the walls.

---

## Step 4 — Screen shake and damage flash

**Why:** feedback for being hit and for big guns. These are **presentation only** — they
never change where you aim (shake is on the camera, not the aim pose), so the simulation
stays deterministic.

### 4a. Module — new `src/render/screenfx.h/.c`

```c
typedef struct {
    float shake;          /* current strength, radians */
    float flash;          /* 0..1 tint strength */
    Vec3 flash_color;
} ScreenFx;

void screenfx_shake(ScreenFx *fx, float amount);              /* keeps the max */
void screenfx_flash(ScreenFx *fx, Vec3 color, float amount);
void screenfx_update(ScreenFx *fx, float dt);                 /* exponential decay */
/* Small yaw/pitch offsets for the camera this frame. Smooth noise from time
 * (sum of two sines), not rand(), so it doesn't jitter frame-to-frame. */
void screenfx_camera_offset(const ScreenFx *fx, float time, float *yaw, float *pitch);
```
*Why in render/:* it only affects what's drawn. *Why not in overlay:* the shake moves the
3D camera, which happens before the overlay.

### 4b. Wiring — `src/main.c`

- `play_event_effects(&fx, &game.events)` next to `play_event_sounds()`:
  `EV_PLAYER_HURT` → flash red scaled by `amount`, small shake;
  `EV_WEAPON_FIRED` → shake by a new `WeaponDef.shake` value (shotgun > pistol);
  `EV_EXPLOSION` (Step 6) → shake by distance.
- Camera: `Pose view = game_view(&game); view.yaw += dy; view.pitch += dp;` then
  `camera_from_pose()`.
- Overlay: `OverlayState.flash` + `flash_color`; overlay blends the frame toward the
  color (like `tint_dead()` in `src/render/overlay.c`, but partial).

**Tests:** new `tests/test_screenfx.c` — decay reaches ~0, shake keeps the max of
overlapping requests, offset is 0 when shake is 0; `tests/test_overlay.c` — flash 1.0
turns a grey frame red-dominant.
**Done when:** getting clawed flashes red, the shotgun visibly jolts the view.

---

## Step 5 — Particles: blood, gibs, sparks

**Why:** hits need to read at a glance. Particles are **many and tiny**; ray-tracing them
as sprites would cost ~0.11 ms each per frame (measured in Phase 1.5). Instead, after the
raytrace, project each particle to the screen and splat a few pixels, hidden by a depth
test. Cost becomes per-particle, not per-pixel.

```
raytrace (per pixel) ──> color + depth buffer
particles (per particle) ──> project ──> depth test ──> 1–4 px square
overlay
```

### 5a. Depth buffer — `src/render/render.h/.c`

- `Framebuffer` gains `float depth[SCREEN_W * SCREEN_H];` (distance along the ray;
  `RENDER_FAR` for sky).
- `shade_pixel_lit()` writes `rec.t` (it already has it). Since it returns a color today,
  either pass `float *depth_out` or write both in `render_chunk_func()`.
  *Why distance, not z:* `camera_ray()` rays are unit-length, so `rec.t` is distance, and a
  particle's distance is just `|p − eye|` — no matrix needed for the test.

### 5b. Projection — `src/gfx/camera.h/.c`

The inverse of `camera_ray()`:
```c
/* World point → pixel. False if behind the camera or off-screen. */
bool camera_project(const Camera *cam, Vec3 p, int *px, int *py, float *dist);
```
Key logic (mirror the math in `camera_ray()`):
```
local = rotate p − cam.position by −pitch about X, then −yaw about Y  (inverse of pose_to_world_dir)
if local.z >= −NEAR: return false                 // behind / too close
ndc_x = (local.x / −local.z) / (aspect * tan(fov/2))
ndc_y = (local.y / −local.z) / tan(fov/2)
px = (ndc_x + 1) / 2 * SCREEN_W ; py = (1 − ndc_y) / 2 * SCREEN_H
```
Test it against `camera_ray()`: project a point along the ray through pixel (x, y) and
expect (x, y) back.

### 5c. Particle system — new `src/render/particles.h/.c`

```c
typedef enum { PARTICLE_BLOOD, PARTICLE_GIB, PARTICLE_SPARK, PARTICLE_DEBRIS } ParticleKind;

typedef struct {
    Vec3 pos, vel;
    float life;          /* seconds left */
    ParticleKind kind;   /* picks color, size, gravity from a small table */
} Particle;

#define PARTICLES_MAX 512
typedef struct { Particle items[PARTICLES_MAX]; int count; uint32_t seed; } ParticleSystem;

void particles_burst(ParticleSystem *ps, ParticleKind kind, Vec3 at, Vec3 dir,
                     int count, float speed, float spread);
void particles_update(ParticleSystem *ps, float dt);   /* gravity, life, swap-remove */
void particles_draw(const ParticleSystem *ps, Framebuffer *fb, const Camera *cam);
```
- Kind table (`static const ParticleLook looks[]`): color, size in meters, gravity, life.
  Blood dark red, gibs bigger/darker and bounce once off `y = spawn floor` (optional),
  sparks yellow, no gravity, short life.
- Randomness: a tiny LCG on `ps->seed` — `rand()` is shared/global, and this keeps tests
  reproducible.
- Draw: `camera_project()`; size in pixels = `size_m * focal / dist` clamped 1..4; for each
  covered pixel, write only if `dist < fb->depth[i]`. Darken with distance so far blood
  fades into the fog (reuse the fog idea from `make_render_params()`; a simple
  `1 − dist/FOG_END` is enough).
- Overflow: drop new particles when full (same policy as `EventQueue`).

*Why render/, not world/:* particles are cosmetic, never collide with gameplay, and use
frame `dt`. *Why not game events → entities:* entities are simulation objects with
indices that must stay stable; hundreds of short-lived particles don't belong there.

### 5d. Spawning — `src/main.c` (`spawn_event_particles()`)

From events each frame: `EV_ENEMY_PAIN` → blood at entity position + 1 m; `EV_ENEMY_DIED`
→ gibs + blood; `EV_WEAPON_IMPACT` with `entity == ENTITY_INDEX_NONE` → sparks;
`EV_PLAYER_HURT` → nothing (the red flash covers it); `EV_EXPLOSION` (Step 6) → debris.
For the burst direction on hits, add `Vec3 dir` to `GameEvent` (shot direction for
impacts) — or use "away from the player", which needs no event change.

**Tests:** `tests/test_camera.c` — project/ray round-trip, behind-camera rejected;
new `tests/test_particles.c` — burst count, update kills expired, gravity pulls down,
draw respects the depth buffer (a particle behind a near depth value draws nothing).
**Done when:** shooting a ghoul sprays blood, walls spark, kills leave gibs.

---

## Step 6 — Projectiles and the rocket launcher

**Why:** a slow, visible projectile with splash damage is a different kind of fight
(leading targets, self-damage, area denial) and Step 7's enemy reuses it.

### 6a. Projectiles are entities — `src/world/entity.h`, `entity_types.c`

- `ENTITY_PROJECTILE`, level name `"projectile"` (never placed by designers, but the
  table needs a row). `RENDER_SPRITE`, `SPRITE_GHOST`.
  *Why ghost:* `TRACE_SOLID` must not see projectiles, or rockets would hit each other and
  the shooter's own aim ray would stop at the rocket.
- Union member:
  ```c
  struct {
      int kind;        /* index into game's ProjectileDef table; world treats it as a frame */
      Vec3 velocity;
      int owner;       /* entity index of the shooter, or PROJECTILE_OWNER_PLAYER (-2) */
      float life;      /* seconds until it fizzles */
  } projectile;
  ```
- Sprite hook: frame = `kind` in a `projectiles` sheet (rocket, spit); size from a small
  table in `entity_types.c` (it's a look, not behaviour).
- New proctex sheet `projectiles` (2 frames, 16×16, glowing). Its color should read in the
  dark; later the spit can carry its own light.

### 6b. Runtime spawning needs a material

Projectiles don't exist at load time, so no loader gives them a material. Fix generally:
- `Scene` gains `int type_material[ENTITY_TYPE_COUNT];` filled in `level_finish()`
  (`src/world/level.c`) by calling `matlib_entity(mats, entity_def(t), "")` for every type
  that sets a new `EntityDef` flag `spawns_at_runtime`.
  *Why in level_finish:* `MatLib` only exists during loading, and both loaders share
  `level_finish()`.
- Add `int entity_spawn_runtime(Scene *s, EntityType t, Vec3 pos)` in
  `src/world/scene.c`: reuses an **inactive slot of the same type** if one exists, else
  `entity_add()`. *Why reuse:* indices must stay stable (never remove), but a rocket
  every second would grow the list forever.

### 6c. Behaviour — new `src/game/projectile.h/.c`

```c
typedef enum { PROJECTILE_ROCKET, PROJECTILE_SPIT, PROJECTILE_KIND_COUNT } ProjectileKind;

typedef struct {
    float speed;            /* m/s */
    float damage;           /* direct hit */
    float splash_radius;    /* 0 = none */
    float splash_damage;    /* at the center, linear falloff to 0 at the radius */
    float radius;           /* for hitting the player */
    float life;
    Vec3 light_color;       /* explosion light; intensity 0 = none */
    float light_intensity;
} ProjectileDef;

const ProjectileDef *projectile_def(ProjectileKind kind);
int  projectile_fire(Scene *s, ProjectileKind kind, Vec3 from, Vec3 dir, int owner);
void projectile_update(Scene *s, Player *p, float dt, EventQueue *ev);
```
`projectile_update()` per active projectile (pseudo-code):
```
step = velocity * dt
hit = scene_trace(scene, Ray(pos, dir), ε, |step|, TRACE_SOLID)     // walls + solid sprites
if hit and entity_hit(hit) == owner: ignore it (re-trace from just past it)
player_t = segment_hits_cylinder(pos, step, player body)            // player isn't in the scene
pick the nearest of hit / player_t
if none: pos += step; life -= dt; if life <= 0 → explode at pos
else explode at the impact point:
    direct: entity_damage(target) or player_damage(...)
    splash: for each active entity with a damage hook within splash_radius,
              if a TRACE_SOLID ray from the explosion reaches it → entity_damage(falloff)
            same for the player (rockets hurt you too — Quake rules)
    light_add_temporary(explosion light), push EV_EXPLOSION {pos, amount = splash_radius}
    deactivate the projectile
```
- `segment_hits_cylinder()` (static helper): closest approach of the segment to the
  player's vertical axis in XZ, within `PLAYER_RADIUS + def->radius`, and the Y at that
  point between feet and head. Explain in its comment with a small diagram.
- Damage results must produce the same `EV_ENEMY_PAIN/DIED` and kill counting as
  bullets. **Refactor first:** move the body of `resolve_impacts()` in `src/game/game.c`
  into `void game_damage_entity(Game*, int entity, float amount)` (or a free function
  taking scene + events + kills counter) and call it from both places.
  *Why:* one path for "something got hurt", so later effects (blood, kill counts) can't
  diverge between weapons.
- Call order in `game_tick()`: AI → player → weapon → `projectile_update()` →
  `resolve_impacts()`. Projectiles also update in `GAME_DEAD` (rockets in flight still
  explode).

### 6d. Launcher fires rockets — `WeaponDef` + `src/game/weapon.c`

- `WeaponDef` gains `int projectile;` (`-1` = hitscan). In `weapon_update()`, after taking
  ammo: if `def->projectile >= 0`, emit `EV_WEAPON_FIRED` but don't trace; return a
  request instead. *Why not call `projectile_fire()` from weapon.c:* weapon gets a
  `const Scene*` (read-only). Give `Weapon` a field
  `bool launch_this_tick; Ray launch;` and let `game.c` call `projectile_fire()` — same
  pattern as the muzzle flash.
- Launch from slightly below/right of the eye so the rocket doesn't spawn inside your
  view, but aim along the view ray.

### 6e. Events and audio

- `EV_EXPLOSION` (`pos`, `amount` = radius), optionally `EV_PROJECTILE_FIRED`.
- `src/platform/audio.c`: an explosion (`gen_vocal` won't do — a noise burst + low
  sine, like `gen_weapon_fire()` but longer), a rocket launch whoosh.
- `main.c`: sounds, shake by distance, debris particles.

**Tests:** new `tests/test_projectile.c` (use the floor/wall scene helper from
`tests/test_ai.c`): rocket travels `speed·dt` per tick; hits a wall and explodes; direct hit
damages a ghoul; splash damages a ghoul behind the impact point but **not** one behind a
wall; self-damage applies to the player; owner is never hit; inactive slots are reused.
`tests/test_scene.c`: `entity_spawn_runtime` reuse + material from `type_material`.
**Done when:** rockets fly, light the corridor, gib ghouls in a radius, and hurt you up close.

---

## Step 7 — The spitter (ranged enemy)

**Why:** a ranged enemy forces movement and use of cover — the ghoul only rewards backing up.

### 7a. World — `src/world/entity_types.c`, `src/gfx/proctex.c`

- `ENTITY_SPITTER` (`"spitter"`), `RENDER_SPRITE`, health ~45, `spawn`/`damage`/`sprite`
  hooks shaped like the ghoul's. If the hooks are identical apart from the sheet, factor a
  shared `enemy_damage()` / `enemy_sprite_frame()` instead of copying.
- Sheet `spitter`: 5 frames in the same `GhoulFrame` order (rename the enum to
  `EnemyFrame` so both use it). A hunched shape with a glowing throat; the attack frame
  shows it lit up.

### 7b. AI — `src/game/ai.h/.c`

`EnemyDef` gains:
```c
EnemyAttack attack;       /* ATTACK_MELEE | ATTACK_PROJECTILE */
ProjectileKind projectile;
float preferred_range;    /* ranged: stop approaching inside this, if it can see you */
```
Changes in `tick_enemy()`:
- CHASE: for ranged enemies, if `dist <= preferred_range` **and** `sees_player()`, stand
  still; when `cooldown <= 0`, go ATTACK. Otherwise keep approaching (so it rounds corners
  into view).
- ATTACK at windup end: melee as today; projectile → aim at the player's chest
  (`view.pos − 0.4 m`), spawn from the enemy's eye (`ENEMY_EYE_HEIGHT`), call
  `projectile_fire(..., owner = index)`. If it lost sight during windup, skip the shot and
  return to CHASE (no shooting through walls).
- *Why `projectile_fire` from ai.c is fine here:* ai.c already gets a mutable `Scene*`
  (unlike weapon.c), and it's game layer.

### 7c. Sounds per enemy type — `src/main.c`

`EV_ENEMY_*` events carry the entity index; `play_event_sounds()` currently plays ghoul
sounds for everyone. Add a small table in main:
```c
static const EnemySounds enemy_sounds[ENTITY_TYPE_COUNT] = {
    [ENTITY_GHOUL]   = {SND_ENEMY_ALERT, ...},
    [ENTITY_SPITTER] = {SND_SPITTER_ALERT, ...},
};
```
and look up `game.scene.entities.items[ev->entity].type`. New vocal tones in
`audio_generate_sounds()` (higher, wetter `VocalTone` rows) plus a spit sound.

**Tests:** `tests/test_ai.c` — spitter holds position at preferred range with LOS; fires one
projectile per cooldown; doesn't fire without LOS; walks toward you without LOS.
`tests/test_levelcheck.c` — a floating spitter is flagged (the checker already covers any
entity with a damage hook — confirm).
**Done when:** a spitter at the end of a hall pins you with dodgeable spit until you close
the distance or use cover.

---

## Step 8 — Content, balance, docs

- **Arena map** in TrenchBroom: `levels/arena.map` — a hall with cover, two ghouls, two
  spitters, a shotgun + shells, a launcher + rockets, health, an exit. `make editor` first so
  the new entity types and keys show up.
- **Balance pass** (all data): ghoul vs shotgun (should drop in 1–2 close blasts), rocket
  splash radius (~2 m), spitter cooldown (~1.5 s) and projectile speed (~8 m/s, dodgeable),
  ammo amounts. Watch the F3 overlay: projectiles and temporary lights count against the
  sprite/light budget.
- **Docs:**
  - `CLAUDE.md`: modules `projectile`, `particles`, `screenfx`; patterns: temporary lights,
    runtime spawning + `type_material`, presentation-only effects in main, weapon switching
    phases; entity table rows (`weapon`, `ammo` keys, `spitter`); "New weapon" now includes
    viewmodel texture, flash and projectile fields.
  - `README.md`: controls (1/2/3, wheel), features.
  - `ROADMAP.md`: mark Phase 2 ✅, carry forward what's left (e.g. projectile lights for
    spit, gib bounce, infighting).

---

## Critical files (summary)

| Area | Files |
|------|-------|
| Weapons | `src/game/weapon.h/.c`, `weapon_types.c`, `inventory.h/.c`, `cmd.h` |
| Input | `src/platform/input.c` |
| Game flow | `src/game/game.c` (select weapon, muzzle flash, projectile call, shared damage path) |
| Projectiles | new `src/game/projectile.h/.c`; `src/world/entity.h`, `entity_types.c`, `scene.h/.c`, `level.c` |
| AI | `src/game/ai.h/.c` |
| Lights | `src/world/light.h/.c` |
| Presentation | `src/render/overlay.h/.c`, new `screenfx.h/.c`, new `particles.h/.c`, `render.h/.c` (depth, `render_light_at`), `src/gfx/camera.h/.c` (`camera_project`), `src/main.c` |
| Assets | `src/gfx/proctex.h/.c` (view_*, projectiles, spitter), `src/platform/audio.c` |
| Editor | `src/tools/editor_export.c` (`KEY_STRING`) |

Reuse rather than rewrite: `weapon_pellet_ray()` (shotgun), `scene_trace()` +
`TRACE_SOLID`, `entity_hit()`, `entity_damage()`, `sees_player()` / `step_axis()` in
`ai.c`, `light_attenuation()`, `texture_opaque()`, `sheet_*` helpers in `proctex.c`,
`kv_string()`, `matlib_entity()`, `pose_to_world_dir()`, the scene helpers in
`tests/test_ai.c` / `tests/test_player.c`.

## Verification

Per step: `make test` (all suites green, zero warnings in `make clean && make rt`), plus the
step's "done when" check in the game. Useful while developing:
- `./rt levels/arena.map` and keep TrenchBroom open: saves hot-reload.
- F3: watch work time, lights and sprites while firing rockets and with spitters active.
- Offscreen frames to eyeball new sprites/viewmodels (build a small program against
  `build/librt.a` that renders one frame to a PPM, as done for the ghoul and HUD).
- End of phase: play `levels/arena.map` through — pick up both weapons, kill every enemy with
  each, take damage from both enemy types, die once, reach the exit. Check the terminal for
  `check:` warnings and the `budget:` line.
