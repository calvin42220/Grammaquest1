#ifndef FONT_H
#define FONT_H
typedef struct { unsigned short cp; unsigned char adv; unsigned char w; unsigned int off; } Glyph;
typedef struct { int h; int n; const Glyph *g; const unsigned char *bmp; } Font;
extern const Font font_big, font_small;
#endif
