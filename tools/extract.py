import re, json, sys
src = open('/mnt/user-data/uploads/GrammaQuest_NatureDesMots_v86.html', encoding='utf-8').read()
start = src.index('const NM_TAGGED_SENTENCES = [')
end = src.index('\n];', start)
block = src[start:end]
IDS = ['verbe','nom','adjectif','determinant','pronom','adverbe','preposition','conjonction','interjection']
out = []
skipped = 0
for line in block.split('\n'):
    line = line.strip()
    if not line.startswith('{text:'): continue
    if 'easterEgg' in line:
        skipped += 1; continue
    m = re.match(r'\{text:"((?:[^"\\]|\\.)*)",tags:\{(.*?)\}', line)
    if not m:
        print('PARSE FAIL', line[:80], file=sys.stderr); continue
    text = m.group(1).replace('\\"','"').replace("\\'", "'")
    tags = {int(k): IDS.index(v) + 1 for k, v in re.findall(r"(\d+):'(\w+)'", m.group(2))}
    out.append((text, tags))
print(len(out), 'phrases,', skipped, 'oeufs de paques ignores', file=sys.stderr)
maxtok = max(len(t.split(' ')) for t, _ in out)
print('max tokens', maxtok, file=sys.stderr)
cps = sorted({ord(c) for t, _ in out for c in t})
print('non-ascii:', [(hex(c), chr(c)) for c in cps if c > 126], file=sys.stderr)
json.dump({'sentences': out, 'maxtok': maxtok}, open('/tmp/sent.json', 'w'), ensure_ascii=False)
