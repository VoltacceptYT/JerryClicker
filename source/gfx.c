#include "gfx.h"

#include <string.h>

void gfx_begin(Canvas *c, px *buf) {
    c->buf = buf;
    gfx_unclip(c);
}

void gfx_unclip(Canvas *c) {
    c->cx0 = 0;
    c->cy0 = 0;
    c->cx1 = SCREEN_W;
    c->cy1 = SCREEN_H;
}

void gfx_clip(Canvas *c, int x, int y, int w, int h) {
    c->cx0 = x < 0 ? 0 : x;
    c->cy0 = y < 0 ? 0 : y;
    c->cx1 = x + w > SCREEN_W ? SCREEN_W : x + w;
    c->cy1 = y + h > SCREEN_H ? SCREEN_H : y + h;
}

static inline void put(Canvas *c, int x, int y, px v) {
    if (x >= c->cx0 && x < c->cx1 && y >= c->cy0 && y < c->cy1) c->buf[y * SCREEN_W + x] = v;
}

void gfx_fill(Canvas *c, int x, int y, int w, int h, px color) {
    int x0 = x < c->cx0 ? c->cx0 : x;
    int y0 = y < c->cy0 ? c->cy0 : y;
    int x1 = x + w > c->cx1 ? c->cx1 : x + w;
    int y1 = y + h > c->cy1 ? c->cy1 : y + h;
    for (int yy = y0; yy < y1; yy++) {
        px *row = c->buf + yy * SCREEN_W;
        for (int xx = x0; xx < x1; xx++) row[xx] = color;
    }
}

void gfx_frame(Canvas *c, int x, int y, int w, int h, px color) {
    gfx_fill(c, x, y, w, 1, color);
    gfx_fill(c, x, y + h - 1, w, 1, color);
    gfx_fill(c, x, y, 1, h, color);
    gfx_fill(c, x + w - 1, y, 1, h, color);
}

// Minecraft-style bevelled panel (light top/left, dark bottom/right).
void gfx_bevel_panel(Canvas *c, int x, int y, int w, int h, px fill, px light, px dark) {
    gfx_fill(c, x, y, w, h, fill);
    gfx_fill(c, x, y, w, 1, light);
    gfx_fill(c, x, y, 1, h, light);
    gfx_fill(c, x, y + h - 1, w, 1, dark);
    gfx_fill(c, x + w - 1, y, 1, h, dark);
}

void gfx_tile(Canvas *c, const uint16_t *tex, int x, int y, int w, int h) {
    int x0 = x < c->cx0 ? c->cx0 : x;
    int y0 = y < c->cy0 ? c->cy0 : y;
    int x1 = x + w > c->cx1 ? c->cx1 : x + w;
    int y1 = y + h > c->cy1 ? c->cy1 : y + h;
    for (int yy = y0; yy < y1; yy++) {
        px *row = c->buf + yy * SCREEN_W;
        const uint16_t *trow = tex + (((yy - y) & 15) << 4);
        for (int xx = x0; xx < x1; xx++) row[xx] = trow[(xx - x) & 15] | 0x8000;
    }
}

// Stretches a 16x16 texture with a 3px border (like CSS border-image-slice: 3).
void gfx_nineslice(Canvas *c, const uint16_t *tex, int x, int y, int w, int h) {
    const int S = 3;
    for (int yy = 0; yy < h; yy++) {
        int sy = yy < S ? yy : (yy >= h - S ? 16 - (h - yy) : S + ((yy - S) % (16 - 2 * S)));
        for (int xx = 0; xx < w; xx++) {
            int sx = xx < S ? xx : (xx >= w - S ? 16 - (w - xx) : S + ((xx - S) % (16 - 2 * S)));
            put(c, x + xx, y + yy, tex[sy * 16 + sx] | 0x8000);
        }
    }
}

// 16x16 blit; transparent pixels (bit 15 clear) are skipped. half=1 blends 50% with what is underneath.
void gfx_blit16(Canvas *c, const uint16_t *tex, int x, int y, int half) {
    for (int yy = 0; yy < 16; yy++) {
        int py = y + yy;
        if (py < c->cy0 || py >= c->cy1) continue;
        for (int xx = 0; xx < 16; xx++) {
            int pxx = x + xx;
            if (pxx < c->cx0 || pxx >= c->cx1) continue;
            uint16_t v = tex[yy * 16 + xx];
            if (!(v & 0x8000)) continue;
            if (half) {
                uint16_t d = c->buf[py * SCREEN_W + pxx];
                v = 0x8000 | ((((v & 0x7C00) + (d & 0x7C00)) >> 1) & 0x7C00) |
                    ((((v & 0x03E0) + (d & 0x03E0)) >> 1) & 0x03E0) | ((((v & 0x001F) + (d & 0x001F)) >> 1) & 0x001F);
            }
            c->buf[py * SCREEN_W + pxx] = v;
        }
    }
}

/* ---------------- text ---------------- */

static int glyph_index(char ch) {
    unsigned char u = (unsigned char)ch;
    if (u < 32 || u > 126) u = '?';
    return u - 32;
}

int gfx_text_width(const Font *f, const char *s) {
    int w = 0;
    for (; *s; s++) w += f->adv[glyph_index(*s)];
    return w > 0 ? w - 1 : 0;
}

static void draw_glyph(Canvas *c, const Font *f, int gi, int x, int y, px color) {
    const uint16_t *rows = f->rows + gi * f->height;
    for (int r = 0; r < f->height; r++) {
        uint16_t bits = rows[r];
        for (int b = 0; bits; b++, bits >>= 1)
            if (bits & 1) put(c, x + b, y + r, color);
    }
}

void gfx_text(Canvas *c, const Font *f, int x, int y, const char *s, px color, int shadow) {
    if (shadow) {
        int sx = x;
        for (const char *p = s; *p; p++) {
            int gi = glyph_index(*p);
            draw_glyph(c, f, gi, sx + 1, y + 1, RGB8(0x20, 0x20, 0x20));
            sx += f->adv[gi];
        }
    }
    for (const char *p = s; *p; p++) {
        int gi = glyph_index(*p);
        draw_glyph(c, f, gi, x, y, color);
        x += f->adv[gi];
    }
}

void gfx_text_centered(Canvas *c, const Font *f, int cx, int y, const char *s, px color, int shadow) {
    gfx_text(c, f, cx - gfx_text_width(f, s) / 2, y, s, color, shadow);
}

void gfx_text_right(Canvas *c, const Font *f, int rx, int y, const char *s, px color, int shadow) {
    gfx_text(c, f, rx - gfx_text_width(f, s), y, s, color, shadow);
}
