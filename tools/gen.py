import json, math
from PIL import Image, ImageDraw, ImageFont
LIB = '/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf'
d = json.load(open('/tmp/sent.json', encoding='utf-8'))
MAXTOK = d['maxtok']

# ---------- phrases ----------
def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
with open('source/sentences.c', 'w', encoding='utf-8') as f:
    f.write('/* Genere par tools/gen.py a partir de GrammaQuest Nature des mots v86 - ne pas modifier a la main */\n')
    f.write('#include "sentences.h"\n\nconst Sentence SENTENCES[] = {\n')
    for text, tags in d['sentences']:
        n = len(text.split(' '))
        arr = [0] * MAXTOK
        for k, v in tags.items():
            arr[int(k)] = v
        f.write('  {%s, {%s}},\n' % (cstr(text), ','.join(map(str, arr[:MAXTOK]))))
    f.write('};\n\nconst int NUM_SENTENCES = %d;\n' % len(d['sentences']))
with open('source/sentences.h', 'w') as f:
    f.write('#ifndef SENTENCES_H\n#define SENTENCES_H\n#define MAXTOK %d\n' % MAXTOK)
    f.write('typedef struct { const char *text; unsigned char tags[MAXTOK]; } Sentence;\n')
    f.write('/* tags : 0 = mot non evalue, 1..9 = verbe, nom, adjectif, determinant, pronom, adverbe, preposition, conjonction, interjection */\n')
    f.write('extern const Sentence SENTENCES[];\nextern const int NUM_SENTENCES;\n#endif\n')

# ---------- polices ----------
cps = list(range(32, 127)) + list(range(0xC0, 0x100)) + [0x152, 0x153, 0x2019, 0xAB, 0xBB, 0x2026]
cps = sorted(set(cps))

def build(name, size):
    font = ImageFont.truetype(LIB, size)
    asc, desc = font.getmetrics()
    H = asc + desc
    glyphs, bmp = [], bytearray()
    for cp in cps:
        ch = chr(cp)
        adv = font.getlength(ch)
        w = int(math.ceil(adv)) + 3
        img = Image.new('L', (w, H), 0)
        ImageDraw.Draw(img).text((1, 0), ch, font=font, fill=255)
        glyphs.append((cp, int(round(adv)), w, len(bmp)))
        bmp.extend(img.tobytes())
    return name, H, asc, glyphs, bmp

fonts = [build('big', 15), build('small', 12)]
with open('source/font_data.c', 'w') as f:
    f.write('/* Genere par tools/gen.py (Liberation Sans Bold) - ne pas modifier a la main */\n#include "font.h"\n\n')
    for name, H, asc, glyphs, bmp in fonts:
        f.write('static const unsigned char bmp_%s[] = {\n' % name)
        for i in range(0, len(bmp), 32):
            f.write(','.join(str(b) for b in bmp[i:i+32]) + ',\n')
        f.write('};\nstatic const Glyph glyphs_%s[] = {\n' % name)
        for cp, adv, w, off in glyphs:
            f.write('  {%d,%d,%d,%d},\n' % (cp, adv, w, off))
        f.write('};\nconst Font font_%s = {%d, %d, glyphs_%s, bmp_%s};\n\n' % (name, H, len(glyphs), name, name))
with open('source/font.h', 'w') as f:
    f.write('''#ifndef FONT_H
#define FONT_H
typedef struct { unsigned short cp; unsigned char adv; unsigned char w; unsigned int off; } Glyph;
typedef struct { int h; int n; const Glyph *g; const unsigned char *bmp; } Font;
extern const Font font_big, font_small;
#endif
''')
for name, H, asc, glyphs, bmp in fonts:
    print(name, 'hauteur', H, 'taille', len(bmp), 'octets')
