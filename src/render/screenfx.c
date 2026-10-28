#include "screenfx.h"
#include <math.h>

#define SHAKE_DECAY  9.0f   /* per second: a jolt is mostly gone in ~0.3 s */
#define FLASH_DECAY  4.0f   /* per second: a hit flash lingers a little longer */
#define FX_EPSILON   1e-3f  /* below this (invisible) an effect snaps to zero */

/* Incommensurate frequencies so the motion never visibly repeats */
#define SHAKE_FREQ_A 31.0f
#define SHAKE_FREQ_B 47.0f
#define SHAKE_FREQ_C 23.0f
#define SHAKE_FREQ_D 53.0f

void screenfx_shake(ScreenFx *fx, float amount) {
    fx->shake = fmaxf(fx->shake, amount);
}

void screenfx_flash(ScreenFx *fx, Vec3 color, float amount) {
    if (amount < fx->flash) {
        return;
    }
    fx->flash = fminf(amount, 1.0f);
    fx->flash_color = color;
}

static float decay(float value, float rate, float dt) {
    value *= expf(-rate * dt);
    return value < FX_EPSILON ? 0.0f : value;
}

void screenfx_update(ScreenFx *fx, float dt) {
    fx->shake = decay(fx->shake, SHAKE_DECAY, dt);
    fx->flash = decay(fx->flash, FLASH_DECAY, dt);
}

void screenfx_camera_offset(const ScreenFx *fx, float time, float *yaw, float *pitch) {
    float a = fx->shake;
    *yaw = a * 0.5f * (sinf(time * SHAKE_FREQ_A) + sinf(time * SHAKE_FREQ_B));
    *pitch = a * 0.5f * (sinf(time * SHAKE_FREQ_C + 1.0f) + sinf(time * SHAKE_FREQ_D));
}
