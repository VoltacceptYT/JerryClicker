// Tiny software renderer for 256x192 ARGB1555 framebuffers (portable C).
#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include "gfxdata.h"

#define SCREEN_W 256
#define SCREEN_H 192

typedef uint16_t px;

#define RGB8(r, g, b) ((px)(0x8000 | ((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10)))

typedef struct {
    px *buf;
    int cx0, cy0, cx1, cy1; // clip rectangle (x1/y1 exclusive)
} Canvas;

void gfx_begin(Canvas *c, px *buf);
void gfx_clip(Canvas *c, int x, int y, int w, int h);
void gfx_unclip(Canvas *c);

void gfx_fill(Canvas *c, int x, int y, int w, int h, px color);
void gfx_frame(Canvas *c, int x, int y, int w, int h, px color); // 1px outline
void gfx_bevel_panel(Canvas *c, int x, int y, int w, int h, px fill, px light, px dark);
void gfx_tile(Canvas *c, const uint16_t *tex16, int x, int y, int w, int h);
void gfx_nineslice(Canvas *c, const uint16_t *tex16, int x, int y, int w, int h);
void gfx_blit16(Canvas *c, const uint16_t *tex16, int x, int y, int half);


// Text. Supports ASCII 32..126; other characters are drawn as '?'.
int gfx_text_width(const Font *f, const char *s);
void gfx_text(Canvas *c, const Font *f, int x, int y, const char *s, px color, int shadow);
void gfx_text_centered(Canvas *c, const Font *f, int cx, int y, const char *s, px color, int shadow);
void gfx_text_right(Canvas *c, const Font *f, int rx, int y, const char *s, px color, int shadow);

#endif
