#!/usr/bin/env python3
"""Genere source/gamedata.[ch] et source/font_data.c a partir des 7 jeux du hub GrammaQuest.
Usage : python3 tools/gen_hub.py GrammaQuest_Hub.html   (necessite node et Pillow)"""
import sys, os, re, json, base64, subprocess, math
import html as _html
HERE = os.path.dirname(os.path.abspath(__file__))
hub = sys.argv[1] if len(sys.argv) > 1 else '/mnt/user-data/uploads/GrammaQuest_Hub.html'
src = open(hub, encoding='utf-8').read()
line = [l for l in src.split('\n') if l.startswith('const DATA=')][0]
DATA = json.loads(line[len('const DATA='):].rstrip(';'))
apps = {k: base64.b64decode(v).decode('utf-8') for k, v in DATA.items()}
os.makedirs('/tmp/apps', exist_ok=True)
for k, v in apps.items():
    open(f'/tmp/apps/{k}.html', 'w', encoding='utf-8').write(v)

def js(app, *names):
    out = subprocess.run(['node', os.path.join(HERE, 'jsextract.js'), f'/tmp/apps/{app}.html', *names],
                         capture_output=True, text=True, check=True).stdout
    return json.loads(out)

MAXTOK = 18
def norm_text(s):
    s = s.replace('\u2019', "'").replace('\u2018', "'").replace('\u2192', '->').replace('\u2013', '-').replace('\u2014', '-')
    s = s.replace('\u2026', '...').replace('\u00a0', ' ').replace('\u202f', ' ').replace('\u2009', ' ')
    s = re.sub(r' +([?!:;])', r'\1', s)   # pas d'espace avant ? ! : ; (sinon le signe devient un mot a part)
    s = re.sub(r'\u00ab ', '\u00ab', s); s = re.sub(r' \u00bb', '\u00bb', s)
    return re.sub(r'\s+', ' ', s).strip()
def norm_text_light(s):
    s = s.replace('\u2019', "'").replace('\u2018', "'").replace('\u2192', '->').replace('\u2013', '-').replace('\u2014', '-').replace('\u2026', '...')
    return s
def ntok(t):
    return re.sub(r"^[\W_]+|[\W_]+$", '', t.lower())
def find_phrase(tokens, phrase):
    pt = [ntok(x) for x in norm_text(phrase).split(' ') if ntok(x)]
    tt = [ntok(x) for x in tokens]
    for i in range(len(tt) - len(pt) + 1):
        if tt[i:i + len(pt)] == pt:
            return list(range(i, i + len(pt)))
    return None


def tok_map(text):
    """decoupe en mots et fusionne les signes ? ! : ; isoles avec le mot precedent ; renvoie (mots, correspondance ancien->nouveau)"""
    orig = text.replace('\u00a0', ' ').replace('\u202f', ' ').replace('\u2009', ' ').split(' ')
    orig = [o for o in orig if o != '']
    new, mp = [], []
    for o in orig:
        if new and re.fullmatch(r"[?!:;\u00bb]+", o):
            new[-1] += o; mp.append(len(new) - 1)
        else:
            new.append(o); mp.append(len(new) - 1)
    return new, mp

stats = {}
def prose(s):
    s = norm_text_light(s).replace('\u00a0', ' ').replace('\u202f', ' ').replace('\u2009', ' ')
    return re.sub(r'\s+', ' ', s).strip()
def Q(text, tags=None, a=0, b=0, cat=0, extra='', expl=''):
    return dict(text=norm_text(text), tags=tags or {}, a=a, b=b, cat=cat, extra=prose(extra), expl=prose(expl))

games = []

# ---------- couleurs / categories des jeux a menu ----------
def cats_from_natures(app, ids_order=None, short=None):
    N = js(app, 'NATURES')['NATURES']
    out = []
    for n in N:
        plural = n['plural']
        plural = 'les ' + plural[4:] if plural.startswith('des ') else plural
        out.append(dict(id=n['id'], label=(short or {}).get(n['id'], n['label']), find=n['find'], plural=plural, sub=n['sub'],
                        col=n['color'], light=n['light']))
    return out

# ---------- 1. determinants ----------
det_cats = cats_from_natures('det', short={'defini': 'Art. défini', 'indefini': 'Art. indéfini', 'contracte': 'Art. contracté',
    'demonstratif': 'Démonstratif', 'possessif': 'Possessif', 'numeral': 'Numéral', 'interrogatif': 'Interrogatif', 'exclamatif': 'Exclamatif'})
det_ids = [c['id'] for c in det_cats]
S = js('det', 'DT_TAGGED_SENTENCES')['DT_TAGGED_SENTENCES']
qs = []
for s in S:
    if s.get('easterEgg'): continue
    toks, mp = tok_map(norm_text_light(s['text']))
    if len(toks) > MAXTOK: continue
    tags = {mp[int(k)]: det_ids.index(v) + 1 for k, v in s['tags'].items() if v in det_ids and int(k) < len(mp)}
    qs.append(Q(' '.join(toks), tags))
games.append(dict(id='det', label='Les déterminants', sub='le, un, mon, ce…', col='#9333EA', kind=0, cats=det_cats, qs=qs))

# ---------- 2. nature des mots ----------
nm_src = apps['nm']
NM_IDS = ['verbe', 'nom', 'adjectif', 'determinant', 'pronom', 'adverbe', 'preposition', 'conjonction', 'interjection']
nm_cats = cats_from_natures('nm', short={'determinant': 'Déterminant', 'verbe': 'Verbe', 'nom': 'Nom', 'adjectif': 'Adjectif', 'pronom': 'Pronom',
    'adverbe': 'Adverbe', 'preposition': 'Préposition', 'conjonction': 'Conjonction', 'interjection': 'Interjection'})
nm_cats = [next(c for c in nm_cats if c['id'] == i) for i in NM_IDS]
S = js('nm', 'NM_TAGGED_SENTENCES')['NM_TAGGED_SENTENCES']
qs = []
for s in S:
    if s.get('easterEgg'): continue
    toks, mp = tok_map(norm_text_light(s['text']))
    if len(toks) > MAXTOK: continue
    tags = {mp[int(k)]: NM_IDS.index(v) + 1 for k, v in s['tags'].items() if v in NM_IDS and int(k) < len(mp)}
    qs.append(Q(' '.join(toks), tags))
games.append(dict(id='nm', label='Nature des mots', sub='Verbe, nom, adjectif', col='#EAB308', kind=0, cats=nm_cats, qs=qs))

# ---------- 3. sujet ----------
S = js('suj', 'SENTENCES')['SENTENCES']
qs, bad = [], 0
for s in S:
    if s.get('easterEgg'): continue
    text = norm_text(s['text']); toks = text.split(' ')
    idx = find_phrase(toks, s['targetPhrase'])
    if not idx or len(toks) > MAXTOK: bad += 1; continue
    qs.append(Q(text, {i: 1 for i in idx}, cat=1 if s.get('lang') == 'en' else 0, extra=s.get('verb', ''), expl=s.get('explanation', '')))
stats['suj'] = (len(qs), bad)
games.append(dict(id='suj', label='Le Sujet', sub='Trouver le sujet', col='#1D5FD4', kind=1, qs=qs))

# ---------- 4. type & forme ----------
S = js('tf', 'SENTENCES')['SENTENCES']
TYPES = ['déclaratif', 'interrogatif', 'impératif', 'exclamatif']; FORMS = ['affirmative', 'négative']
qs = []
for s in S:
    if s.get('easterEgg'): continue
    text = norm_text(s['text'])
    qs.append(Q(text, a=TYPES.index(s['targetType']), b=FORMS.index(s['targetForm']), expl=s.get('explanation', '')))
games.append(dict(id='tf', label='Type & Forme', sub='Type et forme', col='#E81A1A', kind=3, qs=qs,
                  types=['Déclarative', 'Interrogative', 'Impérative', 'Exclamative'], forms=['Affirmative', 'Négative']))

# ---------- 5/6. COD-COI et circonstanciels ----------
def comp_game(app, gid, label, sub, col, types, longs):
    S = js(app, 'SENTENCES')['SENTENCES']
    qs, bad = [], 0
    for s in S:
        if s.get('easterEgg') or s.get('secret'): continue
        text = norm_text(s['text']); toks = text.split(' ')
        if len(toks) > MAXTOK: bad += 1; continue
        targets = s.get('targets') or [dict(phrase=s['targetPhrase'], type=s['targetType'], expl=s.get('explanation', ''))]
        tg, ts, ex = {}, [], []
        for k, t in enumerate(targets[:2]):
            idx = find_phrase(toks, t['phrase'])
            if not idx or t['type'] not in types or any(i in tg for i in idx): continue
            for i in idx: tg[i] = len(ts) + 1
            ts.append(types.index(t['type'])); ex.append(t.get('expl', ''))
        if not ts: bad += 1; continue
        qs.append(Q(text, tg, a=ts[0], b=ts[1] if len(ts) > 1 else 0, cat=1 if len(ts) > 1 else 0, extra=s.get('verb', ''), expl=ex[0]))
        qs[-1]['two'] = len(ts) > 1
        if len(ts) > 1: qs[-1]['expl'] = prose(' '.join(ex))
    stats[gid] = (len(qs), bad)
    return dict(id=gid, label=label, sub=sub, col=col, kind=2, qs=qs, types=types, longs=longs)
games.append(comp_game('co', 'co', 'COD & COI', 'COD ou COI ?', '#E8861A', ['COD', 'COI'], ['COD', 'COI']))
games.append(comp_game('cc', 'cc', 'Circonstanciels', 'Lieu, temps, manière', '#0D9488', ['CCL', 'CCT', 'CCM'], ['CCL lieu', 'CCT temps', 'CCM manière']))

# ---------- 7. homophones ----------
H = js('ho', 'HOMOPHONE_SENTENCES', 'HOMOPHONE_PAIRS')
HP, HS = H['HOMOPHONE_PAIRS'], H['HOMOPHONE_SENTENCES']
ho_cats, qs = [], []
SHORT = {'tout_tous': 'tout / tous'}
for ci, (key, p) in enumerate(HP.items()):
    d = HS[key]
    ho_cats.append(dict(id=key, label=SHORT.get(key, p['label']), find='', plural='', sub=norm_text(p['sub']), col=p['color'], light='#FFFFFF',
                        hint=norm_text(d.get('astuceShort', '')), ch=[norm_text(c) for c in d['choices']]))
    for it in d['items']:
        text = norm_text(it['text'])
        if '___' not in text: continue
        ans = norm_text(it['answer'])
        if ans not in ho_cats[-1]['ch']: continue
        qs.append(Q(text, a=ho_cats[-1]['ch'].index(ans), cat=ci, expl=it.get('explanation', '')))
games.append(dict(id='ho', label='Homophones', sub='a/à, et/est, on/ont', col='#EC4899', kind=4, cats=ho_cats, qs=qs))

# ordre du hub : det, nm, suj, tf, co, cc, ho
for g in games: print(g['id'], len(g['qs']), file=sys.stderr)
print('suj/co/cc (gardes, rejetes):', stats, file=sys.stderr)


# ======================= indices (astuces) des jeux a categories =======================
for gid in ('nm', 'det'):
    CD = js(gid, 'COURS_DATA')['COURS_DATA']
    g = next(x for x in games if x['id'] == gid)
    for c in g['cats']:
        tip = CD.get(c['id'], {}).get('tip', '')
        c['hint'] = re.sub(r'<[^>]+>', '', _html.unescape(tip)) if tip else ''
# ======================= trophees =======================
def clean_text(t):
    t = _html.unescape(t)
    t = t.replace('\u2705', '[OK]').replace('\u274c', '[X]').replace('\u2192', '->').replace('\u2190', '<-')
    t = norm_text_light(t).replace('\u00a0', ' ').replace('\u202f', ' ').replace('\u2009', ' ')
    t = re.sub('[\U0001F000-\U0001FFFF\u2600-\u27BF\uFE0F\u200D\u2B50\u2B06\u2B07\u2BEA\u2300-\u23FF\u25A0-\u25FF\u2190-\u21FF]', '', t)
    t = t.replace('\u201c', '"').replace('\u201d', '"').replace('\u2022', '-').replace('\u00b7', '-')
    return t
def prose2(t):
    t = clean_text(t)
    t = re.sub(r'[ \t]+', ' ', t)
    t = re.sub(r' ?\n ?', '\n', t)
    t = re.sub(r'\n{3,}', '\n\n', t)
    return t.strip()
for g in games:
    T = js(g['id'], 'TROPHIES')['TROPHIES']
    g['troph'] = []
    for t in T:
        flag = 3 if t.get('ultimate') else 2 if t.get('legendary') else 1 if t.get('speedrun') else 0
        g['troph'].append(dict(id=t['id'], name=prose2(t['name']), desc=prose2(t['desc']), flag=flag))

# ======================= cours =======================
def html2text(h):
    h = re.sub(r'<(script|style)[^>]*>.*?</\1>', '', h, flags=re.S)
    h = re.sub(r'<br\s*/?>', '\n', h)
    h = re.sub(r'</(p|div|li|h[1-6]|tr|section)>', '\n', h)
    h = re.sub(r'<[^>]+>', '', h)
    return prose2(h)
def screen_text(app, sid):
    h = apps[app]
    m = re.search(r'<div[^>]*id="%s"[^>]*>' % sid, h)
    if not m: return ''
    rest = h[m.end():]
    n = re.search(r'<div class="screen[ "]', rest)
    seg = rest[:n.start()] if n else rest[:20000]
    t = html2text(seg)
    t = re.sub(r'^.*(Menu principal|Menu cours|Retour au menu|Je m.entra.ne maintenant|Passer à l.entra).*$', '', t, flags=re.M)
    return re.sub(r'\n{3,}', '\n\n', t).strip()
def cours_from_cats(app, cats):
    C = js(app, 'COURS_DATA')['COURS_DATA']
    out = []
    for c in cats:
        d = C.get(c['id'])
        if not d: continue
        part = [c['label'].upper(), html2text(d.get('rule', ''))]
        for ex in d.get('examples', [])[:3]:
            part.append('- ' + html2text(ex.get('s', '')) + ' ' + html2text(ex.get('a', '')))
        if d.get('tip'): part.append('Astuce : ' + html2text(d['tip']))
        out.append('\n'.join(x for x in part if x))
    return '\f'.join(out)
for g in games:
    i = g['id']
    if i in ('nm', 'det'):
        g['cours'] = cours_from_cats(i, g['cats'])
    elif i == 'ho':
        H2 = js('ho', 'HOMOPHONE_SENTENCES')['HOMOPHONE_SENTENCES']
        parts = []
        for ci, c in enumerate(g['cats']):
            d = H2[c['id']]
            sub = '; '.join('« %s » -> %s' % (x['word'], x['correctFor'] if x['correctFor'] else 'autre choix') for x in d.get('substitutions', []))
            parts.append('\n'.join(x for x in [c['label'].upper(), c['sub'], prose2(d.get('astuce', '')), ('Remplacer par : ' + sub) if sub else ''] if x))
        g['cours'] = '\f'.join(parts)
    else:
        g['cours'] = screen_text(i, 'screenCours')
        if i == 'suj':
            en = screen_text('suj', 'screenCoursEN')
            g['cours_en'] = en
    g.setdefault('cours_en', '')
    print(i, 'cours', len(g['cours']), len(g['cours_en']), 'caracteres', file=sys.stderr)

# ======================= ecriture C =======================
def cs(s):
    out = []
    for ch in s:
        o = ord(ch)
        out.append({'\\': '\\\\', '"': '\\"', '\n': ' '}.get(ch, ch))
    return '"' + ''.join(out) + '"'
def cs_multi(s):
    out = []
    for ch in s:
        out.append({'\\': '\\\\', '"': '\\"', '\n': '\\n', '\f': '\\f'}.get(ch, ch))
    return '"' + ''.join(out) + '"'
def hexc(h):
    h = h.lstrip('#'); return '0x' + h.upper()
with open(os.path.join(HERE, '..', 'source', 'gamedata.h'), 'w') as f:
    f.write('''/* Genere par tools/gen_hub.py - ne pas modifier a la main */
#ifndef GAMEDATA_H
#define GAMEDATA_H
#define MAXTOK %d
enum { K_TAG, K_SEL, K_SELTYPE, K_TF, K_GAP };
typedef struct { const char *text, *extra, *expl; unsigned char tags[MAXTOK]; unsigned char a, b, cat; } Q;
typedef struct { const char *label, *find, *plural, *sub, *hint; unsigned col, light; const char *ch[4]; int nch; } Cat;
typedef struct { const char *id, *name, *desc; int flag; } Troph;   /* flag : 0 normal, 1 vitesse, 2 legendaire, 3 ultime */
typedef struct {
  const char *id, *label, *sub; unsigned col; int kind;
  int ncat; const Cat *cats; const Q *qs; int nq;
  int ntypes; const char *const *types; const char *const *longs;
  int nforms; const char *const *forms;
  int ntroph; const Troph *troph; const char *cours, *cours_en;
} GameData;
#define NGAMES %d
extern const GameData GAMES[NGAMES];
#endif
''' % (MAXTOK, len(games)))
with open(os.path.join(HERE, '..', 'source', 'gamedata.c'), 'w', encoding='utf-8') as f:
    f.write('/* Genere par tools/gen_hub.py a partir de GrammaQuest_Hub.html - ne pas modifier a la main */\n#include "gamedata.h"\n\n')
    for g in games:
        f.write('static const Q Q_%s[] = {\n' % g['id'])
        for q in g['qs']:
            tg = [0] * MAXTOK
            for k, v in q['tags'].items(): tg[k] = v
            f.write(' {%s,%s,%s,{%s},%d,%d,%d},\n' % (cs(q['text']), cs(q['extra']), cs(q['expl']), ','.join(map(str, tg)), q['a'], q['b'], q['cat']))
        f.write('};\n')
        if g.get('cats'):
            f.write('static const Cat C_%s[] = {\n' % g['id'])
            for c in g['cats']:
                ch = c.get('ch', [])
                f.write(' {%s,%s,%s,%s,%s,%s,%s,{%s},%d},\n' % (cs(c['label']), cs(c['find']), cs(c['plural']), cs(c['sub']), cs(c.get('hint', '')),
                        hexc(c['col']), hexc(c['light']), ','.join(cs(x) for x in ch + ['"'] * 0) if ch else '0', len(ch)))
            f.write('};\n')
        f.write('static const Troph TR_%s[] = {\n' % g['id'])
        for t in g['troph']: f.write(' {%s,%s,%s,%d},\n' % (cs(t['id']), cs(t['name']), cs(t['desc']), t['flag']))
        f.write('};\n')
        f.write('static const char CO_%s[] = %s;\n' % (g['id'], cs_multi(g['cours'])))
        f.write('static const char CE_%s[] = %s;\n' % (g['id'], cs_multi(g['cours_en'])))
        for nm, key in (('T', 'types'), ('L', 'longs'), ('F', 'forms')):
            if g.get(key):
                f.write('static const char *const %s_%s[] = {%s};\n' % (nm, g['id'], ','.join(cs(x) for x in g[key])))
    f.write('\nconst GameData GAMES[NGAMES] = {\n')
    for g in games:
        i = g['id']
        f.write(' {%s,%s,%s,%s,%d, %s,%s,Q_%s,%d, %s,%s,%s, %s,%s, %d,TR_%s,CO_%s,CE_%s},\n' % (
            cs(i), cs(g['label']), cs(g['sub']), hexc(g['col']), g['kind'],
            str(len(g['cats'])) if g.get('cats') else '0', 'C_' + i if g.get('cats') else '0', i, len(g['qs']),
            str(len(g['types'])) if g.get('types') else '0', 'T_' + i if g.get('types') else '0', 'L_' + i if g.get('longs') else '0',
            str(len(g['forms'])) if g.get('forms') else '0', 'F_' + i if g.get('forms') else '0', len(g['troph']), i, i, i))
    f.write('};\n')

# ======================= polices =======================
from PIL import Image, ImageDraw, ImageFont
LIB = '/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf'
used = set()
alltext = ''
for g in games:
    for q in g['qs']: alltext += q['text'] + q['extra'] + q['expl']
    for c in g.get('cats', []): alltext += c['label'] + c['find'] + c['plural'] + c['sub'] + c.get('hint', '') + ''.join(c.get('ch', []))
    alltext += g['label'] + g['sub']
used = {ord(ch) for ch in alltext}
extra_ui = 'ÉÈÊËÀÂÎÏÔÛÙÇéèêëàâîïôûùçœŒ«»…’°' + 'Étape déposé Bientôt'
used |= {ord(ch) for ch in extra_ui}
cps = sorted(set(range(32, 127)) | {c for c in used if c > 126 and c not in (0xA0, 0x202F)})
missing = [chr(c) for c in cps if c > 126 and c > 0x17F]
print('caracteres speciaux :', ''.join(chr(c) for c in cps if c > 126), file=sys.stderr)

def build(name, size):
    font = ImageFont.truetype(LIB, size)
    asc, desc = font.getmetrics(); H = asc + desc
    glyphs, bmp = [], bytearray()
    for cp in cps:
        ch = chr(cp); adv = font.getlength(ch); w = int(math.ceil(adv)) + 3
        img = Image.new('L', (w, H), 0); ImageDraw.Draw(img).text((1, 0), ch, font=font, fill=255)
        glyphs.append((cp, int(round(adv)), w, len(bmp))); bmp.extend(img.tobytes())
    return name, H, glyphs, bmp
fonts = [build('big', 15), build('small', 12)]
with open(os.path.join(HERE, '..', 'source', 'font_data.c'), 'w') as f:
    f.write('/* Genere par tools/gen_hub.py (Liberation Sans Bold) - ne pas modifier a la main */\n#include "font.h"\n\n')
    for name, H, glyphs, bmp in fonts:
        f.write('static const unsigned char bmp_%s[] = {\n' % name)
        for i in range(0, len(bmp), 32): f.write(','.join(str(b) for b in bmp[i:i + 32]) + ',\n')
        f.write('};\nstatic const Glyph glyphs_%s[] = {\n' % name)
        for cp, adv, w, off in glyphs: f.write('  {%d,%d,%d,%d},\n' % (cp, adv, w, off))
        f.write('};\nconst Font font_%s = {%d, %d, glyphs_%s, bmp_%s};\n\n' % (name, H, len(glyphs), name, name))
with open(os.path.join(HERE, '..', 'source', 'font.h'), 'w') as f:
    f.write('''#ifndef FONT_H
#define FONT_H
typedef struct { unsigned short cp; unsigned char adv; unsigned char w; unsigned int off; } Glyph;
typedef struct { int h; int n; const Glyph *g; const unsigned char *bmp; } Font;
extern const Font font_big, font_small;
#endif
''')
