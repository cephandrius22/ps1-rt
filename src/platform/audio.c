#include "audio.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* --- Procedural sound generation --- */

static Sound make_sound(int sample_rate, float duration) {
    Sound s;
    s.sample_rate = sample_rate;
    s.length = (int)(duration * sample_rate);
    s.samples = calloc((size_t)s.length, sizeof(int16_t));
    return s;
}

/* Pistol: a sharp crack, a low thump under it, and a muffled tail so
 * the shot hangs in the room for a moment */
static void gen_weapon_fire(Sound *s) {
    const float duration = 0.32f;
    *s = make_sound(AUDIO_SAMPLE_RATE, duration);
    if (!s->samples) {
        return;
    }
    float tail = 0;
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        tail += (noise - tail) * 0.08f;   /* low-passed: the room's rumble */
        float crack = noise * expf(-t * 55.0f);
        float thump = sinf(t * 95.0f * 6.28f) * expf(-t * 18.0f);
        float val = crack * 0.7f + thump * 0.9f + tail * 2.2f * expf(-t * 9.0f);
        float fade = 1.0f - t / duration;
        s->samples[i] = (int16_t)(fminf(fmaxf(val * fade, -1.0f), 1.0f) * 30000.0f);
    }
}

/* A bullet into a body: a wet, low thud */
static void gen_flesh_hit(Sound *s) {
    const float duration = 0.14f;
    *s = make_sound(AUDIO_SAMPLE_RATE, duration);
    if (!s->samples) {
        return;
    }
    float squelch = 0;
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        squelch += (noise - squelch) * 0.25f;
        float thud = sinf(t * 140.0f * 6.28f) * expf(-t * 30.0f);
        float val = (thud * 0.8f + squelch * 1.2f * expf(-t * 25.0f));
        s->samples[i] = (int16_t)(fminf(fmaxf(val, -1.0f), 1.0f) * 20000.0f);
    }
}

static void gen_weapon_impact(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.08f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float env = 1.0f - t / 0.08f;
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        float click = sinf(t * 800.0f * 6.28f) * expf(-t * 60.0f);
        float val = (noise * 0.3f + click * 0.7f) * env;
        s->samples[i] = (int16_t)(val * 16000.0f);
    }
}

static void gen_footstep(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.1f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float env = (1.0f - t / 0.1f);
        env = env * env * env;
        /* Low thud with filtered noise */
        float thud = sinf(t * 60.0f * 6.28f) * expf(-t * 30.0f);
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        float val = (thud * 0.7f + noise * 0.15f) * env;
        s->samples[i] = (int16_t)(val * 12000.0f);
    }
}

static void gen_pickup(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.25f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float env = 1.0f - t / 0.25f;
        /* Rising tone — two harmonics */
        float freq = 440.0f + t * 800.0f;
        float val = sinf(t * freq * 6.28f) * 0.6f
                  + sinf(t * freq * 2.0f * 6.28f) * 0.3f;
        s->samples[i] = (int16_t)(val * env * 14000.0f);
    }
}

static void gen_door_open(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.3f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float env = 1.0f - t / 0.3f;
        /* Creaky scraping sound: low frequency + noise */
        float creak = sinf(t * 80.0f * 6.28f + sinf(t * 13.0f * 6.28f) * 3.0f);
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        float val = (creak * 0.5f + noise * 0.2f) * env;
        s->samples[i] = (int16_t)(val * 10000.0f);
    }
}

static void gen_door_close(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.2f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float env = expf(-t * 12.0f);
        /* Solid thump */
        float thump = sinf(t * 90.0f * 6.28f);
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        float val = (thump * 0.7f + noise * 0.2f) * env;
        s->samples[i] = (int16_t)(val * 14000.0f);
    }
}

/* Two detuned square-ish tones: a short, unfriendly buzz */
static void gen_door_locked(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.25f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float env = t < 0.2f ? 1.0f : 1.0f - (t - 0.2f) / 0.05f;
        float a = sinf(t * 110.0f * 6.28f) > 0 ? 1.0f : -1.0f;
        float b = sinf(t * 116.0f * 6.28f) > 0 ? 1.0f : -1.0f;
        s->samples[i] = (int16_t)((a + b) * 0.5f * env * 6000.0f);
    }
}

/* Sharp metallic click: a fast-decaying high tone over a thud */
static void gen_switch(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.12f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float click = sinf(t * 1800.0f * 6.28f) * expf(-t * 60.0f);
        float thud = sinf(t * 140.0f * 6.28f) * expf(-t * 25.0f);
        s->samples[i] = (int16_t)((click * 0.6f + thud * 0.5f) * 12000.0f);
    }
}

/* Bright noise burst over a few high, quickly dying rings: breaking glass */
static void gen_glass(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.35f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        float rings = sinf(t * 3100.0f * 6.28f) + sinf(t * 4700.0f * 6.28f) * 0.7f;
        float val = noise * expf(-t * 30.0f) * 0.6f + rings * expf(-t * 12.0f) * 0.25f;
        s->samples[i] = (int16_t)(val * 12000.0f);
    }
}

static void gen_ambient(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 3.0f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        /* Dark drone with slow modulation */
        float drone = sinf(t * 40.0f * 6.28f) * 0.3f
                    + sinf(t * 42.5f * 6.28f) * 0.2f
                    + sinf(t * 80.0f * 6.28f + sinf(t * 0.5f * 6.28f) * 2.0f) * 0.15f;
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f * 0.05f;
        float val = drone + noise;
        /* Fade in/out for seamless looping */
        float fade_len = 0.1f;
        if (t < fade_len) {
            val *= t / fade_len;
        }
        if (t > 3.0f - fade_len) {
            val *= (3.0f - t) / fade_len;
        }
        s->samples[i] = (int16_t)(val * 8000.0f);
    }
}

#define LOOP_LEN  3.0f /* seconds; loops fade at both ends to join seamlessly */
#define LOOP_FADE 0.1f

static float loop_fade(float t) {
    if (t < LOOP_FADE) {
        return t / LOOP_FADE;
    }
    if (t > LOOP_LEN - LOOP_FADE) {
        return (LOOP_LEN - t) / LOOP_FADE;
    }
    return 1.0f;
}

/* Sparse drops: short falling pings at a few fixed times, faint air */
static void gen_ambient_drip(Sound *s) {
    static const float drops[] = {0.35f, 1.1f, 1.45f, 2.3f};
    *s = make_sound(AUDIO_SAMPLE_RATE, LOOP_LEN);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float val = ((float)(rand() % 20000) - 10000) / 10000.0f * 0.02f;
        for (size_t d = 0; d < sizeof(drops) / sizeof(drops[0]); d++) {
            float dt = t - drops[d];
            if (dt >= 0 && dt < 0.15f) {
                float freq = 1400.0f - dt * 3000.0f; /* pitch falls: a drop */
                val += sinf(dt * freq * 6.28f) * expf(-dt * 35.0f) * 0.6f;
            }
        }
        s->samples[i] = (int16_t)(val * loop_fade(t) * 9000.0f);
    }
}

/* Mains hum: 50 Hz with harmonics and a slow beat */
static void gen_ambient_hum(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, LOOP_LEN);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float beat = 0.8f + 0.2f * sinf(t * 2.0f / LOOP_LEN * 6.28f);
        float val = (sinf(t * 50.0f * 6.28f) * 0.4f + sinf(t * 100.0f * 6.28f) * 0.25f +
                     sinf(t * 150.0f * 6.28f) * 0.1f) * beat;
        s->samples[i] = (int16_t)(val * loop_fade(t) * 7000.0f);
    }
}

/* --- Church loops. Unlike the fades above, these repeat exactly: every
 * tone completes whole cycles per loop, filters run over the loop twice
 * (the second pass starts in the state the end leaves), and bell tails
 * wrap around to the start. No dip or click at the seam. --- */

#define TAU         6.2831853f
#define WIND_LEN    6.0f
#define ORGAN_LEN   8.0f
#define BELL_LEN    12.0f
#define FILTER_PASSES 2     /* settle pass, then the pass that's kept */

/* Repeatable white noise in -1..1: loops sound the same every run */
static float lcg_noise(uint32_t *state) {
    *state = *state * 1664525u + 1013904223u;
    return (float)(*state >> 8) / 8388608.0f - 1.0f;
}

/* freq rounded to whole cycles per loop of len seconds, so it joins */
static float loop_hz(float freq, float len) {
    return roundf(freq * len) / len;
}

/* Allocate s and a zeroed float work buffer of the same length */
static float *loop_alloc(Sound *s, float len) {
    *s = make_sound(AUDIO_SAMPLE_RATE, len);
    if (!s->samples) {
        return NULL;
    }
    return calloc((size_t)s->length, sizeof(float));
}

/* Scale buf so its loudest sample reaches peak, into s; frees buf */
static void loop_finish(Sound *s, float *buf, float peak) {
    float max = 1e-6f;
    for (int i = 0; i < s->length; i++) {
        max = fmaxf(max, fabsf(buf[i]));
    }
    for (int i = 0; i < s->length; i++) {
        s->samples[i] = (int16_t)(buf[i] / max * peak);
    }
    free(buf);
}

/* Wind through a high window: low rumble plus a whistle from a resonant
 * band-pass whose pitch rises with each gust. Gusts are slow sines with
 * whole cycles per loop:
 *
 *   noise ──┬─> one-pole low-pass ────────── rumble ─┐
 *           └─> band-pass (fc = 250..700 Hz) ─ whistle ┴─> × gust
 */
static void gen_ambient_wind(Sound *s) {
    float *buf = loop_alloc(s, WIND_LEN);
    if (!buf) {
        return;
    }
    uint32_t seed = 0x5eed1u;
    for (int i = 0; i < s->length; i++) {
        buf[i] = lcg_noise(&seed);
    }

    float low = 0, band = 0, rumble = 0;
    for (int pass = 0; pass < FILTER_PASSES; pass++) {
        for (int i = 0; i < s->length; i++) {
            float t = (float)i / s->sample_rate;
            float gust = 0.55f + 0.3f * sinf(TAU * t / WIND_LEN) +
                         0.15f * sinf(TAU * t * 3 / WIND_LEN + 1.3f);
            float fc = 250.0f + 450.0f * gust;
            float f = 2.0f * sinf(TAU / 2 * fc / s->sample_rate);

            /* State-variable filter; small damping = whistly resonance */
            float noise = buf[i];
            low += f * band;
            band += f * (noise - low - 0.12f * band);
            rumble += (noise - rumble) * 0.02f;
            if (pass == FILTER_PASSES - 1) {
                buf[i] = (band * 0.12f * gust + rumble * 2.0f) * gust;
            }
        }
    }
    loop_finish(s, buf, 9000.0f);
}

#define ORGAN_TABLE 1024  /* one cycle of the pipe waveform */
#define ORGAN_NOTES 4

/* Pipe tone: a fundamental and fading harmonics (an open diapason) */
static const float ORGAN_HARMONICS[] = {1.0f, 0.5f, 0.35f, 0.2f, 0.12f, 0.08f};

/* D minor, then B-flat major, low in the pedal range */
static const float ORGAN_CHORDS[2][ORGAN_NOTES] = {
    {73.42f, 110.0f, 146.83f, 174.61f},
    {58.27f, 87.31f, 116.54f, 146.83f},
};

/* Distant organ: two chords swelling into each other over the loop.
 * Each note plays a precomputed pipe cycle (cheap), plus a copy 0.2%
 * sharp so the pipes beat slowly like a real rank. Every pipe starts at
 * its own phase: all in step at once would thump at each loop start. */
static void gen_ambient_organ(Sound *s) {
    float *buf = loop_alloc(s, ORGAN_LEN);
    if (!buf) {
        return;
    }
    float table[ORGAN_TABLE];
    for (int k = 0; k < ORGAN_TABLE; k++) {
        float v = 0;
        for (size_t h = 0; h < sizeof(ORGAN_HARMONICS) / sizeof(ORGAN_HARMONICS[0]); h++) {
            v += ORGAN_HARMONICS[h] * sinf(TAU * (h + 1) * k / ORGAN_TABLE);
        }
        table[k] = v;
    }

    uint32_t seed = 0x0f9a1u;
    for (int c = 0; c < 2; c++) {
        for (int n = 0; n < ORGAN_NOTES; n++) {
            for (int detune = 0; detune < 2; detune++) {
                double hz = loop_hz(ORGAN_CHORDS[c][n] * (detune ? 1.002f : 1.0f), ORGAN_LEN);
                double start = lcg_noise(&seed) * 0.5 + 0.5;
                for (int i = 0; i < s->length; i++) {
                    double t = (double)i / s->sample_rate;
                    double phase = hz * t + start;
                    float wave = table[(int)((phase - floor(phase)) * ORGAN_TABLE)];

                    /* Chord 0 full at the start, chord 1 halfway */
                    float w = 0.5f + 0.5f * cosf(TAU * (float)t / ORGAN_LEN);
                    buf[i] += wave * (c == 0 ? w : 1.0f - w);
                }
            }
        }
    }

    /* Slow swell, and the breath of the pipes (low-passed noise) */
    float breath = 0;
    float *air = calloc((size_t)s->length, sizeof(float));
    if (air) {
        for (int i = 0; i < s->length; i++) {
            air[i] = lcg_noise(&seed);
        }
        for (int pass = 0; pass < FILTER_PASSES; pass++) {
            for (int i = 0; i < s->length; i++) {
                breath += (air[i] - breath) * 0.05f;
                if (pass == FILTER_PASSES - 1) {
                    buf[i] += breath * 4.0f;
                }
            }
        }
        free(air);
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        buf[i] *= 0.8f + 0.2f * sinf(TAU * t * 2 / ORGAN_LEN);
    }
    loop_finish(s, buf, 7000.0f);
}

/* Church bell partials relative to the strike note: hum (an octave
 * below), prime, minor-third tierce, quint, nominal and upper partials.
 * Low ones ring longest. */
typedef struct {
    float ratio;
    float gain;
    float decay;  /* per second */
} BellPartial;

static const BellPartial BELL_PARTIALS[] = {
    {0.5f, 0.35f, 0.35f}, {1.0f, 0.5f, 0.6f}, {1.19f, 0.3f, 0.8f}, {1.5f, 0.2f, 1.0f},
    {2.0f, 0.45f, 0.9f}, {2.51f, 0.15f, 1.6f}, {3.01f, 0.12f, 2.0f}, {4.1f, 0.08f, 3.0f},
};

#define BELL_NOTE   196.0f  /* G3 */
#define BELL_RING   9.0f    /* seconds each strike is rendered */
#define BELL_STRIKE_1 0.5f  /* a slow toll: twice per loop */
#define BELL_STRIKE_2 6.5f

/* One strike, added into the loop at `at`; the tail wraps to the start */
static void bell_strike(Sound *s, float *buf, float at) {
    int start = (int)(at * s->sample_rate);
    int len = (int)(BELL_RING * s->sample_rate);
    uint32_t seed = 0xbe11u;
    for (int j = 0; j < len; j++) {
        float t = (float)j / s->sample_rate;
        float v = lcg_noise(&seed) * expf(-t * 60.0f) * 0.3f; /* clapper */
        for (size_t p = 0; p < sizeof(BELL_PARTIALS) / sizeof(BELL_PARTIALS[0]); p++) {
            const BellPartial *b = &BELL_PARTIALS[p];
            v += sinf(TAU * BELL_NOTE * b->ratio * t) * b->gain * expf(-t * b->decay);
        }
        buf[(start + j) % s->length] += v;
    }
}

/* A distant church bell tolling, muffled by the walls (low-passed) */
static void gen_ambient_bell(Sound *s) {
    float *buf = loop_alloc(s, BELL_LEN);
    if (!buf) {
        return;
    }
    bell_strike(s, buf, BELL_STRIKE_1);
    bell_strike(s, buf, BELL_STRIKE_2);

    float *dry = malloc(sizeof(float) * (size_t)s->length);
    if (dry) {
        memcpy(dry, buf, sizeof(float) * (size_t)s->length);
        float muffled = 0;
        for (int pass = 0; pass < FILTER_PASSES; pass++) {
            for (int i = 0; i < s->length; i++) {
                muffled += (dry[i] - muffled) * 0.25f;
                if (pass == FILTER_PASSES - 1) {
                    buf[i] = muffled;
                }
            }
        }
        free(dry);
    }
    loop_finish(s, buf, 10000.0f);
}

/* Three rising notes, each a soft bell */
static void gen_secret(Sound *s) {
    static const float notes[] = {523.0f, 659.0f, 784.0f}; /* C5 E5 G5 */
    const float step = 0.12f;
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.7f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float val = 0;
        for (int n = 0; n < 3; n++) {
            float dt = t - n * step;
            if (dt >= 0) {
                val += sinf(dt * notes[n] * 6.28f) * expf(-dt * 5.0f) * 0.35f;
            }
        }
        s->samples[i] = (int16_t)(val * 12000.0f);
    }
}

static void gen_dry_fire(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.05f);
    if (!s->samples) {
        return;
    }
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float click = sinf(t * 2400.0f * 6.28f) * expf(-t * 120.0f);
        s->samples[i] = (int16_t)(click * 9000.0f);
    }
}

/* Rough vocal: a buzzy tone sliding from f0 to f1 Hz, roughened by
 * vibrato ("rasp") and noise. One generator covers grunts, growls,
 * shrieks and death gurgles with different settings. */
typedef struct {
    float duration; /* seconds */
    float f0, f1;   /* start/end pitch, Hz */
    float rasp;     /* vibrato depth, 0..1 */
    float noise;    /* breath/noise mix, 0..1 */
    float gain;     /* peak amplitude, 0..1 */
} VocalTone;

#define VOCAL_ATTACK   0.01f  /* seconds of fade-in to avoid a click */
#define VOCAL_RASP_HZ  31.0f  /* vibrato rate: fast enough to sound rough */
#define PCM_MAX        32000.0f

static void gen_vocal(Sound *s, VocalTone v) {
    *s = make_sound(AUDIO_SAMPLE_RATE, v.duration);
    if (!s->samples) {
        return;
    }

    float phase = 0;
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float k = t / v.duration;

        /* Slide pitch, wobble it, and accumulate phase so the slide is smooth */
        float freq = v.f0 + (v.f1 - v.f0) * k;
        freq *= 1.0f + v.rasp * sinf(t * VOCAL_RASP_HZ * 6.28f);
        phase += freq * 6.28f / s->sample_rate;

        /* Buzzy: fundamental plus two harmonics */
        float tone = sinf(phase) + 0.5f * sinf(2 * phase) + 0.33f * sinf(3 * phase);
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        float val = tone * (1 - v.noise) * 0.55f + noise * v.noise;

        float env = fminf(t / VOCAL_ATTACK, 1.0f) * (1.0f - k) * (1.0f - k);
        s->samples[i] = (int16_t)(val * env * v.gain * PCM_MAX);
    }
}

/* Noise burst over a low thump, both decaying: shotgun, explosion.
 * A one-pole low-pass makes the noise a muffled roar instead of hiss. */
typedef struct {
    float duration;   /* seconds */
    float thump_hz;   /* low sine body */
    float decay;      /* envelope e-folding rate, 1/s */
    float lowpass;    /* 0..1: noise smoothing (higher = duller) */
    float gain;
} BlastTone;

static void gen_blast(Sound *s, BlastTone b) {
    *s = make_sound(AUDIO_SAMPLE_RATE, b.duration);
    if (!s->samples) {
        return;
    }
    float smooth = 0;
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        smooth += (noise - smooth) * (1.0f - b.lowpass);
        float thump = sinf(t * b.thump_hz * 6.28f);
        float env = expf(-t * b.decay) * fminf(t / VOCAL_ATTACK, 1.0f);
        s->samples[i] = (int16_t)((smooth * 0.7f + thump * 0.5f) * env * b.gain * PCM_MAX);
    }
}

/* Whoosh: filtered noise swelling then fading, with a falling tone */
static void gen_rocket_launch(Sound *s) {
    float duration = 0.45f;
    *s = make_sound(AUDIO_SAMPLE_RATE, duration);
    if (!s->samples) {
        return;
    }
    float smooth = 0;
    float phase = 0;
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float k = t / duration;
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        smooth += (noise - smooth) * 0.3f;
        phase += (300.0f - 200.0f * k) * 6.28f / s->sample_rate;
        float env = sinf(k * 3.14159f);
        s->samples[i] = (int16_t)((smooth * 0.6f + sinf(phase) * 0.3f) * env * 0.7f * PCM_MAX);
    }
}

void audio_generate_sounds(Sound sounds[SND_COUNT]) {
    gen_weapon_fire(&sounds[SND_WEAPON_FIRE]);
    gen_weapon_impact(&sounds[SND_WEAPON_IMPACT]);
    gen_flesh_hit(&sounds[SND_FLESH_HIT]);
    gen_footstep(&sounds[SND_FOOTSTEP]);
    gen_pickup(&sounds[SND_PICKUP]);
    gen_door_open(&sounds[SND_DOOR_OPEN]);
    gen_door_close(&sounds[SND_DOOR_CLOSE]);
    gen_door_locked(&sounds[SND_DOOR_LOCKED]);
    gen_switch(&sounds[SND_SWITCH]);
    gen_glass(&sounds[SND_GLASS]);
    gen_ambient(&sounds[SND_AMBIENT]);
    gen_ambient_drip(&sounds[SND_AMBIENT_DRIP]);
    gen_ambient_hum(&sounds[SND_AMBIENT_HUM]);
    gen_ambient_wind(&sounds[SND_AMBIENT_WIND]);
    gen_ambient_organ(&sounds[SND_AMBIENT_ORGAN]);
    gen_ambient_bell(&sounds[SND_AMBIENT_BELL]);
    gen_secret(&sounds[SND_SECRET]);
    gen_dry_fire(&sounds[SND_DRY_FIRE]);

    /*                                         dur    f0     f1    rasp  noise gain */
    gen_vocal(&sounds[SND_PLAYER_PAIN],  (VocalTone){0.25f, 160.0f, 110.0f, 0.05f, 0.25f, 0.6f});
    gen_vocal(&sounds[SND_PLAYER_DEATH], (VocalTone){0.90f, 150.0f, 45.0f,  0.10f, 0.30f, 0.7f});
    gen_vocal(&sounds[SND_ENEMY_ALERT],  (VocalTone){0.60f, 75.0f,  60.0f,  0.25f, 0.35f, 0.7f});
    gen_vocal(&sounds[SND_ENEMY_ATTACK], (VocalTone){0.30f, 210.0f, 120.0f, 0.20f, 0.50f, 0.7f});
    gen_vocal(&sounds[SND_ENEMY_PAIN],   (VocalTone){0.20f, 320.0f, 230.0f, 0.10f, 0.30f, 0.6f});
    gen_vocal(&sounds[SND_ENEMY_DEATH],  (VocalTone){0.90f, 130.0f, 35.0f,  0.30f, 0.45f, 0.8f});

    /*                                        dur    thump  decay lowpass gain */
    gen_blast(&sounds[SND_SHOTGUN_FIRE], (BlastTone){0.6f, 55.0f, 7.0f, 0.3f, 1.0f});
    gen_blast(&sounds[SND_EXPLOSION],    (BlastTone){1.60f, 38.0f, 2.8f,  0.8f, 1.0f});
    gen_rocket_launch(&sounds[SND_ROCKET_LAUNCH]);

    /* Spitter: higher, wetter (more noise) than the ghoul */
    gen_vocal(&sounds[SND_SPITTER_ALERT],  (VocalTone){0.50f, 180.0f, 140.0f, 0.30f, 0.55f, 0.6f});
    gen_vocal(&sounds[SND_SPITTER_ATTACK], (VocalTone){0.35f, 260.0f, 380.0f, 0.15f, 0.70f, 0.7f});
    gen_vocal(&sounds[SND_SPITTER_PAIN],   (VocalTone){0.20f, 420.0f, 300.0f, 0.15f, 0.40f, 0.6f});
    gen_vocal(&sounds[SND_SPITTER_DEATH],  (VocalTone){0.80f, 240.0f, 60.0f,  0.35f, 0.60f, 0.8f});
    gen_blast(&sounds[SND_SPIT_SPLAT], (BlastTone){0.25f, 160.0f, 18.0f, 0.5f, 0.6f});
}

void sound_free(Sound *s) {
    free(s->samples);
    s->samples = NULL;
    s->length = 0;
}

/* --- Mixing --- */

#define ECHO_LEFT       1543   /* delay lengths in samples: unequal, so the */
#define ECHO_RIGHT      1867   /* tail doesn't ring at one pitch (~70/85 ms) */
#define ECHO_WET_MAX    0.6f   /* echo level at amount 1 */
#define ECHO_FEED_BASE  0.3f   /* feedback: how long the tail lasts */
#define ECHO_FEED_RANGE 0.4f
#define ECHO_INPUT      0.5f   /* share of the dry signal fed into the echo */

_Static_assert(ECHO_LEFT < AUDIO_ECHO_MAX && ECHO_RIGHT < AUDIO_ECHO_MAX,
               "echo delays must fit their buffers");

static float clamp_sample(float v) {
    return v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v;
}

/* Constant-power pan: the voice keeps its loudness as it moves across */
static void pan_gains(float pan, float *left, float *right) {
    float p = pan < -1 ? -1 : pan > 1 ? 1 : pan;
    *left = sqrtf((1.0f - p) * 0.5f);
    *right = sqrtf((1.0f + p) * 0.5f);
}

/* Next sample of a voice (0 once it ends), advancing it */
static float voice_next(Voice *voice) {
    const Sound *snd = voice->sound;
    if (voice->position >= snd->length) {
        if (!voice->looping) {
            voice->sound = NULL;
            return 0;
        }
        voice->position = 0;
    }
    return snd->samples[voice->position++] * voice->volume;
}

/* Two delay lines, each fed by the other: a wide, diffuse tail */
static void echo(Reverb *r, float *left, float *right) {
    if (r->amount <= 0) {
        return;
    }
    float wet = r->amount * ECHO_WET_MAX;
    float feed = ECHO_FEED_BASE + ECHO_FEED_RANGE * r->amount;
    float dl = r->left[r->pos_left];
    float dr = r->right[r->pos_right];

    r->left[r->pos_left] = *left * ECHO_INPUT + dr * feed;
    r->right[r->pos_right] = *right * ECHO_INPUT + dl * feed;
    r->pos_left = (r->pos_left + 1) % ECHO_LEFT;
    r->pos_right = (r->pos_right + 1) % ECHO_RIGHT;

    *left += dl * wet;
    *right += dr * wet;
}

void audio_mix(AudioSystem *audio, int16_t *out, int frames) {
    for (int i = 0; i < frames; i++) {
        float left = 0, right = 0;
        for (int v = 0; v < AUDIO_MAX_VOICES; v++) {
            Voice *voice = &audio->voices[v];
            if (!voice->sound) {
                continue;
            }
            float sample = voice_next(voice);
            float gl, gr;
            pan_gains(voice->pan, &gl, &gr);
            left += sample * gl;
            right += sample * gr;
        }
        echo(&audio->reverb, &left, &right);
        left *= audio->master;
        right *= audio->master;
        out[i * AUDIO_CHANNELS] = (int16_t)clamp_sample(left);
        out[i * AUDIO_CHANNELS + 1] = (int16_t)clamp_sample(right);
    }
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    int frames = len / (int)(sizeof(int16_t) * AUDIO_CHANNELS);
    audio_mix(userdata, (int16_t *)stream, frames);
}

/* --- Public API --- */

int audio_init(AudioSystem *audio) {
    memset(audio, 0, sizeof(*audio));
    audio->initialized = false;

    audio->master = 1.0f;
    audio_generate_sounds(audio->sounds);

    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = AUDIO_CHANNELS;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = audio;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL audio: %s\n", SDL_GetError());
        return -1;
    }
    audio->device_id = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio->device_id == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return -1;
    }

    /* Start playback */
    SDL_PauseAudioDevice(audio->device_id, 0);
    audio->initialized = true;
    return 0;
}

void audio_shutdown(AudioSystem *audio) {
    if (audio->device_id) {
        SDL_CloseAudioDevice(audio->device_id);
        audio->device_id = 0;
    }
    for (int i = 0; i < SND_COUNT; i++) {
        sound_free(&audio->sounds[i]);
    }
    audio->initialized = false;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/* Claim a free voice for a sound; -1 if none (or no device/sound) */
static int start_voice(AudioSystem *audio, SoundID id, float volume, float pan, bool looping) {
    if (!audio->initialized || id < 0 || id >= SND_COUNT || !audio->sounds[id].samples) {
        return -1;
    }

    SDL_LockAudioDevice(audio->device_id);
    int slot = -1;
    for (int i = 0; i < AUDIO_MAX_VOICES && slot < 0; i++) {
        if (!audio->voices[i].sound) {
            slot = i;
        }
    }
    if (slot >= 0) {
        audio->voices[slot] = (Voice){.sound = &audio->sounds[id], .volume = volume,
                                      .pan = pan, .looping = looping};
    }
    SDL_UnlockAudioDevice(audio->device_id);
    return slot;
}

int audio_play(AudioSystem *audio, SoundID id, float volume) {
    return start_voice(audio, id, volume, 0, false);
}

int audio_play_at(AudioSystem *audio, SoundID id, float volume, float pan) {
    return start_voice(audio, id, volume, pan, false);
}

int audio_play_loop(AudioSystem *audio, SoundID id, float volume) {
    return start_voice(audio, id, volume, 0, true);
}

void audio_set_volume(AudioSystem *audio, int voice, float volume) {
    if (!audio->initialized || voice < 0 || voice >= AUDIO_MAX_VOICES) {
        return;
    }
    SDL_LockAudioDevice(audio->device_id);
    audio->voices[voice].volume = volume;
    SDL_UnlockAudioDevice(audio->device_id);
}

void audio_set_master(AudioSystem *audio, float volume) {
    if (!audio->initialized) {
        audio->master = volume;
        return;
    }
    SDL_LockAudioDevice(audio->device_id);
    audio->master = volume;
    SDL_UnlockAudioDevice(audio->device_id);
}

void audio_set_reverb(AudioSystem *audio, float amount) {
    if (!audio->initialized) {
        audio->reverb.amount = amount; /* no device: nothing to lock against */
        return;
    }
    SDL_LockAudioDevice(audio->device_id);
    audio->reverb.amount = amount;
    SDL_UnlockAudioDevice(audio->device_id);
}

void audio_stop(AudioSystem *audio, int voice) {
    if (!audio->initialized) {
        return;
    }
    if (voice < 0 || voice >= AUDIO_MAX_VOICES) {
        return;
    }

    SDL_LockAudioDevice(audio->device_id);
    audio->voices[voice].sound = NULL;
    SDL_UnlockAudioDevice(audio->device_id);
}

int audio_load_wav(AudioSystem *audio, SoundID id, const char *filename) {
    if (id < 0 || id >= SND_COUNT) {
        return -1;
    }

    SDL_AudioSpec spec;
    Uint8 *wav_buf = NULL;
    Uint32 wav_len = 0;

    if (!SDL_LoadWAV(filename, &spec, &wav_buf, &wav_len)) {
        fprintf(stderr, "SDL_LoadWAV(%s): %s\n", filename, SDL_GetError());
        return -1;
    }

    /* Convert to our format: mono 16-bit at AUDIO_SAMPLE_RATE */
    SDL_AudioCVT cvt;
    int ret = SDL_BuildAudioCVT(&cvt,
        spec.format, spec.channels, spec.freq,
        AUDIO_S16SYS, 1, AUDIO_SAMPLE_RATE);

    if (ret < 0) {
        SDL_FreeWAV(wav_buf);
        return -1;
    }

    cvt.len = (int)wav_len;
    cvt.buf = malloc((size_t)(wav_len * (size_t)cvt.len_mult));
    if (!cvt.buf) {
        SDL_FreeWAV(wav_buf);
        return -1;
    }
    memcpy(cvt.buf, wav_buf, wav_len);
    SDL_FreeWAV(wav_buf);

    if (ret > 0) {
        SDL_ConvertAudio(&cvt);
    }

    /* Replace existing sound */
    if (audio->initialized) {
        SDL_LockAudioDevice(audio->device_id);
    }

    sound_free(&audio->sounds[id]);
    audio->sounds[id].samples = (int16_t *)cvt.buf;
    audio->sounds[id].length = cvt.len_cvt / (int)sizeof(int16_t);
    audio->sounds[id].sample_rate = AUDIO_SAMPLE_RATE;

    if (audio->initialized) {
        SDL_UnlockAudioDevice(audio->device_id);
    }

    return 0;
}
