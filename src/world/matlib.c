#include "matlib.h"
#include "kv.h"
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

#define DEFAULT_MAT_PREFIX "@" /* untextured defaults: "@health", "@exit" */
#define UNTEXTURED_SIZE    64

int matlib_entity(MatLib *lib, const EntityDef *def, const char *args) {
    char name[MATLIB_NAME_MAX];
    if (kv_string(args, "mat", name, sizeof(name))) {
        int idx = matlib_find(lib, name);
        if (idx == MATLIB_NONE && proctex_find(name) >= 0) {
            return matlib_texture(lib, name, def->default_color); /* mat=<texture> */
        }
        if (idx == MATLIB_NONE) {
            fprintf(stderr, "level: unknown material '%s', using 0\n", name);
            return 0;
        }
        return idx;
    }

    const char *tex = def->default_texture ? def->default_texture : "none";
    if (proctex_find(tex) >= 0) {
        return matlib_texture(lib, tex, def->default_color);
    }

    snprintf(name, sizeof(name), DEFAULT_MAT_PREFIX "%s", def->name);
    int idx = matlib_find(lib, name);
    return idx != MATLIB_NONE ? idx : matlib_add(lib, name, "none", def->default_color);
}

void matlib_texture_size(const MatLib *lib, int material, int *w, int *h) {
    *w = UNTEXTURED_SIZE;
    *h = UNTEXTURED_SIZE;
    if (material < 0 || material >= lib->count) {
        return;
    }

    int tex = lib->scene->materials[material].texture_id;
    for (int i = 0; i < PTEX_COUNT; i++) {
        if (tex >= 0 && lib->proc_scene_index[i] == tex) {
            *w = lib->proc[i].width;
            *h = lib->proc[i].height;
            return;
        }
    }
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
