/*
 * settings.h — Player preferences, kept between runs in a small text file.
 *
 *   # rt.cfg (next to the game; written by the options menu)
 *   sensitivity 1
 *   volume 0.8
 *   fov 69
 *
 * Missing or bad lines keep their defaults; values are clamped to their
 * ranges, which the options menu also uses.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

#define SETTINGS_FILE "rt.cfg"

#define SENSITIVITY_MIN 0.2f   /* mouse speed multiplier */
#define SENSITIVITY_MAX 3.0f
#define VOLUME_MIN      0.0f
#define VOLUME_MAX      1.0f
#define FOV_MIN         50.0f  /* vertical field of view, degrees */
#define FOV_MAX         100.0f

typedef struct {
    float sensitivity;  /* multiplies the base mouse speed */
    float volume;       /* master volume, 0..1 */
    float fov;          /* vertical field of view, degrees */
} Settings;

/* The values a first run starts with */
Settings settings_default(void);

/* Read path over the defaults. False if the file can't be read (the
 * defaults stand, which is normal on a first run). */
bool settings_load(Settings *s, const char *path);

/* Write all settings to path. False on failure. */
bool settings_save(const Settings *s, const char *path);

/* Keep every value inside its range */
void settings_clamp(Settings *s);

#endif
