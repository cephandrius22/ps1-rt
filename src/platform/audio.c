#include "audio.h"
#include <SDL.h>
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

static void gen_weapon_fire(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.15f);
    if (!s->samples) return;
    for (int i = 0; i < s->length; i++) {
        float t = (float)i / s->sample_rate;
        float env = 1.0f - t / 0.15f;
        env = env * env;
        /* Mix noise burst with low-frequency punch */
        float noise = ((float)(rand() % 20000) - 10000) / 10000.0f;
        float punch = sinf(t * 120.0f * 6.28f) * expf(-t * 40.0f);
        float val = (noise * 0.6f + punch * 0.8f) * env;
        s->samples[i] = (int16_t)(val * 24000.0f);
    }
}

static void gen_weapon_impact(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 0.08f);
    if (!s->samples) return;
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
    if (!s->samples) return;
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
    if (!s->samples) return;
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
    if (!s->samples) return;
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
    if (!s->samples) return;
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

static void gen_ambient(Sound *s) {
    *s = make_sound(AUDIO_SAMPLE_RATE, 3.0f);
    if (!s->samples) return;
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
        if (t < fade_len) val *= t / fade_len;
        if (t > 3.0f - fade_len) val *= (3.0f - t) / fade_len;
        s->samples[i] = (int16_t)(val * 8000.0f);
    }
}

void audio_generate_sounds(Sound sounds[SND_COUNT]) {
    gen_weapon_fire(&sounds[SND_WEAPON_FIRE]);
    gen_weapon_impact(&sounds[SND_WEAPON_IMPACT]);
    gen_footstep(&sounds[SND_FOOTSTEP]);
    gen_pickup(&sounds[SND_PICKUP]);
    gen_door_open(&sounds[SND_DOOR_OPEN]);
    gen_door_close(&sounds[SND_DOOR_CLOSE]);
    gen_ambient(&sounds[SND_AMBIENT]);
}

void sound_free(Sound *s) {
    free(s->samples);
    s->samples = NULL;
    s->length = 0;
}

/* --- SDL2 audio callback --- */

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    AudioSystem *audio = userdata;
    int sample_count = len / (int)sizeof(int16_t);
    int16_t *out = (int16_t *)stream;

    memset(stream, 0, (size_t)len);

    for (int v = 0; v < AUDIO_MAX_VOICES; v++) {
        Voice *voice = &audio->voices[v];
        if (!voice->sound) continue;

        const Sound *snd = voice->sound;
        for (int i = 0; i < sample_count; i++) {
            if (voice->position >= snd->length) {
                if (voice->looping) {
                    voice->position = 0;
                } else {
                    voice->sound = NULL;
                    break;
                }
            }
            /* Mix into output (clamp to int16 range) */
            int32_t mixed = out[i] + (int32_t)(snd->samples[voice->position] * voice->volume);
            if (mixed > 32767) mixed = 32767;
            if (mixed < -32768) mixed = -32768;
            out[i] = (int16_t)mixed;
            voice->position++;
        }
    }
}

/* --- Public API --- */

int audio_init(AudioSystem *audio) {
    memset(audio, 0, sizeof(*audio));
    audio->initialized = false;

    audio_generate_sounds(audio->sounds);

    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = audio;

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
    for (int i = 0; i < SND_COUNT; i++)
        sound_free(&audio->sounds[i]);
    audio->initialized = false;
}

int audio_play(AudioSystem *audio, SoundID id, float volume) {
    if (!audio->initialized) return -1;
    if (id < 0 || id >= SND_COUNT) return -1;
    if (!audio->sounds[id].samples) return -1;

    /* Find a free voice */
    SDL_LockAudioDevice(audio->device_id);
    int slot = -1;
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (!audio->voices[i].sound) {
            slot = i;
            break;
        }
    }
    if (slot >= 0) {
        audio->voices[slot].sound = &audio->sounds[id];
        audio->voices[slot].position = 0;
        audio->voices[slot].volume = volume;
        audio->voices[slot].looping = false;
    }
    SDL_UnlockAudioDevice(audio->device_id);
    return slot;
}

int audio_play_loop(AudioSystem *audio, SoundID id, float volume) {
    if (!audio->initialized) return -1;
    if (id < 0 || id >= SND_COUNT) return -1;
    if (!audio->sounds[id].samples) return -1;

    SDL_LockAudioDevice(audio->device_id);
    int slot = -1;
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (!audio->voices[i].sound) {
            slot = i;
            break;
        }
    }
    if (slot >= 0) {
        audio->voices[slot].sound = &audio->sounds[id];
        audio->voices[slot].position = 0;
        audio->voices[slot].volume = volume;
        audio->voices[slot].looping = true;
    }
    SDL_UnlockAudioDevice(audio->device_id);
    return slot;
}

void audio_stop(AudioSystem *audio, int voice) {
    if (!audio->initialized) return;
    if (voice < 0 || voice >= AUDIO_MAX_VOICES) return;

    SDL_LockAudioDevice(audio->device_id);
    audio->voices[voice].sound = NULL;
    SDL_UnlockAudioDevice(audio->device_id);
}

int audio_load_wav(AudioSystem *audio, SoundID id, const char *filename) {
    if (id < 0 || id >= SND_COUNT) return -1;

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

    if (ret > 0)
        SDL_ConvertAudio(&cvt);

    /* Replace existing sound */
    if (audio->initialized)
        SDL_LockAudioDevice(audio->device_id);

    sound_free(&audio->sounds[id]);
    audio->sounds[id].samples = (int16_t *)cvt.buf;
    audio->sounds[id].length = cvt.len_cvt / (int)sizeof(int16_t);
    audio->sounds[id].sample_rate = AUDIO_SAMPLE_RATE;

    if (audio->initialized)
        SDL_UnlockAudioDevice(audio->device_id);

    return 0;
}
