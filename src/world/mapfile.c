#include "mapfile.h"
#include "matlib.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_TOKEN_MAX    256
#define MAP_KEYS_MAX     32
#define MAP_KEY_MAX      64
#define MAP_VALUE_MAX    256
#define MAP_ARGS_MAX     1024
#define BRUSH_FACES_MAX  64
#define POLY_VERTS_MAX   64
#define PLANE_EPS        0.01   /* map units: points this close count as on-plane */
#define BRUSH_EXTENT     65536.0 /* half-size of the initial face polygon */
#define DEG_TO_RAD       (3.14159265358979 / 180.0)

#define DEFAULT_LIGHT_INTENSITY 2.0f
#define DEFAULT_LIGHT_RADIUS    320.0f  /* map units (10 m) */
#define COLOR_BYTE_MAX          255.0f
#define UNTEXTURED_GREY         0.5f

/* Map geometry is done in double: polygons start 65536 units wide and
 * get clipped down, which float can't do to 0.01-unit accuracy. */
typedef struct {
    double x, y, z;
} D3;

static D3 d3(double x, double y, double z) { return (D3){x, y, z}; }
static D3 d3_add(D3 a, D3 b) { return d3(a.x + b.x, a.y + b.y, a.z + b.z); }
static D3 d3_sub(D3 a, D3 b) { return d3(a.x - b.x, a.y - b.y, a.z - b.z); }
static D3 d3_mul(D3 a, double s) { return d3(a.x * s, a.y * s, a.z * s); }
static double d3_dot(D3 a, D3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static D3 d3_cross(D3 a, D3 b) {
    return d3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static D3 d3_normalize(D3 a) {
    double len = sqrt(d3_dot(a, a));
    return len > 0 ? d3_mul(a, 1.0 / len) : a;
}

static Vec3 d3_to_engine(D3 p) {
    return map_to_engine(vec3((float)p.x, (float)p.y, (float)p.z));
}

/* --- Tokenizer: { } ( ) [ ], "quoted strings", bare words; // comments --- */

typedef struct {
    const char *p;
    int line;
    char tok[MAP_TOKEN_MAX];
} Lexer;

static bool is_punct(char c) {
    return c && strchr("{}()[]", c) != NULL;
}

static void skip_space(Lexer *lx) {
    for (;;) {
        while (isspace((unsigned char)*lx->p)) {
            lx->line += *lx->p == '\n';
            lx->p++;
        }
        if (lx->p[0] != '/' || lx->p[1] != '/') {
            return;
        }
        while (*lx->p && *lx->p != '\n') {
            lx->p++;
        }
    }
}

static bool next_token(Lexer *lx) {
    skip_space(lx);
    if (!*lx->p) {
        return false;
    }

    size_t n = 0;
    if (is_punct(*lx->p)) {
        lx->tok[n++] = *lx->p++;
    } else if (*lx->p == '"') {
        lx->p++;
        while (*lx->p && *lx->p != '"' && n < MAP_TOKEN_MAX - 1) {
            lx->tok[n++] = *lx->p++;
        }
        if (*lx->p == '"') {
            lx->p++;
        }
    } else {
        while (*lx->p && !isspace((unsigned char)*lx->p) && !is_punct(*lx->p) &&
               *lx->p != '"' && n < MAP_TOKEN_MAX - 1) {
            lx->tok[n++] = *lx->p++;
        }
    }
    lx->tok[n] = '\0';
    return true;
}

/* Next token without consuming it */
static const char *peek_token(Lexer *lx, char *buf, size_t size) {
    Lexer saved = *lx;
    buf[0] = '\0';
    if (next_token(&saved)) {
        snprintf(buf, size, "%s", saved.tok);
    }
    return buf;
}

static bool expect(Lexer *lx, const char *want) {
    if (next_token(lx) && strcmp(lx->tok, want) == 0) {
        return true;
    }
    fprintf(stderr, "map: line %d: expected '%s', got '%s'\n", lx->line, want, lx->tok);
    return false;
}

static double read_number(Lexer *lx) {
    next_token(lx);
    return atof(lx->tok);
}

static bool read_point(Lexer *lx, D3 *out) {
    if (!expect(lx, "(")) {
        return false;
    }
    out->x = read_number(lx);
    out->y = read_number(lx);
    out->z = read_number(lx);
    return expect(lx, ")");
}

/* --- Brushes --- */

typedef struct {
    D3 normal;                 /* outward */
    double dist;
    char texture[MAP_TOKEN_MAX];
    D3 u_axis, v_axis;         /* texture axes, map units → texels */
    double u_offset, v_offset;
    double u_scale, v_scale;
} Face;

typedef struct {
    Face faces[BRUSH_FACES_MAX];
    int count;
} Brush;

/* Quake's texture projection for the Standard format: pick the axis
 * plane closest to the face, then rotate its axes by `rotation` degrees.
 * Rows: face normal, u axis, v axis. */
static const double base_axes[18][3] = {
    {0, 0, 1},  {1, 0, 0}, {0, -1, 0},  /* floor */
    {0, 0, -1}, {1, 0, 0}, {0, -1, 0},  /* ceiling */
    {1, 0, 0},  {0, 1, 0}, {0, 0, -1},  /* west wall */
    {-1, 0, 0}, {0, 1, 0}, {0, 0, -1},  /* east wall */
    {0, 1, 0},  {1, 0, 0}, {0, 0, -1},  /* south wall */
    {0, -1, 0}, {1, 0, 0}, {0, 0, -1},  /* north wall */
};

static void standard_axes(Face *f, double rotation) {
    int best = 0;
    double best_dot = -1;
    for (int i = 0; i < 6; i++) {
        const double *n = base_axes[i * 3];
        double dot = f->normal.x * n[0] + f->normal.y * n[1] + f->normal.z * n[2];
        if (dot > best_dot) {
            best_dot = dot;
            best = i;
        }
    }

    double axes[2][3];
    memcpy(axes[0], base_axes[best * 3 + 1], sizeof(axes[0]));
    memcpy(axes[1], base_axes[best * 3 + 2], sizeof(axes[1]));

    /* Rotate within the plane spanned by each axis's non-zero component */
    double s = sin(rotation * DEG_TO_RAD);
    double c = cos(rotation * DEG_TO_RAD);
    int sv = axes[0][0] != 0 ? 0 : axes[0][1] != 0 ? 1 : 2;
    int tv = axes[1][0] != 0 ? 0 : axes[1][1] != 0 ? 1 : 2;
    for (int i = 0; i < 2; i++) {
        double ns = c * axes[i][sv] - s * axes[i][tv];
        double nt = s * axes[i][sv] + c * axes[i][tv];
        axes[i][sv] = ns;
        axes[i][tv] = nt;
    }

    f->u_axis = d3(axes[0][0], axes[0][1], axes[0][2]);
    f->v_axis = d3(axes[1][0], axes[1][1], axes[1][2]);
}

/* One face line:
 *   ( p0 ) ( p1 ) ( p2 ) TEX  xoff yoff rot xscale yscale            Standard
 *   ( p0 ) ( p1 ) ( p2 ) TEX [ ux uy uz uoff ] [ vx vy vz voff ] rot xscale yscale   Valve 220
 * possibly followed by Quake 2/3 flag numbers, which are ignored. */
static bool read_face(Lexer *lx, Face *f) {
    D3 p[3];
    for (int i = 0; i < 3; i++) {
        if (!read_point(lx, &p[i])) {
            return false;
        }
    }

    /* qbsp convention: this cross product points out of the brush */
    f->normal = d3_normalize(d3_cross(d3_sub(p[0], p[1]), d3_sub(p[2], p[1])));
    f->dist = d3_dot(f->normal, p[0]);

    next_token(lx);
    snprintf(f->texture, sizeof(f->texture), "%s", lx->tok);

    char peek[MAP_TOKEN_MAX];
    double rotation = 0;
    if (strcmp(peek_token(lx, peek, sizeof(peek)), "[") == 0) {
        D3 axes[2];
        double offsets[2];
        for (int i = 0; i < 2; i++) {
            expect(lx, "[");
            axes[i].x = read_number(lx);
            axes[i].y = read_number(lx);
            axes[i].z = read_number(lx);
            offsets[i] = read_number(lx);
            expect(lx, "]");
        }
        f->u_axis = axes[0];
        f->v_axis = axes[1];
        f->u_offset = offsets[0];
        f->v_offset = offsets[1];
        rotation = read_number(lx); /* already baked into the axes */
    } else {
        f->u_offset = read_number(lx);
        f->v_offset = read_number(lx);
        rotation = read_number(lx);
    }
    f->u_scale = read_number(lx);
    f->v_scale = read_number(lx);
    f->u_scale = f->u_scale != 0 ? f->u_scale : 1;
    f->v_scale = f->v_scale != 0 ? f->v_scale : 1;

    if (strcmp(peek, "[") != 0) {
        standard_axes(f, rotation);
    }

    /* Skip optional trailing numbers (content/surface flags) */
    while (strcmp(peek_token(lx, peek, sizeof(peek)), "(") != 0 && strcmp(peek, "}") != 0 &&
           peek[0] != '\0') {
        next_token(lx);
    }
    return true;
}

static bool read_brush(Lexer *lx, Brush *b) {
    b->count = 0;
    char peek[MAP_TOKEN_MAX];
    while (strcmp(peek_token(lx, peek, sizeof(peek)), "(") == 0) {
        if (b->count >= BRUSH_FACES_MAX) {
            fprintf(stderr, "map: line %d: brush has more than %d faces\n", lx->line, BRUSH_FACES_MAX);
            return false;
        }
        if (!read_face(lx, &b->faces[b->count++])) {
            return false;
        }
    }
    return expect(lx, "}");
}

/* Sutherland–Hodgman: keep the part of poly behind plane (n·p <= d) */
static int clip_polygon(const D3 *in, int n, D3 normal, double dist, D3 *out) {
    int count = 0;
    for (int i = 0; i < n && count < POLY_VERTS_MAX - 1; i++) {
        D3 a = in[i];
        D3 b = in[(i + 1) % n];
        double da = d3_dot(normal, a) - dist;
        double db = d3_dot(normal, b) - dist;
        bool a_in = da <= PLANE_EPS;
        bool b_in = db <= PLANE_EPS;

        if (a_in) {
            out[count++] = a;
        }
        if (a_in != b_in) {
            double t = da / (da - db);
            out[count++] = d3_add(a, d3_mul(d3_sub(b, a), t));
        }
    }
    return count;
}

/* Polygon of face i: a huge square on its plane, clipped by every other
 * plane. Winding is counter-clockwise seen from outside. */
static int face_polygon(const Brush *b, int i, D3 *out) {
    const Face *f = &b->faces[i];
    D3 up = fabs(f->normal.z) > 0.9 ? d3(1, 0, 0) : d3(0, 0, 1);
    D3 right = d3_normalize(d3_cross(up, f->normal));
    up = d3_cross(f->normal, right);
    D3 center = d3_mul(f->normal, f->dist);

    D3 r = d3_mul(right, BRUSH_EXTENT);
    D3 u = d3_mul(up, BRUSH_EXTENT);
    D3 poly[POLY_VERTS_MAX] = {
        d3_sub(d3_sub(center, r), u), d3_sub(d3_add(center, r), u),
        d3_add(d3_add(center, r), u), d3_add(d3_sub(center, r), u),
    };
    int n = 4;

    for (int j = 0; j < b->count && n >= 3; j++) {
        if (j == i) {
            continue;
        }
        D3 clipped[POLY_VERTS_MAX];
        n = clip_polygon(poly, n, b->faces[j].normal, b->faces[j].dist, clipped);
        memcpy(poly, clipped, sizeof(D3) * (size_t)n);
    }

    memcpy(out, poly, sizeof(D3) * (size_t)n);
    return n;
}

/* --- Loader state --- */

typedef struct {
    MatLib mats;
    Mesh world;
    Scene *scene;
    LevelInfo *info;
} MapLoader;

static bool is_nodraw(const char *texture) {
    return strcmp(texture, "skip") == 0 || strcmp(texture, "clip") == 0 ||
           strcmp(texture, "trigger") == 0;
}

/* "rt/brick" → "brick" */
static const char *texture_basename(const char *texture) {
    const char *slash = strrchr(texture, '/');
    return slash ? slash + 1 : texture;
}

static int face_material(MapLoader *ml, const char *texture) {
    if (matlib_find(&ml->mats, texture) == MATLIB_NONE && proctex_find(texture) < 0) {
        fprintf(stderr, "map: unknown texture '%s', drawing it untextured\n", texture);
    }
    float grey = UNTEXTURED_GREY;
    return matlib_texture(&ml->mats, texture, vec3(grey, grey, grey));
}

static Vec2 face_uv(const Face *f, D3 p, int tex_w, int tex_h) {
    double u = (d3_dot(p, f->u_axis) / f->u_scale + f->u_offset) / tex_w;
    double v = (d3_dot(p, f->v_axis) / f->v_scale + f->v_offset) / tex_h;
    return (Vec2){(float)u, (float)v};
}

/* Triangulate every drawable face of a brush into out */
static void emit_brush(MapLoader *ml, const Brush *b, Mesh *out) {
    for (int i = 0; i < b->count; i++) {
        const Face *f = &b->faces[i];
        const char *tex = texture_basename(f->texture);
        if (is_nodraw(tex)) {
            continue;
        }

        D3 poly[POLY_VERTS_MAX];
        int n = face_polygon(b, i, poly);
        if (n < 3) {
            continue;
        }

        int mat = face_material(ml, tex);
        int tw, th;
        matlib_texture_size(&ml->mats, mat, &tw, &th);
        Vec3 normal = vec3_normalize(map_to_engine(vec3((float)f->normal.x,
                                                        (float)f->normal.y,
                                                        (float)f->normal.z)));

        /* Fan from vertex 0 */
        for (int k = 1; k + 1 < n; k++) {
            D3 c[3] = {poly[0], poly[k], poly[k + 1]};
            Triangle t = {
                .v0 = d3_to_engine(c[0]), .v1 = d3_to_engine(c[1]), .v2 = d3_to_engine(c[2]),
                .uv0 = face_uv(f, c[0], tw, th),
                .uv1 = face_uv(f, c[1], tw, th),
                .uv2 = face_uv(f, c[2], tw, th),
                .normal = normal,
                .material_id = mat,
            };
            mesh_add_tri(out, t);
        }
    }
}

/* Grow [lo, hi] (engine space) by every face corner, drawn or not: a
 * trigger is all "trigger" faces and still has a size */
static void brush_bounds(const Brush *b, Vec3 *lo, Vec3 *hi) {
    for (int i = 0; i < b->count; i++) {
        D3 poly[POLY_VERTS_MAX];
        int n = face_polygon(b, i, poly);
        for (int k = 0; k < n; k++) {
            Vec3 p = d3_to_engine(poly[k]);
            *lo = vec3(fminf(lo->x, p.x), fminf(lo->y, p.y), fminf(lo->z, p.z));
            *hi = vec3(fmaxf(hi->x, p.x), fmaxf(hi->y, p.y), fmaxf(hi->z, p.z));
        }
    }
}

/* --- Entities --- */

typedef struct {
    char key[MAP_KEY_MAX];
    char value[MAP_VALUE_MAX];
} KeyValue;

typedef struct {
    KeyValue pairs[MAP_KEYS_MAX];
    int count;
} KeyValues;

static const char *kv_get(const KeyValues *kvs, const char *key, const char *def) {
    for (int i = 0; i < kvs->count; i++) {
        if (strcmp(kvs->pairs[i].key, key) == 0) {
            return kvs->pairs[i].value;
        }
    }
    return def;
}

static D3 kv_vector(const KeyValues *kvs, const char *key, D3 def) {
    const char *v = kv_get(kvs, key, NULL);
    D3 out = def;
    if (v) {
        sscanf(v, "%lf %lf %lf", &out.x, &out.y, &out.z);
    }
    return out;
}

static bool is_world_geometry(const char *classname) {
    return strcmp(classname, "worldspawn") == 0 || strcmp(classname, "func_group") == 0 ||
           strcmp(classname, "func_detail") == 0;
}

/* Keys the loader handles itself or that TrenchBroom adds for its own use */
static bool is_reserved_key(const char *key) {
    return strcmp(key, "classname") == 0 || strcmp(key, "origin") == 0 ||
           strcmp(key, "angle") == 0 || strncmp(key, "_tb_", 4) == 0;
}

/* Build "key=value ..." args for entity_spawn(), converting lengths from
 * map units to meters and the map angle to rot (radians) */
static void entity_args(const KeyValues *kvs, const EntityDef *def, char *out, size_t size) {
    size_t len = 0;
    out[0] = '\0';
    for (int i = 0; i < kvs->count && len < size; i++) {
        const KeyValue *kv = &kvs->pairs[i];
        if (is_reserved_key(kv->key)) {
            continue;
        }
        if (!entity_key_known(def, kv->key)) {
            fprintf(stderr, "map: %s has unknown key '%s'\n", def->name, kv->key);
        }
        const EntityKey *meta = entity_key(def, kv->key);
        if (meta && meta->type == KEY_LENGTH) {
            float meters = (float)atof(kv->value) / MAP_UNITS_PER_METER;
            len += (size_t)snprintf(out + len, size - len, "%s=%g ", kv->key, meters);
        } else {
            len += (size_t)snprintf(out + len, size - len, "%s=%s ", kv->key, kv->value);
        }
    }

    const char *angle = kv_get(kvs, "angle", NULL);
    if (angle && len < size) {
        double rot = map_angle_to_yaw((float)atof(angle)) * DEG_TO_RAD;
        snprintf(out + len, size - len, "rot=%g", rot);
    }
}

static const char *light_keys[] = {
    "intensity", "radius", "color", "shadows", "flicker_speed", "flicker_amount",
    "cord", "swing_speed", "swing_angle",
};

static void warn_unknown_light_keys(const KeyValues *kvs) {
    for (int i = 0; i < kvs->count; i++) {
        const char *key = kvs->pairs[i].key;
        bool known = is_reserved_key(key);
        for (size_t k = 0; k < sizeof(light_keys) / sizeof(light_keys[0]) && !known; k++) {
            known = strcmp(key, light_keys[k]) == 0;
        }
        if (!known) {
            fprintf(stderr, "map: light has unknown key '%s'\n", key);
        }
    }
}

static void spawn_light(MapLoader *ml, const KeyValues *kvs, Vec3 pos) {
    warn_unknown_light_keys(kvs);

    D3 rgb = kv_vector(kvs, "color", d3(1, 1, 1));
    if (rgb.x > 1 || rgb.y > 1 || rgb.z > 1) {
        rgb = d3_mul(rgb, 1.0 / COLOR_BYTE_MAX); /* editor color picker: 0..255 */
    }
    float intensity = (float)atof(kv_get(kvs, "intensity", "0"));
    float radius = (float)atof(kv_get(kvs, "radius", "0"));
    intensity = intensity > 0 ? intensity : DEFAULT_LIGHT_INTENSITY;
    radius = (radius > 0 ? radius : DEFAULT_LIGHT_RADIUS) / MAP_UNITS_PER_METER;

    /* Optional keys in light_from_keys() format; cord is a length */
    char args[MAP_ARGS_MAX];
    float cord = (float)atof(kv_get(kvs, "cord", "0")) / MAP_UNITS_PER_METER;
    snprintf(args, sizeof(args), "shadows=%s flicker_speed=%s flicker_amount=%s "
             "swing_speed=%s swing_angle=%s cord=%g",
             kv_get(kvs, "shadows", "0"), kv_get(kvs, "flicker_speed", "0"),
             kv_get(kvs, "flicker_amount", "0"), kv_get(kvs, "swing_speed", "0"),
             kv_get(kvs, "swing_angle", "0"), cord);

    Vec3 color = vec3((float)rgb.x, (float)rgb.y, (float)rgb.z);
    light_add(&ml->scene->lights, light_from_keys(pos, color, intensity, radius, args));
}

static void spawn_point_entity(MapLoader *ml, const KeyValues *kvs, const char *classname) {
    D3 origin = kv_vector(kvs, "origin", d3(0, 0, 0));
    Vec3 pos = d3_to_engine(origin);

    if (strcmp(classname, "info_player_start") == 0) {
        ml->info->spawn_pos = pos;
        ml->info->has_spawn = true;
        ml->info->spawn_yaw = map_angle_to_yaw((float)atof(kv_get(kvs, "angle", "0")));
        return;
    }
    if (strcmp(classname, "light") == 0) {
        spawn_light(ml, kvs, pos);
        return;
    }

    EntityType type = entity_type_by_name(classname);
    if (type == ENTITY_NONE || entity_def(type)->runtime_only) {
        fprintf(stderr, "map: unknown entity class '%s'\n", classname);
        return;
    }

    const EntityDef *def = entity_def(type);
    char args[MAP_ARGS_MAX];
    entity_args(kvs, def, args, sizeof(args));

    Entity e = entity_spawn(type, pos, args);
    e.material_id = matlib_entity(&ml->mats, def, args);
    entity_add(&ml->scene->entities, e);
}

/* Brushes of a brush entity (func_door), gathered while reading it */
typedef struct {
    Mesh tris;
    Vec3 lo, hi;
    int brushes;
} BrushModel;

static EntityType brush_type(const char *classname) {
    EntityType type = entity_type_by_name(classname);
    return type != ENTITY_NONE && entity_def(type)->brush ? type : ENTITY_NONE;
}

/* Where a brush goes: the world, the entity's model, or nowhere */
static void place_brush(MapLoader *ml, const Brush *b, const char *classname,
                        BrushModel *model) {
    if (is_world_geometry(classname)) {
        emit_brush(ml, b, &ml->world);
        return;
    }
    if (brush_type(classname) != ENTITY_NONE) {
        emit_brush(ml, b, &model->tris);
        brush_bounds(b, &model->lo, &model->hi);
        model->brushes++;
        return;
    }
    if (model->brushes == 0) {
        fprintf(stderr, "map: brushes on '%s' ignored (not a brush entity)\n", classname);
    }
    model->brushes = -1; /* warned once */
}

static void spawn_brush_entity(MapLoader *ml, const KeyValues *kvs, EntityType type,
                               BrushModel *model) {
    if (model->brushes <= 0) {
        fprintf(stderr, "map: %s has no brushes\n", entity_def(type)->name);
        mesh_free(&model->tris);
        return;
    }
    char args[MAP_ARGS_MAX];
    entity_args(kvs, entity_def(type), args, sizeof(args));
    level_add_brush_entity(ml->scene, type, model->lo, model->hi, &model->tris, args);
}

/* { "key" "value" ... { brush } ... } — the opening brace is consumed */
static bool read_entity(Lexer *lx, MapLoader *ml) {
    KeyValues kvs = {0};
    Brush *brush = malloc(sizeof(Brush));
    if (!brush) {
        return false;
    }
    BrushModel model = {.lo = vec3(INFINITY, INFINITY, INFINITY),
                        .hi = vec3(-INFINITY, -INFINITY, -INFINITY)};
    mesh_init(&model.tris);

    bool ok = true;
    while (ok && next_token(lx) && strcmp(lx->tok, "}") != 0) {
        if (strcmp(lx->tok, "{") == 0) {
            ok = read_brush(lx, brush);
            if (ok) {
                place_brush(ml, brush, kv_get(&kvs, "classname", ""), &model);
            }
            continue;
        }

        if (kvs.count >= MAP_KEYS_MAX) {
            next_token(lx);
            continue;
        }
        KeyValue *kv = &kvs.pairs[kvs.count++];
        snprintf(kv->key, sizeof(kv->key), "%s", lx->tok);
        next_token(lx);
        snprintf(kv->value, sizeof(kv->value), "%s", lx->tok);
    }
    free(brush);

    const char *classname = kv_get(&kvs, "classname", "");
    EntityType type = brush_type(classname);
    if (!ok || is_world_geometry(classname)) {
        mesh_free(&model.tris);
    } else if (type != ENTITY_NONE) {
        spawn_brush_entity(ml, &kvs, type, &model);
    } else {
        mesh_free(&model.tris);
        spawn_point_entity(ml, &kvs, classname);
    }
    return ok;
}

/* --- Public API --- */

static char *read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *text = malloc((size_t)size + 1);
    if (text) {
        size_t got = fread(text, 1, (size_t)size, f);
        text[got] = '\0';
    }
    fclose(f);
    return text;
}

int mapfile_load(Scene *scene, const char *filename, LevelInfo *info) {
    char *text = read_file(filename);
    if (!text) {
        fprintf(stderr, "map: cannot open '%s'\n", filename);
        return -1;
    }

    MapLoader ml = {.scene = scene, .info = info};
    matlib_init(&ml.mats, scene);
    mesh_init(&ml.world);
    info->spawn_pos = vec3(0, 0, 0);
    info->has_spawn = false;
    info->spawn_yaw = 0;

    Lexer lx = {.p = text, .line = 1};
    bool ok = true;
    while (ok && next_token(&lx)) {
        if (strcmp(lx.tok, "{") != 0) {
            fprintf(stderr, "map: line %d: expected '{', got '%s'\n", lx.line, lx.tok);
            ok = false;
            break;
        }
        ok = read_entity(&lx, &ml);
    }
    free(text);

    level_finish(scene, &ml.mats, &ml.world, filename);
    return ok ? 0 : -1;
}
