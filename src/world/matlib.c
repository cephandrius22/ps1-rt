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

#define TGA_EXT ".tga"

static bool ends_with(const char *s, const char *tail) {
    size_t n = strlen(s), t = strlen(tail);
    return n >= t && strcmp(s + n - t, tail) == 0;
}

static bool file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
    }
    return f != NULL;
}

/* Where a non-procedural name's TGA lives: the name itself if it is a
 * .tga path, else textures/<name>.tga */
static void tga_path(const char *texture, char *out, size_t size) {
    if (ends_with(texture, TGA_EXT)) {
        snprintf(out, size, "%s", texture);
        return;
    }
    snprintf(out, size, MATLIB_TEXTURE_DIR "/%s" TGA_EXT, texture);
}

/* Scene texture index for a TGA, loading it on first use; -1 if missing */
static int file_index(MatLib *lib, const char *texture) {
    char path[MATLIB_PATH_MAX];
    tga_path(texture, path, sizeof(path));
    for (int i = 0; i < lib->file_count; i++) {
        if (strcmp(lib->file_paths[i], path) == 0) {
            return lib->file_scene_index[i];
        }
    }
    if (!file_exists(path)) {
        return -1;
    }
    if (lib->file_count >= MATLIB_FILES_MAX) {
        fprintf(stderr, "level: more than %d texture files; '%s' skipped\n", MATLIB_FILES_MAX, path);
        return -1;
    }

    int i = lib->file_count;
    if (texture_load_tga(&lib->files[i], path) != 0) {
        return -1;
    }
    snprintf(lib->file_paths[i], MATLIB_PATH_MAX, "%s", path);
    lib->file_scene_index[i] = lib->texture_count++;
    lib->file_count++;
    return lib->file_scene_index[i];
}

/* Scene texture index for a texture name (header: procedural, then
 * TGA file), generating or loading it on first use; -1 for "none" or
 * names that resolve to nothing */
static int texture_index(MatLib *lib, const char *texture) {
    if (strcmp(texture, "none") == 0) {
        return -1;
    }
    int ptex = proctex_find(texture);
    if (ptex < 0) {
        return file_index(lib, texture);
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

/* Motion that comes with a texture, so a .map face textured "slime"
 * flows and glows without any setup */
typedef struct {
    const char *texture;
    Vec2 scroll;
    float glow;
    float pulse;
} TextureMotion;

static const TextureMotion texture_motions[] = {
    {"slime",         {0.04f, 0.015f}, 0.35f, 0.4f},
    {"water",         {0.025f, 0.01f}, 0.0f,  0.0f},
    {"screen",        {0.0f, 0.0f},    0.6f,  6.0f},
    {"stained_glass", {0.0f, 0.0f},    0.5f,  0.0f}, /* lit from outside */
    {"rose_window",   {0.0f, 0.0f},    0.5f,  0.0f},
    {"candelabra",    {0.0f, 0.0f},    0.5f,  0.0f}, /* flames show in the dark */
};

static void apply_texture_motion(Material *mat, const char *texture) {
    for (size_t i = 0; i < sizeof(texture_motions) / sizeof(texture_motions[0]); i++) {
        const TextureMotion *m = &texture_motions[i];
        if (strcmp(m->texture, texture) == 0) {
            mat->scroll = m->scroll;
            mat->glow = m->glow;
            mat->pulse = m->pulse;
            return;
        }
    }
}

void matlib_motion(MatLib *lib, int material, const char *args) {
    if (material < 0 || material >= lib->count) {
        return;
    }
    Material *mat = &lib->scene->materials[material];
    mat->scroll.u = kv_float(args, "scroll_u", mat->scroll.u);
    mat->scroll.v = kv_float(args, "scroll_v", mat->scroll.v);
    mat->glow = kv_float(args, "glow", mat->glow);
    mat->pulse = kv_float(args, "pulse", mat->pulse);
}

int matlib_add(MatLib *lib, const char *name, const char *texture, Vec3 color) {
    Material mat = {.color = color, .texture_id = texture_index(lib, texture)};
    apply_texture_motion(&mat, texture);

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
    if (tex < 0) {
        return;
    }
    for (int i = 0; i < PTEX_COUNT; i++) {
        if (lib->proc_scene_index[i] == tex) {
            *w = lib->proc[i].width;
            *h = lib->proc[i].height;
            return;
        }
    }
    for (int i = 0; i < lib->file_count; i++) {
        if (lib->file_scene_index[i] == tex) {
            *w = lib->files[i].width;
            *h = lib->files[i].height;
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
    for (int i = 0; i < lib->file_count; i++) {
        s->textures[lib->file_scene_index[i]] = lib->files[i];
    }
}
