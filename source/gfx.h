/* Primitives de dessin 2D (framebuffers RGB15 256x192) - inclus par game.c */
#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "font.h"

#define SW 256
#define SH 192
typedef uint16_t px;

static px hex(uint32_t c) {
    return (px)(((c >> 19) & 31) | (((c >> 11) & 31) << 5) | (((c >> 3) & 31) << 10) | 0x8000);
}
static uint32_t lighten(uint32_t c, int pct) {  /* melange avec du blanc */
    int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    r += (255 - r) * pct / 100; g += (255 - g) * pct / 100; b += (255 - b) * pct / 100;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
static const px WHITE_ = 0xFFFF;
#define INK hex(0x1E293B)
#define GREY hex(0x64748B)

static const Glyph *find_glyph(const Font *f, unsigned cp) {
    int lo = 0, hi = f->n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->g[mid].cp == cp) return &f->g[mid];
        if (f->g[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}
static unsigned next_cp(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned c = s[0];
    if (c < 0x80) { *p += 1; return c; }
    if ((c & 0xE0) == 0xC0 && s[1]) { *p += 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0 && s[1] && s[2]) { *p += 3; return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
    *p += 1;
    return '?';
}
static int text_w(const Font *f, const char *s) {
    int w = 0;
    while (*s) {
        unsigned cp = next_cp(&s);
        const Glyph *g = find_glyph(f, cp);
        if (!g) g = find_glyph(f, '?');
        if (g) w += g->adv;
    }
    return w;
}
static void fill_rect(px *buf, int x, int y, int w, int h, px col) {
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if (yy < 0 || yy >= SH) continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            if (xx < 0 || xx >= SW) continue;
            buf[yy * SW + xx] = col;
        }
    }
}
static void rrect(px *buf, int x, int y, int w, int h, int r, px col) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int cx = -1, cy = -1;
            if (i < r) cx = r - 1 - i; else if (i >= w - r) cx = i - (w - r);
            if (j < r) cy = r - 1 - j; else if (j >= h - r) cy = j - (h - r);
            if (cx >= 0 && cy >= 0 && (cx * cx + cy * cy) > (r - 1) * (r - 1) + r / 2) continue;
            int xx = x + i, yy = y + j;
            if (xx < 0 || xx >= SW || yy < 0 || yy >= SH) continue;
            buf[yy * SW + xx] = col;
        }
    }
}
static void box(px *buf, int x, int y, int w, int h, int r, px fill, px border) {
    rrect(buf, x, y, w, h, r, border);
    rrect(buf, x + 2, y + 2, w - 4, h - 4, r > 2 ? r - 2 : 1, fill);
}
static void disc(px *buf, int cx, int cy, int r, px col) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) {
                int xx = cx + x, yy = cy + y;
                if (xx >= 0 && xx < SW && yy >= 0 && yy < SH) buf[yy * SW + xx] = col;
            }
}
static void draw_text(px *buf, const Font *f, int x, int y, const char *s, px col, int sc) {
    int cr = col & 31, cg = (col >> 5) & 31, cb = (col >> 10) & 31;
    while (*s) {
        unsigned cp = next_cp(&s);
        if (cp == 0xA0 || cp == 0x202F) cp = ' ';
        const Glyph *g = find_glyph(f, cp);
        if (!g) g = find_glyph(f, '?');
        if (!g) continue;
        for (int r = 0; r < f->h; r++) {
            for (int c = 0; c < g->w; c++) {
                int a = f->bmp[g->off + r * g->w + c];
                if (a < 20) continue;
                for (int sy = 0; sy < sc; sy++) {
                    int yy = y + r * sc + sy;
                    if (yy < 0 || yy >= SH) continue;
                    for (int sx = 0; sx < sc; sx++) {
                        int xx = x + (c - 1) * sc + sx;
                        if (xx < 0 || xx >= SW) continue;
                        px d = buf[yy * SW + xx];
                        int dr = d & 31, dg = (d >> 5) & 31, db = (d >> 10) & 31;
                        buf[yy * SW + xx] = (px)((dr + (cr - dr) * a / 255) | ((dg + (cg - dg) * a / 255) << 5) |
                                                 ((db + (cb - db) * a / 255) << 10) | 0x8000);
                    }
                }
            }
        }
        x += g->adv * sc;
    }
}
static void draw_center(px *buf, const Font *f, int cx, int y, const char *s, px col, int sc) {
    draw_text(buf, f, cx - text_w(f, s) * sc / 2, y, s, col, sc);
}
/* Texte multi-lignes : gere les retours a la ligne '\n'. Renvoie le nombre de lignes. draw=0 : mesure seulement. */
static int wrap(px *buf, const Font *f, int x, int y, int maxw, const char *s, px col, int lineh, int draw) {
    char line[300], word[64], trial[380];
    int lines = 0;
    line[0] = 0;
    while (*s) {
        if (*s == '\n') {
            if (draw && line[0]) draw_text(buf, f, x, y + lines * lineh, line, col, 1);
            lines++; line[0] = 0; s++; continue;
        }
        int n = 0;
        while (*s && *s != ' ' && *s != '\n' && n < 63) word[n++] = *s++;
        word[n] = 0;
        while (*s == ' ') s++;
        if (line[0]) snprintf(trial, sizeof trial, "%s %s", line, word);
        else snprintf(trial, sizeof trial, "%s", word);
        if (line[0] && text_w(f, trial) > maxw) {
            if (draw) draw_text(buf, f, x, y + lines * lineh, line, col, 1);
            lines++;
            snprintf(line, sizeof line, "%s", word);
        } else {
            snprintf(line, sizeof line, "%s", trial);
        }
    }
    if (line[0]) { if (draw) draw_text(buf, f, x, y + lines * lineh, line, col, 1); lines++; }
    return lines;
}
#endif
