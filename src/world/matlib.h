/*
 * matlib.h — Named material registry used while loading a level.
 *
 * Both level loaders (.level text, .map from TrenchBroom) resolve names
 * through this, so a material means the same thing in either format:
 *
 *   "material rust_wall rust 0.4 0.2 0.1"  ──matlib_add()──┐
 *   .map face texture "brick"           ──matlib_texture()─┼─> scene->materials[i]
 *   ghoul with no mat=                  ──matlib_texture()─┘     (+ generated textures)
 *
 * A texture name resolves to, in order:
 *
 *   "brick"             a procedural texture (generated on first use)
 *   "crate"             textures/crate.tga, if that file exists
 *   "art/wall.tga"      that TGA file (paths relative to the game's folder)
 *
 * Textures are handed to the scene by matlib_finish().
 */
#ifndef MATLIB_H
#define MATLIB_H

#include "scene.h"
#include "proctex.h"

#define MATLIB_MAX       64
#define MATLIB_NAME_MAX  32
#define MATLIB_NONE      (-1) /* lookup failed */
#define MATLIB_FILES_MAX 32   /* TGA files per level */
#define MATLIB_PATH_MAX  256
#define MATLIB_TEXTURE_DIR "textures" /* where bare names look for <name>.tga */

typedef struct MatLib {
    char names[MATLIB_MAX][MATLIB_NAME_MAX];
    int count;

    Texture proc[PTEX_COUNT];
    int proc_scene_index[PTEX_COUNT]; /* -1 = not generated yet */

    Texture files[MATLIB_FILES_MAX];  /* TGAs loaded so far */
    char file_paths[MATLIB_FILES_MAX][MATLIB_PATH_MAX];
    int file_scene_index[MATLIB_FILES_MAX];
    int file_count;

    int texture_count;                /* scene textures handed out */

    Scene *scene;
} MatLib;

/* Start a registry that writes materials into scene (no textures yet) */
void matlib_init(MatLib *lib, Scene *scene);

/* Define a material. texture is a texture name (see above) or "none".
 * Redefining a name updates it. Returns its index, or MATLIB_NONE if full. */
int matlib_add(MatLib *lib, const char *name, const char *texture, Vec3 color);

/* Index of a material by name, or MATLIB_NONE */
int matlib_find(const MatLib *lib, const char *name);

/* Override a material's motion from scroll_u=, scroll_v=, glow=, pulse=
 * keys (unset keys keep the texture's default motion) */
void matlib_motion(MatLib *lib, int material, const char *args);

/* Material named after a texture, created on first use with the given
 * color. Lets .map faces and entity defaults name textures directly. */
int matlib_texture(MatLib *lib, const char *texture, Vec3 color);

/* Material for an entity: mat=<name> from args if given (a declared
 * material or a texture name), else the type's default (a material named
 * after its texture, or "@type" if untextured) */
int matlib_entity(MatLib *lib, const EntityDef *def, const char *args);

/* Pixel size of a material's texture; 64x64 if untextured */
void matlib_texture_size(const MatLib *lib, int material, int *w, int *h);

/* Move generated textures into the scene. Call once, after loading. */
void matlib_finish(MatLib *lib);

#endif
