/*
 * dae_loader.c - see dae_loader.h.
 *
 * This is a hand-rolled scanner for one specific slice of COLLADA 1.4.1,
 * not a general XML parser: it locates elements by literal tag text
 * (strstr) and reads attributes with a small "attr="..."" scanner, which is
 * enough for the well-formed, single-line-per-array exports this loader
 * targets. It deliberately never looks at <library_visual_scenes> or
 * <library_controllers> - joints/skinning/animation are out of scope, so a
 * rigged character loads as a static mesh in whatever pose its raw
 * <source> position arrays describe.
 *
 * Builds for the same platforms as obj_loader.c/character.c (needs a
 * filesystem + heap).
 */
#if defined(PLATFORM_PC) || defined(PLATFORM_GC)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dae_loader.h"

static char *read_whole_file(const char *path) {
    FILE *f = fopen(path, "rb");
    long size;
    char *buf;

    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }

    buf = (char *)malloc((size_t)size + 1);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(buf);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* ---- growable float scratch (used for position/normal/uv arrays; stride
   is tracked by the caller, not this struct) ---- */
typedef struct { float *data; int count, cap; } FloatArr;

static void floatarr_push(FloatArr *a, float v) {
    if (a->count == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 1024;
        a->data = (float *)realloc(a->data, sizeof(float) * (size_t)a->cap);
    }
    a->data[a->count++] = v;
}

static void verts_push(Mesh *m, MeshVertex v, int *cap) {
    if (m->vert_count == *cap) {
        *cap = *cap ? *cap * 2 : 512;
        m->verts = (MeshVertex *)realloc(m->verts, sizeof(MeshVertex) * (size_t)(*cap));
    }
    m->verts[m->vert_count++] = v;
}

/* Find the first literal occurrence of `tag` at or after `from`, never at
   or past `limit` (NULL = unbounded). Tags are passed as e.g. "<polylist"
   or "<p>" - just enough literal text to be unambiguous in this schema. */
static const char *xml_find(const char *from, const char *limit, const char *tag) {
    const char *p = strstr(from, tag);
    if (p && limit && p >= limit) return NULL;
    return p;
}

/* Reads attr="value" out of the start tag beginning at `tag_start` (scoped
   to that tag's own '<...>' span so it can't bleed into a later tag). */
static int xml_attr(const char *tag_start, const char *attr, char *out, size_t out_size) {
    const char *tag_end = strchr(tag_start, '>');
    char needle[64];
    const char *p;
    size_t i;
    if (!tag_end) return 0;
    snprintf(needle, sizeof(needle), "%s=\"", attr);
    p = strstr(tag_start, needle);
    if (!p || p >= tag_end) return 0;
    p += strlen(needle);
    i = 0;
    while (*p && *p != '"' && i + 1 < out_size) out[i++] = *p++;
    out[i] = '\0';
    return 1;
}

static void strip_leading_hash(char *s) {
    if (s[0] == '#') memmove(s, s + 1, strlen(s));
}

/* Reads the <float_array> inside <source id="id"> (searched within
   [scope_start, scope_end)) into `out`. Returns 1 on success. */
static int parse_float_array(const char *scope_start, const char *scope_end,
                             const char *id, FloatArr *out) {
    char needle[80];
    const char *src, *arr, *gt, *close, *p;

    snprintf(needle, sizeof(needle), "id=\"%s\"", id);
    src = strstr(scope_start, needle);
    if (!src || (scope_end && src >= scope_end)) return 0;

    arr = xml_find(src, scope_end, "<float_array");
    if (!arr) return 0;
    gt = strchr(arr, '>');
    if (!gt) return 0;
    close = strstr(gt, "</float_array>");
    if (!close) return 0;

    p = gt + 1;
    while (p < close) {
        char *stop;
        float v = strtof(p, &stop);
        if (stop == p) { ++p; continue; }
        floatarr_push(out, v);
        p = stop;
    }
    return out->count > 0;
}

/* Parses one <geometry>...</geometry> block (bounded [g_start, g_end)) and
   appends its triangles to `m`. Returns 1 on success, 0 if this geometry
   didn't look like something we understand (missing position source, no
   polylist/triangles, etc.) - callers just skip it and move on. */
static int process_geometry(const char *g_start, const char *g_end, Mesh *m, int *verts_cap) {
    const char *vtag, *vclose, *pos_input;
    char pos_src_id[64]   = {0};
    char normal_src_id[64] = {0};
    char uv_src_id[64]     = {0};
    FloatArr positions = {0, 0, 0};
    FloatArr normals   = {0, 0, 0};
    FloatArr uvs       = {0, 0, 0};
    int  *vcounts = NULL, vcount_n = 0, vcount_cap = 0;
    int  *idx = NULL, idx_n = 0, idx_cap = 0;
    int   vertex_offset = -1, normal_offset = -1, uv_offset = -1, stride = 1;
    int   is_triangles = 0, face_count = 0, ok = 0;
    const char *poly, *poly_gt, *poly_close;

    /* <vertices id="..."><input semantic="POSITION" source="#points.."/></vertices> */
    vtag = xml_find(g_start, g_end, "<vertices");
    if (!vtag) return 0;
    vclose = strstr(vtag, "</vertices>");
    pos_input = xml_find(vtag, vclose, "<input");
    if (pos_input) {
        xml_attr(pos_input, "source", pos_src_id, sizeof(pos_src_id));
        strip_leading_hash(pos_src_id);
    }
    if (pos_src_id[0] == '\0') return 0;
    if (!parse_float_array(g_start, g_end, pos_src_id, &positions)) return 0;

    /* <polylist> (variable vertex count per face) or <triangles> (always 3) */
    poly = xml_find(g_start, g_end, "<polylist");
    if (poly) {
        poly_close = strstr(poly, "</polylist>");
    } else {
        poly = xml_find(g_start, g_end, "<triangles");
        is_triangles = 1;
        poly_close = poly ? strstr(poly, "</triangles>") : NULL;
    }
    if (!poly || !poly_close) { free(positions.data); return 0; }
    poly_gt = strchr(poly, '>');

    {
        char buf[32];
        if (xml_attr(poly, "count", buf, sizeof(buf))) face_count = atoi(buf);
    }

    /* <input offset="N" semantic="VERTEX|NORMAL|TEXCOORD" source="#..."/>,
       one per referenced source, appearing before <vcount>/<p>. */
    {
        const char *p_open   = xml_find(poly_gt, poly_close, "<p>");
        const char *scan_end = p_open ? p_open : poly_close;
        const char *cur      = poly_gt;
        for (;;) {
            const char *in = xml_find(cur, scan_end, "<input");
            char sem[32], src[64], off[8];
            int o;
            if (!in) break;
            xml_attr(in, "semantic", sem, sizeof(sem));
            xml_attr(in, "source", src, sizeof(src));
            strip_leading_hash(src);
            xml_attr(in, "offset", off, sizeof(off));
            o = atoi(off);
            if (o + 1 > stride) stride = o + 1;

            if (strcmp(sem, "VERTEX") == 0) {
                vertex_offset = o;
            } else if (strcmp(sem, "NORMAL") == 0) {
                normal_offset = o;
                strncpy(normal_src_id, src, sizeof(normal_src_id) - 1);
            } else if (strcmp(sem, "TEXCOORD") == 0) {
                uv_offset = o;
                strncpy(uv_src_id, src, sizeof(uv_src_id) - 1);
            }
            cur = strchr(in, '>');
            if (!cur) break;
            ++cur;
        }
    }
    if (vertex_offset < 0) { free(positions.data); return 0; }
    if (normal_src_id[0]) parse_float_array(g_start, g_end, normal_src_id, &normals);
    if (uv_src_id[0])     parse_float_array(g_start, g_end, uv_src_id, &uvs);

    /* <vcount>3 4 3 ...</vcount> - one entry per face (polylist only). */
    if (!is_triangles) {
        const char *vc = xml_find(poly_gt, poly_close, "<vcount>");
        if (vc) {
            const char *close = strstr(vc, "</vcount>");
            const char *p = strchr(vc, '>') + 1;
            while (close && p < close) {
                char *stop;
                long v = strtol(p, &stop, 10);
                if (stop == p) { ++p; continue; }
                if (vcount_n == vcount_cap) {
                    vcount_cap = vcount_cap ? vcount_cap * 2 : 256;
                    vcounts = (int *)realloc(vcounts, sizeof(int) * (size_t)vcount_cap);
                }
                vcounts[vcount_n++] = (int)v;
                p = stop;
            }
        }
    }

    /* <p>flat, stride-interleaved index list</p>; fan-triangulate each
       face exactly like obj_loader.c does for OBJ polygons. */
    {
        const char *p_open  = xml_find(poly_gt, poly_close, "<p>");
        const char *p_close = p_open ? strstr(p_open, "</p>") : NULL;

        if (p_open && p_close) {
            const char *p = strchr(p_open, '>') + 1;
            int face, base;

            while (p < p_close) {
                char *stop;
                long v = strtol(p, &stop, 10);
                if (stop == p) { ++p; continue; }
                if (idx_n == idx_cap) {
                    idx_cap = idx_cap ? idx_cap * 2 : 1024;
                    idx = (int *)realloc(idx, sizeof(int) * (size_t)idx_cap);
                }
                idx[idx_n++] = (int)v;
                p = stop;
            }

            base = 0;
            for (face = 0; face < face_count; ++face) {
                int fv = is_triangles ? 3 : ((vcounts && face < vcount_n) ? vcounts[face] : 3);
                int t;
                for (t = 1; t + 1 < fv; ++t) {
                    int corner[3];
                    Vec3 gn = vec3(0.0f, 1.0f, 0.0f);
                    int k;
                    corner[0] = 0; corner[1] = t; corner[2] = t + 1;

                    {
                        int i0 = idx[(base + corner[0]) * stride + vertex_offset];
                        int i1 = idx[(base + corner[1]) * stride + vertex_offset];
                        int i2 = idx[(base + corner[2]) * stride + vertex_offset];
                        if (i0 >= 0 && i1 >= 0 && i2 >= 0 &&
                            (i0 * 3 + 2) < positions.count &&
                            (i1 * 3 + 2) < positions.count &&
                            (i2 * 3 + 2) < positions.count) {
                            Vec3 a = vec3(positions.data[i0*3], positions.data[i0*3+1], positions.data[i0*3+2]);
                            Vec3 b = vec3(positions.data[i1*3], positions.data[i1*3+1], positions.data[i1*3+2]);
                            Vec3 c = vec3(positions.data[i2*3], positions.data[i2*3+1], positions.data[i2*3+2]);
                            gn = vec3_norm(vec3_cross(vec3_sub(b, a), vec3_sub(c, a)));
                        }
                    }

                    for (k = 0; k < 3; ++k) {
                        int vi = idx[(base + corner[k]) * stride + vertex_offset];
                        MeshVertex mv;
                        if (vi < 0 || (vi * 3 + 2) >= positions.count) continue;

                        mv.pos    = vec3(positions.data[vi*3], positions.data[vi*3+1], positions.data[vi*3+2]);
                        mv.normal = gn;
                        if (normal_offset >= 0 && normals.count > 0) {
                            int ni = idx[(base + corner[k]) * stride + normal_offset];
                            if (ni >= 0 && (ni * 3 + 2) < normals.count)
                                mv.normal = vec3(normals.data[ni*3], normals.data[ni*3+1], normals.data[ni*3+2]);
                        }
                        mv.u = 0.0f; mv.v = 0.0f;
                        if (uv_offset >= 0 && uvs.count > 0) {
                            int ti = idx[(base + corner[k]) * stride + uv_offset];
                            if (ti >= 0 && (ti * 2 + 1) < uvs.count) {
                                mv.u = uvs.data[ti*2];
                                mv.v = 1.0f - uvs.data[ti*2 + 1]; /* match OBJ's flipped-V convention */
                            }
                        }
                        mv.mtl = 0; /* single flat material; see mesh_load_dae */
                        verts_push(m, mv, verts_cap);
                    }
                }
                base += fv;
            }
            ok = 1;
        }
        free(idx);
    }

    free(vcounts);
    free(positions.data);
    free(normals.data);
    free(uvs.data);
    return ok;
}

int mesh_load_dae(Mesh *m, const char *path) {
    char *text = read_whole_file(path);
    const char *lib_start, *lib_end, *cursor;
    int verts_cap = 0;

    if (!text) return 0;

    memset(m, 0, sizeof(*m));
    /* One flat fallback material (used when no texture is bound) since this
       loader doesn't attempt to track COLLADA <material>/<effect> colors. */
    strncpy(m->materials[0].name, "default", sizeof(m->materials[0].name) - 1);
    m->materials[0].r = 0.82f; m->materials[0].g = 0.70f; m->materials[0].b = 0.60f;
    m->material_count = 1;

    lib_start = strstr(text, "<library_geometries");
    lib_end   = lib_start ? strstr(lib_start, "</library_geometries>") : NULL;
    if (!lib_start || !lib_end) { free(text); return 0; }

    cursor = lib_start;
    for (;;) {
        const char *g_start = xml_find(cursor, lib_end, "<geometry");
        const char *g_end;
        if (!g_start) break;
        g_end = strstr(g_start, "</geometry>");
        if (!g_end || g_end > lib_end) break;
        process_geometry(g_start, g_end, m, &verts_cap);
        cursor = g_end + strlen("</geometry>");
    }

    free(text);

    if (m->vert_count == 0) { mesh_free(m); return 0; }

    m->bounds_min = m->verts[0].pos;
    m->bounds_max = m->verts[0].pos;
    {
        int i;
        for (i = 1; i < m->vert_count; ++i) {
            Vec3 p = m->verts[i].pos;
            if (p.x < m->bounds_min.x) m->bounds_min.x = p.x;
            if (p.y < m->bounds_min.y) m->bounds_min.y = p.y;
            if (p.z < m->bounds_min.z) m->bounds_min.z = p.z;
            if (p.x > m->bounds_max.x) m->bounds_max.x = p.x;
            if (p.y > m->bounds_max.y) m->bounds_max.y = p.y;
            if (p.z > m->bounds_max.z) m->bounds_max.z = p.z;
        }
    }
    return 1;
}

#endif /* PLATFORM_PC || PLATFORM_GC */
