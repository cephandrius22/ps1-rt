#include "mesh.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool ray_triangle_intersect(Ray ray, const Triangle *tri, float t_min, float t_max, HitRecord *rec) {
    Vec3 e1 = vec3_sub(tri->v1, tri->v0);
    Vec3 e2 = vec3_sub(tri->v2, tri->v0);
    Vec3 h = vec3_cross(ray.dir, e2);
    float a = vec3_dot(e1, h);

    if (a > -1e-7f && a < 1e-7f)
        return false;

    float f = 1.0f / a;
    Vec3 s = vec3_sub(ray.origin, tri->v0);
    float u = f * vec3_dot(s, h);
    if (u < 0.0f || u > 1.0f)
        return false;

    Vec3 q = vec3_cross(s, e1);
    float v = f * vec3_dot(ray.dir, q);
    if (v < 0.0f || u + v > 1.0f)
        return false;

    float t = f * vec3_dot(e2, q);
    if (t < t_min || t > t_max)
        return false;

    rec->t = t;
    rec->u = u;
    rec->v = v;
    rec->point = vec3_add(ray.origin, vec3_mul(ray.dir, t));
    rec->normal = tri->normal;
    rec->material_id = tri->material_id;
    rec->hit = true;

    float w = 1.0f - u - v;
    rec->uv.u = w * tri->uv0.u + u * tri->uv1.u + v * tri->uv2.u;
    rec->uv.v = w * tri->uv0.v + u * tri->uv1.v + v * tri->uv2.v;

    return true;
}

void mesh_init(Mesh *m) {
    m->count = 0;
    m->capacity = 64;
    m->tris = malloc(sizeof(Triangle) * m->capacity);
}

void mesh_add_tri(Mesh *m, Triangle tri) {
    if (m->count >= m->capacity) {
        m->capacity *= 2;
        m->tris = realloc(m->tris, sizeof(Triangle) * m->capacity);
    }
    m->tris[m->count++] = tri;
}

void mesh_free(Mesh *m) {
    free(m->tris);
    m->tris = NULL;
    m->count = 0;
    m->capacity = 0;
}

HitRecord mesh_intersect(const Mesh *m, Ray ray, float t_min, float t_max) {
    HitRecord closest = {.hit = false, .t = t_max};

    for (int i = 0; i < m->count; i++) {
        HitRecord rec;
        if (ray_triangle_intersect(ray, &m->tris[i], t_min, closest.t, &rec)) {
            closest = rec;
        }
    }
    return closest;
}

int mesh_load_obj(Mesh *m, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Failed to open OBJ: %s\n", filename);
        return -1;
    }

    Vec3 *verts = NULL;
    Vec2 *uvs = NULL;
    Vec3 *normals = NULL;
    int v_count = 0, v_cap = 256;
    int vt_count = 0, vt_cap = 256;
    int vn_count = 0, vn_cap = 256;

    verts = malloc(sizeof(Vec3) * v_cap);
    uvs = malloc(sizeof(Vec2) * vt_cap);
    normals = malloc(sizeof(Vec3) * vn_cap);

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            Vec3 v;
            sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z);
            if (v_count >= v_cap) { v_cap *= 2; verts = realloc(verts, sizeof(Vec3) * v_cap); }
            verts[v_count++] = v;
        } else if (line[0] == 'v' && line[1] == 't') {
            Vec2 vt;
            sscanf(line + 3, "%f %f", &vt.u, &vt.v);
            if (vt_count >= vt_cap) { vt_cap *= 2; uvs = realloc(uvs, sizeof(Vec2) * vt_cap); }
            uvs[vt_count++] = vt;
        } else if (line[0] == 'v' && line[1] == 'n') {
            Vec3 vn;
            sscanf(line + 3, "%f %f %f", &vn.x, &vn.y, &vn.z);
            if (vn_count >= vn_cap) { vn_cap *= 2; normals = realloc(normals, sizeof(Vec3) * vn_cap); }
            normals[vn_count++] = vn;
        } else if (line[0] == 'f' && line[1] == ' ') {
            // Parse face - supports v, v/vt, v/vt/vn, v//vn
            int vi[4], ti[4], ni[4];
            int has_vt = 0, has_vn = 0;
            int face_verts = 0;

            char *tok = strtok(line + 2, " \t\n\r");
            while (tok && face_verts < 4) {
                vi[face_verts] = 0; ti[face_verts] = 0; ni[face_verts] = 0;

                if (strchr(tok, '/')) {
                    char *slash1 = strchr(tok, '/');
                    *slash1 = '\0';
                    vi[face_verts] = atoi(tok);
                    char *after1 = slash1 + 1;
                    char *slash2 = strchr(after1, '/');
                    if (slash2) {
                        *slash2 = '\0';
                        if (after1[0] != '\0') { ti[face_verts] = atoi(after1); has_vt = 1; }
                        ni[face_verts] = atoi(slash2 + 1); has_vn = 1;
                    } else {
                        ti[face_verts] = atoi(after1); has_vt = 1;
                    }
                } else {
                    vi[face_verts] = atoi(tok);
                }
                face_verts++;
                tok = strtok(NULL, " \t\n\r");
            }

            // Triangulate (fan from vertex 0)
            for (int i = 1; i < face_verts - 1; i++) {
                Triangle tri;
                int idx0 = vi[0] - 1, idx1 = vi[i] - 1, idx2 = vi[i + 1] - 1;

                tri.v0 = (idx0 >= 0 && idx0 < v_count) ? verts[idx0] : vec3(0, 0, 0);
                tri.v1 = (idx1 >= 0 && idx1 < v_count) ? verts[idx1] : vec3(0, 0, 0);
                tri.v2 = (idx2 >= 0 && idx2 < v_count) ? verts[idx2] : vec3(0, 0, 0);

                if (has_vt) {
                    int t0 = ti[0] - 1, t1 = ti[i] - 1, t2 = ti[i + 1] - 1;
                    tri.uv0 = (t0 >= 0 && t0 < vt_count) ? uvs[t0] : (Vec2){0, 0};
                    tri.uv1 = (t1 >= 0 && t1 < vt_count) ? uvs[t1] : (Vec2){0, 0};
                    tri.uv2 = (t2 >= 0 && t2 < vt_count) ? uvs[t2] : (Vec2){0, 0};
                } else {
                    tri.uv0 = tri.uv1 = tri.uv2 = (Vec2){0, 0};
                }

                if (has_vn) {
                    int n0 = ni[0] - 1;
                    tri.normal = (n0 >= 0 && n0 < vn_count) ? normals[n0] : vec3(0, 1, 0);
                } else {
                    tri.normal = vec3_normalize(vec3_cross(
                        vec3_sub(tri.v1, tri.v0),
                        vec3_sub(tri.v2, tri.v0)));
                }

                tri.material_id = 0;
                mesh_add_tri(m, tri);
            }
        }
    }

    fclose(f);
    free(verts);
    free(uvs);
    free(normals);

    printf("Loaded OBJ '%s': %d triangles\n", filename, m->count);
    return 0;
}
