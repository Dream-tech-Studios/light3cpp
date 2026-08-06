/*
 * font.h - tiny built-in 5x7 bitmap font for the HUD.
 *
 * Header-only and self-contained (no texture, no font file) so both
 * renderers (render_gl.c, render_gx.c) can decode it into the same small
 * quads they already use for boxes/ground - no new rendering technique.
 *
 * Covers what the HUD and the select-screen carousel need: A-Z, 0-9, space,
 * ':' '/' '.' '!' plus '-' '#' '[' ']' '<' '>' (arrows + ASCII stat bars).
 * Unknown characters fall back to a blank space.
 */
#ifndef FONT_H
#define FONT_H

#define FONT_GLYPH_W 5
#define FONT_GLYPH_H 7

/* One row per byte; bit 4 = leftmost pixel of the row, bit 0 = rightmost. */
#define FONT_ROW(a, b, c, d, e) \
    (unsigned char)(((a) << 4) | ((b) << 3) | ((c) << 2) | ((d) << 1) | (e))

typedef struct {
    char          ch;
    unsigned char rows[FONT_GLYPH_H];
} FontGlyph;

static const FontGlyph g_font_glyphs[] = {
    { ' ', { FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0),
             FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0) } },
    { '!', { FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0),
             FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,1,0,0) } },
    { '.', { FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0),
             FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,1,0,0) } },
    { ':', { FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,0,0,0),
             FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,0,0,0) } },
    { '/', { FONT_ROW(0,0,0,0,1), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,1,0,0),
             FONT_ROW(0,1,0,0,0), FONT_ROW(0,1,0,0,0), FONT_ROW(1,0,0,0,0) } },
    { '-', { FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,1,1,1,0),
             FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0), FONT_ROW(0,0,0,0,0) } },
    { '#', { FONT_ROW(0,1,0,1,0), FONT_ROW(0,1,0,1,0), FONT_ROW(1,1,1,1,1), FONT_ROW(0,1,0,1,0),
             FONT_ROW(1,1,1,1,1), FONT_ROW(0,1,0,1,0), FONT_ROW(0,1,0,1,0) } },
    { '[', { FONT_ROW(0,1,1,1,0), FONT_ROW(0,1,0,0,0), FONT_ROW(0,1,0,0,0), FONT_ROW(0,1,0,0,0),
             FONT_ROW(0,1,0,0,0), FONT_ROW(0,1,0,0,0), FONT_ROW(0,1,1,1,0) } },
    { ']', { FONT_ROW(0,1,1,1,0), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,0,1,0),
             FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,0,1,0), FONT_ROW(0,1,1,1,0) } },
    { '<', { FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,1,0,0,0), FONT_ROW(1,0,0,0,0),
             FONT_ROW(0,1,0,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,0,1,0) } },
    { '>', { FONT_ROW(0,1,0,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,0,0,1),
             FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,1,0,0,0) } },

    { '0', { FONT_ROW(0,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,1,1), FONT_ROW(1,0,1,0,1),
             FONT_ROW(1,1,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,0) } },
    { '1', { FONT_ROW(0,0,1,0,0), FONT_ROW(0,1,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0),
             FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,1,1,1,0) } },
    { '2', { FONT_ROW(0,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(0,0,0,0,1), FONT_ROW(0,0,0,1,0),
             FONT_ROW(0,0,1,0,0), FONT_ROW(0,1,0,0,0), FONT_ROW(1,1,1,1,1) } },
    { '3', { FONT_ROW(1,1,1,1,1), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,0,1,0),
             FONT_ROW(0,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,0) } },
    { '4', { FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,1,1,0), FONT_ROW(0,1,0,1,0), FONT_ROW(1,0,0,1,0),
             FONT_ROW(1,1,1,1,1), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,0,1,0) } },
    { '5', { FONT_ROW(1,1,1,1,1), FONT_ROW(1,0,0,0,0), FONT_ROW(1,1,1,1,0), FONT_ROW(0,0,0,0,1),
             FONT_ROW(0,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,0) } },
    { '6', { FONT_ROW(0,0,1,1,0), FONT_ROW(0,1,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,1,1,1,0),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,0) } },
    { '7', { FONT_ROW(1,1,1,1,1), FONT_ROW(0,0,0,0,1), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,1,0,0),
             FONT_ROW(0,1,0,0,0), FONT_ROW(0,1,0,0,0), FONT_ROW(0,1,0,0,0) } },
    { '8', { FONT_ROW(0,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,0),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,0) } },
    { '9', { FONT_ROW(0,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,1),
             FONT_ROW(0,0,0,0,1), FONT_ROW(0,0,0,1,0), FONT_ROW(0,1,1,0,0) } },

    { 'A', { FONT_ROW(0,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,1,1,1,1),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1) } },
    { 'B', { FONT_ROW(1,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,1,1,1,0),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,1,1,1,0) } },
    { 'C', { FONT_ROW(0,1,1,1,1), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0),
             FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(0,1,1,1,1) } },
    { 'D', { FONT_ROW(1,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,1,1,1,0) } },
    { 'E', { FONT_ROW(1,1,1,1,1), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,1,1,1,0),
             FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,1,1,1,1) } },
    { 'F', { FONT_ROW(1,1,1,1,1), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,1,1,1,0),
             FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0) } },
    { 'G', { FONT_ROW(0,1,1,1,1), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,1,1,1),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,1) } },
    { 'H', { FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,1,1,1,1),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1) } },
    { 'I', { FONT_ROW(0,1,1,1,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0),
             FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,1,1,1,0) } },
    { 'J', { FONT_ROW(0,0,1,1,1), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,0,1,0),
             FONT_ROW(0,0,0,1,0), FONT_ROW(1,0,0,1,0), FONT_ROW(0,1,1,0,0) } },
    { 'K', { FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,1,0), FONT_ROW(1,0,1,0,0), FONT_ROW(1,1,0,0,0),
             FONT_ROW(1,0,1,0,0), FONT_ROW(1,0,0,1,0), FONT_ROW(1,0,0,0,1) } },
    { 'L', { FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0),
             FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,1,1,1,1) } },
    { 'M', { FONT_ROW(1,0,0,0,1), FONT_ROW(1,1,0,1,1), FONT_ROW(1,0,1,0,1), FONT_ROW(1,0,1,0,1),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1) } },
    { 'N', { FONT_ROW(1,0,0,0,1), FONT_ROW(1,1,0,0,1), FONT_ROW(1,0,1,0,1), FONT_ROW(1,0,1,0,1),
             FONT_ROW(1,0,0,1,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1) } },
    { 'O', { FONT_ROW(0,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,0) } },
    { 'P', { FONT_ROW(1,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,1,1,1,0),
             FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0) } },
    { 'Q', { FONT_ROW(0,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1),
             FONT_ROW(1,0,1,0,1), FONT_ROW(1,0,0,1,0), FONT_ROW(0,1,1,0,1) } },
    { 'R', { FONT_ROW(1,1,1,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,1,1,1,0),
             FONT_ROW(1,0,1,0,0), FONT_ROW(1,0,0,1,0), FONT_ROW(1,0,0,0,1) } },
    { 'S', { FONT_ROW(0,1,1,1,1), FONT_ROW(1,0,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(0,1,1,1,0),
             FONT_ROW(0,0,0,0,1), FONT_ROW(0,0,0,0,1), FONT_ROW(1,1,1,1,0) } },
    { 'T', { FONT_ROW(1,1,1,1,1), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0),
             FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0) } },
    { 'U', { FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1),
             FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,1,1,0) } },
    { 'V', { FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1),
             FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,0,1,0), FONT_ROW(0,0,1,0,0) } },
    { 'W', { FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,1,0,1),
             FONT_ROW(1,0,1,0,1), FONT_ROW(1,0,1,0,1), FONT_ROW(0,1,0,1,0) } },
    { 'X', { FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,0,1,0), FONT_ROW(0,0,1,0,0),
             FONT_ROW(0,1,0,1,0), FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1) } },
    { 'Y', { FONT_ROW(1,0,0,0,1), FONT_ROW(1,0,0,0,1), FONT_ROW(0,1,0,1,0), FONT_ROW(0,0,1,0,0),
             FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0), FONT_ROW(0,0,1,0,0) } },
    { 'Z', { FONT_ROW(1,1,1,1,1), FONT_ROW(0,0,0,0,1), FONT_ROW(0,0,0,1,0), FONT_ROW(0,0,1,0,0),
             FONT_ROW(0,1,0,0,0), FONT_ROW(1,0,0,0,0), FONT_ROW(1,1,1,1,1) } },
};

#define FONT_GLYPH_COUNT (int)(sizeof(g_font_glyphs) / sizeof(g_font_glyphs[0]))

/* Returns the 7-row bitmap for `ch` (case-insensitive), or a blank space
   if there's no glyph for it. */
static inline const unsigned char *font_glyph_rows(char ch) {
    int i;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    for (i = 0; i < FONT_GLYPH_COUNT; ++i) {
        if (g_font_glyphs[i].ch == ch) return g_font_glyphs[i].rows;
    }
    return g_font_glyphs[0].rows; /* space */
}

#endif /* FONT_H */
