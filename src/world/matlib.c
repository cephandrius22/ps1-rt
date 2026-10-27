#include "matlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void matlib_init(MatLib *lib, Scene *scene) {
    memset(lib, 0, sizeof(*lib));
    lib->scene = scene;
    for (int i = 0; i < PTEX_COUNT; i++) {
        lib->proc_scene_index[i] = -1;
    }
}

/* Scene texture index for a procedural texture name, generating it on
 * first use; -1 for "none" or unknown names */
static int texture_index(MatLib *lib, const char *texture) {
    int ptex = proctex_find(texture);
    if (ptex < 0) {
        return -1;
    }
    if (lib->proc_scene_index[ptex] < 0) {
        proctex_generate(&lib->proc[ptex], (ProceduralTextureID)ptex);
        lib->proc_scene_index[ptex] = lib->texture_count++;
    }
    return lib->proc_scene_index[ptex];
}

int matlib_find(const MatLib *lib, const char *name) {
    for (int i = 0; i < lib->count; i++) {
        if (strcmp(lib->names[i], name) == 0) {
            return i;
        }
    }
    return MATLIB_NONE;
}

int matlib_add(MatLib *lib, const char *name, const char *texture, Vec3 color) {
    Material mat = {.color = color, .texture_id = texture_index(lib, texture)};

    int idx = matlib_find(lib, name);
    if (idx != MATLIB_NONE) {
        lib->scene->materials[idx] = mat;
        return idx;
    }
    if (lib->count >= MATLIB_MAX) {
        fprintf(stderr, "level: too many materials (max %d)\n", MATLIB_MAX);
        return MATLIB_NONE;
    }

    Material *grown = realloc(lib->scene->materials, (size_t)(lib->count + 1) * sizeof(Material));
    if (!grown) {
        return MATLIB_NONE;
    }
    lib->scene->materials = grown;

    idx = lib->count++;
    snprintf(lib->names[idx], MATLIB_NAME_MAX, "%s", name);
    lib->scene->materials[idx] = mat;
    lib->scene->material_count = lib->count;
    return idx;
}

int matlib_texture(MatLib *lib, const char *texture, Vec3 color) {
    int idx = matlib_find(lib, texture);
    if (idx != MATLIB_NONE) {
        return idx;
    }
    return matlib_add(lib, texture, texture, color);
}

void matlib_finish(MatLib *lib) {
    if (lib->texture_count == 0) {
        return;
    }

    Scene *s = lib->scene;
    s->textures = malloc((size_t)lib->texture_count * sizeof(Texture));
    if (!s->textures) {
        return;
    }
    s->texture_count = lib->texture_count;
    for (int i = 0; i < PTEX_COUNT; i++) {
        if (lib->proc_scene_index[i] >= 0) {
            s->textures[lib->proc_scene_index[i]] = lib->proc[i];
        }
    }
}
