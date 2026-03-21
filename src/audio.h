/*
 * audio.h — Sound system with procedural generation and WAV loading.
 *
 * Sounds are identified by SoundID. On init, procedural sounds are
 * generated for each ID. Call audio_load_wav() to replace any
 * procedural sound with a WAV file. Playback mixes into SDL2's
 * audio queue.
 */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SND_WEAPON_FIRE = 0,
    SND_WEAPON_IMPACT,
    SND_FOOTSTEP,
    SND_PICKUP,
    SND_DOOR_OPEN,
    SND_DOOR_CLOSE,
    SND_AMBIENT,
    SND_COUNT
} SoundID;

typedef struct {
    int16_t *samples;   /* mono 16-bit PCM */
    int length;         /* number of samples */
    int sample_rate;
} Sound;

#define AUDIO_SAMPLE_RATE 22050
#define AUDIO_MAX_VOICES  8

typedef struct {
    const Sound *sound;
    int position;       /* current playback position in samples */
    float volume;
    bool looping;
} Voice;

typedef struct {
    Sound sounds[SND_COUNT];
    Voice voices[AUDIO_MAX_VOICES];
    uint32_t device_id;
    bool initialized;
} AudioSystem;

/* Initialize audio system and generate procedural sounds.
 * Returns 0 on success. Safe to call game logic even if this fails
 * (audio_play becomes a no-op). */
int  audio_init(AudioSystem *audio);
void audio_shutdown(AudioSystem *audio);

/* Play a sound. Returns voice index or -1 if no voice available. */
int  audio_play(AudioSystem *audio, SoundID id, float volume);

/* Play a sound looping (for ambient). */
int  audio_play_loop(AudioSystem *audio, SoundID id, float volume);

/* Stop a specific voice. */
void audio_stop(AudioSystem *audio, int voice);

/* Replace a procedural sound with a WAV file. Returns 0 on success. */
int  audio_load_wav(AudioSystem *audio, SoundID id, const char *filename);

/* Generate procedural sounds (called by audio_init, exposed for testing). */
void audio_generate_sounds(Sound sounds[SND_COUNT]);

/* Free a single sound's sample buffer. */
void sound_free(Sound *s);

#endif
