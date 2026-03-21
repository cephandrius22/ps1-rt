#include "level.h"
#include "proctex.h"
#include "ps1_effects.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_MATERIALS 64
#define UV_TILE_SCALE 2.0f /* 1 texture repeat per this many world units */

typedef struct {
    char name[32];
    int scene_index; /* index into scene->materials[] */
} MatEntry;

typedef struct {
    MatEntry mats[MAX_MATERIALS];
    int mat_count;

    Texture proc_textures[PTEX_COUNT];
    bool proc_generated[PTEX_COUNT];
    int proc_scene_index[PTEX_COUNT]; /* maps PTEX_* to scene->textures[] index */

    Mesh world_mesh;
    Scene *scene;

    int next_tex_index;
} LevelParser;

/* --- Material name lookup --- */

static int find_mat(LevelParser *p, const char *name) {
    for (int i = 0; i < p->mat_count; i++) {
        if (strcmp(p->mats[i].name, name) == 0)
            return p->mats[i].scene_index;
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

/* --- Key=value parser for optional parameters --- */

static float kv_float(const char *line, const char *key, float def) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *found = strstr(line, pattern);
    if (!found) return def;
    return (float)atof(found + strlen(pattern));
}

static int kv_int(const char *line, const char *key, int def) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *found = strstr(line, pattern);
    if (!found) return def;
    return atoi(found + strlen(pattern));
}

/* --- Line parsers --- */

static void parse_spawn(const char *line, LevelInfo *info) {
    sscanf(line, "spawn %f %f %f %f",
           &info->spawn_pos.x, &info->spawn_pos.y, &info->spawn_pos.z,
           &info->spawn_yaw);
}

static void parse_material(const char *line, LevelParser *p) {
    if (p->mat_count >= MAX_MATERIALS) {
        fprintf(stderr, "level: too many materials (max %d)\n", MAX_MATERIALS);
        return;
    }

    char name[32], tex_name[32];
    float r = 0.5f, g = 0.5f, b = 0.5f;
    sscanf(line, "material %31s %31s %f %f %f", name, tex_name, &r, &g, &b);

    int tex_id = -1;
    int ptex = proctex_find(tex_name);
    if (ptex >= 0) {
        /* Generate this procedural texture if not already done */
        if (!p->proc_generated[ptex]) {
            proctex_generate(&p->proc_textures[ptex], ptex);
            p->proc_generated[ptex] = true;
            p->proc_scene_index[ptex] = p->next_tex_index++;
        }
        tex_id = p->proc_scene_index[ptex];
    }

    int idx = p->mat_count;
    MatEntry *me = &p->mats[idx];
    strncpy(me->name, name, 31);
    me->name[31] = '\0';
    me->scene_index = idx;
    p->mat_count++;

    /* Grow scene materials array */
    p->scene->materials = realloc(p->scene->materials,
                                   (size_t)(idx + 1) * sizeof(Material));
    p->scene->materials[idx] = (Material){.color = vec3(r, g, b), .texture_id = tex_id};
    p->scene->material_count = idx + 1;
}

static void parse_room(const char *line, LevelParser *p) {
    float x1, y1, z1, x2, y2, z2;
    char wall_name[32], floor_name[32], ceil_name[32];

    if (sscanf(line, "room %f %f %f %f %f %f %31s %31s %31s",
               &x1, &y1, &z1, &x2, &y2, &z2,
               wall_name, floor_name, ceil_name) < 9)
        return;

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
               &x1, &y1, &z1, &x2, &y2, &z2, mat_name) < 7)
        return;

    int mat_id = find_mat(p, mat_name);
    add_box_outer(&p->world_mesh, vec3(x1, y1, z1), vec3(x2, y2, z2), mat_id);
}

static void parse_quad(const char *line, LevelParser *p) {
    float x1,y1,z1, x2,y2,z2, x3,y3,z3, x4,y4,z4;
    char mat_name[32];

    if (sscanf(line, "quad %f %f %f %f %f %f %f %f %f %f %f %f %31s",
               &x1,&y1,&z1, &x2,&y2,&z2, &x3,&y3,&z3, &x4,&y4,&z4, mat_name) < 13)
        return;

    int mat_id = find_mat(p, mat_name);
    add_quad_uv(&p->world_mesh,
                vec3(x1,y1,z1), vec3(x2,y2,z2), vec3(x3,y3,z3), vec3(x4,y4,z4),
                uv(0,0), uv(1,0), uv(1,1), uv(0,1), mat_id);
}

static void parse_obj(const char *line, LevelParser *p) {
    char filename[256], mat_name[32];
    float x, y, z, rot_y, scale;

    if (sscanf(line, "obj %255s %f %f %f %f %f %31s",
               filename, &x, &y, &z, &rot_y, &scale, mat_name) < 7)
        return;

    int mat_id = find_mat(p, mat_name);

    Mesh obj_mesh;
    mesh_init(&obj_mesh);
    if (mesh_load_obj(&obj_mesh, filename) != 0) {
        mesh_free(&obj_mesh);
        return;
    }

    /* Transform and add to world mesh */
    float rad = rot_y * (float)M_PI / 180.0f;
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

static void parse_entity(const char *line, Scene *scene) {
    char type_str[32];
    float x, y, z;

    if (sscanf(line, "entity %31s %f %f %f", type_str, &x, &y, &z) < 4)
        return;

    Entity e = {.position = vec3(x, y, z), .active = true};

    if (strcmp(type_str, "health") == 0) {
        e.type = ENTITY_PICKUP_HEALTH;
        e.mesh_id = ENTITY_PICKUP_HEALTH;
        e.pickup.amount = kv_float(line, "amount", 25);
    } else if (strcmp(type_str, "ammo") == 0) {
        e.type = ENTITY_PICKUP_AMMO;
        e.mesh_id = ENTITY_PICKUP_AMMO;
        e.pickup.amount = kv_float(line, "amount", 10);
    } else if (strcmp(type_str, "door") == 0) {
        e.type = ENTITY_DOOR;
        e.mesh_id = ENTITY_DOOR;
        e.rotation_y = kv_float(line, "rot", 0);
        e.door.angle = 0;
        e.door.target_angle = 0;
        e.door.swing_speed = DOOR_SWING_SPEED;
        e.door.hinge = vec3(
            kv_float(line, "hinge_x", x),
            0,
            kv_float(line, "hinge_z", z));
    } else if (strcmp(type_str, "prop") == 0) {
        e.type = ENTITY_PROP;
        e.mesh_id = ENTITY_PROP;
        e.rotation_y = kv_float(line, "rot", 0);
    } else {
        fprintf(stderr, "level: unknown entity type '%s'\n", type_str);
        return;
    }

    /* Material ID from key=value, default based on type */
    e.material_id = kv_int(line, "mat", 0);

    entity_add(&scene->entities, e);
}

static void parse_light(const char *line, Scene *scene) {
    float x, y, z, r, g, b, intensity, radius;

    if (sscanf(line, "light %f %f %f %f %f %f %f %f",
               &x, &y, &z, &r, &g, &b, &intensity, &radius) < 8)
        return;

    PointLight pl = {
        .position = vec3(x, y, z),
        .color = vec3(r, g, b),
        .intensity = intensity,
        .radius = radius,
        .cast_shadows = kv_int(line, "shadows", 0) != 0,
        .flicker_speed = kv_float(line, "flicker_speed", 0),
        .flicker_amount = kv_float(line, "flicker_amount", 0),
        .anchor = vec3(
            kv_float(line, "anchor_x", x),
            kv_float(line, "anchor_y", y),
            kv_float(line, "anchor_z", z)),
        .cord_length = kv_float(line, "cord", 0),
        .swing_speed = kv_float(line, "swing_speed", 0),
        .swing_angle = kv_float(line, "swing_angle", 0),
    };

    light_add(&scene->lights, pl);
}

/* --- Public API --- */

int level_load(Scene *scene, const char *filename, LevelInfo *info) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "level: cannot open '%s'\n", filename);
        return -1;
    }

    /* Initialize parser state */
    LevelParser parser = {0};
    parser.scene = scene;
    mesh_init(&parser.world_mesh);
    for (int i = 0; i < PTEX_COUNT; i++) {
        parser.proc_generated[i] = false;
        parser.proc_scene_index[i] = -1;
    }

    /* Default spawn */
    info->spawn_pos = vec3(0, 0, 0);
    info->spawn_yaw = 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        /* Skip empty lines and comments */
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;

        if (strncmp(p, "spawn ", 6) == 0)
            parse_spawn(p, info);
        else if (strncmp(p, "material ", 9) == 0)
            parse_material(p, &parser);
        else if (strncmp(p, "room ", 5) == 0)
            parse_room(p, &parser);
        else if (strncmp(p, "box ", 4) == 0)
            parse_box(p, &parser);
        else if (strncmp(p, "quad ", 5) == 0)
            parse_quad(p, &parser);
        else if (strncmp(p, "obj ", 4) == 0)
            parse_obj(p, &parser);
        else if (strncmp(p, "entity ", 7) == 0)
            parse_entity(p, scene);
        else if (strncmp(p, "light ", 6) == 0)
            parse_light(p, scene);
        else
            fprintf(stderr, "level: unknown directive: %.40s\n", p);
    }

    fclose(f);

    /* Build texture array from generated procedural textures */
    if (parser.next_tex_index > 0) {
        scene->textures = malloc((size_t)parser.next_tex_index * sizeof(Texture));
        scene->texture_count = parser.next_tex_index;
        for (int i = 0; i < PTEX_COUNT; i++) {
            if (parser.proc_generated[i]) {
                scene->textures[parser.proc_scene_index[i]] = parser.proc_textures[i];
            }
        }
    }

    /* Apply PS1 vertex jitter to world geometry */
    for (int i = 0; i < parser.world_mesh.count; i++)
        parser.world_mesh.tris[i] = ps1_jitter_triangle(&parser.world_mesh.tris[i]);

    /* Build static BVH */
    printf("Level '%s': %d triangles, %d materials, %d textures, %d entities, %d lights\n",
           filename, parser.world_mesh.count, scene->material_count,
           scene->texture_count, scene->entities.count, scene->lights.count);

    scene_build_static(scene, parser.world_mesh.tris, parser.world_mesh.count);
    printf("Static BVH: %d nodes\n", scene->static_bvh.node_count);

    mesh_free(&parser.world_mesh);
    return 0;
}
