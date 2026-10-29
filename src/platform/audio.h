/*
 * audio.h — Sound system with procedural generation and WAV loading.
 *
 * Sounds are identified by SoundID. On init, procedural sounds are
 * generated for each ID. Call audio_load_wav() to replace any
 * procedural sound with a WAV file. Up to AUDIO_MAX_VOICES voices are
 * mixed in SDL2's audio callback thread, in stereo:
 *
 *   voice (mono) ──volume, pan──> L/R ──┬──────────────────> out
 *                                       └─> echo (reverb) ─┘
 *
 * Reverb is two cross-fed delay lines (a cheap, wide tail); its amount
 * comes from the zone the listener stands in.
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
    SND_DRY_FIRE,       /* empty weapon click */
    SND_PLAYER_PAIN,
    SND_PLAYER_DEATH,
    SND_ENEMY_ALERT,    /* growl on spotting the player */
    SND_ENEMY_ATTACK,
    SND_ENEMY_PAIN,
    SND_ENEMY_DEATH,
    SND_SHOTGUN_FIRE,
    SND_ROCKET_LAUNCH,
    SND_EXPLOSION,
    SND_SPITTER_ALERT,
    SND_SPITTER_ATTACK,
    SND_SPITTER_PAIN,
    SND_SPITTER_DEATH,
    SND_SPIT_SPLAT,
    SND_DOOR_LOCKED,    /* low buzz: door needs a keycard */
    SND_SWITCH,         /* button click */
    SND_GLASS,          /* a bulb shot out */
    SND_AMBIENT_DRIP,   /* loop: water dripping in a cave */
    SND_AMBIENT_HUM,    /* loop: machinery hum */
    SND_SECRET,         /* rising chime: a secret area found */
    SND_FLESH_HIT,      /* wet thud: a bullet into a body */
    SND_AMBIENT_WIND,   /* loop: wind whistling through high windows */
    SND_AMBIENT_ORGAN,  /* loop: distant organ, two chords swelling */
    SND_AMBIENT_BELL,   /* loop: a far bell tolling every 6 s */
    SND_COUNT
} SoundID;

typedef struct {
    int16_t *samples;   /* mono 16-bit PCM */
    int length;         /* number of samples */
    int sample_rate;
} Sound;

#define AUDIO_SAMPLE_RATE 22050
#define AUDIO_MAX_VOICES  8
#define AUDIO_CHANNELS    2     /* stereo, interleaved L R */
#define AUDIO_ECHO_MAX    2048  /* samples per delay line (~93 ms) */

typedef struct {
    const Sound *sound;
    int position;       /* current playback position in samples */
    float volume;
    float pan;          /* -1 left .. 0 center .. 1 right */
    bool looping;
} Voice;

typedef struct {
    float left[AUDIO_ECHO_MAX];
    float right[AUDIO_ECHO_MAX];
    int pos_left, pos_right;
    float amount;       /* 0 dry .. 1 cavernous */
} Reverb;

typedef struct {
    Sound sounds[SND_COUNT];
    Voice voices[AUDIO_MAX_VOICES];
    Reverb reverb;
    float master;       /* master volume 0..1 (settings) */
    uint32_t device_id;
    bool initialized;
} AudioSystem;

/* Initialize audio system and generate procedural sounds.
 * Returns 0 on success. Safe to call game logic even if this fails
 * (audio_play becomes a no-op). */
int  audio_init(AudioSystem *audio);
void audio_shutdown(AudioSystem *audio);

/* Play a sound, centered. Returns voice index or -1 if no voice available. */
int  audio_play(AudioSystem *audio, SoundID id, float volume);

/* Play a sound placed left/right: pan -1 (left) .. 1 (right) */
int  audio_play_at(AudioSystem *audio, SoundID id, float volume, float pan);

/* Play a sound looping (for ambient). */
int  audio_play_loop(AudioSystem *audio, SoundID id, float volume);

/* Change a playing voice's volume (ambient crossfades) */
void audio_set_volume(AudioSystem *audio, int voice, float volume);

/* Overall loudness, 0 silent .. 1 full (applied after mixing) */
void audio_set_master(AudioSystem *audio, float volume);

/* How much echo everything gets: 0 dry .. 1 cavernous */
void audio_set_reverb(AudioSystem *audio, float amount);

/* Mix all voices into frames of interleaved stereo. The audio callback's
 * work; exposed so tests can listen without a device. */
void audio_mix(AudioSystem *audio, int16_t *out, int frames);

/* Stop a specific voice. */
void audio_stop(AudioSystem *audio, int voice);

/* Replace a procedural sound with a WAV file. Returns 0 on success. */
int  audio_load_wav(AudioSystem *audio, SoundID id, const char *filename);

/* Generate procedural sounds (called by audio_init, exposed for testing). */
void audio_generate_sounds(Sound sounds[SND_COUNT]);

/* Free a single sound's sample buffer. */
void sound_free(Sound *s);

#endif
