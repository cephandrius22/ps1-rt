#include "test_framework.h"
#include "../src/audio.h"
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

    for (int i = 0; i < SND_COUNT; i++)
        sound_free(&sounds[i]);
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

    for (int i = 1; i < SND_COUNT; i++)
        sound_free(&sounds[i]);
}

TEST(test_weapon_fire_is_short) {
    Sound sounds[SND_COUNT];
    memset(sounds, 0, sizeof(sounds));
    audio_generate_sounds(sounds);

    /* Weapon fire should be ~0.15s = 3307 samples at 22050 Hz */
    float duration = (float)sounds[SND_WEAPON_FIRE].length / sounds[SND_WEAPON_FIRE].sample_rate;
    ASSERT(duration > 0.1f);
    ASSERT(duration < 0.3f);

    for (int i = 0; i < SND_COUNT; i++)
        sound_free(&sounds[i]);
}

TEST(test_ambient_is_long) {
    Sound sounds[SND_COUNT];
    memset(sounds, 0, sizeof(sounds));
    audio_generate_sounds(sounds);

    /* Ambient should be ~3.0s */
    float duration = (float)sounds[SND_AMBIENT].length / sounds[SND_AMBIENT].sample_rate;
    ASSERT(duration > 2.0f);
    ASSERT(duration < 5.0f);

    for (int i = 0; i < SND_COUNT; i++)
        sound_free(&sounds[i]);
}

TEST(test_sounds_have_nonzero_samples) {
    Sound sounds[SND_COUNT];
    memset(sounds, 0, sizeof(sounds));
    audio_generate_sounds(sounds);

    /* Each generated sound should have at least some non-zero samples */
    for (int s = 0; s < SND_COUNT; s++) {
        int nonzero = 0;
        for (int i = 0; i < sounds[s].length; i++) {
            if (sounds[s].samples[i] != 0)
                nonzero++;
        }
        ASSERT(nonzero > sounds[s].length / 10);
    }

    for (int i = 0; i < SND_COUNT; i++)
        sound_free(&sounds[i]);
}

TEST(test_sound_id_count) {
    ASSERT(SND_COUNT == 7);
    ASSERT(SND_WEAPON_FIRE == 0);
    ASSERT(SND_AMBIENT == SND_COUNT - 1);
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

int main(void) {
    TEST_SUITE("audio");
    RUN_TEST(test_generate_sounds_all_valid);
    RUN_TEST(test_sound_free);
    RUN_TEST(test_weapon_fire_is_short);
    RUN_TEST(test_ambient_is_long);
    RUN_TEST(test_sounds_have_nonzero_samples);
    RUN_TEST(test_sound_id_count);
    RUN_TEST(test_audio_play_without_init);
    RUN_TEST(test_audio_play_invalid_id);
    RUN_TEST(test_audio_stop_without_init);
    TEST_REPORT();
}
