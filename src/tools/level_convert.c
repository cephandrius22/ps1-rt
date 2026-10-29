#include "level_convert.h"
#include "entity.h"
#include "mapfile.h"
#include "proctex.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX_LEN   512
#define TOKEN_MAX      256
#define MATS_MAX       64
#define NAME_LEN       32
#define ROOM_WALL      0.25f  /* meters: thickness of the slabs around a room */
#define REPEAT_METERS  2.0f   /* .level UVs: one texture repeat per 2 m */
#define DEFAULT_TEX_PX 64
#define TOOL_TRIGGER   "trigger" /* tool texture for invisible brushes */
#define RAD_TO_DEG     (180.0 / MATH_PI)
#define YAW_TO_ANGLE   90.0f  /* map angle = engine yaw + 90 (mapfile.h) */

static const float U = MAP_UNITS_PER_METER;

/* --- Growable text buffer: world brushes and entities are collected
 * separately, since worldspawn (with every world brush) comes first --- */

typedef struct {
    char *text;
    size_t len, cap;
} Buf;

static void buf_printf(Buf *b, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int need = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (need < 0) {
        return;
    }
    if (b->len + (size_t)need + 1 > b->cap) {
        size_t cap = (b->cap ? b->cap * 2 : 4096) + (size_t)need;
        char *grown = realloc(b->text, cap);
        if (!grown) {
            return;
        }
        b->text = grown;
        b->cap = cap;
    }
    va_start(args, fmt);
    vsnprintf(b->text + b->len, b->cap - b->len, fmt, args);
    va_end(args);
    b->len += (size_t)need;
}

/* --- Materials: .map faces name textures, not materials --- */

typedef struct {
    char name[NAME_LEN];
    char texture[NAME_LEN];
} Mat;

typedef struct {
    Mat mats[MATS_MAX];
    int mat_count;
    Buf world;    /* brushes inside worldspawn */
    Buf env;      /* worldspawn keys */
    Buf ents;     /* every other entity */
    int line_no;
} Converter;

static void warn(const Converter *c, const char *what, const char *line) {
    fprintf(stderr, "convert: line %d: %s: %.60s\n", c->line_no, what, line);
}

/* A material's texture ("none" if unknown or untextured) */
static const char *mat_texture(const Converter *c, const char *name) {
    for (int i = 0; i < c->mat_count; i++) {
        if (strcmp(c->mats[i].name, name) == 0) {
            return c->mats[i].texture;
        }
    }
    return "none";
}

/* Pixel width of a texture, for the scale that keeps 2 m per repeat */
static int texture_px(const char *texture) {
    int id = proctex_find(texture);
    if (id < 0) {
        return DEFAULT_TEX_PX;
    }
    Texture t;
    proctex_generate(&t, (ProceduralTextureID)id);
    int w = t.width;
    texture_free(&t);
    return w;
}

/* --- Geometry: engine meters (Y up) → map units (Z up): (x, -z, y) * 32 --- */

typedef struct {
    double x0, y0, z0, x1, y1, z1;   /* map units, x0 < x1 etc. */
} MapBox;

static MapBox map_box(Vec3 lo, Vec3 hi) {
    return (MapBox){lo.x * U, -hi.z * U, lo.y * U, hi.x * U, -lo.z * U, hi.y * U};
}

/* One face: three points (winding: outward normal), texture, Valve axes */
static void face(Buf *b, double p[9], const char *tex, const char *axes, double scale) {
    buf_printf(b, "( %g %g %g ) ( %g %g %g ) ( %g %g %g ) rt/%s %s 0 %g %g\n",
               p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], tex, axes, scale, scale);
}

#define AXES_X "[ 0 1 0 0 ] [ 0 0 -1 0 ]"
#define AXES_Y "[ 1 0 0 0 ] [ 0 0 -1 0 ]"
#define AXES_Z "[ 1 0 0 0 ] [ 0 -1 0 0 ]"

/* Axis-aligned brush; tex[] per face: +x -x +y -y +z(top) -z(bottom) */
static void brush(Buf *b, MapBox m, const char *tex[6]) {
    double x0 = m.x0, y0 = m.y0, z0 = m.z0, x1 = m.x1, y1 = m.y1, z1 = m.z1;
    double faces[6][9] = {
        {x1, y0 + 1, z0, x1, y0, z0, x1, y0, z0 + 1},
        {x0, y0, z0 + 1, x0, y0, z0, x0, y0 + 1, z0},
        {x0, y1, z0 + 1, x0, y1, z0, x0 + 1, y1, z0},
        {x0 + 1, y0, z0, x0, y0, z0, x0, y0, z0 + 1},
        {x0 + 1, y0, z1, x0, y0, z1, x0, y0 + 1, z1},
        {x0, y0 + 1, z0, x0, y0, z0, x0 + 1, y0, z0},
    };
    const char *axes[6] = {AXES_X, AXES_X, AXES_Y, AXES_Y, AXES_Z, AXES_Z};
    buf_printf(b, "{\n");
    for (int f = 0; f < 6; f++) {
        bool tool = strcmp(tex[f], TOOL_TRIGGER) == 0;
        double scale = tool ? 1.0 : REPEAT_METERS * U / texture_px(tex[f]);
        face(b, faces[f], tex[f], axes[f], scale);
    }
    buf_printf(b, "}\n");
}

static void brush_one(Buf *b, MapBox m, const char *tex) {
    const char *all[6] = {tex, tex, tex, tex, tex, tex};
    brush(b, m, all);
}

static Vec3 ordered_lo(float v[6]) {
    return vec3(fminf(v[0], v[3]), fminf(v[1], v[4]), fminf(v[2], v[5]));
}

static Vec3 ordered_hi(float v[6]) {
    return vec3(fmaxf(v[0], v[3]), fmaxf(v[1], v[4]), fmaxf(v[2], v[5]));
}

/* --- key=value tokens (values may be "quoted") --- */

/* Next whitespace-separated token from *p; quotes keep spaces and are
 * dropped. False at the end of the line. */
static bool next_token(const char **p, char *out, size_t size) {
    const char *s = *p + strspn(*p, " \t");
    if (*s == '\0') {
        return false;
    }
    size_t n = 0;
    bool quoted = false;
    for (; *s && (quoted || (*s != ' ' && *s != '\t')); s++) {
        if (*s == '"') {
            quoted = !quoted;
            continue;
        }
        if (n + 1 < size) {
            out[n++] = *s;
        }
    }
    out[n] = '\0';
    *p = s;
    return true;
}

/* "key" "value" for every key=value on the line, translated for .map:
 * rot (radians) → angle, mat → its texture, length keys → units */
static void write_keys(Converter *c, Buf *b, const EntityDef *def, const char *line) {
    char tok[TOKEN_MAX];
    const char *p = line;
    while (next_token(&p, tok, sizeof(tok))) {
        char *eq = strchr(tok, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        const char *key = tok, *value = eq + 1;

        if (strcmp(key, "rot") == 0) {
            buf_printf(b, "\"angle\" \"%g\"\n", atof(value) * RAD_TO_DEG + YAW_TO_ANGLE);
            continue;
        }
        if (strcmp(key, "mat") == 0) {
            buf_printf(b, "\"mat\" \"%s\"\n", mat_texture(c, value));
            continue;
        }
        const EntityKey *meta = def ? entity_key(def, key) : NULL;
        if (meta && meta->type == KEY_LENGTH) {
            buf_printf(b, "\"%s\" \"%g\"\n", key, atof(value) * U);
            continue;
        }
        buf_printf(b, "\"%s\" \"%s\"\n", key, value);
    }
}

/* --- Directives --- */

static void convert_material(Converter *c, const char *line) {
    Mat m = {0};
    if (sscanf(line, "material %31s %31s", m.name, m.texture) < 2 || c->mat_count >= MATS_MAX) {
        warn(c, "bad material", line);
        return;
    }
    if (strcmp(m.texture, "none") == 0) {
        warn(c, "flat-color material can't be a .map texture; it draws untextured", line);
    }
    if (strstr(line, "scroll_") || strstr(line, "glow=") || strstr(line, "pulse=")) {
        warn(c, "material motion keys are lost; .map uses the texture's defaults", line);
    }
    c->mats[c->mat_count++] = m;
}

/* A room is the space inside; its walls become slabs around it */
static void convert_room(Converter *c, const char *line) {
    float v[6];
    char wall[NAME_LEN], floor_m[NAME_LEN], ceil_m[NAME_LEN];
    if (sscanf(line, "room %f %f %f %f %f %f %31s %31s %31s", &v[0], &v[1], &v[2], &v[3],
               &v[4], &v[5], wall, floor_m, ceil_m) < 9) {
        warn(c, "bad room", line);
        return;
    }
    Vec3 lo = ordered_lo(v), hi = ordered_hi(v);
    float t = ROOM_WALL;
    const char *w = mat_texture(c, wall);
    struct { Vec3 lo, hi; const char *tex; } slabs[6] = {
        {vec3(lo.x - t, lo.y - t, lo.z - t), vec3(hi.x + t, lo.y, hi.z + t), mat_texture(c, floor_m)},
        {vec3(lo.x - t, hi.y, lo.z - t), vec3(hi.x + t, hi.y + t, hi.z + t), mat_texture(c, ceil_m)},
        {vec3(lo.x - t, lo.y, lo.z - t), vec3(lo.x, hi.y, hi.z + t), w},
        {vec3(hi.x, lo.y, lo.z - t), vec3(hi.x + t, hi.y, hi.z + t), w},
        {vec3(lo.x, lo.y, lo.z - t), vec3(hi.x, hi.y, lo.z), w},
        {vec3(lo.x, lo.y, hi.z), vec3(hi.x, hi.y, hi.z + t), w},
    };
    for (int i = 0; i < 6; i++) {
        brush_one(&c->world, map_box(slabs[i].lo, slabs[i].hi), slabs[i].tex);
    }
}

static void convert_box(Converter *c, const char *line) {
    float v[6];
    char mat[NAME_LEN];
    if (sscanf(line, "box %f %f %f %f %f %f %31s", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5],
               mat) < 7) {
        warn(c, "bad box", line);
        return;
    }
    brush_one(&c->world, map_box(ordered_lo(v), ordered_hi(v)), mat_texture(c, mat));
}

/* brush <type> box [mat] keys: a brush entity around one box */
static void convert_brush(Converter *c, const char *line) {
    char type[NAME_LEN], mat[NAME_LEN] = "";
    float v[6];
    if (sscanf(line, "brush %31s %f %f %f %f %f %f %31s", type, &v[0], &v[1], &v[2], &v[3],
               &v[4], &v[5], mat) < 7) {
        warn(c, "bad brush", line);
        return;
    }
    bool has_mat = mat[0] && !strchr(mat, '=');
    buf_printf(&c->ents, "{\n\"classname\" \"%s\"\n", type);
    write_keys(c, &c->ents, entity_def(entity_type_by_name(type)), line);
    brush_one(&c->ents, map_box(ordered_lo(v), ordered_hi(v)),
              has_mat ? mat_texture(c, mat) : TOOL_TRIGGER);
    buf_printf(&c->ents, "}\n");
}

/* Fog distances in a value "start end r g b" → units */
static void fog_value(const char *value, char *out, size_t size) {
    float start, end, r, g, b;
    if (sscanf(value, "%f %f %f %f %f", &start, &end, &r, &g, &b) == 5) {
        snprintf(out, size, "%g %g %g %g %g", start * U, end * U, r, g, b);
    } else {
        snprintf(out, size, "%s", value);
    }
}

static void convert_zone(Converter *c, const char *line) {
    float v[6];
    if (sscanf(line, "zone %f %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) < 6) {
        warn(c, "bad zone", line);
        return;
    }
    buf_printf(&c->ents, "{\n\"classname\" \"env_zone\"\n");
    char tok[TOKEN_MAX], fog[TOKEN_MAX];
    const char *p = line;
    while (next_token(&p, tok, sizeof(tok))) {
        char *eq = strchr(tok, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        if (strcmp(tok, "fog") == 0) {
            fog_value(eq + 1, fog, sizeof(fog));
            buf_printf(&c->ents, "\"fog\" \"%s\"\n", fog);
            continue;
        }
        buf_printf(&c->ents, "\"%s\" \"%s\"\n", tok, eq + 1);
    }
    brush_one(&c->ents, map_box(ordered_lo(v), ordered_hi(v)), TOOL_TRIGGER);
    buf_printf(&c->ents, "}\n");
}

static void origin(Buf *b, Vec3 p) {
    buf_printf(b, "\"origin\" \"%g %g %g\"\n", p.x * U, -p.z * U, p.y * U);
}

static void convert_entity(Converter *c, const char *line) {
    char type[NAME_LEN];
    float x, y, z;
    if (sscanf(line, "entity %31s %f %f %f", type, &x, &y, &z) < 4) {
        warn(c, "bad entity", line);
        return;
    }
    buf_printf(&c->ents, "{\n\"classname\" \"%s\"\n", type);
    origin(&c->ents, vec3(x, y, z));
    write_keys(c, &c->ents, entity_def(entity_type_by_name(type)), line);
    buf_printf(&c->ents, "}\n");
}

/* Light keys: lengths to units, aim to map axes; the swing anchor is
 * implied by cord in .map (straight above), so anchor_* are dropped */
static void convert_light(Converter *c, const char *line) {
    float x, y, z, r, g, b, intensity, radius;
    if (sscanf(line, "light %f %f %f %f %f %f %f %f", &x, &y, &z, &r, &g, &b, &intensity,
               &radius) < 8) {
        warn(c, "bad light", line);
        return;
    }
    Buf *e = &c->ents;
    buf_printf(e, "{\n\"classname\" \"light\"\n");
    origin(e, vec3(x, y, z));
    buf_printf(e, "\"color\" \"%g %g %g\"\n\"intensity\" \"%g\"\n\"radius\" \"%g\"\n", r, g, b,
               intensity, radius * U);

    char tok[TOKEN_MAX];
    float aim[3] = {0, -1, 0};
    bool aimed = false;
    const char *p = line;
    while (next_token(&p, tok, sizeof(tok))) {
        char *eq = strchr(tok, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        const char *key = tok, *value = eq + 1;
        if (strncmp(key, "anchor_", 7) == 0) {
            continue;
        }
        if (strncmp(key, "aim_", 4) == 0 && key[4] >= 'x' && key[4] <= 'z' && !key[5]) {
            aim[key[4] - 'x'] = (float)atof(value);
            aimed = true;
            continue;
        }
        if (strcmp(key, "cord") == 0) {
            buf_printf(e, "\"cord\" \"%g\"\n", atof(value) * U);
            continue;
        }
        buf_printf(e, "\"%s\" \"%s\"\n", key, value);
    }
    if (aimed) {
        buf_printf(e, "\"aim\" \"%g %g %g\"\n", aim[0], -aim[2], aim[1]);
    }
    buf_printf(e, "}\n");
}

static void convert_spawn(Converter *c, const char *line) {
    float x, y, z, yaw = 0;
    if (sscanf(line, "spawn %f %f %f %f", &x, &y, &z, &yaw) < 3) {
        warn(c, "bad spawn", line);
        return;
    }
    buf_printf(&c->ents, "{\n\"classname\" \"info_player_start\"\n");
    origin(&c->ents, vec3(x, y, z));
    buf_printf(&c->ents, "\"angle\" \"%g\"\n}\n", yaw + YAW_TO_ANGLE);
}

/* ambient/fog/sun/sky/reverb/ambience → worldspawn keys */
static bool convert_environment(Converter *c, const char *line) {
    static const char *keys[] = {"ambient", "fog", "sun", "sky", "reverb", "ambience"};
    char key[NAME_LEN];
    int used = 0;
    if (sscanf(line, "%31s %n", key, &used) < 1) {
        return false;
    }
    const char *value = line + used;
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (strcmp(key, keys[i]) != 0) {
            continue;
        }
        char out[TOKEN_MAX];
        float dx, dy, dz, r, g, b, k;
        if (strcmp(key, "fog") == 0) {
            fog_value(value, out, sizeof(out));
        } else if (strcmp(key, "sun") == 0 &&
                   sscanf(value, "%f %f %f %f %f %f %f", &dx, &dy, &dz, &r, &g, &b, &k) == 7) {
            snprintf(out, sizeof(out), "%g %g %g %g %g %g %g", dx, -dz, dy, r, g, b, k);
        } else {
            snprintf(out, sizeof(out), "%.*s", (int)strcspn(value, "\r\n"), value);
        }
        buf_printf(&c->env, "\"%s\" \"%s\"\n", key, out);
        return true;
    }
    return false;
}

typedef struct {
    const char *word;   /* with its trailing space */
    void (*convert)(Converter *c, const char *line);
} Directive;

static const Directive directives[] = {
    {"material ", convert_material}, {"room ", convert_room},     {"box ", convert_box},
    {"brush ", convert_brush},       {"zone ", convert_zone},     {"entity ", convert_entity},
    {"light ", convert_light},       {"spawn ", convert_spawn},
};

static void convert_line(Converter *c, const char *line) {
    for (size_t i = 0; i < sizeof(directives) / sizeof(directives[0]); i++) {
        if (strncmp(line, directives[i].word, strlen(directives[i].word)) == 0) {
            directives[i].convert(c, line);
            return;
        }
    }
    if (convert_environment(c, line)) {
        return;
    }
    warn(c, "not convertible (quad/obj have no .map form); skipped", line);
}

int level_convert(const char *level_path, const char *map_path) {
    FILE *in = fopen(level_path, "r");
    if (!in) {
        fprintf(stderr, "convert: cannot open '%s'\n", level_path);
        return -1;
    }

    static Converter c; /* large: material table and buffers */
    memset(&c, 0, sizeof(c));
    char line[LINE_MAX_LEN];
    while (fgets(line, sizeof(line), in)) {
        c.line_no++;
        line[strcspn(line, "\r\n")] = '\0';
        const char *p = line + strspn(line, " \t");
        if (*p != '\0' && *p != '#') {
            convert_line(&c, p);
        }
    }
    fclose(in);

    FILE *out = fopen(map_path, "w");
    int result = -1;
    if (out) {
        fprintf(out, "// Converted from %s by ./rt --convert\n// Valve220\n{\n"
                     "\"classname\" \"worldspawn\"\n\"mapversion\" \"220\"\n%s%s}\n%s",
                level_path, c.env.text ? c.env.text : "", c.world.text ? c.world.text : "",
                c.ents.text ? c.ents.text : "");
        result = ferror(out) ? -1 : 0;
        fclose(out);
    }
    if (result != 0) {
        fprintf(stderr, "convert: cannot write '%s'\n", map_path);
    }
    free(c.world.text);
    free(c.env.text);
    free(c.ents.text);
    return result;
}
