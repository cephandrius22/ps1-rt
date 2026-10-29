#include "editor_export.h"
#include "entity.h"
#include "mapfile.h"
#include "player.h"
#include "proctex.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define EXPORT_PATH_MAX   512
#define TEXTURE_SUBDIR    "game/textures/rt"
#define FGD_PATH          "trenchbroom/RT/rt.fgd"
#define POINT_HALF_SIZE   8    /* editor box for entities with no visible size */
#define COLOR_BYTE        255.0f

/* Editor box for a type in map units: sprite size, mesh bounds, or a
 * small cube. Map axes: x, y horizontal, z up from the origin (feet). */
typedef struct {
    float min[3];
    float max[3];
} EditorBox;

static EditorBox point_box(void) {
    float h = POINT_HALF_SIZE;
    return (EditorBox){{-h, -h, -h}, {h, h, h}};
}

static EditorBox sprite_box(const EntityDef *def, EntityType type) {
    Entity e = entity_spawn(type, vec3(0, 0, 0), "");
    Sprite sp = {.frame_count = 1};
    def->sprite(&e, &sp);

    float half = sp.width * 0.5f * MAP_UNITS_PER_METER;
    float height = sp.height * MAP_UNITS_PER_METER;
    return (EditorBox){{-half, -half, 0}, {half, half, height}};
}

/* Template mesh bounds, converted to map axes (engine x,y,z → map x,-z,y) */
static EditorBox mesh_box(const Mesh *m) {
    EditorBox box = {{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
    for (int i = 0; i < m->count; i++) {
        const Vec3 *v = &m->tris[i].v0;
        for (int k = 0; k < 3; k++) {
            float p[3] = {v[k].x, -v[k].z, v[k].y};
            for (int a = 0; a < 3; a++) {
                box.min[a] = fminf(box.min[a], p[a] * MAP_UNITS_PER_METER);
                box.max[a] = fmaxf(box.max[a], p[a] * MAP_UNITS_PER_METER);
            }
        }
    }
    return m->count > 0 ? box : point_box();
}

/* TrenchBroom draws a line from each target to what it names */
static const char *string_kind(const char *key) {
    if (strcmp(key, "target") == 0) {
        return "target_destination";
    }
    if (strcmp(key, "targetname") == 0) {
        return "target_source";
    }
    return "string";
}

static void write_key(FILE *out, const EntityKey *k) {
    switch (k->type) {
    case KEY_BOOL:
        fprintf(out, "\t%s(choices) : \"%s\" : %d = [ 0 : \"No\" 1 : \"Yes\" ]\n",
                k->name, k->desc, k->def != 0);
        break;
    case KEY_INT:
        fprintf(out, "\t%s(integer) : \"%s\" : %d\n", k->name, k->desc, (int)k->def);
        break;
    case KEY_LENGTH:
        fprintf(out, "\t%s(float) : \"%s (units)\" : \"%g\"\n",
                k->name, k->desc, k->def * MAP_UNITS_PER_METER);
        break;
    case KEY_FLOAT:
        fprintf(out, "\t%s(float) : \"%s\" : \"%g\"\n", k->name, k->desc, k->def);
        break;
    case KEY_STRING:
        fprintf(out, "\t%s(%s) : \"%s\" : \"%s\"\n", k->name, string_kind(k->name), k->desc,
                k->def_string ? k->def_string : "");
        break;
    case KEY_END:
        break;
    }
}

static void write_keys(FILE *out, const EntityDef *def) {
    for (const EntityKey *k = def->keys; k && k->type != KEY_END; k++) {
        write_key(out, k);
    }
    fputs("]\n\n", out);
}

/* Brush entities: drawn as brushes, textured per face (no mat key) */
static void write_solid_class(FILE *out, const EntityDef *def) {
    Vec3 c = def->default_color;
    fprintf(out, "@SolidClass color(%d %d %d) = %s : \"%s\"\n[\n",
            (int)(c.x * COLOR_BYTE), (int)(c.y * COLOR_BYTE), (int)(c.z * COLOR_BYTE),
            def->name, def->desc ? def->desc : def->name);
    fputs("\tangle(integer) : \"Direction, degrees (opens=side)\" : 0\n", out);
    write_keys(out, def);
}

/* Built-in classes the map loader handles itself */
static void write_builtin_classes(FILE *out) {
    fputs("@SolidClass = worldspawn : \"World\"\n[\n"
          "\tambient(float) : \"Light everywhere\" : \"0.04\"\n"
          "\tfog(string) : \"Fog: start end (units) r g b\" : \"256 704 0.04 0.03 0.02\"\n"
          "\tsun(string) : \"Sun: direction (map axes) r g b intensity; 0 = none\" : \"0.5 0.3 1 1 0.95 0.7 0.15\"\n"
          "\tsky(string) : \"Sky: top r g b, horizon r g b\" : \"0.02 0.01 0.04 0.08 0.06 0.03\"\n]\n"
          "@SolidClass = func_group : \"Group (static geometry)\" []\n"
          "@SolidClass = func_detail : \"Detail (static geometry)\" []\n\n", out);

    fprintf(out, "@PointClass size(-16 -16 0, 16 16 %d) color(0 255 0) = info_player_start : "
                 "\"Player spawn (origin = feet)\"\n[\n"
                 "\tangle(integer) : \"Facing, degrees (0 = east, 90 = north)\" : 0\n]\n\n",
            (int)(PLAYER_HEIGHT * MAP_UNITS_PER_METER));

    fprintf(out, "@PointClass size(-8 -8 -8, 8 8 8) color(255 255 128) = light : \"Point light\"\n[\n"
                 "\tintensity(float) : \"Brightness\" : \"2\"\n"
                 "\tradius(float) : \"Reach (units)\" : \"320\"\n"
                 "\tcolor(color1) : \"Color\" : \"1 1 1\"\n"
                 "\tshadows(choices) : \"Casts shadows\" : 0 = [ 0 : \"No\" 1 : \"Yes\" ]\n"
                 "\tflicker_speed(float) : \"Flicker speed (0 = steady)\" : \"0\"\n"
                 "\tflicker_amount(float) : \"Flicker depth, 0-1\" : \"0\"\n"
                 "\tcord(float) : \"Cord length for swinging (units)\" : \"0\"\n"
                 "\tswing_speed(float) : \"Swing speed (0 = still)\" : \"0\"\n"
                 "\tswing_angle(float) : \"Max swing, radians\" : \"0\"\n"
                 "\ttargetname(target_source) : \"Name; triggers targeting it toggle it\"\n"
                 "\toff(choices) : \"Starts switched off\" : 0 = [ 0 : \"No\" 1 : \"Yes\" ]\n"
                 "\tcone(float) : \"Spot half-angle, degrees (0 = all around)\" : \"0\"\n"
                 "\taim(string) : \"Spot direction, map axes\" : \"0 0 -1\"\n]\n\n");
}

int editor_write_fgd(FILE *out) {
    fputs("// Generated by `./rt --export-editor` — do not edit.\n"
          "// Source of truth: src/world/entity_types.c (EntityDef table).\n\n", out);
    write_builtin_classes(out);

    EntityMeshes meshes;
    entity_meshes_init(&meshes);

    for (int t = ENTITY_NONE + 1; t < ENTITY_TYPE_COUNT; t++) {
        const EntityDef *def = entity_def((EntityType)t);
        if (def->runtime_only) {
            continue;
        }
        Vec3 c = def->default_color;
        if (def->brush) {
            write_solid_class(out, def);
            continue;
        }

        EditorBox box = point_box();
        if (def->render == RENDER_SPRITE && def->sprite) {
            box = sprite_box(def, (EntityType)t);
        } else if (def->render == RENDER_MESH && meshes.initialized[t]) {
            box = mesh_box(&meshes.meshes[t]);
        }

        fprintf(out, "@PointClass size(%g %g %g, %g %g %g) color(%d %d %d) = %s : \"%s\"\n[\n",
                box.min[0], box.min[1], box.min[2], box.max[0], box.max[1], box.max[2],
                (int)(c.x * COLOR_BYTE), (int)(c.y * COLOR_BYTE), (int)(c.z * COLOR_BYTE),
                def->name, def->desc ? def->desc : def->name);
        fputs("\tangle(integer) : \"Facing, degrees\" : 0\n"
              "\tmat(string) : \"Material name (empty = type default)\"\n", out);
        write_keys(out, def);
    }

    entity_meshes_free(&meshes);
    return ferror(out) ? -1 : 0;
}

#define TOOL_TEX_SIZE   32
#define TOOL_TEX_STRIPE 4

typedef struct {
    const char *name;   /* the map loader drops faces with these names */
    uint8_t rgb[3];
} ToolTexture;

static const ToolTexture tool_textures[] = {
    {"skip", {220, 200, 40}},  /* hidden faces: no geometry */
    {"clip", {150, 60, 200}},  /* reserved for player clips (no geometry yet) */
    {"trigger", {230, 120, 30}}, /* trigger brushes: invisible in game */
};

/* Diagonal stripes on black, so tool faces stand out in the editor */
static int export_tool_texture(const char *dir, const ToolTexture *tool) {
    uint8_t pixels[TOOL_TEX_SIZE * TOOL_TEX_SIZE * 3];
    Texture tex = {.pixels = pixels, .width = TOOL_TEX_SIZE, .height = TOOL_TEX_SIZE};
    for (int y = 0; y < TOOL_TEX_SIZE; y++) {
        for (int x = 0; x < TOOL_TEX_SIZE; x++) {
            bool stripe = ((x + y) / TOOL_TEX_STRIPE) % 2 == 0;
            uint8_t *p = pixels + (y * TOOL_TEX_SIZE + x) * 3;
            for (int c = 0; c < 3; c++) {
                p[c] = stripe ? tool->rgb[c] : 0;
            }
        }
    }

    char path[EXPORT_PATH_MAX];
    snprintf(path, sizeof(path), "%s/" TEXTURE_SUBDIR "/%s.tga", dir, tool->name);
    return texture_save_tga(&tex, path);
}

static int export_textures(const char *dir) {
    for (size_t i = 0; i < sizeof(tool_textures) / sizeof(tool_textures[0]); i++) {
        if (export_tool_texture(dir, &tool_textures[i]) != 0) {
            return -1;
        }
    }

    for (int i = 0; i < PTEX_COUNT; i++) {
        char path[EXPORT_PATH_MAX];
        snprintf(path, sizeof(path), "%s/" TEXTURE_SUBDIR "/%s.tga", dir,
                 proctex_name((ProceduralTextureID)i));

        Texture tex;
        proctex_generate(&tex, (ProceduralTextureID)i);
        int err = texture_save_tga(&tex, path);
        texture_free(&tex);
        if (err) {
            return -1;
        }
    }
    return 0;
}

int editor_export(const char *dir) {
    if (export_textures(dir) != 0) {
        return -1;
    }

    char path[EXPORT_PATH_MAX];
    snprintf(path, sizeof(path), "%s/" FGD_PATH, dir);
    FILE *out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "editor: cannot write %s\n", path);
        return -1;
    }
    int err = editor_write_fgd(out);
    fclose(out);

    printf("Wrote %d textures and %s\n", PTEX_COUNT, path);
    return err;
}
