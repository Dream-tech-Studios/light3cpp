/*
 * level_editor.c - a small standalone level-preview / block-out tool.
 *
 * This is NOT part of the game: it's a separate desktop GUI (its own main,
 * its own window) for roughing out a level and seeing "estimated" how it
 * would look - flat directional shading only, no textures/shaders needed.
 *
 * It reuses the game's own file loaders and math so a mesh looks here the
 * same way it will in-game:
 *   - math3d.h          camera / matrices (header-only)
 *   - font.h            the 5x7 HUD font (header-only)
 *   - obj_loader.c      .obj meshes           (compiled in via the Makefile)
 *   - dae_loader.c      .dae (COLLADA) meshes  (   "        "         "     )
 *   - image_load.c      pulled in only to satisfy the loaders' link deps
 * It does its OWN OpenGL drawing (it does not link render_gl.c) so it can
 * scale/rotate primitives freely and draw editor-only extras (grid, gizmo,
 * selection box, crosshair) without touching the shipping renderer.
 *
 * What you can do:
 *   - Fly around: WASD + mouse-look (hold RIGHT MOUSE), Q/E down/up,
 *     hold SHIFT to move faster.
 *   - Drop in geometry: press 1 = cube, 2 = slope, 3 = sphere. Each spawns
 *     at the ground cursor (the crosshair's hit on the y=0 plane), so aim
 *     the camera where you want it.
 *   - Drag & drop a .obj or .dae file from your file manager onto the
 *     window to place that model at the cursor (auto-scaled to a sane size).
 *   - Edit the selected object (the most recent, or TAB to cycle):
 *       arrows  move on X/Z        PageUp/PageDn move on Y
 *       [ / ]   rotate (yaw)       - / =  shrink / grow
 *       DELETE/BACKSPACE  remove   G  toggle grid snap on spawn
 *   - Press P to EXPORT: writes a real src/levels/level_editor_N.c + .h from
 *     the current layout (cubes -> level_add_platform, slopes/spheres ->
 *     level_add_slope/level_add_sphere, dropped models -> level_add_mesh) and
 *     auto-registers it in src/levels.c. Rebuild with `make` and it shows up
 *     in the level-select screen, fully playable (rendered + collidable).
 *     Run the editor from the project root so those paths resolve.
 *
 * Build:  make editor      (see the root Makefile)  ->  ./level_editor
 */
/* PLATFORM_PC is normally supplied by the Makefile (-DPLATFORM_PC); define
   it here too so the shared loaders still compile if this file is built by
   hand without that flag. */
#ifndef PLATFORM_PC
#define PLATFORM_PC
#endif

#include <SDL.h>

#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl.h>
#else
  #include <GL/gl.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "math3d.h"
#include "font.h"
#include "obj_loader.h"
#include "dae_loader.h"

/* ---------------------------------------------------------------------- */
/* Editor object model                                                     */
/* ---------------------------------------------------------------------- */

typedef enum { OBJ_CUBE, OBJ_SLOPE, OBJ_SPHERE, OBJ_MESH } ObjKind;

typedef struct {
    ObjKind kind;
    Vec3    pos;      /* world position of the object's local origin */
    float   yaw;      /* radians */
    Vec3    size;     /* half-extents (cube/slope) or radii (sphere) */
    float   scale;    /* uniform scale (OBJ_MESH only) */
    float   r, g, b;
    Mesh    mesh;     /* valid iff kind == OBJ_MESH */
    int     has_mesh;
    Vec3    lmin, lmax; /* local-space AABB, for the selection wire box */
    char    label[64];
    char    src_path[512]; /* OBJ_MESH: the file that was dropped in, for export */
} EObj;

#define MAX_OBJECTS 256

static EObj g_objs[MAX_OBJECTS];
static int  g_obj_count = 0;
static int  g_selected  = -1;

/* ---------------------------------------------------------------------- */
/* Camera                                                                  */
/* ---------------------------------------------------------------------- */

static Vec3  g_eye   = { 0.0f, 6.0f, 18.0f };
static float g_yaw   = 0.0f;   /* 0 => looking toward -Z */
static float g_pitch = -0.25f; /* looking slightly down */

static int   g_win_w = 1280, g_win_h = 720;
static int   g_grid_snap = 1;

static Vec3 cam_forward(void) {
    float cp = cosf(g_pitch), sp = sinf(g_pitch);
    return vec3(cp * sinf(g_yaw), sp, -cp * cosf(g_yaw));
}

/* Where the screen-center crosshair points on the ground plane y=0. If the
   camera is looking up (no ground hit), fall back to a point in front. */
static Vec3 ground_cursor(void) {
    Vec3 f = cam_forward();
    if (f.y < -0.0001f) {
        float t = -g_eye.y / f.y;
        if (t > 0.0f) return vec3(g_eye.x + f.x * t, 0.0f, g_eye.z + f.z * t);
    }
    return vec3(g_eye.x + f.x * 12.0f, 0.0f, g_eye.z + f.z * 12.0f);
}

static float snapf(float v, float step) {
    if (step <= 0.0f) return v;
    return step * floorf(v / step + 0.5f);
}

/* ---------------------------------------------------------------------- */
/* Shading (matches render_gl.c's cheap directional model)                 */
/* ---------------------------------------------------------------------- */

static void shaded_color(Vec3 n, float r, float g, float b) {
    Vec3 L = vec3_norm(vec3(0.4f, 0.85f, 0.35f));
    float d = vec3_dot(vec3_norm(n), L);
    float s;
    if (d < 0.0f) d = 0.0f;
    s = 0.35f + 0.65f * d;
    glColor3f(r * s, g * s, b * s);
}

static void push_model(Vec3 pos, float yaw) {
    Mat4 m = mat4_mul(mat4_translate(pos), mat4_rotate_y(yaw));
    glPushMatrix();
    glMultMatrixf(&m.m[0][0]);
}

/* ---------------------------------------------------------------------- */
/* Primitive drawing                                                       */
/* ---------------------------------------------------------------------- */

static void draw_cube(const EObj *o) {
    Vec3 h = o->size;
    push_model(o->pos, o->yaw);
    glBegin(GL_QUADS);
    shaded_color(vec3(0,0,1), o->r, o->g, o->b);
    glVertex3f(-h.x,-h.y, h.z); glVertex3f( h.x,-h.y, h.z); glVertex3f( h.x, h.y, h.z); glVertex3f(-h.x, h.y, h.z);
    shaded_color(vec3(0,0,-1), o->r, o->g, o->b);
    glVertex3f( h.x,-h.y,-h.z); glVertex3f(-h.x,-h.y,-h.z); glVertex3f(-h.x, h.y,-h.z); glVertex3f( h.x, h.y,-h.z);
    shaded_color(vec3(-1,0,0), o->r, o->g, o->b);
    glVertex3f(-h.x,-h.y,-h.z); glVertex3f(-h.x,-h.y, h.z); glVertex3f(-h.x, h.y, h.z); glVertex3f(-h.x, h.y,-h.z);
    shaded_color(vec3(1,0,0), o->r, o->g, o->b);
    glVertex3f( h.x,-h.y, h.z); glVertex3f( h.x,-h.y,-h.z); glVertex3f( h.x, h.y,-h.z); glVertex3f( h.x, h.y, h.z);
    shaded_color(vec3(0,1,0), o->r, o->g, o->b);
    glVertex3f(-h.x, h.y, h.z); glVertex3f( h.x, h.y, h.z); glVertex3f( h.x, h.y,-h.z); glVertex3f(-h.x, h.y,-h.z);
    shaded_color(vec3(0,-1,0), o->r, o->g, o->b);
    glVertex3f(-h.x,-h.y,-h.z); glVertex3f( h.x,-h.y,-h.z); glVertex3f( h.x,-h.y, h.z); glVertex3f(-h.x,-h.y, h.z);
    glEnd();
    glPopMatrix();
}

/* A right-triangular prism ramp: low at -Z, rising to full height at +Z. */
static void draw_slope(const EObj *o) {
    float hx = o->size.x, hy = o->size.y, hz = o->size.z;
    /* corners */
    Vec3 A = vec3(-hx,-hy,-hz), B = vec3( hx,-hy,-hz);   /* bottom back  */
    Vec3 C = vec3(-hx,-hy, hz), D = vec3( hx,-hy, hz);   /* bottom front */
    Vec3 E = vec3(-hx, hy, hz), F = vec3( hx, hy, hz);   /* top front    */
    Vec3 slope_n = vec3_norm(vec3(0.0f, 2.0f * hz, 2.0f * hy)); /* faces up/back */
    push_model(o->pos, o->yaw);
    glBegin(GL_TRIANGLES);
    /* slope face (A,B -> F,E) as two tris */
    shaded_color(slope_n, o->r, o->g, o->b);
    glVertex3f(A.x,A.y,A.z); glVertex3f(B.x,B.y,B.z); glVertex3f(F.x,F.y,F.z);
    glVertex3f(A.x,A.y,A.z); glVertex3f(F.x,F.y,F.z); glVertex3f(E.x,E.y,E.z);
    /* bottom */
    shaded_color(vec3(0,-1,0), o->r, o->g, o->b);
    glVertex3f(A.x,A.y,A.z); glVertex3f(C.x,C.y,C.z); glVertex3f(D.x,D.y,D.z);
    glVertex3f(A.x,A.y,A.z); glVertex3f(D.x,D.y,D.z); glVertex3f(B.x,B.y,B.z);
    /* front vertical face (C,D,F,E) */
    shaded_color(vec3(0,0,1), o->r, o->g, o->b);
    glVertex3f(C.x,C.y,C.z); glVertex3f(D.x,D.y,D.z); glVertex3f(F.x,F.y,F.z);
    glVertex3f(C.x,C.y,C.z); glVertex3f(F.x,F.y,F.z); glVertex3f(E.x,E.y,E.z);
    /* left triangle (A,C,E) */
    shaded_color(vec3(-1,0,0), o->r, o->g, o->b);
    glVertex3f(A.x,A.y,A.z); glVertex3f(C.x,C.y,C.z); glVertex3f(E.x,E.y,E.z);
    /* right triangle (B,F,D) */
    shaded_color(vec3(1,0,0), o->r, o->g, o->b);
    glVertex3f(B.x,B.y,B.z); glVertex3f(F.x,F.y,F.z); glVertex3f(D.x,D.y,D.z);
    glEnd();
    glPopMatrix();
}

#define SPHERE_LAT 12
#define SPHERE_LON 18

static void draw_sphere(const EObj *o) {
    int i, j;
    push_model(o->pos, o->yaw);
    glBegin(GL_QUADS);
    for (i = 0; i < SPHERE_LAT; ++i) {
        float t0 = M3_PI * ((float)i / SPHERE_LAT - 0.5f);
        float t1 = M3_PI * ((float)(i + 1) / SPHERE_LAT - 0.5f);
        for (j = 0; j < SPHERE_LON; ++j) {
            float p0 = 2.0f * M3_PI * ((float)j / SPHERE_LON);
            float p1 = 2.0f * M3_PI * ((float)(j + 1) / SPHERE_LON);
            /* unit-sphere directions (also serve as normals) */
            Vec3 n00 = vec3(cosf(t0)*cosf(p0), sinf(t0), cosf(t0)*sinf(p0));
            Vec3 n01 = vec3(cosf(t0)*cosf(p1), sinf(t0), cosf(t0)*sinf(p1));
            Vec3 n11 = vec3(cosf(t1)*cosf(p1), sinf(t1), cosf(t1)*sinf(p1));
            Vec3 n10 = vec3(cosf(t1)*cosf(p0), sinf(t1), cosf(t1)*sinf(p0));
            Vec3 avg = vec3_add(vec3_add(n00, n01), vec3_add(n11, n10));
            shaded_color(avg, o->r, o->g, o->b);
            glVertex3f(n00.x*o->size.x, n00.y*o->size.y, n00.z*o->size.z);
            glVertex3f(n01.x*o->size.x, n01.y*o->size.y, n01.z*o->size.z);
            glVertex3f(n11.x*o->size.x, n11.y*o->size.y, n11.z*o->size.z);
            glVertex3f(n10.x*o->size.x, n10.y*o->size.y, n10.z*o->size.z);
        }
    }
    glEnd();
    glPopMatrix();
}

static void draw_mesh_obj(const EObj *o) {
    const Mesh *m = &o->mesh;
    Vec3 L = vec3_norm(vec3(0.4f, 0.85f, 0.35f));
    float c = cosf(o->yaw), s = sinf(o->yaw);
    int i;
    Mat4 model = mat4_mul(mat4_translate(o->pos),
                 mat4_mul(mat4_rotate_y(o->yaw),
                          mat4_scale(vec3(o->scale, o->scale, o->scale))));
    glPushMatrix();
    glMultMatrixf(&model.m[0][0]);
    glBegin(GL_TRIANGLES);
    for (i = 0; i < m->vert_count; ++i) {
        const MeshVertex *v = &m->verts[i];
        const MeshMaterial *mt = &m->materials[v->mtl];
        Vec3 n = v->normal;
        Vec3 wn = vec3(n.x * c + n.z * s, n.y, -n.x * s + n.z * c);
        float d = vec3_dot(vec3_norm(wn), L);
        float b;
        if (d < 0.0f) d = 0.0f;
        b = 0.35f + 0.65f * d;
        glColor3f(mt->r * b, mt->g * b, mt->b * b);
        glVertex3f(v->pos.x, v->pos.y, v->pos.z);
    }
    glEnd();
    glPopMatrix();
}

static void draw_object(const EObj *o) {
    switch (o->kind) {
    case OBJ_CUBE:   draw_cube(o);   break;
    case OBJ_SLOPE:  draw_slope(o);  break;
    case OBJ_SPHERE: draw_sphere(o); break;
    case OBJ_MESH:   draw_mesh_obj(o); break;
    }
}

/* ---------------------------------------------------------------------- */
/* Editor-only visuals: grid, axis gizmo, selection box, ground cursor     */
/* ---------------------------------------------------------------------- */

static void draw_grid(void) {
    int i;
    int n = 40;
    float step = 1.0f;
    float ext = n * step;
    glBegin(GL_LINES);
    for (i = -n; i <= n; ++i) {
        float p = i * step;
        if (i == 0) glColor3f(0.5f, 0.5f, 0.55f);
        else        glColor3f(0.32f, 0.36f, 0.34f);
        glVertex3f(p, 0.0f, -ext); glVertex3f(p, 0.0f, ext);
        glVertex3f(-ext, 0.0f, p); glVertex3f(ext, 0.0f, p);
    }
    glEnd();

    /* axis gizmo at origin */
    glBegin(GL_LINES);
    glColor3f(0.9f, 0.2f, 0.2f); glVertex3f(0,0.02f,0); glVertex3f(3,0.02f,0); /* +X */
    glColor3f(0.2f, 0.9f, 0.2f); glVertex3f(0,0.02f,0); glVertex3f(0,3,0);     /* +Y */
    glColor3f(0.3f, 0.5f, 1.0f); glVertex3f(0,0.02f,0); glVertex3f(0,0.02f,3); /* +Z */
    glEnd();
}

static void wire_box(Vec3 pos, float yaw, Vec3 lmin, Vec3 lmax, float r, float g, float b) {
    float x0 = lmin.x, y0 = lmin.y, z0 = lmin.z;
    float x1 = lmax.x, y1 = lmax.y, z1 = lmax.z;
    push_model(pos, yaw);
    glColor3f(r, g, b);
    glBegin(GL_LINES);
    /* bottom rectangle */
    glVertex3f(x0,y0,z0); glVertex3f(x1,y0,z0);
    glVertex3f(x1,y0,z0); glVertex3f(x1,y0,z1);
    glVertex3f(x1,y0,z1); glVertex3f(x0,y0,z1);
    glVertex3f(x0,y0,z1); glVertex3f(x0,y0,z0);
    /* top rectangle */
    glVertex3f(x0,y1,z0); glVertex3f(x1,y1,z0);
    glVertex3f(x1,y1,z0); glVertex3f(x1,y1,z1);
    glVertex3f(x1,y1,z1); glVertex3f(x0,y1,z1);
    glVertex3f(x0,y1,z1); glVertex3f(x0,y1,z0);
    /* verticals */
    glVertex3f(x0,y0,z0); glVertex3f(x0,y1,z0);
    glVertex3f(x1,y0,z0); glVertex3f(x1,y1,z0);
    glVertex3f(x1,y0,z1); glVertex3f(x1,y1,z1);
    glVertex3f(x0,y0,z1); glVertex3f(x0,y1,z1);
    glEnd();
    glPopMatrix();
}

static void draw_ground_cursor(Vec3 c) {
    float s = 0.6f;
    glColor3f(1.0f, 0.9f, 0.2f);
    glBegin(GL_LINES);
    glVertex3f(c.x - s, 0.03f, c.z); glVertex3f(c.x + s, 0.03f, c.z);
    glVertex3f(c.x, 0.03f, c.z - s); glVertex3f(c.x, 0.03f, c.z + s);
    glEnd();
}

/* ---------------------------------------------------------------------- */
/* 2D overlay (crosshair + text), reusing font.h                           */
/* ---------------------------------------------------------------------- */

static void ui_begin(void) {
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0.0, g_win_w, g_win_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
}

static void ui_end(void) {
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}

static void ui_text(float x, float y, float scale, float r, float g, float b, const char *text) {
    const char *p;
    float cx = x;
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    for (p = text; *p; ++p) {
        const unsigned char *rows = font_glyph_rows(*p);
        int row, col;
        for (row = 0; row < FONT_GLYPH_H; ++row)
            for (col = 0; col < FONT_GLYPH_W; ++col) {
                float px, py;
                if (!((rows[row] >> (FONT_GLYPH_W - 1 - col)) & 1)) continue;
                px = cx + col * scale; py = y + row * scale;
                glVertex2f(px, py); glVertex2f(px + scale, py);
                glVertex2f(px + scale, py + scale); glVertex2f(px, py + scale);
            }
        cx += (FONT_GLYPH_W + 1) * scale;
    }
    glEnd();
}

static void draw_crosshair(void) {
    float cx = g_win_w * 0.5f, cy = g_win_h * 0.5f, s = 8.0f;
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(cx - s, cy); glVertex2f(cx + s, cy);
    glVertex2f(cx, cy - s); glVertex2f(cx, cy + s);
    glEnd();
}

static const char *kind_name(ObjKind k) {
    switch (k) {
    case OBJ_CUBE:   return "CUBE";
    case OBJ_SLOPE:  return "SLOPE";
    case OBJ_SPHERE: return "SPHERE";
    case OBJ_MESH:   return "MESH";
    }
    return "?";
}

static void draw_hud(void) {
    char line[128];
    float y = 10.0f;
    const float sc = 2.0f;
    const float lh = FONT_GLYPH_H * sc + 5.0f;

    ui_begin();
    draw_crosshair();

    ui_text(10, y, sc, 1,1,0.3f, "PLATFORMER LEVEL EDITOR"); y += lh;
    snprintf(line, sizeof(line), "OBJECTS %d/%d   GRID SNAP %s",
             g_obj_count, MAX_OBJECTS, g_grid_snap ? "ON" : "OFF");
    ui_text(10, y, sc, 1,1,1, line); y += lh;

    if (g_selected >= 0) {
        const EObj *o = &g_objs[g_selected];
        snprintf(line, sizeof(line), "SEL %s  POS %.1f %.1f %.1f",
                 kind_name(o->kind), o->pos.x, o->pos.y, o->pos.z);
        ui_text(10, y, sc, 0.6f,1,0.6f, line); y += lh;
    } else {
        ui_text(10, y, sc, 0.7f,0.7f,0.7f, "SEL NONE"); y += lh;
    }

    /* controls, bottom-left */
    {
        float by = g_win_h - lh * 6.0f;
        ui_text(10, by, 1.6f, 0.8f,0.8f,0.9f, "RMOUSE LOOK   WASD MOVE   Q/E DOWN/UP   SHIFT FAST"); by += lh;
        ui_text(10, by, 1.6f, 0.8f,0.8f,0.9f, "1 CUBE   2 SLOPE   3 SPHERE   DRAG IN .OBJ / .DAE"); by += lh;
        ui_text(10, by, 1.6f, 0.8f,0.8f,0.9f, "ARROWS MOVE XZ   PGUP/PGDN MOVE Y   [ ] ROTATE"); by += lh;
        ui_text(10, by, 1.6f, 0.8f,0.8f,0.9f, "- = SCALE   TAB SELECT   DEL REMOVE   G SNAP"); by += lh;
        ui_text(10, by, 1.6f, 0.8f,0.8f,0.9f, "P WRITE + REGISTER NEW LEVEL FILE"); by += lh;
    }
    ui_end();
}

/* ---------------------------------------------------------------------- */
/* Object creation / editing                                               */
/* ---------------------------------------------------------------------- */

static void recompute_aabb(EObj *o) {
    switch (o->kind) {
    case OBJ_CUBE:
    case OBJ_SLOPE:
    case OBJ_SPHERE:
        o->lmin = vec3(-o->size.x, -o->size.y, -o->size.z);
        o->lmax = vec3( o->size.x,  o->size.y,  o->size.z);
        break;
    case OBJ_MESH: {
        Vec3 mn = o->mesh.bounds_min, mx = o->mesh.bounds_max;
        o->lmin = vec3_scale(mn, o->scale);
        o->lmax = vec3_scale(mx, o->scale);
        break;
    }
    }
}

static EObj *new_object(void) {
    EObj *o;
    if (g_obj_count >= MAX_OBJECTS) {
        fprintf(stderr, "[editor] object limit (%d) reached\n", MAX_OBJECTS);
        return NULL;
    }
    o = &g_objs[g_obj_count];
    memset(o, 0, sizeof(*o));
    g_selected = g_obj_count;
    ++g_obj_count;
    return o;
}

static Vec3 spawn_point(void) {
    Vec3 c = ground_cursor();
    if (g_grid_snap) { c.x = snapf(c.x, 0.5f); c.z = snapf(c.z, 0.5f); }
    return c;
}

static void spawn_primitive(ObjKind kind) {
    EObj *o = new_object();
    Vec3 c;
    if (!o) return;
    c = spawn_point();
    o->kind  = kind;
    o->yaw   = 0.0f;
    o->scale = 1.0f;
    o->r = 0.45f; o->g = 0.62f; o->b = 0.85f;
    switch (kind) {
    case OBJ_CUBE:   o->size = vec3(1.5f, 0.6f, 1.5f); break;
    case OBJ_SLOPE:  o->size = vec3(1.5f, 1.0f, 1.5f); o->r = 0.85f; o->g = 0.6f; o->b = 0.35f; break;
    case OBJ_SPHERE: o->size = vec3(1.0f, 1.0f, 1.0f); o->r = 0.85f; o->g = 0.35f; o->b = 0.5f; break;
    case OBJ_MESH:   break; /* not reached */
    }
    o->pos = vec3(c.x, o->size.y, c.z); /* rest on the ground */
    snprintf(o->label, sizeof(o->label), "%s", kind_name(kind));
    recompute_aabb(o);
}

static int has_suffix_ci(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    size_t i;
    if (ls < lf) return 0;
    for (i = 0; i < lf; ++i) {
        char a = s[ls - lf + i], sfx = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != sfx) return 0;
    }
    return 1;
}

static void spawn_mesh_from_file(const char *path) {
    EObj *o;
    Mesh m;
    int ok;
    Vec3 c;
    float maxd;

    if (has_suffix_ci(path, ".dae"))      ok = mesh_load_dae(&m, path);
    else if (has_suffix_ci(path, ".obj")) ok = mesh_load_obj(&m, path);
    else {
        fprintf(stderr, "[editor] dropped file is not .obj/.dae: %s\n", path);
        return;
    }
    if (!ok) {
        fprintf(stderr, "[editor] failed to load mesh: %s\n", path);
        return;
    }
    mesh_recenter_to_ground(&m);

    o = new_object();
    if (!o) { mesh_free(&m); return; }
    o->kind     = OBJ_MESH;
    o->mesh     = m;
    o->has_mesh = 1;
    o->yaw      = 0.0f;
    o->r = o->g = o->b = 1.0f; /* meshes use their own material colors */

    maxd = mesh_max_dimension(&m);
    o->scale = (maxd > 1e-4f) ? (3.0f / maxd) : 1.0f; /* normalize to ~3 units */

    c = spawn_point();
    o->pos = vec3(c.x, 0.0f, c.z); /* recentered mesh sits with base on ground */

    snprintf(o->src_path, sizeof(o->src_path), "%s", path);
    /* label = basename */
    {
        const char *base = path, *p;
        for (p = path; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
        snprintf(o->label, sizeof(o->label), "%s", base);
    }
    recompute_aabb(o);
    fprintf(stderr, "[editor] placed \"%s\" (%d tris, scale %.3f)\n",
            o->label, o->mesh.vert_count / 3, o->scale);
}

static void delete_selected(void) {
    int i;
    if (g_selected < 0) return;
    if (g_objs[g_selected].has_mesh) mesh_free(&g_objs[g_selected].mesh);
    for (i = g_selected; i < g_obj_count - 1; ++i) g_objs[i] = g_objs[i + 1];
    --g_obj_count;
    if (g_obj_count == 0) g_selected = -1;
    else if (g_selected >= g_obj_count) g_selected = g_obj_count - 1;
}

/* ---------------------------------------------------------------------- */
/* Export: write a real src/levels/levelNN_*.c + .h and register it in       */
/* src/levels.c, so the layout becomes a playable, buildable level.          */
/* (Run the editor from the project root - `make run-editor` - so these      */
/*  relative paths resolve.)                                                 */
/* ---------------------------------------------------------------------- */

/* Turn a dropped model's absolute path into an asset-relative one the level
   loader can open (ASSET_ROOT + this). If the file already lives under the
   project's assets/, reuse that; otherwise assume assets/props/<basename>
   and flag it so we can warn the user to copy the file there. */
static void asset_rel_path(const char *abs, char *out, size_t n, int *needs_copy) {
    const char *m = strstr(abs, "/assets/");
    if (m) { snprintf(out, n, "%s", m + 1); *needs_copy = 0; return; }
    {
        const char *base = abs, *p;
        for (p = abs; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
        snprintf(out, n, "assets/props/%s", base);
        *needs_copy = 1;
    }
}

static char *read_whole_file(const char *path) {
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    buf[n] = '\0';
    fclose(f);
    return buf;
}

/* Return a new buffer with `ins` inserted at the start of the line that
   contains `marker`; NULL if the marker isn't present. */
static char *insert_before_marker(const char *buf, const char *marker, const char *ins) {
    const char *m = strstr(buf, marker);
    const char *ls;
    size_t pre, inslen, taillen;
    char *out;
    if (!m) return NULL;
    ls = m;
    while (ls > buf && ls[-1] != '\n') --ls; /* back up to start of marker's line */
    pre = (size_t)(ls - buf);
    inslen = strlen(ins);
    taillen = strlen(ls);
    out = (char *)malloc(pre + inslen + taillen + 1);
    if (!out) return NULL;
    memcpy(out, buf, pre);
    memcpy(out + pre, ins, inslen);
    memcpy(out + pre + inslen, ls, taillen + 1);
    return out;
}

static int register_in_levels_c(const char *slug, const char *func) {
    const char *path = "src/levels.c";
    char inc[256], reg[256];
    char *buf, *b1, *b2;
    FILE *f;

    buf = read_whole_file(path);
    if (!buf) { fprintf(stderr, "[editor] could not read %s (run from project root)\n", path); return 0; }

    snprintf(inc, sizeof(inc), "#include \"levels/%s.h\"\n", slug);
    snprintf(reg, sizeof(reg), "    { %s },\n", func);

    b1 = insert_before_marker(buf, "@editor-includes", inc);
    free(buf);
    if (!b1) { fprintf(stderr, "[editor] marker @editor-includes missing in %s; add the entry manually\n", path); return 0; }
    b2 = insert_before_marker(b1, "@editor-registry", reg);
    free(b1);
    if (!b2) { fprintf(stderr, "[editor] marker @editor-registry missing in %s; add the entry manually\n", path); return 0; }

    f = fopen(path, "wb");
    if (!f) { free(b2); fprintf(stderr, "[editor] could not write %s\n", path); return 0; }
    fputs(b2, f);
    fclose(f);
    free(b2);
    return 1;
}

static int next_level_index(void) {
    int k;
    for (k = 1; k < 1000; ++k) {
        char p[256];
        FILE *f;
        snprintf(p, sizeof(p), "src/levels/level_editor_%d.c", k);
        f = fopen(p, "rb");
        if (!f) return k;
        fclose(f);
    }
    return -1;
}

static void write_body(FILE *c) {
    int i, first_cube = -1, last_cube = -1;
    for (i = 0; i < g_obj_count; ++i)
        if (g_objs[i].kind == OBJ_CUBE) { if (first_cube < 0) first_cube = i; last_cube = i; }

    for (i = 0; i < g_obj_count; ++i) {
        const EObj *o = &g_objs[i];
        switch (o->kind) {
        case OBJ_CUBE:
            /* the level system's platform center.y is the WALKABLE TOP */
            fprintf(c, "    level_add_platform(lvl, vec3(%.3ff, %.3ff, %.3ff), %.3ff, %.3ff, %.3ff);\n",
                    o->pos.x, o->pos.y + o->size.y, o->pos.z, o->size.x, o->size.z, o->size.y);
            break;
        case OBJ_SLOPE:
            fprintf(c, "    level_add_slope(lvl, vec3(%.3ff, %.3ff, %.3ff), %.4ff, vec3(%.3ff, %.3ff, %.3ff), %.2ff, %.2ff, %.2ff);\n",
                    o->pos.x, o->pos.y, o->pos.z, o->yaw, o->size.x, o->size.y, o->size.z, o->r, o->g, o->b);
            break;
        case OBJ_SPHERE:
            fprintf(c, "    level_add_sphere(lvl, vec3(%.3ff, %.3ff, %.3ff), %.4ff, vec3(%.3ff, %.3ff, %.3ff), %.2ff, %.2ff, %.2ff);\n",
                    o->pos.x, o->pos.y, o->pos.z, o->yaw, o->size.x, o->size.y, o->size.z, o->r, o->g, o->b);
            break;
        case OBJ_MESH: {
            char rel[256];
            int needs_copy = 0;
            asset_rel_path(o->src_path, rel, sizeof(rel), &needs_copy);
            if (needs_copy)
                fprintf(c, "    /* NOTE: copy \"%s\" to assets/props/ for this to load */\n", o->src_path);
            fprintf(c, "    level_add_mesh(lvl, vec3(%.3ff, %.3ff, %.3ff), %.4ff, %.4ff, \"%s\");\n",
                    o->pos.x, o->pos.y, o->pos.z, o->yaw, o->scale, rel);
            break;
        }
        }
    }

    /* Start on the first platform, goal above the last (best-effort defaults;
       tweak by hand afterwards). */
    if (first_cube >= 0) {
        const EObj *s = &g_objs[first_cube];
        const EObj *e = &g_objs[last_cube];
        fprintf(c, "\n    level_set_start(lvl, vec3(%.3ff, %.3ff, %.3ff), 0.0f);\n",
                s->pos.x, s->pos.y + s->size.y, s->pos.z);
        fprintf(c, "    level_set_goal(lvl, vec3(%.3ff, %.3ff, %.3ff), 3.5f);\n",
                e->pos.x, e->pos.y + e->size.y + 1.0f, e->pos.z);
    } else {
        fprintf(c, "\n    level_set_start(lvl, vec3(0.0f, 0.0f, 0.0f), 0.0f);\n");
        fprintf(c, "    level_set_goal(lvl, vec3(0.0f, 1.0f, 0.0f), 3.5f);\n");
    }
}

static void export_to_files(void) {
    int k = next_level_index();
    char slug[64], func[64], guard[80], cpath[256], hpath[256];
    FILE *c, *h;

    if (k < 0) { fprintf(stderr, "[editor] too many editor levels already exist\n"); return; }

    snprintf(slug, sizeof(slug), "level_editor_%d", k);
    snprintf(func, sizeof(func), "level_editor_%d_build", k);
    snprintf(guard, sizeof(guard), "LEVEL_EDITOR_%d_H", k);
    snprintf(cpath, sizeof(cpath), "src/levels/%s.c", slug);
    snprintf(hpath, sizeof(hpath), "src/levels/%s.h", slug);

    h = fopen(hpath, "wb");
    if (!h) { fprintf(stderr, "[editor] could not create %s (run from project root)\n", hpath); return; }
    fprintf(h, "/* Auto-generated by tools/level_editor.c - safe to edit or rename. */\n");
    fprintf(h, "#ifndef %s\n#define %s\n\n#include \"level.h\"\n\n", guard, guard);
    fprintf(h, "void %s(Level *lvl);\n\n#endif /* %s */\n", func, guard);
    fclose(h);

    c = fopen(cpath, "wb");
    if (!c) { fprintf(stderr, "[editor] could not create %s\n", cpath); return; }
    fprintf(c, "/* Auto-generated by tools/level_editor.c - safe to edit or rename. */\n");
    fprintf(c, "#include \"%s.h\"\n\n", slug);
    fprintf(c, "void %s(Level *lvl) {\n", func);
    fprintf(c, "    level_begin(lvl, \"Editor Level %d\");\n\n", k);
    write_body(c);
    fprintf(c, "}\n");
    fclose(c);

    fprintf(stderr, "[editor] wrote %s and %s (%d objects)\n", cpath, hpath, g_obj_count);

    if (register_in_levels_c(slug, func))
        fprintf(stderr, "[editor] registered %s in src/levels.c - rebuild with `make` to play it\n", func);
    else
        fprintf(stderr, "[editor] add  #include \"levels/%s.h\"  and  { %s }  to src/levels.c yourself\n",
                slug, func);
}

/* ---------------------------------------------------------------------- */
/* Per-frame continuous input (held keys)                                  */
/* ---------------------------------------------------------------------- */

static void update_continuous(const Uint8 *ks, float dt) {
    Vec3 f = cam_forward();
    Vec3 up = vec3(0, 1, 0);
    Vec3 right = vec3_norm(vec3_cross(f, up));
    float speed = (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]) ? 24.0f : 9.0f;
    Vec3 move = vec3_zero();

    if (ks[SDL_SCANCODE_W]) move = vec3_add(move, f);
    if (ks[SDL_SCANCODE_S]) move = vec3_sub(move, f);
    if (ks[SDL_SCANCODE_D]) move = vec3_add(move, right);
    if (ks[SDL_SCANCODE_A]) move = vec3_sub(move, right);
    if (ks[SDL_SCANCODE_E]) move = vec3_add(move, up);
    if (ks[SDL_SCANCODE_Q]) move = vec3_sub(move, up);
    if (vec3_len(move) > 1e-4f)
        g_eye = vec3_add(g_eye, vec3_scale(vec3_norm(move), speed * dt));

    /* selected-object transform */
    if (g_selected >= 0) {
        EObj *o = &g_objs[g_selected];
        float mv = (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]) ? 8.0f : 3.0f;
        int changed = 0;
        if (ks[SDL_SCANCODE_LEFT])  { o->pos.x -= mv * dt; changed = 1; }
        if (ks[SDL_SCANCODE_RIGHT]) { o->pos.x += mv * dt; changed = 1; }
        if (ks[SDL_SCANCODE_UP])    { o->pos.z -= mv * dt; changed = 1; }
        if (ks[SDL_SCANCODE_DOWN])  { o->pos.z += mv * dt; changed = 1; }
        if (ks[SDL_SCANCODE_PAGEUP])   { o->pos.y += mv * dt; }
        if (ks[SDL_SCANCODE_PAGEDOWN]) { o->pos.y -= mv * dt; }
        if (ks[SDL_SCANCODE_LEFTBRACKET])  o->yaw -= 1.6f * dt;
        if (ks[SDL_SCANCODE_RIGHTBRACKET]) o->yaw += 1.6f * dt;
        if (ks[SDL_SCANCODE_MINUS]) {
            float k = 1.0f - 0.9f * dt;
            if (o->kind == OBJ_MESH) o->scale *= k; else o->size = vec3_scale(o->size, k);
            recompute_aabb(o);
        }
        if (ks[SDL_SCANCODE_EQUALS]) {
            float k = 1.0f + 0.9f * dt;
            if (o->kind == OBJ_MESH) o->scale *= k; else o->size = vec3_scale(o->size, k);
            recompute_aabb(o);
        }
        (void)changed;
    }
}

/* ---------------------------------------------------------------------- */
/* Main                                                                    */
/* ---------------------------------------------------------------------- */

int main(int argc, char **argv) {
    SDL_Window *win;
    SDL_GLContext ctx;
    Uint64 prev, freq;
    int running = 1;
    int looking = 0;
    int i;

    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    win = SDL_CreateWindow("Platformer Level Editor",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           g_win_w, g_win_h,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!win) { SDL_Log("CreateWindow failed: %s", SDL_GetError()); SDL_Quit(); return 1; }

    ctx = SDL_GL_CreateContext(win);
    if (!ctx) { SDL_Log("GL context failed: %s", SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }
    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);

    freq = SDL_GetPerformanceFrequency();
    prev = SDL_GetPerformanceCounter();

    while (running) {
        SDL_Event e;
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((double)(now - prev) / (double)freq);
        const Uint8 *ks;
        prev = now;
        if (dt > 0.1f) dt = 0.1f;

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    SDL_GL_GetDrawableSize(win, &g_win_w, &g_win_h);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_RIGHT) {
                    looking = 1;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_RIGHT) {
                    looking = 0;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                break;
            case SDL_MOUSEMOTION:
                if (looking) {
                    g_yaw   += e.motion.xrel * 0.0032f;
                    g_pitch -= e.motion.yrel * 0.0032f;
                    g_pitch = m3_clampf(g_pitch, -1.55f, 1.55f);
                }
                break;
            case SDL_DROPFILE:
                if (e.drop.file) {
                    spawn_mesh_from_file(e.drop.file);
                    SDL_free(e.drop.file);
                }
                break;
            case SDL_KEYDOWN:
                if (e.key.repeat) break;
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: running = 0; break;
                case SDLK_1: spawn_primitive(OBJ_CUBE);   break;
                case SDLK_2: spawn_primitive(OBJ_SLOPE);  break;
                case SDLK_3: spawn_primitive(OBJ_SPHERE); break;
                case SDLK_TAB:
                    if (g_obj_count > 0)
                        g_selected = (g_selected + 1) % g_obj_count;
                    break;
                case SDLK_DELETE:
                case SDLK_BACKSPACE:
                    delete_selected();
                    break;
                case SDLK_g:
                    g_grid_snap = !g_grid_snap;
                    break;
                case SDLK_p:
                    export_to_files();
                    break;
                default: break;
                }
                break;
            default: break;
            }
        }

        ks = SDL_GetKeyboardState(NULL);
        update_continuous(ks, dt);

        /* ---- render ---- */
        glViewport(0, 0, g_win_w, g_win_h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
            float aspect = (float)g_win_w / (float)(g_win_h < 1 ? 1 : g_win_h);
            Mat4 proj = mat4_perspective(m3_deg2rad(60.0f), aspect, 0.05f, 1000.0f);
            Vec3 f = cam_forward();
            Mat4 view = mat4_lookat(g_eye, vec3_add(g_eye, f), vec3(0, 1, 0));
            glMatrixMode(GL_PROJECTION); glLoadMatrixf(&proj.m[0][0]);
            glMatrixMode(GL_MODELVIEW);  glLoadMatrixf(&view.m[0][0]);
        }

        draw_grid();
        draw_ground_cursor(ground_cursor());
        for (i = 0; i < g_obj_count; ++i) draw_object(&g_objs[i]);
        if (g_selected >= 0) {
            EObj *o = &g_objs[g_selected];
            wire_box(o->pos, o->yaw, o->lmin, o->lmax, 1.0f, 0.95f, 0.2f);
        }

        draw_hud();

        SDL_GL_SwapWindow(win);
    }

    for (i = 0; i < g_obj_count; ++i)
        if (g_objs[i].has_mesh) mesh_free(&g_objs[i].mesh);

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
