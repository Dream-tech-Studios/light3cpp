/*
 * render_gl.c - desktop renderer using OpenGL fixed-function immediate mode.
 *
 * Deliberately uses only OpenGL 1.x style calls (matrix stack, glBegin/glEnd).
 * Why: no shaders, no VBOs, no GL function loader (GLAD/GLEW) needed, and it
 * maps almost 1:1 onto the GameCube's GX fixed-function pipeline. Works on
 * Windows (opengl32), Linux (libGL) and macOS (2.1 compatibility profile).
 */
#ifdef PLATFORM_PC

#include <stddef.h>

#if defined(__APPLE__)
  /* Fixed-function GL is deprecated on macOS but still fully functional;
     silence the per-call deprecation warnings. */
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl.h>
#else
  #include <GL/gl.h>
#endif

#include "render.h"
#include "font.h"

static int g_fb_w = 1, g_fb_h = 1;

void render_init(int fb_width, int fb_height) {
    g_fb_w = fb_width;
    g_fb_h = fb_height;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    /* Backface culling is intentionally left off: loaded OBJ meshes (e.g.
       Blender exports, which often flip winding when remapping to Y-up)
       can't be relied on to have consistent winding, and this game only
       ever draws a handful of small meshes, so the fillrate cost of
       drawing both sides is negligible. This keeps any future kart/track
       mesh "just work" without per-model winding fixes. */
    glDisable(GL_CULL_FACE);
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f); /* sky blue */
}

void render_shutdown(void) {
    /* nothing owned by the renderer yet */
}

void render_set_viewport(int fb_width, int fb_height) {
    g_fb_w = fb_width;
    g_fb_h = fb_height < 1 ? 1 : fb_height;
    glViewport(0, 0, g_fb_w, g_fb_h);
}

void render_begin_frame(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void render_end_frame(void) {
    /* buffer swap is done by the platform layer (SDL_GL_SwapWindow) */
}

void render_set_camera(Vec3 eye, Vec3 target, Vec3 up, float fov_deg) {
    float aspect = (float)g_fb_w / (float)g_fb_h;
    Mat4 proj = mat4_perspective(m3_deg2rad(fov_deg), aspect, 0.05f, 500.0f);
    Mat4 view = mat4_lookat(eye, target, up);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(&proj.m[0][0]);

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(&view.m[0][0]);
}

void render_draw_box(Vec3 pos, Vec3 he, float yaw,
                     float r, float g, float b) {
    glPushMatrix();
    {
        Mat4 model = mat4_mul(mat4_translate(pos), mat4_rotate_y(yaw));
        glMultMatrixf(&model.m[0][0]);

        glBegin(GL_QUADS);
        /* front (+Z) */
        glColor3f(r, g, b);
        glVertex3f(-he.x, -he.y,  he.z);
        glVertex3f( he.x, -he.y,  he.z);
        glVertex3f( he.x,  he.y,  he.z);
        glVertex3f(-he.x,  he.y,  he.z);
        /* back (-Z) */
        glColor3f(r*0.8f, g*0.8f, b*0.8f);
        glVertex3f( he.x, -he.y, -he.z);
        glVertex3f(-he.x, -he.y, -he.z);
        glVertex3f(-he.x,  he.y, -he.z);
        glVertex3f( he.x,  he.y, -he.z);
        /* left (-X) */
        glColor3f(r*0.7f, g*0.7f, b*0.7f);
        glVertex3f(-he.x, -he.y, -he.z);
        glVertex3f(-he.x, -he.y,  he.z);
        glVertex3f(-he.x,  he.y,  he.z);
        glVertex3f(-he.x,  he.y, -he.z);
        /* right (+X) */
        glColor3f(r*0.7f, g*0.7f, b*0.7f);
        glVertex3f( he.x, -he.y,  he.z);
        glVertex3f( he.x, -he.y, -he.z);
        glVertex3f( he.x,  he.y, -he.z);
        glVertex3f( he.x,  he.y,  he.z);
        /* top (+Y) */
        glColor3f(r*1.0f, g*1.0f, b*1.0f);
        glVertex3f(-he.x,  he.y,  he.z);
        glVertex3f( he.x,  he.y,  he.z);
        glVertex3f( he.x,  he.y, -he.z);
        glVertex3f(-he.x,  he.y, -he.z);
        /* bottom (-Y) */
        glColor3f(r*0.5f, g*0.5f, b*0.5f);
        glVertex3f(-he.x, -he.y, -he.z);
        glVertex3f( he.x, -he.y, -he.z);
        glVertex3f( he.x, -he.y,  he.z);
        glVertex3f(-he.x, -he.y,  he.z);
        glEnd();
    }
    glPopMatrix();
}

int render_texture_create(Texture *tex, const unsigned char *rgba, int width, int height) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    /* Some meshes (e.g. the bike) author UVs outside [0,1] expecting the
       texture to tile, so wrap rather than clamp. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, 0);

    tex->loaded  = 1;
    tex->width   = width;
    tex->height  = height;
    tex->gl_id   = id;
    tex->gx_handle = NULL;
    return 1;
}

void render_texture_destroy(Texture *tex) {
    if (!tex->loaded) return;
    glDeleteTextures(1, &tex->gl_id);
    tex->loaded = 0;
}

void render_draw_mesh(const Mesh *m, Vec3 pos, float yaw, float scale, const Texture *tex) {
    /* Light direction in world space for cheap per-vertex shading. */
    Vec3 L = vec3_norm(vec3(0.4f, 0.85f, 0.35f));
    float c = cosf(yaw), s = sinf(yaw);
    int textured = (tex && tex->loaded);
    int i;

    if (textured) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex->gl_id);
    }

    glPushMatrix();
    {
        Mat4 model = mat4_mul(mat4_translate(pos),
                     mat4_mul(mat4_rotate_y(yaw),
                              mat4_scale(vec3(scale, scale, scale))));
        glMultMatrixf(&model.m[0][0]);

        glBegin(GL_TRIANGLES);
        for (i = 0; i < m->vert_count; ++i) {
            const MeshVertex *v = &m->verts[i];
            const MeshMaterial *mt = &m->materials[v->mtl];
            /* rotate normal into world space (yaw only) for lighting */
            Vec3 n = v->normal;
            Vec3 wn = vec3(n.x * c + n.z * s, n.y, -n.x * s + n.z * c);
            float d = vec3_dot(wn, L);
            float b;
            if (d < 0.0f) d = 0.0f;
            b = 0.35f + 0.65f * d;

            if (textured) {
                /* GL_MODULATE (the default texture env mode) multiplies
                   the texel by this color, so a flat (b,b,b) reproduces
                   the same directional shading the untextured path uses. */
                glColor3f(b, b, b);
                glTexCoord2f(v->u, v->v);
            } else {
                glColor3f(mt->r * b, mt->g * b, mt->b * b);
            }
            glVertex3f(v->pos.x, v->pos.y, v->pos.z);
        }
        glEnd();
    }
    glPopMatrix();

    if (textured) glDisable(GL_TEXTURE_2D);
}

void render_draw_ground(float y, float half_size, float tile) {
    int nx, nz;
    float x, z;
    int n = (int)(half_size / tile);
    if (n < 1) n = 1;

    glBegin(GL_QUADS);
    for (nx = -n; nx < n; ++nx) {
        for (nz = -n; nz < n; ++nz) {
            int checker = ((nx + nz) & 1);
            if (checker) glColor3f(0.30f, 0.55f, 0.30f);
            else         glColor3f(0.26f, 0.48f, 0.26f);

            x = nx * tile;
            z = nz * tile;
            glVertex3f(x,        y, z + tile);
            glVertex3f(x + tile, y, z + tile);
            glVertex3f(x + tile, y, z);
            glVertex3f(x,        y, z);
        }
    }
    glEnd();
}

void render_begin_ui(void) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    /* top-left origin, +Y down, matches pixel/HUD-layout convention */
    glOrtho(0.0, g_fb_w, g_fb_h, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
}

void render_end_ui(void) {
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void render_draw_text(float x, float y, float scale, float r, float g, float b, const char *text) {
    float cx = x;
    const char *p;

    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    for (p = text; *p; ++p) {
        const unsigned char *rows = font_glyph_rows(*p);
        int row, col;
        for (row = 0; row < FONT_GLYPH_H; ++row) {
            for (col = 0; col < FONT_GLYPH_W; ++col) {
                float px, py;
                if (!((rows[row] >> (FONT_GLYPH_W - 1 - col)) & 1)) continue;
                px = cx + (float)col * scale;
                py = y + (float)row * scale;
                glVertex2f(px,         py);
                glVertex2f(px + scale, py);
                glVertex2f(px + scale, py + scale);
                glVertex2f(px,         py + scale);
            }
        }
        cx += (float)(FONT_GLYPH_W + 1) * scale;
    }
    glEnd();
}

#endif /* PLATFORM_PC */
