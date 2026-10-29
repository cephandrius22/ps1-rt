#include "test_framework.h"
#include "audio.h"
#include <stdlib.h>
#include <string.h>

/* --- Sound generation tests (no SDL required) --- */

TEST(test_generate_sounds_all_valid) {
    Sound sounds[SND_COUNT];
    memset(sounds, 0, sizeof(sounds));
    audio_generate_sounds(sounds);

    for (int i = 0; i < SND_COUNT; i++) {
        ASSERT(sounds[i].samples != NULL);
        ASSERT(sounds[i].length > 0);
        ASSERT(sounds[i].sample_rate == AUDIO_SAMPLE_RATE);
    }

    for (int i = 0; i < SND_COUNT; i++) {
        sound_free(&sounds[i]);
    }
}

TEST(test_sound_free) {
    Sound sounds[SND_COUNT];
    memset(sounds, 0, sizeof(sounds));
    audio_generate_sounds(sounds);

    sound_free(&sounds[0]);
    ASSERT(sounds[0].samples == NULL);
    ASSERT(sounds[0].length == 0);

    /* Double-free should be safe */
    sound_free(&sounds[0]);
    ASSERT(sounds[0].samples == NULL);

    for (int i = 1; i < SND_COUNT; i++) {
        sound_free(&sounds[i]);
    }
}

TEST(test_weapon_fire_is_short) {
    Sound sounds[SND_COUNT];
    memset(sounds, 0, sizeof(sounds));
    audio_generate_sounds(sounds);

    /* A shot with its tail: about a third of a second, never a drone */
    float duration = (float)sounds[SND_WEAPON_FIRE].length / sounds[SND_WEAPON_FIRE].sample_rate;
    ASSERT(duration > 0.1f);
    ASSERT(duration < 0.5f);

    for (int i = 0; i < SND_COUNT; i++) {
        sound_free(&sounds[i]);
    }
}

TEST(test_ambient_is_long) {
    Sound sounds[SND_COUNT];
    memset(sounds, 0, sizeof(sounds));
    audio_generate_sounds(sounds);

    /* Ambient should be ~3.0s */
    float duration = (float)sounds[SND_AMBIENT].length / sounds[SND_AMBIENT].sample_rate;
    ASSERT(duration > 2.0f);
    ASSERT(duration < 5.0f);

    for (int i = 0; i < SND_COUNT; i++) {
        sound_free(&sounds[i]);
    }
}

TEST(test_sounds_have_nonzero_samples) {
    Sound sounds[SND_COUNT];
    memset(sounds, 0, sizeof(sounds));
    audio_generate_sounds(sounds);

    /* Each generated sound should have at least some non-zero samples */
    for (int s = 0; s < SND_COUNT; s++) {
        int nonzero = 0;
        for (int i = 0; i < sounds[s].length; i++) {
            if (sounds[s].samples[i] != 0) {
                nonzero++;
            }
        }
        ASSERT(nonzero > sounds[s].length / 10);
    }

    for (int i = 0; i < SND_COUNT; i++) {
        sound_free(&sounds[i]);
    }
}

TEST(test_sound_id_count) {
    ASSERT(SND_COUNT == 32);
    ASSERT(SND_WEAPON_FIRE == 0);
    ASSERT(SND_AMBIENT_BELL == SND_COUNT - 1);
}

/* Largest jump between neighboring samples inside the sound */
static int max_step(const Sound *s) {
    int big = 0;
    for (int i = 1; i < s->length; i++) {
        int d = abs(s->samples[i] - s->samples[i - 1]);
        big = d > big ? d : big;
    }
    return big;
}

/* Church loops join without a click: the step from the last sample back
 * to the first is no bigger than steps inside the loop. They are the
 * same every run (seeded noise). */
TEST(test_church_loops_seamless) {
    static Sound a[SND_COUNT], b[SND_COUNT];
    audio_generate_sounds(a);
    audio_generate_sounds(b);

    SoundID loops[] = {SND_AMBIENT_WIND, SND_AMBIENT_ORGAN, SND_AMBIENT_BELL};
    for (size_t k = 0; k < sizeof(loops) / sizeof(loops[0]); k++) {
        const Sound *s = &a[loops[k]];
        float seconds = (float)s->length / s->sample_rate;
        ASSERT(seconds >= 5.0f && seconds <= 15.0f);
        ASSERT(abs(s->samples[s->length - 1] - s->samples[0]) <= max_step(s));
        ASSERT(memcmp(s->samples, b[loops[k]].samples, sizeof(int16_t) * (size_t)s->length) == 0);
    }

    for (int i = 0; i < SND_COUNT; i++) {
        sound_free(&a[i]);
        sound_free(&b[i]);
    }
}

/* The bell tolls twice per loop: two loud onsets, quiet between */
TEST(test_church_bell_tolls) {
    static Sound sounds[SND_COUNT];
    audio_generate_sounds(sounds);
    const Sound *s = &sounds[SND_AMBIENT_BELL];

    int window = s->sample_rate / 4;
    int onsets = 0;
    int prev = 0;
    for (int w = 0; w + window <= s->length; w += window) {
        int peak = 0;
        for (int i = w; i < w + window; i++) {
            peak = abs(s->samples[i]) > peak ? abs(s->samples[i]) : peak;
        }
        onsets += peak > 2 * prev + 1000;
        prev = peak;
    }
    ASSERT_EQ_INT(onsets, 2);

    for (int i = 0; i < SND_COUNT; i++) {
        sound_free(&sounds[i]);
    }
}

/* --- AudioSystem struct tests (no SDL init) --- */

TEST(test_audio_play_without_init) {
    AudioSystem audio;
    memset(&audio, 0, sizeof(audio));
    audio.initialized = false;

    /* Should be a no-op, returning -1 */
    ASSERT(audio_play(&audio, SND_WEAPON_FIRE, 1.0f) == -1);
    ASSERT(audio_play_loop(&audio, SND_AMBIENT, 0.5f) == -1);
}

TEST(test_audio_play_invalid_id) {
    AudioSystem audio;
    memset(&audio, 0, sizeof(audio));
    audio.initialized = false;

    ASSERT(audio_play(&audio, -1, 1.0f) == -1);
    ASSERT(audio_play(&audio, SND_COUNT, 1.0f) == -1);
}

TEST(test_audio_stop_without_init) {
    AudioSystem audio;
    memset(&audio, 0, sizeof(audio));
    audio.initialized = false;

    /* Should be a no-op, not crash */
    audio_stop(&audio, 0);
    audio_stop(&audio, -1);
    audio_stop(&audio, AUDIO_MAX_VOICES);
}

/* Mixer, without a device: a constant tone in voice 0 */
static AudioSystem mixer;   /* large (echo buffers): keep off the stack */
static int16_t tone_samples[256];
static int16_t mixed[512 * AUDIO_CHANNELS];

static void start_tone(float pan, float reverb) {
    memset(&mixer, 0, sizeof(mixer));
    for (int i = 0; i < 256; i++) {
        tone_samples[i] = 10000;
    }
    static Sound tone = {tone_samples, 256, AUDIO_SAMPLE_RATE};
    mixer.voices[0] = (Voice){.sound = &tone, .volume = 1.0f, .pan = pan};
    mixer.reverb.amount = reverb;
    mixer.master = 1.0f;
}

static int channel(int frame, int ch) {
    return mixed[frame * AUDIO_CHANNELS + ch];
}

TEST(test_mix_pans) {
    start_tone(0.0f, 0);
    audio_mix(&mixer, mixed, 64);
    ASSERT(abs(channel(10, 0) - channel(10, 1)) <= 1);   /* centered: equal */

    start_tone(1.0f, 0);
    audio_mix(&mixer, mixed, 64);
    ASSERT(channel(10, 1) > 9000 && abs(channel(10, 0)) <= 1);  /* hard right */
}

/* The tone stops after 256 samples: dry, silence follows; with reverb, a tail */
TEST(test_mix_reverb_tail) {
    start_tone(0.0f, 0);
    audio_mix(&mixer, mixed, 512);
    ASSERT_EQ_INT(channel(500, 0), 0);
    ASSERT(mixer.voices[0].sound == NULL);   /* finished */

    start_tone(0.0f, 1.0f);
    for (int block = 0; block < 4; block++) {  /* past the first echo (~70 ms) */
        audio_mix(&mixer, mixed, 512);
    }
    int tail = 0;
    for (int f = 0; f < 512; f++) {
        tail |= channel(f, 0) | channel(f, 1);
    }
    ASSERT(tail != 0);
}

/* Master volume scales everything after mixing */
TEST(test_mix_master_volume) {
    start_tone(0.0f, 0);
    mixer.master = 0.5f;
    audio_mix(&mixer, mixed, 16);
    int half = channel(10, 0);
    start_tone(0.0f, 0);
    audio_mix(&mixer, mixed, 16);
    ASSERT(abs(channel(10, 0) - 2 * half) <= 2);
}

int main(void) {
    TEST_SUITE("audio");
    RUN_TEST(test_generate_sounds_all_valid);
    RUN_TEST(test_sound_free);
    RUN_TEST(test_weapon_fire_is_short);
    RUN_TEST(test_ambient_is_long);
    RUN_TEST(test_sounds_have_nonzero_samples);
    RUN_TEST(test_sound_id_count);
    RUN_TEST(test_church_loops_seamless);
    RUN_TEST(test_church_bell_tolls);
    RUN_TEST(test_audio_play_without_init);
    RUN_TEST(test_audio_play_invalid_id);
    RUN_TEST(test_audio_stop_without_init);
    RUN_TEST(test_mix_pans);
    RUN_TEST(test_mix_reverb_tail);
    RUN_TEST(test_mix_master_volume);
    TEST_REPORT();
}
