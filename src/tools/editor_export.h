/*
 * editor_export.h — Generate TrenchBroom files from the engine's own data.
 *
 * Run as `./rt --export-editor <dir>` (or `make editor`). Writes:
 *
 *   <dir>/game/textures/rt/<name>.tga   every procedural texture
 *   <dir>/trenchbroom/RT/rt.fgd         entity definitions
 *
 * Both are derived from code (proctex generators, EntityDef table), so
 * the editor can never offer a texture or key the engine doesn't know.
 * Like main.c, this tool sits above the layers and may use any of them.
 */
#ifndef EDITOR_EXPORT_H
#define EDITOR_EXPORT_H

#include <stdio.h>

/* Write the FGD text to out. Returns 0 on success. */
int editor_write_fgd(FILE *out);

/* Write all files under dir (subfolders must exist). Returns 0 on success. */
int editor_export(const char *dir);

#endif
