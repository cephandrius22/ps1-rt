#include "level.h"
#include "proctex.h"
#include "ps1_effects.h"
#include "kv.h"
#include "matlib.h"
#include "mapfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define UV_TILE_SCALE 2.0f /* 1 texture repeat per this many world units */

typedef struct {
    MatLib mats;
    Mesh world_mesh;
    Scene *scene;
} LevelParser;

/* --- Material name lookup --- */

static int find_mat(LevelParser *p, const char *name) {
    int idx = matlib_find(&p->mats, name);
    if (idx != MATLIB_NONE) {
        return idx;
    }
    fprintf(stderr, "level: unknown material '%s', using 0\n", name);
    return 0;
}

/* --- Geometry helpers --- */

static Triangle make_tri(Vec3 v0, Vec3 v1, Vec3 v2, Vec2 uv0, Vec2 uv1, Vec2 uv2, int mat_id) {
    Vec3 normal = vec3_normalize(vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0)));
    return (Triangle){
        .v0 = v0, .v1 = v1, .v2 = v2,
        .uv0 = uv0, .uv1 = uv1, .uv2 = uv2,
        .normal = normal, .material_id = mat_id
    };
}

static void add_quad_uv(Mesh *m, Vec3 a, Vec3 b, Vec3 c, Vec3 d,
                         Vec2 uva, Vec2 uvb, Vec2 uvc, Vec2 uvd, int mat_id) {
    mesh_add_tri(m, make_tri(a, b, c, uva, uvb, uvc, mat_id));
    mesh_add_tri(m, make_tri(a, c, d, uva, uvc, uvd, mat_id));
}

static Vec2 uv(float u, float v) { return (Vec2){u, v}; }

/* Compute UV tiling based on world-space dimensions */
static float tile(float size) { return size / UV_TILE_SCALE; }

static void add_box_outer(Mesh *m, Vec3 lo, Vec3 hi, int mat_id) {
    float w = hi.x - lo.x, h = hi.y - lo.y, d = hi.z - lo.z;
    float tw = tile(w), th = tile(h), td = tile(d);

    /* Front (+Z) */
    add_quad_uv(m, vec3(lo.x, lo.y, hi.z), vec3(hi.x, lo.y, hi.z),
                   vec3(hi.x, hi.y, hi.z), vec3(lo.x, hi.y, hi.z),
                   uv(0,0), uv(tw,0), uv(tw,th), uv(0,th), mat_id);
    /* Back (-Z) */
    add_quad_uv(m, vec3(hi.x, lo.y, lo.z), vec3(lo.x, lo.y, lo.z),
                   vec3(lo.x, hi.y, lo.z), vec3(hi.x, hi.y, lo.z),
                   uv(0,0), uv(tw,0), uv(tw,th), uv(0,th), mat_id);
    /* Left (-X) */
    add_quad_uv(m, vec3(lo.x, lo.y, lo.z), vec3(lo.x, lo.y, hi.z),
                   vec3(lo.x, hi.y, hi.z), vec3(lo.x, hi.y, lo.z),
                   uv(0,0), uv(td,0), uv(td,th), uv(0,th), mat_id);
    /* Right (+X) */
    add_quad_uv(m, vec3(hi.x, lo.y, hi.z), vec3(hi.x, lo.y, lo.z),
                   vec3(hi.x, hi.y, lo.z), vec3(hi.x, hi.y, hi.z),
                   uv(0,0), uv(td,0), uv(td,th), uv(0,th), mat_id);
    /* Top (+Y) */
    add_quad_uv(m, vec3(lo.x, hi.y, hi.z), vec3(hi.x, hi.y, hi.z),
                   vec3(hi.x, hi.y, lo.z), vec3(lo.x, hi.y, lo.z),
                   uv(0,0), uv(tw,0), uv(tw,td), uv(0,td), mat_id);
    /* Bottom (-Y) */
    add_quad_uv(m, vec3(lo.x, lo.y, lo.z), vec3(hi.x, lo.y, lo.z),
                   vec3(hi.x, lo.y, hi.z), vec3(lo.x, lo.y, hi.z),
                   uv(0,0), uv(tw,0), uv(tw,td), uv(0,td), mat_id);
}

static void add_room_inner(Mesh *m, Vec3 lo, Vec3 hi,
                           int wall_mat, int floor_mat, int ceil_mat) {
    float w = hi.x - lo.x, h = hi.y - lo.y, d = hi.z - lo.z;
    float tw = tile(w), th = tile(h), td = tile(d);

    /* Walls: inner-facing (reversed winding from add_box_outer) */
    /* Front wall (at +Z, facing -Z) */
    add_quad_uv(m, vec3(hi.x, lo.y, hi.z), vec3(lo.x, lo.y, hi.z),
                   vec3(lo.x, hi.y, hi.z), vec3(hi.x, hi.y, hi.z),
                   uv(0,0), uv(tw,0), uv(tw,th), uv(0,th), wall_mat);
    /* Back wall (at -Z, facing +Z) */
    add_quad_uv(m, vec3(lo.x, lo.y, lo.z), vec3(hi.x, lo.y, lo.z),
                   vec3(hi.x, hi.y, lo.z), vec3(lo.x, hi.y, lo.z),
                   uv(0,0), uv(tw,0), uv(tw,th), uv(0,th), wall_mat);
    /* Left wall (at -X, facing +X) */
    add_quad_uv(m, vec3(lo.x, lo.y, hi.z), vec3(lo.x, lo.y, lo.z),
                   vec3(lo.x, hi.y, lo.z), vec3(lo.x, hi.y, hi.z),
                   uv(0,0), uv(td,0), uv(td,th), uv(0,th), wall_mat);
    /* Right wall (at +X, facing -X) */
    add_quad_uv(m, vec3(hi.x, lo.y, lo.z), vec3(hi.x, lo.y, hi.z),
                   vec3(hi.x, hi.y, hi.z), vec3(hi.x, hi.y, lo.z),
                   uv(0,0), uv(td,0), uv(td,th), uv(0,th), wall_mat);

    /* Floor (at lo.y, facing +Y) */
    if (floor_mat >= 0) {
        add_quad_uv(m, vec3(lo.x, lo.y, hi.z), vec3(hi.x, lo.y, hi.z),
                       vec3(hi.x, lo.y, lo.z), vec3(lo.x, lo.y, lo.z),
                       uv(0,0), uv(tw,0), uv(tw,td), uv(0,td), floor_mat);
    }

    /* Ceiling (at hi.y, facing -Y) */
    if (ceil_mat >= 0) {
        add_quad_uv(m, vec3(lo.x, hi.y, lo.z), vec3(hi.x, hi.y, lo.z),
                       vec3(hi.x, hi.y, hi.z), vec3(lo.x, hi.y, hi.z),
                       uv(0,0), uv(tw,0), uv(tw,td), uv(0,td), ceil_mat);
    }
}

/* --- Line parsers --- */

static void parse_spawn(const char *line, LevelInfo *info) {
    sscanf(line, "spawn %f %f %f %f",
           &info->spawn_pos.x, &info->spawn_pos.y, &info->spawn_pos.z,
           &info->spawn_yaw);
    info->has_spawn = true;
}

static void parse_material(const char *line, LevelParser *p) {
    char name[MATLIB_NAME_MAX], tex_name[MATLIB_NAME_MAX];
    float r = 0.5f, g = 0.5f, b = 0.5f;
    if (sscanf(line, "material %31s %31s %f %f %f", name, tex_name, &r, &g, &b) < 2) {
        return;
    }
    int idx = matlib_add(&p->mats, name, tex_name, vec3(r, g, b));
    matlib_motion(&p->mats, idx, line);
    bool untextured = idx != MATLIB_NONE && p->scene->materials[idx].texture_id < 0;
    if (untextured && strcmp(tex_name, "none") != 0) {
        fprintf(stderr, "level: unknown texture '%s' (not procedural, no "
                MATLIB_TEXTURE_DIR "/%s.tga); material '%s' is flat color\n",
                tex_name, tex_name, name);
    }
}

static void parse_room(const char *line, LevelParser *p) {
    float x1, y1, z1, x2, y2, z2;
    char wall_name[32], floor_name[32], ceil_name[32];

    if (sscanf(line, "room %f %f %f %f %f %f %31s %31s %31s",
               &x1, &y1, &z1, &x2, &y2, &z2,
               wall_name, floor_name, ceil_name) < 9) {
        return;
    }

    int wall_mat = find_mat(p, wall_name);
    int floor_mat = strcmp(floor_name, "none") == 0 ? -1 : find_mat(p, floor_name);
    int ceil_mat = strcmp(ceil_name, "none") == 0 ? -1 : find_mat(p, ceil_name);

    add_room_inner(&p->world_mesh, vec3(x1, y1, z1), vec3(x2, y2, z2),
                   wall_mat, floor_mat, ceil_mat);
}

static void parse_box(const char *line, LevelParser *p) {
    float x1, y1, z1, x2, y2, z2;
    char mat_name[32];

    if (sscanf(line, "box %f %f %f %f %f %f %31s",
               &x1, &y1, &z1, &x2, &y2, &z2, mat_name) < 7) {
        return;
    }

    int mat_id = find_mat(p, mat_name);
    add_box_outer(&p->world_mesh, vec3(x1, y1, z1), vec3(x2, y2, z2), mat_id);
}

static void parse_quad(const char *line, LevelParser *p) {
    float x1,y1,z1, x2,y2,z2, x3,y3,z3, x4,y4,z4;
    char mat_name[32];

    if (sscanf(line, "quad %f %f %f %f %f %f %f %f %f %f %f %f %31s",
               &x1,&y1,&z1, &x2,&y2,&z2, &x3,&y3,&z3, &x4,&y4,&z4, mat_name) < 13) {
        return;
    }

    int mat_id = find_mat(p, mat_name);
    add_quad_uv(&p->world_mesh,
                vec3(x1,y1,z1), vec3(x2,y2,z2), vec3(x3,y3,z3), vec3(x4,y4,z4),
                uv(0,0), uv(1,0), uv(1,1), uv(0,1), mat_id);
}

static void parse_obj(const char *line, LevelParser *p) {
    char filename[256], mat_name[32];
    float x, y, z, rot_y, scale;

    if (sscanf(line, "obj %255s %f %f %f %f %f %31s",
               filename, &x, &y, &z, &rot_y, &scale, mat_name) < 7) {
        return;
    }

    int mat_id = find_mat(p, mat_name);

    Mesh obj_mesh;
    mesh_init(&obj_mesh);
    if (mesh_load_obj(&obj_mesh, filename) != 0) {
        mesh_free(&obj_mesh);
        return;
    }

    /* Transform and add to world mesh */
    float rad = rot_y * (float)MATH_PI / 180.0f;
    float cs = cosf(rad), sn = sinf(rad);

    for (int i = 0; i < obj_mesh.count; i++) {
        Triangle *t = &obj_mesh.tris[i];
        Vec3 *verts[3] = {&t->v0, &t->v1, &t->v2};
        for (int v = 0; v < 3; v++) {
            Vec3 *p_vert = verts[v];
            /* Scale */
            p_vert->x *= scale; p_vert->y *= scale; p_vert->z *= scale;
            /* Rotate Y */
            float rx = p_vert->x * cs - p_vert->z * sn;
            float rz = p_vert->x * sn + p_vert->z * cs;
            p_vert->x = rx; p_vert->z = rz;
            /* Translate */
            p_vert->x += x; p_vert->y += y; p_vert->z += z;
        }
        /* Recompute normal after transform */
        t->normal = vec3_normalize(vec3_cross(
            vec3_sub(t->v1, t->v0), vec3_sub(t->v2, t->v0)));
        t->material_id = mat_id;
        mesh_add_tri(&p->world_mesh, *t);
    }

    mesh_free(&obj_mesh);
}

#define KEY_NAME_MAX 32

/* Warn about key=value pairs the type doesn't use (likely typos) */
static void warn_unknown_keys(const char *line, const EntityDef *def) {
    for (const char *eq = strchr(line, '='); eq; eq = strchr(eq + 1, '=')) {
        const char *start = eq;
        while (start > line && start[-1] != ' ' && start[-1] != '\t') {
            start--;
        }
        char key[KEY_NAME_MAX];
        int len = (int)(eq - start) < KEY_NAME_MAX - 1 ? (int)(eq - start) : KEY_NAME_MAX - 1;
        memcpy(key, start, (size_t)len);
        key[len] = '\0';

        if (!entity_key_known(def, key)) {
            fprintf(stderr, "level: %s has unknown key '%s'\n", def->name, key);
        }
    }
}

static void parse_entity(const char *line, LevelParser *p) {
    char type_str[32];
    float x, y, z;

    if (sscanf(line, "entity %31s %f %f %f", type_str, &x, &y, &z) < 4) {
        return;
    }

    EntityType type = entity_type_by_name(type_str);
    if (type == ENTITY_NONE || entity_def(type)->runtime_only) {
        fprintf(stderr, "level: unknown entity type '%s'\n", type_str);
        return;
    }

    const EntityDef *def = entity_def(type);
    warn_unknown_keys(line, def);

    Entity e = entity_spawn(type, vec3(x, y, z), line);
    e.material_id = matlib_entity(&p->mats, def, line);
    entity_add(&p->scene->entities, e);
}

/* brush <type> x1 y1 z1 x2 y2 z2 [mat] [key=value ...]: a box-shaped
 * brush entity, e.g. "brush func_door -1 0 -5 1 2.5 -4.8 rust". Invisible
 * types (triggers) need no material. */
static void parse_brush(const char *line, LevelParser *p) {
    char type_str[32];
    char mat_name[32] = "";
    float x1, y1, z1, x2, y2, z2;
    if (sscanf(line, "brush %31s %f %f %f %f %f %f %31s", type_str,
               &x1, &y1, &z1, &x2, &y2, &z2, mat_name) < 7) {
        return;
    }
    bool has_mat = mat_name[0] != '\0' && strchr(mat_name, '=') == NULL;

    EntityType type = entity_type_by_name(type_str);
    if (type == ENTITY_NONE || !entity_def(type)->brush) {
        fprintf(stderr, "level: '%s' is not a brush entity type\n", type_str);
        return;
    }
    warn_unknown_keys(line, entity_def(type));

    Vec3 lo = vec3(fminf(x1, x2), fminf(y1, y2), fminf(z1, z2));
    Vec3 hi = vec3(fmaxf(x1, x2), fmaxf(y1, y2), fmaxf(z1, z2));
    Mesh tris;
    mesh_init(&tris);
    if (has_mat) {
        add_box_outer(&tris, lo, hi, find_mat(p, mat_name));
    }
    level_add_brush_entity(p->scene, type, lo, hi, &tris, line);
}

static void parse_light(const char *line, Scene *scene) {
    float x, y, z, r, g, b, intensity, radius;
    if (sscanf(line, "light %f %f %f %f %f %f %f %f",
               &x, &y, &z, &r, &g, &b, &intensity, &radius) < 8) {
        return;
    }

    Vec3 pos = vec3(x, y, z);
    Light pl = light_from_keys(pos, vec3(r, g, b), intensity, radius, line);

    /* .level files may place the swing anchor explicitly */
    pl.anchor = vec3(kv_float(line, "anchor_x", pl.anchor.x),
                     kv_float(line, "anchor_y", pl.anchor.y),
                     kv_float(line, "anchor_z", pl.anchor.z));
    light_add(&scene->lights, pl);
}

/* ambient / fog / sun / sky <numbers>: the level's environment */
static void parse_environment(const char *line, Scene *scene) {
    char key[16];
    int skip = 0;
    if (sscanf(line, "%15s %n", key, &skip) < 1 ||
        !environment_set(&scene->env, key, line + skip)) {
        fprintf(stderr, "level: bad environment line: %.60s\n", line);
    }
}

/* zone x1 y1 z1 x2 y2 z2 [ambient=a] [fog="start end r g b"] */
static void parse_zone(const char *line, Scene *scene) {
    float x1, y1, z1, x2, y2, z2;
    if (sscanf(line, "zone %f %f %f %f %f %f", &x1, &y1, &z1, &x2, &y2, &z2) < 6) {
        fprintf(stderr, "level: bad zone line: %.60s\n", line);
        return;
    }
    Vec3 lo = vec3(fminf(x1, x2), fminf(y1, y2), fminf(z1, z2));
    Vec3 hi = vec3(fmaxf(x1, x2), fmaxf(y1, y2), fmaxf(z1, z2));

    EnvZone zone;
    if (!zone_from_keys(&zone, lo, hi, &scene->env, line)) {
        fprintf(stderr, "level: zone sets no ambient, fog, reverb or ambience: %.60s\n", line);
        return;
    }
    zone_add(&scene->zones, zone);
}

static bool is_environment(const char *line) {
    static const char *keys[] = {"ambient ", "fog ", "sun ", "sky ", "reverb ", "ambience "};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (strncmp(line, keys[i], strlen(keys[i])) == 0) {
            return true;
        }
    }
    return false;
}

/* --- Public API --- */

void level_add_brush_entity(Scene *scene, EntityType type, Vec3 lo, Vec3 hi,
                            Mesh *tris, const char *args) {
    Vec3 center = vec3_mul(vec3_add(lo, hi), 0.5f);
    Vec3 half = vec3_mul(vec3_sub(hi, lo), 0.5f);
    Entity e = entity_spawn_brush(type, center, half, args);

    /* Model triangles are stored relative to the entity, so posing it
     * (a door sliding) is a plain offset */
    if (entity_def(type)->render == RENDER_MESH && tris->count > 0) {
        for (int i = 0; i < tris->count; i++) {
            Triangle *t = &tris->tris[i];
            t->v0 = vec3_sub(t->v0, center);
            t->v1 = vec3_sub(t->v1, center);
            t->v2 = vec3_sub(t->v2, center);
        }
        e.model = entity_meshes_add_model(&scene->entity_meshes, tris);
    }
    mesh_free(tris);
    entity_add(&scene->entities, e);
}

void level_finish(Scene *scene, MatLib *mats, Mesh *world, const char *name) {
    /* Materials for types spawned during play (no level line to resolve) */
    for (int t = ENTITY_NONE + 1; t < ENTITY_TYPE_COUNT; t++) {
        const EntityDef *def = entity_def((EntityType)t);
        if (def->runtime_only) {
            scene->type_material[t] = matlib_entity(mats, def, "");
        }
    }
    matlib_finish(mats);

    /* Lights entities bring with them (candelabra flames) */
    for (int i = 0; i < scene->entities.count; i++) {
        const Entity *e = &scene->entities.items[i];
        const EntityDef *def = entity_def(e->type);
        Light light;
        if (e->active && def->light && def->light(e, &light)) {
            light_add(&scene->lights, light);
        }
    }

    /* PS1 vertex jitter on world geometry, before the BVH sees it */
    for (int i = 0; i < world->count; i++) {
        world->tris[i] = ps1_jitter_triangle(&world->tris[i]);
    }

    printf("Level '%s': %d triangles, %d materials, %d textures, %d entities, %d lights\n",
           name, world->count, scene->material_count, scene->texture_count,
           scene->entities.count, scene->lights.count);

    scene_build_static(scene, world->tris, world->count);
    printf("Static BVH: %d nodes\n", scene->static_bvh.node_count);

    nav_build(&scene->nav, &scene->static_bvh);
    printf("Nav grid: %dx%d cells, %d floors\n",
           scene->nav.nx, scene->nav.nz, nav_floor_count(&scene->nav));
    mesh_free(world);
}

static bool has_extension(const char *path, const char *ext) {
    size_t n = strlen(path);
    size_t e = strlen(ext);
    return n >= e && strcmp(path + n - e, ext) == 0;
}

int level_load(Scene *scene, const char *filename, LevelInfo *info) {
    if (has_extension(filename, ".map")) {
        return mapfile_load(scene, filename, info);
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "level: cannot open '%s'\n", filename);
        return -1;
    }

    /* Initialize parser state */
    LevelParser parser = {.scene = scene};
    matlib_init(&parser.mats, scene);
    mesh_init(&parser.world_mesh);

    /* Default spawn */
    info->spawn_pos = vec3(0, 0, 0);
    info->has_spawn = false;
    info->spawn_yaw = 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        char *nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
        }
        char *cr = strchr(line, '\r');
        if (cr) {
            *cr = '\0';
        }

        /* Skip empty lines and comments */
        const char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0' || *p == '#') {
            continue;
        }

        if (strncmp(p, "spawn ", 6) == 0) {
            parse_spawn(p, info);
        } else if (strncmp(p, "material ", 9) == 0) {
            parse_material(p, &parser);
        } else if (strncmp(p, "room ", 5) == 0) {
            parse_room(p, &parser);
        } else if (strncmp(p, "box ", 4) == 0) {
            parse_box(p, &parser);
        } else if (strncmp(p, "quad ", 5) == 0) {
            parse_quad(p, &parser);
        } else if (strncmp(p, "obj ", 4) == 0) {
            parse_obj(p, &parser);
        } else if (strncmp(p, "brush ", 6) == 0) {
            parse_brush(p, &parser);
        } else if (strncmp(p, "entity ", 7) == 0) {
            parse_entity(p, &parser);
        } else if (strncmp(p, "light ", 6) == 0) {
            parse_light(p, scene);
        } else if (strncmp(p, "zone ", 5) == 0) {
            parse_zone(p, scene);
        } else if (is_environment(p)) {
            parse_environment(p, scene);
        } else {
            fprintf(stderr, "level: unknown directive: %.40s\n", p);
        }
    }

    fclose(f);

    level_finish(scene, &parser.mats, &parser.world_mesh, filename);
    return 0;
}
