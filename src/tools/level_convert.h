/*
 * level_convert.h — Turn a hand-written .level into a TrenchBroom .map.
 *
 * Run as `./rt --convert in.level out.map`. Line by line, so the level's
 * structure survives and the result is pleasant to edit:
 *
 *   room          ──> six wall slabs around the space (ROOM_WALL thick)
 *   box / brush   ──> one brush (brush: inside its brush entity)
 *   entity        ──> point entity; rot → angle, lengths → units
 *   light         ──> light; radius/cord → units, aim → map axes
 *   spawn         ──> info_player_start
 *   ambient, fog, sun, sky, reverb, ambience ──> worldspawn keys
 *   zone          ──> env_zone brush
 *   material      ──> faces use its texture (motion keys must match the
 *                     texture's defaults; flat-color materials can't
 *                     be expressed and draw untextured)
 *   quad, obj     ──> skipped with a warning
 *
 * Texture scale matches the .level look: one repeat per 2 m whatever the
 * texture's pixel size. Like main.c, this tool may use any layer.
 */
#ifndef LEVEL_CONVERT_H
#define LEVEL_CONVERT_H

/* Convert; warnings go to stderr. Returns 0 on success, -1 if a file
 * can't be read or written. */
int level_convert(const char *level_path, const char *map_path);

#endif
